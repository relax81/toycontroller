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



//Encoder
  //depending on your encoder - try 1,2 or 4 to get expected behaviour
  #define ROTARY_ENCODER_STEPS 2
  #define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
  //instead of changing here, rather change numbers above
  AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

// Display Type
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Timer
  void blinktext();
  TickTwo timer2(blinktext, 400); // flash the display text every second

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

void displayLayout()
  {
  u8g2.setFont(u8g2_font_ncenB08_tr);	// choose a suitable font
  u8g2.drawStr(22,10,"Toy Controller");
  u8g2.drawHLine(0,13,128);
  }


void setup() {
  Serial.begin(115200);
  debugln("setup started");
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
  digitalWrite(CH1_5V, LOW);
  digitalWrite(CH2_5V, LOW);
  digitalWrite(CH1_30VMax, LOW);
  digitalWrite(CH2_30VMax, LOW);
  digitalWrite(RF_433, LOW);

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
  timer2.start(); // timer for blinking text
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.clearDisplay();
  u8g2.setFontMode(1);
  displayLayout();
  u8g2.sendBuffer();
}

void loop() {
  // put your main code here, to run repeatedly:
}