#ifndef CONFIG_H
#define CONFIG_H

// Constants, pin defines, fonts and menu sizes moved out of main.cpp.
// Only #define, const (internal linkage) and extern declarations belong here.

#include <Arduino.h>
#include <U8g2lib.h>
#include "pinout.h"

// set the font types being used
const uint8_t* const font_status_messages = u8g2_font_crox4hb_tr;
const uint8_t* const font_main_menu = u8g2_font_t0_13b_mf;
const uint8_t* const font_manual_menu = u8g2_font_ncenB08_tr;
const uint8_t* const font_bluetooth_menu = u8g2_font_pixzillav1_tr;
const uint8_t* const font_check_symbol = u8g2_font_open_iconic_check_1x_t;
const uint8_t* const font_wifi_symbol = u8g2_font_open_iconic_www_1x_t;

// DogCollar
#define PIN_TRANSMITTER 15  // gpio15 is a strapping pin that can cause issues at bootup

// Encoder
  //depending on your encoder - try 1,2 or 4 to get expected behaviour
  #define ROTARY_ENCODER_STEPS 4
  #define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */

// PWM settings
  const int freq = 5000;
  const int resolution = 8;
  // LEDC channels: PWM1-4 use the pairs (0,1) (2,3) at 5 kHz, the buzzer has the
  // pair (4,5) and the pump the pair (6,7) for itself, because each pair shares one
  // timer / frequency
  const int PWMOUT_1 = 0; // max 30v ch1
  const int PWMOUT_2 = 1; // max 30v ch2
  const int PWMOUT_3 = 2; // 5v ch1
  const int PWMOUT_4 = 3; // 5v ch2
  const int buzzer = 4;
  const int pumpFrequency = 500;
  const int pumpOUT = 6; // Pump PWM Output
  const int pwmOutChannel[4] = {PWMOUT_1, PWMOUT_2, PWMOUT_3, PWMOUT_4}; // index i = Ch(i+1)
  const int buzzerFrequency = 2000; // initial buzzerFrequency
// LEDC: the channels share one timer (= one frequency) per pair (0,1) (2,3) (4,5) (6,7).
// Two channels in the same pair with different frequencies would silently change
// each other's frequency, so this is checked at compile time.
#define LEDC_PAIR_OK(a, fa, b, fb) ((a) != (b) && ((a) / 2 != (b) / 2 || (fa) == (fb)))
static_assert(
  LEDC_PAIR_OK(PWMOUT_1,freq,PWMOUT_2,freq) &&
  LEDC_PAIR_OK(PWMOUT_1,freq,PWMOUT_3,freq) &&
  LEDC_PAIR_OK(PWMOUT_1,freq,PWMOUT_4,freq) &&
  LEDC_PAIR_OK(PWMOUT_1,freq,buzzer,buzzerFrequency) &&
  LEDC_PAIR_OK(PWMOUT_1,freq,pumpOUT,pumpFrequency) &&
  LEDC_PAIR_OK(PWMOUT_2,freq,PWMOUT_3,freq) &&
  LEDC_PAIR_OK(PWMOUT_2,freq,PWMOUT_4,freq) &&
  LEDC_PAIR_OK(PWMOUT_2,freq,buzzer,buzzerFrequency) &&
  LEDC_PAIR_OK(PWMOUT_2,freq,pumpOUT,pumpFrequency) &&
  LEDC_PAIR_OK(PWMOUT_3,freq,PWMOUT_4,freq) &&
  LEDC_PAIR_OK(PWMOUT_3,freq,buzzer,buzzerFrequency) &&
  LEDC_PAIR_OK(PWMOUT_3,freq,pumpOUT,pumpFrequency) &&
  LEDC_PAIR_OK(PWMOUT_4,freq,buzzer,buzzerFrequency) &&
  LEDC_PAIR_OK(PWMOUT_4,freq,pumpOUT,pumpFrequency) &&
  LEDC_PAIR_OK(buzzer,buzzerFrequency,pumpOUT,pumpFrequency),
  "LEDC channel pair conflict: two channels with different frequencies share a timer (ch/2)");

// Main Menu New
const int MainMenuNumItems = 5; // number of items in the list
const int MainMenuMaxItemLength = 20; // maximum characters for the item name
extern char MainMenuItems [MainMenuNumItems] [MainMenuMaxItemLength];
// Bluetooth Menu
const int OutputNumItems = 7; // number of items in the list
const int OutputItemsMaxLength = 20; // maximum characters for the item name
extern char OutputItems [OutputNumItems] [OutputItemsMaxLength];

#endif
