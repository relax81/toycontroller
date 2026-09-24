#include <Arduino.h>
#include "true-credentials.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "config.h"
#include "wifi_setup.h"

// globals defined in main.cpp
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern bool WiFi_Enabled;
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
// Initialize WiFi
  void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    u8g2.clearBuffer();
    u8g2.setFont(font_status_messages);
    u8g2.drawStr(8, 20, "Connecting");
    u8g2.drawStr(40, 45, "WiFi");
    u8g2.sendBuffer();
    // while (WiFi.status() != WL_CONNECTED) {
    //   Serial.print('.');
    //   delay(1000);
    // }
    WiFi_Enabled = true;
    Serial.println(WiFi.localIP());
  }

// Web Server Root URL
  void initWebServerRoot() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");
  }
