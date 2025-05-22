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

#define LED_PIN     48

// Create NeoPixel object
Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Colors
uint32_t RED  = strip.Color(255, 0, 0);
uint32_t BLUE = strip.Color(0, 0, 255);
uint32_t OFF  = strip.Color(0, 0, 0);
uint32_t WHITE = strip.Color(120,120,120);
uint32_t GREEN = strip.Color(0,50,0);

unsigned long lastInterrupt_MCP0 = 0;
unsigned long lastInterrupt_MCP1 = 0;
unsigned long lastInterrupt_MCP2 = 0;
unsigned long lastInterrupt_MCP3 = 0;

const int intPin[NUM_MCPS] = {4, 5, 6, 7};

const byte mcp_addresses[NUM_MCPS] = {0x00, 0x01, 0x02, 0x03};
SPIClass *spi3 = new SPIClass(HSPI);

MCP23S17 MCP_A(MCP_CS_PIN, mcp_addresses[0], spi3);
MCP23S17 MCP_B(MCP_CS_PIN, mcp_addresses[1], spi3);
MCP23S17 MCP_C(MCP_CS_PIN, mcp_addresses[2], spi3);
MCP23S17 MCP_D(MCP_CS_PIN, mcp_addresses[3], spi3);

MCP23S17 mcp[NUM_MCPS] = {MCP_A, MCP_B, MCP_C, MCP_D};

// Hardware addresses for your MCP23S17 chips
// const byte mcp_addresses[NUM_MCPS] = {0x00, 0x01, 0x02, 0x03};
volatile bool mcp_interrupted[NUM_MCPS] = {false};

// ISR function
void IRAM_ATTR ISR0() {
  unsigned long now = micros();
  if (now - lastInterrupt_MCP0 < 20000) { // Example: 20ms = 20000µs if you need longer
    return;
  }
  lastInterrupt_MCP0 = now;
  mcp_interrupted[0] = true;
}

void IRAM_ATTR ISR1() {
  unsigned long now = micros();
  if (now - lastInterrupt_MCP1 < 20000) { // Example: 20ms = 20000µs if you need longer
    return;
  }
  lastInterrupt_MCP1 = now;
  mcp_interrupted[1] = true;
}

void IRAM_ATTR ISR2() {
  unsigned long now = micros();
  if (now - lastInterrupt_MCP2 < 20000) { // Example: 20ms = 20000µs if you need longer
    return;
  }
  lastInterrupt_MCP2 = now;
  mcp_interrupted[2] = true;
}

void IRAM_ATTR ISR3() {
  unsigned long now = micros();
  if (now - lastInterrupt_MCP3 < 20000) { // Example: 20ms = 20000µs if you need longer
    return;
  }
  lastInterrupt_MCP3 = now;
  mcp_interrupted[3] = true;
}

typedef void (*ISR_FUNC_PTR)();

ISR_FUNC_PTR mcp_isrs[NUM_MCPS] = {
  ISR0,
  ISR1,
  ISR2,
  ISR3
};

// void setupMCP() {
void setup() {
  // spi3 = new SPIClass(1);
  spi3->begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN, MCP_CS_PIN);
  pinMode(spi3->pinSS(), OUTPUT);
  digitalWrite(spi3->pinSS(), HIGH);

  Serial.begin(115200);
  Serial.println();
  Serial.print("MCP23S17_LIB_VERSION: ");
  Serial.println(MCP23S17_LIB_VERSION); 
  Serial.println();
  delay(100);
  // SPI.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN, MCP_CS_PIN);
  for (int i = 0; i < NUM_MCPS; i++) {
    pinMode(intPin[i], INPUT);
    attachInterrupt(digitalPinToInterrupt(intPin[i]), mcp_isrs[i], FALLING);
    bool b = mcp[i].begin();

    mcp[i].setSPIspeed(8000000);
    mcp[i].pinMode16(0xFFFF); // Set all pins to input
    mcp[i].setPullup16(0xFFFF); // Enable pull-up resistors for all pins
    mcp[i].setPolarity16(0x0000); // Set polarity of all pins  
    mcp[i].setInterruptPolarity(0); // Set interrupt polarity
    mcp[i].mirrorInterrupts(true);
    mcp[i].enableInterrupt16(0xFFFF, CHANGE); // Enable interrupt for all pins
    mcp[i].getInterruptCaptureRegister(); // read interrupt capture register to reset interrupt
    
    Serial.println(b ? "true" : "false");
    Serial.print("SS pin: ");
    Serial.println(spi3->pinSS());
    // mcp[i].pinMode16(1);

    // testConnection(mcp[i]);
    // strip.clear();
  }
  strip.clear();

}

