
/*
 Copyright (C) 2015 Daniel Perron
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 Read DS18B20 sensor using a nRF24L01 sensor base on J.Coliz code

*/



/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
//2014 - TMRh20 - Updated along with Optimized RF24 Library fork
 */

/**
 * Example for Getting Started with nRF24L01+ radios.
 *
 * This is an example of how to use the RF24 class to communicate on a basic level.  Write this sketch to two
 * different nodes.  Put one of the nodes into 'transmit' mode by connecting with the serial monitor and
 * sending a 'T'.  The ping node sends the current time to the pong node, which responds by sending the value
 * back.  The ping node can then see how long the whole cycle took.
 * Note: For a more efficient call-response scenario see the GettingStarted_CallResponse.ino example.
 * Note: When switching between sketches, the radio may need to be powered down to clear settings that are not "un-set" otherwise
 */


#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"


///////////////   radio /////////////////////
// Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(8, 7);

#define UNIT_ID 0xc7

const uint8_t MasterPipe[6] = {0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0};

const uint8_t SensorPipe[6]  = { UNIT_ID, 0xc2, 0xc2, 0xc2, 0xc2, 0};


// Radio pipe addresses for the 2 nodes to communicate.


// Set up roles to simplify testing
//boolean role;                                    // The main role variable, holds the current role identifier
//boolean role_ping_out = 1, role_pong_back = 0;   // The two different roles.
unsigned long Count = 0;


// because of sleep mode calibration problem
// and I don't want to change the library
// we will mimic the calibration
// by disable it and enable only when we reach 0 at modulus 720
// we will calibrate the watch dog  when we wait  for the DS18B20 conversion

void setLed(unsigned char channel, unsigned char value)
{
 switch(channel)
    {
       case 0 : pinMode(2,OUTPUT);
                digitalWrite(2, value ==0);
                break;
       case 1 : pinMode(3,OUTPUT);
                digitalWrite(3, value ==0);
                break;
       case 2 : pinMode(4,OUTPUT);
                digitalWrite(4, value ==0);
                break;
    }
} 
  

void StopRadio()
{
//  Serial.print("*** STOP RADIO ***\n");
//  delay(100);
//  pinMode(RF24_POWER_PIN, OUTPUT);
//  digitalWrite(RF24_POWER_PIN, LOW);
}


void StartRadio()
{
  Serial.print("*** START RADIO ***\n");

//  pinMode(RF24_POWER_PIN, OUTPUT);
//  digitalWrite(RF24_POWER_PIN, HIGH);
  delay(50);
  radio.begin();                          // Start up the radio
  radio.setPayloadSize(32);
  radio.setChannel(0x4e);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.setRetries(7, 3);  // Max delay between retries & number of retries
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.maskIRQ(true, true, false);
  //role = role_ping_out;                  // Become the primary transmitter (ping out)
  radio.openWritingPipe(MasterPipe);
  radio.openReadingPipe(1, SensorPipe);
  radio.startListening();                 // Start listening
}



void setup() {

  setLed(0,0);
  setLed(1,0);
  setLed(2,0);
  
  Serial.begin(57600);

  printf_begin();
  printf("\n\rRF24  OUTPUT\n\r");
  StartRadio();
  radio.stopListening();
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
  radio.startListening();

}


enum cycleMode {ModeInit, ModeListen};

cycleMode cycle = ModeInit;


#define STRUCT_TYPE_GETDATA    0
#define STRUCT_TYPE_INIT_DATA  1
#define STRUCT_TYPE_DHT22_DATA 2
#define STRUCT_TYPE_DS18B20_DATA 3
#define STRUCT_TYPE_MAX6675_DATA 4
#define STRUCT_TYPE_DIGITAL_OUT  5




typedef struct
{
  char header;
  unsigned char structSize;
  unsigned char structType;
  unsigned char txmUnitId;
  unsigned long currentTime;
  unsigned short nextTimeReading;
  unsigned short nextTimeOnTimeOut;
  unsigned char  Channel;
  unsigned char  Value;
  char Spare[18];
} RcvPacketStruct;



typedef struct
{
  char header;
  unsigned char structSize;
  unsigned char structType;
  unsigned char txmUnitId;
  unsigned char Status;
  unsigned long  stampTime;
  unsigned short voltageA2D;
  unsigned short temperature;
} TxmDS18B20PacketStruct;


RcvPacketStruct RcvData;
TxmDS18B20PacketStruct Txmdata;

unsigned char * pt = (unsigned char *) &RcvData;


unsigned char rcvBuffer[32];

void PrintHex(uint8_t *data, uint8_t length) // prints 16-bit data in hex with leading zeroes
{
  char tmp[32];
  for (int i = 0; i < length; i++)
  {
    sprintf(tmp, "0x%.2X", data[i]);
    Serial.print(tmp); Serial.print(" ");
  }
}


//From http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/
unsigned short readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return (unsigned short) result; // Vcc in millivolts
}


void loop(void) {
  int loop;
  unsigned long deltaTime;
  /****************** Ping Out Role ***************************/

  if (cycle == ModeInit)
  {
    Count++;
    Txmdata.header = '*';
    Txmdata.structSize = sizeof(TxmDS18B20PacketStruct);
    Txmdata.structType = STRUCT_TYPE_INIT_DATA;
    Txmdata.txmUnitId = UNIT_ID;
    Txmdata.stampTime = 0;
    Txmdata.Status = 0;
    Txmdata.temperature = 0;
    Txmdata.voltageA2D = 0;
    radio.writeAckPayload(1, &Txmdata, 0);
    cycle = ModeListen;
  }

  if (cycle == ModeListen)
  {
    if (radio.available())
    {
      int rcv_size = radio.getDynamicPayloadSize();
      radio.read( &RcvData, rcv_size);

      if (RcvData.header == '*')
        if(RcvData.structType == STRUCT_TYPE_DIGITAL_OUT)
          {
            
            setLed(RcvData.Channel,RcvData.Value);
            Serial.print("OUTPUT ");
            Serial.print(RcvData.Channel);
            Serial.print(" : ");
            Serial.print(RcvData.Value);
            Serial.print("\n");
          }
      radio.writeAckPayload(1,&Txmdata,0);
      cycle = ModeListen;
    }
  }


}
