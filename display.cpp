#include <LiquidCrystal.h>

#include "display.h"
#include "util.h"

LiquidCrystal lcd(7, 9, 2, 3, 5, 6);

// 10 minutes timeout
#define TIMEOUT (10 * 60000L)

struct Sensor {
  char code;
  boolean seen;
  long lastTime;
};

Sensor sensor[MAX_SENSORS];

void initDisplay() {
  lcd.begin(2, 16);
  lcd.print("WeatherCentral");
  for (byte i = 0; i < MAX_SENSORS; i++)
    sensor[i].code = HEX_CHARS[i];
  sensor[UNKN_SENSOR].code = '?';  
  sensor[RAIN_SENSOR].code = 'R';  
  sensor[UVLT_SENSOR].code = 'U';
  sensor[WIND_SENSOR].code = 'W';
  sensor[HTTP_STATUS].code = '*';
}

char sStatus[MAX_SENSORS + 1];

void updateDisplay(byte sid, char* s) {
  long time = millis();
  sensor[sid].seen = true;
  sensor[sid].lastTime = time;
  for (byte i = 0; i < MAX_SENSORS; i++)
    sStatus[i] = sensor[i].seen && time - sensor[i].lastTime < TIMEOUT ? sensor[i].code : ' ';
  lcd.setCursor(0, 0);
  lcd.print(s);
  for (byte i = strlen(s); i < DISPLAY_LENGTH; i++)
    lcd.print(' ');
  lcd.setCursor(0, 1);
  lcd.print(sStatus);
}

