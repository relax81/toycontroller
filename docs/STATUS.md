# Stand des Umbaus (Branch `dev-wifi-ble-parallel`)

Stand: 2026-09-24, Schritt 4 und `outputs.h/.cpp` abgeschlossen und getestet (letzter Code-Commit `de6562c`). Zum Weitermachen morgen: erst
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

## Weitere Ideen (nicht begonnen)

- Ereignis-Queue und `state_set()` (WS, BLE, Menü reihen ein, nur `loop()`
  schreibt den State); `values` (JSONVar) ist nicht thread-sicher.
- Kanalfälle im Manuell-Menü tabellengetrieben machen.
- Persistente Einstellungen über Preferences (NVS), JSON-Protokoll für den
  WebSocket und Migration von `data/script.js` (siehe Plan in der
  Unterhaltung; nichts davon ist umgesetzt).
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
