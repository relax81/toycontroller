# Projektstand ToyController (Arbeitsverlauf, Branch `main`)

Stand: nach Commit `594be56` auf `main`. Die früheren Feature-Branches (`dev-webserver`, `dev-wifi-ble-parallel`)
sind in `main` gemergt und gelöscht; die Abschnitte unten sind der Arbeitsverlauf und behalten ihre alten Commit-Hashes.
Neu seit dem Merge: Zeiten in Zehntelsekunden, Pumpen-Timer, Web-Layout, Toy-Modellauswahl und Metronom als BT-Ziel
(Abschnitt "Seit dem Merge nach `main`"). Zum Weitermachen: erst diese Datei und `docs/TODO-notes.md` lesen.

## Arbeitsregeln (kurz)

- Repo-Root `toycontroller`, Code in `Software/`, Build: `pio run -d Software`.
- Alles liegt auf `main`. Kleine Änderungen gehen direkt auf `main`, für größere Umbauten wird ein
  Feature-Branch angelegt (Entscheidung des Nutzers, Ein-Personen-Projekt). Push nur auf Zuruf; vor dem
  Push das Diff auf Zugangsdaten prüfen (`true-credentials.h` ist nicht getrackt und darf nie committet werden).
- Flashen nur auf Zuruf und immer mit `--upload-port COM5`; `upload_port` /
  `monitor_port` (COM7) in der `platformio.ini` nicht ändern. Plattform
  `espressif32 @ ~3.5.0` bleibt.
- CRLF-Zeilenenden erhalten. Dateien einzeln mit `git add` hinzufügen.
- Die Pumpe ist getestet (Steuersignal 0-3,3 V PWM, kein Leistungsausgang). Das 433-MHz-Halsband ist
  auf der Hardware bestätigt. Der Metronom-Buzzer ist bewusst nicht Teil des Failsafes.
- HTerm muss getrennt sein, wenn geflasht wird. Beim Öffnen von COM5 aus
  Python gibt es keinen Reset; Boot-Log nur per EN-Taste sichtbar. Ein
  fehlgeschlagener Upload (`Timed out waiting for packet header`) klappt
  meist beim direkten zweiten Versuch.

## Erledigt und auf der Hardware bestätigt

- WLAN und BLE laufen parallel (NimBLE-Arduino statt Bluedroid),
  Ausgabe-Arbitrierung (BLE hat pro Ausgang Vorrang, solange sein Wert > 0
  ist), WS-Failsafe (15 s Timeout, Ping alle 5 s).
- Schritt 0: Ch3-Timerfix, nicht blockierender Buzzer, BLE-Parser
  aufgeräumt, LEDC-Fix (PWM4 lief mit 2 kHz, jetzt Kanäle 0-3 / Buzzer 4 /
  Pumpe 6, `static_assert` gegen Timer-Kollisionen), `[fw]`- und
  `[ledc]`-Diagnose.
- Modularisierung: `config.h`, `wifi_setup.h/.cpp`, `state.h/.cpp`
  (`AppState`).
- Schritt 3: Ch1-4 und Pumpe liegen komplett im State (`state.out[i]`,
  `state.pump`, `state.rt[i]`), `PWM_Output` und `disable_Outputs` sind
  Schleifen, ein Kanal startet beim Einschalten mit der On-Phase.

## Schritt 4 (Collar, Buzzer, BT-Zuordnung, BLE-Eingang, `bt_hold` in den State): abgeschlossen

Methode wie Schritt 3: erst Brücke (die alten Namen werden Referenzen auf
`state.*`), dann Region für Region per Skript umbenennen (Zuordnungstabelle,
Prüfung pro Block, Rundlauf), am Ende die Brücke entfernen.

| Commit | Inhalt | Status |
|---|---|---|
| `6b87941` | `state.h`: Collar-, Buzzer-, BLE-Strukturen, `OutputId` (compile-time initialisiert) | Build |
| `9cfb581` | Notizen zum 433-MHz-Code in `TODO-notes.md` | - |
| `8c1bb51` | tote Variablen und `reset_Outputs()` entfernt | Build |
| `998378b` | `buzzer_Metronome()` ohne Parameter | getestet |
| `ed723d6` | Brücke für Collar, Buzzer, BT-Map, BLE-Eingang, `bt_hold` (29 Referenzen) | getestet |
| `164ca3d` | Buzzer auf `state.buzzer.*` (26 Ersetzungen) | getestet |
| `64d491d` | Collar auf `state.collar.*` (25 Ersetzungen) | getestet |
| `e00e7d5` | 6a: BLE-Callbacks, Verbindungsstatus, Loop-Block auf `state.ble.in.*` | getestet |
| `dbf01b5` | 6b: BT-Menü und Mapping auf `state.ble.map[0/1].*` (60 Ersetzungen) | getestet |
| `00d827a` | 7: `state.ble.hold/collarMapped`, Helfer `outputId(i)` | getestet |
| `3b90924` | 8: Brücke entfernt, keine alten Namen mehr | getestet |

