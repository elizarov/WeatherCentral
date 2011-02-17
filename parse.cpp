#include <avr/pgmspace.h>

#include "parse.h"
#include "display.h"
#include "push.h"
#include "util.h"

#define WIND_DIR_LEN 3

// used for fast generation of ASCII hex strings
const char STS_CHARS[17] PROGMEM = " ghijklmnoabcdef";

const char WIND_DIR[][WIND_DIR_LEN + 1] PROGMEM = {
  "N  ", "NNE", "NE ", "ENE", "E  ", "ESE", "SE ", "SSE", 
  "S  ", "SSW", "SW ", "WSW", "W  ", "WNW", "NW ", "NWN" };

// POSITIONS            0123456789012345
char sUNKN[] PROGMEM = "?: -------------";
char sTEMP[] PROGMEM = "#: +??.? ??%   !";
char sRAIN[] PROGMEM = "R: ------ --.--!";
char sUVLT[] PROGMEM = "U: --          !";
char sWIND[] PROGMEM = "W: --- --- --- !";
// POSITIONS            0123456789012345

void parseStatus(byte* packet) {
  displayBuf[15] = pgm_read_byte_near(STS_CHARS + packet[7]);
}

void parseUnkn(byte* packet, byte len) {
  strcpy_P(displayBuf, sUNKN);
  for (int i = 0; i < 13; i++)
    displayBuf[3 + i] = i < len ? HEX_CHARS[packet[i]] : ' ';
}

void parseTemp(byte* packet, byte len) {
  strcpy_P(displayBuf, sTEMP);
  byte ch = packet[4];
  int temp = 100 * packet[10] + 10 * packet[9] + packet[8];
  if (packet[11] != 0)
    temp = -temp;
  int humidity = 10 * packet[13] + packet[12];
  push(ch, temp, 1);
  push(ch + 10, humidity, 0);
// boolean batteryOkay = (packet[7] & 0x4) == 0;
  displayBuf[0] = '0' + ch;
  formatDecimal(temp, &displayBuf[3], 5, 1 | FMT_SIGN | FMT_SPACE);
  formatDecimal(humidity, &displayBuf[9], 2, FMT_SPACE);
  parseStatus(packet);
}

void parseRain(byte* packet, byte len) {
  strcpy_P(displayBuf, sRAIN);
  long total = 100000L * packet[17] + 10000L * packet[16] + 1000 * packet[15] +
              100 * packet[14] + 10 * packet[13] + packet[12];
  int rate = 1000 * packet[11] + 100 * packet[10] + 10 * packet[9] + packet[8];
  push(21, rate, 2);
  formatDecimal(total, &displayBuf[3], 6, FMT_SPACE);
  formatDecimal(rate, &displayBuf[10], 5, 2 | FMT_SPACE);
  parseStatus(packet);
}

void parseUvlt(byte* packet, byte len) {
  strcpy_P(displayBuf, sUVLT);
  int uv = 10 * packet[9] + packet[8];
  push(22, uv, 0);
  formatDecimal(uv, &displayBuf[3], 2, FMT_SPACE);
  parseStatus(packet);
}

void parseWind(byte* packet, byte len) {
  strcpy_P(displayBuf, sWIND);
  int dir = packet[8];
  int avg = 100 * packet[16] + 10 * packet[15] + packet[14];
  int gust = 100 * packet[13] + 10 * packet[12] + packet[11];
  push(23, dir, 0);
  push(24, avg, 1);
  push(25, gust, 1);
  formatDecimal(avg, &displayBuf[3], 3, FMT_SPACE);
  formatDecimal(gust, &displayBuf[7], 3, FMT_SPACE);
  memcpy_P(&displayBuf[11], WIND_DIR[dir], WIND_DIR_LEN);
  parseStatus(packet);
}

void parsePacket(byte* packet, byte len) {
  int id = (packet[0] << 12) | (packet[1] << 8) | (packet[2] << 4) | packet[3];
  byte ch = packet[4];
  byte rc = (packet[5] << 4) | packet[6];
  switch (id) {
  case 0xF824: // THGR810
  case 0xF8B4: // THGR810 (in the anemometer)
  case 0x1D20: // THGR122NX and THGN123N
    parseTemp(packet, len);
    break;
  case 0x2914: // Rain Bucket
    parseRain(packet, len);
    break;
  case 0xD874: // UVN800
  case 0xEC70: // UVR123
    parseUvlt(packet, len);
    break;    
  case 0x1984: // Wind (Anemometer)
  case 0x1994:
    parseWind(packet, len);
    break;
  default:
    parseUnkn(packet, len);
  }  
  updateDisplay(displayBuf);
}

