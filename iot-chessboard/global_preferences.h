#ifndef global_preferences
#define global_preferences

  #include <Preferences.h>

  Preferences prefs;

  extern void setupGlobalPreferences(){
    prefs.begin("IoTChessboard", false);
  }


#endif