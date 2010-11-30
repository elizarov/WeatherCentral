#ifndef DISPLAY_H
#define DISPLAY_H

#include <WProgram.h>

#define DISPLAY_LENGTH 16

#define MAX_SENSORS 16

#define UNKN_SENSOR 11
#define RAIN_SENSOR 12
#define UVLT_SENSOR 13
#define WIND_SENSOR 14
#define HTTP_STATUS 15

extern void initDisplay();
extern void updateDisplay(byte sid, char* s);
extern void checkDisplay();

#endif

