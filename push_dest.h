#ifndef PUSH_DEST_H
#define PUSH_DEST_H

#include <avr/pgmspace.h>

#include <WProgram.h>
#include <Client.h>
#include <Metro.h>

#define INITIAL_INTERVAL 60000L // 1min
#define RETRY_INTERVAL   10000L // 10sec
#define NEXT_INTERVAL   300000L // 5min

#define PUSH_TIMEOUT     30000L // 30sec

#define MAX_RESPONSE 20

class PushDest {
private:  
  byte _mask;
  byte _ip[4];
  PGM_P _host;
  PGM_P _url;
  PGM_P _auth;
  
  Client _client;
  Metro _period; 
  Metro _timeout;
  
  byte _next;
  boolean _sending;
  boolean _eoln;
  byte _responseSize;
  char _response[MAX_RESPONSE + 1];

  boolean sendPacket(int size);  
  boolean readResponse();
  void doneSend(boolean success);
public:
  PushDest(byte mask, byte ip0, byte ip1, byte ip2, byte ip3, int port, PGM_P host, PGM_P url, PGM_P auth);
  void check();
};

// both are declared in push_config.cpp
extern PushDest pachube;
extern PushDest haworks;

#endif

