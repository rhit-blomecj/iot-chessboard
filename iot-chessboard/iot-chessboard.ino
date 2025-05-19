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
  //auto promotes to Queen
  char file = 'a' + pair.file_index;
  char rank = '1' + pair.rank_index;

  char c_strng_tile [3] = "12";

  c_strng_tile [0] = file;
  c_strng_tile [1] = rank;

  String tile = String(c_strng_tile);

  return tile;
}

IndexPair translateFileAndRankToIndexes(String tile){
  //promotion will currently break this make sure before handing tile to this it pulls off the promotion char
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

  
  serializeJson(streamBoardState(lichessToken, "96UxhMfikxD3", gameStream), Serial);

}

void loop() {

  //if we have a move to send send it
  if(Serial.available()){
    String move = Serial.readString();
    Serial.println("Sending Move: ");
    makeBoardMove(lichessToken, "96UxhMfikxD3", move);
  }

  //if there is a move to recieve recieve it
  if(gameStream.connected()){
    while(gameStream.available()){
      String line;
      line = gameStream.readStringUntil('\n');
      line.trim();

      
      if (line.startsWith("{")) {
        JsonDocument nextStreamedEvent;
        deserializeJson(nextStreamedEvent, line);


        String moves_list = nextStreamedEvent["moves"].as<String>();
        int index_of_last_space = moves_list.lastIndexOf(' ') + 1;//if this returns -1 I get 0 so it is start of string
        Serial.println("Move: " + moves_list.substring(index_of_last_space));
        String move = moves_list.substring(index_of_last_space);
        IndexPair startIndexes = translateFileAndRankToIndexes(move.substring(0,2));
        IndexPair endIndexes = translateFileAndRankToIndexes(move.substring(2,4));
        Serial.printf("start_file_index: %d\nstart_rank_index: %d\nend_file_index: %d\nend_rank_index: %d\n", startIndexes.file_index, startIndexes.rank_index, endIndexes.file_index, endIndexes.rank_index);
        Serial.println("Translated back into UCI: " + translateIndexesToFileAndRank(startIndexes) + translateIndexesToFileAndRank(endIndexes));
         

        Serial.print("Recieved Event: ");
        serializeJson(nextStreamedEvent, Serial);
        Serial.println();
      }else{
        // line.trim();
        // Serial.println("Ignoring non-JSON line: " + line);
        continue;
      }

      
    }
  }

  delay(2);

}



