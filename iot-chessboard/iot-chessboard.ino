#include <WiFi.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <Preferences.h>
#include "lichess_api.h"
#include "bluetooth_serial_wifi_setup.h"
// #include "wifi_setup.h"

WiFiClientSecure gameStream;

void setup() {
  Serial.begin(115200);
  Serial.println(ESP.getFreeHeap());

  runBluetoothNetworkSetup();
  // connectWifi("RHIT-OPEN", "");

  runOAuthServer();

  
  serializeJson(streamBoardState(token, "1gf7S4jn", gameStream), Serial);

}

void loop() {

  if(Serial.available()){
    String move = Serial.readString();
    Serial.println("Value Read From Serial" + move);
    Serial.print("Sending Move: ");
    serializeJson(makeBoardMove(token, "1gf7S4jn", move), Serial);
    Serial.println();
  }

  // put your main code here, to run repeatedly:
  if(gameStream.connected()){
    while(gameStream.available()){
      // Serial.println("gameStream in .ino file available.");
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



