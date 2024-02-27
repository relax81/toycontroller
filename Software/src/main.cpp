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
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "DogCollar3.h"

// software version
String version = "0.1";

// set the font types being used
const uint8_t* font_status_messages = u8g2_font_crox4hb_tr;
const uint8_t* font_main_menu = u8g2_font_t0_13b_mf;
const uint8_t* font_manual_menu = u8g2_font_ncenB08_tr;
const uint8_t* font_bluetooth_menu = u8g2_font_pixzillav1_tr; 
const uint8_t* font_check_symbol = u8g2_font_open_iconic_check_1x_t;
const uint8_t* font_wifi_symbol = u8g2_font_open_iconic_www_1x_t;

void displayMenuManual();
void buttonMenuManual();
void displayBluetoothMenu();
void buttonMenuBluetooth();
void reset_Outputs();
void update_values_ws();
void bluetooth_write_pwm(int, int);
void disable_Outputs();

// DogCollar 
  #define PIN_TRANSMITTER 15  // gpio15 is a strapping pin that can cause issues at bootup
  // Unique ID (16 bit) of the Shock Collar. You can also keep this and use pairing mode of the collar
  String uniqueKeyOfDevice = "0010110011011000";
  DogCollar dg(PIN_TRANSMITTER,uniqueKeyOfDevice);
  int vibration;
  int shock;
  int beep;
  int keepawake;
  int collar_strength;
  int previous_shock = 30;
  bool Collar_Enable = false;
  bool button_beep = false;
  bool button_vib = false;
  bool button_shock = false;
  bool collar_bt_only_changes = true;
  unsigned long previous_Collar_Wakeup = 0; 
  unsigned long keep_Collar_Awake_Interval = 120000; // 2 Minutes

// Random - name later
  unsigned long currentMillis;
  int wlanstatus;
  bool WiFi_Enabled = false;
  bool BT_Enabled = false;
  bool buttonPressed = false;
  bool buttonLongPressed = false;
  int buttonDownCount = 0;
  int encoderPosition = 0;
  bool drawcolorstate = true;
  unsigned long lastTimePressed = 0;
  int item_selected = 0; // which item in the menu is selected
  int item_sel_previous; // previous item - used in the menu screen to draw the item before the selected one
  int item_sel_next; // next item - used in the menu screen to draw next item after the selected one
  int current_screen = 0;   // 0 = main menu, 
  int manualMenuSelect = 1; // from Manual Mode Menu
  int bluetoothMenuSelect = 1; // from bluetooth mode menu
  bool Ch1_Enable = false;
  bool Ch2_Enable = false;
  bool Ch3_Enable = false;
  bool Ch4_Enable = false;
  bool Pump_Enable = false;
  int Ch1_On = 0;
  int Ch1_Off = 0;
  int Ch1_PWM = 0;
  int Ch2_On = 0;
  int Ch2_Off = 0;
  int Ch2_PWM = 0;
  int Ch3_On = 0;
  int Ch3_Off = 0;
  int Ch3_PWM = 0;
  int Ch4_On = 0;
  int Ch4_Off = 0;
  int Ch4_PWM = 0;
  int pump_PWM = 0;
// Bluetooth Menu
  int BT_V1_Output = 0;
  int BT_V1_Min_PWM = 0;
  int BT_V1_Max_PWM = 255;
  int BT_V2_Output = 1;
  int BT_V2_Min_PWM = 0;
  int BT_V2_Max_PWM = 255;
  bool BT_V1_Paused = false;
  bool BT_V2_Paused = false;
// PWM settings
  const int freq = 5000;
  const int resolution = 8;
  const int PWMOUT_1 = 1; // max 30v ch1
  const int PWMOUT_2 = 2; // max 30v ch2
  const int PWMOUT_3 = 3; // 5v ch1
  const int PWMOUT_4 = 4; // 5v ch2
  const int buzzer = 5;
  const int pumpOUT = 6; // Pump PWM Output
  bool pwm1_paused = false;
  bool pwm2_paused = false;
  bool pwm3_paused = false;
  bool pwm4_paused = false;
  unsigned long pwm1_timeStarted = 0;
  unsigned long pwm1_timeStopped = 0;
  unsigned long pwm2_timeStarted = 0;
  unsigned long pwm2_timeStopped = 0;
  unsigned long pwm3_timeStarted = 0;
  unsigned long pwm3_timeStopped = 0;
  unsigned long pwm4_timeStarted = 0;
  unsigned long pwm4_timeStopped = 0;
  // String lb1_mode;
  // String lb2_mode;
  String tempString;
// Main Menu New
  const int MainMenuNumItems = 4; // number of items in the list 
  const int MainMenuMaxItemLength = 20; // maximum characters for the item name
  char MainMenuItems [MainMenuNumItems] [MainMenuMaxItemLength] = {"Manual","WiFi Status","Bluetooth","Info"};
// Bluetooth Menu
  const int OutputNumItems = 7; // number of items in the list 
  const int OutputItemsMaxLength = 20; // maximum characters for the item name
  char OutputItems [OutputNumItems] [OutputItemsMaxLength] = {"OFF","PWM1","PWM2","PWM3","PWM4","PUMP","Shoc"};
// buzzer 
  bool buzzer_Metronome_Enabled = false;
  int buzzerVolume = 5; // 0 - 10
  const int buzzerFrequency = 2000; // initial buzzerFrequency
  unsigned long buzzerPreviousMillis = 0;
  int buzzerBPM = 60; // 1 - 255
  int beatInterval = 60000 / buzzerBPM; // duration of one beat in milliseconds
  int buzzerOnTimeMS = 50;
  bool buzzerIsPlaying = false;




// Display Type
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

//Encoder
  //depending on your encoder - try 1,2 or 4 to get expected behaviour
  #define ROTARY_ENCODER_STEPS 4
  #define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
  //instead of changing here, rather change numbers above
  AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

