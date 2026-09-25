#ifndef OUTPUTS_H
#define OUTPUTS_H

// Output hardware (pins, LEDC PWM, buzzer, pump, collar), moved out of main.cpp.
// Works on the global AppState (state.h) and uses config.h / pinout.h.

#include "config.h"
#include "DogCollar3.h"

void outputs_init();   // output pins and LEDC channels, call once from setup()

void PWM_Output();     // web / manual mode, skips outputs held by BLE
void disable_Outputs(); // zero what is neither enabled nor held by BLE

void collar_send(CollarMode mode, int strength); // channel 1
void bluetooth_write_pwm(int output, int mappedPwm); // output id 1-6

void buzzer_Metronome(unsigned long nowMs);     // one step of the non-blocking metronome

void outputs_arbitrate(); // fills state.ble.hold[] / collarMapped, call once per loop() before PWM_Output()
void outputs_all_off();   // Ch1-4, pump and collar to "disabled" in the state

#if DEBUG_LEDC == 1
void debug_ledc();     // diagnostics only: configured vs. actual LEDC frequency
#endif

#endif
