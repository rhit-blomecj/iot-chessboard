#ifndef mcp_logic
#define mcp_logic

#include <SPI.h>
#include "MCP23S17.h"
#include <Adafruit_NeoPixel.h>

// SPI Pins (HSPI)
#define HSPI_MOSI_PIN 35
#define HSPI_MISO_PIN 37
#define HSPI_SCK_PIN  36
#define MCP_CS_PIN    34

#define MCP_INTERRUPT_PIN 4 // any gpio pin

#define NUM_MCPS 4

#define HALL_PIN 47

// LED matrix size
#define WIDTH       8
#define HEIGHT      8
#define NUMPIXELS   (WIDTH * HEIGHT)

// Create NeoPixel object
Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Colors
uint32_t RED  = strip.Color(255, 0, 0);
uint32_t BLUE = strip.Color(0, 0, 255);
uint32_t OFF  = strip.Color(0, 0, 0);
uint32_t WHITE = strip.Color(120,120,120);

SPIClass *spi3 = NULL;
MCP23S17 mcp[NUM_MCPS];

// Hardware addresses for your MCP23S17 chips
const byte mcp_addresses[NUM_MCPS] = {0x00, 0x01, 0x02, 0x03};
volatile bool mcp_interrupt_flag = false;

// ISR function
void IRAM_ATTR handle_mcp_interrupt() {
  mcp_interrupt_flag = true;
}

int testConnection(MCP23S17 &chip, byte address) {
  uint16_t magic_test_number = 0xABCD;
  uint16_t old_value;

  if (!chip.getPolarity16(old_value)) {
    Serial.print("MCP@0x"); 
    Serial.print(address, HEX); 
    Serial.println(" getPolarity16 failed (pre)");
    return -1;
  }
  if (!chip.setPolarity16(magic_test_number)) {
    Serial.print("MCP@0x"); 
    Serial.print(address, HEX); 
    Serial.println(" setPolarity16 failed");
    return -2;
  }
  uint16_t temp;
  if (!chip.getPolarity16(temp)) {
    Serial.print("MCP@0x"); 
    Serial.print(address, HEX); 
    Serial.println(" getPolarity16 failed (post)");
    return -3;
  }
  if (!chip.setPolarity16(old_value)) {
    Serial.print("MCP@0x"); 
    Serial.print(address, HEX); 
    Serial.println(" setPolarity16 restore failed");
    return -4;
  }
  if (temp != magic_test_number) {
    Serial.print("MCP@0x"); 
    Serial.print(address, HEX); 
    Serial.print(" magic number mismatch. Read: 0x");
    Serial.println(temp, HEX);
    return -5;
  }
  Serial.print("MCP@0x"); 
  Serial.print(address, HEX); 
  Serial.println(" connection test OK.");
  return 0; // OK
}

void setupMCP() {
  spi3 = new SPIClass(HSPI); 
  
  spi3->begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN, MCP_CS_PIN); // CS managed manually or by library per device
  
  pinMode(MCP_CS_PIN, OUTPUT);
  digitalWrite(MCP_CS_PIN, HIGH);

  // Serial.begin(115200); assuming this was already setup
  Serial.println();
  Serial.print("MCP23S17_LIB_VERSION: ");
  Serial.println(MCP23S17_LIB_VERSION);
  Serial.println();
  delay(100);

  bool all_mcp_ok = true;
  for (int i = 0; i < NUM_MCPS; i++) {
    Serial.print("Initializing MCP "); Serial.println(i);
    mcp[i] = MCP23S17(MCP_CS_PIN, spi3, mcp_addresses[i]);

    if (!mcp[i].begin()) {
      Serial.print("MCP "); Serial.print(i); Serial.print(" (Addr 0x"); Serial.print(mcp_addresses[i], HEX);
      Serial.println(") FAILED to begin!");
      all_mcp_ok = false;
    } else {
      Serial.print("MCP "); Serial.print(i); Serial.print(" (Addr 0x"); Serial.print(mcp_addresses[i], HEX);
      Serial.println(") began successfully.");
    }

    int rv = 1;
    int retries = 0;
    while (rv != 0 && retries < 3) {
        rv = testConnection(mcp[i], mcp_addresses[i]);
        if (rv != 0) {
            Serial.print("MCP "); Serial.print(i); Serial.print(" test connection result: "); Serial.println(rv);
            mcp[i].begin(); // Try to re-init
            delay(500);
        }
        retries++;
    }
    if (rv != 0) {
        all_mcp_ok = false;
        Serial.print("MCP "); Serial.print(i); Serial.println(" failed connection test after retries.");
    }


    // configure all 16 pins as input with pull-up
    mcp[i].pinMode16(INPUT_PULLUP); 

    // configure interrupts on the MCP
    mcp[i].mirrorInterrupts(true);      // INTA reflects GPA and GPB
    mcp[i].setInterruptPolarity(2);
    mcp[i].getInterruptCaptureRegister(); // clears INTCAP and interrupt flags
    mcp[i].enableInterrupt16(0xFFFF, CHANGE);
    mcp[i].getInterruptCaptureRegister();

    Serial.print("MCP "); Serial.print(i); Serial.println(" configured for interrupts.");
  }

  if (!all_mcp_ok) {
    Serial.println("!!! One or more MCPs failed to initialize properly. Check wiring and addresses. !!!");
    while(1) delay(1000);
  }

  pinMode(MCP_INTERRUPT_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(MCP_INTERRUPT_PIN), handle_mcp_interrupt, FALLING);

  Serial.println("Setup complete. Waiting for interrupts...");
}