// Bluetooth start
  BLEServer* pServer = NULL;
  BLECharacteristic* pTxCharacteristic = NULL;
  BLECharacteristic* pRxCharacteristic = NULL;
  // String bleAddress = "C0:42:3D:01:28:34"; // CONFIGURATION: < Use the real device BLE address here.
  String bleAddress = "FF:FF:FF:FF:FF:FF"; // CONFIGURATION: < Use the real device BLE address here.
  bool deviceConnected = false;
  bool oldDeviceConnected = false;
  uint32_t value = 0;
  int bt_rotation;
  int bt_vibration;
  int bt_vibration1;
  int bt_vibration2;
  int bt_airlevel;
  #define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
  #define CHARACTERISTIC_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
  #define CHARACTERISTIC_TX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
  // CONFIGURATION:                           ^ Replace X and Y with values that suit you.
  class MyServerCallbacks: public BLEServerCallbacks {
      void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        BLEDevice::startAdvertising();
      };

      void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
      }
  };
  class MySerialCallbacks: public BLECharacteristicCallbacks {
      void onWrite(BLECharacteristic *pCharacteristic) {
        static uint8_t messageBuf[64];
        assert(pCharacteristic == pRxCharacteristic);
        std::string rxValue = pRxCharacteristic->getValue();
        debugln(("rxValue: " + rxValue).c_str()); // debugging
        
        // Uncomment for full serial output
        // if (rxValue.length() > 0) {
        //   debugln("*********");
        //   debug("Received Value: ");
        //   for (int i = 0; i < rxValue.length(); i++)
        //     debug(rxValue[i]);

        //   debugln();
        //   debugln("*********");
        // }
        if (rxValue == "DeviceType;") {
          // debugln("$Responding to Device Enquiry");
          memmove(messageBuf, "J:40:C0423D012834;", 18);
          // memmove(messageBuf, "EI:40:C0FFFFFFFFFF;", 18);
          // CONFIGURATION:               ^ Use a BLE address of the Lovense device you're cloning.
          pTxCharacteristic->setValue(messageBuf, 18);
          pTxCharacteristic->notify();
        } else if (rxValue == "Battery;") {
          memmove(messageBuf, "90;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue == "PowerOff;") {
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue == "RotateChange;") {
                  memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Status:", 0) == 0) {
          memmove(messageBuf, "2;", 2);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Vibrate:", 0) == 0) {
          bt_vibration1 = std::atoi(rxValue.substr(8).c_str());
          bt_vibration2 = std::atoi(rxValue.substr(8).c_str());
          debug("V:");
          debugln(bt_vibration);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Rotate:", 0) == 0) {
          bt_rotation = std::atoi(rxValue.substr(7).c_str());
          debug("R:");
          debugln(bt_rotation);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
          } else if (rxValue.rfind("Vibrate:", 0) == 0) {
          bt_vibration1 = std::atoi(rxValue.substr(8).c_str());
          debug("V:");
          debugln(bt_vibration1);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Vibrate1:", 0) == 0) {
          bt_vibration1 = std::atoi(rxValue.substr(9).c_str());
          debug("V1:");
          debugln(bt_vibration1);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Vibrate2:", 0) == 0) {
          bt_vibration2 = std::atoi(rxValue.substr(9).c_str());
          debug("V2:");
          debugln(bt_vibration2);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Air:Level:", 0) == 0) {
          bt_airlevel = std::atoi(rxValue.substr(10).c_str());
          debug("AL:");
          debugln(bt_airlevel);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else {
          // debugln("$Unknown request");        
          memmove(messageBuf, "ERR;", 4);
          pTxCharacteristic->setValue(messageBuf, 4);
          pTxCharacteristic->notify();
        }
      }
  };
  //bluetooth end


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
    u8g2.clearBuffer();
    u8g2.setFont(font_status_messages);
    u8g2.drawStr(8, 20, "Connecting");
    u8g2.drawStr(40, 45, "WiFi");
    u8g2.sendBuffer();
    // while (WiFi.status() != WL_CONNECTED) {
    //   Serial.print('.');
    //   delay(1000);
    // }
    WiFi_Enabled = true;
    Serial.println(WiFi.localIP());
  }


// Bluetooth/WiFi Switching start
  void turn_OFF_WIFI() {
      Serial.println("WIFI OFF");
      WiFi.mode( WIFI_MODE_NULL );
      WiFi_Enabled = false;
      delay(1000);
    }
  void turn_ON_Bluetooth() {
  if (BT_Enabled == false)
    {
      reset_Outputs();
    }
    // Bluetooth
    // Create the BLE Device
  debugln("ble init");  
  BLEDevice::init("LVS-Z001"); // CONFIGURATION: The name doesn't actually matter, The app identifies it by the reported id.
  // Create the BLE Server
  debugln("create ble server");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  debugln("create ble service");
  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  debugln("create ble characteristics");
    // Create a BLE Characteristics
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_TX_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_RX_UUID,
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pRxCharacteristic->setCallbacks(new MySerialCallbacks());
    // Create the BLE Service
  // Start the service
  debugln("start the service bt pService");
  pService->start();
  debugln("bt start advertising");
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  debugln("Waiting a client connection to notify...");
  BT_Enabled = true;
}
  void turn_OFF_Bluetooth() {
  reset_Outputs();
  disable_Outputs();
  u8g2.clearBuffer();
  u8g2.setFont(font_status_messages);
  u8g2.drawStr(15, 20, "Disabling");
  u8g2.drawStr(15, 45, "Bluetooth");
  u8g2.sendBuffer();
  BLEDevice::deinit(false);
  BT_Enabled = false;
  delay(1000);
}
//Bluetooth/WiFi Switching end

// Timer
  void blinktext();
  void buzzing();
  TickTwo timer1(blinktext, 400); // flash the display text every second

// Send Slider values
  void notifyClients(String sliderValues) {
    ws.textAll(sliderValues);
  }

// Encoder Functions
  void rotary_onButtonClick()
      {
        lastTimePressed = 0;
        //ignore multiple press in that time milliseconds
        if (millis() - lastTimePressed < 200)
        {
          return;
        }
        else if ( (millis() - lastTimePressed > 200) && (buttonDownCount == 0) ){
          buttonPressed = true;
          debugln("button clicked");
        }
        lastTimePressed = millis();
        buttonLongPressed = false; 
        buttonDownCount = 0;
      }
  void rotary_onButtonDown()
      {
        // buttonLongPressed = true;
        buttonDownCount++;
        if (buttonDownCount >= 2) {
          buttonLongPressed = true;
          debugln("button long press");
        }
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
        if (rotaryEncoder.isEncoderButtonDown())
        {
          rotary_onButtonDown();
        }

      }
  void IRAM_ATTR readEncoderISR()
      {
        rotaryEncoder.readEncoder_ISR();
      }

// display blink text
  void blinktext()
  {
    drawcolorstate = !drawcolorstate;
  }  


// display the main intro menu
  void displayMainMenu()
  {
    item_selected = encoderPosition;
    if (current_screen == 0) {
      rotaryEncoder.setBoundaries(0, 3, true);

    // WiFi Status Symbol
    wlanstatus = WiFi.status();
    if (wlanstatus == 3) { 
      u8g2.setFont(font_wifi_symbol);
      u8g2.drawGlyph(110, 8, 72);	// WiFi Symbol
      }
    else { 
      u8g2.setFont(font_check_symbol);
      u8g2.drawGlyph(110, 8, 68);	// Sync X Symbol
      }

      u8g2.setFont(font_main_menu);
      u8g2.drawStr(25, 15, MainMenuItems[item_sel_previous]); 
      u8g2.drawStr(25, 35, MainMenuItems[item_selected]);
      u8g2.drawStr(25, 55, MainMenuItems[item_sel_next]);  
      u8g2.drawFrame(6,22,112,18);
    }
    else if (current_screen == 10) {
      displayMenuManual();
      buttonMenuManual();
      if (buttonLongPressed == true) {      
        current_screen = 0;
        item_selected = 0;
        encoderPosition = 0;
        rotaryEncoder.setEncoderValue(encoderPosition);
      }
    }
    else if (current_screen == 11) {
      rotaryEncoder.setBoundaries(1, 1, false);
      u8g2.setFont(font_main_menu);
      u8g2.drawStr(30, 10, "Local IP");
      u8g2.setCursor(8, 25);
      u8g2.print(WiFi.localIP());
      u8g2.drawStr(24, 50, "RSSI: ");
      u8g2.setCursor(70, 50);
      u8g2.print(WiFi.RSSI());

      if (buttonLongPressed == true) {      
        current_screen = 0;
        item_selected = 1;
        encoderPosition = 1;
        rotaryEncoder.setEncoderValue(encoderPosition);
      }
    }

    else if (current_screen == 12) {
      if (deviceConnected == 0){
        u8g2.setFont(font_status_messages);
        u8g2.clearBuffer();
        u8g2.drawStr(25, 16, "Waiting");
        u8g2.drawStr(30, 36, "for BT");
        u8g2.drawStr(8, 56, "Connection");
        u8g2.sendBuffer();
        }
      if (deviceConnected == 1) {
        displayBluetoothMenu();
        buttonMenuBluetooth();
        }
      if (buttonLongPressed == true) {      
        current_screen = 0;
        item_selected = 2;
        encoderPosition = 2;
        rotaryEncoder.setEncoderValue(encoderPosition);
        }
    }

    else if (current_screen == 13) {
      rotaryEncoder.setBoundaries(3, 3, false);
      u8g2.setFont(font_main_menu);
      u8g2.drawStr(30, 17, "Software"); 
      u8g2.drawStr(31, 32, "Version");
      u8g2.setCursor(45, 47);
      u8g2.print(version);  
      if (buttonLongPressed == true) {      
        current_screen = 0;
        item_selected = 3;
        encoderPosition = 3;
        rotaryEncoder.setEncoderValue(encoderPosition);
      }
    }
    // u8g2.sendBuffer();
  }

// actions on short and long button presses
  void menuButtonAction(){
  if ( (buttonPressed == true) && (current_screen == 0) ) {
    //start switch case
    switch (item_selected) {
      case 0:
      // Manual
      current_screen = 10;
      manualMenuSelect = 1;
      encoderPosition = manualMenuSelect;
      rotaryEncoder.setEncoderValue(encoderPosition);
      break;

      case 1:
      // WiFi Status
      current_screen = 11;
      displayMainMenu();
      break;

      case 2:
      // Bluetooth
      current_screen = 12;
      manualMenuSelect = 1;      
      encoderPosition = manualMenuSelect;
      rotaryEncoder.setEncoderValue(encoderPosition);
      break;

      case 3:
      // Info
      current_screen = 13;
      displayMainMenu();
      break;

      default:
      break;
    } // end switch case
  buttonPressed = false;
  } // end button pressed

  else if (buttonLongPressed == true) {
    //
  }
}

// menu manual display
  void displayMenuManual()
  {
  u8g2.setFont(font_manual_menu);
  u8g2.drawStr(1,8,"Ch1");
  u8g2.drawStr(34,8,"Ch2");
  u8g2.drawStr(69,8,"Ch3");
  u8g2.drawStr(102,8,"Ch4");
  u8g2.drawHLine(0,11,128);
  u8g2.drawVLine(30,0,64);
  u8g2.drawVLine(61,0,64);
  u8g2.drawVLine(95,0,64);
  // 1st line
  u8g2.setCursor(1,25);
  u8g2.print(Ch1_Enable ? "ON" : "OFF");
  u8g2.setCursor(36,25);
  u8g2.print(Ch2_Enable ? "ON" : "OFF");
  u8g2.setCursor(69,25);
  u8g2.print(Ch3_Enable ? "ON" : "OFF");
  u8g2.setCursor(102,25);
  u8g2.print(Ch4_Enable ? "ON" : "OFF");

  // 2nd line  
  u8g2.setCursor(1,37);
  u8g2.print(Ch1_On);
  u8g2.setCursor(36,37);
  u8g2.print(Ch2_On);
  u8g2.setCursor(69,37);
  u8g2.print(Ch3_On);
  u8g2.setCursor(102,37);
  u8g2.print(Ch4_On);
  // 3rd line
  u8g2.setCursor(1, 49);
  u8g2.print(Ch1_Off);
  u8g2.setCursor(36,49);
  u8g2.print(Ch2_Off);
  u8g2.setCursor(69,49);
  u8g2.print(Ch3_Off);
  u8g2.setCursor(102,49);
  u8g2.print(Ch4_Off);
  // 4th line
  u8g2.setCursor(1, 61);
  u8g2.print(Ch1_PWM);
  u8g2.setCursor(36,61);
  u8g2.print(Ch2_PWM);
  u8g2.setCursor(69,61);
  u8g2.print(Ch3_PWM);
  u8g2.setCursor(102,61);
  u8g2.print(Ch4_PWM);
  }
// menu System controls
  void buttonMenuManual() {
  switch (manualMenuSelect) {

    case 1: //
      rotaryEncoder.setBoundaries(1, 4, false);
      manualMenuSelect = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(1,8,"Ch1");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(Ch1_Enable);
          encoderPosition = Ch1_Enable;
          manualMenuSelect = manualMenuSelect * 10;
          }
      break;

      case 2: //
      manualMenuSelect = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(34,8,"Ch2");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(Ch2_Enable);
          encoderPosition = Ch2_Enable;
          manualMenuSelect = manualMenuSelect * 10;
          }
      break;

    case 3: //
      rotaryEncoder.setBoundaries(1, 4, false); 
      manualMenuSelect = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(69,8,"Ch3");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(Ch3_Enable);
          encoderPosition = Ch3_Enable;
          manualMenuSelect = manualMenuSelect * 10;
          }
      break;
    
    case 4: //
      rotaryEncoder.setBoundaries(1, 4, false); 
      manualMenuSelect = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(102,8,"Ch4");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(Ch4_Enable);
          encoderPosition = Ch4_Enable;
          manualMenuSelect = manualMenuSelect * 10;
          }
      break;

      case 10: // 
      Ch1_Enable = encoderPosition;
      u8g2.setCursor(1,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_Enable ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["toggle_a"] = Ch1_Enable;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 100, false);
          rotaryEncoder.setEncoderValue(Ch1_On);
          encoderPosition = Ch1_On;
          manualMenuSelect++;
          }
      break;

    case 11: // 
      rotaryEncoder.setBoundaries(0, 100, false);
      Ch1_On = encoderPosition;
      u8g2.setCursor(1,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_On);  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_a"] = Ch1_On;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch1_Off);
          encoderPosition = Ch1_Off;
          manualMenuSelect++;
          }
      break;

    case 12: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch1_Off = encoderPosition;
      u8g2.setCursor(1,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_b"] = Ch1_Off;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch1_PWM);
          encoderPosition = Ch1_PWM;
          manualMenuSelect++;
          }
      break;

    case 13: // 
      rotaryEncoder.setBoundaries(0, 100, false);
      Ch1_PWM = encoderPosition;
      u8g2.setCursor(1,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch1_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_c"] = Ch1_PWM;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(1);
          encoderPosition = 1;
          manualMenuSelect = 1;
          }
      break;

    case 20: //
      rotaryEncoder.setBoundaries(0, 1, false);
      Ch2_Enable = encoderPosition;
      u8g2.setCursor(36,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_Enable ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["toggle_b"] = Ch2_Enable;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 100, false);
          rotaryEncoder.setEncoderValue(Ch2_On);
          encoderPosition = Ch2_On;
          manualMenuSelect++;
          }
      break;

    case 21: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch2_On = encoderPosition;
      u8g2.setCursor(36,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_On);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_d"] = Ch2_On;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch2_Off);
          encoderPosition = Ch2_Off;
          manualMenuSelect++;
          }
      break;

    case 22: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch2_Off = encoderPosition;
      u8g2.setCursor(36,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_e"] = Ch2_Off;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch2_PWM);
          encoderPosition = Ch2_PWM;
          manualMenuSelect++;
          }
      break;

    case 23: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch2_PWM = encoderPosition;
      u8g2.setCursor(36,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch2_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_f"] = Ch2_PWM;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(2);
          encoderPosition = 2;
          manualMenuSelect = 2;
          }
      break;
    
    case 30: //
      rotaryEncoder.setBoundaries(0, 1, false);
      Ch3_Enable = encoderPosition;
      u8g2.setCursor(69,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_Enable ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["toggle_c"] = Ch3_Enable;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 100, false);
          rotaryEncoder.setEncoderValue(Ch3_On);
          encoderPosition = Ch3_On;
          manualMenuSelect++;
          }
      break;

    case 31: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch3_On = encoderPosition;
      u8g2.setCursor(69,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_On);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_g"] = Ch3_On;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch3_Off);
          encoderPosition = Ch3_Off;
          manualMenuSelect++;
          }
      break;

    case 32: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch3_Off = encoderPosition;
      u8g2.setCursor(69,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_h"] = Ch3_Off;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch3_PWM);
          encoderPosition = Ch3_PWM;        
          manualMenuSelect++;
          }
      break;

    case 33: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch3_PWM = encoderPosition;
      u8g2.setCursor(69,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch3_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_i"] = Ch3_PWM;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(3);
          encoderPosition = 3;
          manualMenuSelect = 3;
          }
      break;

      case 40: //
      rotaryEncoder.setBoundaries(0, 1, false);
      Ch4_Enable = encoderPosition;
      u8g2.setCursor(102,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_Enable ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["toggle_d"] = Ch4_Enable;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 100, false);
          rotaryEncoder.setEncoderValue(Ch4_On);
          encoderPosition = Ch4_On;
          manualMenuSelect++;
          }
      break;


      case 41: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch4_On = encoderPosition;
      u8g2.setCursor(102,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_On);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_j"] = Ch4_On;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch4_Off);
          encoderPosition = Ch4_Off;
          manualMenuSelect++;
          }
      break;

    case 42: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch4_Off = encoderPosition;
      u8g2.setCursor(102,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_Off);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          values["slider_k"] = Ch4_Off;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(Ch4_PWM);          
          encoderPosition = Ch4_PWM;         
          manualMenuSelect++;
          }
      break;

    case 43: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      Ch4_PWM = encoderPosition;
      u8g2.setCursor(102,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(Ch4_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {  
          values["slider_l"] = Ch4_PWM;
          update_values_ws();
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(4);
          encoderPosition = 4;
          manualMenuSelect = 4;
          }
      break;

    default:
      // Tue etwas, im Defaultfall
      // Dieser Fall ist optional
      break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
}


// Bluetooth
// Bluetooth Menu
void displayBluetoothMenu(){
  item_selected = 0;
  u8g2.setFont(font_bluetooth_menu);
  u8g2.drawStr(6,12,"BT");
  u8g2.drawStr(1,30,"Map");
  u8g2.drawStr(1,44,"Min");
  u8g2.drawStr(1,58,"Max");
  u8g2.drawHLine(0,15,128);
  u8g2.drawVLine(34,0,64);
  u8g2.drawVLine(80,0,64);
  // V1
  u8g2.drawStr(50, 11, "V1");
  u8g2.drawStr(40, 30, OutputItems[BT_V1_Output]);
  u8g2.setCursor(48, 44);
  u8g2.print(BT_V1_Min_PWM);
  u8g2.setCursor(48, 58);
  u8g2.print(BT_V1_Max_PWM);
  // V2
  u8g2.drawStr(95, 10, "V2");
  u8g2.drawStr(86, 30, OutputItems[BT_V2_Output]);
  u8g2.setCursor(93,44);
  u8g2.print(BT_V2_Min_PWM);
  u8g2.setCursor(93,58);
  u8g2.print(BT_V2_Max_PWM);
}
// Bluetooth Menu Controls
  void buttonMenuBluetooth() {
  switch (bluetoothMenuSelect) {

    case 1: //
      rotaryEncoder.setBoundaries(1, 2, false);
      bluetoothMenuSelect = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(50, 11,"V1");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, (OutputNumItems - 1), false);
          rotaryEncoder.setEncoderValue(BT_V1_Output);
          encoderPosition = BT_V1_Output;
          bluetoothMenuSelect = bluetoothMenuSelect * 10;
          }
      break;

      case 2: //
      bluetoothMenuSelect = encoderPosition;
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(95,10,"V2");
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, (OutputNumItems - 1), false);
          rotaryEncoder.setEncoderValue(BT_V2_Output);
          encoderPosition = BT_V2_Output;
          bluetoothMenuSelect = bluetoothMenuSelect * 10;
          }
      break;

      case 10: // 
      BT_V1_Output = encoderPosition;
      if (BT_V1_Output == 6) {
        BT_V1_Max_PWM = 100;
      }
      u8g2.setCursor(40,30);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(40, 30, OutputItems[BT_V1_Output]);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          if (BT_V1_Output != 6){
          rotaryEncoder.setBoundaries(0, 255, false);
          }
            else 
            {rotaryEncoder.setBoundaries(0, 100, false); }
          rotaryEncoder.setEncoderValue(BT_V1_Min_PWM);
          encoderPosition = BT_V1_Min_PWM;
          bluetoothMenuSelect++;
          }
      break;

    case 11: // 
      BT_V1_Min_PWM = encoderPosition;
      u8g2.setCursor(48,44);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(BT_V1_Min_PWM); 
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          if (BT_V1_Output != 6){
		        rotaryEncoder.setBoundaries(0, 255, false);
            }
            else 
            {
            rotaryEncoder.setBoundaries(0, 100, false); 
            }
          rotaryEncoder.setEncoderValue(BT_V1_Max_PWM);
          encoderPosition = BT_V1_Max_PWM;
          bluetoothMenuSelect++;
          }
      break;

    case 12: // 
      BT_V1_Max_PWM = encoderPosition;
      u8g2.setCursor(48,58);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(BT_V1_Max_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(1);
          encoderPosition = 1;
          bluetoothMenuSelect = 1;
          }
      break;

    case 20: //
      BT_V2_Output = encoderPosition;
      if (BT_V2_Output == 6) {
        BT_V2_Max_PWM = 100;
      }
      u8g2.setCursor(86,30);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(86, 30, OutputItems[BT_V2_Output]);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 255, false);
          rotaryEncoder.setEncoderValue(BT_V2_Min_PWM);
          encoderPosition = BT_V2_Min_PWM;
          bluetoothMenuSelect++;
          }
      break;

    case 21: // 
      BT_V2_Min_PWM = encoderPosition;
      u8g2.setCursor(93,44);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(BT_V2_Min_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
		  rotaryEncoder.setBoundaries(0, 255, false);
          rotaryEncoder.setEncoderValue(BT_V2_Max_PWM);
          encoderPosition = BT_V2_Max_PWM;
          bluetoothMenuSelect++;
          }
      break;

    case 22: // 
      BT_V2_Max_PWM = encoderPosition;
      u8g2.setCursor(93,58);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(BT_V2_Max_PWM);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(2);
          encoderPosition = 2;
          bluetoothMenuSelect = 2;
          }
      break;
    
      default:
      bluetoothMenuSelect = 1;
      break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
  }
}

