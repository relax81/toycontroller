// Updated DolgCollar3 by Cerb
//  fixed the channel to 4 bit, strenght to 8 bit and key to 16 bit  (was previously 3, 7 and 17)
//  removed the key conversion to make it easier for people that obtain their remote key through SDR / Logic Analyerz etc
//
//    FILE: DogCollar2.h
//  AUTHOR: Flash89y
// VERSION: 0.1
// PURPOSE: replacing Remote of dog collar training device using 
//          3-pin 433 MHz sender module (5V/GND/DATA) with an arduino
//          this is the second version (DogCollar2) with advantages:
//     URL: https://github.com/flash89y/Arduino/tree/master/libraries/DogCollar2
//  THANKS: thank you smouldery for making a nice memory friendly
//          implementation I could combine with previous "DogCollar"
//          link: https://github.com/smouldery/shock-collar-control/
//

#ifndef _DOGCOLLAR_h_
#define _DOGCOLLAR_h_

#if ARDUINO < 100
    #include <WProgram.h>
#else
    #include <Arduino.h>
#endif

enum class CollarMode
{
  Shock,
  Vibe,
  Beep,
  Blink
};

enum class CollarChannel
{
  CH1,
  CH2,
  BOTH
};

class DogCollar
{   
public:
    DogCollar(int transmitPin, String uniqueKey, int repeatedSending = 3);
    ~DogCollar();
    void sendCollar(CollarChannel ch, CollarMode mode, uint8_t str);
    bool keepAlive();
private:

    int repeatNumber;
    int pin;
    
    // timings for keep-alive messages
    uint64_t lastMillis;
    uint64_t currentMillis;
    const uint64_t millisDifferenceAllowed;

    // key code things of the remote the collars are asigned to
    String key; // 'static' key - each remote has it's own
    bool keyIsOK;
    const int lenKey=16;

    // timings for sending magiccode
    // for default type
    const uint16_t delayStart;
    const uint16_t delayLong;
    const uint16_t delayShort;
    const uint16_t delayBetween;
    // for new type 
    const uint16_t HighStart;
    const uint16_t HighTransmit;
    const uint16_t LowShort;
    const uint16_t LowLong;

    // strings to put together the magiccode
    String magiccode;
    String strength;
    String finalstrength;
    String inverted;
    String channelPartOne;
    String channelPartTwo;
    String modePartOne;
    String modePartTwo;

    // private functions for internal usage
    void fillSequences(CollarChannel ch, CollarMode mode, uint8_t str);
    void transmitCode();
};

#endif // _DOGCOLLAR_h_
