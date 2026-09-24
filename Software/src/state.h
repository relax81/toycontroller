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
//  - output ids (OutputId) are 1-based: 0 = OFF, 1-4 = PWM1-4, 5 = pump, 6 = collar.
//    They are used by BLE mapping (map[].output), hold[] and OutputItems.
//  - ble.map[0] / ble.in.vib[0] = V1 (Vibrate1), [1] = V2 (Vibrate2)

enum OutputId {
  OUT_OFF = 0,
  OUT_PWM1, OUT_PWM2, OUT_PWM3, OUT_PWM4,
  OUT_PUMP,
  OUT_COLLAR,
  OUT_ID_COUNT   // 7, size of hold[]
};

// state.out[] index (0-based) -> output id (1-based)
constexpr int outputId(int i) { return i + 1; }

struct OutputChannel {   // Ch1 .. Ch4
  bool enabled = false;
  int  on  = 0;          // seconds
  int  off = 0;          // seconds
  int  pwm = 0;          // 0 - 100
};

struct PumpChannel {
  bool enabled = false;
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

struct BleState {
  BleInput in;
  bool wasConnected = false;             // loop() only
  BtMap map[2] = { BtMap(0), BtMap(1) }; // V1 -> OFF, V2 -> PWM1 (as before)
  bool hold[OUT_ID_COUNT] = {};          // BLE holds this output id (loop() only)
  bool collarMapped = false;
};

struct AppState {
  OutputChannel out[4] = {};
  PumpChannel   pump;
  PwmRuntime    rt[4] = {};
  CollarState   collar;
  BuzzerState   buzzer;
  BleState      ble;
};

extern AppState state;

#endif
