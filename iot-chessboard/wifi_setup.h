#ifndef wifi_setup
#define wifi_setup

  #include <WiFi.h>

  extern bool connectWifi(String ssid, String password){
      Serial.println("SSID in connectWifi: " + ssid);
      Serial.println("password in connectWifi: " + password);

      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      Serial.println("");

      int timeout = millis() + 15000;//set timeout time to 15 seconds in the future

      // Wait for connection
      while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
        delay(500);
        Serial.print(".");
      }

      if(millis() >= timeout){
        Serial.println("");
        Serial.print("Failed to Connect to ");
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