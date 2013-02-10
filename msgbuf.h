#ifndef MSGBUF_H
#define MSGBUG_F

#include <avr/pgmspace.h>

const int MSGBUF_SIZE = 200;
const int MAX_MESSAGE_SIZE = 120;
const int MESSAGE_TAG_SIZE = 5;

const int MESSAGE_TIME_LEN = 10;
const int MESSAGE_INDEX_LEN = 2;
const int MAX_MESSAGE_INDEX = 100;

class MsgBufClass {
private:
  byte _buf[MSGBUF_SIZE];
  byte _head;
  byte _tail;
  byte _size;
  byte _cur;
  byte _cur_size;
  byte _index;
  
  void ensureFreeSpace();
  void putInternal(char c);
  char getInternal();
public:
  void putChar(char c);
  void putMessage_P(PGM_P s);
  void undoMessage();
  void saveMessage();
  boolean available();
  byte encodeMessages(char *s, byte len, byte &index);
  void removeMessages(byte index);
};

extern MsgBufClass MsgBuf;

#endif
