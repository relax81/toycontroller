#include <Arduino.h>
#include "driver/ledc.h"
#include "config.h"
#include "state.h"
#include "outputs.h"

// DogCollar (433 MHz), all sends go through collar_send()
  // Unique ID (16 bit) of the Shock Collar. You can also keep this and use pairing mode of the collar
  String uniqueKeyOfDevice = "0010110011011000";
  DogCollar dg(PIN_TRANSMITTER,uniqueKeyOfDevice);

void collar_send(CollarMode mode, int strength) {
  dg.sendCollar(CollarChannel::CH1, mode, strength);
}

void outputs_init() {
  // Pins
  pinMode(buzzerPin, OUTPUT);
  pinMode(wsLED, OUTPUT);
  pinMode(CH1_5V, OUTPUT);
  pinMode(CH2_5V, OUTPUT);
  pinMode(CH1_30VMax, OUTPUT);
  pinMode(CH2_30VMax, OUTPUT);
  pinMode(RF_433, OUTPUT); // uncomment if jtag debugging is used
  pinMode(pumpPin, OUTPUT);
  digitalWrite(RF_433, LOW);

  // define PWM
  ledcSetup(PWMOUT_1, freq, resolution);
  ledcSetup(PWMOUT_2, freq, resolution);
  ledcSetup(PWMOUT_3, freq, resolution);
  ledcSetup(PWMOUT_4, freq, resolution);
  ledcSetup(buzzer, buzzerFrequency, resolution);
  ledcSetup(pumpOUT, pumpFrequency, resolution);
  ledcAttachPin(CH1_30VMax, PWMOUT_1);
  ledcAttachPin(CH2_30VMax, PWMOUT_2);
  ledcAttachPin(CH1_5V, PWMOUT_3);
  ledcAttachPin(CH2_5V, PWMOUT_4);
  ledcAttachPin(buzzerPin, buzzer);
  ledcAttachPin(pumpPin, pumpOUT);
#if DEBUG_LEDC == 1
  debug_ledc();
#endif
}

// diagnostics only: configured vs. actual LEDC frequency; the ESP32 LEDC shares one
// timer between the channel pairs (0,1) (2,3) (4,5) (6,7).
// ledcReadFreq() returns 0 while the duty is 0, so the timer is read directly.
#if DEBUG_LEDC == 1
void debug_ledc() {
  const char* ledcNames[] = {"PWM1", "PWM2", "PWM3", "PWM4", "buzzer", "pump"};
  const int ledcChannels[] = {PWMOUT_1, PWMOUT_2, PWMOUT_3, PWMOUT_4, buzzer, pumpOUT};
  const int ledcSetFreq[] = {freq, freq, freq, freq, buzzerFrequency, pumpFrequency};
  for (int i = 0; i < 6; i++) {
    unsigned int actual = (unsigned int)ledc_get_freq(LEDC_HIGH_SPEED_MODE, (ledc_timer_t)((ledcChannels[i] / 2) % 4));
    Serial.printf("[ledc] %-6s ch=%d timer=%d set=%d Hz actual=%u Hz%s\n", ledcNames[i], ledcChannels[i], (ledcChannels[i] / 2) % 4, ledcSetFreq[i], actual, (actual == (unsigned int)ledcSetFreq[i]) ? "" : "  <-- MISMATCH");
  }
}
#endif

// control pwm outputs in web or manual mode
  void PWM_Output(){
  // Outputs 1-4 (channel i = Ch(i+1))
  for (int i = 0; i < 4; i++) {
    // rising edge of "enabled" (and not held by BLE): start with the On phase
    // instead of the stale timeStarted / paused state of the last run
    bool active = state.out[i].enabled && !state.ble.hold[outputId(i)];
    if (active && !state.rt[i].wasEnabled) {
      state.rt[i].timeStarted = millis();
      state.rt[i].paused = false;
    }
    state.rt[i].wasEnabled = active;

    if (state.ble.hold[outputId(i)]) {
      // driven by BLE
    }
    else if ((state.rt[i].paused == false) && (state.out[i].enabled == true))
    {
       int mapped_PWM;
       mapped_PWM = map(state.out[i].pwm, 0, 100, 0, 255);
       ledcWrite(pwmOutChannel[i], mapped_PWM);
       if ((state.out[i].off > 0) && (millis() - state.rt[i].timeStarted >= state.out[i].on * 100)) {
        state.rt[i].paused = true;
        state.rt[i].timeStopped = millis();
      }
    }  
    else if ((state.rt[i].paused == true) && (state.out[i].enabled == true))
    {
      ledcWrite(pwmOutChannel[i], 0);
      if (millis() - state.rt[i].timeStopped >= state.out[i].off * 100)
      {
        state.rt[i].paused = false;
        state.rt[i].timeStarted = millis();
      }
    }
  }
  // Pump Output 5: same On/Off cycle as Ch1-4 (off = 0: runs continuously)
  {
    bool pumpActive = state.pump.enabled && !state.ble.hold[5];
    if (pumpActive && !state.pumpRt.wasEnabled) {
      state.pumpRt.timeStarted = millis();
      state.pumpRt.paused = false;
    }
    state.pumpRt.wasEnabled = pumpActive;
  }
  if (state.ble.hold[5]) {
    // driven by BLE
  }
  else if ((state.pumpRt.paused == false) && (state.pump.enabled == true))
  {
    int mapped_pump_PWM;
    mapped_pump_PWM = map(state.pump.pwm, 0, 100, 0, 255);
    ledcWrite(pumpOUT, mapped_pump_PWM);
    if ((state.pump.off > 0) && (millis() - state.pumpRt.timeStarted >= (unsigned long)state.pump.on * 100)) {
      state.pumpRt.paused = true;
      state.pumpRt.timeStopped = millis();
    }
  }
  else if ((state.pumpRt.paused == true) && (state.pump.enabled == true))
  {
    ledcWrite(pumpOUT, 0);
    if (millis() - state.pumpRt.timeStopped >= (unsigned long)state.pump.off * 100)
    {
      state.pumpRt.paused = false;
      state.pumpRt.timeStarted = millis();
    }
  }
  else {
    ledcWrite(pumpOUT, 0);
  }
}

