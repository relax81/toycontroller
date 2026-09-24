#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
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
static const unsigned long PORTAL_AFTER_LOSS_MS = 180000;    // connection lost this long -> portal
static const unsigned long PORTAL_STA_RETRY_MS = 60000;     // STA attempt while the portal runs
static const unsigned long PORTAL_STA_TRY_MS = 15000;       // how long one such attempt may take

static WifiState s_state = WifiState::NoCredentials;
static unsigned long s_stateSince = 0;
static unsigned long s_lastAttempt = 0;
static char s_ssid[33] = "";
static char s_pass[64] = "";
static bool s_usingPending = false;   // s_ssid/s_pass come from the portal and are not confirmed yet
static bool s_mdnsOn = false;

// portal
static DNSServer s_dns;
static char s_apSsid[12] = "";        // "Toy-AB12"
static char s_apPass[9] = "";         // 8 digits, only shown on the OLED
static bool s_showRequest = false;
static bool s_staTrying = false;      // STA attempt while the portal runs
static unsigned long s_staTryStart = 0;
static volatile bool s_restartPending = false;
static volatile unsigned long s_restartAt = 0;

// ---------------------------------------------------------------- credentials

static void loadStored() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  String ns = prefs.getString("ssid", "");
  String np = prefs.getString("pass", "");
  prefs.end();
  strncpy(s_ssid, ns.c_str(), sizeof(s_ssid) - 1);
  s_ssid[sizeof(s_ssid) - 1] = '\0';
  strncpy(s_pass, np.c_str(), sizeof(s_pass) - 1);
  s_pass[sizeof(s_pass) - 1] = '\0';
}

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
  String ps = prefs.getString("p_ssid", "");
  String pp = prefs.getString("p_pass", "");
  prefs.end();
  loadStored();
  if (ps.length() > 0) { // portal credentials wait for their first successful connection
    strncpy(s_ssid, ps.c_str(), sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_pass, pp.c_str(), sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
    s_usingPending = true;
  }
}

static void clearPending() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.remove("p_ssid");
  prefs.remove("p_pass");
  prefs.end();
}

static void confirmPending() { // the pending credentials worked: they become the stored ones
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("ssid", s_ssid);
  prefs.putString("pass", s_pass);
  prefs.remove("p_ssid");
  prefs.remove("p_pass");
  prefs.end();
  s_usingPending = false;
  Serial.println("[wifi] new credentials confirmed");
}

static void savePending(const String& newSsid, const String& newPass) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("p_ssid", newSsid);
  prefs.putString("p_pass", newPass);
  prefs.end();
}

// ---------------------------------------------------------------- station / mDNS

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
  if (s_usingPending) confirmPending();
  mdnsStart();
}

// ---------------------------------------------------------------- portal

static void enterPortal(unsigned long nowMs) {
  WiFi.disconnect();              // stop a running STA attempt, it would disturb the hotspot
  s_staTrying = false;
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(s_apSsid, sizeof(s_apSsid), "Toy-%02X%02X", mac[4], mac[5]);
  for (int i = 0; i < 8; i++) s_apPass[i] = '0' + (esp_random() % 10);
  s_apPass[8] = '\0';
  WiFi.softAP(s_apSsid, s_apPass);
  s_dns.start(53, "*", WiFi.softAPIP());
  setState(WifiState::Portal, nowMs);
  s_lastAttempt = nowMs;
  s_showRequest = true;
  Serial.printf("[wifi] portal started, hotspot \"%s\" ip=%s\n", s_apSsid, WiFi.softAPIP().toString().c_str());
}

static void leavePortal() {
  s_dns.stop();
  WiFi.softAPdisconnect(true);    // hotspot off, station stays
  WiFi.setHostname(WIFI_HOSTNAME);
  s_staTrying = false;
  Serial.println("[wifi] portal stopped");
}

