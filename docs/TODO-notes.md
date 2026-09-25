# TODO-Notizen

Nur Notizen, kein Code. Bewusst verschobene Punkte aus dem Umbau
(Branch `dev-wifi-ble-parallel`).

## Nebenläufigkeit

- ~~`values` (JSONVar) und die Ausgangswerte wurden aus mehreren Tasks
  geschrieben.~~ **Erledigt (Block A):** Ereignis-Queue, `loop()` schreibt den State
  und baut `values`; BLE-Callbacks und WS-Handler reihen nur ein.
- Offen: `sendCollar()` blockiert weiterhin (siehe 433-MHz-Abschnitt), die
  Collar-Klicks laufen aus dem `async_tcp`-Task und lesen `state.collar.*` ohne Sperre.

## AsyncTCP / WebSocket

- **Seltener Absturz bei RST** (Use-after-free in AsyncTCP 1.1.1, `AsyncClient::_error()` schreibt auf einen
  bereits freigegebenen `pcb`; Crash in `AsyncServer::_accept`). Bewusst nicht gepatcht, Details in
  `STATUS.md`. Neustart führt in den sicheren Zustand. Späterer Weg, falls es im Alltag stört: gepflegte
  AsyncTCP/ESPAsyncWebServer-Versionen prüfen (Kompatibilität mit `espressif32 @ ~3.5.0` ungeklärt) oder
  `_error`/`_close` in einer lokalen Kopie absichern.
- **Stummer Client ohne volle Queue** wird vom Stall-Schutz nicht erkannt (nur im synthetischen
  Parallelaufbau beobachtet). Möglicher Weg: Client meldet sein `n`, Server schließt bei dauerhaftem Rückstand.

## AsyncTCP / WebSocket

- **Seltener Absturz bei RST** (Use-after-free in AsyncTCP 1.1.1, `AsyncClient::_error()` schreibt auf einen
  bereits freigegebenen `pcb`; Crash in `AsyncServer::_accept`). Bewusst nicht gepatcht, Details in
  `STATUS.md`. Neustart führt in den sicheren Zustand. Späterer Weg, falls es im Alltag stört: gepflegte
  AsyncTCP/ESPAsyncWebServer-Versionen prüfen (Kompatibilität mit `espressif32 @ ~3.5.0` ungeklärt) oder
  `_error`/`_close` in einer lokalen Kopie absichern.
- **Stummer Client ohne volle Queue** wird vom Stall-Schutz nicht erkannt (nur im synthetischen
  Parallelaufbau beobachtet). Möglicher Weg: Client meldet sein `n`, Server schließt bei dauerhaftem Rückstand.

## Ausgangslogik

- ~~**`timeStarted` wird beim Einschalten eines Kanals nicht gesetzt**
  (Ch1-4, `PWM_Output()`): Ein Kanal mit Off > 0, der lange nach dem Start
  eingeschaltet wird, pausiert im ersten Durchlauf sofort, holt die
  Off-Zeit nach und läuft erst dann normal.~~ **Erledigt:** `PWM_Output()`
  erkennt pro Kanal die steigende Flanke von "aktiv" (`enabled` und nicht
  von BLE gehalten, `PwmRuntime::wasEnabled`) und setzt dann
  `timeStarted = millis()` und `paused = false`. Der Kanal startet mit der
  On-Zeit. Gilt auch, wenn BLE einen Kanal wieder freigibt: der Zyklus
  beginnt dann von vorn mit der On-Phase.
- ~~Die vier Kanalblöcke in `PWM_Output()` (und `disable_Outputs()`) sind
  Kopien.~~ **Erledigt:** Schleife über `state.out[i]` mit
  `pwmOutChannel[i]` (`config.h`) und `bt_hold[i + 1]`. Offen: die
  Kanalfälle im Manuell-Menü (`buttonMenuManual`, `displayMenuManual`) sind
  noch Kopien.

## State / Architektur

- ~~Ereignis-Queue und `state_set()`~~ **Erledigt (Block A, siehe STATUS.md).**
- **`Collar_Enable` und `bt_hold[]` sind noch nicht im State** (bleiben
  vorerst in `main.cpp`), ebenso BT-Zuordnung, Buzzer und Menüvariablen.
- Indizes: `state.out[0]` = Ch1 ... `state.out[3]` = Ch4, aber `bt_hold[]`
  und `OutputItems` zählen ab 1 (1-4 PWM, 5 Pumpe, 6 Halsband).

