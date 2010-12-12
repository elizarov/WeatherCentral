#include <WProgram.h>

#include "command.h"
#include "msgbuf.h"

#define STATE_ANY  0
#define STATE_STAR 1

byte state = STATE_ANY;

void parseChar(char ch) {
  boolean eoln = ch == '\r' || ch == '\n' || ch == 0 || ch == ',';
  switch (state) {
  case STATE_ANY:
    if (eoln) 
      MsgBuf.undoMessage();
    else if (ch == '*')
      state = STATE_STAR;
    else
      MsgBuf.putChar(ch);
    break;
  case STATE_STAR:
    if (eoln) {
      MsgBuf.saveMessage();
      state = STATE_ANY;
    } else {
      MsgBuf.putChar('*');
      MsgBuf.putChar(ch);
      state = STATE_ANY;
    }
    break;
  }  
}

void checkCommand() {
  while (Serial.available())
    parseChar(Serial.read());
}