Der Gesamt-Smoke-Test nach Schritt 8 (inkl. Halsband) ist bestanden. Offen ist
nur die Pumpe (Hardware fehlt).

## `outputs.h/.cpp` herausgelöst (abgeschlossen, getestet)

| Commit | Inhalt |
|---|---|
| `e406d9e` | `outputs_init()` (Pins, LEDC), `debug_ledc`; Debug-Makros nach `config.h` |
| `de40fe8` | `PWM_Output`, `disable_Outputs` |
| `a594e08` | `bluetooth_write_pwm`, `dg`/`uniqueKeyOfDevice`, Wrapper `collar_send(mode, strength)` |
| `7ac843a` | `buzzer_Metronome(nowMs)` |
| `701f45f` | `outputs_arbitrate()`, `outputs_all_off()`; `ws_failsafe()` behält die `values`-Spiegelung |
| `523b6a1`, `de6562c` | Serial-Befehl `reboot`/`restart` (Ersatz für den Reset-Knopf); `523b6a1` baut nicht, `de6562c` repariert das (bei bisect überspringen) |

`DogCollar3` und das Sendeverhalten sind unverändert (siehe Abschnitt 433 MHz
in `TODO-notes.md`). Serielles Terminal (z. B. HTerm): "Send on enter" auf LF
stellen, `reboot` senden. Boot-Log und `[ledc]`-Diagnose ohne Reset-Knopf.

## WLAN-Manager, Einrichtungsportal und mDNS (`wifi_manager.h/.cpp`)

| Commit | Inhalt | Status |
|---|---|---|
| `150ec7b` | Stufe 1: NVS-Zugangsdaten, Zustandsautomat, mDNS, `wifi-reset` | getestet |
| `7dead0a` | Stufe 2: Hotspot, Captive-DNS, Portalseite, Scan, pending-Logik | getestet |
| `54608e1` | Portal-OLED: größere Schrift, Auswahl zur Laufzeit | getestet |
| `d1fad23` | Stufe 3: 3-min-Regel bei Verlust, Encoder-Langdruck beim Boot, Doku | getestet |

- Zugangsdaten im NVS-Namespace `wifi` (`ssid`, `pass`, Flag `seeded`,
  `p_ssid`/`p_pass` für noch unbestätigte Portal-Daten). `true-credentials.h`
  füllt den NVS nur einmal (`seeded`), nur wenn die Datei existiert
  (`__has_include`); sie ist untracked und wird nie gelesen oder committet.
  `wifi-reset` und der Langdruck löschen die Zugangsdaten, nicht das Flag.
- Ablauf: `Connecting` (15 s) -> `Connected`; Timeout oder keine Daten ->
  `Portal`. Verlust im Betrieb: `Reconnecting` (Neuversuch alle 10 s), nach
  3 min `Portal`. Im Portal alle 60 s ein STA-Versuch (max. 15 s, nur ohne
  Client am Hotspot); klappt er, geht der Hotspot aus.
- Hotspot `Toy-XXXX` (letzte 2 MAC-Bytes), Passwort 8 Ziffern zufällig pro
  Portalstart, nur auf der OLED (Bild öffnet sich beim Start, ein Klick
  schließt es, über "WiFi Status" wieder aufrufbar). Portalseite liegt im Code
  (PROGMEM), Scan async, `/wifi/scan` und `/wifi/save` nur im Portalzustand
  (sonst 403). Alles Unbekannte wird auf `192.168.4.1` umgeleitet, die Steuerung
  ist als Link erreichbar.
- Neue Zugangsdaten aus dem Portal sind "pending": erst nach der ersten
  erfolgreichen Verbindung gelten sie, sonst bleiben die alten und das Portal
  startet wieder.
- Rücksetzen: Serial `wifi-reset`, oder Encoder-Taster (GPIO32, kein
  Strapping-Pin) beim Einschalten halten: innerhalb der ersten 3 s nach dem
  Setup drücken und 3 s halten.
- Alles auf der Hardware getestet (2026-09-24), inkl. Verlust im Betrieb mit dem
  mobilen Hotspot und Langdruck beim Boot.
- Das WLAN-Passwort steht nirgends im Log. Flash: 1370570 Byte (41,0 %),
  RAM 58180 Byte.

## Block A: Ereignis-Queue und Persistenz (umgesetzt, auf der Hardware getestet 2026-09-25)

