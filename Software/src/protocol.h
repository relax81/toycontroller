#ifndef PROTOCOL_H
#define PROTOCOL_H

// JSON protocol of the WebSocket (v2). The old "id?value" messages stay valid.
//
// Client -> server (all with an optional "id" that comes back in ack / err):
//   {"t":"get"}                         full state (answer: "state"); the first get marks the client as v2
//   {"t":"get","k":["ch1.pwm"]}         only these keys
//   {"t":"set","id":7,"d":{"ch1.pwm":50,"ble.map0.min":10}}   one or more keys
// Server -> client:
//   {"t":"ack","id":7}                          all keys accepted (queued, the state follows as patch)
//   {"t":"err","id":7,"applied":1,"errors":[{"k":"buzzer.bpm","code":"range","min":1,"max":255}]}
//   {"t":"err","id":7,"code":"parse","msg":"..."}    message level error
//   {"t":"state","n":41,"d":{"ch1.en":false,...}}    sent from loop()
//
// Keys are defined in one table (protocol.cpp): range checks there mirror the state limits.
// Everything the server sends about the state is built in loop() (single writer / reader).

#include <stdint.h>
#include <stddef.h>

class AsyncWebSocket;
class AsyncWebSocketClient;

void protocol_client_connected(uint32_t id);   // WS_EVT_CONNECT (async_tcp task)
void protocol_client_gone(uint32_t id);        // WS_EVT_DISCONNECT (async_tcp task)
void protocol_handle(AsyncWebSocketClient* client, const char* msg, size_t len); // async_tcp task
void protocol_loop(AsyncWebSocket& ws);        // loop(): answers pending "get" requests

#endif
