# HTTP-API des Toycontrollers

Steuerung per HTTP auf dem vorhandenen Webserver (Port 80), gedacht für Skripte, Home Assistant, Stream Deck
und KI-Bots. Die API nutzt intern **dieselbe Schlüsseltabelle, dieselben Prüfungen und dieselbe Ereignis-Queue**
wie das WebSocket-Protokoll der Web-UI (`Software/src/protocol.cpp`), es gibt keinen zweiten Codepfad.

- Basis-URL: `http://toycontroller.local` oder die IP des Geräts (steht im Serial-Log beim Start, `[wifi] connected ip=...`).
- Alle Antworten sind JSON mit `Cache-Control: no-store` und `Access-Control-Allow-Origin: *`.
- **Kein Schutz, kein Schlüssel.** Siehe "Risiken" unten, bevor du das Gerät irgendwo erreichbar machst.
- Aufrufe der API zählen **nicht** für den Web-Failsafe (`sys.failsafe`).
- Selbstbeschreibung für Bots: `GET /api/keys` liefert alle Schlüssel, Kommandos und Parameter als JSON.

## Routen

| Route | Methode | Zweck |
|---|---|---|
| `/api/state` | GET | gesamter Zustand (aus einer in `loop()` vorbereiteten Kopie) und laufende `for=`-Timer |
| `/api/keys` | GET | Selbstbeschreibung aller Schlüssel, Kommandos und Parameter |
| `/api/set` | GET, POST | einen oder mehrere Schlüssel setzen |
| `/api/cmd` | GET, POST | Kommando ausführen (`collar.beep`, `collar.vibe`, `collar.shock`, `all_off`) |
| `/api/toggle?k=<Schlüssel>` | GET | einen bool-Schlüssel umschalten |
| alle `/api/...` | OPTIONS | CORS-Preflight (204, mit `Access-Control-Allow-Private-Network: true`) |

### GET /api/state

```
{"ok":true,"n":41,"d":{"ch1.en":false,"ch1.on":0,...},"timers":[{"k":"ch1.en","left_s":12}]}
```
`n` zählt bei jeder Änderung hoch (wie beim WebSocket). Der Zustand ist höchstens einen `loop()`-Durchlauf alt,
also einige Millisekunden bis Zehntelsekunden. Direkt nach einem `set` kann er noch den alten Wert zeigen.

### POST /api/set

Body (max. 512 Byte): ein flaches JSON-Objekt. Die WebSocket-Form `{"t":"set","d":{...}}` geht ebenfalls.
```
curl -X POST http://toycontroller.local/api/set -H "Content-Type: application/json" \
     -d '{"ch1.en":true,"ch1.pwm":60}'
```
Mehrere Schlüssel in einem Aufruf: die gültigen werden angewendet, ungültige einzeln gemeldet.

`GET /api/set?ch1.en=1&ch1.pwm=60` macht dasselbe. Bool-Werte: `1`, `0`, `true`, `false`. Alles andere sind
ganze Zahlen. Query-Parameter und Body dürfen kombiniert werden. Ein Body mit dem Content-Type
`application/x-www-form-urlencoded` (das ist der Default von `curl -d`) wird ebenfalls verstanden, als JSON
(`-d '{"ch1.pwm":5}'`) oder als Formularfelder (`-d 'ch1.pwm=5&ch1.en=1'`).

Antwort bei Erfolg (HTTP 200):
```
{"ok":true,"applied":2}
```
Bei Fehlern (HTTP 400, oder 503 bei voller Queue) steht im Body, was angewendet wurde und was nicht:
```
{"ok":false,"applied":1,"errors":[{"k":"buzzer.bpm","code":"range","min":1,"max":255},{"k":"nope","code":"unknown"}]}
```
`ok:true` gibt es nur, wenn **alles** angewendet wurde. Ein gültiger Schlüssel neben einem ungültigen wird trotzdem
angewendet (`applied`).

Zusätzliche Felder in der Antwort:

| Feld | Bedeutung |
|---|---|
| `held` | Liste von Schlüsseln, die gespeichert wurden, deren Ausgang aber gerade von Bluetooth gehalten wird (siehe "BLE-Hold") |
| `restart_in_s` | das Gerät startet in dieser Zeit neu (nur `ble.toy` mit geändertem Wert) |
| `timers` | für `*.en`-Schlüssel gestartete `for=`-Timer |
| `for_ignored` | Schlüssel, für die `for=` nicht gilt |

