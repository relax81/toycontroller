#ifndef OUTPUTS_H
#define OUTPUTS_H

// Output hardware (pins, LEDC PWM, buzzer, pump, collar), moved out of main.cpp.
// Works on the global AppState (state.h) and uses config.h / pinout.h.

#include "config.h"

void outputs_init();   // output pins and LEDC channels, call once from setup()

#if DEBUG_LEDC == 1
void debug_ledc();     // diagnostics only: configured vs. actual LEDC frequency
#endif

#endif
