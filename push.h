#ifndef PUSH_H
#define PUSH_H

#include <WProgram.h>

#define MAX_PUSH_ID 30

extern void setupPush();
extern void checkPush();
extern void push(byte id, int val, byte prec);

#endif