// handle websocket message
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

      case 't':
        switch(message[7])
        {
          case 'a':
          if (message[9] == 't')//true
            {
            Ch1_Enable = true;
            values["toggle_a"] = Ch1_Enable;
            }
          else if (message[9] == 'f')//false
            {
            Ch1_Enable = false;
            values["toggle_a"] = Ch1_Enable;
            } 
          break;

          case 'b':
          if (message[9] == 't')//true
            {
            Ch2_Enable = true;
            values["toggle_b"] = Ch2_Enable;
            }
          else if (message[9] == 'f')//false
            {
            Ch2_Enable = false;
            values["toggle_b"] = Ch2_Enable;
            } 
          break;

          case 'c':
          if (message[9] == 't')//true
            {
            Ch3_Enable = true;
            values["toggle_c"] = Ch3_Enable;
            }
          else if (message[9] == 'f')//false
            {
            Ch3_Enable = false;
            values["toggle_c"] = Ch3_Enable;
            } 
          break;

          case 'd':
          if (message[9] == 't')//true
            {
            Ch4_Enable = true;
            values["toggle_d"] = Ch4_Enable;
            }
          else if (message[9] == 'f')//false
            {
            Ch4_Enable = false;
            values["toggle_d"] = Ch4_Enable;
            } 
          break;          

          case 'e':
          if (message[9] == 't')//true
            {
            Pump_Enable = true;
            values["toggle_e"] = Pump_Enable;
            }
          else if (message[9] == 'f')//false
            {
            Pump_Enable = false;
            values["toggle_e"] = Pump_Enable;
            } 
          break; 

          case 'f':
          if (message[9] == 't')//true
            {
            Collar_Enable = true;
            values["toggle_f"] = Collar_Enable;
            }
          else if (message[9] == 'f')//false
            {
            Collar_Enable = false;
            values["toggle_f"] = Collar_Enable;
            } 
          break; 

          case 'g':
          if (message[9] == 't')//true
            {
            buzzer_Metronome_Enabled = true;
            values["toggle_g"] = buzzer_Metronome_Enabled;
            }
          else if (message[9] == 'f')//false
            {
            buzzer_Metronome_Enabled = false;
            values["toggle_g"] = buzzer_Metronome_Enabled;
            } 
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

          case 'm':
            pump_PWM = slider;
            values["slider_m"] = pump_PWM;
            break;
          case 'n':
            collar_strength = slider;
            values["slider_n"] = collar_strength;
            break;
          case 'o':
            buzzerBPM = slider;
            values["slider_o"] = buzzerBPM;
            break;
          case 'p':
            buzzerVolume = slider;
            values["slider_p"] = buzzerVolume;
            break;

        }
        break;

      case 'b': //buzzer
        if (message[8] == 'n')//on
        {
          buzzer_Metronome_Enabled = true;
        }
        else if (message[8] == 'f') //off
        {
          buzzer_Metronome_Enabled = false;
        }
        values["buzzer"] = buzzer_Metronome_Enabled ? "on" : "off";
        debugln("buzzer output");
        debugln(values["buzzer"]);
        break;

      case 'c': // click button
        switch (message[6])
        {
          case 'b': // collar beep
          if (Collar_Enable == true) {
          dg.sendCollar(CollarChannel::CH1, CollarMode::Beep, collar_strength);
          debugln("collar beeped");
          }
          break;

        case 'v':  // collar vib
          if (Collar_Enable == true) {
          dg.sendCollar(CollarChannel::CH1, CollarMode::Vibe, collar_strength);
          debug("collar vibrates at level: ");
          debugln(collar_strength);
          }
          break;

        case 's': // collar shock
          if (Collar_Enable == true) {
          dg.sendCollar(CollarChannel::CH1, CollarMode::Shock, collar_strength);
          debug("collar shocks at level: ");
          debugln(collar_strength);
          }
          break;
          }
        break;

      // case 'l'://L bluetooth ch1

      //   switch (message[2])
      //   {
      //     case '1': //lb1 
      //       debugln("lb case 1 triggered");
      //       if (message[6] == 'f')//off
      //       {
      //         lb1_mode = "off";
      //         debugln("bluetooth ch1 off");
      //       }
      //       else if (message[6] == '1')
      //       {
      //         lb1_mode = "ch1";
      //         debugln("BT Ch1 -> Output 1 Enabled");
      //       }
      //       else if (message[6] == '2')
      //       {
      //         lb1_mode = "ch2";
      //         debugln("BT Ch1 -> Output 2 Enabled");
      //       }
      //       else if (message[6] == '3')
      //       {
      //         lb1_mode = "ch3";
      //         debugln("BT Ch1 -> Output 3 Enabled");
      //       }
      //       else if (message[6] == '4')
      //       {
      //         lb1_mode = "ch4";
      //         debugln("BT Ch1 -> Output 4 Enabled");
      //       }
      //       else if (message[6] == '5')
      //       {
      //         lb1_mode = "ch5";
      //         debugln("BT Ch1 -> Pump Enabled");
      //       }
      //       else
      //       {
      //         debugln("unknown lb1");
      //       }
      //       values["lb1"] = lb1_mode;
      //       debugln("Bluetooth Ch1 output");
      //       debugln(values["lb1"]);
      //       break;

      //     case '2': //lb2 
      //     debugln("lb case 2 triggered");
      //       if (message[6] == 'f')//off
      //       {
      //         lb2_mode = "off";
      //         debugln("BT Ch2 off");
      //       }
      //       else if (message[6] == '1')
      //       {
      //         lb2_mode = "ch1";
      //         debugln("BT Ch2 -> Output 1 Enabled");
      //       }
      //       else if (message[6] == '2')
      //       {
      //         lb2_mode = "ch2";
      //         debugln("BT Ch2 -> Output 2 Enabled");
      //       }
      //       else if (message[6] == '3')
      //       {
      //         lb2_mode = "ch3";
      //         debugln("BT Ch2 -> Output 3 Enabled");
      //       }
      //       else if (message[6] == '4')
      //       {
      //         lb2_mode = "ch4";
      //         debugln("BT Ch2 -> Output 4 Enabled");
      //       }
      //       else if (message[6] == '5')
      //       {
      //         lb2_mode = "ch5";
      //         debugln("BT Ch2 -> Pump Enabled");
      //       }
      //       else
      //       {
      //         debugln("unknown lb2");
      //       }
      //       values["lb2"] = lb2_mode;
      //       debugln("Bluetooth Ch2 output");
      //       debugln(values["lb2"]);
      //       break;
      //     } // switch message[2] end

    } // switch message[0] end

    json_string = JSON.stringify(values);
    ws.textAll(json_string);
  }
} // handleWebSocketMessage_ws end

