#include <Arduino.h>
#include <Arduino_JSON.h>
#include "ESPAsyncWebServer.h"
#include "state.h"
#include "protocol.h"
#include "api.h"

// The HTTP API does not touch ws_last_seen: calls do not count for the web failsafe.

static const size_t   BODY_MAX = 512;       // same limit as a WebSocket message
static const uint32_t FOR_MAX_S = 3600;
static const uint32_t FOR_MS_MIN = 100;      // for_ms=<100 - 3600000>
static const uint32_t FOR_MS_MAX = 3600000;
static const int      MAX_TIMERS = 8;
static const uint32_t COLLAR_GAP_MS = 300;  // minimum gap between two collar.* commands
static const int      RESTART_IN_S = 2;     // loop() restarts 2 s after a toy model change
static const int      MAX_KEYS = 64;        // protocol.cpp: key masks are 64 bit

// ---------------------------------------------------------------------------
// Replies
// ---------------------------------------------------------------------------
static void add_headers(AsyncWebServerResponse* r) {
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Cache-Control", "no-store");
}

static void reply(AsyncWebServerRequest* req, int code, const String& body) {
  AsyncWebServerResponse* r = req->beginResponse(code, "application/json", body);
  add_headers(r);
  req->send(r);
}

static void reply_err(AsyncWebServerRequest* req, int code, const char* c, const char* msg) {
  reply(req, code, String("{\"ok\":false,\"code\":\"") + c + "\",\"msg\":\"" + msg + "\"}");
}

static void handle_options(AsyncWebServerRequest* req) {
  AsyncWebServerResponse* r = req->beginResponse(204);
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  r->addHeader("Access-Control-Allow-Headers", "Content-Type");
  r->addHeader("Access-Control-Allow-Private-Network", "true");
  r->addHeader("Access-Control-Max-Age", "600");
  req->send(r);
}

// ---------------------------------------------------------------------------
// for= timers (set from the async_tcp task, expired in loop())
// ---------------------------------------------------------------------------
struct ApiTimer {
  bool used;
  int ki;            // key index
  uint32_t endMs;    // millis() at which the key is switched off
};
static ApiTimer timers[MAX_TIMERS];
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static bool timer_set(int ki, uint32_t ms) {
  bool ok = false;
  uint32_t end = millis() + ms;
  portENTER_CRITICAL(&timerMux);
  int slot = -1;
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].used && timers[i].ki == ki) { slot = i; break; }   // same key: replace (restart)
    if (!timers[i].used && slot < 0) slot = i;
  }
  if (slot >= 0) { timers[slot] = { true, ki, end }; ok = true; }
  portEXIT_CRITICAL(&timerMux);
  return ok;
}

static void timer_cancel(int ki) {
  portENTER_CRITICAL(&timerMux);
  for (int i = 0; i < MAX_TIMERS; i++) if (timers[i].used && timers[i].ki == ki) timers[i].used = false;
  portEXIT_CRITICAL(&timerMux);
}

static void timers_clear() {
  portENTER_CRITICAL(&timerMux);
  for (int i = 0; i < MAX_TIMERS; i++) timers[i].used = false;
  portEXIT_CRITICAL(&timerMux);
}

// bit per key index: the writable boolean keys called "*.en"
static uint64_t enable_mask() {
  static uint64_t mask = 0;
  static bool init = false;
  if (!init) {
    for (int i = 0; i < protocol_key_count() && i < MAX_KEYS; i++) {
      KeyInfo k;
      if (!protocol_key_info(i, k) || !k.writable || !k.isBool) continue;
      size_t n = strlen(k.name);
      if (n > 3 && strcmp(k.name + n - 3, ".en") == 0) mask |= (uint64_t)1 << i;
    }
    init = true;
  }
  return mask;
}

void api_loop() {
  bool any = false;
  portENTER_CRITICAL(&timerMux);
  for (int i = 0; i < MAX_TIMERS; i++) if (timers[i].used) any = true;
  portEXIT_CRITICAL(&timerMux);
  if (!any) return;

  int32_t vals[MAX_KEYS];
  if (!protocol_snapshot(vals, nullptr)) return;
  uint32_t now = millis();
  for (int i = 0; i < MAX_TIMERS; i++) {
    int expired = -1;
    portENTER_CRITICAL(&timerMux);
    if (timers[i].used && (int32_t)(now - timers[i].endMs) >= 0) {
      timers[i].used = false;
      expired = timers[i].ki;
    }
    portEXIT_CRITICAL(&timerMux);
    if (expired < 0 || vals[expired] == 0) continue; // already off (all_off, web, menu): nothing to do
    KeyInfo k;
    if (!protocol_key_info(expired, k)) continue;
    SetResult r;
    protocol_apply_text(k.name, "0", r);  // loop() context: applied at once
    Serial.printf("[api] timer expired: %s off\n", k.name);
  }
}

