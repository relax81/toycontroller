#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "config.h"
#include "wifi_manager.h"

// Optional development seed: copied into NVS exactly once (flag "seeded"), the file is
// untracked and never committed. Without the file nothing is seeded.
#if __has_include("true-credentials.h")
#include "true-credentials.h"
#define WIFI_HAS_SEED 1
#else
#define WIFI_HAS_SEED 0
#endif

// globals defined in main.cpp
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

static const char* const WIFI_HOSTNAME = "toycontroller";
static const char* const NVS_NAMESPACE = "wifi";
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000; // first attempt after boot
static const unsigned long WIFI_RETRY_MS = 10000;           // between reconnect attempts

static WifiState s_state = WifiState::NoCredentials;
static unsigned long s_stateSince = 0;
static unsigned long s_lastAttempt = 0;
static char s_ssid[33] = "";
static char s_pass[64] = "";
static bool s_mdnsOn = false;

static void loadCredentials() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
#if WIFI_HAS_SEED
  if (!prefs.getBool("seeded", false)) {
    prefs.putBool("seeded", true); // once only, wifi_forget() does not reset the flag
    if (prefs.getString("ssid", "").length() == 0) {
      prefs.putString("ssid", ssid);
      prefs.putString("pass", password);
      Serial.println("[wifi] credentials seeded from true-credentials.h");
    }
  }
#endif
  String ns = prefs.getString("ssid", "");
  String np = prefs.getString("pass", "");
  prefs.end();
  strncpy(s_ssid, ns.c_str(), sizeof(s_ssid) - 1);
  s_ssid[sizeof(s_ssid) - 1] = '\0';
  strncpy(s_pass, np.c_str(), sizeof(s_pass) - 1);
  s_pass[sizeof(s_pass) - 1] = '\0';
}

static void startConnect() {
  WiFi.begin(s_ssid, s_pass); // does not block
}

static void mdnsStart() {
  if (MDNS.begin(WIFI_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    s_mdnsOn = true;
    Serial.printf("[wifi] mDNS %s.local\n", WIFI_HOSTNAME);
  }
  else {
    Serial.println("[wifi] mDNS start failed");
  }
}

static void mdnsStop() {
  if (s_mdnsOn) {
    MDNS.end();
    s_mdnsOn = false;
  }
}

static void setState(WifiState st, unsigned long nowMs) {
  s_state = st;
  s_stateSince = nowMs;
}

static void onConnected(unsigned long nowMs) {
  setState(WifiState::Connected, nowMs);
  Serial.printf("[wifi] connected ip=%s rssi=%d heap=%u\n", WiFi.localIP().toString().c_str(), WiFi.RSSI(), ESP.getFreeHeap());
  mdnsStart();
}

void wifi_manager_init() {
  WiFi.persistent(false);       // the credentials live in our own NVS namespace
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // the state machine retries itself
  WiFi.setHostname(WIFI_HOSTNAME);
  loadCredentials();
  if (s_ssid[0] == '\0') {
    setState(WifiState::NoCredentials, millis());
    Serial.println("[wifi] no credentials stored");
    return;
  }
  u8g2.clearBuffer();
  u8g2.setFont(font_status_messages);
  u8g2.drawStr(8, 20, "Connecting");
  u8g2.drawStr(40, 45, "WiFi");
  u8g2.sendBuffer();
  startConnect();
  s_lastAttempt = millis();
  setState(WifiState::Connecting, millis());
  Serial.printf("[wifi] connecting to \"%s\"\n", s_ssid);
}

void wifi_manager_update(unsigned long nowMs) {
  bool up = (WiFi.status() == WL_CONNECTED);
  switch (s_state) {
    case WifiState::Connecting:
      if (up) {
        onConnected(nowMs);
      }
      else if (nowMs - s_stateSince >= WIFI_CONNECT_TIMEOUT_MS) {
        setState(WifiState::Reconnecting, nowMs);
        s_lastAttempt = nowMs;
        Serial.println("[wifi] connect timeout, retrying");
      }
      break;
    case WifiState::Connected:
      if (!up) {
        mdnsStop();
        setState(WifiState::Reconnecting, nowMs);
        s_lastAttempt = nowMs;
        Serial.println("[wifi] connection lost");
      }
      break;
    case WifiState::Reconnecting:
      if (up) {
        onConnected(nowMs);
      }
      else if (nowMs - s_lastAttempt >= WIFI_RETRY_MS) {
        s_lastAttempt = nowMs;
        startConnect();
      }
      break;
    case WifiState::NoCredentials:
      break;
  }
}

WifiState wifi_state() {
  return s_state;
}

const char* wifi_state_text() {
  switch (s_state) {
    case WifiState::NoCredentials: return "No WiFi data";
    case WifiState::Connecting:    return "Connecting...";
    case WifiState::Connected:     return "Connected";
    case WifiState::Reconnecting:  return "Reconnecting";
  }
  return "";
}

bool wifi_connected() {
  return s_state == WifiState::Connected;
}

void wifi_forget() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end(); // "seeded" stays, otherwise the seed would refill the credentials
  s_ssid[0] = '\0';
  s_pass[0] = '\0';
}
