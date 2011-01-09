#include <avr/pgmspace.h>
#include <WProgram.h>
#include <OsReceiver.h>

#include "display.h"
#include "parse.h"
#include "push.h"
#include "util.h"
#include "print_p.h"
#include "command.h"
#include "msgbuf.h"

const char BANNER[] PROGMEM = "{W:WeatherCentral started}";

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
  delay(500); // wait 0.5s (make sure XBee on serial port starts)
  Serial.begin(57600);
  print_P(Serial, BANNER);
  Serial.println('*');
  setupDisplay();
  setupPush();
  MsgBuf.putMessage_P(BANNER);
  OsReceiver.init();  
}

void loop() {
  receiveWeatherData();
  checkDisplay();
  checkPush();
  checkCommand();
}