| Commit | Inhalt |
|---|---|
| `e44c335` | `state_set()`/`state_drain()`, Queue (64 Einträge), `loopmax` in der `[heap]`-Zeile |
| `4eb4ef6` | BLE-Callbacks reihen Ereignisse ein (V1/V2 als ein Paar-Event) |
| `79efa6e` | WS-Handler reiht ein, `values` (JSONVar) nur noch in `loop()` aus dem State gebaut |
| `277e899` | `settings_load()` (NVS `cfg`, validiert), `state.failsafeTimeoutS` |
| `de24468` | verzögertes Speichern (5 s nach der letzten Änderung, nur geänderte Schlüssel), BT-Menü und `co_chg` über `state_set()` |
| `6b4b702` | Serial `cfg`, `cfg failsafe <3-120>`, `cfg reset` |
| `e472460` | sofort speichern, wenn der letzte WS-Client trennt (Buzzer-Werte gingen bei Power-Loss innerhalb des 5-s-Debounce verloren) |

- `loop()` ist der einzige State-Schreiber. `state_set()` wendet im `loop`-Task sofort
  an, aus anderen Tasks wird eingereiht. Kritische Ereignisse (`ALL_OFF`, `*_ENABLE`
  mit 0, `OUT_OFF`, BLE) warten bis 10 ms; scheitert das, setzt `state.cpp` die
  Flags `all_off_req` / `ble_disc_req` (alles aus bzw. BLE getrennt) und loggt
  `[evq]`. Wert-Ereignisse (PWM, bpm, vol) werden bei Überlauf verworfen und gezählt.
- Der Web-Broadcast kommt aus `loop()` (Dirty-Flag, höchstens einmal pro Durchlauf);
  die JSON-Schlüssel und `script.js` sind unverändert. Der Broadcast enthält jetzt
  immer den aktuellen State (auch uncommittete Encoder-Werte im Manuell-Menü).
- NVS `cfg` (Version 1): `ver`, `b0_out`/`b1_out` (u8), `b0_min`/`b0_max`/
  `b1_min`/`b1_max` (u16), `fs_to` (u16, s), `bz_vol`/`bz_bpm` (u8), `bz_on` (u16, ms),
  `co_chg` (u8), `toy` (u8, Toy-Modell-Index, seit `70c9a97`). Nie gespeichert: Enable-Flags und Laufzeitwerte. Grenzen (min <= max,
  Ausgang 6 max <= 100) setzt `state_apply()` immer, auch zur Laufzeit.
- Collar-Klicks (`sendCollar`, blockierend) laufen unverändert aus dem `async_tcp`-Task.
- Toy-Modell: umgesetzt, siehe "Seit dem Merge nach `main`".

## Block B, Teil 1: JSON-Protokoll für den WebSocket (umgesetzt, Hardware-Test offen)

| Commit | Inhalt |
|---|---|
| `9d301a5` | `protocol.h/.cpp`: Schlüsseltabelle, `get`/`set`/`ack`/`err`/`state`, Dispatch am führenden `{` |
| `833c906` | alte `toggle_*`/`slider_*`/`buzzer?`-Nachrichten laufen über dieselbe Tabelle (Bereichsprüfung, Verwerfen mit Log) |
| `a8ce607` | `patch` mit Sequenznummer `n` für JSON-Clients, flaches Alt-JSON nur noch für alte Clients |
| `3189172` | `cmd` (`collar.beep/vibe/shock`, `all_off`) |
| `0b0e714`, `5bf20a8`, `7693f27` | `data/`: `data-key`/`data-cmd`, `script.js` empfängt `state`/`patch`, sendet `set`/`cmd` |

- Protokoll und Beispiele stehen im Kopf von `protocol.h`. Schlüssel: `ch1..4.{en,on,off,pwm}`,
  `pump.{en,pwm}`, `collar.{en,strength,btonly}`, `buzzer.{en,bpm,vol}`,
  `ble.map0/1.{out,min,max}`, `sys.failsafe` (rw), `ble.connected`, `ble.hold.*` (nur lesen).
- Ein Client wird durch sein erstes `get` zum JSON-Client (Slot pro Client, max. 8). `state` und
  `patch` entstehen nur in `loop()` (`protocol_loop()` nach `outputs_arbitrate()`), `n` zählt
  pro Patch hoch; Lücke im Client: `get` erneut. Ein Client mit voller Queue bekommt wieder
  einen vollen `state`.
- `set` mit mehreren Schlüsseln: gültige werden angewendet, ungültige kommen als Liste in
  einem `err` (`unknown`, `readonly`, `type`, `range` mit `min`/`max`). `ack` heißt eingereiht,
  der tatsächliche Wert kommt per `patch` (z. B. geklemmtes `ble.map*.max`).
- Alte Nachrichten (`id?wert`, `click_*`, `getValues`) funktionieren weiter. Die Altpfade werden
  erst nach dem Hardware-Test in einem eigenen Commit entfernt.
- `data/` ändert sich: dafür `uploadfs` (getrennt von der Firmware, nur auf Zuruf mit COM5).

