#include <Arduino.h>

#include "msgbuf.h"
#include "fmt_util.h"

MsgBufClass MsgBuf;

void MsgBufClass::ensureFreeSpace() {
  while (MSGBUF_SIZE - _size < MAX_MESSAGE_SIZE + MESSAGE_TAG_SIZE) 
    removeMessages(MAX_MESSAGE_INDEX);
}

void MsgBufClass::putInternal(char c) {
  _buf[_cur++] = c;
  if (_cur == MSGBUF_SIZE) _cur = 0;
  _cur_size++;
}

char MsgBufClass::getInternal() {
  char ch = _buf[_head++];
  if (_head == MSGBUF_SIZE) _head = 0;
  _size--;
  return ch;
}

void MsgBufClass::putChar(char c) {
  if (c == 0)
    return;
  if (_cur_size == 0)
    ensureFreeSpace();
  if (_cur_size >= MAX_MESSAGE_SIZE)
    return;
  putInternal(c);
}

void MsgBufClass::putMessage_P(PGM_P s) {
  char ch;
  while ((ch = pgm_read_byte_near(s++)) != 0)
    putChar(ch);
  saveMessage();  
}

void MsgBufClass::undoMessage() {
  _cur = _tail;
  _cur_size = 0;
}

void MsgBufClass::saveMessage() {
  if (_cur_size >= MAX_MESSAGE_SIZE) {
    undoMessage();
    return;
  }  
  putInternal(0);
  long time = millis();
  putInternal(time >> 24);
  putInternal(time >> 16);
  putInternal(time >> 8);
  putInternal(time);
  _index++;
  if (_index >= MAX_MESSAGE_INDEX) _index = 1;
  putInternal(_index);
  _tail = _cur;
  _size += _cur_size;
  _cur_size = 0;  
}

boolean MsgBufClass::available() {
  return _size != 0;
}

byte MsgBufClass::encodeMessages(char *s, byte len, byte &index) {
  byte head0 = _head;
  byte size0 = _size;
  byte i = 0;
  while (_size != 0 && i + 2 <= len) {
    byte head1 = _head;
    byte size1 = _size;
    byte i1 = i;
    s[i++] = '1';
    s[i++] = ',';
    char ch;
    while (i < len && (ch = getInternal()) != 0)
      s[i++] = ch;
    if (i + MESSAGE_TIME_LEN + MESSAGE_INDEX_LEN + 3 >= len) {
      _head = head1;
      _size = size1;
      i = i1;
      break;
    }  
    // format time
    long time = (getInternal() & 0xffL) << 24;
    time |= (getInternal() & 0xffL) << 16;
    time |= (getInternal() & 0xffL) << 8;
    time |= (getInternal() & 0xffL);
    s[i++] = ',';
    i += formatDecimal((long)(time - millis()), &s[i], MESSAGE_TIME_LEN, FMT_SIGN | FMT_SPACE | FMT_LEFT);
    // format index
    index = getInternal();
    s[i++] = ',';
    i += formatDecimal(index, &s[i], MESSAGE_INDEX_LEN, FMT_SPACE | FMT_LEFT);
    s[i++] = '\n';
    if (index == MAX_MESSAGE_INDEX - 1)
      break; // last message in this session
  }
  s[i] = 0;
  _head = head0;
  _size = size0;
  return i;  
}

// when index == MAX_MESSAGE_INDEX removes any one message from head
void MsgBufClass::removeMessages(byte index) {
  if (_size == 0)
    return;
  byte head0 = _head;
  byte size0 = _size;
  while (_size != 0) {  
    while (getInternal() != 0); // skip message
    for (byte i = 0; i < 4; i++)  
      getInternal(); // skip time
    if (getInternal() == index)
      return; // removed  
    if (index == MAX_MESSAGE_INDEX)
      return; // removed anyway  
  }
  // not found
  _head = head0;
  _size = size0;
}
