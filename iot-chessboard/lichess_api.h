#ifndef lichess_api
#define lichess_api

  #include <WiFi.h>
  #include <NetworkClient.h>
  #include "WiFiClientSecure.h"
  #include <WebServer.h>
  #include <ArduinoJson.h>
  #include <base64.h>
  #include <Crypto.h>
  #include <SHA256.h>

  #include "oled_functions.h"
  #include "global_preferences.h"

  const char* lichess_root_ca = R"EOF(
  -----BEGIN CERTIFICATE-----
  MIIEVzCCAj+gAwIBAgIRALBXPpFzlydw27SHyzpFKzgwDQYJKoZIhvcNAQELBQAw
  TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
  cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw
  WhcNMjcwMzEyMjM1OTU5WjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg
  RW5jcnlwdDELMAkGA1UEAxMCRTYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAATZ8Z5G
  h/ghcWCoJuuj+rnq2h25EqfUJtlRFLFhfHWWvyILOR/VvtEKRqotPEoJhC6+QJVV
  6RlAN2Z17TJOdwRJ+HB7wxjnzvdxEP6sdNgA1O1tHHMWMxCcOrLqbGL0vbijgfgw
  gfUwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcD
  ATASBgNVHRMBAf8ECDAGAQH/AgEAMB0GA1UdDgQWBBSTJ0aYA6lRaI6Y1sRCSNsj
  v1iU0jAfBgNVHSMEGDAWgBR5tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcB
  AQQmMCQwIgYIKwYBBQUHMAKGFmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0g
  BAwwCjAIBgZngQwBAgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVu
  Y3Iub3JnLzANBgkqhkiG9w0BAQsFAAOCAgEAfYt7SiA1sgWGCIpunk46r4AExIRc
  MxkKgUhNlrrv1B21hOaXN/5miE+LOTbrcmU/M9yvC6MVY730GNFoL8IhJ8j8vrOL
  pMY22OP6baS1k9YMrtDTlwJHoGby04ThTUeBDksS9RiuHvicZqBedQdIF65pZuhp
  eDcGBcLiYasQr/EO5gxxtLyTmgsHSOVSBcFOn9lgv7LECPq9i7mfH3mpxgrRKSxH
  pOoZ0KXMcB+hHuvlklHntvcI0mMMQ0mhYj6qtMFStkF1RpCG3IPdIwpVCQqu8GV7
  s8ubknRzs+3C/Bm19RFOoiPpDkwvyNfvmQ14XkyqqKK5oZ8zhD32kFRQkxa8uZSu
  h4aTImFxknu39waBxIRXE4jKxlAmQc4QjFZoq1KmQqQg0J/1JF8RlFvJas1VcjLv
  YlvUB2t6npO6oQjB3l+PNf0DpQH7iUx3Wz5AjQCi6L25FjyE06q6BZ/QlmtYdl/8
  ZYao4SRqPEs/6cAiF+Qf5zg2UkaWtDphl1LKMuTNLotvsX99HP69V2faNyegodQ0
  LyTApr/vT01YPE46vNsDLgK+4cL6TrzC/a4WcmF5SRJ938zrv/duJHLXQIku5v0+
  EwOy59Hdm0PT/Er/84dDV0CSjdR/2XuZM3kpysSKLgD1cKiDA+IRguODCxfO9cyY
  Ig46v9mFmBvyH04=
  -----END CERTIFICATE-----
  )EOF";

  char validPKCEChars [67] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

  char apiBaseUrl [] = "lichess.org";

  WiFiClientSecure apiClient;
  WebServer server(80);

  String code_verifier = "ThisIsARandomStringWithoutAnythingWeirdGoingOnWithItNothingToSeeHereIHopeThisWorks";
  String state = "StateShouldStayTheSameAcrossCalls";
  String code = "";
  String token = "";

  bool authenticated = false;

  String recievedState = "";

  void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
  }

  String prepareCodeChallenge(String input){
      SHA256 sha256;
      uint8_t hash[32];
      // Step 1: Compute SHA-256
      sha256.reset();
      sha256.update((const uint8_t*)input.c_str(), input.length());
      sha256.finalize(hash, sizeof(hash));

      // Step 2: Base64 encode the hash
      String b64 =  base64::encode(hash, 32);

      // Step 3: Convert to Base64-URL format
      b64.replace("+", "-");
      b64.replace("/", "_");
      b64.replace("=", ""); // Remove padding

      return b64;
  }


  void handleLichessOAuth(){
    /*
    OAUTH Scopes: from
      preference:read - Read your preferences
      preference:write - Write your preferences
      email:read - Read your email address
      engine:read - Read your external engines
      engine:write - Create, update, delete your external engines
      challenge:read - Read incoming challenges
      challenge:write - Create, accept, decline challenges
      challenge:bulk - Create, delete, query bulk pairings
      study:read - Read private studies and broadcasts
      study:write - Create, update, delete studies and broadcasts
      tournament:write - Create tournaments
      racer:write - Create and join puzzle races
      puzzle:read - Read puzzle activity
      team:read - Read private team information
      team:write - Join, leave teams
      team:lead - Manage teams (kick members, send PMs)
      follow:read - Read followed players
      follow:write - Follow and unfollow other players
      msg:write - Send private messages to other players
      board:play - Play with the Board API
      bot:play - Play with the Bot API. Only for Bot accounts
      web:mod - Use moderator tools (within the bounds of your permissions)
    */
    server.sendHeader("Location", "https://" + String(apiBaseUrl) + "/oauth?response_type=code&client_id=chessboard&redirect_uri=http://" + WiFi.localIP().toString() + "/callback&code_challenge_method=S256&code_challenge=" + prepareCodeChallenge(code_verifier) + "&scope=challenge:read challenge:write board:play" + "&state=" + state);
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);//redirect to lichess oauth call
  }

  /*This endpoint's body is application/x-www-form-urlencoded*/
  extern JsonDocument obtainAccessToken(String body) {
    Serial.println("\nStarting connection to server...");
    apiClient.setInsecure();//should use certificate but I just wanna see if this will work
    // apiClient.setCACert(lichess_root_ca);

    // Connect to Lichess server and send API request
    apiClient.connect(apiBaseUrl, 443);
    if (apiClient.connected()) {
      Serial.println("connected to server");

      // Make a HTTP request:
      apiClient.println("POST /api/token HTTP/1.1");   // POST request
      
      apiClient.println(String("Host: ") + apiBaseUrl);
      apiClient.println("Connection: close");
      apiClient.println("Content-Type: application/x-www-form-urlencoded");
      apiClient.println(String("Content-Length: ") + body.length());
      apiClient.println();
      
      apiClient.println(body);
    } else {
      Serial.println("unable to connect");
    }

    delay(1000);

    //  Parse returned JSON and store values in global variables
    JsonDocument oAuthGrantObject;
    String line = "";
    if (apiClient.connected()) {
      apiClient.readStringUntil('{');
      line = apiClient.readStringUntil('\n');
      line = "{" + line;

      Serial.println(line);

      deserializeJson(oAuthGrantObject, line);
    }

    return oAuthGrantObject;//this will be uninitialized if apiClient.connected() is false
  }

  /*This endpoint's body is text/plain with comma separated OAuth tokens*/
  extern JsonDocument testAccessToken(String body) {
    Serial.println("\nStarting connection to server...");
    apiClient.setInsecure();//should use certificate but I just wanna see if this will work
    // apiClient.setCACert(lichess_root_ca);
    // Connect to OpenWeather server and send API request
    apiClient.connect(apiBaseUrl, 443);
    if (apiClient.connected()) {
      Serial.println("connected to server");

      // Make a HTTP request:
      apiClient.println("POST /api/token/test HTTP/1.1");   // POST request
      
      apiClient.println(String("Host: ") + apiBaseUrl);
      apiClient.println("Connection: close");
      apiClient.println("Content-Type: text/plain");
      apiClient.println(String("Content-Length: ") + body.length());
      apiClient.println();
      
      apiClient.println(body);
    } else {
      Serial.println("unable to connect");
    }

    delay(1000);

    //  Parse returned JSON and store values in global variables
    JsonDocument response;
    String line = "";
    if (apiClient.connected()) {
      apiClient.readStringUntil('{');
      line = apiClient.readStringUntil('\n');
      line = "{" + line;

      Serial.println(line);

      
      deserializeJson(response, line);
    }

    return response;
  }

  /*This endpoint doesn't have a body*/
  extern JsonDocument makeBoardMove(String authToken, String gameId, String move) {
    Serial.println("\nStarting connection to server...");
    apiClient.setInsecure();//should use certificate but I just wanna see if this will work
    // apiClient.setCACert(lichess_root_ca);
    // Connect to OpenWeather server and send API request
    apiClient.connect(apiBaseUrl, 443);
    if (apiClient.connected()) {
      Serial.println("connected to server");

      // Make a HTTP request:
      apiClient.println("POST /api/board/game/" + gameId + "/move/" + move + " HTTP/1.1");   // POST request
      
      apiClient.println(String("Host: ") + apiBaseUrl);
      apiClient.println("Connection: close");
      apiClient.println("Authorization: Bearer " + authToken);
      // apiClient.println(String("Content-Length: ") + body.length());
      apiClient.println();
      
      // apiClient.println(body);
    } else {
      Serial.println("unable to connect");
    }

    delay(1000);

    //  Parse returned JSON and store values in global variables
    JsonDocument response;
    String line = "";
    if (apiClient.connected()) {
      apiClient.readStringUntil('{');
      line = apiClient.readStringUntil('\0');
      line = "{" + line;

      Serial.println(line);

      
      deserializeJson(response, line);
    }

    return response;
  }

 /*This endpoint's body is application/x-www-form-urlencoded*/
  extern JsonDocument challengeTheAI(String authToken, String body) {//hard coding this to be against a level 1 ai
    Serial.println("\nStarting connection to server...");
    apiClient.setInsecure();//should use certificate but I just wanna see if this will work
    // apiClient.setCACert(lichess_root_ca);

    // Connect to Lichess server and send API request
    apiClient.connect(apiBaseUrl, 443);
    if (apiClient.connected()) {
      Serial.println("connected to server");

      // Make a HTTP request:
      apiClient.println("POST /api/challenge/ai HTTP/1.1");   // POST request
      
      apiClient.println(String("Host: ") + apiBaseUrl);
      apiClient.println("Connection: close");
      apiClient.println("Authorization: Bearer " + authToken);
      apiClient.println("Content-Type: application/x-www-form-urlencoded");
      apiClient.println(String("Content-Length: ") + body.length());
      apiClient.println();
      
      apiClient.println(body);
    } else {
      Serial.println("unable to connect");
    }

    delay(1000);

    //  Parse returned JSON and store values in global variables
    JsonDocument response;
    String line = "";
    if (apiClient.connected()) {
      apiClient.readStringUntil('{');
      line = apiClient.readStringUntil('\n');
      line = "{" + line;

      Serial.println(line);

      deserializeJson(response, line);
    }

    return response;
  }

  /*This endpoint doesn't have a body*/
  extern JsonDocument streamBoardState(String authToken, String gameId, WiFiClientSecure &gameStreamClient) {
    Serial.println("\nStarting connection to server...");
    gameStreamClient.setInsecure();//should use certificate but I just wanna see if this will work
    // gameStreamClient.setCACert(lichess_root_ca);
    // Connect to OpenWeather server and send API request
    gameStreamClient.connect(apiBaseUrl, 443);
    if (gameStreamClient.connected()) {
      Serial.println("connected to server");

      // Make a HTTP request:
      gameStreamClient.println("GET /api/board/game/stream/" + gameId + " HTTP/1.1");   // POST request
      
      gameStreamClient.println(String("Host: ") + apiBaseUrl);
      gameStreamClient.println("Connection: keep-alive");
      gameStreamClient.println("Authorization: Bearer " + authToken);
      gameStreamClient.println("Accept: application/x-ndjson");
      // apiClient.println(String("Content-Length: ") + body.length());
      gameStreamClient.println();
      
      // apiClient.println(body);
    } else {
      Serial.println("unable to connect");
    }

    // delay(1000);

    //  Parse returned JSON and store values in global variables
    JsonDocument response;
    String line = "";
    if (gameStreamClient.connected()) {
      Serial.println("gameStreamClient inside lichess_api function is connected.");

      gameStreamClient.readStringUntil('{');
      line = gameStreamClient.readStringUntil('\n');
      line = "{" + line;

      
      deserializeJson(response, line);
    }

    return response;
  }

  //root page can be accessed only if authentication is ok
  void handleLichessOAuthCallBack() {

    code = server.arg(String("code"));
    recievedState = server.arg(String("state"));

    String body = String("grant_type=authorization_code") + 
            "&code=" + code +
            "&code_verifier=" + code_verifier +
            "&redirect_uri=http%3A%2F%2F" + WiFi.localIP().toString() + "%2Fcallback" +
            "&client_id=chessboard";

    JsonDocument response = obtainAccessToken(body);

    token = response["access_token"].as<String>();

    prefs.putString("lichess_token", token);
    server.send(200);
    authenticated = true;
  }

  //modified code from here https://forum.arduino.cc/t/randomize-string/600620
  String generateRandomString(int stringLength){
    int generated = 0;
    String generatedString = "";

    while (generated < stringLength)
    {
      byte randomValue = random(0, 66);//there are 66 valid PKCE characters
      char letter = validPKCEChars[randomValue];
      generatedString += letter;
      generated ++;
    }

    Serial.println(generatedString);
    return generatedString;
  }

  String getOAuthURL(){
    return "http://" + WiFi.localIP().toString() + "/oauth";
  }

  //assumes that we are connected to Wifi using Wifi class and assumes we have Serial.begin
  extern void runOAuthServer() {
    randomSeed(analogRead(0));//seeds random number generator

    code_verifier = generateRandomString(random(43, 129));//magic numbers from PKCE code_verifier length requirements
    state = generateRandomString(random(43, 129));


    server.on("/oauth", handleLichessOAuth);
    server.on("/callback", handleLichessOAuthCallBack);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");

    //want to print this to oLED so I made getOAuthURL a function
    Serial.println("To start lichess setup go to: " + getOAuthURL());

    displayString(0, 0, "To start lichess setup go to: ");
    displayString(0, 10, getOAuthURL());

    

    while(!authenticated){
      server.handleClient();
      delay(2);  //allow the cpu to switch to other tasks
    }

    display.clear();
    VextOFF();
    server.stop();
  }

#endif