void disableInterrupt() {
  disableInterrupt16(0x0000);
}

void enableInterrupt() {
  
}

void ignoreInterruptCastle() {
  int count = 0;
  while (count < 2) {
    if (mcp_interrupt_flag) {

      Serial.println("----------------------------------");
      Serial.println("ESP32 Interrupt Triggered for Castle");
      bool interrupt_found_on_mcp = false;

      for (int i = 0; i < NUM_MCPS; i++) { 
        uint16_t intFlags = mcp[i].getInterruptFlagRegister();

        if (intFlags != 0) {
          interrupt_found_on_mcp = true;

          uint16_t capturedValues = mcp[i].getInterruptCaptureRegister();

          for (int pin_idx_on_mcp = 0; pin_idx_on_mcp < 16; pin_idx_on_mcp++) {
            if ((intFlags >> pin_idx_on_mcp) & 0x01) { 
              
              // get chess pos
              String chess_pos = getChessPosition(i, pin_idx_on_mcp);
              int neo_pos = getNeoPixelIndex(i, pin_idx_on_mcp)
              bool current_pin_state = (capturedValues >> pin_idx_on_mcp) & 0x01;
              
              Serial.print(chess_pos); 
              Serial.print(" changed to: ");
              Serial.println(current_pin_state ? "HIGH" : "LOW");

              //eg ec

              strip.setNeoPixelColor(neo_pos, green);
              
              // ---chess board logic---
              // TODO: add the chessboard logic (chess_pos is the string)
            }
          }
        }
      }
      
      if (!interrupt_found_on_mcp) {
          for (int m = 0; m < NUM_MCPS; m++) {
              mcp[m].getInterruptCaptureRegister(); 
          }
      }

      mcp_interrupt_flag = false;
      Serial.println("----------------------------------");
    }
  }
}

String getMoveFromHardware() {
  String finalMove = "";
  bool move_not_completed = true;
  int highCount = 0;
  int lowCount = 0;
  while (move_not_completed) {
    if (mcp_interrupt_flag) {

      Serial.println("----------------------------------");
      Serial.println("ESP32 Interrupt Triggered!");
      bool interrupt_found_on_mcp = false;

      for (int i = 0; i < NUM_MCPS; i++) { 
        uint16_t intFlags = mcp[i].getInterruptFlagRegister();

        if (intFlags != 0) {
          interrupt_found_on_mcp = true;

          uint16_t capturedValues = mcp[i].getInterruptCaptureRegister();

          for (int pin_idx_on_mcp = 0; pin_idx_on_mcp < 16; pin_idx_on_mcp++) {
            if ((intFlags >> pin_idx_on_mcp) & 0x01) { 
              
              // get chess pos
              String chess_pos = getChessPosition(i, pin_idx_on_mcp);
              int neo_pos = getNeoPixelIndex(i, pin_idx_on_mcp)
              bool current_pin_state = (capturedValues >> pin_idx_on_mcp) & 0x01;
              
              Serial.print(chess_pos); 
              Serial.print(" changed to: ");
              Serial.println(current_pin_state ? "HIGH" : "LOW");

              //eg ec

              if (current_pin_state) {
                if (highCount == 0) { // picking up your own piece
                  strip.setPixelColor(neo_pos, GREEN);
                  finalMove += chess_pos;
                }
                if (highCount > 0 && lowCount == 0) { // picking up enemy piece
                  // taking enemy piece
                  // turning on blue
                  strip.setPixelColor(neo_pos, BLUE);
                }
                highCount++;
              } else {
                // putting down a piece (finish a move or finish taking)
                finalMove += chess_pos;
                lowCount++;
                strip.setPixelColor(neo_pos, GREEN);
                move_not_completed = false;
              }
              
              // ---chess board logic---
              // TODO: add the chessboard logic (chess_pos is the string)
            }
          }
        }
      }
      
      if (!interrupt_found_on_mcp) {
          for (int m = 0; m < NUM_MCPS; m++) {
              mcp[m].getInterruptCaptureRegister(); 
          }
      }

      mcp_interrupt_flag = false;
      Serial.println("----------------------------------");
    }
  }
  return finalMove;
}

String getChessPosition(int mcp_idx, int pin_on_mcp) {
  if (mcp_idx < 0 || mcp_idx >= NUM_MCPS || pin_on_mcp < 0 || pin_on_mcp > 15) {
    return "ERR";
  }

  char file_char;
  char rank_char;
  int base_rank_index = mcp_idx * 2; // each mcp has 2 ranks
  int sub_rank_offset = (pin_on_mcp / 8); // pin > 8 is 1 and pin > 0 is 0
  int chess_rank_index = base_rank_index + sub_rank_offset; // this gives 0-7 index for ranks

  rank_char = '1' + chess_rank_index;

  int chess_file_index = pin_on_mcp % 8; 

  file_char = 'a' + chess_file_index;

  String position = "";
  position += file_char;
  position += rank_char;
  return position;
}

int getNeoPixelIndex(int mcp_idx, int pin_on_mcp) {
  if (mcp_idx < 0 || mcp_idx >= NUM_MCPS || pin_on_mcp < 0 || pin_on_mcp > 15) {
    return -1;
  }

  int base_rank_index = mcp_idx * 2;
  int sub_rank_offset = pin_on_mcp / 8;
  int rank_index = base_rank_index + sub_rank_offset;

  int file_index = pin_on_mcp % 8;

  if (rank_index % 2 == 1) {
    file_index = 7 - file_index;
  }

  return rank_index * 8 + file_index;
}

#endif
