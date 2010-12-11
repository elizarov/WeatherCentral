#include <LiquidCrystal.h>
#include <Metro.h>

#include "display.h"
#include "util.h"

LiquidCrystal lcd(7, 9, 2, 3, 5, 6);

#define TIMEOUT (10 * 60000L) // 10 min
#define ANIMATION_LENGTH 2 

struct Sensor {
  char code;
  boolean seen;
  long lastTime;
};

Sensor sensor[MAX_SENSORS];
Metro animationPeriod(1000, true); 
char animation[ANIMATION_LENGTH] = { ' ', '.' };
byte animationPos;

void initDisplay() {
  lcd.begin(2, 16);
  lcd.print("WeatherCentral");
  for (byte i = TEMP_SENSOR_1; i <= TEMP_SENSOR_MAX; i++)
    sensor[i].code = HEX_CHARS[i];
  sensor[UNKN_SENSOR].code = '?';  
  sensor[RAIN_SENSOR].code = 'R';  
  sensor[UVLT_SENSOR].code = 'U';
  sensor[WIND_SENSOR].code = 'W';
  sensor[HTTP_STATUS].code = '*';
}

char sStatus[MAX_SENSORS + 1];

void updateDisplay(byte sid, char* s) {
  // prepare strings for display
  long time = millis();
  sensor[sid].seen = true;
  sensor[sid].lastTime = time;
  sStatus[0] = animation[animationPos];
  for (byte i = 1; i < MAX_SENSORS; i++)
    sStatus[i] = sensor[i].seen && time - sensor[i].lastTime < TIMEOUT ? sensor[i].code : ' ';
  // display  
  lcd.setCursor(0, 0);
  lcd.print(s);
  for (byte i = strlen(s); i < DISPLAY_LENGTH; i++)
    lcd.print(' ');
  lcd.setCursor(0, 1);
  lcd.print(sStatus);
}

void checkDisplay() {
  if (!animationPeriod.check())
    return;
  animationPos++;
  if (animationPos == ANIMATION_LENGTH)
    animationPos = 0;
  // display animation  
  lcd.setCursor(0, 1);
  lcd.print(animation[animationPos]);
}
