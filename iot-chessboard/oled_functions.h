#ifndef oled_functions
#define oled_functions

  #include <Wire.h>
  #include "HT_SSD1306Wire.h"

  //OLED declaration
  static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

  // Turn Vext ON (powers OLED)
  extern void VextON(void) {
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
  }

  // Turn Vext OFF (default)
  extern void VextOFF(void) {
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, HIGH);
  }

  extern void setupOLED(void) {
    //OLED setup
    VextON();
    delay(100);
    // Initialising the UI will init the display too.
    display.init();
    delay(200);
    // defined font sizes in library are 10, 16, and 24
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "IoT Chessboard");
    display.display();
    
    delay(1000);
    
    display.clear();
    VextOFF();
  }

  //IT IS THE RESPONSIBILITY OF THE CALLER TO CALL display.clear(); and VextOFF(); when they are done using the OLED
  extern void displayString(int x, int y, String string){
    VextON();
    delay(100);
    display.drawString(x, y, string);
    display.display();
  }

#endif