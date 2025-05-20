#include <WiFi.h>
#include <ArduinoJson.h>

#include "mcp_logic.h"
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"
#include "oled_functions.h"
#include "wifi_setup.h"
#include "global_preferences.h"

#define PRG_BTN       0       // user button (PRG) pin

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

int translateIndexesToNeoPixel(IndexPair pair){
  const int FILE_WIDTH = 8;
  if(pair.rank_index%2 == 0){
    return (pair.rank_index * FILE_WIDTH + pair.file_index);
  } else{
    return (pair.rank_index * FILE_WIDTH + (FILE_WIDTH - pair.file_index));
  }
}

//in the future may split further to show winners
enum GameState {
  RUNNING,
  GAME_OVER
};

enum GameState gameState = GAME_OVER;

WiFiClientSecure gameStream;

String lichessToken;
String gameId;

String lastMove = "";

void setup() {
  Serial.begin(115200);
  setupGlobalPreferences();

  setupOLED();

  pinMode(PRG_BTN, INPUT);//setup start game button using this may compromise seeding our random number generator but that can be a future me problem

  // if stored wifi credentials don't work then run Bluetooth network setup
  String network_ssid = prefs.getString("network_ssid");
  String network_pw = prefs.getString("network_pw");
  
  if(!connectWifi(network_ssid, network_pw)){
    runBluetoothNetworkSetup();
  }

  // if stored lichess credentials don't work then run OAuth setup
  lichessToken = prefs.getString("lichess_token");
  String testedTokenResponse = testAccessToken(lichessToken)[lichessToken].as<String>();//if this is null then it is invalid idk what it looks like when expired but hopefully the same
  if(testedTokenResponse == "null"){
    runOAuthServer();
  }

  //runOAuthServer should have written to prefs so this will now have the correct data
  lichessToken = prefs.getString("lichess_token");

  gameId = prefs.getString("game_id");
}

void loop() {
  gameStream.flush();
  gameStream.stop();
  JsonDocument open_game = streamBoardState(lichessToken, gameId, gameStream);
  serializeJson(open_game, Serial);
  Serial.println();

  while(open_game["state"]["status"].as<String>() != "started"){//if game is ended start a new one
    gameStream.flush();
    gameStream.stop();

    if (digitalRead(PRG_BTN) == LOW) {
      delay(5); // debounce

      //create a challenge
      String body = "level=1";
      gameId = challengeTheAI(lichessToken, body)["id"].as<String>();

      //this should have updated gameId
      open_game = streamBoardState(lichessToken, gameId, gameStream);

      serializeJson(open_game, Serial);
      Serial.println();
      
      
      prefs.putString("game_id", gameId);
      // wait for USER release
      while (digitalRead(PRG_BTN) == LOW);
    }
  }

  //we should have correct open_game so we can pull the last move from the game here
  String moves_list = open_game["state"]["moves"].as<String>();
  lastMove = getLastMove(moves_list);

  Serial.println("In Setup from gameStream Last Move was: " + lastMove);

  gameState = RUNNING;

  while(gameState == RUNNING){
    //if we have a move to send send it
    if(Serial.available()){
      String move = Serial.readString();
      Serial.println("Sending Move: ");
      makeBoardMove(lichessToken, gameId, move);
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
          
          lastMove = getLastMove(moves_list);
          Serial.println("Move: " + lastMove);

          if(nextStreamedEvent["status"].as<String>() != "started"){
            gameState = GAME_OVER;
          }
          

          //parse from website to indexPairs and test values This is where we would send stuff to the hard ware
          String promoteTo = "";
          if(lastMove.length() > 4) {//promotion occured
            promoteTo = lastMove.substring(4,5);
          }

          IndexPair startIndexes = translateFileAndRankToIndexes(lastMove.substring(0,2));
          IndexPair endIndexes = translateFileAndRankToIndexes(lastMove.substring(2,4));
          Serial.printf("start_file_index: %d\nstart_rank_index: %d\nend_file_index: %d\nend_rank_index: %d\n", startIndexes.file_index, startIndexes.rank_index, endIndexes.file_index, endIndexes.rank_index);
          Serial.println("Translated back into UCI: " + translateIndexesToFileAndRank(startIndexes) + translateIndexesToFileAndRank(endIndexes));
          Serial.printf("Translated to NeoPixel Start: %d\nTranslated to NeoPixel End: %d\n", translateIndexesToNeoPixel(startIndexes), translateIndexesToNeoPixel(endIndexes));
          

          Serial.print("Recieved Event: ");
          serializeJson(nextStreamedEvent, Serial);
          Serial.println();
        }else{
          continue;
        }

        
      }
    }

    delay(2);
  }

}

String getLastMove(String moves_list){
  int index_of_last_space = moves_list.lastIndexOf(' ') + 1;//if this returns -1 I get 0 so it is start of string
  return moves_list.substring(index_of_last_space);
}



