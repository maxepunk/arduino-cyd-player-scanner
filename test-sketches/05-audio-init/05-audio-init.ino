// Test AudioOutputI2S initialization
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
AudioOutputI2S *out = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== AudioOutputI2S Init Test ===");
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Audio Init Test");
  
  Serial.println("Before AudioOutputI2S initialization...");
  
  // Initialize Audio exactly like ALNScanner does
  out = new AudioOutputI2S(0, 1);
  
  Serial.println("After AudioOutputI2S initialization!");
  Serial.println("If you see this, audio init doesn't break serial");
}

void loop() {
  Serial.println("Audio initialized, serial still works!");
  delay(1000);
}