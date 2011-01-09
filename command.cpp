#include <WProgram.h>

#include "command.h"
#include "display.h"
#include "msgbuf.h"
#include "push.h"

/*----------------------------------------------------------------
   Parse text message. A message is any line ending with '*' (star).
   This message line (without start) is pushed to the internet.
*/

#define MSG_STATE_ANY  0   // beginning or middle of the line
#define MSG_STATE_STAR 1   // just seen star ('*')

byte msgState = MSG_STATE_ANY;

void parseMsgChar(char ch) {
  boolean eoln = ch == '\r' || ch == '\n' || ch == 0 || ch == ',';
  switch (msgState) {
  case MSG_STATE_ANY:
    if (eoln) 
      MsgBuf.undoMessage();
    else if (ch == '*')
      msgState = MSG_STATE_STAR;
    else
      MsgBuf.putChar(ch);
    break;
  case MSG_STATE_STAR:
    if (eoln) {
      MsgBuf.saveMessage();
      msgState = MSG_STATE_ANY;
    } else {
      MsgBuf.putChar('*');
      MsgBuf.putChar(ch);
      msgState = MSG_STATE_ANY;
    }
    break;
  }  
}

/*----------------------------------------------------------------
   Parse data message. Data format is:
   !C=a<num>b<num>...
*/

#define DATA_C_ID   30  // First data id for ControlHeater tags

#define DATA_STATE_ANY   0  // Anything
#define DATA_STATE_ATTN  1  // Just seen exclamation mark ('!')
#define DATA_STATE_CTRL  2  // Just seen ControlHeater data tag ('C')
#define DATA_STATE_WTAG  3  // Wait for num tag, just after equals ('=') or prev tag
#define DATA_STATE_WNUM  4  // Tag read, waiting for number
#define DATA_STATE_NUM0  5  // Reading number, just seen '+' or '-' sign
#define DATA_STATE_NUM   6  // Reading number, seen at lest one digit
#define DATA_STATE_FRAC  7  // Reading number after decimal point, seen decimal point

#define DATA_BUF_START 2 

char dataBuf[17] = "C=              ";
byte dataBufPos;
byte dataState = DATA_STATE_ANY;
byte dataId;    
int dataVal;
byte dataPrec;
int8_t dataMul;

inline void pushDataNum() {
  push(dataId, dataVal * dataMul, dataPrec);
}

inline void dataBufInit() {
  for (byte i = DATA_BUF_START; dataBuf[i] != 0; i++)
    dataBuf[i] = ' ';
  dataBufPos = DATA_BUF_START;
}

void dataBufChar(char ch) {
  if (dataBuf[dataBufPos] != 0)
    dataBuf[dataBufPos++] = ch;
}

void dataDone() {
  dataState = DATA_STATE_ANY;
  updateDisplay(&dataBuf[0]);
}

void parseDataTag(char ch) {
  dataId = DATA_C_ID + ch - 'a';
  if (ch < 'a' || ch > 'z' || dataId >= MAX_PUSH_ID)
    dataDone();
  else {
    dataBufChar(ch);
    dataState = DATA_STATE_WNUM;
    dataVal = 0;
    dataPrec = 0;  
    dataMul = 1;    
  }
}

void parseDataChar(char ch) {
  switch (dataState) {
  case DATA_STATE_ANY:
    if (ch == '!') 
      dataState = DATA_STATE_ATTN;  
    break;
  case DATA_STATE_ATTN:
    if (ch == 'C')
      dataState = DATA_STATE_CTRL;
    else
      dataState = DATA_STATE_ANY;
    break;
  case DATA_STATE_CTRL:
    if (ch == '=') {
      dataState = DATA_STATE_WTAG;
      dataBufInit();
    } else
      dataState = DATA_STATE_ANY;
    break;
  case DATA_STATE_WTAG:
    parseDataTag(ch);
    break;
  case DATA_STATE_WNUM:
    if (ch == '+' || ch == '-')  {
      dataBufChar(ch);
      dataState = DATA_STATE_NUM0;
      if (ch == '-')
        dataMul = -1; 
      break;
    }
    // fallthough!
  case DATA_STATE_NUM0:
  case DATA_STATE_NUM:
    if (ch == '.') {
      dataBufChar(ch);
      dataState = DATA_STATE_FRAC;
      break;  
    }
    // fallthough!
  case DATA_STATE_FRAC:
    if (ch >= '0' && ch <= '9') {
      dataBufChar(ch);
      dataVal *= 10;
      dataVal += ch - '0';
      if (dataState == DATA_STATE_FRAC)
        dataPrec++;      
      else
        dataState = DATA_STATE_NUM; // seen digit!  
    } else {
      if (dataState == DATA_STATE_NUM || dataState == DATA_STATE_FRAC) {
        // just parsed number, can have next tag
        pushDataNum();
        parseDataTag(ch);
      } else
        dataDone(); // did not have a number
    }
  }
}


/*----------------------------------------------------------------
   Driver code. 
*/

void checkCommand() {
  while (Serial.available()) {
    char ch = Serial.read();
    parseMsgChar(ch);
    parseDataChar(ch);
  }
}

