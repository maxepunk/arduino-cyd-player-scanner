#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(100);  // Give serial time to init
  Serial.println("\n=== Display + Serial Test ===");
  Serial.println("Initializing display...");
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Display Test");
  tft.println("Check Serial!");
  
  Serial.println("Display initialized!");
  Serial.println("You should see 'Display Test' on screen");
}

void loop() {
  Serial.println("Loop running - serial works with display!");
  delay(2000);
}