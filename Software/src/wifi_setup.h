#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

// WiFi, SPIFFS and webserver root setup (moved out of main.cpp).
// Uses the globals u8g2, WiFi_Enabled and server, which still live in main.cpp.

void initFS();            // mount SPIFFS
void initWiFi();          // start WiFi station mode, does not wait for the connection
void initWebServerRoot(); // "/" -> index.html and serveStatic for the SPIFFS files

#endif