### Zustellung an die Clients und bekannte Grenzen

- Pro Client merkt sich `protocol_loop()` das zuletzt eingereihte `n` (`syncedN`). Ein Client, der genau
  einen Schritt zurückliegt, bekommt den `patch`, ein weiter zurückliegender den vollen `state`. Gesendet
  wird nur, wenn seine Queue nicht voll ist (`queueIsFull()`), sonst wird er im nächsten Durchlauf bedient.
  Grund: Die Bibliothek verwirft bei voller Queue still, ein verlorener letzter Patch blieb sonst unbemerkt.
- Stall-Schutz: Ist die Queue eines Clients 2000 ms (`STALL_MS`) durchgehend voll, schließt das Gerät die
  Verbindung (`client()->close(true)`, Log `[ws] client #N stalled ...`). `script.js` verbindet neu und holt
  per `get` den Stand. Gemessen werden kann nur "voll", die Queue-Länge ist nicht öffentlich.
- **Bekannte Grenze:** Ein Client, der stumm wird, dessen Queue aber nie voll wird (weniger als 32 offene
  Nachrichten), wird nicht erkannt. Beobachtet nur bei sehr aggressivem, synthetischem Parallelaufbau
  (Testskript), nicht mit zwei bedienten Browser-Tabs. Bewusst nicht behandelt (ein `n`-Heartbeat vom
  Client wäre der Weg).
- **Bekannter, seltener Absturz bei hartem Verbindungsabbruch (RST):** `Guru Meditation LoadProhibited`
  in `AsyncServer::_accept` (`this == NULL`), gefolgt von einem Neustart. Wahrscheinliche Ursache
  (aus Quelltext und Disassembly gelesen, nicht per Test bewiesen): Use-after-free in AsyncTCP 1.1.1.
  Bei RST ruft lwIP den Fehler-Callback und gibt den `pcb` frei, `AsyncClient::_error()` läuft danach im
  `async_tcp`-Task und ruft `tcp_arg(_pcb, NULL)` usw. auf dem freigegebenen `pcb` auf. Wird der Speicher
  für eine neue Verbindung wiederverwendet, verliert deren Accept sein `callback_arg`. `_close()` hat dasselbe
  Muster. Auslöser im Test: Verbindungen per RST beenden und sofort neu aufbauen (Node `process.exit`,
  `rstchurn.py`). Im Alltag nur bei abrupten Abbrüchen zu erwarten (Tab-/App-Absturz, WLAN weg), nicht beim
  normalen Schließen. Der Stall-Schutz war daran nicht beteiligt (im Absturzlauf keine `stalled`-Zeile).
  **Bewusst nicht gepatcht** (Fremdcode, Plattform `espressif32 @ ~3.5.0` bleibt stabil). Folge ist ein Neustart
  in den sicheren Zustand (Ausgänge aus, Failsafe, WLAN-Reconnect, Client-Resync). Nach dem Neustart
  kann das WLAN-Connect in den Timeout laufen und das Portal starten, es verbindet sich später selbst.
- Testskripte schließen Verbindungen sauber per Close-Handshake (kein `process.exit` direkt nach `close()`),
  damit sie keine künstlichen RSTs erzeugen.

## Block B, Teil 2: Bluetooth-Zuordnung und Live-Indikator im Web-UI (umgesetzt, auf der Hardware getestet 2026-09-25)

| Commit | Inhalt |
|---|---|
| `5902cd4` | `script.js`/`style.css`: `data-hold` setzt die Klasse `ble-held`, editierbewusste Updates, `err` setzt den Serverwert zurück, `update_select()` |
| `cca6027` | `index.html`: `data-hold` und Hinweistext an Ch1-4, Pump, Collar |
| `66d02a8` | einklappbare Karte "Bluetooth-Zuordnung" (`<details>`), Platzhalter für das Toy-Modell |
| `cfe0e28` | Firmware: Ping-Intervall = min(5000, Failsafe / 3) ms, bei jedem `loop()`-Durchlauf aus `state.failsafeTimeoutS` |
| `3c6c307` | Nacharbeit aus dem Hardware-Test (Min über Max ziehen): 800-ms-Rückstellung bei ausbleibendem Patch |
| `1e0bbbe` | Korrektur dazu: feste Regler-Bereiche, Anhalten von Min/Max per Skript |

- Die Karte enthält für V1/V2 je Ziel (Aus, Ch1-4, Pumpe, Halsband) und Min/Max, `collar.btonly` und
  `sys.failsafe`. Alle Felder hängen wie die übrigen Regler an `data-key` und werden vom vorhandenen
  `state`/`patch` befüllt (auch zugeklappt). Der Aufklappzustand liegt nur im `localStorage` des Browsers.
