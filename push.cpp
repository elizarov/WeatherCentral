#include <Client.h>
#include <Ethernet.h>
#include <Metro.h>

#include "push.h"
#include "config.h" // defines FEED_ID and API_KEY
#include "display.h"
#include "util.h"

#define INITIAL_INTERVAL 60000L // 1min
#define RETRY_INTERVAL   10000L // 10sec
#define NEXT_INTERVAL   300000L // 5min

#define MAX_PACKET 512

struct Item {
  byte id;    // feed id
  int val;    // value
  byte prec;  // precision
  boolean updated; 
  boolean sending;
};

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 7, 40};
byte gateway[] = {192, 168, 7, 1};	
byte subnet[] = {255, 255, 255, 0};
byte server[] = {173, 203, 98, 29}; // api.pachube.com 

Item items[MAX_PUSH_ID];
Metro pushPeriod(INITIAL_INTERVAL, true); 
char packet[MAX_PACKET];
Client client(server, 80);

void initPush() {
  Ethernet.begin(mac, ip, gateway, subnet);
}

boolean sendPacket(int size) {
  Serial.print("PUT ");  
  Serial.print(size, DEC);
  Serial.print(" bytes - ");
  if (!client.connect()) {
    Serial.println("Failed to connect");
    return false;
  }
  client.print("PUT /v2/feeds/" FEED_ID ".csv HTTP/1.1\r\n");
  client.print("Host: api.pachube.com\r\n");
  client.print("X-PachubeApiKey: " API_KEY "\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(size, DEC);
  client.print("\r\n");
  client.print("\r\n");
  client.print(packet);
  boolean eoln = false;
  char s[DISPLAY_LENGTH + 1];
  byte i = 0;
  while (client.connected()) {
    if (client.available()) {
      char ch = client.read();
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
  client.stop();
  return eoln;
}

void checkPush() {
  if (!pushPeriod.check())
    return;
  byte i = 0;  
  while (1) {  
    // Compose packet(s)
    int size = 0;
    for (; i < MAX_PUSH_ID && size < MAX_PACKET - 10; i++)
      if (items[i].updated) {
        items[i].sending = true;
        byte k = formatDecimal(items[i].id, &packet[size], 2, FMT_SPACE | FMT_LEFT);
        size += k;
        packet[size++] = ',';
        k = formatDecimal(items[i].val, &packet[size], 5, items[i].prec | FMT_SPACE | FMT_LEFT);
        size += k;
        packet[size++] = '\n';
      }
    if (size == 0)
      return;  
    packet[size] = 0;  
    if (!sendPacket(size)) {
      // faled to send, will retry
      for (byte j = 0; j <= i; j++)
        items[j].sending = false;
      pushPeriod.interval(RETRY_INTERVAL);
      return; 
    }
    // Sent successfully
    pushPeriod.interval(NEXT_INTERVAL);
    for (byte j = 0; j <= i; j++)
      if (sending[j]) {
        items[j].sending = false;
        items[j].updated = false;
      }
  }
}

void push(byte id, int val, byte prec) {
  items[id].val = val;
  items[id].prec = prec;
  items[id].updated = true;
}

