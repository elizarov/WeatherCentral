#include "xprint.h"

#include <Metro.h>

#define INITIAL_PRINT_INTERVAL 500L   // wait 500 ms before first print to get XBee time to initialize
#define PRINT_INTERVAL         200L   // wait 200 ms between prints 

Metro printTimeout(INITIAL_PRINT_INTERVAL);

void setupPrint() {
  Serial.begin(57600);  
}

void waitPrint() {
  while (!printTimeout.check()); // just wait...
  printTimeout.interval(PRINT_INTERVAL);
  printTimeout.reset();
}

void waitPrintln(const char* s) {
  waitPrint();
  Serial.println(s);
}

void print_P(Print& out, PGM_P str) {
  while (1) {
    char ch = pgm_read_byte_near(str++);
    if (!ch)
      return;
    out.write(ch);
  }
}

void print_P(PGM_P str) {
  print_P(Serial, str);
}