// on websocket event
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

// update websocket values
  void update_values_ws(){
    json_string = JSON.stringify(values);
    debugln(json_string);
    ws.textAll(json_string);
}

// initialize Websocket
  void init_ws() {
  ws.onEvent(onEvent_ws);
  server.addHandler(&ws);
}

// disable outputs 
  void disable_Outputs()
{
  if (!Ch1_Enable) {
    pwm1_paused = false;
    ledcWrite(PWMOUT_1, 0);
  }
  if (!Ch2_Enable) {
    pwm2_paused = false;
    ledcWrite(PWMOUT_2, 0);
  }
  if (!Ch3_Enable) {
    pwm3_paused = false;
    ledcWrite(PWMOUT_3, 0);
  }
 if (!Ch4_Enable) {
    pwm4_paused = false;
    ledcWrite(PWMOUT_4,0);
 }
  if (!Pump_Enable){
    ledcWrite(pumpOUT, 0);
    Pump_Enable = false;
  }
}

// reset outputs
  void reset_Outputs(){
  Ch1_Enable = false;
  Ch2_Enable = false;
  Ch3_Enable = false;
  Ch4_Enable = false;
  Pump_Enable = false;
  Ch1_On = 0;
  Ch1_Off = 0;
  Ch1_PWM = 0;
  Ch2_On = 0;
  Ch2_Off = 0;
  Ch2_PWM = 0;
  Ch3_On = 0;
  Ch3_Off = 0;
  Ch3_PWM = 0;
  Ch4_On = 0;
  Ch4_Off = 0;
  Ch4_PWM = 0;
  pump_PWM = 0;
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
  values["slider_m"] = 0; // pump
  values["slider_n"] = 0; // collar strength
  values["slider_o"] = 60; // Buzzer Metronome BPM
  values["slider_p"] = 5; // Buzzer Metronome Volume
  values["toggle_a"] = false;
  values["toggle_b"] = false;
  values["toggle_c"] = false;
  values["toggle_d"] = false;
  values["toggle_e"] = false; // pump
  values["toggle_f"] = false; // collar
  values["toggle_g"] = false; // Buzzer Metronome
  // values["buzzer"] = "off";
  // values["lb1"] = "off";
  // values["lb2"] = "off";
}

