#ifndef wifi_setup
#define wifi_setup

  #include <WiFi.h>

  extern bool connectWifi(String ssid, String password){
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      Serial.println("");

      int timeout = millis() + 10000;//set timeout time to 10 seconds in the future

      // Wait for connection
      while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
        delay(500);
        Serial.print(".");
      }

      if(millis() >= timeout){
        Serial.println("");
        Serial.print("Faile to Connect to ");
        Serial.println(ssid);
        return false;
      }

      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      return true;
  }

#endif