### POST /api/cmd

```
curl -X POST http://toycontroller.local/api/cmd -H "Content-Type: application/json" -d '{"c":"collar.beep"}'
curl "http://toycontroller.local/api/cmd?c=all_off"
```
- `all_off`: schaltet alle Ausgänge aus und löscht alle `for=`-Timer.
- `collar.beep`, `collar.vibe`, `collar.shock`: brauchen `collar.en = true` (sonst 409 `disabled`) und nutzen
  `collar.strength`. **Mindestabstand 300 ms** zwischen zwei Collar-Kommandos, sonst HTTP 429:
  `{"ok":false,"code":"rate_limited","retry_ms":180}`.
- Die Prüfung `collar.en` nutzt den Live-Wert. Wenn du direkt zuvor `collar.en` gesetzt hast, warte etwa 100 ms,
  sonst kann `disabled` kommen, weil `loop()` das Setzen noch nicht angewendet hat.
- Die Collar-Sendung blockiert wie beim WebSocket kurz den Netzwerk-Task.

### GET /api/toggle

`GET /api/toggle?k=ch1.en` schaltet einen schreibbaren bool-Schlüssel um und antwortet mit dem neuen Wert:
`{"ok":true,"k":"ch1.en","value":true,"applied":1}`. Andere Schlüssel liefern `type` (nicht bool),
`readonly` oder `unknown`.

## Fehler und Statuscodes

| HTTP | `code` | Bedeutung |
|---|---|---|
| 200 | | alles angewendet |
| 400 | `unknown` / `readonly` / `type` / `range` (je Schlüssel in `errors`) | Schlüssel unbekannt, nur lesbar, falscher Typ (auch Kommazahl), außerhalb des Bereichs (mit `min`/`max`) |
| 400 | `parse` | Body ist kein JSON-Objekt |
| 400 | `empty` | keine Schlüssel im Aufruf |
| 400 | `missing_c`, `missing_k`, `unknown_cmd` | Parameter fehlt oder unbekanntes Kommando |
| 400 | `for` (in `errors`) | `for=` außerhalb 1-3600 oder keine Zahl, es wird nichts angewendet |
| 400 | `timer_full` (in `errors`) | kein freier Timer, der Schlüssel wurde wieder auf 0 gesetzt (bei 8 Timern und 7 `*.en`-Schlüsseln praktisch nicht erreichbar) |
| 409 | `disabled` | `collar.en` ist aus |
| 411 | `length_required` | Body mit `Transfer-Encoding: chunked` (nicht unterstützt) |
| 413 | `too_large` | Body länger als 512 Byte |
| 429 | `rate_limited` | Collar-Kommando innerhalb von 300 ms nach dem letzten |
| 503 | `queue_full` (in `errors`) | die Ereignis-Queue war voll, nochmal versuchen |
| 503 | `not_ready` / `no_memory` | Zustand noch nicht bereit (kurz nach dem Start) / Body konnte nicht gepuffert werden |

## Zeitgesteuert: `for=<Sekunden>`

`for` gilt bei `set` und `toggle` (als Query-Parameter, bei `set` auch als Mitglied im JSON-Body:
`{"ch1.en":true,"for":30}`) und wirkt auf die **`*.en`-Schlüssel, die dieser Aufruf auf 1 setzt**:
nach Ablauf setzt das Gerät sie wieder auf 0.

- Bereich 1 bis **3600** s. Ungültig: 400, nichts wird angewendet.
- Maximal 8 Timer gleichzeitig, ein Timer je Schlüssel. Ein neuer Aufruf für denselben Schlüssel **ersetzt** den
  Timer (Restzeit beginnt neu, "letzter Aufruf gewinnt").
- Ein Setzen desselben Schlüssels **ohne** `for=` oder auf 0 **bricht** den Timer ab. `all_off` löscht alle.
- Ablauf nur, wenn der Schlüssel dann noch 1 ist. War er inzwischen aus (Web-UI, Menü, `all_off`), passiert nichts.
- Andere Schlüssel im Aufruf (`ch1.pwm` usw.) werden normal angewendet und stehen in `for_ignored`.
- Laufende Timer siehst du in `GET /api/state` unter `timers` (`left_s`) und in der Antwort unter `timers`.
- Timer liegen nur im RAM und sind nach einem Neustart weg.

Beispiel: Kanal 1 für 20 s einschalten.
```
curl "http://toycontroller.local/api/set?ch1.pwm=70&ch1.en=1&for=20"
```