// disable outputs 
  void disable_Outputs()
{
  for (int i = 0; i < 4; i++) {
    if (!state.out[i].enabled && !state.ble.hold[outputId(i)]) {
      state.rt[i].paused = false;
      ledcWrite(pwmOutChannel[i], 0);
    }
  }
  if (!state.pump.enabled && !state.ble.hold[5]){
    state.pumpRt.paused = false;
    ledcWrite(pumpOUT, 0);
    state.pump.enabled = false;
  }
}

// control pwm outputs in bluetooth mode
void bluetooth_write_pwm(int output, int mapped_PWM) {
  switch (output) {
    case 1:
      ledcWrite(PWMOUT_1, mapped_PWM);
      break;
    case 2:
      ledcWrite(PWMOUT_2, mapped_PWM);
      break;
    case 3:
      ledcWrite(PWMOUT_3, mapped_PWM);
      break;
    case 4:
      ledcWrite(PWMOUT_4, mapped_PWM);
      break;
    case 5:
      ledcWrite(pumpOUT, mapped_PWM);
      break;
    case 6: 

      if (state.collar.btOnlyChanges == true) {
        if (mapped_PWM != state.collar.previousShock){
          collar_send(CollarMode::Shock, mapped_PWM);
        }
      state.collar.previousShock = mapped_PWM;
      }

      else {
        collar_send(CollarMode::Shock, mapped_PWM);
      }

      break;
  }
}

void buzzer_Metronome(unsigned long nowMs) {
    state.buzzer.beatInterval = 60000 / state.buzzer.bpm;
    int buzzerPWM = map(state.buzzer.volume, 0, 10, 0, 140);
    if (!state.buzzer.isPlaying) { // turn on after the pause between the beeps
      if ((long)(nowMs - state.buzzer.previousMillis) >= (long)state.buzzer.beatInterval - state.buzzer.onTimeMs) {
        ledcWrite (buzzer, buzzerPWM);
        state.buzzer.isPlaying = true;
        state.buzzer.previousMillis = nowMs;
      }
    }
    else if (nowMs - state.buzzer.previousMillis >= (unsigned long)state.buzzer.onTimeMs) { // turn off
      ledcWrite (buzzer, 0);
      state.buzzer.isPlaying = false;
      state.buzzer.previousMillis = nowMs;
    }
}

// web / manual settings of an output id, to detect a change after a BLE command
static void webSettings(int id, int s[4]) {
  s[0] = s[1] = s[2] = s[3] = 0;
  if (id >= 1 && id <= 4) {
    const OutputChannel& c = state.out[id - 1];
    s[0] = c.enabled; s[1] = c.on; s[2] = c.off; s[3] = c.pwm;
  }
  else if (id == 5) {
    s[0] = state.pump.enabled; s[1] = state.pump.pwm; s[2] = state.pump.on; s[3] = state.pump.off;
  }
  else if (id == 6) {
    s[0] = state.collar.enabled;
  }
}

// BLE has priority on an output while its level is > 0. A BLE command also counts at
// level 0: the latch keeps the priority while connected until the web / encoder changes
// the output afterwards (last source wins) or BLE disconnects.
void outputs_arbitrate() {
  for (int i = 0; i < 7; i++) state.ble.hold[i] = false;
  for (int k = 0; k < 2; k++) {
    BleLatch& l = state.ble.latch[k];
    int out = state.ble.map[k].output;
    int vib = state.ble.in.vib[k];
    if (!state.ble.in.connected || out != l.output) l.active = false;
    if (out > 0 && state.ble.in.connected && vib != l.prevVib) {
      l.active = true;                 // new BLE command
      l.output = out;
      webSettings(out, l.snap);
    }
    else if (l.active && vib == 0) {
      int now[4];
      webSettings(out, now);
      for (int j = 0; j < 4; j++) if (now[j] != l.snap[j]) l.active = false; // web / encoder changed it
    }
    l.prevVib = vib;
    if ((out > 0) && (vib > 0 || l.active)) state.ble.hold[out] = true;
  }
  state.ble.collarMapped = (state.ble.map[0].output == 6 || state.ble.map[1].output == 6);
}

// switch off everything the web interface controls (state only)
void outputs_all_off() {
  state.out[0].enabled = false;
  state.out[1].enabled = false;
  state.out[2].enabled = false;
  state.out[3].enabled = false;
  state.pump.enabled = false;
  state.collar.enabled = false;
}