// control pwm outputs in web or manual mode
  void PWM_Output(){
  // Output 1
  if ((pwm1_paused == false) && (Ch1_Enable == true))
  {
     int mapped_Ch1_PWM;
     mapped_Ch1_PWM = map(Ch1_PWM, 0, 100, 0, 255);
     ledcWrite(PWMOUT_1, mapped_Ch1_PWM);
     if ((Ch1_Off > 0) && (millis() - pwm1_timeStarted >= Ch1_On * 1000)) {
      pwm1_paused = true;
      pwm1_timeStopped = millis();
    }
  }  
  else if ((pwm1_paused == true) && (Ch1_Enable == true))
  {
    ledcWrite(PWMOUT_1, 0);
    if (millis() - pwm1_timeStopped >= Ch1_Off * 1000)
    {
      pwm1_paused = false;
      pwm1_timeStarted = millis();
    }
  }
  // Output 2
  if ((pwm2_paused == false) && (Ch2_Enable == true))
  {
     int mapped_Ch2_PWM;
     mapped_Ch2_PWM = map(Ch2_PWM, 0, 100, 0, 255);
     ledcWrite(PWMOUT_2, mapped_Ch2_PWM);
     if ((Ch2_Off > 0) && (millis() - pwm2_timeStarted >= Ch2_On * 1000)) {
      pwm2_paused = true;
      pwm2_timeStopped = millis();
    }
  }  
  else if ((pwm2_paused == true) && (Ch2_Enable == true))
  {
    ledcWrite(PWMOUT_2, 0);
    if (millis() - pwm2_timeStopped >= Ch2_Off * 1000)
    {
      pwm2_paused = false;
      pwm2_timeStarted = millis();
    }
  }
  // Output 3
  if ((pwm3_paused == false) && (Ch3_Enable == true))
  {
     int mapped_Ch3_PWM;
     mapped_Ch3_PWM = map(Ch3_PWM, 0, 100, 0, 255);
     ledcWrite(PWMOUT_3, mapped_Ch3_PWM);
     if ((Ch3_Off > 0) && (millis() - pwm4_timeStarted >= Ch3_On * 1000)) {
      pwm3_paused = true;
      pwm3_timeStopped = millis();
    }
  }  
  else if ((pwm3_paused == true) && (Ch3_Enable == true))
  {
    ledcWrite(PWMOUT_3, 0);
    if (millis() - pwm3_timeStopped >= Ch3_Off * 1000)
    {
      pwm3_paused = false;
      pwm3_timeStarted = millis();
    }
  }
  // Output 4
  if ((pwm4_paused == false) && (Ch4_Enable == true))
  {
     int mapped_Ch4_PWM;
     mapped_Ch4_PWM = map(Ch4_PWM, 0, 100, 0, 255);
     ledcWrite(PWMOUT_4, mapped_Ch4_PWM);
     if ((Ch4_Off > 0) && (millis() - pwm4_timeStarted >= Ch4_On * 1000)) {
      pwm4_paused = true;
      pwm4_timeStopped = millis();
    }
  }  
  else if ((pwm4_paused == true) && (Ch4_Enable == true))
  {
    ledcWrite(PWMOUT_4, 0);
    if (millis() - pwm4_timeStopped >= Ch4_Off * 1000)
    {
      pwm4_paused = false;
      pwm4_timeStarted = millis();
    }
  }
  // Pump Output 5
  if (Pump_Enable == true)
    {
      int mapped_pump_PWM;
      mapped_pump_PWM = map(pump_PWM, 0, 100, 0, 255);
      ledcWrite(pumpOUT, mapped_pump_PWM);
    }
  else {
    ledcWrite(pumpOUT, 0);
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

      if (collar_bt_only_changes == true) {
        if (mapped_PWM != previous_shock){
          dg.sendCollar(CollarChannel::CH1, CollarMode::Shock, mapped_PWM);
        }
      previous_shock = mapped_PWM;
      }

      else {
        dg.sendCollar(CollarChannel::CH1, CollarMode::Shock, mapped_PWM);
      }

      break;
  }
}

