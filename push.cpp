#include <Ethernet.h>

#include "push.h"
#include "push_dest.h"
#include "display.h"
#include "fmt_util.h"
#include "xprint.h"
#include "msgbuf.h"

#define MAX_PACKET 200
#define MASK_ALL 0xff

struct Item {
  int val;    // value
  byte prec;  // precision
  byte updated; 
  byte sending;
};

#define MAC_ADDR     {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}
#define IP_ADDR      IPAddress(192, 168, 7, 40)
#define GATEWAY_ADDR IPAddress(192, 168, 7, 1)
#define SUBNET_ADDR  IPAddress(255, 255, 255, 0)

const char ETHERNET_RESET[] PROGMEM = "{W:Ethernet reset}";

const char HTTP_RES[] PROGMEM = "HTTP/1.1";
const char HTTP_OK[] PROGMEM = "HTTP/1.1 200 OK";
const char PUT[] PROGMEM = "PUT";
const char POST[] PROGMEM = "POST";
const char SET_COOKIE[] PROGMEM = "Set-Cookie: ";

#define RESPONSE_LINE1       0  // 1st line of response
#define RESPONSE_HEADERS0    1  // response headers start of line; '\n' was seen
#define RESPONSE_HEADERS1    2  // response headers; '\n\r' was seen
#define RESPONSE_HEADERS_ANY 3  // response headers, mid of line
#define RESPONSE_BODY        4  // response body; '\n[\r]\n' was seen 

#define PCOOKIE_STATE_0          0 
#define PCOOKIE_STATE_SET_COOKIE 12 // length of SET_COOKIE string
#define PCOOKIE_STATE_ERR        PCOOKIE_STATE_SET_COOKIE + MAX_COOKIE_LEN
#define PCOOKIE_STATE_DONE       PCOOKIE_STATE_ERR + 1

#define PBODY_STATE_0     0  // expect '2'
#define PBODY_STATE_1     1  // expect ','
#define PBODY_STATE_MSG   2  // reading message chars
#define PBODY_STATE_WIDX  3  // wait for ',' to begin index
#define PBODY_STATE_IDX   4  // parsing index
#define PBODY_STATE_DONE  5  // successfully parsed
#define PBODY_STATE_ERR   6  // error

#define FAILURE_NORMAL   0
#define FAILURE_SINGLE   1
#define FAILURE_RESET    2
#define FAILURE_REPEATED 3

#define FAILURE_ETHERNET_RESET_TIMEOUT 60000L // 1 minute
#define FAILURE_HARDWARE_RESET_TIMEOUT (60 * 60000L) // 1 hour

#define HARDWARE_RESET_PIN A1

#define MAX_RESPONSE 20

EthernetClient client;
boolean clientBusy;

byte responsePart;
byte responseSize;
char response[MAX_RESPONSE + 1];

byte failureStatus;
long firstFailureTime;
long lastFailureTime;

Item items[MAX_PUSH_ID];
char packet[MAX_PACKET];

void updateFailureTime(boolean success) {
  if (success) {
    failureStatus = FAILURE_NORMAL;
  } else if (failureStatus == FAILURE_NORMAL) {
    failureStatus = FAILURE_SINGLE;
    lastFailureTime = firstFailureTime = millis();
  } else if (failureStatus == FAILURE_RESET) {
    failureStatus = FAILURE_REPEATED;
    lastFailureTime = millis();
  }
}

inline void hardwareReset () {
  pinMode(HARDWARE_RESET_PIN, OUTPUT);
}

inline void ethernetReset() {
  waitPrint();
  print_P(ETHERNET_RESET);
  if (failureStatus == FAILURE_SINGLE) {
    MsgBuf.putMessage_P(ETHERNET_RESET);
    Serial.println('*');
  } else {
    Serial.println();
  }
  failureStatus = FAILURE_RESET;
  setupPush();
}

inline void checkFailure() {
  if (failureStatus != FAILURE_SINGLE && failureStatus != FAILURE_REPEATED)
    return;
  if (millis() - firstFailureTime > FAILURE_HARDWARE_RESET_TIMEOUT)
    hardwareReset();
  if (millis() - lastFailureTime > FAILURE_ETHERNET_RESET_TIMEOUT)
    ethernetReset();
}