- Jede Änderung ist genau ein `set`. Mehrere Änderungen nacheinander gehen als getrennte `set` in der
  Reihenfolge `out`, `max`, `min` raus (verlustfrei geklemmt, wie `settings_load()`). Die geklemmten Werte
  kommen per `patch` zurück. Die Regler haben feste Bereiche: 0-255, beim Halsband 0-100 (beide Regler
  des Kanals).
- Live-Indikator: Ch1-4, Pump und Collar werden bei `ble.hold.*` abgedunkelt und mit "BLE" markiert, mit
  Hinweistext. Die Regler bleiben **bedienbar** (nur markiert, nicht gesperrt): Bei Level 0 endet der Latch
  durch eine Änderung aus dem Web, bei Level > 0 ignoriert der Server Web-Werte für Ch1-4/Pumpe und
  speichert sie. Der Halsband-Klick wird vom Server nicht durch `hold` blockiert.
- `script.js`: Ein Element, das gerade gezogen oder getippt wird (`input` bis `change`/Loslassen), wird
  nicht überschrieben, eingehende Werte warten. Ein nur fokussiertes Element wird wieder aktualisiert
  (vorher blieb ein geklemmter Wert falsch stehen). Ein `err` setzt den letzten Serverwert zurück.
- Ping-Intervall: Vorher fest 5 s, ein Failsafe unter etwa 5 s konnte bei einem ruhigen, gesunden Tab
  zwischen zwei Pongs ablaufen. Bereich 3-120 s bleibt.
- Min/Max (`3c6c307`, `1e0bbbe`): Der Server klemmt `min <= max`. Ändert die Klemmung nichts (z. B. zweimal
  `min=220` bei `max=100`), kommt kein Patch und der Regler zeigte einen falschen Wert. Deshalb hält
  `btClampInput()` Min beim Ziehen (`input`, auch bei Klick auf die Leiste) an Max an und Max an Min.
  Die Bereiche bleiben dabei fest: Ein Bereich, der dem anderen Regler folgt (erster Versuch in `3c6c307`),
  skaliert die Leiste um und der andere Regler springt optisch.
- Rückstellung bei ausbleibendem Patch (`v2Verify()`, gilt für **alle** `data-key`-Regler und -Dropdowns):
  Kommt 800 ms nach einer Änderung kein Patch, der den Wert bestätigt, wird der letzte Serverwert wieder
  angezeigt. Es zählt nur die neueste Änderung eines Elements, nicht während es gezogen oder getippt wird.
- Auf der Hardware bestätigt (Testpunkte 1-11): Karte auf/zu, Werte, Ziel-Wechsel mit laufendem BLE, Min/Max,
  Halte-Anzeige, Bedienung bei gehaltenem Kanal, V1=V2-Hinweis, Failsafe-Feld (auch ruhiger Tab bei 3-4 s),
  Persistenz nach Reboot, zwei Tabs gleichzeitig, Halsband als Trockentest. Die Pumpe bleibt ohne Hardware
  ungetestet. `data/` braucht `uploadfs`, die Firmware einen Flash (nur auf Zuruf).
- Bekannte Grenzen: V1 und V2 auf demselben Ausgang wird nur im UI angemerkt, der Server prüft nichts (beide
  schreiben nacheinander, ein Kanal kann kurz auf 0 fallen). `hold` unterscheidet nicht "BLE aktiv (> 0)" von
  "Latch bei 0". Beim Wechsel des Ziels weg vom Halsband während eines Levels > 0 wird am Halsband nichts
  zurückgenommen (Verhalten ungetestet). Bei Queue-Überlauf wird ein Wert verworfen und trotzdem mit `ack`
  bestätigt (nur `[evq]`-Log).

### Zustellung an die Clients und bekannte Grenzen

- Pro Client merkt sich `protocol_loop()` das zuletzt eingereihte `n` (`syncedN`). Ein Client, der genau
  einen Schritt zurückliegt, bekommt den `patch`, ein weiter zurückliegender den vollen `state`. Gesendet
  wird nur, wenn seine Queue nicht voll ist (`queueIsFull()`), sonst wird er im nächsten Durchlauf bedient.
  Grund: Die Bibliothek verwirft bei voller Queue still, ein verlorener letzter Patch blieb sonst unbemerkt.
- Stall-Schutz: Ist die Queue eines Clients 2000 ms (`STALL_MS`) durchgehend voll, schließt das Gerät die
  Verbindung (`client()->close(true)`, Log `[ws] client #N stalled ...`). `script.js` verbindet neu und holt
  per `get` den Stand. Gemessen werden kann nur "voll", die Queue-Länge ist nicht öffentlich.
