# Stand des Umbaus (Branch `dev-wifi-ble-parallel`)

Stand: 2026-09-24, Schritt 4, `outputs.h/.cpp` und das WLAN-Einrichtungsportal (Stufe 1-3) umgesetzt. Zum Weitermachen morgen: erst
diese Datei und `docs/TODO-notes.md` lesen.

## Arbeitsregeln (kurz)

- Repo-Root `toycontroller`, Code in `Software/`, Build: `pio run -d Software`.
- Nur auf `dev-wifi-ble-parallel` arbeiten, nie auf `main`. Push/Merge nur
  auf Zuruf; vor dem Push das Diff auf Zugangsdaten prüfen
  (`true-credentials.h` ist nicht getrackt und darf nie committet werden).
- Flashen nur auf Zuruf und immer mit `--upload-port COM5`; `upload_port` /
  `monitor_port` (COM7) in der `platformio.ini` nicht ändern. Plattform
  `espressif32 @ ~3.5.0` bleibt.
- CRLF-Zeilenenden erhalten. Dateien einzeln mit `git add` hinzufügen.
- Nur die Pumpe ist derzeit nicht testbar (Hardware fehlt), in Testlisten nur
  als "später" führen. Das 433-MHz-Halsband ist am 2026-09-24 auf der Hardware
  bestätigt. Der Metronom-Buzzer ist bewusst
  nicht Teil des Failsafes.
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
  `co_chg` (u8). Nie gespeichert: Enable-Flags und Laufzeitwerte. Grenzen (min <= max,
  Ausgang 6 max <= 100) setzt `state_apply()` immer, auch zur Laufzeit.
- Collar-Klicks (`sendCollar`, blockierend) laufen unverändert aus dem `async_tcp`-Task.
- Toy-Modell kommt in Block B.

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

## Weitere Ideen (nicht begonnen)

- Kanalfälle im Manuell-Menü tabellengetrieben machen.
- Visueller Indikator in der Web-UI: zeigt pro Kanal, ob gerade BLE oder Web
  den Ausgang steuert (BLE-Kanäle ausgegraut bzw. markiert), live ohne
  Refresh. Zustand pro Kanal (BLE oder Web) kommt per WebSocket und gehört in
  die JSON-Protokoll-Phase. Grundlage ist das Latch in `outputs_arbitrate()`
  (`state.ble.latch[]`, `state.ble.hold[]`): BLE behält den Ausgang auch bei
  Level 0, bis Web/Encoder ihn danach ändert oder BLE trennt. Die Lovense-App
  hält die Verbindung nach dem Schließen lange offen, deshalb taugt "verbunden"
  nicht als Kriterium. Der Server muss bei jeder Änderung des Zustands ein
  Update senden.
- JSON-Protokoll für den WebSocket und Migration von `data/script.js` (siehe Plan
  in der Unterhaltung; nicht umgesetzt).
- Optional: kleine WLAN-Diagnose (Status im Log, Neuverbinden). Einmal blieb
  das WLAN nach dem Start aus, bei erneutem Flashen desselben Stands war es
  wieder da (vermutlich Router/Koexistenz, nicht reproduzierbar).
- Merge nach `dev-webserver` erst nach ausdrücklicher Freigabe.

## Hinweise zur Skript-Methode

- Die Umbenennungsskripte liegen nicht im Repo (nur im Sitzungs-Scratchpad)
  und müssen bei Bedarf neu erstellt werden. Aufbau: Region per Marker
  bestimmen, pro Block die erwarteten Variablen mit Anzahl prüfen, Brücken-
  zeilen auslassen, Rundlauf (Rückübersetzung = Originaltext) prüfen, erst
  dann anwenden.
- Beim Aufruf aus Git-Bash Regex-Argumente mit `//` durch
  `MSYS_NO_PATHCONV=1` schützen (sonst werden sie als Pfad umgeschrieben).
