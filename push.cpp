#include <Client.h>
#include <Ethernet.h>
#include <Metro.h>

#include "push.h"
#include "config.h" // defines FEED_ID and API_KEY
#include "util.h"

#define INITIAL_INTERVAL 60000L // 1min
#define RETRY_INTERVAL   10000L // 10sec
#define NEXT_INTERVAL   300000L // 5min

#define MAX_PACKET 512

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 7, 40};
byte gateway[] = {192, 168, 7, 1};	
byte subnet[] = {255, 255, 255, 0};
byte server[] = {173, 203, 98, 29}; // api.pachube.com 

PushItem items[MAX_PUSH_ID];
boolean updated[MAX_PUSH_ID];
boolean sending[MAX_PUSH_ID];
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
    pushPeriod.interval(RETRY_INTERVAL);
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
  while (client.connected()) {
    if (client.available()) {
      char ch = client.read();
      if (eoln)
        continue;
      if (ch == '\r' || ch == '\n') {
        eoln = true;
        Serial.println();
      } else
        Serial.print(ch);
    }
  }
  if (!eoln)
    Serial.println("No response");
  client.stop();
  pushPeriod.interval(eoln ? NEXT_INTERVAL : RETRY_INTERVAL);
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
      if (updated[i]) {
        sending[i] = true;
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
    if (sendPacket(size)) {
      for (byte j = 0; j < MAX_PUSH_ID; j++)
        if (sending[j]) {
          sending[j] = false;
          updated[j] = false;
        }
    }
  }
}

void push(const PushItem& item) {
  items[item.id] = item;
  updated[item.id] = true;
}