void buzzer_Metronome (int buzzerBPM, int buzzerOnTimeMS, int buzzerVolume) {
    beatInterval = 60000 / buzzerBPM;
    int buzzerPWM = map(buzzerVolume, 0, 10, 0, 140);
    if ((currentMillis - buzzerPreviousMillis >= beatInterval - buzzerOnTimeMS) && (!buzzerIsPlaying)) { // turn on
        ledcWrite (buzzer, buzzerPWM);
        buzzerIsPlaying = true;
        buzzerPreviousMillis = currentMillis;
  } else { // turn off
    if (currentMillis - buzzerPreviousMillis >= buzzerOnTimeMS) {
        ledcWrite (buzzer, 0);
        buzzerIsPlaying = false;
        buzzerPreviousMillis = currentMillis;
        delay(beatInterval - buzzerOnTimeMS);
    }
  }
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
  pinMode(pumpPin, OUTPUT);
  digitalWrite(RF_433, LOW);

  // define PWM
  ledcSetup(PWMOUT_1, freq, resolution);
  ledcSetup(PWMOUT_2, freq, resolution);
  ledcSetup(PWMOUT_3, freq, resolution);
  ledcSetup(PWMOUT_4, freq, resolution);
  ledcSetup(buzzer, buzzerFrequency, resolution);
  ledcSetup(pumpOUT, 500, resolution);
  ledcAttachPin(CH1_30VMax, PWMOUT_1);
  ledcAttachPin(CH2_30VMax, PWMOUT_2);
  ledcAttachPin(CH1_5V, PWMOUT_3);
  ledcAttachPin(CH2_5V, PWMOUT_4);
  ledcAttachPin(buzzerPin, buzzer);
  ledcAttachPin(pumpPin, pumpOUT);

  //Encoder
  //we must initialize rotary encoder
	rotaryEncoder.begin();
	rotaryEncoder.setup(readEncoderISR);
	//set boundaries and if values should cycle or not
	//in this example we will set possible values between 0 and 60;

	rotaryEncoder.setBoundaries(0, 3, false); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
  /*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
	//rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
	rotaryEncoder.setAcceleration(0); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
  rotaryEncoder.setEncoderValue(1);

  //Display
  timer1.start(); // timer for blinking text
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.clearDisplay();
  u8g2.setFontMode(1);
  u8g2.setFont(font_status_messages);
  u8g2.drawStr(15,20,"Universal");
  u8g2.drawStr(13,45,"Controller");
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
  values["slider_m"] = 0; // pump
  values["slider_n"] = 0; // collar strength
  values["slider_o"] = 60; // Buzzer Metronome BPM
  values["slider_p"] = 5; // Buzzer Metronome Volume
  values["toggle_a"] = false;
  values["toggle_b"] = false;
  values["toggle_c"] = false;
  values["toggle_d"] = false;
  values["toggle_e"] = false; // pump
  values["toggle_f"] = false; // collar
  values["toggle_g"] = false; // Buzzer Metronome
  values["buzzer"] = "off";
  // values["lb1"] = "off";
  // values["lb2"] = "off";

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
  currentMillis = millis();
  ws.cleanupClients();
  timer1.update(); // display blinking text timer

  // controls pwm outputs if system isn't in bluetooth mode / sub menu
  if (BT_Enabled == false) {
  PWM_Output();
  }

  // disable Outputs
  disable_Outputs();

  // Encoder
  rotary_loop();

  // encoder control in main menu - probably put in function
  if (encoderPosition != item_selected) 
    {
    item_selected = encoderPosition;
    // set correct values for the previous and next items
    item_sel_previous = item_selected - 1;
    if (item_sel_previous < 0) {item_sel_previous = MainMenuNumItems - 1;} // previous item would be below first = make it the last
    item_sel_next = item_selected + 1;  
    if (item_sel_next >= MainMenuNumItems) {item_sel_next = 0;} // next item would be after last = make it the first
    // Update the main menu
    if (current_screen == 0) 
      {
      displayMainMenu();
      }
    }

  // Encoder Acceleration
  if (current_screen == 0) {
    rotaryEncoder.setAcceleration(0);
  }
  else if (current_screen == 10 || current_screen == 12) {
    rotaryEncoder.setAcceleration(50);
  }
  
  u8g2.clearBuffer();
  menuButtonAction();
  displayMainMenu();

  u8g2.sendBuffer();
// switch from wifi to bluetooth
  if ((current_screen == 12) && (WiFi_Enabled == true)){
    turn_OFF_WIFI();
    WiFi_Enabled = false;
    debugln("disabling WiFi");
    delay(2000);
    debugln("trying to start bluetooth again");
        turn_ON_Bluetooth();
  }
// switch from bluetooth to wifi
  if ((WiFi_Enabled == false) && (current_screen != 12)) {
    turn_OFF_Bluetooth();
    debugln("enabling WiFi");
    initWiFi();
    reset_Outputs();
    update_values_ws();
    }

  // Bluetooth start
  // Bluetooth connection status
  if (!deviceConnected && oldDeviceConnected && WiFi_Enabled == false) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        debugln("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
  if (deviceConnected && !oldDeviceConnected && WiFi_Enabled == false) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
  // Bluetooth end

  //Bluetooth Output Control
  if (BT_Enabled == true) {
    int BT_mapped_PWM[2];
    BT_mapped_PWM[0] = map(bt_vibration1, 1, 20, BT_V1_Min_PWM, BT_V1_Max_PWM);
    BT_mapped_PWM[1] = map(bt_vibration2, 1, 20, BT_V2_Min_PWM, BT_V2_Max_PWM);

    Ch1_Enable = (BT_V1_Output == 1 || BT_V2_Output == 1);
    Ch2_Enable = (BT_V1_Output == 2 || BT_V2_Output == 2);
    Ch3_Enable = (BT_V1_Output == 3 || BT_V2_Output == 3);
    Ch4_Enable = (BT_V1_Output == 4 || BT_V2_Output == 4);
    Pump_Enable = (BT_V1_Output == 5 || BT_V2_Output == 5);
    Collar_Enable = (BT_V1_Output == 6 || BT_V2_Output == 6);

    if ((BT_V1_Output > 0) && (bt_vibration1 > 0)) {
      BT_V1_Paused = false;
      bluetooth_write_pwm(BT_V1_Output, BT_mapped_PWM[0]);
    }
    else if ((BT_V1_Output > 0) && (bt_vibration1 == 0) && (BT_V1_Paused == false)){
      BT_V1_Paused = true;
      bluetooth_write_pwm(BT_V1_Output, 0);
    }
    if ((BT_V2_Output > 0) && (bt_vibration2 > 0)) {
      BT_V2_Paused = false;
      bluetooth_write_pwm(BT_V2_Output, BT_mapped_PWM[1]);
    }
    else if ((BT_V2_Output > 0) && (bt_vibration2 == 0) && (BT_V2_Paused == false)) {
      BT_V2_Paused = true;
      bluetooth_write_pwm(BT_V2_Output, 0);
    }

  }

  // Keep collar awake if enabled
  if ((currentMillis - previous_Collar_Wakeup >= keep_Collar_Awake_Interval) && (Collar_Enable == true)) {
    debugln("keeping collar awake");
    previous_Collar_Wakeup = millis();
    dg.sendCollar(CollarChannel::CH1, CollarMode::Blink, 100);
  }

// buzzer start
  if (buzzer_Metronome_Enabled == true) {
  buzzer_Metronome(buzzerBPM, buzzerOnTimeMS, buzzerVolume);
  }
  else if (buzzer_Metronome_Enabled == false) {
    ledcWrite(buzzer, 0);
  }
// buzzer end

} // Loop end