- **Bekannte Grenze:** Ein Client, der stumm wird, dessen Queue aber nie voll wird (weniger als 32 offene
  Nachrichten), wird nicht erkannt. Beobachtet nur bei sehr aggressivem, synthetischem Parallelaufbau
  (Testskript), nicht mit zwei bedienten Browser-Tabs. Bewusst nicht behandelt (ein `n`-Heartbeat vom
  Client wäre der Weg).
- **Bekannter, seltener Absturz bei hartem Verbindungsabbruch (RST):** `Guru Meditation LoadProhibited`
  in `AsyncServer::_accept` (`this == NULL`), gefolgt von einem Neustart. Wahrscheinliche Ursache
  (aus Quelltext und Disassembly gelesen, nicht per Test bewiesen): Use-after-free in AsyncTCP 1.1.1.
  Bei RST ruft lwIP den Fehler-Callback und gibt den `pcb` frei, `AsyncClient::_error()` läuft danach im
  `async_tcp`-Task und ruft `tcp_arg(_pcb, NULL)` usw. auf dem freigegebenen `pcb` auf. Wird der Speicher
  für eine neue Verbindung wiederverwendet, verliert deren Accept sein `callback_arg`. `_close()` hat dasselbe
  Muster. Auslöser im Test: Verbindungen per RST beenden und sofort neu aufbauen (Node `process.exit`,
  `rstchurn.py`). Im Alltag nur bei abrupten Abbrüchen zu erwarten (Tab-/App-Absturz, WLAN weg), nicht beim
  normalen Schließen. Der Stall-Schutz war daran nicht beteiligt (im Absturzlauf keine `stalled`-Zeile).
  **Bewusst nicht gepatcht** (Fremdcode, Plattform `espressif32 @ ~3.5.0` bleibt stabil). Folge ist ein Neustart
  in den sicheren Zustand (Ausgänge aus, Failsafe, WLAN-Reconnect, Client-Resync). Nach dem Neustart
  kann das WLAN-Connect in den Timeout laufen und das Portal starten, es verbindet sich später selbst.
- Testskripte schließen Verbindungen sauber per Close-Handshake (kein `process.exit` direkt nach `close()`),
  damit sie keine künstlichen RSTs erzeugen.

## Seit dem Merge nach `main` (Zeiten, Pumpe, Web-UI, Toy-Modelle, Metronom)

| Commit | Inhalt |
|---|---|
| `c9de902` | `ch1..4.on/off` in Zehntelsekunden (0-900 = 0-90 s), Zeit-Slider mit 0,1-s-Schritten bis 2 s |
| `d22b65f` | Pumpe mit On/Off-Zyklus (`pump.on/off`), Collar-Checkbox in der Collar-Karte, Karte "Allgemeine Einstellungen" |
| `9d03d25`, `ba36552` | Collar-Buttons untereinander (mittig, gleich breit), schmalere Klappkarten |
| `70c9a97` | wählbares Toy-Modell (Tabelle Name/UUID/Kennung), NVS `toy`, Neustart nach Wechsel |
| `e3647db` | `vibChannels` je Modell (V2 bei Ein-Kanal-Modellen unbenutzt und ausgeblendet), `<meta charset>` in `index.html` |
| `90f342a` | Max entfernt, Nora nutzt V2 für Rotate (0-20), Edge ist Modell 5 |
| `594be56` | Ausgang 7 "Metronom (BPM)" als Bluetooth-Ziel, Halte-Markierung an der Buzzer-Karte |

**Zeiten in Zehntelsekunden.** `state.out[i].on/off` und `state.pump.on/off` sind Zehntelsekunden, die
Schlüssel `ch1..4.on/off` und `pump.on/off` haben den Bereich 0-900 (`protocol.cpp`), `outputs.cpp` rechnet
`* 100`. OLED zeigt eine Nachkommastelle, der Encoder stellt ganze Sekunden (0-90, intern `* 10`). Die Zeiten
werden nicht im NVS gespeichert. Alte Clients (Legacy-Pfad) bekommen jetzt Zehntelsekunden statt Sekunden. Im Web
haben die Zeit-Slider `data-scale="time"`: Position 0-20 = 0,0-2,0 s in 0,1-s-Schritten, 21-108 = 3-90 s in
1-s-Schritten, dazu -/+ Buttons (eine Position). Die Umrechnung liegt in `script.js` (`posToTenths`/`tenthsToPos`).

**Pumpe mit On/Off-Zyklus.** Eigener Laufzeitzustand `state.pumpRt` wie `state.rt[i]`. Off = 0: Dauerlauf, sonst
On-Phase, dann Pause. Beim Einschalten startet der Zyklus mit der On-Phase, unter BLE-Hold zählt er nicht mit.
On = 0 mit Off > 0 lässt die Pumpe (wie bei Ch1-4) sofort in die Pause fallen.

