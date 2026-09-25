#ifndef PROTOCOL_H
#define PROTOCOL_H

// JSON protocol of the WebSocket (v2). The old "id?value" messages stay valid.
//
// Client -> server (all with an optional "id" that comes back in ack / err):
//   {"t":"get"}                         full state (answer: "state"); the first get marks the client as v2
//   {"t":"get","k":["ch1.pwm"]}         only these keys
//   {"t":"set","id":7,"d":{"ch1.pwm":50,"ble.map0.min":10}}   one or more keys
//   {"t":"cmd","id":8,"c":"collar.beep"}   c: collar.beep | collar.vibe | collar.shock | all_off
// Server -> client:
//   {"t":"ack","id":7}                          all keys accepted (queued, the state follows as patch)
//   {"t":"err","id":7,"applied":1,"errors":[{"k":"buzzer.bpm","code":"range","min":1,"max":255}]}
//   {"t":"err","id":7,"code":"parse","msg":"..."}    message level error
//   {"t":"state","n":41,"d":{"ch1.en":false,...}}    sent from loop() after a get
//   {"t":"patch","n":42,"d":{"ch1.pwm":50}}           changed keys, n counts up by one per patch
//                                                    (a gap in n: send get again; patches before the first state are ignored)
// Old clients (no get) keep receiving the flat legacy JSON.
//
// Keys are defined in one table (protocol.cpp): range checks there mirror the state limits.
// Everything the server sends about the state is built in loop() (single writer / reader).

#include <stdint.h>
#include <stddef.h>
#include <WString.h>

class JSONVar;
class AsyncWebSocket;
class AsyncWebSocketClient;

void protocol_client_connected(uint32_t id);   // WS_EVT_CONNECT (async_tcp task)
void protocol_client_gone(uint32_t id);        // WS_EVT_DISCONNECT (async_tcp task)
void protocol_handle(AsyncWebSocketClient* client, const char* msg, size_t len); // async_tcp task
bool protocol_legacy_set(const char* key, long value); // old "id?value" messages, same checks; false: rejected
void protocol_loop(AsyncWebSocket& ws);        // loop(), after outputs_arbitrate(): patches and answers to "get"
int  protocol_v1_count();                      // clients that use the old flat JSON
void protocol_send_legacy(AsyncWebSocket& ws, const String& json); // flat JSON to the v1 clients only


// Shared by the WebSocket and the HTTP API: the same key table, checks and event queue.
struct SetResult {
  int applied = 0;       // keys that were queued
  int errors = 0;
  String errList;        // JSON list entries {"k":..,"code":..}
  String held;           // JSON list of names: stored, but a BLE hold overrides the output right now
  bool restart = false;  // the change restarts the device (ble.toy)
  bool queueFull = false;// at least one event did not fit into the queue
  uint64_t appliedMask = 0; // bit per key index: keys that were queued
  uint64_t onMask = 0;      // ... of those the boolean keys set to true
  uint64_t offMask = 0;     // ... and set to false
};
struct KeyInfo {
  const char* name;
  const char* unit;
  const char* desc;
  bool isBool;
  bool writable;
  int32_t lo, hi;
  bool restart;            // a changed value restarts the device
  const char* heldBy;      // name of the ble.hold.* key that overrides this key's output, or nullptr
};
bool protocol_key_info(int i, KeyInfo& out);
int  protocol_key_index(const char* name);      // -1 if unknown
void protocol_error(SetResult& r, const char* key, const char* code); // adds an error entry (with range for "range")
void protocol_apply_object(JSONVar& d, SetResult& r, const char* skip = nullptr); // key -> boolean / number
void protocol_apply_text(const char* key, const char* val, SetResult& r); // one key from a query string
const char* protocol_run_cmd(const char* cmd);  // nullptr = done, else "disabled" / "unknown_cmd"
int  protocol_key_count();
bool protocol_snapshot(int32_t* vals, uint32_t* n); // copy prepared in loop(); false before the first pass

#endif
