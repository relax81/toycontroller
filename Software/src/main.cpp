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
#include "true-credentials.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <TickTwo.h>
#include <AiEsp32RotaryEncoder.h>
#include <pinout.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <Arduino_JSON.h>



bool buttonPressed = false;
int encoderPosition = 0;
bool drawcolorstate = true;

int menu = 1;

int Ch1_On = 0;
int Ch1_Off = 0;
int Ch1_PWM = 0;
int Ch1_Toggle = 0;
int Ch2_On = 0;
int Ch2_Off = 0;
int Ch2_PWM = 0;
int Ch3_On = 0;
int Ch3_Off = 0;
int Ch3_PWM = 0;
int Ch4_On = 0;
int Ch4_Off = 0;
int Ch4_PWM = 0;

// PWM settings
const int freq = 5000;
const int resolution = 8;
const int PWMOUT_1 = 1; // max 30v ch1
const int PWMOUT_2 = 2; // max 30v ch2
const int PWMOUT_3 = 3; // 5v ch1
const int PWMOUT_4 = 4; // 5v ch2
const int buzzer = 5;
unsigned long pwm1_previousMillis = 0;
unsigned long pwm2_previousMillis = 0;
unsigned long pwm3_previousMillis = 0;
unsigned long pwm4_previousMillis = 0;
bool pwm1_enabled = false;
bool pwm2_enabled = false;
bool pwm3_enabled = false;
bool pwm4_enabled = false;
bool buzzer_enabled = false;

String tempString;

// WebSocket
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");
//Json Variable to Hold Slider Values
JSONVar values;
String json_string;




// Initialize SPIFFS
void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}
// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}
void notifyClients(String sliderValues) {
  ws.textAll(sliderValues);
}


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
  void buzzing();
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

void update_values_ws();

void displayMenu()
  {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(1,8,"Ch1");
  u8g2.drawStr(34,8,"Ch2");
  u8g2.drawStr(69,8,"Ch3");
  u8g2.drawStr(102,8,"Ch4");
  u8g2.drawHLine(0,13,128);
  u8g2.drawVLine(30,0,64);
  u8g2.drawVLine(61,0,64);
  u8g2.drawVLine(95,0,64);
  }

void displayValues()
  {
  // 1st line  
  u8g2.setCursor(1,25);
  u8g2.print(Ch1_On);
  u8g2.setCursor(36,25);
  u8g2.print(Ch2_On);
  u8g2.setCursor(69,25);
  u8g2.print(Ch3_On);
  u8g2.setCursor(102,25);
  u8g2.print(Ch4_On);
  // 2nd line
  u8g2.setCursor(1, 40);
  u8g2.print(Ch1_Off);
  u8g2.setCursor(36,40);
  u8g2.print(Ch2_Off);
  u8g2.setCursor(69,40);
  u8g2.print(Ch3_Off);
  u8g2.setCursor(102,40);
  u8g2.print(Ch4_Off);
  // 3rd line
  u8g2.setCursor(1, 55);
  u8g2.print(Ch1_PWM);
  u8g2.setCursor(36,55);
  u8g2.print(Ch2_PWM);
  u8g2.setCursor(69,55);
  u8g2.print(Ch3_PWM);
  u8g2.setCursor(102,55);
  u8g2.print(Ch4_PWM);
  }

