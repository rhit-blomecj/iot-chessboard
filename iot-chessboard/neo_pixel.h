#ifndef neo_pixel
#define neo_pixel

#include <Adafruit_NeoPixel.h>

#define LED_PIN 48
// LED matrix size
#define WIDTH       8
#define HEIGHT      8
#define NUMPIXELS   (WIDTH * HEIGHT)

#define BRIGHTNESS 50 // Set BRIGHTNESS to about 1/5 (max = 255)

// Create NeoPixel object
Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Colors
uint32_t RED  = strip.Color(255, 0, 0);
uint32_t GREEN = strip.Color(0, 255, 0);
uint32_t BLUE = strip.Color(0, 0, 255);
uint32_t OFF  = strip.Color(0, 0, 0);
//uint32_t WHITE = strip.Color(120,120,120);//this has a declaration in HT_Display.h so we should not redeclare it


struct IndexPair { 
  int file_index;
  int rank_index;
};

String translateIndexesToFileAndRank(IndexPair pair){
  //auto promotes to Queen
  char file = 'a' + pair.file_index;
  char rank = '1' + pair.rank_index;

  char c_strng_tile [3] = "12";

  c_strng_tile [0] = file;
  c_strng_tile [1] = rank;

  String tile = String(c_strng_tile);

  return tile;
}

IndexPair translateFileAndRankToIndexes(String tile){
  const char * c_str_tile = tile.c_str();//should only be size 3 because it should be something like "a1"

  IndexPair pair;

  pair.file_index = (int) (c_str_tile[0] - 'a');
  pair.rank_index = (int) (c_str_tile[1] - '1');
  
  return pair;
}

int translateIndexesToNeoPixel(IndexPair pair){
  const int FILE_WIDTH = 8;
  if(pair.rank_index%2 == 0){
    return (pair.rank_index * FILE_WIDTH + pair.file_index);
  } else{
    return (pair.rank_index * FILE_WIDTH + (FILE_WIDTH - pair.file_index));
  }
}

extern int translateFileAndRankToNeoPixel(String tile){
  return translateIndexesToNeoPixel(translateFileAndRankToIndexes(tile));
}

void printMoveInfo(String move){
  IndexPair startIndexes = translateFileAndRankToIndexes(move.substring(0,2));
  IndexPair endIndexes = translateFileAndRankToIndexes(move.substring(2,4));
  Serial.printf("start_file_index: %d\nstart_rank_index: %d\nend_file_index: %d\nend_rank_index: %d\n", startIndexes.file_index, startIndexes.rank_index, endIndexes.file_index, endIndexes.rank_index);
  Serial.println("Translated back into UCI: " + translateIndexesToFileAndRank(startIndexes) + translateIndexesToFileAndRank(endIndexes));
  Serial.printf("Translated to NeoPixel Start: %d\nTranslated to NeoPixel End: %d\n", translateIndexesToNeoPixel(startIndexes), translateIndexesToNeoPixel(endIndexes));
}


void neoPixelSetup(){
  strip.begin();
  strip.show();
}

void setPixelColor(int neo_pos, uint32_t color){
  Serial.println("setPixelColor Called");
  strip.setPixelColor(neo_pos, color);
  strip.show();
  strip.setBrightness(BRIGHTNESS);
}

void clearPixel(int neo_pos){
  strip.setPixelColor(neo_pos, OFF);
  strip.show();
}

void clearAllPixels(){
  Serial.println("clearAllPixels Called");
  strip.clear();
  strip.show();
}

#endif