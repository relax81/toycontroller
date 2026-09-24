#ifndef SETTINGS_H
#define SETTINGS_H

// Persistent settings in the NVS namespace "cfg" (WiFi credentials stay in "wifi").
// Stored: BLE mapping (output, min, max for V1/V2), failsafe timeout, buzzer values,
// collar "BT trigger only on level change". Never stored: enable flags and runtime values.
//
// Keys (type):  ver u8 | b0_out b1_out u8 | b0_min b0_max b1_min b1_max u16 |
//               fs_to u16 (s) | bz_vol bz_bpm u8 | bz_on u16 (ms) | co_chg u8

void settings_load();   // once in setup(), before the servers start; invalid or missing values -> defaults

#endif
