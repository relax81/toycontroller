// activate deactivate heap logging (free / min free / largest free block)
#define DEBUG_HEAP 1
#if DEBUG_HEAP == 1
extern volatile unsigned long loop_max_us; // longest loop() pass of the last full 5 s window
#define heap_log(tag) Serial.printf("[heap] %-12s free=%u min=%u maxblock=%u loopmax=%lu us\n", tag, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), loop_max_us)
#else
#define heap_log(tag)
#endif

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <TickTwo.h>
#include <AiEsp32RotaryEncoder.h>
#include "config.h"
#include "wifi_setup.h"
#include "wifi_manager.h"
#include "state.h"
#include "outputs.h"
#include "settings.h"
#include "toy_models.h"
#include "protocol.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <NimBLEDevice.h>
#include "driver/ledc.h"

#ifndef GIT_HASH
#define GIT_HASH "unknown" // set by git_hash.py
#endif

// software version
String version = "0.1";

void displayMenuManual();
void buttonMenuManual();
void displayBluetoothMenu();
void buttonMenuBluetooth();
void update_values_ws();

// Random - name later
  unsigned long currentMillis;
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
// Bluetooth Menu
  // String lb1_mode;
  // String lb2_mode;
  String tempString;
// Main Menu New
  char MainMenuItems [MainMenuNumItems] [MainMenuMaxItemLength] = {"Manual","WiFi Status","Bluetooth","Info","Settings"};
// Bluetooth Menu
  char OutputItems [OutputNumItems] [OutputItemsMaxLength] = {"OFF","PWM1","PWM2","PWM3","PWM4","PUMP","Shoc"};
// buzzer 




// Display Type
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

//Encoder
  //instead of changing here, rather change numbers above
  AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

