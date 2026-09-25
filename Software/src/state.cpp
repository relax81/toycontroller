#include <Arduino.h>
#include "state.h"
#include "toy_models.h"
#include "settings.h"
#include <type_traits>

// The state must be initialized at compile time (no dynamic initializer).
static_assert(std::is_literal_type<AppState>::value, "AppState must be a literal type");
static_assert(AppState().ble.map[0].output == 0 && AppState().ble.map[1].output == 1, "BLE map defaults: V1 -> OFF, V2 -> PWM1");
static_assert(AppState().buzzer.beatInterval == 60000 / AppState().buzzer.bpm, "beatInterval default = 60000 / bpm");
static_assert(sizeof(AppState().ble.hold) == OUT_ID_COUNT * sizeof(bool), "hold[] has one entry per output id");

AppState state;

void outputs_all_off(); // outputs.cpp

// ---------------------------------------------------------------------------
// Event queue
// ---------------------------------------------------------------------------
static const int EVQ_LENGTH = 64;
static const TickType_t EVQ_CRIT_WAIT = pdMS_TO_TICKS(10); // critical events may wait this long for a free slot

static QueueHandle_t evq = nullptr;
static TaskHandle_t loopTask = nullptr;

// fallbacks when a critical event does not fit into the queue (evaluated first in state_drain())
static volatile bool all_off_req = false;
static volatile bool ble_disc_req = false;

bool state_ui_dirty = false;
volatile uint32_t evq_dropped = 0;
volatile uint32_t evq_crit_failed = 0;

void state_init() {
  loopTask = xTaskGetCurrentTaskHandle();
  evq = xQueueCreate(EVQ_LENGTH, sizeof(Event));
}

// events that switch something off must not get lost
static bool event_is_critical(const Event& e) {
  switch (e.type) {
    case EV_ALL_OFF:
    case EV_OUT_OFF:
    case EV_BLE_CONN:
    case EV_BLE_VIB:
      return true;
    case EV_OUT_ENABLE:
    case EV_PUMP_ENABLE:
    case EV_COLLAR_ENABLE:
    case EV_BUZZ_ENABLE:
      return e.val == 0;
    default:
      return false;
  }
}

// BLE mapping limits: min <= max, the collar (output 6) takes 0-100 at most
static void bt_clamp(BtMap& m) {
  if (m.minPwm < 0) m.minPwm = 0;
  if (m.maxPwm < 0) m.maxPwm = 0;
  if (m.output == OUT_COLLAR && m.maxPwm > 100) m.maxPwm = 100;
  if (m.minPwm > m.maxPwm) m.minPwm = m.maxPwm;
}

