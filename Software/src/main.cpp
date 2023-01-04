// activate deactivate serial output for debugging
#define DEBUG 1
#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)      
#define debugln(x)
#endif

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <TickTwo.h>
#include <AiEsp32RotaryEncoder.h>
#include <pinout.h>

bool buttonPressed = false;
int encoderPosition = 0;
bool drawcolorstate = true;

int menu = 1;

int Ch15vOn = 0;
int Ch15vOff = 0;
int Ch15vPWM = 0;
int Ch25vOn = 0;
int Ch25vOff = 0;
int Ch25vPWM = 0;
int Ch130vOn = 0;
int Ch130vOff = 0;
int Ch130vPWM = 0;
int Ch230vOn = 0;
int Ch230vOff = 0;
int Ch230vPWM = 0;

// PWM settings
const int freq = 5000;
const int resolution = 8;
const int PWMOUT_1 = 1; // max 30v ch1
const int PWMOUT_2 = 2; // max 30v ch2
const int PWMOUT_3 = 3; // 5v ch1
const int PWMOUT_4 = 4; // 5v ch2


//Encoder
  //depending on your encoder - try 1,2 or 4 to get expected behaviour
  #define ROTARY_ENCODER_STEPS 4
  #define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
  //instead of changing here, rather change numbers above
  AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

// Display Type
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Timer
  void blinktext();
  void buzzerOn();
  TickTwo timer1(blinktext, 400); // flash the display text every second



// Encoder Functions
  void rotary_onButtonClick()
      {
        static unsigned long lastTimePressed = 0;
        //ignore multiple press in that time milliseconds
        if (millis() - lastTimePressed < 200)
        {
          return;
        }
        lastTimePressed = millis();
        buttonPressed = true;
      }
  void rotary_loop()
      {
        //dont print anything unless value changed
        if (rotaryEncoder.encoderChanged())
        {
          encoderPosition = rotaryEncoder.readEncoder();
                }
        if (rotaryEncoder.isEncoderButtonClicked())
        {
          rotary_onButtonClick();
        }

      }
  void IRAM_ATTR readEncoderISR()
    {
      rotaryEncoder.readEncoder_ISR();
    }

void blinktext()
  {
    drawcolorstate = !drawcolorstate;
  }  


void displayMenu()
  {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(1,8,"5v1");
  u8g2.drawStr(30,8,"5v2");
  u8g2.drawStr(60,8,"30v1");
  u8g2.drawStr(95,8,"30v2");
  u8g2.drawHLine(0,13,128);
  u8g2.drawVLine(24,0,64);
  u8g2.drawVLine(54,0,64);
  u8g2.drawVLine(89,0,64);
  }

