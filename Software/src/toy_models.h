#ifndef TOY_MODELS_H
#define TOY_MODELS_H

#include <stdint.h>

// Selectable Lovense toy models for the BLE emulation. The list index is stored in the NVS
// ("toy") and used as value of the key "ble.toy". Changing the model restarts the device,
// the BLE identity (name, DeviceType answer, UUIDs) is set once in the BLE init.
//
// Sources: buttplug.io Lovense protocol page (type letters, DeviceType format, UUID generations).
// NOT verified against a real capture: the advertised names, the firmware number "40" and the
// use of the 2nd generation UUID for all models. "Dolce" is the identity used so far ("J:40:...").

#define TOY_MODEL_COUNT 6

struct ToyModel {
  const char* label;    // shown in the web UI (same order as the options in index.html)
  const char* bleName;  // advertised name
  const char* letter;   // model letter in the DeviceType answer
  const char* fw;       // firmware number in the DeviceType answer
  const char* svc;      // service UUID
  const char* tx;       // characteristic the app writes to
  const char* rx;       // characteristic the toy notifies on
  uint8_t vibChannels;  // vibration channels: 2 = Vibrate1 + Vibrate2, 1 = Vibrate only
  char    extra;        // extra function that uses the V2 slot when there is only one vibration channel:
                        // 'R' = Rotate (0-20), 0 = none
};

extern const ToyModel TOY_MODELS[TOY_MODEL_COUNT];

#endif
