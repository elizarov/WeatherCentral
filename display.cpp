#include <LiquidCrystal.h>
#include <Metro.h>

#include "display.h"
#include "util.h"

LiquidCrystal lcd(7, 9, 2, 3, 5, 6);

#define TIMEOUT (10 * 60000L) // 10 min
#define ANIMATION_LENGTH 4 

struct Sensor {
  char code;
  boolean seen;
  long lastTime;
};

byte char1[8] = {
  B00000,
  B11100,
  B11000,
  B10100,
  B00010,
  B00000,
  B00000,
  B00000
};

byte char2[8] = {
  B00000,
  B00111,
  B00011,
  B00101,
  B01000,
  B00000,
  B00000,
  B00000
};

byte char3[8] = {
  B00000,
  B00000,
  B01000,
  B00101,
  B00011,
  B00111,
  B00000,
  B00000
};

byte char4[8] = {
  B00000,
  B00000,
  B00010,
  B10100,
  B11000,
  B11100,
  B00000,
  B00000
};

Sensor sensor[MAX_SENSORS];
Metro animationPeriod(250, true); 
char animation[ANIMATION_LENGTH] = { 1, 2, 3, 4 };
byte animationPos;

void initDisplay() {
  lcd.begin(2, 16);
  lcd.print("WeatherCentral");
  lcd.createChar(1, char1);
  lcd.createChar(2, char2);
  lcd.createChar(3, char3);
  lcd.createChar(4, char4);
  for (byte i = 1; i < MAX_SENSORS; i++)
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
