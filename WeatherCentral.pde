#include <LiquidCrystal.h>
#include <OsReceiver.h>

#include "util.h"

#define CONFIRM_DATA_LED 13

LiquidCrystal lcd(7, 9, 2, 3, 5, 6);

// used for fast generation of ASCII hex strings
const char HEX_CHARS[16] = { 
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'  };

const char WIND_DIR[][4] = {
  "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", 
  "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NWN" };

struct Sensor {
  char code;
  boolean seen;
  long lastTime;
};

#define MAX_SENSORS 16

#define UNKN_SENSOR 0
#define RAIN_SENSOR 13
#define UVLT_SENSOR 14
#define WIND_SENSOR 15

#define TIMEOUT (5 * 60000L)

Sensor sensor[MAX_SENSORS];

void initSensors() {
  for (byte i = 0; i < MAX_SENSORS; i++)
    sensor[i].code = HEX_CHARS[i];
  sensor[UNKN_SENSOR].code = '?';  
  sensor[RAIN_SENSOR].code = 'R';  
  sensor[UVLT_SENSOR].code = 'U';
  sensor[WIND_SENSOR].code = 'W';
}

// POSITIONS    0123456789012345
char sUNKN[] = "U#: ----        ";
char sTEMP[] = "T#: +??.? ??%  !";
char sRAIN[] = "R#:             ";
char sUVLT[] = "U#:             ";
char sWIND[] = "W#: --- --- --- ";
// POSITIONS    0123456789012345

char sStatus[MAX_SENSORS + 1];

char* parseUnkn(byte* packet, byte len) {
  for (int i = 0; i < 4; i++)
    sUNKN[4 + i] = HEX_CHARS[packet[i]];
  return &sUNKN[0];
}

char* parseTemp(byte* packet, byte len) {
  int temp = 100 * packet[10] + 10 * packet[9] + packet[8];
  if (packet[11] != 0)
    temp = -temp;
  int humidity = 10 * packet[13] + packet[12];
  boolean batteryOkay = (packet[7] & 0x4) == 0;
  formatDecimal(temp, &sTEMP[4], 5, 1, true);
  formatDecimal(humidity, &sTEMP[10], 2);
  sTEMP[15] = HEX_CHARS[packet[7]];
  return &sTEMP[0];
}

char* parseRain(byte* packet, byte len) {
  return &sRAIN[0];
}

char* parseUvlt(byte* packet, byte len) {
  return &sUVLT[0];
}

char* parseWind(byte* packet, byte len) {
  int avg = 100 * packet[16] + 10 * packet[15] + packet[14];
  int gust = 100 * packet[13] + 10 * packet[12] + packet[11];
  int dir = packet[8];
  formatDecimal(avg, &sWIND[4], 3);
  formatDecimal(gust, &sWIND[8], 3);
  strcpy(&sWIND[12], WIND_DIR[dir]);
  return &sWIND[0];
}

void parsePacket(byte* packet, byte len) {
  long time = millis();
  int id = (packet[0] << 12) | (packet[1] << 8) | (packet[2] << 4) | packet[3];
  byte ch = packet[4];
  byte rc = (packet[5] << 4) | packet[6];
  int sid;
  char *s;
  switch (id) {
  case 0xF824: // THGR810
  case 0xF8B4: // THGR810 (in the anemometer)
  case 0x1D20: // THGR122NX and THGN123N
    sid = ch;
    s = parseTemp(packet, len);
    break;
  case 0x2914: // Rain Bucket
    sid = RAIN_SENSOR;
    s = parseRain(packet, len);
    break;
  case 0xD874: // UVN800
  case 0xEC70: // UVR123
    sid = UVLT_SENSOR;
    s = parseUvlt(packet, len);
    break;    
  case 0x1984: // Wind (Anemometer)
  case 0x1994:
    sid = WIND_SENSOR;
    s = parseWind(packet, len);
    break;
  default:
    sid = UNKN_SENSOR;
    s = parseUnkn(packet, len);
  }  
  sensor[sid].seen = true;
  sensor[sid].lastTime = time;
  s[1] = '0' + ch;
  for (byte i = 0; i < MAX_SENSORS; i++)
    sStatus[i] = sensor[i].seen && time - sensor[i].lastTime < TIMEOUT ? sensor[i].code : ' ';
  lcd.setCursor(0, 0);
  lcd.print(s);
  lcd.setCursor(0, 1);
  lcd.print(sStatus);
}

void receiveWeatherData() {
  if (!OsReceiver.data_available())
    return;

  byte packet[65];
  byte version;
  byte len = OsReceiver.get_data(packet, sizeof(packet), &version);
  if (len <= 0)
    return;

  digitalWrite(CONFIRM_DATA_LED, 1);

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

  digitalWrite(CONFIRM_DATA_LED, 0);

  parsePacket(&packet[1], len - 1);
}

void setup() {
  Serial.begin(57600);
  lcd.begin(2, 16);
  lcd.print("Welcome!");
  initSensors();
  OsReceiver.init();  
}

void loop() {
  receiveWeatherData();
}

