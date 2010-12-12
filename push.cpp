#include <Ethernet.h>

#include "push.h"
#include "push_dest.h"
#include "display.h"
#include "util.h"
#include "print_p.h"
#include "msgbuf.h"

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

const char HTTP_OK[] PROGMEM = "HTTP/1.1 200 OK";
const char PUT[] PROGMEM = "PUT";
const char POST[] PROGMEM = "POST";
const char NEW_SESSION[] PROGMEM = "newsession1";
const char SET_COOKIE[] PROGMEM = "Set-Cookie: ";

#define PSTATE_0          0 
#define PSTATE_SET_COOKIE 12 // length of SET_COOKIE string
#define PSTATE_ERR        PSTATE_SET_COOKIE + MAX_COOKIE_LEN
#define PSTATE_DONE       PSTATE_ERR + 1

Item items[MAX_PUSH_ID];
char packet[MAX_PACKET];

inline byte composeDataPacket(byte mask, boolean &next) {
  byte size = 0;
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

PushDest::PushDest(byte mask, byte ip0, byte ip1, byte ip2, byte ip3, int port, PGM_P host, PGM_P url, PGM_P auth) :
  _client(&_ip[0], port),
  _period(INITIAL_INTERVAL),
  _timeout(PUSH_TIMEOUT)
{
  _mask = mask;
  _ip[0] = ip0;
  _ip[1] = ip1;
  _ip[2] = ip2;
  _ip[3] = ip3;
  _host = host;
  _url = url;
  _auth = auth;
  _method = PUT;
}

boolean PushDest::sendPacket(byte size) {
  print_P(Serial, _host);
  Serial.print(':');
  Serial.print(' ');
  print_P(Serial, _method);  
  Serial.print(' ');
  Serial.print(size, DEC);
  print_P(Serial, PSTR(" bytes"));
  if (!_client.connect()) {
    print_P(Serial, PSTR(": Failed to connect"));
    Serial.println();
    return false;
  }
  
  // PUT/POST <url> HTTP/1.1
  print_P(_client, _method);
  _client.print(' ');
  print_P(_client, _url);
  printUrlParams();
  print_P(_client, PSTR(" HTTP/1.1"));
  _client.println();
  
  // Host: <host>
  print_P(_client, PSTR("Host: "));
  print_P(_client, _host);
  _client.println();
  
  // <auth>
  print_P(_client, _auth);
  _client.println();
  
  // extra stuff
  printExtraHeaders();
  
  // Connection: close
  print_P(_client, PSTR("Connection: close"));
  _client.println();
  
  // Content-Length: <size>
  print_P(_client, PSTR("Content-Length: "));
  _client.print(size, DEC);
  _client.println();
  
  // empty line & packet itself
  _client.println();
  _client.print(packet);
  _timeout.reset();
  _sending = true;
  _eoln = false;
  _responseSize = 0;
  Serial.println(); // done sending
  return true;
}

void PushDest::doneSend(boolean success) {
  markSent(_mask, success);
  _period.interval(success ? NEXT_INTERVAL : RETRY_INTERVAL);
  _period.reset();
}

void PushDest::parseChar(char ch) {
  if (_eoln) {
    parseResponse(ch);
    return;
  }
  if (ch == '\r' || ch == '\n') {
    _eoln = true;
  } else {
    if (_responseSize < MAX_RESPONSE) 
      _response[_responseSize++] = ch;
  }
}

boolean PushDest::readResponse() {
  if (!_sending)
    return false;
  if (_client.connected() && !_timeout.check()) {
    while (_client.available()) 
      parseChar(_client.read());
    if (_client.connected())
      return true; // if still connected will read more
  }
  // not connected anymore or timeout
  _client.stop();
  _sending = false;
  boolean ok = false;
  print_P(Serial, _host);
  print_P(Serial, PSTR(": "));
  if (_eoln) {
    _response[_responseSize] = 0;
    ok = strcmp_P(_response, HTTP_OK) == 0;
    Serial.println(_response);
    _response[DISPLAY_LENGTH] = 0;
    updateDisplay(HTTP_STATUS, _response);
  } else {
    print_P(Serial, PSTR("No response\n"));
  }
  doneSend(ok);
  return false; // done with response
}

void PushDest::check() {
  if (readResponse())
    return;
  if (_next == 0 && !_period.check())
    return;
  byte size = composeDataPacket(_mask, _next);
  if (size == 0)
    return;
  if (!sendPacket(size))
    doneSend(false);
}

PushMsgDest::PushMsgDest(byte mask, byte ip0, byte ip1, byte ip2, byte ip3, int port, PGM_P host, PGM_P url, PGM_P auth) :
    PushDest(mask, ip0, ip1, ip2, ip3, port, host, url, auth) 
{
  _newsession = true;
  _wait = true;
  _period.interval(INITIAL_MSG_WAIT); // need to wait for Ethernet to initialize
  _method = POST;
}

void PushMsgDest::doneSend(boolean success) {
  if (success) {
    if (_newsession) {
      _newsession = false;
      _index = 0; 
    } else  
      MsgBuf.removeMessages(_index);
  } else {
    _wait = true;
    _period.interval(RETRY_INTERVAL);
    _period.reset();
  }  
}

void PushMsgDest::printUrlParams() {
  char sep = '?';
  if (_newsession) {
    _client.print(sep);
    print_P(_client, NEW_SESSION);
    Serial.print(sep);
    print_P(Serial, NEW_SESSION);
    sep = '&';
  } 
  _pstate = PSTATE_0;
}

void PushMsgDest::printExtraHeaders() {
  if (_newsession || _cookie[0] == 0)
    return;
  print_P(_client, PSTR("Cookie: "));
  _client.print(_cookie);
  _client.println();
  Serial.print(' ');
  Serial.print(_cookie);
}

void PushMsgDest::parseResponse(char ch) {
  boolean eoln = ch == '\r' || ch == '\n';
  if (_pstate < PSTATE_SET_COOKIE) {
    if (eoln)
      _pstate = PSTATE_0;
    else if (ch == pgm_read_byte_near(SET_COOKIE + _pstate))
      _pstate++;
    else
      _pstate = PSTATE_ERR;  
  } else if (_pstate < PSTATE_ERR) {
    if (eoln)
      _pstate = PSTATE_DONE;
    else {  
      _cookie[_pstate - PSTATE_SET_COOKIE] = ch;
      _pstate++;
      _cookie[_pstate - PSTATE_SET_COOKIE] = 0;
    }
  } else if (_pstate == PSTATE_ERR) {
    if (eoln) 
      _pstate = PSTATE_0;
  } // else PSTATE_DONE do nothing
}

void PushMsgDest::check() {
  if (readResponse())
    return;
  if (_wait && !_period.check())
    return;
  _wait = false;  
  byte index = 0;  
  byte size = MsgBuf.encodeMessages(&packet[0], MAX_PACKET, index);
  if (size == 0)
    return;
  if (index < _index) 
    _newsession = true;
  if (_newsession) { 
    // send empty message to create new session
    size = 0;
    packet[0] = 0;
  } else
    _index = index;
  if (!sendPacket(size))
    doneSend(false);
}

void setupPush() {
  Ethernet.begin(mac, ip, gateway, subnet);
}

void checkPush() {
  pachube.check();
  haworks_data.check();
  haworks_message.check();
}

void push(byte id, int val, byte prec) {
  items[id].val = val;
  items[id].prec = prec;
  items[id].updated = MASK_ALL;
}

