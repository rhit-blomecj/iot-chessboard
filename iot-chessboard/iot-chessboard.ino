#include <WiFi.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <Preferences.h>
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"

WiFiClientSecure gameStream;

void setup() {
  Serial.begin(115200);

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



