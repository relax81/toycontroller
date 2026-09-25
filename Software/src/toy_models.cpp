#include "toy_models.h"

// 2nd generation (Nordic UART) profile: service ...0001, the app writes to ...0002, the toy notifies on ...0003
#define SVC "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define WR  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NT  "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

const ToyModel TOY_MODELS[TOY_MODEL_COUNT] = {
  { "Dolce", "LVS-Z001", "J", "40", SVC, WR, NT },
  { "Lush", "LVS-S001", "S", "40", SVC, WR, NT },
  { "Hush", "LVS-Z001", "Z", "40", SVC, WR, NT },
  { "Domi", "LVS-W001", "W", "40", SVC, WR, NT },
  { "Nora", "LVS-A001", "C", "40", SVC, WR, NT },
  { "Max", "LVS-B001", "B", "40", SVC, WR, NT },
  { "Edge", "LVS-P001", "P", "40", SVC, WR, NT },
};
