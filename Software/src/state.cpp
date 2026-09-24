#include "state.h"
#include <type_traits>

// The state must be initialized at compile time (no dynamic initializer).
static_assert(std::is_literal_type<AppState>::value, "AppState must be a literal type");
static_assert(AppState().ble.map[0].output == 0 && AppState().ble.map[1].output == 1, "BLE map defaults: V1 -> OFF, V2 -> PWM1");
static_assert(AppState().buzzer.beatInterval == 60000 / AppState().buzzer.bpm, "beatInterval default = 60000 / bpm");
static_assert(sizeof(AppState().ble.hold) == OUT_ID_COUNT * sizeof(bool), "hold[] has one entry per output id");

AppState state;