void state_apply(const Event& e) {
  const int i = e.idx;
  const bool b = e.val != 0;
  switch (e.type) {
    case EV_OUT_ENABLE:      if (i < 4) { state.out[i].enabled = b; state_ui_dirty = true; } break;
    case EV_OUT_ON:          if (i < 4) { state.out[i].on  = e.val; state_ui_dirty = true; } break;
    case EV_OUT_OFF:         if (i < 4) { state.out[i].off = e.val; state_ui_dirty = true; } break;
    case EV_OUT_PWM:         if (i < 4) { state.out[i].pwm = e.val; state_ui_dirty = true; } break;
    case EV_PUMP_ENABLE:     state.pump.enabled = b;          state_ui_dirty = true; break;
    case EV_PUMP_PWM:        state.pump.pwm = e.val;          state_ui_dirty = true; break;
    case EV_PUMP_ON:         state.pump.on  = e.val;          state_ui_dirty = true; break;
    case EV_PUMP_OFF:        state.pump.off = e.val;          state_ui_dirty = true; break;
    case EV_COLLAR_ENABLE:   state.collar.enabled = b;        state_ui_dirty = true; break;
    case EV_COLLAR_STRENGTH: state.collar.strength = e.val;   state_ui_dirty = true; break;
    case EV_COLLAR_BTONLY:
      if (state.collar.btOnlyChanges != b) { state.collar.btOnlyChanges = b; settings_mark_dirty(); }
      break;
    case EV_BUZZ_ENABLE:     state.buzzer.enabled = b;        state_ui_dirty = true; break;
    case EV_BUZZ_BPM:
      if (state.buzzer.bpm != e.val) { state.buzzer.bpm = e.val; settings_mark_dirty(); }
      state_ui_dirty = true;
      break;
    case EV_BUZZ_VOL:
      if (state.buzzer.volume != e.val) { state.buzzer.volume = e.val; settings_mark_dirty(); }
      state_ui_dirty = true;
      break;
    case EV_BLE_CONN:
      state.ble.in.connected = b;
      if (!b) { // failsafe: no client, no output
        state.ble.in.vib[0] = 0;
        state.ble.in.vib[1] = 0;
      }
      break;
    case EV_BLE_VIB:
      state.ble.in.vib[0] = (int)(e.val & 0xFFFF);
      state.ble.in.vib[1] = (int)((uint32_t)e.val >> 16);
      break;
    case EV_BLE_ROT:         state.ble.in.rotation = e.val;   break;
    case EV_BLE_AIR:         state.ble.in.airLevel = e.val;   break;
    case EV_BT_OUT:
    case EV_BT_MIN:
    case EV_BT_MAX:
      if (i < 2) {
        BtMap& m = state.ble.map[i];
        const BtMap before = m;
        if (e.type == EV_BT_OUT) { if (e.val >= 0 && e.val < OUT_ID_COUNT) m.output = e.val; }
        else if (e.type == EV_BT_MIN) m.minPwm = e.val;
        else m.maxPwm = e.val;
        bt_clamp(m);
        if (m.output != before.output || m.minPwm != before.minPwm || m.maxPwm != before.maxPwm) settings_mark_dirty();
      }
      break;
    case EV_BLE_TOY:
      if (e.val >= 0 && e.val < TOY_MODEL_COUNT && state.ble.toyModel != e.val) {
        state.ble.toyModel = e.val;
        state.ble.toyChangedMs = millis();
        state.ble.toyRestartPending = true; // the new identity applies after a restart (loop())
        settings_mark_dirty();
      }
      state_ui_dirty = true;
      break;
    case EV_FAILSAFE_TO:
      if (e.val >= 3 && e.val <= 120 && state.failsafeTimeoutS != e.val) { state.failsafeTimeoutS = e.val; settings_mark_dirty(); }
      break;
    case EV_ALL_OFF:
      outputs_all_off();
      state_ui_dirty = true;
      break;
    default:
      break;
  }
}

bool state_set(EventType type, int idx, int32_t val) {
  Event e = { (uint8_t)type, (uint8_t)idx, val };
  if (evq == nullptr || xTaskGetCurrentTaskHandle() == loopTask) { // loop() is the writer itself
    state_apply(e);
    return true;
  }
  if (event_is_critical(e)) {
    if (xQueueSend(evq, &e, EVQ_CRIT_WAIT) == pdTRUE) return true;
    evq_crit_failed++;
    if (e.type == EV_BLE_CONN || e.type == EV_BLE_VIB) ble_disc_req = true; // BLE: assume "gone", zero the input
    else all_off_req = true;                                                // web: everything off
    return false;
  }
  if (xQueueSend(evq, &e, 0) == pdTRUE) return true;
  evq_dropped++;
  return false;
}

void state_drain() {
  static uint32_t loggedDropped = 0, loggedCrit = 0;
  if (all_off_req) {
    all_off_req = false;
    state_apply({ EV_ALL_OFF, 0, 0 });
  }
  if (ble_disc_req) {
    ble_disc_req = false;
    state_apply({ EV_BLE_CONN, 0, 0 });
  }
  if (evq != nullptr) {
    Event e;
    for (int n = 0; n < EVQ_LENGTH && xQueueReceive(evq, &e, 0) == pdTRUE; n++) state_apply(e);
  }
  if (evq_dropped != loggedDropped || evq_crit_failed != loggedCrit) {
    loggedDropped = evq_dropped;
    loggedCrit = evq_crit_failed;
    Serial.printf("[evq] dropped=%u crit_failed=%u\n", (unsigned)loggedDropped, (unsigned)loggedCrit);
  }
}
