#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// WiFi connection handling: credentials in NVS (Preferences), non-blocking state
// machine, mDNS (toycontroller.local). No blocking calls, loop() only drives it.
// The WiFi password is never logged.

#include <Arduino.h>

enum class WifiState : uint8_t {
  NoCredentials,   // nothing stored in NVS
  Connecting,      // first attempt after boot (timeout, then Reconnecting)
  Connected,
  Reconnecting     // retry every WIFI_RETRY_MS
};

void wifi_manager_init();                      // call once from setup(), after initFS()
void wifi_manager_update(unsigned long nowMs); // call every loop()
WifiState wifi_state();
const char* wifi_state_text();                 // short text for the OLED (max. 13 chars)
bool wifi_connected();
void wifi_forget();                            // delete the stored credentials (keeps the "seeded" flag)

#endif
