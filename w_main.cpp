#include <avr/pgmspace.h>
#include <Arduino.h>

#include "OsReceiver.h"
#include "display.h"
#include "parse.h"
#include "push.h"
#include "fmt_util.h"
#include "xprint.h"
#include "command.h"
#include "msgbuf.h"
#include "bmp085.h"

const char BANNER[] PROGMEM = "{W:WeatherCentral started}";

// Serialize packet for WeatherStation Data Logger Software
//void serialize(byte* packet, byte len, byte version) {
//  char cPacket[70];
//  int dest = 0;
//
//  cPacket[dest++] = 'O'; 
//  cPacket[dest++] = 'S';
//  cPacket[dest++] = version + '0';
//  cPacket[dest++] = ':';
//
//  for (byte k = 1; k + 2 < len && k + 2 < (byte)sizeof(cPacket); k++)
//    cPacket[dest++] = HEX_CHARS[packet[k]];
//  cPacket[dest++] = 0;
//  Serial.println(cPacket);
//}

void receiveWeatherData() {
  if (!OsReceiver.data_available())
    return;
  byte packet[65];
  byte version;
  byte len = OsReceiver.get_data(packet, sizeof(packet), &version);
  if (len <= 1)
    return;
  //serialize(&packet[0], len, version);
  parsePacket(&packet[1], len - 1);
}

void setup() {
  setupPrint();
  setupDisplay();
  setupPush();
  MsgBuf.putMessage_P(BANNER);
  OsReceiver.init();
  setupBMP085();
  waitPrint();
  print_P(BANNER);
  Serial.println('*');
}

void loop() {
  receiveWeatherData();
  checkDisplay();
  checkPush();
  checkCommand();
  checkBMP085();
}