inline byte composeDataPacket(byte mask, boolean &next) {
  byte size = 0;
  byte i = next;
  for (; i < MAX_PUSH_ID && size < MAX_PACKET - 11; i++)
    if (items[i].updated & mask) {
      items[i].sending |= mask;
      byte k = formatDecimal(i, &packet[size], 2, FMT_SPACE | FMT_LEFT);
      size += k;
      packet[size++] = ',';
      k = formatDecimal(items[i].val, &packet[size], 6, items[i].prec | FMT_SPACE | FMT_LEFT);
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

PushDest::PushDest(byte mask, char* host, int port, PGM_P url, PGM_P auth) :
  _period(INITIAL_INTERVAL),
  _timeout(PUSH_TIMEOUT)
{
  _mask = mask;
  _host = host;
  _port = port;
  _url = url;
  _auth = auth;
  _method = PUT;
}

boolean PushDest::sendPacket(byte size) {
  checkFailure(); // reset ethernet module if needed
  waitPrint();
  Serial.print(_host);
  Serial.print(':');
  Serial.print(' ');
  print_P(_method);  
  Serial.print(' ');
  Serial.print(size, DEC);
  print_P(PSTR(" bytes"));
  Serial.println();
  if (!client.connect(_host, _port)) {
    waitPrint();
    Serial.print(_host);
    print_P(PSTR(": Failed to connect\r\n"));
    return false;
  }
  
  // PUT/POST <url> HTTP/1.1
  print_P(client, _method);
  client.print(' ');
  print_P(client, _url);
  printExtraUrlParams();
  print_P(client, PSTR(" HTTP/1.1"));
  client.println();
  
  // Host: <host>
  print_P(client, PSTR("Host: "));
  client.println(_host);
  
  // <auth>
  print_P(client, _auth);
  client.println();
  
  // extra stuff
  printExtraHeaders();
  
  // Connection: close
  print_P(client, PSTR("Connection: close"));
  client.println();
  
  // Content-Length: <size>
  print_P(client, PSTR("Content-Length: "));
  client.print(size, DEC);
  client.println();
  
  // empty line & packet itself
  client.println();
  client.print(packet);
  _timeout.reset();
  _sending = true;
  responsePart = RESPONSE_LINE1;
  responseSize = 0;
  clientBusy = true;
  return true;
}

void PushDest::doneSend(boolean success) {
  updateFailureTime(success);
  markSent(_mask, success);
  _period.interval(success ? NEXT_INTERVAL : RETRY_INTERVAL);
  _period.reset();
}

void PushDest::parseChar(char ch) {
  switch (responsePart) {
  case RESPONSE_LINE1:
    if (ch == '\n') {
      responsePart = RESPONSE_HEADERS0;
    } else {
      if (ch != '\r' && responseSize < MAX_RESPONSE) 
        response[responseSize++] = ch;
    }
    break;
  case RESPONSE_HEADERS0:
    if (ch == '\r')
      responsePart = RESPONSE_HEADERS1;
    else if (ch == '\n')
      responsePart = RESPONSE_BODY;  
    else
      responsePart = RESPONSE_HEADERS_ANY;  
    parseResponseHeaders(ch);
    break;
  case RESPONSE_HEADERS1:
    if (ch == '\n')
      responsePart = RESPONSE_BODY;
    else
      responsePart = RESPONSE_HEADERS0;
    parseResponseHeaders(ch);
    break;
  case RESPONSE_HEADERS_ANY:
    if (ch == '\n')
      responsePart = RESPONSE_HEADERS0;
    parseResponseHeaders(ch);
    break;
  case RESPONSE_BODY:
    parseResponseBody(ch);
    break;
  }
}

boolean PushDest::readResponse() {
  if (!_sending)
    return false;
  if (client.connected() && !_timeout.check()) {
    while (client.available()) 
      parseChar(client.read());
    if (client.connected())
      return true; // if still connected will read more
  }
  // not connected anymore or timeout
  client.stop();
  _sending = false;
  boolean ok = false;
  if (responsePart != RESPONSE_LINE1 && strncmp_P(response, HTTP_RES, strlen_P(HTTP_RES)) == 0) {
    response[responseSize] = 0;
    ok = strcmp_P(response, HTTP_OK) == 0;
    response[DISPLAY_LENGTH] = 0;
    updateDisplay(response);
  } else {
    waitPrint();
    Serial.print(_host);
    print_P(PSTR(": No response\r\n"));
  }
  doneSend(ok);
  clientBusy = false;
  return false; // done with response
}

void PushDest::check() {
  if (readResponse())
    return; // reading response
  if (clientBusy)
    return; // client is busy serving some other destination 
  if (_next == 0 && !_period.check())
    return;
  byte size = composeDataPacket(_mask, _next);
  if (size == 0)
    return;
  if (!sendPacket(size))
    doneSend(false);
}

PushMsgDest::PushMsgDest(byte mask, char* host, int port, PGM_P url, PGM_P auth) :
    PushDest(mask, host, port, url, auth) 
{
  _newSession = true;
  _wait = true;
  _period.interval(INITIAL_MSG_WAIT); // need to wait for Ethernet to initialize
  _method = POST;
}

void PushMsgDest::doneSend(boolean success) {
  updateFailureTime(success);
  if (success) {
    if (_newSession) {
      _newSession = false;
      _indexOut = 0; 
    } else  
      MsgBuf.removeMessages(_indexOut);
    if (_parseBodyState != PBODY_STATE_DONE)
      _indexIn = 0; // reset incoming index if message was not properly parsed
    _wait = false; // no forced wait
    _period.interval(POLL_MSG_INTERVAL);
    _period.reset();  
  } else {
    _wait = true;
    _period.interval(RETRY_INTERVAL);
    _period.reset();
  }
}

void PushMsgDest::printExtraUrlParams() {
  print_P(client, PSTR("?id=2&last=1"));
  if (_indexIn > 0) {
    print_P(client, PSTR("&index="));
    client.print(_indexIn, DEC);
  }
  if (_newSession)
    print_P(client, PSTR("&newsession1"));
  _parseCookieState = PCOOKIE_STATE_0;
  _parseBodyState = PBODY_STATE_0;
}

void PushMsgDest::printExtraHeaders() {
  if (_newSession || _cookie[0] == 0)
    return;
  print_P(client, PSTR("Cookie: "));
  client.print(_cookie);
  client.println();
}

void PushMsgDest::parseResponseHeaders(char ch) {
  boolean eoln = ch == '\r' || ch == '\n';
  if (_parseCookieState < PCOOKIE_STATE_SET_COOKIE) {
    if (eoln)
      _parseCookieState = PCOOKIE_STATE_0;
    else if (ch == pgm_read_byte(SET_COOKIE + _parseCookieState))
      _parseCookieState++;
    else
      _parseCookieState = PCOOKIE_STATE_ERR;  
  } else if (_parseCookieState < PCOOKIE_STATE_ERR) {
    if (eoln)
      _parseCookieState = PCOOKIE_STATE_DONE;
    else {  
      _cookie[_parseCookieState - PCOOKIE_STATE_SET_COOKIE] = ch;
      _parseCookieState++;
      _cookie[_parseCookieState - PCOOKIE_STATE_SET_COOKIE] = 0;
    }
  } else if (_parseCookieState == PCOOKIE_STATE_ERR) {
    if (eoln) 
      _parseCookieState = PCOOKIE_STATE_0;
  } // else PSTATE_DONE do nothing
}

void PushMsgDest::parseResponseBody(char ch) {
  switch (_parseBodyState) {
  case PBODY_STATE_0:
    _parseBodyState = (ch == '2') ? PBODY_STATE_1 : PBODY_STATE_ERR;
    break;  
  case PBODY_STATE_1:
    _parseBodyState = (ch == ',') ? PBODY_STATE_MSG : PBODY_STATE_ERR;
    break;
  case PBODY_STATE_MSG:
    if (ch == ',') {
      Serial.println();
      _parseBodyState = PBODY_STATE_WIDX;
    } else
      Serial.print(ch);
    break;
  case PBODY_STATE_WIDX:
    if (ch == ',') {
      _parseBodyState = PBODY_STATE_IDX;
      _indexIn = 0;
    }
    break;
  case PBODY_STATE_IDX:
    if (ch == '\r' || ch == '\n') {
      _parseBodyState = PBODY_STATE_DONE;
    } else if (ch >= '0' && ch <= '9') {
      _indexIn *= 10;
      _indexIn += ch - '0';
    } else
      _parseBodyState = PBODY_STATE_ERR;
    break;      
  }
}

void PushMsgDest::check() {
  if (readResponse())
    return;
  if (clientBusy)
    return;  
  boolean periodCheck = _period.check();
  if (_wait && !periodCheck)
    return; // we are in a 'forced wait' either on startup or after error
  byte index = 0;  
  byte size = MsgBuf.encodeMessages(&packet[0], MAX_PACKET, index);
  // We return if we don't have outgoing message nor incoming messages to confirm nor periodic poll time
  if (size == 0 && _indexIn == 0 && !periodCheck)
    return;
  if (size > 0 && index < _indexOut) 
    _newSession = true; // force new session for outgoing messages if index was reset
  if (_newSession) { 
    // send empty message to create new session
    size = 0;
    packet[0] = 0;
  } else
    _indexOut = index;
  if (!sendPacket(size))
    doneSend(false);
}

void setupPush() {
  byte mac[] = MAC_ADDR;
  Ethernet.begin(mac, IP_ADDR, GATEWAY_ADDR, GATEWAY_ADDR, SUBNET_ADDR); // DNS on gateway
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

