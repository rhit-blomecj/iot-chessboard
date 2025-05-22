#ifndef global_preferences
#define global_preferences

  #include <Preferences.h>

  Preferences prefs;

  extern void setupGlobalPreferences(){
    prefs.begin("IoTChessboard", false);
    // prefs.clear();

    Serial.println("Saved SSID: " + prefs.getString("network_ssid", "none"));
    Serial.println("Saved Password: " + prefs.getString("network_pw", "none"));
    Serial.println("Saved Token: " + prefs.getString("lichess_token", "none"));
    Serial.println("Saved Game ID: " + prefs.getString("game_id", "none"));
  }

#endif