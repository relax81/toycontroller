#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

// SPIFFS and webserver root setup (moved out of main.cpp); the WiFi connection lives in
// wifi_manager.h. Uses the global server, which still lives in main.cpp.

void initFS();            // mount SPIFFS
void initWebServerRoot(); // "/" -> index.html and serveStatic for the SPIFFS files

#endif
