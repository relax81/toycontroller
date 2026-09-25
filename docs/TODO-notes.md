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