**Web-UI.** Bis zu 7 Kacheln in einer Zeile (`max-width` 1600 px). Die Bluetooth-Karte enthält nur noch die
Zuordnung und das Toy-Modell; `collar.btonly` sitzt in der Collar-Karte, `sys.failsafe` in der neuen klappbaren
Karte "Allgemeine Einstellungen". `<meta charset="utf-8">` fehlt sonst, der Server sendet `text/html` ohne Zeichensatz.

**Toy-Modellauswahl** (`toy_models.h/.cpp`, Schlüssel `ble.toy` 0-5, NVS `toy`).

| Index | Modell | Kennung | Kanäle |
|---|---|---|---|
| 0 | Dolce | J | V1 + V2 (Vibrate1/2), bisherige Identität `J:40:C0423D012834;`, Name `LVS-Z001` |
| 1 | Lush | S | V1 |
| 2 | Hush | Z | V1 |
| 3 | Domi | W | V1 |
| 4 | Nora | C | V1 + V2 = Rotate (0-20) |
| 5 | Edge | P | V1 + V2 (Vibrate1/2) |

- Die BLE-Identität (Name, Service, DeviceType-Antwort `<Kennung>:<Firmware>:<Adresse>;`) wird nur beim BLE-Init
  gesetzt. Ein Wechsel speichert und startet das Gerät 2 s nach der letzten Änderung neu; die Lovense-App muss
  neu koppeln. Ein ungültiger gespeicherter Wert fällt auf Dolce zurück (so nach dem Entfernen von Max, Edge war
  vorher 6).
- Ein-Kanal-Modelle: V2 bleibt immer 0, `Vibrate2` wird ignoriert, `Vibrate:` setzt nur V1 (bei Dolce/Edge beide).
  Das Web-UI blendet V2 in der Zuordnung dann aus. Nora setzt bei `Rotate:` den Wert 0-20 nach V2.
- Max ist nicht enthalten (der Air-Kanal hat nur 0-5); Modelle mit `Mply:` (Solace, Flexer, Lapis, ...) ebenfalls nicht.
- Quelle: Lovense-Protokollseite auf buttplug.io. **Nicht gegen einen echten Mitschnitt geprüft:** Namen
  (`LVS-<Buchstabe>001`, Dolce und Hush teilen `LVS-Z001`), Firmware-Zahl `40` und die Gen-2-UUID für alle Modelle.
  Mit der Lovense-App am Gerät wurde die Auswahl bestätigt ("läuft alles").

**Metronom als Bluetooth-Ziel (Ausgang 7).** `OUT_BPM`, `OUT_ID_COUNT` = 8, `OutputNumItems` = 8. Der BLE-Wert 1-20
wird auf Min...Max abgebildet, hier sind Min/Max BPM (1-255, im UI "Min BPM"/"Max BPM"). Das Metronom spielt nur bei
Wert > 0, unabhängig vom Ein/Aus-Schalter, mit der Lautstärke aus dem Web. Solange BLE hält (`hold[OUT_BPM]`, Schlüssel
`ble.hold.buzzer`), spielt es nur mit BLE-Wert; eine Änderung von BPM, Volume oder Schalter im Web beendet den
Level-0-Latch (`webSettings` für Id 7). OLED zeigt "BPM". Getestet nur im Browser (Attrappe) und im Code, ein Test mit
der App am Gerät steht aus.

## HTTP-API (Branch `feature/http-api`, umgesetzt, auf der Hardware getestet 2026-09-26)

Referenz für Nutzer und Bots: `docs/API.md`. Kein Schutz, kein Schlüssel, CORS offen; API-Aufrufe zählen nicht für
den Web-Failsafe.

- **Ein Codepfad:** `protocol.cpp` behält die Schlüsseltabelle. Neu darin: Metadaten je Schlüssel (`unit`, `desc`,
  `hold`, `restart`), ein Snapshot aller Werte (`protocol_loop()` kopiert einmal pro `loop()`-Durchlauf unter `portMUX`),
  die gemeinsamen Funktionen `protocol_apply_object`, `protocol_apply_text`, `protocol_run_cmd` mit `SetResult`
  (`applied`, `errors`, `held`, `restart`, Masken der gesetzten Schlüssel). `handle_set`/`handle_cmd` des WebSockets nutzen
  dieselben Funktionen. Neu für den WebSocket: `queue_full` als Fehlercode, wenn `state_set()` das Ereignis nicht einreihen kann.
- **`api.h/.cpp`:** `GET /api/state` (aus dem Snapshot), `GET /api/keys` (gestreamt), `GET|POST /api/set`, `GET|POST /api/cmd`,
  `GET /api/toggle`, OPTIONS mit `Access-Control-Allow-Private-Network`. Body max. 512 Byte (413), chunked gibt 411.
  Ein JSON-Body mit Content-Type `application/x-www-form-urlencoded` (Default von `curl -d`) wird ebenfalls verstanden
  (die Bibliothek legt ihn dann als POST-Parameter `body` ab). Status: 200 alles angewendet, 400 mit `applied` + `errors`
  bei Teilfehlern, 503 bei `queue_full`, 409 Halsband aus, 429 `rate_limited` (Collar-Kommandos, 300 ms).
