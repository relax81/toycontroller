#ifndef API_H
#define API_H

// HTTP API on the existing async web server (no protection, no key, CORS open). It uses the same key
// table, checks and event queue as the WebSocket protocol (protocol.cpp); reads come from the snapshot
// that loop() prepares, so the async_tcp task never touches live state. Reference: docs/API.md
//
//   GET  /api/state                     whole state (snapshot) + running for= timers
//   GET  /api/keys                      self description of all keys, commands and parameters
//   POST /api/set   {"ch1.en":true,"ch1.pwm":60}     (also {"d":{...}}), optional "for":<s>
//   GET  /api/set?ch1.en=1&ch1.pwm=60[&for=<s>]
//   POST /api/cmd   {"c":"collar.beep"}  |  GET /api/cmd?c=all_off
//   GET  /api/toggle?k=ch1.en[&for=<s>]
//   OPTIONS on all of them (CORS preflight)
//
// for=<1-3600> (seconds) or for_ms=<100-3600000> (milliseconds, not both) applies to the "*.en" keys that this call switches on: after that time
// they are set to 0 again (only if still 1). Max. 8 timers, a new call for the same key replaces it.

class AsyncWebServer;

void api_setup(AsyncWebServer& server); // once, before server.begin()
void api_loop();                        // every loop() pass, after protocol_loop(): expires the for= timers

#endif
