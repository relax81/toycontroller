#include <Arduino.h>
#include <Preferences.h>
#include "state.h"
#include "settings.h"

static const char* const NS = "cfg";
static const unsigned long SETTINGS_SAVE_DELAY_MS = 5000;
static bool dirty = false;
static unsigned long lastChangeMs = 0;
static const uint8_t SETTINGS_VERSION = 1;

// accepted ranges (a stored value outside falls back to the default of that key)
static const int FS_MIN_S = 3, FS_MAX_S = 120;
static const int BPM_MIN = 1, BPM_MAX = 255;
static const int VOL_MIN = 0, VOL_MAX = 10;
static const int ON_MIN_MS = 10, ON_MAX_MS = 200;

static bool in_range(int v, int lo, int hi) { return v >= lo && v <= hi; }

// Loading runs in setup() (loop task, before any other task writes the state), so the
// events are applied directly. Min/max limits (min <= max, collar <= 100) are enforced by
// state_apply(); the order output -> max -> min makes the clamping lossless.
void settings_load() {
  Preferences prefs;
  if (!prefs.begin(NS, true)) { // namespace does not exist yet: defaults
    Serial.println("[cfg] no stored settings, defaults");
    return;
  }
  uint8_t ver = prefs.getUChar("ver", 0);
  if (ver > SETTINGS_VERSION) {
    Serial.printf("[cfg] settings version %u is newer than %u, reading the known keys\n", ver, SETTINGS_VERSION);
  }

  static const char* const kOut[2] = {"b0_out", "b1_out"};
  static const char* const kMin[2] = {"b0_min", "b1_min"};
  static const char* const kMax[2] = {"b0_max", "b1_max"};
  for (int k = 0; k < 2; k++) {
    int out = prefs.getUChar(kOut[k], state.ble.map[k].output);
    int mn = prefs.getUShort(kMin[k], state.ble.map[k].minPwm);
    int mx = prefs.getUShort(kMax[k], state.ble.map[k].maxPwm);
    if (!in_range(out, 0, OUT_ID_COUNT - 1)) out = state.ble.map[k].output;
    if (!in_range(mn, 0, 255)) mn = state.ble.map[k].minPwm;
    if (!in_range(mx, 0, 255)) mx = state.ble.map[k].maxPwm;
    state_apply({ EV_BT_OUT, (uint8_t)k, out });
    state_apply({ EV_BT_MAX, (uint8_t)k, mx });
    state_apply({ EV_BT_MIN, (uint8_t)k, mn });
  }

  int fs = prefs.getUShort("fs_to", state.failsafeTimeoutS);
  if (in_range(fs, FS_MIN_S, FS_MAX_S)) state_apply({ EV_FAILSAFE_TO, 0, fs });

  int vol = prefs.getUChar("bz_vol", state.buzzer.volume);
  int bpm = prefs.getUChar("bz_bpm", state.buzzer.bpm);
  int on = prefs.getUShort("bz_on", state.buzzer.onTimeMs);
  if (in_range(vol, VOL_MIN, VOL_MAX)) state_apply({ EV_BUZZ_VOL, 0, vol });
  if (in_range(bpm, BPM_MIN, BPM_MAX)) state_apply({ EV_BUZZ_BPM, 0, bpm });
  if (in_range(on, ON_MIN_MS, ON_MAX_MS)) state.buzzer.onTimeMs = on;

  state_apply({ EV_COLLAR_BTONLY, 0, prefs.getUChar("co_chg", state.collar.btOnlyChanges ? 1 : 0) ? 1 : 0 });
  prefs.end();

  state_ui_dirty = false;
  dirty = false; // loading is not a change
  Serial.printf("[cfg] loaded v%u: V1 out=%d %d-%d, V2 out=%d %d-%d, failsafe %d s, buzzer vol=%d bpm=%d on=%d ms, collar btOnlyChanges=%d\n",
                ver, state.ble.map[0].output, state.ble.map[0].minPwm, state.ble.map[0].maxPwm,
                state.ble.map[1].output, state.ble.map[1].minPwm, state.ble.map[1].maxPwm,
                state.failsafeTimeoutS, state.buzzer.volume, state.buzzer.bpm, state.buzzer.onTimeMs,
                state.collar.btOnlyChanges ? 1 : 0);
}

void settings_mark_dirty() {
  dirty = true;
  lastChangeMs = millis();
}

// write one key only if it differs from what the NVS holds
static int written = 0;
static void put_u8(Preferences& p, const char* key, uint8_t v) {
  if (!p.isKey(key) || p.getUChar(key, (uint8_t)~v) != v) { p.putUChar(key, v); written++; }
}
static void put_u16(Preferences& p, const char* key, uint16_t v) {
  if (!p.isKey(key) || p.getUShort(key, (uint16_t)~v) != v) { p.putUShort(key, v); written++; }
}

static void settings_save() {
  dirty = false;
  Preferences prefs;
  if (!prefs.begin(NS, false)) {
    Serial.println("[cfg] save failed (NVS)");
    return;
  }
  written = 0;
  put_u8(prefs, "ver", SETTINGS_VERSION);
  put_u8(prefs, "b0_out", (uint8_t)state.ble.map[0].output);
  put_u8(prefs, "b1_out", (uint8_t)state.ble.map[1].output);
  put_u16(prefs, "b0_min", (uint16_t)state.ble.map[0].minPwm);
  put_u16(prefs, "b0_max", (uint16_t)state.ble.map[0].maxPwm);
  put_u16(prefs, "b1_min", (uint16_t)state.ble.map[1].minPwm);
  put_u16(prefs, "b1_max", (uint16_t)state.ble.map[1].maxPwm);
  put_u16(prefs, "fs_to", (uint16_t)state.failsafeTimeoutS);
  put_u8(prefs, "bz_vol", (uint8_t)state.buzzer.volume);
  put_u8(prefs, "bz_bpm", (uint8_t)state.buzzer.bpm);
  put_u16(prefs, "bz_on", (uint16_t)state.buzzer.onTimeMs);
  put_u8(prefs, "co_chg", state.collar.btOnlyChanges ? 1 : 0);
  prefs.end();
  Serial.printf("[cfg] saved (%d keys written)\n", written);
}

void settings_update(unsigned long nowMs) {
  if (dirty && nowMs - lastChangeMs >= SETTINGS_SAVE_DELAY_MS) settings_save();
}

void settings_flush() {
  if (dirty) settings_save();
}

void settings_reset() {
  Preferences prefs;
  if (prefs.begin(NS, false)) {
    prefs.clear();
    prefs.end();
  }
  dirty = false;
}
