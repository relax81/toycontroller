#ifndef SETTINGS_H
#define SETTINGS_H

// Persistent settings in the NVS namespace "cfg" (WiFi credentials stay in "wifi").
// Stored: BLE mapping (output, min, max for V1/V2), failsafe timeout, buzzer values,
// collar "BT trigger only on level change". Never stored: enable flags and runtime values.
//
// Keys (type):  ver u8 | b0_out b1_out u8 | b0_min b0_max b1_min b1_max u16 |
//               fs_to u16 (s) | bz_vol bz_bpm u8 | bz_on u16 (ms) | co_chg u8

void settings_load();   // once in setup(), before the servers start; invalid or missing values -> defaults

// Saving: state_apply() calls settings_mark_dirty() when a stored value really changed. The
// values are written SETTINGS_SAVE_DELAY_MS after the last change (bundled, spares the flash),
// only the keys that differ from the NVS. Never called per encoder tick.
void settings_mark_dirty();
void settings_update(unsigned long nowMs); // loop(): saves once the delay has passed
void settings_flush();                     // saves at once if something changed (menu exit, before a restart)

#endif