// Bluetooth start
  NimBLEServer* pServer = NULL;
  NimBLECharacteristic* pTxCharacteristic = NULL;
  NimBLECharacteristic* pRxCharacteristic = NULL;
  // String bleAddress = "C0:42:3D:01:28:34"; // CONFIGURATION: < Use the real device BLE address here.
  String bleAddress = "FF:FF:FF:FF:FF:FF"; // CONFIGURATION: < Use the real device BLE address here.
  uint32_t value = 0;
  // UUIDs, name and DeviceType answer come from the selected toy model (toy_models.cpp).
  // Naming: RX = the characteristic the app writes to, TX = the one the toy notifies on.
  // CONFIGURATION:                           ^ Replace X and Y with values that suit you.
  // BLE callbacks run in the NimBLE task and must not write the state: they queue events.
  // bleVib[] is the last vibration pair queued by this task (both values travel in one
  // event, so V1/V2 are always applied together).
  // V2 is the second vibration channel (Dolce, Edge) or, for Nora, the Rotate value
  // (toy_models.h, field "extra"); for the other models it stays 0.
  static int bleVib[2] = {0, 0};
  static const ToyModel& bleToy() { return TOY_MODELS[state.ble.toyModel]; }
  static void ble_queue_vib(int v1, int v2) {
    bleVib[0] = v1;
    bleVib[1] = v2;
    state_set(EV_BLE_VIB, 0, (int32_t)((uint32_t)(v1 & 0xFFFF) | ((uint32_t)(v2 & 0xFFFF) << 16)));
  }
  class MyServerCallbacks: public NimBLEServerCallbacks {
      void onConnect(NimBLEServer* pServer) {
        state_set(EV_BLE_CONN, 0, 1);
        NimBLEDevice::startAdvertising();
      };

      void onDisconnect(NimBLEServer* pServer) {
        // failsafe: no client, no output (EV_BLE_CONN 0 also zeroes vib[])
        bleVib[0] = 0;
        bleVib[1] = 0;
        state_set(EV_BLE_CONN, 0, 0);
      }
  };
  class MySerialCallbacks: public NimBLECharacteristicCallbacks {
      void onWrite(NimBLECharacteristic *pCharacteristic) {
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
          // "<letter>:<firmware>:<BLE address>;" of the selected toy model
          const ToyModel& tm = TOY_MODELS[state.ble.toyModel];
          String resp = String(tm.letter) + ":" + tm.fw + ":C0423D012834;";
          // CONFIGURATION:                       ^ Use a BLE address of the Lovense device you're cloning.
          pTxCharacteristic->setValue((uint8_t*)resp.c_str(), resp.length());
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
          pTxCharacteristic->setValue(messageBuf, 2);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Vibrate:", 0) == 0) {
          int v = std::atoi(rxValue.substr(8).c_str());
          ble_queue_vib(v, bleToy().vibChannels >= 2 ? v : bleVib[1]); // "Vibrate:" drives both vibration channels
          debug("V:");
          debugln(v);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Rotate:", 0) == 0) {
          int r = std::atoi(rxValue.substr(7).c_str());
          state_set(EV_BLE_ROT, 0, r);
          if (bleToy().extra == 'R') ble_queue_vib(bleVib[0], r < 0 ? 0 : (r > 20 ? 20 : r)); // Nora: Rotate -> V2
          debug("R:");
          debugln(r);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Vibrate1:", 0) == 0) {
          int v = std::atoi(rxValue.substr(9).c_str());
          ble_queue_vib(v, bleVib[1]);
          debug("V1:");
          debugln(v);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Vibrate2:", 0) == 0) {
          int v = std::atoi(rxValue.substr(9).c_str());
          if (bleToy().vibChannels >= 2) ble_queue_vib(bleVib[0], v); // one-channel models ignore Vibrate2
          debug("V2:");
          debugln(v);
          memmove(messageBuf, "OK;", 3);
          pTxCharacteristic->setValue(messageBuf, 3);
          pTxCharacteristic->notify();
        } else if (rxValue.rfind("Air:Level:", 0) == 0) {
          int a = std::atoi(rxValue.substr(10).c_str());
          state_set(EV_BLE_AIR, 0, a);
          debug("AL:");
          debugln(a);
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

// WebSocket failsafe: web-controlled outputs go to 0 when the last WS client
// is gone or nothing (command or ping answer) was heard for the timeout.
// BLE-held outputs are not touched. Only armed after a WS command, so local
// manual control without any web client is not affected.
  #define WS_FAILSAFE 1
  const unsigned long WS_PING_INTERVAL_MS = 5000; // browsers answer ping frames automatically
  // At least 3 pings per failsafe timeout, so an idle but healthy tab never runs into it (with a
  // fixed 5 s a timeout below that expired between two pongs). Read from state on every pass, a
  // runtime change of sys.failsafe counts at once.
  static unsigned long ws_ping_interval_ms() {
    unsigned long third = (unsigned long)state.failsafeTimeoutS * 1000UL / 3;
    return third < WS_PING_INTERVAL_MS ? third : WS_PING_INTERVAL_MS;
  }
  volatile unsigned long ws_last_seen = 0;
  volatile bool ws_failsafe_armed = false;
  volatile bool ws_broadcast_req = false; // set by the WS handler, evaluated in loop()
  unsigned long ws_last_ping = 0;
  //Json Variable to Hold Slider Values
  JSONVar values;
  String json_string;

// initFS(), initWiFi() and initWebServerRoot() live in wifi_setup.cpp


// Bluetooth start (runs in parallel to WiFi)
  void turn_ON_Bluetooth() {
    // Bluetooth
    // Create the BLE Device
  debugln("ble init");  
  const ToyModel& toy = TOY_MODELS[state.ble.toyModel];
  Serial.printf("[ble] toy model %s (%s, type letter %s)\n", toy.label, toy.bleName, toy.letter);
  NimBLEDevice::init(toy.bleName); // The name doesn't actually matter, the app identifies the toy by the reported id.
  // Create the BLE Server
  debugln("create ble server");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  debugln("create ble service");
  // Create the BLE Service
  NimBLEService *pService = pServer->createService(toy.svc);
  debugln("create ble characteristics");
    // Create a BLE Characteristics
  pTxCharacteristic = pService->createCharacteristic(
                      toy.rx,
                      NIMBLE_PROPERTY::NOTIFY
                    );

  pRxCharacteristic = pService->createCharacteristic(
                      toy.tx,
                      NIMBLE_PROPERTY::WRITE  |
                      NIMBLE_PROPERTY::WRITE_NR
                    );
  pRxCharacteristic->setCallbacks(new MySerialCallbacks());
    // Create the BLE Service
  // Start the service
  debugln("start the service bt pService");
  pService->start();
  debugln("bt start advertising");
  // Start advertising
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(toy.svc);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  NimBLEDevice::startAdvertising();
  debugln("Waiting a client connection to notify...");
  BT_Enabled = true;
  heap_log("ble init");
}
//Bluetooth start end

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
    // debugln(item_selected);
    // debugln(encoderPosition);
    if (current_screen == 0) {
      rotaryEncoder.setBoundaries(0, 4, false);

    // WiFi Status Symbol
    if (wifi_connected()) { 
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
      if (wifi_connected()) {
        u8g2.drawStr(30, 10, "Local IP");
        u8g2.setCursor(8, 25);
        u8g2.print(WiFi.localIP());
        u8g2.drawStr(24, 50, "RSSI: ");
        u8g2.setCursor(70, 50);
        u8g2.print(WiFi.RSSI());
        u8g2.setFont(font_manual_menu);
        u8g2.drawStr(8, 62, "toycontroller.local");
      }
      else {
        u8g2.drawStr(46, 10, "WiFi");
        u8g2.drawStr(8, 32, wifi_state_text());
        if (wifi_state() == WifiState::Portal) {
          u8g2.drawStr(8, 52, "Click: data");
          if (buttonPressed == true) {
            buttonPressed = false;
            current_screen = 15;
          }
        }
      }

      if (buttonLongPressed == true) {      
        current_screen = 0;
        item_selected = 1;
        encoderPosition = 1;
        rotaryEncoder.setEncoderValue(encoderPosition);
      }
    }

    else if (current_screen == 12) {
      if (state.ble.in.connected == 0){
        u8g2.setFont(font_status_messages);
        u8g2.clearBuffer();
        u8g2.drawStr(25, 16, "Waiting");
        u8g2.drawStr(30, 36, "for BT");
        u8g2.drawStr(8, 56, "Connection");
        u8g2.sendBuffer();
        }
      if (state.ble.in.connected == 1) {
        displayBluetoothMenu();
        buttonMenuBluetooth();
        }
      if (buttonLongPressed == true) {      
        settings_flush();
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

    else if (current_screen == 15) {
      rotaryEncoder.setBoundaries(1, 1, false);
      wifi_draw_portal_screen();
      if (!wifi_portal_active() || buttonPressed == true || buttonLongPressed == true) { // one click leaves
        buttonPressed = false;
        current_screen = 0;
        item_selected = 1;
        encoderPosition = 1;
        rotaryEncoder.setEncoderValue(encoderPosition);
      }
    }
    else if (current_screen == 14) {
      rotaryEncoder.setBoundaries(0, 1, false);
      u8g2.setFont(font_main_menu);
      u8g2.drawStr(2, 10, "Shock BT trigger"); 
      u8g2.drawStr(10, 24, "only on Level");
      u8g2.drawStr(28, 38, "change");
      if (state.collar.btOnlyChanges == true) {
        u8g2.drawStr(28,55, "ENABLED");
      }
      else {
        u8g2.drawStr(28,55, "DISABLED");
      }

      if (encoderPosition == 0) {
        state_set(EV_COLLAR_BTONLY, 0, 0);
        }
        else if (encoderPosition == 1) {
          state_set(EV_COLLAR_BTONLY, 0, 1);
        }

      if (buttonLongPressed == true) {      
        settings_flush();
        current_screen = 0;
        item_selected = 4;
        encoderPosition = 4;
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

      case 4:
      // Settings
      current_screen = 14;
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
  u8g2.print(state.out[0].enabled ? "ON" : "OFF");
  u8g2.setCursor(36,25);
  u8g2.print(state.out[1].enabled ? "ON" : "OFF");
  u8g2.setCursor(69,25);
  u8g2.print(state.out[2].enabled ? "ON" : "OFF");
  u8g2.setCursor(102,25);
  u8g2.print(state.out[3].enabled ? "ON" : "OFF");

  // 2nd line  
  u8g2.setCursor(1,37);
  u8g2.print(state.out[0].on / 10.0, 1);
  u8g2.setCursor(36,37);
  u8g2.print(state.out[1].on / 10.0, 1);
  u8g2.setCursor(69,37);
  u8g2.print(state.out[2].on / 10.0, 1);
  u8g2.setCursor(102,37);
  u8g2.print(state.out[3].on / 10.0, 1);
  // 3rd line
  u8g2.setCursor(1, 49);
  u8g2.print(state.out[0].off / 10.0, 1);
  u8g2.setCursor(36,49);
  u8g2.print(state.out[1].off / 10.0, 1);
  u8g2.setCursor(69,49);
  u8g2.print(state.out[2].off / 10.0, 1);
  u8g2.setCursor(102,49);
  u8g2.print(state.out[3].off / 10.0, 1);
  // 4th line
  u8g2.setCursor(1, 61);
  u8g2.print(state.out[0].pwm);
  u8g2.setCursor(36,61);
  u8g2.print(state.out[1].pwm);
  u8g2.setCursor(69,61);
  u8g2.print(state.out[2].pwm);
  u8g2.setCursor(102,61);
  u8g2.print(state.out[3].pwm);
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
          rotaryEncoder.setEncoderValue(state.out[0].enabled);
          encoderPosition = state.out[0].enabled;
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
          rotaryEncoder.setEncoderValue(state.out[1].enabled);
          encoderPosition = state.out[1].enabled;
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
          rotaryEncoder.setEncoderValue(state.out[2].enabled);
          encoderPosition = state.out[2].enabled;
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
          rotaryEncoder.setEncoderValue(state.out[3].enabled);
          encoderPosition = state.out[3].enabled;
          manualMenuSelect = manualMenuSelect * 10;
          }
      break;

      case 10: // 
      state.out[0].enabled = encoderPosition;
      u8g2.setCursor(1,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[0].enabled ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 90, false);
          rotaryEncoder.setEncoderValue(state.out[0].on / 10);
          encoderPosition = state.out[0].on / 10;
          manualMenuSelect++;
          }
      break;

    case 11: // 
      rotaryEncoder.setBoundaries(0, 90, false);
      state.out[0].on = encoderPosition * 10;
      u8g2.setCursor(1,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[0].on / 10.0, 1);  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[0].off / 10);
          encoderPosition = state.out[0].off / 10;
          manualMenuSelect++;
          }
      break;

    case 12: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[0].off = encoderPosition * 10;
      u8g2.setCursor(1,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[0].off / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[0].pwm);
          encoderPosition = state.out[0].pwm;
          manualMenuSelect++;
          }
      break;

    case 13: // 
      rotaryEncoder.setBoundaries(0, 100, false);
      state.out[0].pwm = encoderPosition;
      u8g2.setCursor(1,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[0].pwm);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(1);
          encoderPosition = 1;
          manualMenuSelect = 1;
          }
      break;

    case 20: //
      rotaryEncoder.setBoundaries(0, 1, false);
      state.out[1].enabled = encoderPosition;
      u8g2.setCursor(36,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[1].enabled ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 90, false);
          rotaryEncoder.setEncoderValue(state.out[1].on / 10);
          encoderPosition = state.out[1].on / 10;
          manualMenuSelect++;
          }
      break;

    case 21: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[1].on = encoderPosition * 10;
      u8g2.setCursor(36,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[1].on / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[1].off / 10);
          encoderPosition = state.out[1].off / 10;
          manualMenuSelect++;
          }
      break;

    case 22: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[1].off = encoderPosition * 10;
      u8g2.setCursor(36,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[1].off / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[1].pwm);
          encoderPosition = state.out[1].pwm;
          manualMenuSelect++;
          }
      break;

    case 23: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      state.out[1].pwm = encoderPosition;
      u8g2.setCursor(36,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[1].pwm);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(2);
          encoderPosition = 2;
          manualMenuSelect = 2;
          }
      break;
    
    case 30: //
      rotaryEncoder.setBoundaries(0, 1, false);
      state.out[2].enabled = encoderPosition;
      u8g2.setCursor(69,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[2].enabled ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 90, false);
          rotaryEncoder.setEncoderValue(state.out[2].on / 10);
          encoderPosition = state.out[2].on / 10;
          manualMenuSelect++;
          }
      break;

    case 31: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[2].on = encoderPosition * 10;
      u8g2.setCursor(69,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[2].on / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[2].off / 10);
          encoderPosition = state.out[2].off / 10;
          manualMenuSelect++;
          }
      break;

    case 32: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[2].off = encoderPosition * 10;
      u8g2.setCursor(69,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[2].off / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[2].pwm);
          encoderPosition = state.out[2].pwm;        
          manualMenuSelect++;
          }
      break;

    case 33: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      state.out[2].pwm = encoderPosition;
      u8g2.setCursor(69,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[2].pwm);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(3);
          encoderPosition = 3;
          manualMenuSelect = 3;
          }
      break;

      case 40: //
      rotaryEncoder.setBoundaries(0, 1, false);
      state.out[3].enabled = encoderPosition;
      u8g2.setCursor(102,25);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[3].enabled ? "ON" : "OFF");  
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 90, false);
          rotaryEncoder.setEncoderValue(state.out[3].on / 10);
          encoderPosition = state.out[3].on / 10;
          manualMenuSelect++;
          }
      break;


      case 41: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[3].on = encoderPosition * 10;
      u8g2.setCursor(102,37);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[3].on / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[3].off / 10);
          encoderPosition = state.out[3].off / 10;
          manualMenuSelect++;
          }
      break;

    case 42: // 
      rotaryEncoder.setBoundaries(0, 90, false); 
      state.out[3].off = encoderPosition * 10;
      u8g2.setCursor(102,49);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[3].off / 10.0, 1);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          state_ui_dirty = true; // the loop sends the state to the web clients
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(state.out[3].pwm);          
          encoderPosition = state.out[3].pwm;         
          manualMenuSelect++;
          }
      break;

    case 43: // 
      rotaryEncoder.setBoundaries(0, 100, false); 
      state.out[3].pwm = encoderPosition;
      u8g2.setCursor(102,61);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.out[3].pwm);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {  
          state_ui_dirty = true; // the loop sends the state to the web clients
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
  u8g2.drawStr(40, 30, OutputItems[state.ble.map[0].output]);
  u8g2.setCursor(48, 44);
  u8g2.print(state.ble.map[0].minPwm);
  u8g2.setCursor(48, 58);
  u8g2.print(state.ble.map[0].maxPwm);
  // V2
  u8g2.drawStr(95, 10, "V2");
  u8g2.drawStr(86, 30, OutputItems[state.ble.map[1].output]);
  u8g2.setCursor(93,44);
  u8g2.print(state.ble.map[1].minPwm);
  u8g2.setCursor(93,58);
  u8g2.print(state.ble.map[1].maxPwm);
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
          rotaryEncoder.setEncoderValue(state.ble.map[0].output);
          encoderPosition = state.ble.map[0].output;
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
          rotaryEncoder.setEncoderValue(state.ble.map[1].output);
          encoderPosition = state.ble.map[1].output;
          bluetoothMenuSelect = bluetoothMenuSelect * 10;
          }
      break;

      case 10: // 
      state_set(EV_BT_OUT, 0, encoderPosition); // limits (collar <= 100) are applied by the state
      u8g2.setCursor(40,30);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(40, 30, OutputItems[state.ble.map[0].output]);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          if (state.ble.map[0].output != 6){
          rotaryEncoder.setBoundaries(0, 255, false);
          }
            else 
            {rotaryEncoder.setBoundaries(0, 100, false); }
          rotaryEncoder.setEncoderValue(state.ble.map[0].minPwm);
          encoderPosition = state.ble.map[0].minPwm;
          bluetoothMenuSelect++;
          }
      break;

    case 11: // 
      state_set(EV_BT_MIN, 0, encoderPosition);
      u8g2.setCursor(48,44);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.ble.map[0].minPwm); 
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          if (state.ble.map[0].output != 6){
		        rotaryEncoder.setBoundaries(0, 255, false);
            }
            else 
            {
            rotaryEncoder.setBoundaries(0, 100, false); 
            }
          rotaryEncoder.setEncoderValue(state.ble.map[0].maxPwm);
          encoderPosition = state.ble.map[0].maxPwm;
          bluetoothMenuSelect++;
          }
      break;

    case 12: // 
      state_set(EV_BT_MAX, 0, encoderPosition);
      u8g2.setCursor(48,58);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.ble.map[0].maxPwm);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setEncoderValue(1);
          encoderPosition = 1;
          bluetoothMenuSelect = 1;
          }
      break;

    case 20: //
      state_set(EV_BT_OUT, 1, encoderPosition); // limits (collar <= 100) are applied by the state
      u8g2.setCursor(86,30);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.drawStr(86, 30, OutputItems[state.ble.map[1].output]);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
          rotaryEncoder.setBoundaries(0, 255, false);
          rotaryEncoder.setEncoderValue(state.ble.map[1].minPwm);
          encoderPosition = state.ble.map[1].minPwm;
          bluetoothMenuSelect++;
          }
      break;

    case 21: // 
      state_set(EV_BT_MIN, 1, encoderPosition);
      u8g2.setCursor(93,44);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.ble.map[1].minPwm);
      u8g2.setDrawColor(1);
      if (buttonPressed == true) {
          buttonPressed = false;
		  rotaryEncoder.setBoundaries(0, 255, false);
          rotaryEncoder.setEncoderValue(state.ble.map[1].maxPwm);
          encoderPosition = state.ble.map[1].maxPwm;
          bluetoothMenuSelect++;
          }
      break;

    case 22: // 
      state_set(EV_BT_MAX, 1, encoderPosition);
      u8g2.setCursor(93,58);
      u8g2.setDrawColor(drawcolorstate);
      u8g2.print(state.ble.map[1].maxPwm);
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
  void handleWebSocketMessage_ws(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  int slider;
  char* message;

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {

    data[len] = 0;
    message = (char*)data;
    debugln(message);

    if (message[0] == '{') { // JSON protocol (protocol.cpp), everything else is the old "id?value" form
      protocol_handle(client, message, len);
      return;
    }

    switch (message[0])
    {

      case 't': // toggle_a .. toggle_g, message[9] = 't'rue / 'f'alse
        {
          static const char* const toggleKeys[7] = {"ch1.en", "ch2.en", "ch3.en", "ch4.en", "pump.en", "collar.en", "buzzer.en"};
          if (message[7] >= 'a' && message[7] <= 'g' && (message[9] == 't' || message[9] == 'f'))
          {
            protocol_legacy_set(toggleKeys[message[7] - 'a'], (message[9] == 't') ? 1 : 0);
          }
        }
        break;

      case 's': //slider
      debugln("slider triggered");
        {
          // slider_a .. slider_l: three sliders per channel (on, off, pwm), then pump, collar, buzzer
          static const char* const sliderKeys[16] = {
            "ch1.on", "ch1.off", "ch1.pwm", "ch2.on", "ch2.off", "ch2.pwm",
            "ch3.on", "ch3.off", "ch3.pwm", "ch4.on", "ch4.off", "ch4.pwm",
            "pump.pwm", "collar.strength", "buzzer.bpm", "buzzer.vol"};
          slider = atoi(message + 9);
          if (message[7] >= 'a' && message[7] <= 'p')
          {
            protocol_legacy_set(sliderKeys[message[7] - 'a'], slider);
          }
        }
        break;

      case 'b': //buzzer
        if (message[8] == 'n')//on
        {
          protocol_legacy_set("buzzer.en", 1);
        }
        else if (message[8] == 'f') //off
        {
          protocol_legacy_set("buzzer.en", 0);
        }
        debugln("buzzer output");
        break;

      case 'c': // click button
        switch (message[6])
        {
          case 'b': // collar beep
          if (state.collar.enabled == true) {
          collar_send(CollarMode::Beep, state.collar.strength);
          debugln("collar beeped");
          }
          break;

        case 'v':  // collar vib
          if (state.collar.enabled == true) {
          collar_send(CollarMode::Vibe, state.collar.strength);
          debug("collar vibrates at level: ");
          debugln(state.collar.strength);
          }
          break;

        case 's': // collar shock
          if (state.collar.enabled == true) {
          collar_send(CollarMode::Shock, state.collar.strength);
          debug("collar shocks at level: ");
          debugln(state.collar.strength);
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

    ws_broadcast_req = true; // loop() sends the state to all clients (also answers "getValues")
  }
} // handleWebSocketMessage_ws end

// on websocket event
  void onEvent_ws(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type)
  {
    case WS_EVT_CONNECT:
      ws_last_seen = millis();
      protocol_client_connected(client->id());
      //serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      protocol_client_gone(client->id());
      //serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      ws_last_seen = millis();
      ws_failsafe_armed = true;
      handleWebSocketMessage_ws(client, arg, data, len);
      break;
    case WS_EVT_PONG:
      ws_last_seen = millis();
      break;
    case WS_EVT_ERROR:
      break;
  }
}

// fill `values` (JSONVar, loop() only) from the state; keys as the web UI expects them
  static void fill_values_from_state() {
    static const char* const toggles[4] = {"toggle_a", "toggle_b", "toggle_c", "toggle_d"};
    static const char* const sliders[12] = {"slider_a", "slider_b", "slider_c", "slider_d", "slider_e", "slider_f",
                                            "slider_g", "slider_h", "slider_i", "slider_j", "slider_k", "slider_l"};
    for (int i = 0; i < 4; i++) {
      values[toggles[i]] = state.out[i].enabled;
      values[sliders[i * 3]] = state.out[i].on;
      values[sliders[i * 3 + 1]] = state.out[i].off;
      values[sliders[i * 3 + 2]] = state.out[i].pwm;
    }
    values["slider_m"] = state.pump.pwm;
    values["slider_n"] = state.collar.strength;
    values["slider_o"] = state.buzzer.bpm;
    values["slider_p"] = state.buzzer.volume;
    values["toggle_e"] = state.pump.enabled;
    values["toggle_f"] = state.collar.enabled;
    values["toggle_g"] = state.buzzer.enabled;
    values["buzzer"] = state.buzzer.enabled ? "on" : "off";
  }

// update websocket values
  void update_values_ws(){
    fill_values_from_state();
    json_string = JSON.stringify(values);
    debugln(json_string);
    protocol_send_legacy(ws, json_string); // old clients only, JSON clients get patches
}

// initialize Websocket
  void init_ws() {
  ws.onEvent(onEvent_ws);
  server.addHandler(&ws);
}

// websocket failsafe: switch off everything the web interface controls
  void ws_failsafe(){
  debugln("websocket failsafe: web outputs off");
  ws_failsafe_armed = false;
  state_set(EV_ALL_OFF, 0, 0); // loop() context: applied at once, marks the UI dirty
}

void setup() {
  state_init(); // event queue, remembers the loop task
  Serial.begin(115200);
  Serial.printf("[fw] %s %s %s\n", __DATE__, __TIME__, GIT_HASH);
  debugln("setup started");
  settings_load(); // persistent settings (NVS "cfg") into the state

  // Pins (outputs are set up in outputs_init())
  pinMode(PIR, INPUT);
  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  outputs_init();

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
  wifi_manager_init();
  init_ws();

  // Websocket stuff
  fill_values_from_state();
  json_string = JSON.stringify(values);

  // Web Server Root URL
  wifi_manager_attach(server);
  initWebServerRoot();

  // Start server
  server.begin();

  // Bluetooth (Lovense emulation) runs in parallel to WiFi
  turn_ON_Bluetooth();
  heap_log("setup");
}

// serial terminal commands (line based, non-blocking): "reboot" restarts the ESP
// as a replacement for the reset button, "wifi-reset" deletes the stored WiFi credentials
void serial_commands() {
  static char line[32];
  static size_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[len] = '\0';
      len = 0;
      if (strcmp(line, "reboot") == 0 || strcmp(line, "restart") == 0) {
        settings_flush();
        Serial.println("[cmd] restarting");
        Serial.flush();
        ESP.restart();
      }
      else if (strcmp(line, "wifi-reset") == 0) {
        settings_flush();
        wifi_forget();
        Serial.println("[cmd] wifi credentials deleted, restarting");
        Serial.flush();
        ESP.restart();
      }
      else if (strcmp(line, "cfg") == 0) {
        Serial.printf("[cfg] failsafe %d s, V1 out=%d %d-%d, V2 out=%d %d-%d, buzzer vol=%d bpm=%d, collar btOnlyChanges=%d (cfg failsafe <3-120>, cfg reset)\n",
                      state.failsafeTimeoutS, state.ble.map[0].output, state.ble.map[0].minPwm, state.ble.map[0].maxPwm,
                      state.ble.map[1].output, state.ble.map[1].minPwm, state.ble.map[1].maxPwm,
                      state.buzzer.volume, state.buzzer.bpm, state.collar.btOnlyChanges ? 1 : 0);
      }
      else if (strncmp(line, "cfg failsafe ", 13) == 0) {
        int sec = atoi(line + 13);
        if (sec >= 3 && sec <= 120) {
          state_set(EV_FAILSAFE_TO, 0, sec);
          Serial.printf("[cfg] failsafe timeout %d s (saved after 5 s)\n", sec);
        }
        else {
          Serial.println("[cfg] failsafe timeout must be 3-120 s");
        }
      }
      else if (strcmp(line, "cfg reset") == 0) {
        settings_reset();
        Serial.println("[cmd] settings deleted (wifi untouched), restarting");
        Serial.flush();
        ESP.restart();
      }
      else if (line[0] != '\0') {
        Serial.printf("[cmd] unknown command \"%s\" (available: reboot, wifi-reset, cfg, cfg failsafe <s>, cfg reset)\n", line);
      }
    }
    else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}

// Hold the encoder button for 3 s right after the boot (press within the first 3 s after
// setup() has finished):
// deletes the stored WiFi credentials and restarts, the portal opens then.
void boot_wifi_reset_gesture(unsigned long nowMs) {
  static bool finished = false;
  static bool holding = false;
  static unsigned long holdStart = 0;
  static unsigned long windowStart = 0;
  static bool started = false;
  if (finished) return;
  if (!started) { // setup() takes a while, the window starts with the first call
    started = true;
    windowStart = nowMs;
  }
  bool down = rotaryEncoder.isEncoderButtonDown();
  if (!holding) {
    if (nowMs - windowStart >= 3000) { // window over, the button was not pressed
      finished = true;
    }
    else if (down) {
      holding = true;
      holdStart = nowMs;
    }
  }
  else if (!down) {
    holding = false;     // released too early, the window may still be open
  }
  else if (nowMs - holdStart >= 3000) {
    finished = true;
    wifi_forget();
    Serial.println("[boot] wifi credentials deleted (encoder button), restarting");
    u8g2.clearBuffer();
    u8g2.setFont(font_status_messages);
    u8g2.drawStr(8, 20, "WiFi data");
    u8g2.drawStr(20, 45, "deleted");
    u8g2.sendBuffer();
    delay(1200);
    ESP.restart();
  }
}

volatile unsigned long loop_max_us = 0;

void loop() {
  const unsigned long loopStartUs = micros();
  state_drain(); // apply queued events first (loop() is the only writer of the state)
  currentMillis = millis();
#if DEBUG_LEDC == 1
  static bool ledcLogged = false;
  if (!ledcLogged && currentMillis >= 5000) { // once, so it shows up in an already open monitor
    ledcLogged = true;
    debug_ledc();
  }
#endif
#if DEBUG_HEAP == 1
  static unsigned long lastHeapLog = 0;
  if (currentMillis - lastHeapLog >= 10000) {
    lastHeapLog = currentMillis;
    heap_log("loop");
  }
#endif
  serial_commands();
  boot_wifi_reset_gesture(currentMillis);
  wifi_manager_update(currentMillis);
  if (wifi_portal_take_show_request() && current_screen == 0) {
    current_screen = 15; // show hotspot data once when the portal starts
  }
  ws.cleanupClients();
  timer1.update(); // display blinking text timer

#if WS_FAILSAFE == 1
  if (currentMillis - ws_last_ping >= ws_ping_interval_ms()) {
    ws_last_ping = currentMillis;
    ws.pingAll();
  }
  if (ws_failsafe_armed && (ws.count() == 0 || currentMillis - ws_last_seen >= (unsigned long)state.failsafeTimeoutS * 1000UL)) {
    ws_failsafe();
  }
#endif

  // state changes (web, BLE, device menu) go to the web clients, at most once per pass
  if (state_ui_dirty || ws_broadcast_req) {
    state_ui_dirty = false;
    ws_broadcast_req = false;
    if (ws.count() > 0 && protocol_v1_count() > 0) {
      update_values_ws();
    }
  }

  settings_update(currentMillis); // debounced NVS save of changed settings
  if (state.ble.toyRestartPending && currentMillis - state.ble.toyChangedMs >= 2000) {
    // new toy model: save it and restart, the BLE identity is only set in the BLE init
    settings_flush();
    Serial.println("[toy] model changed, restarting");
    delay(100);
    ESP.restart();
  }
  // last web client gone (browser closed): save pending changes at once
  static bool hadWsClient = false;
  bool hasWsClient = ws.count() > 0;
  if (hadWsClient && !hasWsClient) settings_flush();
  hadWsClient = hasWsClient;

  outputs_arbitrate();
  protocol_loop(ws); // patches / state for the JSON clients (after the arbitration: ble.hold.*)

  // controls pwm outputs (web / manual), skips outputs held by BLE
  PWM_Output();

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
  // Bluetooth start
  // Bluetooth connection status
  static bool bleAdvRestartPending = false;
  static unsigned long bleAdvRestartAt = 0;
  if (!state.ble.in.connected && state.ble.wasConnected) {
        // give the bluetooth stack the chance to get things ready (non-blocking)
        bleAdvRestartPending = true;
        bleAdvRestartAt = currentMillis + 500;
        state.ble.wasConnected = state.ble.in.connected;
    }
    // connecting
  if (state.ble.in.connected && !state.ble.wasConnected) {
        // do stuff here on connecting
        state.ble.wasConnected = state.ble.in.connected;
    }
  if (bleAdvRestartPending && (long)(currentMillis - bleAdvRestartAt) >= 0) {
        bleAdvRestartPending = false;
        pServer->startAdvertising(); // restart advertising
        debugln("start advertising");
    }
  // Bluetooth end

  //Bluetooth Output Control
  if (BT_Enabled == true) {
    int BT_mapped_PWM[2];
    BT_mapped_PWM[0] = map(state.ble.in.vib[0], 1, 20, state.ble.map[0].minPwm, state.ble.map[0].maxPwm);
    BT_mapped_PWM[1] = map(state.ble.in.vib[1], 1, 20, state.ble.map[1].minPwm, state.ble.map[1].maxPwm);

    if ((state.ble.map[0].output > 0) && (state.ble.in.vib[0] > 0)) {
      state.ble.map[0].paused = false;
      bluetooth_write_pwm(state.ble.map[0].output, BT_mapped_PWM[0]);
    }
    else if ((state.ble.map[0].output > 0) && (state.ble.in.vib[0] == 0) && (state.ble.map[0].paused == false)){
      state.ble.map[0].paused = true;
      bluetooth_write_pwm(state.ble.map[0].output, 0);
    }
    if ((state.ble.map[1].output > 0) && (state.ble.in.vib[1] > 0)) {
      state.ble.map[1].paused = false;
      bluetooth_write_pwm(state.ble.map[1].output, BT_mapped_PWM[1]);
    }
    else if ((state.ble.map[1].output > 0) && (state.ble.in.vib[1] == 0) && (state.ble.map[1].paused == false)) {
      state.ble.map[1].paused = true;
      bluetooth_write_pwm(state.ble.map[1].output, 0);
    }

  }

  // Keep collar awake if enabled
  if ((currentMillis - state.collar.lastWakeup >= state.collar.keepAwakeMs) && (state.collar.enabled == true || (state.ble.in.connected && state.ble.collarMapped))) {
    debugln("keeping collar awake");
    state.collar.lastWakeup = millis();
    collar_send(CollarMode::Blink, 100);
  }

// buzzer start
  if (state.buzzer.enabled == true) {
  buzzer_Metronome(currentMillis);
  }
  else if (state.buzzer.enabled == false) {
    ledcWrite(buzzer, 0);
  }
// buzzer end

  // longest pass of the last 5 s, reported in the [heap] line
  static unsigned long loopWindowStart = 0, loopWindowMax = 0;
  unsigned long loopUs = micros() - loopStartUs;
  if (loopUs > loopWindowMax) loopWindowMax = loopUs;
  if (currentMillis - loopWindowStart >= 5000) {
    loop_max_us = loopWindowMax;
    loopWindowMax = 0;
    loopWindowStart = currentMillis;
  }

} // Loop end

