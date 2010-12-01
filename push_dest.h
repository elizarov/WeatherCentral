#ifndef PUSH_DEST_H
#define PUSH_DEST_H

#include <WProgram.h>
#include <Client.h>
#include <Metro.h>

#define INITIAL_INTERVAL 60000L // 1min
#define RETRY_INTERVAL   10000L // 10sec
#define NEXT_INTERVAL   300000L // 5min

class PushDest {
private:  
  byte _ip[4];
  char* _host;
  char* _url;
  char* _auth;
  
  Client _client;
  Metro _period; 

  boolean sendPacket(int size);  
public:
  PushDest(byte ip0, byte ip1, byte ip2, byte ip3, int port, char* host, char* url, char* auth);
  void check(byte mask);
};

// both are declared in push_config.cpp
extern PushDest pachube;
extern PushDest haworks;

#endif