// void disableInterrupt() {
//   detachInterrupt(digitalPinToInterrupt(MCP_INTERRUPT_PIN));
//   for (int i = 0; i < NUM_MCPS; i++) {
//     mcp[i].disableInterrupt16(0xFFFF);
//   }
// }

// void enableInterrupt() {
//   for (int i = 0; i < NUM_MCPS; i++) {
//     mcp[i].enableInterrupt16(0xFFFF, CHANGE);
//   }
//   attachInterrupt(digitalPinToInterrupt(MCP_INTERRUPT_PIN), ISR, FALLING);
// }

// void ignoreInterruptCastle() {
//   static int count = 0;
//   while (count < 2) {
//     if (mcp_interrupt_flag) {

//       Serial.println("----------------------------------");
//       Serial.println("ESP32 Interrupt Triggered for Castle");
//       bool interrupt_found_on_mcp = false;

//       for (int i = 0; i < NUM_MCPS; i++) { 
//         uint16_t intFlags = mcp[i].getInterruptFlagRegister();

//         if (intFlags != 0) {
//           interrupt_found_on_mcp = true;

//           uint16_t capturedValues = mcp[i].getInterruptCaptureRegister();

//           for (int pin_idx_on_mcp = 0; pin_idx_on_mcp < 16; pin_idx_on_mcp++) {
//             if ((intFlags >> pin_idx_on_mcp) & 0x01) { 
              
//               // get chess pos
//               String chess_pos = getChessPosition(i, pin_idx_on_mcp);
//               int neo_pos = getNeoPixelIndex(i, pin_idx_on_mcp);
//               bool current_pin_state = (capturedValues >> pin_idx_on_mcp) & 0x01;
              
//               Serial.print(chess_pos); 
//               Serial.print(" changed to: ");
//               Serial.println(current_pin_state ? "HIGH" : "LOW");

//               //eg ec

//               strip.setPixelColor(neo_pos, GREEN);
//               count++;
//             }
//           }

//         }
//       }
      
//       if (!interrupt_found_on_mcp) {
//           for (int m = 0; m < NUM_MCPS; m++) {
//               mcp[m].getInterruptCaptureRegister(); 
//           }
//       }

//       mcp_interrupt_flag = false;
//       Serial.println("----------------------------------");
//     }
//   }
//   count = 0;
// }

// String getMoveFromHardware() {
void loop() {
  // if(mcp_interrupt_flag) {
  //   // ToDo: only read bank A when input A is triggered
  //   for (int i = 0; i < NUM_MCPS; i++) {
  //     uint16_t flag = mcp[i].getInterruptFlagRegister();
  //     uint16_t capt = mcp[i].getInterruptCaptureRegister();
  //     Serial.println("Interrupt triggered!");

  //     Serial.println("Done!");
  //   }
  //   mcp_interrupt_flag = false;
    
  // }
  // if (mcp_interrupt_flag) {
  //   Serial.println("Interrupt Occurred");
  //   for (int i = 0; i < NUM_MCPS; i++) {
  //     uint16_t flag = mcp[i].getInterruptFlagRegister();
  //     uint16_t capt = mcp[i].getInterruptCaptureRegister();
  //     Serial.print("MCP: ");
  //     Serial.println(i);     
  //   }
  //   mcp_interrupt_flag = false;
    
  // }
  String finalMove = "";
  bool move_not_completed = true;
  int highCount = 0;
  int lowCount = 0;
  // while (move_not_completed) {
    for (int i = 0; i < NUM_MCPS; i++) { 
      if (mcp_interrupted[i]) {


        Serial.println("----------------------------------");
        Serial.print("ESP32 Interrupt Triggered for: "); Serial.println(i);
        uint16_t intFlags = mcp[i].getInterruptFlagRegister();
        uint16_t capturedValues = mcp[i].getInterruptCaptureRegister();
        for (int pin_idx_on_mcp = 0; pin_idx_on_mcp < 16; pin_idx_on_mcp++) {
          Serial.print("MCP #");
          Serial.print(i);
          Serial.print(" intFlags: 0x");
          Serial.println(intFlags, HEX);
          Serial.print("Checking pin ");
          Serial.println(pin_idx_on_mcp);
          if ((intFlags >> pin_idx_on_mcp) & 0x01) {
            int neo_pos = getNeoPixelIndex(i, pin_idx_on_mcp);
            bool current_pin_state = (capturedValues >> pin_idx_on_mcp) & 0x01;

            if (current_pin_state) {
              // Magnet arrived
              strip.setPixelColor(neo_pos, GREEN);
            } else {
              // Magnet left
              strip.setPixelColor(neo_pos, 0); // turn off
            }

            strip.show();
          }
        }
      }

      mcp_interrupted[i] = false;
    }
  // }
  // return finalMove;
}

