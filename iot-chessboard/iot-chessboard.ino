#include <WiFi.h>
#include <ArduinoJson.h>

#include "mcp_logic.h"
#include "lichess_api.h"
#include "bluetooth_wifi_setup.h"
#include "oled_functions.h"
#include "wifi_setup.h"
#include "global_preferences.h"
#include "neo_pixel.h"

#define PRG_BTN       0       // user button (PRG) pin

//in the future may split further to show winners
enum GameState {
  RUNNING,
  GAME_OVER
};

enum Color {
  LIGHT,
  DARK
};

enum Color color;
bool hasKingMovedThisGame;

enum GameState gameState = GAME_OVER;

WiFiClientSecure gameStream;

String lichessToken;
String gameId;

String lastMove = "";

String accountId;

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

  accountId = getAccountInfo(lichessToken)["id"].as<String>();
  Serial.println("Account ID: " + accountId);

  //this enables interrupts and to avoid weird behavior I think I should put this at the bottom so the interrupts are only enabled during a game?
  setupMCP();//I could also just run the disable under this function and enable them when I want them enabled

  //NEOPIXEL SETUP GOES HERE IF NECESSARY
}

void loop() {
  JsonDocument open_game = reinitializeGameStream();

  while(open_game["state"]["status"].as<String>() != "started"){//if game is ended start a new one
    if (digitalRead(PRG_BTN) == LOW) {
      delay(5); // debounce

      //create a challenge
      String body = "level=1";
      gameId = challengeTheAI(lichessToken, body)["id"].as<String>();

      //this should update gameId
      reinitializeGameStream();
      
      
      prefs.putString("game_id", gameId);
      // wait for USER release
      while (digitalRead(PRG_BTN) == LOW);
    }
  }

  //we should have correct open_game so we can pull the last move from the game here
  String moves_list = open_game["state"]["moves"].as<String>();
  lastMove = getLastMove(moves_list);
  Serial.println("Last Move in Game was: " + lastMove);

  //highlight last move in game
  displayMoveOnNeoPixel(lastMove, GREEN);

  //will probably need to have a way to determine if this was your or your oponents move so we know how to handle it
  //I could move through the moves string switching between white and blacks turns until I know whose turn it is (we should already know our color at this point so we would know whose turn it is then)
  //^ not super elegant but I know it would work
  //if(lastMove was Oponents){//this means it is the users turn to move but we need the user to move the pieces so it matches the websites so we have to wait for them to do that
  //  disable interrupts and wait for user to hit button to reenable then continue game
  //} else{// your move was the last move so you are just waiting for the oponent to get on with it
  //  leave interrupts enabled (or enable them if they don't start that way) and continue to the normal game loop waiting for oponent to move
  //  could potentially disable interrupts until next oponnents move recieved that way no weird behaviour can occur while you shouldn't be moving your pieces anyway (if we disable interrupts here we just need to wait for button press to enable them again in the game loop because we need to wait for your oponnents move to be streamed)
  //}
  

  gameState = RUNNING;

  while(gameState == RUNNING){

    
    //if we have a move to send send it
    String move = getMoveFromHardware();
    if (move.length() > 0 ){
      Serial.println("Sending Move: ");
      JsonDocument makeMoveResponse = makeBoardMove(lichessToken, gameId, move);
      bool isValidMove = makeMoveResponse["ok"].as<bool>();//if its valid ok holds true if its not I think I would get null which hopefully is false
      if(!isValidMove){
        clearAllPixels();
        //display Invalid Move
        displayMoveOnNeoPixel(move, RED);
        //disable interrupts to allow fixing your boardstate
        disableInterruptsUntilButtonPress();

      } else {//move was valid
        clearAllPixels();
        //display Valid Move
        displayMoveOnNeoPixel(move, GREEN);

        if(isCastleMove(move)){//if king has not moved this game and it is a move that would be a castle assuming the rook is in the correct spot (we know the king is in the right spot bc it hasn't moved this game)
          //ignore next two interrupts to allow finishing the castle move
          ignoreInterruptCastle();// We need to get the user instructions that after moving the king for a castle to wait until lichess responds with valid or invalid and if it is valid then you can move the rook
        }
      }
    }

    //if there is a move to recieve recieve it
    recieveMoveFromGameStream();

    delay(2);
  }

}