void menuSystem() {
  switch (menu) {

    case 1: //
      rotaryEncoder.setBoundaries(1, 4, false); //0-4
      menu = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(1,8,"Ch1");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch1_On);
          encoderPosition = Ch1_On;
          menu = menu * 10;
          }
      break;

      case 2: //
      rotaryEncoder.setBoundaries(1, 4, false); //0-4
      menu = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(34,8,"Ch2");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch2_On);
          encoderPosition = Ch2_On;
          menu = menu * 10;
          }
      break;

    case 3: //
      rotaryEncoder.setBoundaries(1, 4, false); //0-4
      menu = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(69,8,"Ch3");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch3_On);
          encoderPosition = Ch3_On;
          menu = menu * 10;
          }
      break;
    
    case 4: //
      rotaryEncoder.setBoundaries(1, 4, false); //0-4
      menu = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(102,8,"Ch4");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch4_On);
          encoderPosition = Ch4_On;
          menu = menu * 10;
          }
      break;


    case 10: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch1_On = encoderPosition;
      u8g2.setCursor(1,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_On);  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_a"] = Ch1_On;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch1_Off);
          encoderPosition = Ch1_Off;
          menu++;
          }
      break;

    case 11: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch1_Off = encoderPosition;
      u8g2.setCursor(1,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch1_PWM);
          encoderPosition = Ch1_PWM;
          menu++;
          }
      break;

    case 12: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch1_PWM = encoderPosition;
      u8g2.setCursor(1,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(1);
          encoderPosition = 1;
          menu = 1;
          }
      break;

    case 20: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch2_On = encoderPosition;
      u8g2.setCursor(36,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_On);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch2_Off);
          encoderPosition = Ch2_Off;
          menu++;
          }
      break;

    case 21: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch2_Off = encoderPosition;
      u8g2.setCursor(36,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch2_PWM);
          encoderPosition = Ch2_PWM;
          menu++;
          }
      break;

    case 22: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch2_PWM = encoderPosition;
      u8g2.setCursor(36,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(2);
          encoderPosition = 2;
          menu = 2;
          }
      break;
    
    case 30: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch3_On = encoderPosition;
      u8g2.setCursor(69,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_On);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch3_Off);
          encoderPosition = Ch3_Off;
          menu++;
          }
      break;

    case 31: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch3_Off = encoderPosition;
      u8g2.setCursor(69,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch3_PWM);
          encoderPosition = Ch3_PWM;        
          menu++;
          }
      break;

    case 32: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch3_PWM = encoderPosition;
      u8g2.setCursor(69,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(3);
          encoderPosition = 3;
          menu = 3;
          }
      break;

      case 40: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch4_On = encoderPosition;
      u8g2.setCursor(102,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_On);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch4_Off);
          encoderPosition = Ch4_Off;
          menu++;
          }
      break;

    case 41: // 
      rotaryEncoder.setBoundaries(0, 100, false); //0-99 
      Ch4_Off = encoderPosition;
      u8g2.setCursor(102,40);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch4_PWM);          
          encoderPosition = Ch4_PWM;         
          menu++;
          }
      break;

    case 42: // 
      rotaryEncoder.setBoundaries(0, 255, true); //0-99 
      Ch4_PWM = encoderPosition;
      u8g2.setCursor(102,55);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(4);
          encoderPosition = 4;
          menu = 4;
          }
      break;

    default:
      // Tue etwas, im Defaultfall
      // Dieser Fall ist optional
      break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
}

void handleWebSocketMessage_ws(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  int slider;
  char* message;

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    message = (char*)data;
    debugln(message);

    switch (message[0])
    {
      case 'r':
        switch (message[5])
        {
          case 'l':
            //     values["ramp_level"] = mk312_get_ramp_level();

            break;

          case 't':
            //   values["ramp_time"] = mk312_get_ramp_time();

            break;
          case 's':
            // mk312_ramp_start();
            break;
        }
        break;

      case 's': //slider
      debugln("slider triggered");
        slider = atoi(message + 9);
        switch (message[7])
        {
          case 'a':
            Ch1_On = slider;
            values["slider_a"] = Ch1_On;
            break;

          case 'b':
            Ch1_Off = slider;
            values["slider_b"] = Ch1_Off;
            break;

          case 'c':
            Ch1_PWM = slider;
            values["slider_c"] = Ch1_PWM;
            break;
          
          case 'd':
            Ch2_On = slider;
            values["slider_d"] = Ch2_On;
            break;

          case 'e':
            Ch2_Off = slider;
            values["slider_e"] = Ch2_Off;
            break;

          case 'f':
            Ch2_PWM = slider;
            values["slider_f"] = Ch2_PWM;
            break;
          
          case 'g':
            Ch3_On = slider;
            values["slider_g"] = Ch3_On;
            break;

          case 'h':
            Ch3_Off = slider;
            values["slider_h"] = Ch3_Off;
            break;

          case 'i':
            Ch3_PWM = slider;
            values["slider_i"] = Ch3_PWM;
            break;
          
          case 'j':
            Ch4_On = slider;
            values["slider_j"] = Ch4_On;
            break;

          case 'k':
            Ch4_Off = slider;
            values["slider_k"] = Ch4_Off;
            break;

          case 'l':
            Ch4_PWM = slider;
            values["slider_l"] = Ch4_PWM;
            break;

        }
        break;

      case 'a'://adc
        if (message[5] == 'n')//on
        {
          buzzer_enabled = true;
        }
        else
        {
          buzzer_enabled = false;
        }
        values["adc"] = buzzer_enabled ? "on" : "off";
        Serial.println("adc output");
        Serial.println(values["adc"]);
        break;

      //  case 'm'://mode
      //   newmode = strtol(message + 5, NULL, 16);
      //   values["mode"] = newmode;
      //   break;

        }

    json_string = JSON.stringify(values);
    ws.textAll(json_string);
  }
}

void onEvent_ws(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type)
  {
    case WS_EVT_CONNECT:
      //serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      //serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage_ws(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void update_values_ws(){
    json_string = JSON.stringify(values);
    ws.textAll(json_string);
}

