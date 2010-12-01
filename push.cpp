#include <Ethernet.h>

#include "push.h"
#include "push_dest.h"
#include "display.h"
#include "util.h"
#include "print_p.h"

#define MAX_PACKET 200
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

Item items[MAX_PUSH_ID];
char packet[MAX_PACKET];

inline int composePacket(byte mask, boolean &next) {
  int size = 0;
  byte i = next;
  for (; i < MAX_PUSH_ID && size < MAX_PACKET - 10; i++)
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
  next = i < MAX_PUSH_ID ? i : 0;
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

PushDest::PushDest(byte ip0, byte ip1, byte ip2, byte ip3, int port, const char* host, const char* url, const char* auth) :
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
  Serial.print(_host);
  print_P(Serial, PSTR(": PUT "));  
  Serial.print(size, DEC);
  print_P(Serial, PSTR(" bytes"));
  Serial.println();
  if (!_client.connect()) {
    Serial.print(_host);
    print_P(Serial, PSTR(": Failed to connect"));
    Serial.println();
    return false;
  }
  
  print_P(_client, PSTR("PUT "));
  _client.print(_url);
  print_P(_client, PSTR(" HTTP/1.1"));
  _client.println();
  print_P(_client, PSTR("Host: "));
  _client.print(_host);
  _client.println();
  _client.print(_auth);
  _client.println();
  print_P(_client, PSTR("Connection: close"));
  _client.println();
  print_P(_client, PSTR("Content-Length: "));
  _client.print(size, DEC);
  _client.println();
  _client.println();
  _client.print(packet);
  _sending = true;
  _eoln = false;
  _responseSize = 0;
  return true;
}

boolean PushDest::readResponse() {
  if (!_sending)
    return false;
  if (_client.connected()) {
    while (_client.available()) {
      char ch = _client.read();
      if (_eoln)
        continue;
      if (ch == '\r' || ch == '\n') {
        _eoln = true;
      } else {
        if (_responseSize < MAX_RESPONSE) 
          _response[_responseSize++] = ch;
      }
    }
    if (_client.connected())
      return true; // if still connected will read more
  }
  if (_sending) {
    _client.stop();
    _sending = false;
    Serial.print(_host);
    print_P(Serial, PSTR(": "));
    if (_eoln) {
      _response[_responseSize] = 0;
      Serial.println(_response);
      _response[DISPLAY_LENGTH] = 0;
      updateDisplay(HTTP_STATUS, _response);
    } else {
      print_P(Serial, PSTR("No response\n"));
    }
  }
  return false; // done with response
}

void PushDest::check(byte mask) {
  if (readResponse())
    return;
  if (_next == 0 && !_period.check())
    return;
  int size = composePacket(mask, _next);
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