void displayValues()
  {
  // 1st line  
  u8g2.setCursor(1,25);
  u8g2.print(Ch15vOn);
  u8g2.setCursor(30,25);
  u8g2.print(Ch25vOn);
  u8g2.setCursor(62,25);
  u8g2.print(Ch130vOn);
  u8g2.setCursor(98,25);
  u8g2.print(Ch230vOn);
  // 2nd line
  u8g2.setCursor(1, 40);
  u8g2.print(Ch15vOff);
  u8g2.setCursor(30,40);
  u8g2.print(Ch25vOff);
  u8g2.setCursor(62,40);
  u8g2.print(Ch130vOff);
  u8g2.setCursor(98,40);
  u8g2.print(Ch230vOff);
  // 3rd line
  u8g2.setCursor(1, 55);
  u8g2.print(Ch15vPWM);
  u8g2.setCursor(30,55);
  u8g2.print(Ch25vPWM);
  u8g2.setCursor(62,55);
  u8g2.print(Ch130vPWM);
  u8g2.setCursor(98,55);
  u8g2.print(Ch230vPWM);
  }

  void menuSystem() {
  switch (menu) {
    case 1: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch15vOn = encoderPosition;
      u8g2.setCursor(1,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch15vOn);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch15vOff);
          encoderPosition = Ch15vOff;
          menu++;
          }
      break;

    case 2: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch15vOff = encoderPosition;
      u8g2.setCursor(1,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch15vOff);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch15vPWM);
          encoderPosition = Ch15vPWM;
          menu++;
          }
      break;

    case 3: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch15vPWM = encoderPosition;
      u8g2.setCursor(1,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch15vPWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch25vOn);
          encoderPosition = Ch25vOn;
          menu++;
          }
      break;

    case 4: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch25vOn = encoderPosition;
      u8g2.setCursor(30,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch25vOn);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch25vOff);
          encoderPosition = Ch25vOff;
          menu++;
          }
      break;

    case 5: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch25vOff = encoderPosition;
      u8g2.setCursor(30,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch25vOff);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch25vPWM);
          encoderPosition = Ch25vPWM;
          menu++;
          }
      break;

    case 6: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch25vPWM = encoderPosition;
      u8g2.setCursor(30,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch25vPWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch130vOn);
          encoderPosition = Ch130vOn;
          menu++;
          }
      break;
    
    case 7: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch130vOn = encoderPosition;
      u8g2.setCursor(62,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch130vOn);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch130vOff);
          encoderPosition = Ch130vOff;
          menu++;
          }
      break;

    case 8: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch130vOff = encoderPosition;
      u8g2.setCursor(62,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch130vOff);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch130vPWM);
          encoderPosition = Ch130vPWM;
          menu++;
          }
      break;

    case 9: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch130vPWM = encoderPosition;
      u8g2.setCursor(62,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch130vPWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch230vOn);
          encoderPosition = Ch230vOn;
          menu++;
          }
      break;

      case 10: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch230vOn = encoderPosition;
      u8g2.setCursor(98,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch230vOn);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch230vOff);
          encoderPosition = Ch230vOff;
          menu++;
          }
      break;

    case 11: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch230vOff = encoderPosition;
      u8g2.setCursor(98,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch230vOff);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch230vPWM);
          encoderPosition = Ch230vPWM;
          menu++;
          }
      break;

    case 12: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch230vPWM = encoderPosition;
      u8g2.setCursor(98,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch230vPWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch15vOn);
          encoderPosition = Ch15vOn;
          menu = 1;
          }
      break;

    default:
      // Tue etwas, im Defaultfall
      // Dieser Fall ist optional
      break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
}






  
void setup() {
  Serial.begin(115200);
  debugln("setup started");

  // Pins
  pinMode(buzzer, OUTPUT);
  pinMode(wsLED, OUTPUT);
  pinMode(CH1_5V, OUTPUT);
  pinMode(CH2_5V, OUTPUT);
  pinMode(CH1_30VMax, OUTPUT);
  pinMode(CH2_30VMax, OUTPUT);
  pinMode(PIR, INPUT);
  pinMode(RF_433, OUTPUT);
  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  digitalWrite(RF_433, LOW);

  // PWM
  ledcSetup(PWMOUT_1, freq, resolution);
  ledcSetup(PWMOUT_2, freq, resolution);
  ledcSetup(PWMOUT_3, freq, resolution);
  ledcSetup(PWMOUT_4, freq, resolution);
  ledcAttachPin(CH1_30VMax, PWMOUT_1);
  ledcAttachPin(CH2_30VMax, PWMOUT_2);
  ledcAttachPin(CH1_5V, PWMOUT_3);
  ledcAttachPin(CH2_5V, PWMOUT_4);

   //Encoder
  //we must initialize rotary encoder
	rotaryEncoder.begin();
	rotaryEncoder.setup(readEncoderISR);
	//set boundaries and if values should cycle or not
	//in this example we will set possible values between 0 and 60;
	bool circleValues = true;
	rotaryEncoder.setBoundaries(0, 60, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
  /*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
	//rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
	rotaryEncoder.setAcceleration(50); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

  //Display
  timer1.start(); // timer for blinking text
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.clearDisplay();
  u8g2.setFontMode(1);
  displayMenu();
  displayValues();
  u8g2.sendBuffer();
}

void loop() {
  timer1.update(); // flash display text timer
  
  // Encoder
  rotary_loop();

  // Display
  u8g2.clearBuffer();
  displayMenu();
  displayValues();
  menuSystem();
  u8g2.sendBuffer();

  // PWM Output
  ledcWrite(PWMOUT_1,Ch130vPWM);
  ledcWrite(PWMOUT_2,Ch230vPWM);
  ledcWrite(PWMOUT_3,Ch15vPWM);
  ledcWrite(PWMOUT_4,Ch25vPWM);
}