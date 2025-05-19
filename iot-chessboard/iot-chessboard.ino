#include <WiFi.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"
#include "oled_functions.h"
#include "wifi_setup.h"
#include "global_preferences.h"


String translateIndexesToFileAndRank(int file_index, int rank_index){
  //promotion will not break this because the API auto promotes to Queen
  char file = 'a' + file_index;
  char rank = '1' + rank_index;

  char c_strng_tile [3] = "12";

  c_strng_tile [0] = file;
  c_strng_tile [1] = rank;

  String tile = String(c_strng_tile);

  Serial.println(tile);
  return tile;
}

String translateFileAndRankToIndexes(String tile){
  //promotion will currently break this
  char c_str_tile [3] = tile.c_str();

  int file_index = (int) (c_str_tile[0] - 'a');
  int rank_index = (int) (c_str_tile[1] - '1');

  String tile = String(c_strng_tile);

  Serial.println(tile);
  return tile;
}


WiFiClientSecure gameStream;

String lichessToken;

void setup() {
  Serial.begin(115200);
  setupGlobalPreferences();

  setupOLED();

  // if stored wifi credentials don't work then run Bluetooth network setup
  String network_ssid = prefs.getString("network_ssid");
  String network_pw = prefs.getString("network_pw");

  
  if(!connectWifi(network_ssid, network_pw)){
    runBluetoothNetworkSetup();
  }

  // if stored lichess credentials don't work then run OAuth setup
  lichessToken = prefs.getString("lichess_token", "");
  String testedTokenResponse = testAccessToken(lichessToken)[lichessToken].as<String>();//if this is null then it is invalid idk what it looks like when expired but hopefully the same
  if(testedTokenResponse == "null"){
    runOAuthServer();
  }

  //runOAuthServer should have written to prefs so this will now have the correct data
  lichessToken = prefs.getString("lichess_token");

  
  serializeJson(streamBoardState(lichessToken, "p3qqmsZ9", gameStream), Serial);

}

void loop() {

  //if we have a move to send send it
  if(Serial.available()){
    String move = Serial.readString();
    Serial.println("Sending Move: ");
    makeBoardMove(lichessToken, "p3qqmsZ9", move);
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



