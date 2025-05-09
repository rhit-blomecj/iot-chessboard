#include <WiFi.h>
#include <ArduinoJson.h>
// #include <Preferences.h>

#include "arduino_secrets.h"
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"
#include "oled_functions.h"

// const String ranks[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
// const String files[8] = {"a", "b", "c", "d", "e", "f", "g", "h"}; 
//or
// const String board [8] [8] = {
//     {"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"},
//     {"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2"},
//     {"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3"},
//     {"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4"},
//     {"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5"},
//     {"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6"},
//     {"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7"},
//     {"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"}
//   };
//or
/*
char start_file = 'a'+start_index;
char start_rank = '0'+start_index;
*/

WiFiClientSecure gameStream;


void setup() {
  Serial.begin(115200);

  Serial.println(ESP.getFreeHeap());

  setupOLED();

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



