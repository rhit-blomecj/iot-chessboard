#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>  
#include <Preferences.h>             
#include "HT_SSD1306Wire.h"

#include "arduino_secrets.h"
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"

const String ranks[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
const String files[8] = {"a", "b", "c", "d", "e", "f", "g", "h"}; 
//or
const String board [8] [8] = {
    {"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"},
    {"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2"},
    {"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3"},
    {"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4"},
    {"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5"},
    {"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6"},
    {"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7"},
    {"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"}
  };

WiFiClientSecure gameStream;

//OLED declaration
static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

// Turn Vext ON (powers OLED)
void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

// Turn Vext OFF (default)
void VextOFF(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}

void setup() {
  Serial.begin(115200);

  //OLED setup
  VextON();
  delay(100);
  // Initialising the UI will init the display too.
  display.init();
  delay(200);
  // defined font sizes in library are 10, 16, and 24
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "IoT Chessboard");
  display.display();
  delay(1000);
  VextOFF();


  runBluetoothNetworkSetup();

  runOAuthServer();

  
  serializeJson(streamBoardState(token, "wVJpJMuD", gameStream), Serial);

}

void loop() {

  //if we have a move to send send it
  if(Serial.available()){
    String move = Serial.readString();
    Serial.println("Sending Move: ");
    makeBoardMove(token, "wVJpJMuD", move);
  }

  //if there is a move to recieve recieve it
  if(gameStream.connected()){
    while(gameStream.available()){
      String line;
      line = gameStream.readStringUntil('\n');
      line.trim();

      if (line.length() == 0 || line == "1" || !line.startsWith("{")) {
        line = gameStream.readString();
        line.trim();
        Serial.println("Ignoring non-JSON line: " + line);
        continue;
      }

      JsonDocument nextStreamedEvent;
      deserializeJson(nextStreamedEvent, line);
      Serial.print("Recieved Move: ");
      serializeJson(nextStreamedEvent, Serial);
      Serial.println();
    }
  }

  delay(200);
  
}