String getLastMove(String moves_list){
  int index_of_last_space = moves_list.lastIndexOf(' ') + 1;//if this returns -1 I get 0 so it is start of string
  return moves_list.substring(index_of_last_space);
}


void recieveMoveFromGameStream(){
  if(gameStream.connected()){
      while(gameStream.available()){
        String line;
        line = gameStream.readStringUntil('\n');
        line.trim();

        
        if (line.startsWith("{")) {
          JsonDocument nextStreamedEvent;
          deserializeJson(nextStreamedEvent, line);


          String moves_list = nextStreamedEvent["moves"].as<String>();

          setHasKingMovedThisGameFromMovesString(moves_list);//after every move update on website if your king has moved this game or not
          
          lastMove = getLastMove(moves_list);
          Serial.println("Move: " + lastMove);

          clearAllPixels();
          //display Valid Move
          displayMoveOnNeoPixel(lastMove, GREEN);

          if(nextStreamedEvent["status"].as<String>() != "started"){
            gameState = GAME_OVER;
          }
          
          // if(Opponents Move was last move){
          //   disable interrupts until button pressed so player can update their board
          // } else {//was your move that was last streamed
          //   do nothing just let this carry on so we can wait for oponents move
          //   could potentially disable interrupts until next oponnents move recieved that way no weird behaviour can occur while you shouldn't be moving your pieces anyway (if we disable interrupts here we don't need to disable interrupts in the top of this if we just need to wait for button press to enable them again)
          // }

          //
          String promoteTo = "";
          if(lastMove.length() > 4) {//promotion occured
            promoteTo = lastMove.substring(4,5);
          }

          

          Serial.print("Recieved Event: ");
          serializeJson(nextStreamedEvent, Serial);
          Serial.println();
        }else{
          continue;
        }
      }
    }
}

JsonDocument reinitializeGameStream(){
  gameStream.flush();
  gameStream.stop();
  JsonDocument open_game = streamBoardState(lichessToken, gameId, gameStream);

  serializeJson(open_game, Serial);
  Serial.println();

  setPlayersColor(open_game);

  setHasKingMovedThisGameFromMovesString(open_game["state"]["moves"].as<String>());
  return open_game;
}

void setPlayersColor(JsonDocument open_game){
  if(open_game["white"]["id"].as<String>() == accountId){
    color = LIGHT;
  } else if(open_game["black"]["id"].as<String>() == accountId){
    color = DARK;
  }

  Serial.println("setPlayersColor: " + color);
}

void setHasKingMovedThisGameFromMovesString(String moves){
  if(color == LIGHT){
    hasKingMovedThisGame = (moves.indexOf("e1") == -1);
  } else if(color == DARK){
    hasKingMovedThisGame = (moves.indexOf("e8") == -1);
  }

  Serial.println("setHasKingMovedThisGameFromMovesString: " + hasKingMovedThisGame);
}

bool isCastleMove(String move){
  if(color == LIGHT){
    return (!hasKingMovedThisGame && ((move == "e1g1") || (move == "e1c1")));
  } else if(color == DARK){
    return (!hasKingMovedThisGame && ((move == "e8g8") || (move == "e8c8")));
  }
}

void disableInterruptsUntilButtonPress(){
  disableAllMCPInterrupts();
  
  while(true){//wait until button pushed to reenable interrupts
    if (digitalRead(PRG_BTN) == LOW) {
      delay(5); // debounce
      //enable interrupts after button press
      enableAllMCPInterrupts();
      // wait for USER release
      while (digitalRead(PRG_BTN) == LOW);
      break;
    }
  }
}


void displayMoveOnNeoPixel(String move, uint32_t color){
  String lastMoveStartTile = move.substring(0,2);
  String lastMoveEndTile = move.substring(2,4);
  setPixelColor(translateFileAndRankToNeoPixel(lastMoveStartTile), color);
  setPixelColor(translateFileAndRankToNeoPixel(lastMoveEndTile), color);
}
