#ifndef STATE_H
#define STATE_H

// Central application state (step 3: outputs Ch1-4 and pump, step 4: collar,
// buzzer, BLE input/mapping/arbitration).
// One global instance (state.cpp), no getters/setters, no locking.
// Everything is initialized at compile time (constexpr / default member
// initializers, arrays explicitly), so there is no dependency on the order of
// global initializers.
//
// Index rules:
//  - state.out[0] = Ch1 ... state.out[3] = Ch4 (0-based)
//  - output ids (OutputId) are 1-based: 0 = OFF, 1-4 = PWM1-4, 5 = pump, 6 = collar, 7 = metronome BPM.
//    They are used by BLE mapping (map[].output), hold[] and OutputItems.
//  - ble.map[0] / ble.in.vib[0] = V1 (Vibrate1), [1] = V2 (Vibrate2)

enum OutputId {
  OUT_OFF = 0,
  OUT_PWM1, OUT_PWM2, OUT_PWM3, OUT_PWM4,
  OUT_PUMP,
  OUT_COLLAR,
  OUT_BPM,       // BLE value sets the BPM of the buzzer metronome (min/max = BPM)
  OUT_ID_COUNT   // 8, size of hold[]
};

// state.out[] index (0-based) -> output id (1-based)
constexpr int outputId(int i) { return i + 1; }

struct OutputChannel {   // Ch1 .. Ch4
  bool enabled = false;
  int  on  = 0;          // tenths of a second (0 - 900 = 0 - 90 s)
  int  off = 0;          // tenths of a second (0 - 900 = 0 - 90 s)
  int  pwm = 0;          // 0 - 100
};

struct PumpChannel {
  bool enabled = false;
  int  on  = 0;          // tenths of a second (0 - 900 = 0 - 90 s)
  int  off = 0;          // tenths of a second, 0 = pump runs continuously
  int  pwm = 0;          // 0 - 100
};

struct PwmRuntime {      // runtime state of PWM_Output(), not user state
  bool paused = false;
  bool wasEnabled = false; // enabled and not held by BLE in the previous PWM_Output() call
  unsigned long timeStarted = 0;
  unsigned long timeStopped = 0;
};

struct CollarState {
  bool enabled = false;
  int  strength = 0;
  bool btOnlyChanges = true;             // send BLE shock only when the level changes
  int  previousShock = 30;
  unsigned long lastWakeup = 0;
  unsigned long keepAwakeMs = 120000;    // 2 minutes
};

struct BuzzerState {
  bool enabled = false;
  int  volume = 5;                       // 0 - 10
  int  bpm = 60;                         // 1 - 255
  int  onTimeMs = 50;
  bool isPlaying = false;                // runtime
  unsigned long previousMillis = 0;      // runtime
  int  beatInterval = 1000;              // runtime, 60000 / bpm
  int  bleBpm = 0;                       // runtime, BPM set by BLE (mapped to OUT_BPM), 0 = BLE does not play
};

struct BtMap {           // mapping of one BLE vibration channel to an output
  int  output;           // OutputId
  int  minPwm = 0;
  int  maxPwm = 255;
  bool paused = true;    // true = nothing sent to the output yet / already zeroed
  constexpr BtMap(int o = 0) : output(o) {}
};

struct BleInput {        // written by the NimBLE task, read by loop()
  bool connected = false;
  int  vib[2] = {0, 0};
  int  rotation = 0;
  int  airLevel = 0;
};

struct BleLatch {        // BLE keeps an output at level 0 too (loop() only)
  bool active = false;   // BLE has commanded `output` and nothing else changed it since
  int  output = 0;       // OutputId the latch belongs to
  int  prevVib = 0;      // vib value seen in the previous loop()
  int  snap[4] = {};     // web/manual settings of the output at the last BLE command
};

struct BleState {
  BleInput in;
  bool wasConnected = false;             // loop() only
  BtMap map[2] = { BtMap(0), BtMap(1) }; // V1 -> OFF, V2 -> PWM1 (as before)
  BleLatch latch[2];                     // per vibration channel
  bool hold[OUT_ID_COUNT] = {};          // BLE holds this output id (loop() only)
  bool collarMapped = false;
  int  toyModel = 0;                     // index into TOY_MODELS (0 = Dolce), applied at the next start
  bool toyRestartPending = false;        // a model change restarts the device (loop())
  unsigned long toyChangedMs = 0;
};

struct AppState {
  OutputChannel out[4] = {};
  PumpChannel   pump;
  PwmRuntime    pumpRt;
  PwmRuntime    rt[4] = {};
  CollarState   collar;
  BuzzerState   buzzer;
  BleState      ble;
  int           failsafeTimeoutS = 15;   // web failsafe: no WS traffic for this long -> outputs off (3-120)
};

// ---------------------------------------------------------------------------
// Event queue: loop() is the only writer of `state`. Other tasks (async_tcp for the
// WebSocket, the NimBLE task) call state_set(), which queues an event. state_drain()
// applies the queued events at the start of every loop() pass. Called from loop()
// itself, state_set() applies the event immediately.
// ---------------------------------------------------------------------------
#include <stdint.h>

enum EventType : uint8_t {
  EV_OUT_ENABLE,      // idx = 0-3 (Ch1-4), val = 0/1
  EV_OUT_ON,          // idx, val = seconds
  EV_OUT_OFF,         // idx, val = seconds
  EV_OUT_PWM,         // idx, val = 0-100
  EV_PUMP_ENABLE,     // val = 0/1
  EV_PUMP_PWM,        // val = 0-100
  EV_PUMP_ON,         // val = tenths of a second
  EV_PUMP_OFF,        // val = tenths of a second
  EV_COLLAR_ENABLE,   // val = 0/1
  EV_COLLAR_STRENGTH, // val = 0-100
  EV_COLLAR_BTONLY,   // val = 0/1
  EV_BUZZ_ENABLE,     // val = 0/1
  EV_BUZZ_BPM,        // val = 1-255
  EV_BUZZ_VOL,        // val = 0-10
  EV_BLE_CONN,        // val = 0/1 (0 also zeroes vib[])
  EV_BLE_VIB,         // val = vib[0] | vib[1] << 16 (both values at once)
  EV_BLE_ROT,         // val
  EV_BLE_AIR,         // val
  EV_BT_OUT,          // idx = 0/1 (V1/V2), val = OutputId
  EV_BT_MIN,          // idx, val
  EV_BT_MAX,          // idx, val
  EV_FAILSAFE_TO,     // val = seconds (3-120)
  EV_ALL_OFF,         // Ch1-4, pump and collar disabled
  EV_BLE_TOY,         // val = index into TOY_MODELS (device restarts)
  EV_TYPE_COUNT
};

struct Event {
  uint8_t type;   // EventType
  uint8_t idx;
  int32_t val;
};

void state_init();                                       // call once at the start of setup() (in the loop task)
bool state_set(EventType type, int idx, int32_t val);    // false: the event was dropped
void state_drain();                                      // loop() only, call first in every pass
void state_apply(const Event& e);                        // loop() only

// set true by state_apply() for changes the web UI must be told about (loop() only)
extern bool state_ui_dirty;
// event diagnostics ([evq] log lines are printed from state_drain())
extern volatile uint32_t evq_dropped;      // value events dropped on overflow
extern volatile uint32_t evq_crit_failed;  // critical events that did not fit (fallback flags set)

extern AppState state;

#endif
