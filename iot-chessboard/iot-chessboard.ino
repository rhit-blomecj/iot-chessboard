#include <WiFi.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"
#include "oled_functions.h"
#include "wifi_setup.h"
#include "global_preferences.h"

struct IndexPair { 
  int file_index;
  int rank_index;
};

struct Move {
  String start_tile;
  String end_tile;
};

String translateIndexesToFileAndRank(IndexPair pair){
  //promotion will not break this because the API auto promotes to Queen
  char file = 'a' + pair.file_index;
  char rank = '1' + pair.rank_index;

  char c_strng_tile [3] = "12";

  c_strng_tile [0] = file;
  c_strng_tile [1] = rank;

  String tile = String(c_strng_tile);

  Serial.println(tile);
  return tile;
}

IndexPair translateFileAndRankToIndexes(String tile){
  //promotion will currently break this
  const char * c_str_tile = tile.c_str();//should only be size 3 because it should be something like "a1"

  IndexPair pair;

  pair.file_index = (int) (c_str_tile[0] - 'a');
  pair.rank_index = (int) (c_str_tile[1] - '1');
  
  return pair;
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

  
  serializeJson(streamBoardState(lichessToken, "T5Gn5OKT", gameStream), Serial);

}

void loop() {

  //if we have a move to send send it
  if(Serial.available()){
    String move = Serial.readString();
    Serial.println("Sending Move: ");
    makeBoardMove(lichessToken, "T5Gn5OKT", move);
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
        // Serial.println("Ignoring non-JSON line: " + line);
        continue;
      }

      JsonDocument nextStreamedEvent;
      deserializeJson(nextStreamedEvent, line);
      Serial.print("Recieved Event: ");
      serializeJson(nextStreamedEvent, Serial);
      Serial.println();
    }
  }

  delay(2);

}



