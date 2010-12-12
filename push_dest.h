#ifndef PUSH_DEST_H
#define PUSH_DEST_H

#include <avr/pgmspace.h>

#include <WProgram.h>
#include <Client.h>
#include <Metro.h>

#define INITIAL_MSG_WAIT  1000L // 1sec
#define INITIAL_INTERVAL 60000L // 1min
#define RETRY_INTERVAL   10000L // 10sec
#define NEXT_INTERVAL   300000L // 5min

#define PUSH_TIMEOUT     30000L // 30sec

#define MAX_RESPONSE 20
#define MAX_COOKIE_LEN 20

class PushDest {
protected:  
  byte _mask;
  byte _ip[4];
  PGM_P _host;
  PGM_P _url;
  PGM_P _auth;
  PGM_P _method;
  
  Client _client;
  Metro _period; 
  Metro _timeout;
  
  byte _next;
  boolean _sending;
  boolean _eoln;
  byte _responseSize;
  char _response[MAX_RESPONSE + 1];

  boolean sendPacket(byte size);  
  void parseChar(char ch);
  boolean readResponse();
  virtual void doneSend(boolean success);
  virtual void printUrlParams() {}
  virtual void printExtraHeaders() {}
  virtual void parseResponse(char ch) {}
public:
  PushDest(byte mask, byte ip0, byte ip1, byte ip2, byte ip3, int port, PGM_P host, PGM_P url, PGM_P auth);
  void check();
};

class PushMsgDest : PushDest {
protected:
  byte _index;
  boolean _newsession;
  char _cookie[MAX_COOKIE_LEN + 1];
  byte _pstate;
  boolean _wait;
  virtual void doneSend(boolean success);
  virtual void printUrlParams();
  virtual void printExtraHeaders();
  virtual void parseResponse(char ch);
public:  
  PushMsgDest(byte mask, byte ip0, byte ip1, byte ip2, byte ip3, int port, PGM_P host, PGM_P url, PGM_P auth);
  void check();
};

// all are declared in push_config.cpp
extern PushDest pachube;
extern PushDest haworks_data;
extern PushMsgDest haworks_message;

#endif