## BLE-Hold (Bluetooth hat Vorrang)

Solange ein Bluetooth-Gerät einen Ausgang mit einem Level > 0 steuert, ignoriert der Server die gespeicherten
Web-Werte dieses Ausgangs (`ble.hold.<ziel>` ist dann `true`). Die API verwirft solche Werte **nicht still**:
der Wert wird gespeichert (und gilt, sobald der Hold endet), und die Antwort nennt die Schlüssel unter `held`:
```
{"ok":true,"applied":1,"held":["ch1.pwm"]}
```
Zuordnung: `ch1.*` bis `ch4.*` zu `ble.hold.ch1` bis `ch4`, `pump.*` zu `ble.hold.pump`,
`collar.*` zu `ble.hold.collar`, `buzzer.en`/`buzzer.bpm` zu `ble.hold.buzzer`. Steht `held_by` bei einem Schlüssel
in `/api/keys`, gilt das für ihn. Die Angabe stützt sich auf die Kopie in `loop()` und ist höchstens einen Durchlauf alt.

## Toy-Modell (`ble.toy`)

Ein **geänderter** Wert startet das Gerät nach etwa 2 Sekunden neu (die Identität wird nur beim BLE-Start gesetzt),
die Antwort enthält `restart_in_s`. Ein Wert, der dem aktuellen entspricht, ändert nichts. Nach dem Neustart sind
alle `for=`-Timer weg, das Gerät ist einige Sekunden nicht erreichbar, und die Lovense-App muss neu koppeln.

## Schlüssel

Werte in Klammern sind Einheiten. Zeiten sind in Zehntelsekunden (900 = 90 s). Diese Tabelle stammt aus
`GET /api/keys` (dort immer die aktuelle Fassung).

