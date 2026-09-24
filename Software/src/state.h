#ifndef STATE_H
#define STATE_H

// Central application state. Step 3: only the outputs Ch1-4 and the pump.
// One global instance (state.cpp), no getters/setters, no locking.
// Index: state.out[0] = Ch1 ... state.out[3] = Ch4 (bt_hold[] and OutputItems
// count from 1, this array from 0).

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
  unsigned long timeStarted = 0;
  unsigned long timeStopped = 0;
};

struct AppState {
  OutputChannel out[4];
  PumpChannel   pump;
  PwmRuntime    rt[4];
};

extern AppState state;

#endif
