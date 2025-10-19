// Test 01: Display "Hello World" - Basic TFT_eSPI functionality test
// Tests the most basic display operation to verify configuration

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== CYD Display Hello World Test ===");
  Serial.println("Testing basic display initialization...");
  
  // Initialize display
  tft.init();
  tft.setRotation(0);  // Portrait orientation (240x320)
  
  // Turn on backlight (GPIO21 for BOTH variants)
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  
  // Clear screen to black
  tft.fillScreen(TFT_BLACK);
  
  // Set text parameters
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  // Display Hello World (adjusted for portrait)
  tft.setCursor(40, 140);
  tft.println("Hello World!");
  
  // Display variant info
  tft.setTextSize(1);
  tft.setCursor(10, 180);
  tft.println("CYD 2.8\" Display Test");
  tft.setCursor(10, 190);
  tft.println("If you see this, display works!");
  
  // Report success to serial
  Serial.println("Display initialized successfully");
  Serial.print("Display driver: ");
  #ifdef ILI9341_DRIVER
    Serial.println("ILI9341");
  #elif defined(ST7789_DRIVER)
    Serial.println("ST7789");
  #else
    Serial.println("Unknown");
  #endif
  
  Serial.print("Display size: ");
  Serial.print(tft.width());
  Serial.print(" x ");
  Serial.println(tft.height());
  
  Serial.println("\nTest PASSED - Display showing Hello World");
}

void loop() {
  // Blink the text every 2 seconds
  static bool visible = true;
  static unsigned long lastToggle = 0;
  
  if (millis() - lastToggle > 2000) {
    lastToggle = millis();
    visible = !visible;
    
    tft.setTextSize(2);
    tft.setCursor(40, 140);
    
    if (visible) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.println("Hello World!");
      Serial.println("Text visible");
    } else {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.println("Hello World!");
      Serial.println("Text hidden");
    }
  }
}