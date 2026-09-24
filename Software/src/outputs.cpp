#include <Arduino.h>
#include "driver/ledc.h"
#include "config.h"
#include "state.h"
#include "outputs.h"

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