| Schlüssel | Typ | Bereich | Zugriff | Bedeutung |
|---|---|---|---|---|
| `ch1.en` | bool | true/false | schreibbar | channel 1 enabled (BLE-Hold: `ble.hold.ch1`) |
| `ch1.on` | int | 0 bis 900 (0.1 s) | schreibbar | channel 1 on time (0-90 s) (BLE-Hold: `ble.hold.ch1`) |
| `ch1.off` | int | 0 bis 900 (0.1 s) | schreibbar | channel 1 off time, 0 = runs continuously (BLE-Hold: `ble.hold.ch1`) |
| `ch1.pwm` | int | 0 bis 100 (%) | schreibbar | channel 1 power (BLE-Hold: `ble.hold.ch1`) |
| `ch2.en` | bool | true/false | schreibbar | channel 2 enabled (BLE-Hold: `ble.hold.ch2`) |
| `ch2.on` | int | 0 bis 900 (0.1 s) | schreibbar | channel 2 on time (0-90 s) (BLE-Hold: `ble.hold.ch2`) |
| `ch2.off` | int | 0 bis 900 (0.1 s) | schreibbar | channel 2 off time, 0 = runs continuously (BLE-Hold: `ble.hold.ch2`) |
| `ch2.pwm` | int | 0 bis 100 (%) | schreibbar | channel 2 power (BLE-Hold: `ble.hold.ch2`) |
| `ch3.en` | bool | true/false | schreibbar | channel 3 enabled (BLE-Hold: `ble.hold.ch3`) |
| `ch3.on` | int | 0 bis 900 (0.1 s) | schreibbar | channel 3 on time (0-90 s) (BLE-Hold: `ble.hold.ch3`) |
| `ch3.off` | int | 0 bis 900 (0.1 s) | schreibbar | channel 3 off time, 0 = runs continuously (BLE-Hold: `ble.hold.ch3`) |
| `ch3.pwm` | int | 0 bis 100 (%) | schreibbar | channel 3 power (BLE-Hold: `ble.hold.ch3`) |
| `ch4.en` | bool | true/false | schreibbar | channel 4 enabled (BLE-Hold: `ble.hold.ch4`) |
| `ch4.on` | int | 0 bis 900 (0.1 s) | schreibbar | channel 4 on time (0-90 s) (BLE-Hold: `ble.hold.ch4`) |
| `ch4.off` | int | 0 bis 900 (0.1 s) | schreibbar | channel 4 off time, 0 = runs continuously (BLE-Hold: `ble.hold.ch4`) |
| `ch4.pwm` | int | 0 bis 100 (%) | schreibbar | channel 4 power (BLE-Hold: `ble.hold.ch4`) |
| `pump.en` | bool | true/false | schreibbar | pump enabled (BLE-Hold: `ble.hold.pump`) |
| `pump.on` | int | 0 bis 900 (0.1 s) | schreibbar | pump on time (0-90 s) (BLE-Hold: `ble.hold.pump`) |
| `pump.off` | int | 0 bis 900 (0.1 s) | schreibbar | pump off time, 0 = runs continuously (BLE-Hold: `ble.hold.pump`) |
| `pump.pwm` | int | 0 bis 100 (%) | schreibbar | pump power (BLE-Hold: `ble.hold.pump`) |
| `collar.en` | bool | true/false | schreibbar | collar enabled (needed for the collar.* commands) (BLE-Hold: `ble.hold.collar`) |
| `collar.strength` | int | 0 bis 100 (%) | schreibbar | collar strength used by the collar.* commands (BLE-Hold: `ble.hold.collar`) |
| `collar.btonly` | bool | true/false | schreibbar | send the collar shock via Bluetooth only when the level changes (BLE-Hold: `ble.hold.collar`) |
| `buzzer.en` | bool | true/false | schreibbar | buzzer metronome enabled (BLE-Hold: `ble.hold.buzzer`) |
| `buzzer.bpm` | int | 1 bis 255 (BPM) | schreibbar | metronome speed (BLE-Hold: `ble.hold.buzzer`) |
| `buzzer.vol` | int | 0 bis 10 | schreibbar | metronome volume |
| `ble.map0.out` | int | 0 bis 7 | schreibbar | Bluetooth map 0 target: 0 off, 1-4 Ch1-4, 5 pump, 6 collar, 7 metronome |
| `ble.map0.min` | int | 0 bis 255 | schreibbar | Bluetooth map 0 minimum output (BPM for the metronome) |
| `ble.map0.max` | int | 0 bis 255 | schreibbar | Bluetooth map 0 maximum output (BPM for the metronome, collar max 100) |
| `ble.map1.out` | int | 0 bis 7 | schreibbar | Bluetooth map 1 target: 0 off, 1-4 Ch1-4, 5 pump, 6 collar, 7 metronome |
| `ble.map1.min` | int | 0 bis 255 | schreibbar | Bluetooth map 1 minimum output (BPM for the metronome) |
| `ble.map1.max` | int | 0 bis 255 | schreibbar | Bluetooth map 1 maximum output (BPM for the metronome, collar max 100) |
| `ble.toy` | int | 0 bis 5 | schreibbar | Bluetooth toy model: 0 Dolce, 1 Lush, 2 Hush, 3 Domi, 4 Nora, 5 Edge. A change restarts the device (**startet das Gerät neu**) |
| `sys.failsafe` | int | 3 bis 120 (s) | schreibbar | web failsafe: no WebSocket traffic for this long switches everything off |
| `ble.connected` | bool | true/false | nur lesbar | a Bluetooth app is connected |
| `ble.hold.ch1` | bool | true/false | nur lesbar | Bluetooth controls channel 1 (level > 0) |
| `ble.hold.ch2` | bool | true/false | nur lesbar | Bluetooth controls channel 2 (level > 0) |
| `ble.hold.ch3` | bool | true/false | nur lesbar | Bluetooth controls channel 3 (level > 0) |
| `ble.hold.ch4` | bool | true/false | nur lesbar | Bluetooth controls channel 4 (level > 0) |
| `ble.hold.pump` | bool | true/false | nur lesbar | Bluetooth controls the pump (level > 0) |
| `ble.hold.collar` | bool | true/false | nur lesbar | Bluetooth controls the collar (level > 0) |
| `ble.hold.buzzer` | bool | true/false | nur lesbar | Bluetooth controls the metronome (level > 0) |

## Beispiele

### curl

```
curl http://toycontroller.local/api/state
curl http://toycontroller.local/api/keys
curl -X POST http://toycontroller.local/api/set -H "Content-Type: application/json" -d '{"pump.pwm":40,"pump.en":true}'
curl "http://toycontroller.local/api/set?ch1.en=1&ch1.pwm=60&for=30"
curl "http://toycontroller.local/api/toggle?k=ch1.en"
curl "http://toycontroller.local/api/cmd?c=all_off"
```

### Home Assistant (`configuration.yaml`, `rest_command`)

