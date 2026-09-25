#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "config.h"
#include "wifi_setup.h"
#include "wifi_manager.h"

// globals defined in main.cpp
extern AsyncWebServer server;

// Initialize SPIFFS
  void initFS() {
    if (!SPIFFS.begin()) {
      Serial.println("An error has occurred while mounting SPIFFS");
    }
    else{
    Serial.println("SPIFFS mounted successfully");
    }
  }
// Web Server Root URL
  void initWebServerRoot() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (wifi_portal_handle_root(request)) return; // hotspot: setup page
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");
  }
