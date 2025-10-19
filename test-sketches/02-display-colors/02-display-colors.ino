// Test 02: Display Color Bars - Verify color rendering
// Shows RGB color bars to test display color accuracy

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== CYD Display Color Bars Test ===");
  Serial.println("Testing color rendering...");
  
  // Initialize display
  tft.init();
  tft.setRotation(0);  // Portrait: 240x320
  
  // Turn on backlight (GPIO21 for BOTH variants)
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  
  // Get display dimensions
  int width = tft.width();
  int height = tft.height();
  int barWidth = width / 7;  // 7 color bars
  
  Serial.print("Display size: ");
  Serial.print(width);
  Serial.print(" x ");
  Serial.println(height);
  
  // Draw color bars
  Serial.println("Drawing color bars:");
  
  // Red
  tft.fillRect(0, 0, barWidth, height, TFT_RED);
  Serial.println("  1. Red");
  
  // Green
  tft.fillRect(barWidth, 0, barWidth, height, TFT_GREEN);
  Serial.println("  2. Green");
  
  // Blue
  tft.fillRect(barWidth * 2, 0, barWidth, height, TFT_BLUE);
  Serial.println("  3. Blue");
  
  // Yellow
  tft.fillRect(barWidth * 3, 0, barWidth, height, TFT_YELLOW);
  Serial.println("  4. Yellow");
  
  // Cyan
  tft.fillRect(barWidth * 4, 0, barWidth, height, TFT_CYAN);
  Serial.println("  5. Cyan");
  
  // Magenta
  tft.fillRect(barWidth * 5, 0, barWidth, height, TFT_MAGENTA);
  Serial.println("  6. Magenta");
  
  // White
  tft.fillRect(barWidth * 6, 0, width - (barWidth * 6), height, TFT_WHITE);
  Serial.println("  7. White");
  
  // Add labels
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, height - 20);
  tft.print("R  G  B  Y  C  M  W");
  
  Serial.println("\nTest PASSED - Color bars displayed");
  Serial.println("Verify colors appear correct on display");
}

void loop() {
  // Cycle through test patterns every 5 seconds
  static unsigned long lastChange = 0;
  static int testPattern = 0;
  
  if (millis() - lastChange > 5000) {
    lastChange = millis();
    testPattern = (testPattern + 1) % 3;
    
    int width = tft.width();
    int height = tft.height();
    
    switch(testPattern) {
      case 0:
        // Back to color bars
        Serial.println("Pattern: Color bars");
        setup();  // Redraw color bars
        break;
        
      case 1:
        // Gradient test
        Serial.println("Pattern: Gradient");
        for (int x = 0; x < width; x++) {
          uint16_t color = tft.color565(x * 255 / width, 0, 255 - (x * 255 / width));
          tft.drawFastVLine(x, 0, height, color);
        }
        break;
        
      case 2:
        // Checkerboard
        Serial.println("Pattern: Checkerboard");
        int size = 20;
        for (int x = 0; x < width; x += size) {
          for (int y = 0; y < height; y += size) {
            if (((x/size) + (y/size)) % 2 == 0) {
              tft.fillRect(x, y, size, size, TFT_WHITE);
            } else {
              tft.fillRect(x, y, size, size, TFT_BLACK);
            }
          }
        }
        break;
    }
  }
}