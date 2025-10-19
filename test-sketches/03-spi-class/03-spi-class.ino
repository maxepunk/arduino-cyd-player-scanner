#include <SPI.h>
#include <TFT_eSPI.h>

// Test if SPIClass initialization breaks serial
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);  // This is what ALNScanner has

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== SPIClass Test ===");
  Serial.println("If you see this, SPIClass doesn't break serial");
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("SPIClass Test");
  
  Serial.println("Display initialized");
}

void loop() {
  Serial.println("Loop running!");
  delay(1000);
}