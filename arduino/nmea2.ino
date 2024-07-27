#include <M5AtomS3.h>
#include "WiFi.h"

#define RX_PIN      1
#define TX_PIN      2

char ssid[] = "robinetta";
char password[] = "1937oga315";
IPAddress ip(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

IPAddress mcg(224, 0, 0, 61);

WiFiServer server(80);

const byte maxClients = 20;
WiFiClient client[maxClients];
boolean inUse[maxClients];

// WiFiUDP mc();


const byte numChars = 255;
char receivedChars[numChars]; // an array to store the received data

boolean newData = false;
byte ndx = 0;

void recvWithEndMarker(char rc) {
  const char endMarker = '\n';
  if (rc != endMarker) {
    receivedChars[ndx] = rc;
    ndx++;
    if (ndx >= numChars) {
      ndx = numChars - 1;
    }
  } else {
    receivedChars[ndx] = '\0'; // terminate the string
    newData = true;
  }
}

boolean stillConnected(WiFiClient client) {
  if (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial2.write(c);
    }
    return true;
  } else {
    delay(1);
    client.stop();
    Serial.println("client disconnected");
    return false;
  }
}

void setup()
{
  AtomS3.begin(true);
  AtomS3.dis.setBrightness(100);  
  AtomS3.dis.drawpix(0xff0000);
  AtomS3.update();
  Serial.begin(38400);
  Serial.println("Ready");

  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.begin();
  server.begin();

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  Serial2.begin(38400, SERIAL_8N1, RX_PIN, TX_PIN);

  AtomS3.dis.drawpix(0x00ff00);
  AtomS3.update();

  for(byte i = 0; i < maxClients; i++) inUse[i] = false;

  ndx = 0;
  newData = false;

  // uint8_t x = mc.beginMulticast(IPAddress a, 80);
}

void loop()
{
  delay(100);
  while(Serial2.available()) {
    char c = Serial2.read();
    recvWithEndMarker(c);
  }
  WiFiClient nClient = server.available();
  if (nClient) {
    Serial.println("new client");
    boolean stored = false;
    for(byte i = 0; i < maxClients; i++) {
      if (inUse[i] == false) {
        client[i] = nClient;
        inUse[i] = true;
        stored = true;
        break;
      }
    }
    if (!stored) {
      Serial.println("too many clients");
      nClient.println("please try again later");
      nClient.stop();
    }
  }
  for(byte i = 0; i < maxClients; i++) {
    if (inUse[i]) {
      boolean ok = stillConnected(client[i]);
      if (ok) {
        if (newData) {
          client[i].println(receivedChars);
        }
      } else {
        inUse[i] = false;
      }
    }
  }
  // ready for next message
  if (newData) {
    ndx = 0;
    newData = false;
  }
}