// ---------------------------------------------------------------------------
// Request bodies (POST): collected in _tempObject (freed by the library), max. BODY_MAX bytes
// ---------------------------------------------------------------------------
static void on_body(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (total > BODY_MAX) return; // answered with 413 by the request handler
  if (index == 0 && req->_tempObject == nullptr) req->_tempObject = malloc(total + 1);
  if (req->_tempObject == nullptr) return;
  memcpy((uint8_t*)req->_tempObject + index, data, len);
  if (index + len >= total) ((char*)req->_tempObject)[total] = 0;
}

// The POST body as text: collected by on_body(), or, when the client sent it with the content type
// application/x-www-form-urlencoded (curl -d '{...}' without -H), the library keeps a body that starts with { or [
// as the POST parameter "body".
static const char* body_text(AsyncWebServerRequest* req) {
  if (req->_tempObject) return (const char*)req->_tempObject;
  AsyncWebParameter* p = req->getParam("body", true);
  return p ? p->value().c_str() : nullptr;
}

// false: an error reply has been sent
static bool check_body(AsyncWebServerRequest* req) {
  if (req->method() != HTTP_POST) return true;
  if (req->hasHeader("Transfer-Encoding")) {
    reply_err(req, 411, "length_required", "chunked bodies are not supported, send a Content-Length");
    return false;
  }
  size_t cl = req->contentLength();
  if (cl > BODY_MAX) {
    reply_err(req, 413, "too_large", "body too long (512 bytes max)");
    return false;
  }
  if (cl > 0 && body_text(req) == nullptr && req->params() == 0) {
    reply_err(req, 503, "no_memory", "could not buffer the body, try again");
    return false;
  }
  return true;
}

// One numeric parameter: query string, form field or (with a JSON body) a top-level member.
// present: the parameter exists; numeric: it is a number (not necessarily an integer in range).
static void get_number(AsyncWebServerRequest* req, JSONVar* root, const char* name, bool& present, bool& numeric, double& num) {
  present = false;
  numeric = false;
  num = 0;
  AsyncWebParameter* p = req->getParam(name);
  if (!p) p = req->getParam(name, true); // form-typed POST body: for=5
  if (p) {
    present = true;
    const char* text = p->value().c_str();
    char* endp = nullptr;
    long n = strtol(text, &endp, 10);
    if (text[0] != 0 && *endp == 0) { num = (double)n; numeric = true; }
    return;
  }
  if (root && root->hasOwnProperty(name)) {
    present = true;
    JSONVar f = (*root)[name];
    if (JSONVar::typeof_(f) == "number") { num = (double)f; numeric = true; }
  }
}

static void reply_for_error(AsyncWebServerRequest* req, const char* k, const char* code, uint32_t lo, uint32_t hi) {
  String b = String("{\"ok\":false,\"applied\":0,\"errors\":[{\"k\":\"") + k + "\",\"code\":\"" + code + "\"";
  if (strcmp(code, "range") == 0) b += String(",\"min\":") + (unsigned)lo + ",\"max\":" + (unsigned)hi;
  b += "}]}";
  reply(req, 400, b);
}

// Parses for=<s> / for_ms=<ms>. On success have tells whether one was given and ms is the duration in
// milliseconds; on a bad value a 400 has been sent and false is returned (nothing is applied then).
// kind: 'S' for for=, 'M' for for_ms=
static bool parse_for(AsyncWebServerRequest* req, JSONVar* root, bool& have, uint32_t& ms, char& kind) {
  have = false;
  ms = 0;
  kind = 0;
  bool presS, numS, presM, numM;
  double s, m;
  get_number(req, root, "for", presS, numS, s);
  get_number(req, root, "for_ms", presM, numM, m);
  if (presS && presM) {
    reply_for_error(req, "for", "conflict", 0, 0); // for and for_ms in one call
    return false;
  }
  if (!presS && !presM) return true;
  if (presS) {
    if (!numS || s != (double)(long)s) { reply_for_error(req, "for", "type", 0, 0); return false; }
    if (s < 1 || s > FOR_MAX_S) { reply_for_error(req, "for", "range", 1, FOR_MAX_S); return false; }
    ms = (uint32_t)s * 1000UL;
    kind = 'S';
  }
  else {
    if (!numM || m != (double)(long)m) { reply_for_error(req, "for_ms", "type", 0, 0); return false; }
    if (m < FOR_MS_MIN || m > FOR_MS_MAX) { reply_for_error(req, "for_ms", "range", FOR_MS_MIN, FOR_MS_MAX); return false; }
    ms = (uint32_t)m;
    kind = 'M';
  }
  have = true;
  return true;
}

