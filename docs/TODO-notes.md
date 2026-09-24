# TODO-Notizen

Nur Notizen, kein Code. Bewusst verschobene Punkte aus dem Umbau
(Branch `dev-wifi-ble-parallel`).

## Nebenläufigkeit

- **`values` (JSONVar) ist nicht thread-sicher.** Der WS-Handler
  (`handleWebSocketMessage_ws`, Task `async_tcp`) ändert und serialisiert
  `values`, während `ws_failsafe()` und `reset_Outputs()` (Task `loop`)
  ebenfalls hineinschreiben. Selten, aber ein Absturz ist möglich.
  Lösung: Ereignis-Queue (siehe unten), `values` nur noch aus `loop()`.
- Die Ausgangswerte (`ChN_*`) schreibt der `async_tcp`-Task direkt, `loop()`
  liest sie. Mehrere Felder (z. B. `on`/`off`) sind nicht atomar konsistent.
- BLE-Callbacks (NimBLE-Task) dürfen keinen State schreiben, nur
  `bt_vibration1/2` (und künftig Ereignisse einreihen).

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

- **Ereignis-Queue und `state_set()`** kommen in einem späteren Schritt:
  WS, BLE und Menü reihen Änderungen ein, nur `loop()` schreibt den State.
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