// String getChessPosition(int mcp_idx, int pin_on_mcp) {
//   if (mcp_idx < 0 || mcp_idx >= NUM_MCPS || pin_on_mcp < 0 || pin_on_mcp > 15) {
//     return "ERR";
//   }

//   char file_char;
//   char rank_char;
//   int base_rank_index = mcp_idx * 2; // each mcp has 2 ranks
//   int sub_rank_offset = (pin_on_mcp / 8); // pin > 8 is 1 and pin > 0 is 0
//   int chess_rank_index = base_rank_index + sub_rank_offset; // this gives 0-7 index for ranks

//   rank_char = '1' + chess_rank_index;

//   int chess_file_index = pin_on_mcp % 8; 

//   file_char = 'a' + chess_file_index;

//   String position = "";
//   position += file_char;
//   position += rank_char;
//   return position;
// }

String getChessPosition(int mcpIndex, int pinIndexOnMCP) {
  int row = mcpIndex * 2 + (pinIndexOnMCP < 8 ? 0 : 1);
  int col = pinIndexOnMCP % 8;
  char file = 'A' + col;
  int rank = 8 - row; // row 0 is rank 8, row 7 is rank 1
  return String(file) + String(rank);
}

int getNeoPixelIndex(int mcpIndex, int pinIndex) {
  int row = mcpIndex * 2 + (pinIndex >= 8);  // 0-7: GPA, 8-15: GPB
  int col = pinIndex % 8;

  if (row % 2 == 0) {
    // Left to right
    return row * 8 + col;
  } else {
    // Right to left (zigzag)
    return row * 8 + (7 - col);
  }
}

int testConnection(MCP23S17 & mcp)
{
  uint16_t magic_test_number = 0xABCD;

  //  Read the current polarity config to restore later
  uint16_t old_value;
  if (! mcp.getPolarity16(old_value)) return -1;

  //  Write the magic number to polarity register
  if (! mcp.setPolarity16(magic_test_number)) return -2;

  //  Read back the magic number from polarity register
  uint16_t temp;
  if (! mcp.getPolarity16(temp)) return -3;

  //  Write old config to polarity register
  if (! mcp.setPolarity16(old_value)) return -4;

  //  Check the magic connection test
  if (temp != magic_test_number) return -5;

  return 0;  //  OK
}
          // if ((intFlags >> pin_idx_on_mcp) & 0x01) { 
          //   // get chess pos
          //   String chess_pos = getChessPosition(i, pin_idx_on_mcp);
          //   int neo_pos = getNeoPixelIndex(i, pin_idx_on_mcp);
          //   bool current_pin_state = (capturedValues >> pin_idx_on_mcp) & 0x01;
            
          //   Serial.print(chess_pos); 
          //   Serial.print(" changed to: ");
          //   Serial.println(current_pin_state ? "HIGH" : "LOW");

          //   if (current_pin_state) {
          //     if (highCount == 0) { // picking up your own piece
          //       strip.setPixelColor(neo_pos, GREEN);
          //       finalMove += chess_pos;
          //     }
          //     if (highCount > 0 && lowCount == 0) { // picking up enemy piece
          //       // taking enemy piece
          //       // turning on blue
          //       strip.setPixelColor(neo_pos, BLUE);
          //     }
          //     highCount++;
          //   } else {
          //     // putting down a piece (finish a move or finish taking)
          //     finalMove += chess_pos;
          //     lowCount++;
          //     strip.setPixelColor(neo_pos, GREEN);
          //     move_not_completed = false;
          //   }

          //   strip.show();

          //   // strip.show();
          // }
