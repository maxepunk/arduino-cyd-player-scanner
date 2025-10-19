// Test MFRC522::Uid global object
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// Exact same globals as ALNScanner
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
MFRC522::Uid uid;  // This is what we're testing

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MFRC522::Uid Test ===");
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("MFRC522 Uid Test");
  
  Serial.println("If you see this, MFRC522::Uid doesn't break serial");
}

void loop() {
  Serial.println("MFRC522::Uid initialized, serial works!");
  delay(1000);
}