## 433-MHz-Halsband (DogCollar3)

- **`sendCollar()` blockiert.** Das Bitmuster wird mit `delayMicroseconds`
  gesendet (je Bit ca. 1,1 ms, etwa 40 Bit, dazu Start und Pausen) und
  3-mal wiederholt: grob **150 ms pro Aufruf** (Schätzung, nicht gemessen).
- **Aufrufer aus zwei Tasks:** aus dem `async_tcp`-Task (WS-Klicks
  Beep/Vibe/Shock in `handleWebSocketMessage_ws`) und aus `loop()`
  (Keep-alive, `bluetooth_write_pwm` Fall 6). Sendet der Web-Klick, während
  `loop()` gerade sendet, überlagern sich die Bitmuster am selben Pin und
  beide Frames sind unbrauchbar. Außerdem blockiert der Klick den
  `async_tcp`-Task für ca. 150 ms (WS/HTTP stehen).
- **Geplante Lösung:** Klicks als Ereignisse einreihen, nur `loop()` sendet
  (ein Sender, Rate-Limit), ggf. kritischer Abschnitt oder RMT.
  **Erst mit Halsband- oder Empfängertest umstellen.** In Schritt 4 wird am
  Sendeverhalten (DogCollar3, `previousShock`, Keep-alive, Aufrufer) nichts
  geändert, nur umbenannt.
- `dg` (DogCollar) ruft `pinMode` im Konstruktor beim statischen Init auf.

## Sonstiges

- BLE-Anfragen `GetCap;`, `AutoTime;`, `AI,ai;`, `GetAS,<2>;`, `GetLight`,
  `Collect:` werden mit `ERR;` beantwortet (war schon so).
- 433-MHz-Halsband unter WLAN+BLE-Last noch ungetestet (blockierendes
  `delayMicroseconds` in `DogCollar3`).
- Debug-Makros (`DEBUG`, `DEBUG_LEDC`, `DEBUG_HEAP`) stehen noch in
  `main.cpp`, später nach `config.h`.

## Toy-Modelle und BLE-Protokoll

- **Nicht verifiziert:** Namen (`LVS-<Buchstabe>001`), Firmware-Zahl `40` und die Gen-2-UUID (`6e400001-...`) für alle
  Modelle in `toy_models.cpp`. Sicher ist nur Dolce (bisherige Identität). Zum Prüfen: Mitschnitt an einem echten Toy
  (z. B. nRF Connect: Name, Service-UUID, Antwort auf `DeviceType;`).
- **Falls die App ein Modell nicht erkennt:** Gen-3-UUIDs (`XY300001-002Z-4bd4-bbd5-a6920e4c5653`, laut buttplug.io
  X = 0x4/0x5, Y = 0x0-0xf, Z = 0x3/0x4) als Tabellenfeld je Modell. Beobachtung (nicht bestätigt): das erste Byte
  entspricht dem ASCII-Buchstaben des Modells (0x53 = S, 0x5a = Z, 0x57 = W, 0x50 = P, 0x43 = C), welches Suffix
  (`0023`/`0024`) zu welchem Modell gehört, ist unbekannt.
- **Nicht umgesetzt, antwortet mit `ERR;`:** `Mply:a:b:c;` (Mehrfunktionsgeräte, `-1` = unverändert), `Air:In`/`Air:Out`,
  `Preset:n;`, `GetBatch;`, `GetPatten;`, `GetAS;`/`AutoSwith`, `GetLight;`/`Light`, `GetAlight;`/`ALight`,
  `GetLevel;`/`SetLevel` (Domi), `StartMove:1;`/`StopMove:1;`. Für Domi wären harmlose Antworten auf `GetLevel`,
  `Light` und `AutoSwith` nötig. Mögliche Idee: unbekannte Abfragen mit einem Standardwert statt `ERR;` beantworten.
- **Nicht in der Auswahl:** Max (Air-Level 0-5 zu grob, entfernt), Modelle mit `Mply:` (Solace, Flexer, Lapis, Gemini
  u. a.). Vor deren Einbau: Format und Kanalzuordnung per echtem Mitschnitt klären.
- `RotateChange;` (Nora) wird nur mit `OK;` beantwortet, die Drehrichtung geht an keinen Ausgang.
