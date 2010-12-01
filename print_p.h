#ifndef PRINT_P_H
#define PRINT_P_H

#include <WProgram.h>
#include <avr/pgmspace.h>

inline PGM_P id_P(PGM_P str) { return str; }

void print_P(Print& out, PGM_P str);

#endif
