#ifndef DISPLAY_H
#define DISPLAY_H

#include <WProgram.h>

#define DISPLAY_LENGTH 16
#define MAX_SENSORS 16

// current buffer string
extern char displayBuf[DISPLAY_LENGTH+1];

extern void setupDisplay();
extern void updateDisplay(char* s);
extern void checkDisplay();

#endif