// JSON string content, invalid UTF-8 bytes become '?'
static void jsonAppendEscaped(String& out, const String& s) {
  const uint8_t* p = (const uint8_t*)s.c_str();
  size_t n = s.length();
  size_t i = 0;
  while (i < n) {
    uint8_t c = p[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += (char)c;
      i++;
    }
    else if (c < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", c);
      out += b;
      i++;
    }
    else if (c < 0x80) {
      out += (char)c;
      i++;
    }
    else {
      size_t len = 0;
      if (c >= 0xC2 && c < 0xE0) len = 2;
      else if (c >= 0xE0 && c < 0xF0) len = 3;
      else if (c >= 0xF0 && c <= 0xF4) len = 4;
      bool ok = (len > 0) && (i + len <= n);
      for (size_t k = 1; ok && k < len; k++) ok = ((p[i + k] & 0xC0) == 0x80);
      if (ok) {
        for (size_t k = 0; k < len; k++) out += (char)p[i + k];
        i += len;
      }
      else {
        out += '?';
        i++;
      }
    }
  }
}

static void handleScan(AsyncWebServerRequest* request) {
  if (s_state != WifiState::Portal) {
    request->send(403, "text/plain", "Forbidden");
    return;
  }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (n == WIFI_SCAN_FAILED || request->hasParam("start")) { // start a new scan (async)
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  // scan finished: secured, named networks, strongest first, no duplicates
  const int MAXNET = 20;
  int idx[MAXNET];
  int cnt = 0;
  for (int i = 0; i < n && cnt < MAXNET; i++) {
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) continue;
    String name = WiFi.SSID(i);
    if (name.length() == 0) continue;
    bool dup = false;
    for (int j = 0; j < cnt; j++) {
      if (WiFi.SSID(idx[j]) == name) {
        dup = true;
        if (WiFi.RSSI(i) > WiFi.RSSI(idx[j])) idx[j] = i;
        break;
      }
    }
    if (!dup) idx[cnt++] = i;
  }
  for (int a = 0; a < cnt; a++) { // sort by RSSI, strongest first
    for (int b = a + 1; b < cnt; b++) {
      if (WiFi.RSSI(idx[b]) > WiFi.RSSI(idx[a])) {
        int t = idx[a];
        idx[a] = idx[b];
        idx[b] = t;
      }
    }
  }
  String json = "{\"scanning\":false,\"networks\":[";
  for (int a = 0; a < cnt; a++) {
    if (a > 0) json += ',';
    json += "{\"ssid\":\"";
    jsonAppendEscaped(json, WiFi.SSID(idx[a]));
    json += "\",\"rssi\":";
    json += WiFi.RSSI(idx[a]);
    json += '}';
  }
  json += "]}";
  request->send(200, "application/json", json);
}

static void handleSave(AsyncWebServerRequest* request) {
  if (s_state != WifiState::Portal || s_restartPending) {
    request->send(403, "text/plain", "Forbidden");
    return;
  }
  AsyncWebParameter* ps = request->getParam("ssid", true);
  AsyncWebParameter* pp = request->getParam("pass", true);
  if (!ps || !pp) {
    request->send(400, "text/plain", "Fehlende Felder.");
    return;
  }
  String newSsid = ps->value();
  String newPass = pp->value();
  if (newSsid.length() < 1 || newSsid.length() > 32) {
    request->send(400, "text/plain", "SSID: 1 bis 32 Zeichen.");
    return;
  }
  if (newPass.length() < 8 || newPass.length() > 63) {
    request->send(400, "text/plain", "Passwort: 8 bis 63 Zeichen.");
    return;
  }
  savePending(newSsid, newPass);
  request->send(200, "text/plain", "OK");
  Serial.printf("[wifi] credentials for \"%s\" saved as pending, restarting\n", newSsid.c_str());
  s_restartAt = millis() + 1500;
  s_restartPending = true;
}

static void handleNotFound(AsyncWebServerRequest* request) {
  if (s_state == WifiState::Portal) { // captive portal: everything unknown goes to the portal page
    request->redirect("http://192.168.4.1/");
  }
  else {
    request->send(404, "text/plain", "Not found");
  }
}

