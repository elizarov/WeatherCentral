#include <Metro.h>
#include <SPI.h>
#include <Client.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <OsReceiver.h>

#include "display.h"
#include "parse.h"
#include "push.h"
#include "util.h"

// Serialize packet for WeatherStation Data Logger Software
void serialize(byte* packet, byte len, byte version) {
  char cPacket[70];
  int dest = 0;

  cPacket[dest++] = 'O'; 
  cPacket[dest++] = 'S';
  cPacket[dest++] = version + '0';
  cPacket[dest++] = ':';

  for (int k = 1; k < len - 2 && k < sizeof(cPacket) - 2; k++)
    cPacket[dest++] = HEX_CHARS[packet[k]];
  cPacket[dest++] = 0;
  Serial.println(cPacket);
}

void receiveWeatherData() {
  if (!OsReceiver.data_available())
    return;
  byte packet[65];
  byte version;
  byte len = OsReceiver.get_data(packet, sizeof(packet), &version);
  if (len <= 1)
    return;
  serialize(&packet[0], len, version);
  parsePacket(&packet[1], len - 1);
}

void setup() {
  Serial.begin(57600);
  initDisplay();
  initPush();
  OsReceiver.init();  
}

void loop() {
  receiveWeatherData();
  checkPush();
}

