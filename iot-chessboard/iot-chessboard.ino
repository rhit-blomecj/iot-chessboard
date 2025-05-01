#include <WiFi.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <Preferences.h>
#include "lichess_api.h"

WiFiClientSecure gameStream;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(SECRET_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  runOAuthServer();

  serializeJson(makeBoardMove(token, "I2Bs80oaf1aB", "e1d1"), Serial);
  serializeJson(streamBoardState(token, "I2Bs80oaf1aB", gameStream), Serial);

}

void loop() {
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
      serializeJson(nextStreamedEvent, Serial);
      Serial.println();
    }
  }

  delay(200);
  
}



