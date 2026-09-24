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

class AsyncWebSocket;
class AsyncWebSocketClient;

void protocol_client_connected(uint32_t id);   // WS_EVT_CONNECT (async_tcp task)
void protocol_client_gone(uint32_t id);        // WS_EVT_DISCONNECT (async_tcp task)
void protocol_handle(AsyncWebSocketClient* client, const char* msg, size_t len); // async_tcp task
bool protocol_legacy_set(const char* key, long value); // old "id?value" messages, same checks; false: rejected
void protocol_loop(AsyncWebSocket& ws);        // loop(), after outputs_arbitrate(): patches and answers to "get"
int  protocol_v1_count();                      // clients that use the old flat JSON
void protocol_send_legacy(AsyncWebSocket& ws, const String& json); // flat JSON to the v1 clients only

#endif
