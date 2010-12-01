#include "print_p.h"

void print_P(Print& out, PGM_P str) {
  while (1) {
    char ch = pgm_read_byte_near(str++);
    if (!ch)
      return;
    out.write(ch);
  }
}

