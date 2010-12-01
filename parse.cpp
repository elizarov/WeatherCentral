#include "parse.h"
#include "display.h"
#include "push.h"
#include "util.h"

// used for fast generation of ASCII hex strings
const char STS_CHARS[17] = " ghijklmnoabcdef";

const char WIND_DIR[][4] = {
  "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", 
  "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NWN" };

// POSITIONS    0123456789012345
char sUNKN[] = "U#: ------------";
char sTEMP[] = "T#: +??.? ??%  !";
char sRAIN[] = "R#: ------ ----!";
char sUVLT[] = "U#: --         !";
char sWIND[] = "W#: --- --- ---!";
// POSITIONS    0123456789012345

char* parseUnkn(byte* packet, byte len) {
  for (int i = 0; i < 12; i++)
    sUNKN[4 + i] = i < len ? HEX_CHARS[packet[i]] : ' ';
  return &sUNKN[0];
}

char* parseTemp(byte* packet, byte len) {
  byte ch = packet[4];
  int temp = 100 * packet[10] + 10 * packet[9] + packet[8];
  if (packet[11] != 0)
    temp = -temp;
  int humidity = 10 * packet[13] + packet[12];
  push(ch, temp, 1);
  push(ch + 10, humidity, 0);
// boolean batteryOkay = (packet[7] & 0x4) == 0;
  formatDecimal(temp, &sTEMP[4], 5, 1 | FMT_SIGN);
  formatDecimal(humidity, &sTEMP[10], 2, FMT_SPACE);
  sTEMP[15] = STS_CHARS[packet[7]];
  return &sTEMP[0];
}

char* parseRain(byte* packet, byte len) {
  long total = 100000L * packet[17] + 10000L * packet[16] + 1000 * packet[15] +
              100 * packet[14] + 10 * packet[13] + packet[12];
  int rate = 1000 * packet[11] + 100 * packet[10] + 10 * packet[9] + packet[8];
  push(21, rate, 2);
  formatDecimal(total, &sRAIN[4], 6, FMT_SPACE);
  formatDecimal(rate, &sRAIN[11], 4, FMT_SPACE);
  sRAIN[15] = STS_CHARS[packet[7]];
  return &sRAIN[0];
}

char* parseUvlt(byte* packet, byte len) {
  int uv = 10 * packet[9] + packet[8];
  push(22, uv, 0);
  formatDecimal(uv, &sUVLT[4], 2, FMT_SPACE);
  sUVLT[15] = STS_CHARS[packet[7]];
  return &sUVLT[0];
}

char* parseWind(byte* packet, byte len) {
  int dir = packet[8];
  int avg = 100 * packet[16] + 10 * packet[15] + packet[14];
  int gust = 100 * packet[13] + 10 * packet[12] + packet[11];
  push(23, dir, 0);
  push(24, avg, 1);
  push(25, gust, 1);
  formatDecimal(avg, &sWIND[4], 3, FMT_SPACE);
  formatDecimal(gust, &sWIND[8], 3, FMT_SPACE);
  memcpy(&sWIND[12], WIND_DIR[dir], strlen(WIND_DIR[dir]));
  sWIND[15] = STS_CHARS[packet[7]];
  return &sWIND[0];
}

void parsePacket(byte* packet, byte len) {
  int id = (packet[0] << 12) | (packet[1] << 8) | (packet[2] << 4) | packet[3];
  byte ch = packet[4];
  byte rc = (packet[5] << 4) | packet[6];
  byte sid;
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
  s[1] = '0' + ch;
  updateDisplay(sid, s);
}