- **`for=<1-3600>` und `for_ms=<100-3600000>`:** (`for_ms` seit 2026-09-26, die Timer-Liste rechnet in Millisekunden, `left_ms` im State, beide zusammen ergeben `conflict`) bis zu 8 Timer im `loop()` (`api_loop()` nach `protocol_loop()`), je `*.en`-Schlüssel einer, ein neuer
  Aufruf ersetzt, ein Setzen ohne `for=` oder auf 0 und `all_off` löschen. Ablauf nur, wenn der Schlüssel noch 1 ist.
- **BLE-Hold:** Werte werden wie bisher gespeichert, die Antwort nennt die betroffenen Schlüssel unter `held`. Beruht auf dem Snapshot.
- **`ble.toy`:** ein geänderter Wert liefert `restart_in_s: 2`, derselbe Wert nichts.
- **Wichtiger Fund:** `JSONVar::keys()` von Arduino_JSON stürzt bei einem leeren Objekt ab (Nullzeiger in cJSON). Das traf
  auch das WebSocket (`{"t":"set","d":{}}`). Guard in `protocol_apply_object`.
- **Tests:** `Software/tools/api_test.py` (124 Prüfungen, davon 3 nur mit `--collar`: Fehlerfälle, ungültiges JSON, Body 512/600 Byte, chunked, Toggle, 429,
  `for=` inkl. Ersetzen/Abbrechen/`all_off`, 30 Schlüssel in einem Body, 150 + 100 sequentielle Aufrufe, 4 parallele Threads) und
  `Software/tools/ws_regress.py` (21 Prüfungen des WebSocket-Protokolls, zwei Verbindungen). Beide schalten kurz Ausgänge (nicht mit angeschlossener Pumpe o. ä. ausführen). Alles, was 433-MHz-Signale erzeugt
  (`collar.beep`/`collar.vibe`), läuft nur mit `--collar`, sonst meldet das Skript SKIP (ohne `--collar`: 121 von 124 bzw. 20 von 21 Prüfungen).
- **Messungen** (Build vor Etappe A → nach Etappe B): Flash 1 387 366 B (41,5 %) → 1 405 326 B (42,0 %, +17 960 B, Stand mit `for_ms`),
  statisches RAM 58 780 B → 59 100 B (+320 B). Stack des `async_tcp`-Tasks (16 384 B): kleinster freier Wert 13 372 B im ganzen
  Testlauf, auch bei `POST /api/set` mit 30 Schlüsseln (434 B), also etwa 3,0 KB Spitze. Freier Heap im Leerlauf 130-139 KB,
  kleinster Wert unter Last (4 Threads parallel) 111 712 B. Eine Anfrage mit neuer Verbindung dauert etwa 90-100 ms (WLAN, TCP).
- **Nicht auf der Hardware getestet:** die `held`-Meldung bei aktivem BLE-Hold (braucht einen BLE-Client mit Level > 0) und ein
  `ble.toy` mit neuem Wert samt Neustart.
- Merge nach `main` erst nach ausdrücklicher Freigabe.

## Weitere Ideen (nicht begonnen)

- Kanalfälle im Manuell-Menü tabellengetrieben machen.
- Weitere Toy-Modelle und die fehlenden BLE-Befehle: siehe `TODO-notes.md`, Abschnitt "Toy-Modelle".
- ~~Visueller Indikator in der Web-UI für BLE-gehaltene Kanäle~~ **Umgesetzt (Block B, Teil 2).**
- ~~JSON-Protokoll für den WebSocket und Migration von `data/script.js`~~ **Umgesetzt (Block B, Teil 1).**
- Optional: kleine WLAN-Diagnose (Status im Log, Neuverbinden). Einmal blieb
  das WLAN nach dem Start aus, bei erneutem Flashen desselben Stands war es
  wieder da (vermutlich Router/Koexistenz, nicht reproduzierbar).

## Hinweise zur Skript-Methode

- Die Umbenennungsskripte liegen nicht im Repo (nur im Sitzungs-Scratchpad)
  und müssen bei Bedarf neu erstellt werden. Aufbau: Region per Marker
  bestimmen, pro Block die erwarteten Variablen mit Anzahl prüfen, Brücken-
  zeilen auslassen, Rundlauf (Rückübersetzung = Originaltext) prüfen, erst
  dann anwenden.
- Beim Aufruf aus Git-Bash Regex-Argumente mit `//` durch
  `MSYS_NO_PATHCONV=1` schützen (sonst werden sie als Pfad umgeschrieben).