```yaml
rest_command:
  toy_all_off:
    url: "http://toycontroller.local/api/cmd?c=all_off"
    method: get
  toy_channel:
    url: "http://toycontroller.local/api/set?ch{{ ch }}.pwm={{ pwm }}&ch{{ ch }}.en=1&for={{ seconds }}"
    method: get
  toy_set:
    url: "http://toycontroller.local/api/set"
    method: post
    content_type: "application/json"
    payload: '{"pump.pwm": {{ pwm }}, "pump.en": true, "for": {{ seconds }}}'
```
Aufruf in einer Automation: `action: rest_command.toy_channel` mit `data: {ch: 1, pwm: 60, seconds: 30}`.
`toycontroller.local` löst nicht in jedem Docker- oder VLAN-Setup auf, dann die feste IP eintragen.

### Stream Deck

Mit der Standard-Aktion "Website" (Zugriff im Hintergrund, **GET**) je Taste eine URL:

| Taste | URL |
|---|---|
| Kanal 1 an/aus | `http://toycontroller.local/api/toggle?k=ch1.en` |
| Pumpe 20 s | `http://toycontroller.local/api/set?pump.pwm=60&pump.en=1&for=20` |
| Alles aus | `http://toycontroller.local/api/cmd?c=all_off` |
| Piepton | `http://toycontroller.local/api/cmd?c=collar.beep` (Halsband muss aktiviert sein) |

Für POST mit JSON-Body gibt es Plugins wie "API Ninja" oder "HTTP Request". Beim Doppelklick auf Collar-Tasten
antwortet die API mit 429, das ist gewollt.

## Risiken und Grenzen

- **Kein Schutz (CSRF):** Es gibt keine Anmeldung und keinen Schlüssel. **Jede Webseite, die im Browser eines
  Nutzers im selben Netz geöffnet ist, kann Aufrufe an das Gerät auslösen** (Bild-Tag, `fetch` mit
  `no-cors`, Link), auch `collar.shock`. Der Angriff braucht nur, dass die Seite die Adresse des Geräts errät
  (`toycontroller.local` ist bekannt). Wer das nicht will, betreibt das Gerät nur in einem abgeschotteten Netz.
  Ein Schlüssel (Header oder Query-Parameter) lässt sich später nachrüsten, an einer Stelle in `api.cpp`.
- **CORS ist offen** (`Access-Control-Allow-Origin: *`, dazu `Access-Control-Allow-Private-Network: true`), damit Web-Apps im
  Browser die API aufrufen können. Das ist dieselbe Öffnung wie oben.
- **Mixed Content:** Eine Seite über `https://` darf das Gerät nicht über `http://` aufrufen (Browser blockieren das).
  Home Assistant, curl und Stream Deck sind davon nicht betroffen, ein Web-Dashboard auf HTTPS schon.
- **Toggle-Race:** `toggle` liest den Wert aus der Kopie in `loop()` (höchstens einen Durchlauf alt). Zwei Toggles im
  Abstand von wenigen Millisekunden können denselben Wert lesen und den Schlüssel nur einmal umschalten.
- **Timer-Lücke:** Ein `for=`-Timer merkt nicht, wenn ein anderer Client (Web-UI, Menü) denselben Schlüssel
  ändert. War er beim Ablauf noch 1, wird er abgeschaltet, auch wenn er inzwischen bewusst wieder eingeschaltet wurde.
  Ein erneutes Setzen über die API ersetzt oder löscht den Timer.
- **Web-Failsafe:** API-Aufrufe halten den Failsafe nicht am Leben. Wurde die Web-UI seit dem Start einmal benutzt
  und wird sie geschlossen, schaltet der Failsafe nach `sys.failsafe` Sekunden alles aus und löscht damit auch
  Zustände, die per API gesetzt wurden. Ohne Web-UI-Nutzung seit dem Start greift er nie. Eine API-Steuerung
  braucht deshalb einen eigenen Sicherheitsmechanismus, zum Beispiel `for=`.
- **Neustart bei RST:** Die verwendete AsyncTCP-Version hat einen seltenen, bekannten Absturz bei hartem
  Verbindungsabbruch (siehe `STATUS.md`). Viele kurze Verbindungen (Stream Deck, Automationen) erhöhen die
  Zahl der Verbindungen, das Gerät startet im Fehlerfall in den sicheren Zustand (Ausgänge aus).
- Grenzen: Body max. 512 Byte, keine chunked-Bodies, ganze Zahlen (keine Kommazahlen), ein leeres Objekt
  (`{}`) gilt als "keine Schlüssel".
