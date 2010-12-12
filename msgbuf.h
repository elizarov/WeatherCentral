#ifndef MSGBUF_H
#define MSGBUG_F

#include <avr/pgmspace.h>

#define MSGBUF_SIZE 180
#define MAX_MESSAGE_SIZE 80
#define MESSAGE_TAG_SIZE 5

#define MESSAGE_TIME_LEN 10
#define MESSAGE_INDEX_LEN 2
#define MAX_MESSAGE_INDEX 100

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
