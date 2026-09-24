#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// WiFi connection handling: credentials in NVS (Preferences), non-blocking state
// machine, setup portal (hotspot + captive portal) and mDNS (toycontroller.local).
// No blocking calls, loop() only drives it. The WiFi password is never logged.

#include <Arduino.h>

class AsyncWebServer;
class AsyncWebServerRequest;

enum class WifiState : uint8_t {
  NoCredentials,   // internal, only until wifi_manager_init() has started the portal
  Connecting,      // first attempt after boot (timeout, then Portal)
  Connected,
  Reconnecting,    // connection lost, retry every WIFI_RETRY_MS
  Portal           // hotspot "Toy-XXXX" with the setup portal, STA retry every 60 s
};

void wifi_manager_init();                      // call once from setup(), after initFS()
void wifi_manager_update(unsigned long nowMs); // call every loop()
void wifi_manager_attach(AsyncWebServer& server); // portal endpoints, call before initWebServerRoot()
WifiState wifi_state();
const char* wifi_state_text();                 // short text for the OLED (max. 13 chars)
bool wifi_connected();
bool wifi_portal_active();
bool wifi_portal_handle_root(AsyncWebServerRequest* request); // sends the portal page in portal state
bool wifi_portal_take_show_request();          // true once after the portal started (OLED should show it)
void wifi_draw_portal_screen();                // SSID / password / IP into the u8g2 buffer
void wifi_forget();                            // delete stored + pending credentials (keeps the "seeded" flag)

#endif
