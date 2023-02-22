// Updated DolgCollar3 by Cerb
//  fixed the channel to 4 bit, strenght to 8 bit and key to 16 bit  (was previously 3, 7 and 17)
//  removed the key conversion to make it easier for people that obtain their remote key through SDR / Logic Analyerz etc
//
//  timing for default type: delayStart, delayLong, delayShort
//  timing for type1: HighStart, HighTransmit, LowShort, LowLong

//    FILE: DogCollar2.cpp
//  AUTHOR: Flash89y
// VERSION: 0.1
// PURPOSE: replacing Remote of dog collar training device using 
//          3-pin 433 MHz sender module (5V/GND/DATA) with an arduino
//          this is the second version (DogCollar2) with advantages:
//     URL: https://github.com/flash89y/Arduino/tree/master/libraries/DogCollar2
//  THANKS: thank you smouldery for making a nice memory friendly
//          implementation I could combine with previous "DogCollar"
//          link: https://github.com/smouldery/shock-collar-control/


#include "DogCollar3.h"

// uncomment to try different collar/protocol type, especially if there is a FCC sticker on the back of the remote mentioning model pet998db
// default  much likely collar type PET998DR
// collartype1 much likely collar type PET998DB

// #define collartype1   



DogCollar::DogCollar(int transmitPin, String uniqueKey, int repeatedSending) : millisDifferenceAllowed(60000),
    key("1000000111101011"), delayStart(1600), delayLong(800), delayShort(300), delayBetween(2000), HighStart(790), HighTransmit(200), LowShort(820), LowLong(1620)
{
    lastMillis = 0;
    repeatNumber = repeatedSending;
    pin = transmitPin;
    pinMode(pin,OUTPUT);
    keyIsOK = false;
    Serial.begin(115200);

    if(uniqueKey.length() == lenKey)
    {
        key = uniqueKey;
        keyIsOK = true;
    }
}

DogCollar::~DogCollar()
{

}

void DogCollar::transmitCode()
{

    #ifdef collartype1
    // type1 start
    digitalWrite(pin, HIGH);
    delayMicroseconds(HighStart);
    digitalWrite(pin, LOW);
    delayMicroseconds(LowShort);

    for (int n = 0; n < magiccode.length() ; n++)
    {
        if (magiccode.charAt(n) == '1') // transmit a one
        {
            digitalWrite(pin, HIGH);
            delayMicroseconds(HighTransmit);
            digitalWrite(pin, LOW);
            delayMicroseconds(LowShort);
        }
        else // transmit a zero
        {
            digitalWrite(pin, HIGH);
            delayMicroseconds(HighTransmit);
            digitalWrite(pin, LOW);
            delayMicroseconds(LowLong);
        }
    }
    // type1 end    
    #else
    // default type start
    digitalWrite(pin, HIGH);
    delayMicroseconds(delayStart);
    digitalWrite(pin, LOW);
    delayMicroseconds(delayLong);

    for (int n = 0; n < magiccode.length() ; n++)
    {
        if (magiccode.charAt(n) == '1') // transmit a one
        {
            digitalWrite(pin, HIGH);
            delayMicroseconds(delayLong);
            digitalWrite(pin, LOW);
            delayMicroseconds(delayShort);
        }
        else // transmit a zero
        {
            digitalWrite(pin, HIGH);
            delayMicroseconds(delayShort);
            digitalWrite(pin, LOW);
            delayMicroseconds(delayLong);
        }
    }
    // default type end
    #endif

}

void DogCollar::sendCollar(CollarChannel ch, CollarMode mode, uint8_t str)
{
    if(keyIsOK)
    {
        // Serial.println(String(F("key to use: ")) + key);
    }
    else
    {
        Serial.println("invalid key entered, please check again");
        return;
    }
    
    if(ch == CollarChannel::BOTH)
    {
        for(int i=0; i<repeatNumber; i++)
        {
            fillSequences(CollarChannel::CH1,mode,str);
            delayMicroseconds(delayBetween);
            transmitCode();
            
            fillSequences(CollarChannel::CH2,mode,str);
            delayMicroseconds(delayBetween);
            transmitCode();
        }
    }   
    else
    {
        fillSequences(ch,mode,str);
        for(int i=0; i<repeatNumber; i++)
        {
            transmitCode();
            delayMicroseconds(delayBetween);
        }
    }
}

