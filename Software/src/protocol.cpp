#include <Arduino.h>
#include <Arduino_JSON.h>
#include "ESPAsyncWebServer.h"
#include "config.h"
#include "state.h"
#include "protocol.h"

// ---------------------------------------------------------------------------
// Key table: name -> event, range, pointer to the state field (for reading)
// ---------------------------------------------------------------------------
static const uint8_t EV_NONE = 0xFF; // read-only key
enum KeyKind : uint8_t { K_BOOL, K_INT };

struct KeyDef {
  const char* name;
  uint8_t ev;       // EventType or EV_NONE
  uint8_t idx;
  int32_t lo, hi;
  KeyKind kind;
  const void* ptr;  // field in `state`
};

#define RW_B(name, ev, idx, p)         { name, ev, idx, 0, 1, K_BOOL, p }
#define RW_I(name, ev, idx, lo, hi, p) { name, ev, idx, lo, hi, K_INT, p }
#define RO_B(name, p)                  { name, EV_NONE, 0, 0, 1, K_BOOL, p }

#define CHANNEL(n, i) \
  RW_B("ch" #n ".en",  EV_OUT_ENABLE, i, &state.out[i].enabled), \
  RW_I("ch" #n ".on",  EV_OUT_ON,  i, 0, 100, &state.out[i].on), \
  RW_I("ch" #n ".off", EV_OUT_OFF, i, 0, 100, &state.out[i].off), \
  RW_I("ch" #n ".pwm", EV_OUT_PWM, i, 0, 100, &state.out[i].pwm)

#define BLEMAP(k) \
  RW_I("ble.map" #k ".out", EV_BT_OUT, k, 0, OUT_ID_COUNT - 1, &state.ble.map[k].output), \
  RW_I("ble.map" #k ".min", EV_BT_MIN, k, 0, 255, &state.ble.map[k].minPwm), \
  RW_I("ble.map" #k ".max", EV_BT_MAX, k, 0, 255, &state.ble.map[k].maxPwm)

static const KeyDef KEYS[] = {
  CHANNEL(1, 0), CHANNEL(2, 1), CHANNEL(3, 2), CHANNEL(4, 3),
  RW_B("pump.en",  EV_PUMP_ENABLE, 0, &state.pump.enabled),
  RW_I("pump.pwm", EV_PUMP_PWM, 0, 0, 100, &state.pump.pwm),
  RW_B("collar.en",       EV_COLLAR_ENABLE,   0, &state.collar.enabled),
  RW_I("collar.strength", EV_COLLAR_STRENGTH, 0, 0, 100, &state.collar.strength),
  RW_B("collar.btonly",   EV_COLLAR_BTONLY,   0, &state.collar.btOnlyChanges),
  RW_B("buzzer.en",  EV_BUZZ_ENABLE, 0, &state.buzzer.enabled),
  RW_I("buzzer.bpm", EV_BUZZ_BPM, 0, 1, 255, &state.buzzer.bpm),
  RW_I("buzzer.vol", EV_BUZZ_VOL, 0, 0, 10, &state.buzzer.volume),
  BLEMAP(0), BLEMAP(1),
  RW_I("sys.failsafe", EV_FAILSAFE_TO, 0, 3, 120, &state.failsafeTimeoutS),
  RO_B("ble.connected",   &state.ble.in.connected),
  RO_B("ble.hold.ch1",    &state.ble.hold[OUT_PWM1]),
  RO_B("ble.hold.ch2",    &state.ble.hold[OUT_PWM2]),
  RO_B("ble.hold.ch3",    &state.ble.hold[OUT_PWM3]),
  RO_B("ble.hold.ch4",    &state.ble.hold[OUT_PWM4]),
  RO_B("ble.hold.pump",   &state.ble.hold[OUT_PUMP]),
  RO_B("ble.hold.collar", &state.ble.hold[OUT_COLLAR]),
};
static const int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);
static_assert(sizeof(KEYS) / sizeof(KEYS[0]) <= 64, "key masks are 64 bit");

static const uint64_t ALL_KEYS_MASK = (sizeof(KEYS) / sizeof(KEYS[0]) >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << (sizeof(KEYS) / sizeof(KEYS[0]))) - 1);

static int key_find(const char* name) {
  for (int i = 0; i < NKEYS; i++) if (strcmp(KEYS[i].name, name) == 0) return i;
  return -1;
}

// current value of a key; only call from loop() (or the loop task)
static int32_t key_value(const KeyDef& k) {
  return k.kind == K_BOOL ? (int32_t)(*(const bool*)k.ptr ? 1 : 0) : (int32_t)*(const int*)k.ptr;
}

// ---------------------------------------------------------------------------
// Clients (slot per connected WebSocket client)
// ---------------------------------------------------------------------------
struct Slot {
  bool used;
  bool v2;            // has sent a "get"
  uint32_t id;
  uint64_t getMask;   // keys requested by a pending "get" (answered in loop())
};
static const int MAX_SLOTS = 8;
static Slot slots[MAX_SLOTS];
static portMUX_TYPE slotMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t stateSeq = 0; // "n" of state / patch messages (loop() only)

static Slot* slot_find(uint32_t id) { // call with the lock held
  for (int i = 0; i < MAX_SLOTS; i++) if (slots[i].used && slots[i].id == id) return &slots[i];
  return nullptr;
}

void protocol_client_connected(uint32_t id) {
  bool ok = false;
  portENTER_CRITICAL(&slotMux);
  if (slot_find(id) == nullptr) {
    for (int i = 0; i < MAX_SLOTS; i++) {
      if (!slots[i].used) { slots[i] = { true, false, id, 0 }; ok = true; break; }
    }
  }
  else ok = true;
  portEXIT_CRITICAL(&slotMux);
  if (!ok) Serial.printf("[ws] no free client slot for #%u\n", (unsigned)id);
}

void protocol_client_gone(uint32_t id) {
  portENTER_CRITICAL(&slotMux);
  Slot* s = slot_find(id);
  if (s) s->used = false;
  portEXIT_CRITICAL(&slotMux);
}

// ---------------------------------------------------------------------------
// Replies (async_tcp task, own JSON objects only)
// ---------------------------------------------------------------------------
static void send_json(AsyncWebSocketClient* c, JSONVar& r) {
  if (c && c->status() == WS_CONNECTED) c->text(JSON.stringify(r));
}

static void reply_head(JSONVar& r, const char* t, JSONVar& m) {
  r["t"] = t;
  if (m.hasOwnProperty("id")) r["id"] = m["id"];
}

static void send_err(AsyncWebSocketClient* c, JSONVar& m, const char* code, const char* msg) {
  JSONVar r;
  reply_head(r, "err", m);
  r["code"] = code;
  r["msg"] = msg;
  send_json(c, r);
}

static JSONVar key_error(const char* k, const char* code, const KeyDef* def) {
  JSONVar e;
  e["k"] = k;
  e["code"] = code;
  if (def && strcmp(code, "range") == 0) {
    e["min"] = (int)def->lo;
    e["max"] = (int)def->hi;
  }
  return e;
}

// Check a numeric value against the key definition and queue the event.
// Returns nullptr on success, otherwise the error code ("type" or "range").
static const char* set_number(const KeyDef& def, double num) {
  if (num != (double)(long)num) return "type"; // integers only
  if (def.kind == K_BOOL && num != 0 && num != 1) return "range";
  if (num < def.lo || num > def.hi) {
    Serial.printf("[ws] set %s=%ld rejected (range %ld-%ld)\n", def.name, (long)num, (long)def.lo, (long)def.hi);
    return "range";
  }
  state_set((EventType)def.ev, def.idx, (int32_t)num);
  return nullptr;
}

// Old "id?value" messages: same key table, same checks. A rejected value is dropped (log only).
bool protocol_legacy_set(const char* key, long value) {
  int ki = key_find(key);
  if (ki < 0 || KEYS[ki].ev == EV_NONE) return false;
  return set_number(KEYS[ki], (double)value) == nullptr;
}

static void handle_set(AsyncWebSocketClient* c, JSONVar& m) {
  if (!m.hasOwnProperty("d") || JSONVar::typeof_(m["d"]) != "object") {
    send_err(c, m, "type", "set needs a \"d\" object");
    return;
  }
  JSONVar d = m["d"];
  JSONVar keys = d.keys();
  JSONVar errs;
  int nerr = 0, applied = 0;
  for (int i = 0; i < keys.length(); i++) {
    String k = (const char*)keys[i];
    JSONVar v = d[k];
    int ki = key_find(k.c_str());
    if (ki < 0) { errs[nerr++] = key_error(k.c_str(), "unknown", nullptr); continue; }
    const KeyDef& def = KEYS[ki];
    if (def.ev == EV_NONE) { errs[nerr++] = key_error(k.c_str(), "readonly", &def); continue; }
    String ty = JSONVar::typeof_(v);
    double num;
    if (def.kind == K_BOOL && ty == "boolean") num = ((bool)v) ? 1 : 0;
    else if (ty == "number") num = (double)v;
    else { errs[nerr++] = key_error(k.c_str(), "type", &def); continue; }
    const char* err = set_number(def, num);
    if (err) { errs[nerr++] = key_error(k.c_str(), err, &def); continue; }
    applied++;
  }
  JSONVar r;
  if (nerr == 0) {
    reply_head(r, "ack", m);
  }
  else {
    reply_head(r, "err", m);
    r["applied"] = applied;
    r["errors"] = errs;
  }
  send_json(c, r);
}

static void handle_get(AsyncWebSocketClient* c, JSONVar& m) {
  uint64_t mask = 0;
  JSONVar errs;
  int nerr = 0;
  if (m.hasOwnProperty("k") && JSONVar::typeof_(m["k"]) == "array") {
    JSONVar kl = m["k"];
    for (int i = 0; i < kl.length(); i++) {
      if (JSONVar::typeof_(kl[i]) != "string") { errs[nerr++] = key_error("?", "type", nullptr); continue; }
      String k = (const char*)kl[i];
      int ki = key_find(k.c_str());
      if (ki < 0) errs[nerr++] = key_error(k.c_str(), "unknown", nullptr);
      else mask |= (uint64_t)1 << ki;
    }
  }
  else {
    mask = ALL_KEYS_MASK;
  }
  portENTER_CRITICAL(&slotMux);
  Slot* s = slot_find(c->id());
  if (s) { s->v2 = true; s->getMask |= mask; }
  portEXIT_CRITICAL(&slotMux);
  if (nerr > 0) {
    JSONVar r;
    reply_head(r, "err", m);
    r["applied"] = 0;
    r["errors"] = errs;
    send_json(c, r);
  }
}

void protocol_handle(AsyncWebSocketClient* c, const char* msg, size_t len) {
  JSONVar none; // "message" for errors before the message is parsed
  if (len > 512) {
    send_err(c, none, "parse", "message too long (512 bytes max)");
    return;
  }
  JSONVar m = JSONVar::parse(msg);
  if (JSONVar::typeof_(m) != "object") {
    send_err(c, none, "parse", "not a JSON object");
    return;
  }
  if (!m.hasOwnProperty("t") || JSONVar::typeof_(m["t"]) != "string") {
    send_err(c, m, "type", "missing \"t\"");
    return;
  }
  String t = (const char*)m["t"];
  if (t == "set") handle_set(c, m);
  else if (t == "get") handle_get(c, m);
  else send_err(c, m, "unknown_type", "unknown message type");
}

// ---------------------------------------------------------------------------
// loop(): state messages
// ---------------------------------------------------------------------------
static String build_state(uint64_t mask) {
  String s;
  s.reserve(900);
  s += "{\"t\":\"state\",\"n\":";
  s += stateSeq;
  s += ",\"d\":{";
  bool first = true;
  for (int i = 0; i < NKEYS; i++) {
    if (!(mask & ((uint64_t)1 << i))) continue;
    if (!first) s += ',';
    first = false;
    s += '"';
    s += KEYS[i].name;
    s += "\":";
    int32_t v = key_value(KEYS[i]);
    if (KEYS[i].kind == K_BOOL) s += v ? "true" : "false";
    else s += v;
  }
  s += "}}";
  return s;
}

static const uint64_t ALL_KEYS = ALL_KEYS_MASK;

// keys that changed since the last patch (loop() only)
static int32_t lastSent[NKEYS];
static bool haveSnapshot = false;

static String build_patch(uint64_t mask) {
  String s;
  s.reserve(200);
  s += "{\"t\":\"patch\",\"n\":";
  s += stateSeq;
  s += ",\"d\":{";
  bool first = true;
  for (int i = 0; i < NKEYS; i++) {
    if (!(mask & ((uint64_t)1 << i))) continue;
    if (!first) s += ',';
    first = false;
    s += '"';
    s += KEYS[i].name;
    s += "\":";
    int32_t v = key_value(KEYS[i]);
    if (KEYS[i].kind == K_BOOL) s += v ? "true" : "false";
    else s += v;
  }
  s += "}}";
  return s;
}

// Call once per loop() pass after outputs_arbitrate() (the ble.hold.* keys are read-only state):
//  1. keys changed since the last pass -> "patch" (new sequence number n) to all v2 clients
//     that have already received a "state"
//  2. pending "get" requests -> "state" (with the current n) to the requesting client
// Everything is sent from this one place, so patch and state cannot overtake each other.
void protocol_loop(AsyncWebSocket& ws) {
  // pending get requests
  uint32_t getId[MAX_SLOTS];
  uint64_t getMask[MAX_SLOTS];
  bool known[MAX_SLOTS];   // v2 client that already has a state (it gets patches)
  uint32_t ids[MAX_SLOTS];
  int nGet = 0, nKnown = 0;
  portENTER_CRITICAL(&slotMux);
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (!slots[i].used || !slots[i].v2) continue;
    if (slots[i].getMask != 0) {
      getId[nGet] = slots[i].id;
      getMask[nGet++] = slots[i].getMask;
      slots[i].getMask = 0;
    }
    else {
      ids[nKnown++] = slots[i].id;
    }
  }
  portEXIT_CRITICAL(&slotMux);
  (void)known;

  // changes
  uint64_t changed = 0;
  for (int i = 0; i < NKEYS; i++) {
    int32_t v = key_value(KEYS[i]);
    if (!haveSnapshot || v != lastSent[i]) {
      if (haveSnapshot) changed |= (uint64_t)1 << i;
      lastSent[i] = v;
    }
  }
  haveSnapshot = true;
  if (changed != 0) {
    stateSeq++;
    String p = build_patch(changed);
    for (int i = 0; i < nKnown; i++) {
      AsyncWebSocketClient* c = ws.client(ids[i]);
      if (c == nullptr || c->status() != WS_CONNECTED) continue;
      if (c->canSend()) {
        c->text(p);
      }
      else { // the client is behind: it gets the full state as soon as possible
        portENTER_CRITICAL(&slotMux);
        Slot* s = slot_find(ids[i]);
        if (s) s->getMask = ALL_KEYS;
        portEXIT_CRITICAL(&slotMux);
      }
    }
  }

  // state for the requesting clients (n = current sequence number)
  for (int i = 0; i < nGet; i++) {
    AsyncWebSocketClient* c = ws.client(getId[i]);
    if (c && c->status() == WS_CONNECTED) c->text(build_state(getMask[i]));
  }
}

// old (v1) clients get the flat legacy JSON, v2 clients get patches instead
int protocol_v1_count() {
  int n = 0;
  portENTER_CRITICAL(&slotMux);
  for (int i = 0; i < MAX_SLOTS; i++) if (slots[i].used && !slots[i].v2) n++;
  portEXIT_CRITICAL(&slotMux);
  return n;
}

void protocol_send_legacy(AsyncWebSocket& ws, const String& json) {
  uint32_t ids[MAX_SLOTS];
  int n = 0;
  portENTER_CRITICAL(&slotMux);
  for (int i = 0; i < MAX_SLOTS; i++) if (slots[i].used && !slots[i].v2) ids[n++] = slots[i].id;
  portEXIT_CRITICAL(&slotMux);
  for (int i = 0; i < n; i++) ws.text(ids[i], json);
}
