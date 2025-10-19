// Test with ALL includes from ALNScanner
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== All Includes Test ===");
  Serial.println("Testing with all ALNScanner includes");
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("All Includes");
  tft.println("Check Serial!");
  
  Serial.println("If you see this, includes don't break serial");
}

void loop() {
  Serial.println("Still working!");
  delay(1000);
}