void DogCollar::fillSequences(CollarChannel ch, CollarMode mode, uint8_t str)
{   
    // for collartype1 start
    #ifdef collartype1
        // which channel
    if(ch == CollarChannel::CH2)
    {
        channelPartOne = "0000";
        channelPartTwo = "1111";
    }
    else // (ch == CollarChannel::CH1) or (ch == CollarChannel::BOTH)
    {
        channelPartOne = "0111";
        channelPartTwo = "0001";
    }
    
    // which mode
    if(mode == CollarMode::Beep)
    {
        modePartOne = "1011";
        modePartTwo = "0010";
    }
    else if(mode == CollarMode::Blink)
    {
        modePartOne = "0111";
        modePartTwo = "0001";
    }
    else if(mode == CollarMode::Shock)
    {
        modePartOne = "1110";
        modePartTwo = "1000";
    }
    else // (mode == CollarMode::Vibe)
    {
        modePartOne = "1101";
        modePartTwo = "0100";
    }

    // which strength
    strength = "";
    inverted = "";
    finalstrength = "";
    str = constrain(str,1,100);					// limit str between 1 and 100
    int ones = String(str, BIN).length();		// ones of string length
    for (int i = 0; i < 8 - ones; i++)
    {
        strength = strength + "1";
    }
    inverted = String(str, BIN);
    inverted.replace('0','2');
    inverted.replace('1','0');
    inverted.replace('2','1');
    finalstrength = strength + inverted;
    magiccode = channelPartOne + modePartOne + key + finalstrength + modePartTwo + channelPartTwo + "0";
    // for collartype1 end

    #else
    //for default collar type start
    // which channel
    if(ch == CollarChannel::CH2)
    {
        channelPartOne = "1111";
        channelPartTwo = "0000";
    }
    else // (ch == CollarChannel::CH1) or (ch == CollarChannel::BOTH)
    {
        channelPartOne = "1000";
        channelPartTwo = "1110";
    }
    
    // which mode
    if(mode == CollarMode::Beep)
    {
        modePartOne = "0100";
        modePartTwo = "1101";
    }
    else if(mode == CollarMode::Blink)
    {
        modePartOne = "1000";
        modePartTwo = "1110";
    }
    else if(mode == CollarMode::Shock)
    {
        modePartOne = "0001";
        modePartTwo = "0111";
    }
    else // (mode == CollarMode::Vibe)
    {
        modePartOne = "0010";
        modePartTwo = "1011";
    }

    // which strength
    strength = "";
    str = constrain(str,1,100);
    int zeros = String(str, BIN).length();
    for (int i = 0; i < 8 - zeros; i++)
    {
        strength = strength + "0";
    }
    strength = strength + String(str, BIN);
    magiccode = channelPartOne + modePartOne + key + strength + modePartTwo + channelPartTwo + "0";
    // default collar type end
    #endif

    // massive debug outputs ahead
    //~ Serial.println(String("channelPartOne = ") + channelPartOne);
    //~ Serial.println(String("modePartOne = ") + modePartOne);
    //~ Serial.println(String("key = ") + key);
    //~ Serial.println(String("strength = ") + strength);
    //~ Serial.println(String("modePartTwo = ") + modePartTwo);
    //~ Serial.println(String("channelPartTwo = ") + channelPartTwo);
    //~ Serial.println(String("magiccode = ") + magiccode);
    //~ Serial.println(String("magiccode.length() = ") + magiccode.length());
}

bool DogCollar::keepAlive()
{
    currentMillis = millis();
    if(currentMillis > (lastMillis + millisDifferenceAllowed))
    {
        lastMillis = millis();
        // let the led blink on both channels to prohibit deep sleep
        sendCollar(CollarChannel::BOTH, CollarMode::Blink, 100);
        return true;
    }
    return false;
}