// ---------------------------------------------------------------------------
// set / toggle answer
// ---------------------------------------------------------------------------
// Timers for the switched-on *.en keys (with for=), cancel timers of keys that were set explicitly,
// then send the answer: 200 all applied, 503 queue full, 400 otherwise (applied + errors in the body).
static void finish_set(AsyncWebServerRequest* req, SetResult& r, bool haveFor, uint32_t forMs, char forKind, const String& extra) {
  uint64_t en = enable_mask();
  String timerList, ignored;
  for (int i = 0; i < protocol_key_count() && i < MAX_KEYS; i++) {
    uint64_t bit = (uint64_t)1 << i;
    KeyInfo k;
    if (!protocol_key_info(i, k)) continue;
    if ((r.offMask & bit) && (en & bit)) timer_cancel(i);
    if ((r.onMask & bit) && (en & bit)) {
      if (!haveFor) { timer_cancel(i); continue; } // set explicitly without for=: no timer any more
      if (timer_set(i, forMs)) {
        if (timerList.length() > 0) timerList += ',';
        if (forKind == 'M') timerList += String("{\"k\":\"") + k.name + "\",\"for_ms\":" + (unsigned)forMs + "}";
        else timerList += String("{\"k\":\"") + k.name + "\",\"for_s\":" + (unsigned)(forMs / 1000) + "}";
      }
      else { // no free timer: do not leave the key on without an end
        SetResult rb;
        protocol_apply_text(k.name, "0", rb);
        protocol_error(r, k.name, "timer_full");
        r.applied--;
      }
    }
    if (haveFor && (r.appliedMask & bit) && !(en & bit)) {
      if (ignored.length() > 0) ignored += ',';
      ignored += String("\"") + k.name + "\"";
    }
  }
  String b = r.errors == 0 ? "{\"ok\":true" : "{\"ok\":false";
  b += extra;
  b += ",\"applied\":";
  b += r.applied;
  if (r.errors > 0) { b += ",\"errors\":["; b += r.errList; b += ']'; }
  if (r.held.length() > 0) { b += ",\"held\":["; b += r.held; b += ']'; }
  if (r.restart) { b += ",\"restart_in_s\":"; b += RESTART_IN_S; }
  if (timerList.length() > 0) { b += ",\"timers\":["; b += timerList; b += ']'; }
  if (ignored.length() > 0) { b += ",\"for_ignored\":["; b += ignored; b += ']'; }
  b += '}';
  reply(req, r.errors == 0 ? 200 : (r.queueFull ? 503 : 400), b);
}

// ---------------------------------------------------------------------------
// GET/POST /api/set
// ---------------------------------------------------------------------------
static void handle_set(AsyncWebServerRequest* req) {
  if (!check_body(req)) return;
  const char* body = body_text(req);
  JSONVar root;
  bool haveRoot = false;
  if (body && req->contentLength() > 0) {
    root = JSONVar::parse(body);
    if (JSONVar::typeof_(root) != "object") {
      reply_err(req, 400, "parse", "body is not a JSON object");
      return;
    }
    haveRoot = true;
  }
  bool haveFor;
  uint32_t forMs;
  char forKind;
  if (!parse_for(req, haveRoot ? &root : nullptr, haveFor, forMs, forKind)) return;

  SetResult r;
  if (haveRoot) {
    if (root.hasOwnProperty("d") && JSONVar::typeof_(root["d"]) == "object") { // WebSocket style {"t":"set","d":{...}}
      JSONVar d = root["d"];
      protocol_apply_object(d, r);
    }
    else protocol_apply_object(root, r, "for", "for_ms");
  }
  for (int i = 0; i < req->params(); i++) { // query string keys, also form fields of a POST (ch1.en=1&for=5)
    AsyncWebParameter* p = req->getParam(i);
    if (p->isFile() || p->name() == "for" || p->name() == "for_ms" || (p->isPost() && p->name() == "body" && haveRoot)) continue;
    protocol_apply_text(p->name().c_str(), p->value().c_str(), r);
  }
  if (r.applied + r.errors == 0) {
    reply_err(req, 400, "empty", "no keys: send a JSON object or query parameters like ?ch1.en=1");
    return;
  }
  finish_set(req, r, haveFor, forMs, forKind, "");
}