void init_ws() {
  ws.onEvent(onEvent_ws);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  debugln("setup started");

  // Pins
  pinMode(buzzerPin, OUTPUT);
  pinMode(wsLED, OUTPUT);
  pinMode(CH1_5V, OUTPUT);
  pinMode(CH2_5V, OUTPUT);
  pinMode(CH1_30VMax, OUTPUT);
  pinMode(CH2_30VMax, OUTPUT);
  pinMode(PIR, INPUT);
  pinMode(RF_433, OUTPUT); // uncomment if jtag debugging is used
  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  digitalWrite(RF_433, LOW);

  // PWM
  ledcSetup(PWMOUT_1, freq, resolution);
  ledcSetup(PWMOUT_2, freq, resolution);
  ledcSetup(PWMOUT_3, freq, resolution);
  ledcSetup(PWMOUT_4, freq, resolution);
  ledcSetup(buzzer, freq, resolution);
  ledcAttachPin(CH1_30VMax, PWMOUT_1);
  ledcAttachPin(CH2_30VMax, PWMOUT_2);
  ledcAttachPin(CH1_5V, PWMOUT_3);
  ledcAttachPin(CH2_5V, PWMOUT_4);
  ledcAttachPin(buzzerPin, buzzer);

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
  rotaryEncoder.setEncoderValue(1);

  //Display
  timer1.start(); // timer for blinking text
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.clearDisplay();
  u8g2.setFontMode(1);
  displayMenu();
  displayValues();
  u8g2.sendBuffer();

  initFS();
  initWiFi();
  init_ws();

  // Websocket stuff
  values["slider_a"] = 0;
  values["slider_b"] = 0;
  values["slider_c"] = 0;
  values["slider_d"] = 0;
  values["slider_e"] = 0;
  values["slider_f"] = 0;
  values["slider_g"] = 0;
  values["slider_h"] = 0;
  values["slider_i"] = 0;
  values["slider_j"] = 0;
  values["slider_k"] = 0;
  values["slider_l"] = 0;

  values["toggle_a"] = "off";
  values["toggle_b"] = "off";
  values["toggle_c"] = "off";
  values["toggle_d"] = "off";
  values["adc"] = "off";

  json_string = JSON.stringify(values);


  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();
  unsigned long currentMillis = millis();
  timer1.update(); // display blinking text timer

// PWM Output 1
  if ((currentMillis - pwm1_previousMillis >= Ch1_Off*1000) && (!pwm1_enabled) && (Ch1_Toggle != 0)) 
    {
      ledcWrite(PWMOUT_1,Ch1_PWM);
      pwm1_enabled = true;
      pwm1_previousMillis = currentMillis;
    }
  else if ((currentMillis - pwm1_previousMillis >= Ch1_On*1000) && (pwm1_enabled))
    {
      ledcWrite(PWMOUT_1,0);
      pwm1_enabled = false;
      pwm1_previousMillis = currentMillis;
    }
  // PWM Output 2
  else if ((currentMillis - pwm2_previousMillis >= Ch2_Off*1000) && (!pwm2_enabled) && (Ch2_On > 0)) 
    {
      ledcWrite(PWMOUT_2,Ch2_PWM);
      pwm2_enabled = true;
      pwm2_previousMillis = currentMillis;
    }
  else if ((currentMillis - pwm2_previousMillis >= Ch2_On*1000) && (pwm2_enabled))
    {
      ledcWrite(PWMOUT_2,0);
      pwm2_enabled = false;
      pwm2_previousMillis = currentMillis;
    }
  // PWM Output 3
  else if ((currentMillis - pwm3_previousMillis >= Ch3_Off*1000) && (!pwm3_enabled) && (Ch3_On > 0)) 
    {
      ledcWrite(PWMOUT_3,Ch3_PWM);
      pwm3_enabled = true;
      pwm3_previousMillis = currentMillis;
    }
  else if ((currentMillis - pwm3_previousMillis >= Ch3_On*1000) && (pwm3_enabled))
    {
      ledcWrite(PWMOUT_3,0);
      pwm3_enabled = false;
      pwm3_previousMillis = currentMillis;
    }
  // PWM Output 4
   else if ((currentMillis - pwm4_previousMillis >= Ch4_Off*1000) && (!pwm4_enabled) && (Ch4_On > 0)) 
    {
      ledcWrite(PWMOUT_4,Ch4_PWM);      
      pwm4_enabled = true;
      pwm4_previousMillis = currentMillis;
    }
  else if ((currentMillis - pwm4_previousMillis >= Ch4_On*1000) && (pwm4_enabled))
    {
      ledcWrite(PWMOUT_4,0);
      pwm4_enabled = false;
      pwm4_previousMillis = currentMillis;
    }

  
  // Encoder
  rotary_loop();

  // Display
  u8g2.clearBuffer();
  displayMenu();
  displayValues();
  menuSystem();
  u8g2.sendBuffer();

  // Buzzer test
  if (buzzer_enabled == true) {
    ledcWrite(buzzer,50);
  }
  else {
    ledcWrite(buzzer,0);
  }

}