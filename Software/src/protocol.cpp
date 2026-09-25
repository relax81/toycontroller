#include <Arduino.h>
#include <Arduino_JSON.h>
#include "ESPAsyncWebServer.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
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
  RW_I("ch" #n ".on",  EV_OUT_ON,  i, 0, 900, &state.out[i].on), \
  RW_I("ch" #n ".off", EV_OUT_OFF, i, 0, 900, &state.out[i].off), \
  RW_I("ch" #n ".pwm", EV_OUT_PWM, i, 0, 100, &state.out[i].pwm)

#define BLEMAP(k) \
  RW_I("ble.map" #k ".out", EV_BT_OUT, k, 0, OUT_ID_COUNT - 1, &state.ble.map[k].output), \
  RW_I("ble.map" #k ".min", EV_BT_MIN, k, 0, 255, &state.ble.map[k].minPwm), \
  RW_I("ble.map" #k ".max", EV_BT_MAX, k, 0, 255, &state.ble.map[k].maxPwm)

static const KeyDef KEYS[] = {
  CHANNEL(1, 0), CHANNEL(2, 1), CHANNEL(3, 2), CHANNEL(4, 3),
  RW_B("pump.en",  EV_PUMP_ENABLE, 0, &state.pump.enabled),
  RW_I("pump.on",  EV_PUMP_ON,  0, 0, 900, &state.pump.on),
  RW_I("pump.off", EV_PUMP_OFF, 0, 0, 900, &state.pump.off),
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
  bool hasState;      // has been sent a state (from then on it gets patches)
  uint32_t syncedN;   // "n" of the last state / patch that was queued to this client
  uint32_t fullSince; // millis() since when its send queue is full, 0 = not full (loop() only)
};
static const int MAX_SLOTS = 8;
static const uint32_t STALL_MS = 2000; // send queue full this long -> the client is closed
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
      if (!slots[i].used) { slots[i] = { true, false, id, 0, false, 0, 0 }; ok = true; break; }
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
// Replies (async_tcp task). Built as strings: copying JSONVar children between objects
// (r["id"] = m["id"], errs[n] = ...) gave null values with Arduino_JSON.
// ---------------------------------------------------------------------------
static String json_escape(const char* s) {
  String o;
  for (; *s; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch == '"' || ch == 0x5C) { o += (char)0x5C; o += (char)ch; }
    else if (ch < 0x20) o += ' ';
    else o += (char)ch;
  }
  return o;
}

// ,"id":<number or string> of the request, "" if it has none
static String id_field(JSONVar& m) {
  if (!m.hasOwnProperty("id")) return "";
  JSONVar id = m["id"];
  String ty = JSONVar::typeof_(id);
  if (ty == "number") { return String(",\"id\":") + String((long)(double)id); }
  if (ty == "string") { return String(",\"id\":\"") + json_escape((const char*)id) + "\""; }
  return "";
}

static void send_str(AsyncWebSocketClient* c, const String& s) {
  if (c && c->status() == WS_CONNECTED) c->text(s);
}

static void send_ack(AsyncWebSocketClient* c, JSONVar& m) {
  send_str(c, String("{\"t\":\"ack\"") + id_field(m) + "}");
}

static void send_err(AsyncWebSocketClient* c, JSONVar& m, const char* code, const char* msg) {
  send_str(c, String("{\"t\":\"err\"") + id_field(m) + ",\"code\":\"" + code + "\",\"msg\":\"" + json_escape(msg) + "\"}");
}

// one entry of the "errors" list
static void add_key_error(String& list, const char* k, const char* code, const KeyDef* def) {
  if (list.length() > 0) list += ',';
  list += "{\"k\":\"";
  list += json_escape(k);
  list += "\",\"code\":\"";
  list += code;
  list += '"';
  if (def && strcmp(code, "range") == 0) {
    list += ",\"min\":";
    list += (int)def->lo;
    list += ",\"max\":";
    list += (int)def->hi;
  }
  list += '}';
}

static void send_key_errors(AsyncWebSocketClient* c, JSONVar& m, int applied, const String& list) {
  send_str(c, String("{\"t\":\"err\"") + id_field(m) + ",\"applied\":" + applied + ",\"errors\":[" + list + "]}");
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
  String errs;
  int nerr = 0, applied = 0;
  for (int i = 0; i < keys.length(); i++) {
    String k = (const char*)keys[i];
    JSONVar v = d[k];
    int ki = key_find(k.c_str());
    if (ki < 0) { add_key_error(errs, k.c_str(), "unknown", nullptr); nerr++; continue; }
    const KeyDef& def = KEYS[ki];
    if (def.ev == EV_NONE) { add_key_error(errs, k.c_str(), "readonly", &def); nerr++; continue; }
    String ty = JSONVar::typeof_(v);
    double num;
    if (def.kind == K_BOOL && ty == "boolean") num = ((bool)v) ? 1 : 0;
    else if (ty == "number") num = (double)v;
    else { add_key_error(errs, k.c_str(), "type", &def); nerr++; continue; }
    const char* err = set_number(def, num);
    if (err) { add_key_error(errs, k.c_str(), err, &def); nerr++; continue; }
    applied++;
  }
  if (nerr == 0) send_ack(c, m);
  else send_key_errors(c, m, applied, errs);
}

static void handle_get(AsyncWebSocketClient* c, JSONVar& m) {
  uint64_t mask = 0;
  String errs;
  int nerr = 0;
  if (m.hasOwnProperty("k") && JSONVar::typeof_(m["k"]) == "array") {
    JSONVar kl = m["k"];
    for (int i = 0; i < kl.length(); i++) {
      if (JSONVar::typeof_(kl[i]) != "string") { add_key_error(errs, "?", "type", nullptr); nerr++; continue; }
      String k = (const char*)kl[i];
      int ki = key_find(k.c_str());
      if (ki < 0) { add_key_error(errs, k.c_str(), "unknown", nullptr); nerr++; }
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
  if (nerr > 0) send_key_errors(c, m, 0, errs);
}

// Commands. The collar sends run here in the async_tcp task, exactly like the old click_* messages
// (the blocking sender is not moved into loop()).
static void handle_cmd(AsyncWebSocketClient* c, JSONVar& m) {
  if (!m.hasOwnProperty("c") || JSONVar::typeof_(m["c"]) != "string") {
    send_err(c, m, "type", "cmd needs a \"c\" string");
    return;
  }
  String cmd = (const char*)m["c"];
  if (cmd == "all_off") {
    state_set(EV_ALL_OFF, 0, 0);
  }
  else if (cmd == "collar.beep" || cmd == "collar.vibe" || cmd == "collar.shock") {
    if (!state.collar.enabled) {
      send_err(c, m, "disabled", "collar is not enabled");
      return;
    }
    CollarMode mode = (cmd == "collar.beep") ? CollarMode::Beep : (cmd == "collar.vibe") ? CollarMode::Vibe : CollarMode::Shock;
    collar_send(mode, state.collar.strength);
    debugln(cmd);
  }
  else {
    send_err(c, m, "unknown_cmd", "unknown command");
    return;
  }
  send_ack(c, m);
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
  else if (t == "cmd") handle_cmd(c, m);
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

// Call once per loop() pass after outputs_arbitrate() (the ble.hold.* keys are read-only state).
// Every client has its own sync state (syncedN = "n" of the last message queued to it), so what a
// client gets does not depend on who caused a change:
//  - explicit "get"                                  -> "state" (with the current n)
//  - has a state, exactly one n behind, keys changed -> "patch" (new sequence number n)
//  - further behind (a message could not be queued)  -> full "state"
// A message is only queued if the client's queue has room (the library drops silently when it is
// full). Otherwise the client stays behind and is served again on the next pass, so a lost patch,
// also the last one, is always made up for. A client whose queue stays full for STALL_MS is closed.
// Everything is sent from this one place, so patch and state cannot overtake each other.
void protocol_loop(AsyncWebSocket& ws) {
  // changes since the last pass
  uint64_t changed = 0;
  for (int i = 0; i < NKEYS; i++) {
    int32_t v = key_value(KEYS[i]);
    if (!haveSnapshot || v != lastSent[i]) {
      if (haveSnapshot) changed |= (uint64_t)1 << i;
      lastSent[i] = v;
    }
  }
  haveSnapshot = true;
  if (changed != 0) stateSeq++;

  // the v2 clients (copied, the sends happen without the lock)
  uint32_t ids[MAX_SLOTS];
  uint64_t masks[MAX_SLOTS];
  bool hasState[MAX_SLOTS];
  uint32_t synced[MAX_SLOTS];
  uint32_t fullSince[MAX_SLOTS];
  int n = 0;
  portENTER_CRITICAL(&slotMux);
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (!slots[i].used || !slots[i].v2) continue;
    ids[n] = slots[i].id;
    masks[n] = slots[i].getMask;
    hasState[n] = slots[i].hasState;
    synced[n] = slots[i].syncedN;
    fullSince[n] = slots[i].fullSince;
    n++;
  }
  portEXIT_CRITICAL(&slotMux);

  String patch;
  bool patchBuilt = false;
  uint32_t now = millis();
  for (int i = 0; i < n; i++) {
    AsyncWebSocketClient* c = ws.client(ids[i]);
    if (c == nullptr || c->status() != WS_CONNECTED) continue;

    // A queue that stays full is a stalled client (does not read, dead link): the library drops
    // silently from then on and never recovers it. Close the connection; the page reconnects and asks
    // for "get". Only "full" can be measured (the queue length is not public).
    bool isFull = c->queueIsFull();
    uint32_t since = isFull ? (fullSince[i] != 0 ? fullSince[i] : (now != 0 ? now : 1)) : 0;
    if (since != 0 && now - since >= STALL_MS) {
      Serial.printf("[ws] client #%u stalled (send queue full for %u ms), closing\n", (unsigned)ids[i], (unsigned)(now - since));
      c->client()->close(true);
      continue;
    }
    if (since != fullSince[i]) {
      portENTER_CRITICAL(&slotMux);
      Slot* s = slot_find(ids[i]);
      if (s) s->fullSince = since;
      portEXIT_CRITICAL(&slotMux);
    }

    uint32_t behind = stateSeq - synced[i];
    bool sendState = masks[i] != 0 || !hasState[i] || behind > 1 || (behind == 1 && changed == 0);
    if (!sendState && behind == 0) continue; // up to date
    if (isFull) continue; // stays behind, next pass
    if (sendState) {
      c->text(build_state(masks[i] != 0 ? masks[i] : ALL_KEYS));
    }
    else {
      if (!patchBuilt) { patch = build_patch(changed); patchBuilt = true; }
      c->text(patch);
    }
    portENTER_CRITICAL(&slotMux);
    Slot* s = slot_find(ids[i]);
    if (s) {
      s->hasState = true;
      s->syncedN = stateSeq;
      s->getMask &= ~masks[i]; // only what was answered, a newer "get" stays pending
    }
    portEXIT_CRITICAL(&slotMux);
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