// ---------------------------------------------------------------------------
// GET /api/toggle?k=ch1.en[&for=<s>]
// ---------------------------------------------------------------------------
static void handle_toggle(AsyncWebServerRequest* req) {
  AsyncWebParameter* kp = req->getParam("k");
  if (!kp) {
    reply_err(req, 400, "missing_k", "toggle needs ?k=<boolean key>");
    return;
  }
  String k = kp->value();
  bool haveFor;
  uint32_t forMs;
  char forKind;
  if (!parse_for(req, nullptr, haveFor, forMs, forKind)) return;

  SetResult r;
  String extra;
  int ki = protocol_key_index(k.c_str());
  KeyInfo info;
  if (ki < 0) protocol_error(r, k.c_str(), "unknown");
  else {
    protocol_key_info(ki, info);
    if (!info.writable) protocol_error(r, k.c_str(), "readonly");
    else if (!info.isBool) protocol_error(r, k.c_str(), "type");
    else {
      int32_t vals[MAX_KEYS];
      if (!protocol_snapshot(vals, nullptr)) {
        reply_err(req, 503, "not_ready", "state is not ready yet, try again");
        return;
      }
      bool now = vals[ki] != 0; // the snapshot is at most one loop() pass old
      protocol_apply_text(k.c_str(), now ? "0" : "1", r);
      if (r.errors == 0) extra = String(",\"k\":\"") + info.name + "\",\"value\":" + (now ? "false" : "true");
    }
  }
  finish_set(req, r, haveFor, forMs, forKind, extra);
}

// ---------------------------------------------------------------------------
// GET/POST /api/cmd
// ---------------------------------------------------------------------------
static bool is_collar_cmd(const char* c) {
  return strcmp(c, "collar.beep") == 0 || strcmp(c, "collar.vibe") == 0 || strcmp(c, "collar.shock") == 0;
}

static void handle_cmd(AsyncWebServerRequest* req) {
  if (!check_body(req)) return;
  String cmd;
  bool have = false;
  const char* body = body_text(req);
  if (body && req->contentLength() > 0) {
    JSONVar root = JSONVar::parse(body);
    if (JSONVar::typeof_(root) != "object") {
      reply_err(req, 400, "parse", "body is not a JSON object");
      return;
    }
    if (root.hasOwnProperty("c") && JSONVar::typeof_(root["c"]) == "string") { cmd = (const char*)root["c"]; have = true; }
  }
  AsyncWebParameter* p = req->getParam("c");
  if (!p) p = req->getParam("c", true);
  if (p) { cmd = p->value(); have = true; }
  if (!have) {
    reply_err(req, 400, "missing_c", "send the command as ?c=<name> or as the c member of a JSON body");
    return;
  }

  static uint32_t lastCollarMs = 0;   // only the async_tcp task uses these two
  static bool haveCollar = false;
  uint32_t now = millis();
  bool collar = is_collar_cmd(cmd.c_str());
  if (collar && haveCollar && now - lastCollarMs < COLLAR_GAP_MS) {
    reply(req, 429, String("{\"ok\":false,\"code\":\"rate_limited\",\"retry_ms\":") + (int)(COLLAR_GAP_MS - (now - lastCollarMs)) + "}");
    return;
  }
  const char* err = protocol_run_cmd(cmd.c_str());
  if (err) {
    if (strcmp(err, "disabled") == 0) reply_err(req, 409, "disabled", "collar is not enabled (set collar.en first)");
    else reply_err(req, 400, err, "unknown command (see /api/keys)");
    return;
  }
  if (collar) { lastCollarMs = now; haveCollar = true; }
  if (cmd == "all_off") timers_clear();
  reply(req, 200, String("{\"ok\":true,\"c\":\"") + cmd + "\"}"); // cmd is one of the known names here
}