// setup page, plain HTML/JS, everything a user sets is written with textContent / encodeURIComponent
static const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Toycontroller WLAN</title>
<style>body{font-family:sans-serif;max-width:420px;margin:1em auto;padding:0 1em;background:#111;color:#eee}
h1{font-size:1.3em}button,input{width:100%;box-sizing:border-box;padding:.6em;margin:.3em 0;font-size:1em}
ul{list-style:none;padding:0}li{padding:.5em;border-bottom:1px solid #333;cursor:pointer}small{color:#999}a{color:#8cf}</style></head>
<body><h1>WLAN einrichten</h1>
<button id="scan">Netze suchen</button>
<ul id="list"></ul>
<input id="ssid" maxlength="32" placeholder="SSID" autocapitalize="none" autocorrect="off">
<input id="pass" type="password" maxlength="63" placeholder="Passwort (8-63 Zeichen)">
<label><input id="show" type="checkbox" style="width:auto"> Passwort anzeigen</label>
<button id="save">Speichern</button>
<p id="msg"></p>
<p><a href="/index.html">Steuerung &ouml;ffnen</a></p>
<script>
const $=id=>document.getElementById(id);let timer;
function poll(){fetch('/wifi/scan').then(r=>r.json()).then(d=>{
if(d.scanning){timer=setTimeout(poll,1500);return}
$('scan').disabled=false;const ul=$('list');ul.textContent='';
d.networks.forEach(n=>{const li=document.createElement('li');li.textContent=n.ssid+' ';
const s=document.createElement('small');s.textContent=n.rssi+' dBm';li.appendChild(s);
li.onclick=()=>{$('ssid').value=n.ssid;$('pass').focus()};ul.appendChild(li)});
if(!d.networks.length)$('msg').textContent='Keine Netze gefunden.';
}).catch(()=>{$('scan').disabled=false})}
$('scan').onclick=()=>{$('scan').disabled=true;$('msg').textContent='';
fetch('/wifi/scan?start=1').then(()=>{clearTimeout(timer);timer=setTimeout(poll,1500)})};
$('show').onchange=()=>{$('pass').type=$('show').checked?'text':'password'};
$('save').onclick=()=>{const s=$('ssid').value,p=$('pass').value;
if(!s||s.length>32||p.length<8||p.length>63){$('msg').textContent='SSID 1-32 Zeichen, Passwort 8-63 Zeichen.';return}
$('save').disabled=true;
fetch('/wifi/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
body:'ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)})
.then(r=>r.text().then(t=>{$('msg').textContent=r.ok?'Gespeichert. Das Gerät startet neu und verbindet sich. Klappt das nicht, erscheint der Hotspot wieder.':t;if(!r.ok)$('save').disabled=false}))
.catch(()=>{$('msg').textContent='Gespeichert, Verbindung getrennt. Das Gerät startet neu.'})};
$('scan').click();
</script></body></html>)rawliteral";

void wifi_manager_attach(AsyncWebServer& server) {
  server.on("/wifi/scan", HTTP_GET, handleScan);
  server.on("/wifi/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
}

bool wifi_portal_handle_root(AsyncWebServerRequest* request) {
  if (s_state != WifiState::Portal) return false;
  request->send_P(200, "text/html", PORTAL_HTML);
  return true;
}

bool wifi_portal_active() {
  return s_state == WifiState::Portal;
}

bool wifi_portal_take_show_request() {
  bool r = s_showRequest;
  s_showRequest = false;
  return r;
}

// OLED: SSID, password, IP. Fonts are picked at runtime: the password gets the biggest
// digit font that fits the width, then SSID and IP the biggest fonts that fit the rest
// of the 64 px height (candidate lists are ordered big to small).
void wifi_draw_portal_screen() {
  const uint8_t* pwFonts[] = {u8g2_font_fub20_tn, u8g2_font_fub17_tn, u8g2_font_fub14_tn, font_status_messages};
  const uint8_t* ssidFonts[] = {u8g2_font_fub14_tr, u8g2_font_fub11_tr, font_main_menu, font_manual_menu};
  const uint8_t* ipFonts[] = {u8g2_font_fub11_tr, font_manual_menu};
  const char* ip = "192.168.4.1";
  const int NPW = 4, NSSID = 4, NIP = 2;
  const int fit = 64 - 4;       // some air between the lines
  int bp = NPW - 1, bs = NSSID - 1, bi = NIP - 1; // smallest as fallback
  int hp = 0, hs = 0, hi = 0;
  bool found = false;
  for (int p = 0; p < NPW && !found; p++) {
    u8g2.setFont(pwFonts[p]);
    if (u8g2.getStrWidth(s_apPass) > 128) continue;
    int h1 = u8g2.getAscent() - u8g2.getDescent();
    for (int s = 0; s < NSSID && !found; s++) {
      u8g2.setFont(ssidFonts[s]);
      if (u8g2.getStrWidth(s_apSsid) > 128) continue;
      int h2 = u8g2.getAscent() - u8g2.getDescent();
      for (int i = 0; i < NIP && !found; i++) {
        u8g2.setFont(ipFonts[i]);
        if (u8g2.getStrWidth(ip) > 128) continue;
        int h3 = u8g2.getAscent() - u8g2.getDescent();
        if (h1 + h2 + h3 <= fit) {
          bp = p; bs = s; bi = i;
          hp = h1; hs = h2; hi = h3;
          found = true;
        }
      }
    }
  }
  const uint8_t* fonts[3] = {ssidFonts[bs], pwFonts[bp], ipFonts[bi]};
  const char* lines[3] = {s_apSsid, s_apPass, ip};
  if (!found) { // fallback: smallest fonts, measure them
    hp = hs = hi = 0;
    u8g2.setFont(fonts[0]); hs = u8g2.getAscent() - u8g2.getDescent();
    u8g2.setFont(fonts[1]); hp = u8g2.getAscent() - u8g2.getDescent();
    u8g2.setFont(fonts[2]); hi = u8g2.getAscent() - u8g2.getDescent();
  }
  int heights[3] = {hs, hp, hi};
  int gap = (64 - (hs + hp + hi)) / 4;
  if (gap < 0) gap = 0;
  int top = gap;
  for (int l = 0; l < 3; l++) {
    u8g2.setFont(fonts[l]);
    int x = (128 - u8g2.getStrWidth(lines[l])) / 2;
    if (x < 0) x = 0;
    u8g2.drawStr(x, top + u8g2.getAscent(), lines[l]);
    top += heights[l] + gap;
  }
}

// ---------------------------------------------------------------- state machine

void wifi_manager_init() {
  WiFi.persistent(false);       // the credentials live in our own NVS namespace
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // the state machine retries itself
  WiFi.setHostname(WIFI_HOSTNAME);
  loadCredentials();
  if (s_ssid[0] == '\0') {
    Serial.println("[wifi] no credentials stored");
    enterPortal(millis());
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
  Serial.printf("[wifi] connecting to \"%s\"%s\n", s_ssid, s_usingPending ? " (new credentials)" : "");
}

void wifi_manager_update(unsigned long nowMs) {
  if (s_restartPending && (long)(nowMs - s_restartAt) >= 0) {
    Serial.flush();
    ESP.restart();
  }
  bool up = (WiFi.status() == WL_CONNECTED);
  switch (s_state) {
    case WifiState::Connecting:
      if (up) {
        onConnected(nowMs);
      }
      else if (nowMs - s_stateSince >= WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("[wifi] connect timeout");
        if (s_usingPending) { // the new credentials failed, the old ones stay
          clearPending();
          s_usingPending = false;
          loadStored();
          Serial.println("[wifi] new credentials failed, old ones kept");
        }
        enterPortal(nowMs);
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
      else if (nowMs - s_stateSince >= PORTAL_AFTER_LOSS_MS) { // s_stateSince = moment of the loss
        Serial.println("[wifi] connection lost for 3 min, starting portal");
        enterPortal(nowMs);
      }
      else if (nowMs - s_lastAttempt >= WIFI_RETRY_MS) {
        s_lastAttempt = nowMs;
        startConnect();
      }
      break;
    case WifiState::Portal: {
      s_dns.processNextRequest();
      if (up) { // the router is back, portal no longer needed
        leavePortal();
        onConnected(nowMs);
        break;
      }
      bool clients = (WiFi.softAPgetStationNum() > 0);
      if (s_staTrying) {
        if (clients || nowMs - s_staTryStart >= PORTAL_STA_TRY_MS) {
          WiFi.disconnect();
          s_staTrying = false;
          s_lastAttempt = nowMs;
        }
      }
      else if (!clients && s_ssid[0] != '\0' && nowMs - s_lastAttempt >= PORTAL_STA_RETRY_MS) {
        startConnect();
        s_staTrying = true;
        s_staTryStart = nowMs;
      }
      break;
    }
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
    case WifiState::Portal:        return "Setup portal";
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
  prefs.remove("p_ssid");
  prefs.remove("p_pass");
  prefs.end(); // "seeded" stays, otherwise the seed would refill the credentials
  s_ssid[0] = '\0';
  s_pass[0] = '\0';
  s_usingPending = false;
}
