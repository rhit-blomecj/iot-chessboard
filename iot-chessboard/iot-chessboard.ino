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
bool isUsersTurn;

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

  Serial.println(lichessToken);
  accountId = testAccessToken(lichessToken)[lichessToken]["userId"].as<String>();
  // accountId = getAccountInfo(lichessToken)["id"].as<String>();
  Serial.println("Account ID: " + accountId);


  gameId = prefs.getString("game_id");

  

  //this enables interrupts and to avoid weird behavior I think I should put this at the bottom so the interrupts are only enabled during a game?
  setupMCP();//I could also just run the disable under this function and enable them when I want them enabled

  //NEOPIXEL SETUP GOES HERE IF NECESSARY
  neoPixelSetup();
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
      open_game = reinitializeGameStream();
      
      
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
  clearAllPixels();
  if(lastMove.length() != 0){
    displayMoveOnNeoPixel(lastMove, GREEN);
  }

  if(isUsersTurn){//this means it is the users turn to move but we need the user to move the pieces so it matches the websites so we have to wait for them to do that
    Serial.println("It's Your Turn");
    disableInterruptsUntilButtonPress();
  } else{// your move was the last move so you are just waiting for the oponent to get on with it
    disableAllMCPInterrupts();
    Serial.println("It's Not Your Turn");
  }
  

  gameState = RUNNING;

  while(gameState == RUNNING){

    
    //if we have a move to send send it
    String move = getMoveFromHardware();
    Serial.println("past get Move From Hardware: " + move);
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
  Serial.println("startgetLastMove");
  int index_of_last_space = moves_list.lastIndexOf(' ') + 1;//if this returns -1 I get 0 so it is start of string
  String lastMove = moves_list.substring(index_of_last_space);
  Serial.println("getLastMove End: "+ lastMove);
  return lastMove;
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

          //should toggle whose turn it is
          //if this for some reason causes a bug we can instead calculate it again by calling setIsUsersTurn(String moves);
          isUsersTurn = !isUsersTurn;
          
          lastMove = getLastMove(moves_list);
          Serial.println("Move: " + lastMove);

          clearAllPixels();
          //display Valid Move
          displayMoveOnNeoPixel(lastMove, GREEN);

          if(nextStreamedEvent["status"].as<String>() != "started"){
            gameState = GAME_OVER;
          }
          
          if(isUsersTurn){//oponent was last to move so you need to update their side of the board then hit press button
            enableInterruptsOnButtonPress();
          } else {//was your move that was last streamed disable the interrupts until it is your turn again then update enemies side of board and press button to reenable interrupts
            disableAllMCPInterrupts();
          }

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

  setUsersColor(open_game);

  String moves = open_game["state"]["moves"].as<String>();

  setIsUsersTurn(moves);

  setHasKingMovedThisGameFromMovesString(moves);
  return open_game;
}

void setUsersColor(JsonDocument open_game){
  String whitePlayerId = open_game["white"]["id"].as<String>();
  String blackPlayerId = open_game["black"]["id"].as<String>();

  Serial.print("setUsersColor: ");
  if(whitePlayerId == accountId){
    color = LIGHT;
    Serial.println("WHITE");
  } else if(blackPlayerId == accountId){
    color = DARK;
    Serial.println("BLACK");
  }
}

void setIsUsersTurn(String moves){
  Serial.println("setIsUsersTurn running");
  if(color == LIGHT){
    isUsersTurn = true;
  } else if (color == DARK){
    isUsersTurn = false;
  }

  // if moves string is empty we just set correct values
  if(moves.length() == 0){
    return;
  }

  String tempMoves = moves;
  int index = 0;
  //guarunteed to execute once because we know the string is not empty
  do {//toggle
    tempMoves = tempMoves.substring(index);//this goes at top and we make index starts as 0 so I'm not trying to substring a negative index
    Serial.println(tempMoves);
    isUsersTurn = !isUsersTurn;
    index = tempMoves.indexOf(" ") + 1;
  } while(index != 0);//indexOf returns -1 plus 1 gives 0
}

void setHasKingMovedThisGameFromMovesString(String moves){
  if(color == LIGHT){
    hasKingMovedThisGame = (moves.indexOf("e1") != -1);
  } else if(color == DARK){
    hasKingMovedThisGame = (moves.indexOf("e8") != -1);
  }

  Serial.print("setHasKingMovedThisGameFromMovesString: ");
  Serial.println(hasKingMovedThisGame);
}

bool isCastleMove(String move){
  if(color == LIGHT){
    return (!hasKingMovedThisGame && ((move == "e1g1") || (move == "e1c1")));
  } else if(color == DARK){
    return (!hasKingMovedThisGame && ((move == "e8g8") || (move == "e8c8")));
  }
}

void enableInterruptsOnButtonPress(){
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

void disableInterruptsUntilButtonPress(){
  disableAllMCPInterrupts();
  enableInterruptsOnButtonPress();
}


void displayMoveOnNeoPixel(String move, uint32_t color){
  String lastMoveStartTile = move.substring(0,2);
  String lastMoveEndTile = move.substring(2,4);
  setPixelColor(translateFileAndRankToNeoPixel(lastMoveStartTile), color);
  setPixelColor(translateFileAndRankToNeoPixel(lastMoveEndTile), color);
  Serial.println("Light Up NeoPixels Please");
}