// ---------------------------------------------------------------------------
// GET /api/state
// ---------------------------------------------------------------------------
static void handle_state(AsyncWebServerRequest* req) {
  int32_t vals[MAX_KEYS];
  uint32_t n = 0;
  if (!protocol_snapshot(vals, &n)) {
    reply_err(req, 503, "not_ready", "state is not ready yet, try again");
    return;
  }
  String s;
  s.reserve(1500);
  s += "{\"ok\":true,\"n\":";
  s += n;
  s += ",\"d\":{";
  for (int i = 0; i < protocol_key_count() && i < MAX_KEYS; i++) {
    KeyInfo k;
    protocol_key_info(i, k);
    if (i > 0) s += ',';
    s += '"';
    s += k.name;
    s += "\":";
    if (k.isBool) s += vals[i] ? "true" : "false";
    else s += vals[i];
  }
  s += "},\"timers\":[";
  uint32_t now = millis();
  bool first = true;
  portENTER_CRITICAL(&timerMux);
  ApiTimer copy[MAX_TIMERS];
  memcpy(copy, timers, sizeof(copy));
  portEXIT_CRITICAL(&timerMux);
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (!copy[i].used) continue;
    KeyInfo k;
    if (!protocol_key_info(copy[i].ki, k)) continue;
    int32_t left = (int32_t)(copy[i].endMs - now);
    if (left < 0) left = 0;
    if (!first) s += ',';
    first = false;
    s += String("{\"k\":\"") + k.name + "\",\"left_s\":" + (int)((left + 999) / 1000) + ",\"left_ms\":" + (int)left + "}";
  }
  s += "]}";
  reply(req, 200, s);
}

// ---------------------------------------------------------------------------
// GET /api/keys: self description (streamed, about 6 KB)
// ---------------------------------------------------------------------------
static void handle_keys(AsyncWebServerRequest* req) {
  AsyncResponseStream* r = req->beginResponseStream("application/json");
  add_headers(r);
  r->print("{\"api\":1,\"note\":\"Booleans are true/false (0/1 in query strings), everything else integers. "
           "Writable keys can be set with /api/set. A set is always accepted for a key that Bluetooth currently holds, "
           "it is stored and the answer lists it under held.\",\"keys\":[");
  for (int i = 0; i < protocol_key_count(); i++) {
    KeyInfo k;
    protocol_key_info(i, k);
    String e = i > 0 ? ",{" : "{";
    e += String("\"k\":\"") + k.name + "\",\"type\":\"" + (k.isBool ? "bool" : "int") + "\",\"min\":" + (int)k.lo +
         ",\"max\":" + (int)k.hi + ",\"rw\":\"" + (k.writable ? "rw" : "r") + "\"";
    if (k.unit[0]) e += String(",\"unit\":\"") + k.unit + "\"";
    e += String(",\"desc\":\"") + k.desc + "\"";
    if (k.heldBy) e += String(",\"held_by\":\"") + k.heldBy + "\"";
    if (k.restart) e += ",\"restarts_device\":true";
    e += '}';
    r->print(e);
  }
  r->print("],\"cmds\":["
           "{\"c\":\"collar.beep\",\"desc\":\"collar beep (needs collar.en = true; at most one collar command per 300 ms)\"},"
           "{\"c\":\"collar.vibe\",\"desc\":\"collar vibration with collar.strength\"},"
           "{\"c\":\"collar.shock\",\"desc\":\"collar shock with collar.strength\"},"
           "{\"c\":\"all_off\",\"desc\":\"switch every output off and cancel all for= timers\"}],"
           "\"params\":{\"for\":\"1-3600 seconds, with set and toggle: the *.en keys switched on by the call are set to 0 again "
           "after that time (max. 8 timers, a new call for the same key restarts its timer, an explicit set of the key cancels it)\","
           "\"for_ms\":\"same as for, in milliseconds (100-3600000); for and for_ms in one call is an error (conflict)\"},"
           "\"routes\":[\"GET /api/state\",\"GET /api/keys\",\"GET|POST /api/set\",\"GET|POST /api/cmd\",\"GET /api/toggle?k=<bool key>\"]}");
  req->send(r);
}

// ---------------------------------------------------------------------------
void api_setup(AsyncWebServer& server) {
  server.on("/api/state", HTTP_GET, handle_state);
  server.on("/api/keys", HTTP_GET, handle_keys);
  server.on("/api/set", HTTP_GET | HTTP_POST, handle_set, nullptr, on_body);
  server.on("/api/cmd", HTTP_GET | HTTP_POST, handle_cmd, nullptr, on_body);
  server.on("/api/toggle", HTTP_GET, handle_toggle);
  static const char* const routes[] = { "/api/state", "/api/keys", "/api/set", "/api/cmd", "/api/toggle" };
  for (const char* route : routes) server.on(route, HTTP_OPTIONS, handle_options);
}
