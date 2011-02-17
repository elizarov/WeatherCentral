#include <LiquidCrystal.h>
#include <Metro.h>
#include <avr/pgmspace.h>

#include "display.h"
#include "util.h"

LiquidCrystal lcd(7, 9, 2, 3, 5, 6);

// current buffer string
char displayBuf[DISPLAY_LENGTH+1];

const char SENSOR_CODES[] PROGMEM = " 123456789?CRUWH";

#define TIMEOUT (10 * 60000L) // 10 min
#define ANIMATION_LENGTH 2 

struct Sensor {
  boolean seen;
  long lastTime;
};

Sensor sensor[MAX_SENSORS];
Metro animationPeriod(1000, true); 
char animation[ANIMATION_LENGTH] = { ' ', '.' };
byte animationPos;

void setupDisplay() {
  lcd.begin(2, 16);
  lcd.print("WeatherCentral");
}

char sStatus[MAX_SENSORS + 1];

void updateDisplay(char* s) {
  // echo to console
  Serial.print('[');
  Serial.print(s);
  Serial.println(']');
  // find sensor id
  char scode = s[0];
  byte sid;
  for (sid = 0; sid < MAX_SENSORS; sid++)
    if (pgm_read_byte(&(SENSOR_CODES[sid])) == scode)
      break;
  if (sid >= MAX_SENSORS)
    return;
  // prepare strings for display
  long time = millis();
  sensor[sid].seen = true;
  sensor[sid].lastTime = time;
  // prepare status line
  sStatus[0] = animation[animationPos];
  for (byte i = 1; i < MAX_SENSORS; i++)
    sStatus[i] = sensor[i].seen && time - sensor[i].lastTime < TIMEOUT ? pgm_read_byte(&(SENSOR_CODES[i])) : ' ';
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
