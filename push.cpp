#include <Ethernet.h>

#include "push.h"
#include "push_dest.h"
#include "display.h"
#include "util.h"

#define MAX_PACKET 512
#define MASK_ALL 0xff

struct Item {
  int val;    // value
  byte prec;  // precision
  byte updated; 
  byte sending;
};

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 7, 40};
byte gateway[] = {192, 168, 7, 1};	
byte subnet[] = {255, 255, 255, 0};
char* crlf = "\r\n";

Item items[MAX_PUSH_ID];
char packet[MAX_PACKET];

int composePacket(byte mask) {
  int size = 0;
  for (byte i = 0; i < MAX_PUSH_ID && size < MAX_PACKET - 10; i++)
    if (items[i].updated & mask) {
      items[i].sending |= mask;
      byte k = formatDecimal(i, &packet[size], 2, FMT_SPACE | FMT_LEFT);
      size += k;
      packet[size++] = ',';
      k = formatDecimal(items[i].val, &packet[size], 5, items[i].prec | FMT_SPACE | FMT_LEFT);
      size += k;
      packet[size++] = '\n';
    }
  packet[size] = 0;  
  return size;
}

void markSent(byte mask, boolean success) {
  for (byte i = 0; i <= MAX_PUSH_ID; i++) 
    if (items[i].sending & mask) {
      items[i].sending &= ~mask;
      if (success)
        items[i].updated &= ~mask;
    }
}

PushDest::PushDest(byte ip0, byte ip1, byte ip2, byte ip3, int port, char* host, char* url, char* auth) :
  _client(&_ip[0], port),
  _period(INITIAL_INTERVAL, true)
{
  _ip[0] = ip0;
  _ip[1] = ip1;
  _ip[2] = ip2;
  _ip[3] = ip3;
  _host = host;
  _url = url;
  _auth = auth;
}

boolean PushDest::sendPacket(int size) {
  Serial.print("PUT ");  
  Serial.print(size, DEC);
  Serial.print(" bytes to ");
  Serial.println(_host);
  if (!_client.connect()) {
    Serial.println("Failed to connect");
    return false;
  }
  _client.print("PUT ");
  _client.print(_url);
  _client.print(" HTTP/1.1");
  _client.print(crlf);
  _client.print("Host: ");
  _client.print(_host);
  _client.print(crlf);
  _client.print(_auth);
  _client.print(crlf);
  _client.print("Connection: close");
  _client.print(crlf);
  _client.print("Content-Length: ");
  _client.print(size, DEC);
  _client.print(crlf);
  _client.print(crlf);
  _client.print(packet);
  boolean eoln = false;
  char s[DISPLAY_LENGTH + 1];
  byte i = 0;
  while (_client.connected()) {
    if (_client.available()) {
      char ch = _client.read();
      if (eoln)
        continue;
      if (ch == '\r' || ch == '\n') {
        eoln = true;
        Serial.println();
      } else {
        Serial.print(ch);
        if (i < DISPLAY_LENGTH) 
          s[i++] = ch;
      }
    }
  }
  if (eoln) {
    s[i++] = 0;
    updateDisplay(HTTP_STATUS, s);
  } else
    Serial.println("No response");
  _client.stop();
  return eoln;
}

void PushDest::check(byte mask) {
  if (!_period.check())
    return;
  while (1) {  
    int size = composePacket(mask);
    if (size == 0)
      return;
    if (!sendPacket(size)) {
      // faled to send, will retry
      markSent(mask, false);
      _period.interval(RETRY_INTERVAL);
      return; 
    }
    // Sent successfully
    markSent(mask, true);
    _period.interval(NEXT_INTERVAL);
  }
}

void initPush() {
  Ethernet.begin(mac, ip, gateway, subnet);
}

void checkPush() {
  pachube.check(0x01);
  haworks.check(0x02);
}

void push(byte id, int val, byte prec) {
  items[id].val = val;
  items[id].prec = prec;
  items[id].updated = MASK_ALL;
}

