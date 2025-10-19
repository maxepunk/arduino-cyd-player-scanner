// Display Test Sketch for CYD
// Shows color bars and text on detected display

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Pin definitions for backlight
#define BACKLIGHT_PIN_SINGLE 21
#define BACKLIGHT_PIN_DUAL   27

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nCYD Display Test v1.0");
  Serial.println("====================");
  
  // Initialize display
  tft.begin();
  
  // Try both backlight pins
  pinMode(BACKLIGHT_PIN_SINGLE, OUTPUT);
  pinMode(BACKLIGHT_PIN_DUAL, OUTPUT);
  digitalWrite(BACKLIGHT_PIN_SINGLE, HIGH);
  digitalWrite(BACKLIGHT_PIN_DUAL, HIGH);
  
  Serial.println("Display initialized");
  Serial.println("Testing display output...");
  
  // Clear screen
  tft.fillScreen(TFT_BLACK);
  
  // Test text rendering
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Display Test - Text Sizes:");
  
  tft.setTextSize(1);
  tft.println("Size 1: ABCDEFGHIJKLMNOP");
  
  tft.setTextSize(2);
  tft.println("Size 2: ABCDEF");
  
  tft.setTextSize(3);
  tft.println("Size 3: ABC");
  
  delay(2000);
  
  Serial.println("Text rendering test complete");
}

void loop() {
  static int testPhase = 0;
  static uint32_t phaseTimer = 0;
  
  if (millis() - phaseTimer > 3000) {
    phaseTimer = millis();
    
    switch(testPhase) {
      case 0:
        // Color bars test
        Serial.println("Testing color bars...");
        drawColorBars();
        break;
        
      case 1:
        // Gradient test
        Serial.println("Testing gradients...");
        drawGradient();
        break;
        
      case 2:
        // Geometric shapes test
        Serial.println("Testing shapes...");
        drawShapes();
        break;
        
      case 3:
        // Brightness test
        Serial.println("Testing brightness levels...");
        testBrightness();
        break;
    }
    
    testPhase = (testPhase + 1) % 4;
  }
}

void drawColorBars() {
  int barWidth = 240 / 8;
  uint16_t colors[] = {
    TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE,
    TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE
  };
  
  tft.fillScreen(TFT_BLACK);
  
  for (int i = 0; i < 8; i++) {
    tft.fillRect(i * barWidth, 0, barWidth, 320, colors[i]);
  }
  
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 150);
  tft.println("Color Bars");
}

void drawGradient() {
  tft.fillScreen(TFT_BLACK);
  
  // Red gradient
  for (int i = 0; i < 80; i++) {
    uint8_t red = map(i, 0, 79, 0, 31);
    uint16_t color = (red << 11);
    tft.drawFastHLine(0, i, 240, color);
  }
  
  // Green gradient
  for (int i = 80; i < 160; i++) {
    uint8_t green = map(i - 80, 0, 79, 0, 63);
    uint16_t color = (green << 5);
    tft.drawFastHLine(0, i, 240, color);
  }
  
  // Blue gradient
  for (int i = 160; i < 240; i++) {
    uint8_t blue = map(i - 160, 0, 79, 0, 31);
    uint16_t color = blue;
    tft.drawFastHLine(0, i, 240, color);
  }
  
  // White gradient
  for (int i = 240; i < 320; i++) {
    uint8_t level = map(i - 240, 0, 79, 0, 31);
    uint16_t color = (level << 11) | ((level * 2) << 5) | level;
    tft.drawFastHLine(0, i, 240, color);
  }
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 150);
  tft.println("Gradients");
}

void drawShapes() {
  tft.fillScreen(TFT_BLACK);
  
  // Draw various shapes
  tft.drawCircle(60, 60, 40, TFT_RED);
  tft.fillCircle(180, 60, 40, TFT_GREEN);
  
  tft.drawRect(20, 120, 80, 60, TFT_BLUE);
  tft.fillRect(140, 120, 80, 60, TFT_YELLOW);
  
  tft.drawTriangle(60, 200, 20, 260, 100, 260, TFT_CYAN);
  tft.fillTriangle(180, 200, 140, 260, 220, 260, TFT_MAGENTA);
  
  tft.drawRoundRect(20, 280, 80, 30, 10, TFT_WHITE);
  tft.fillRoundRect(140, 280, 80, 30, 10, TFT_ORANGE);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Shapes Test");
}

void testBrightness() {
  tft.fillScreen(TFT_WHITE);
  
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Brightness Test");
  
  // Note: Actual PWM brightness control would be done here
  // For now just showing different gray levels
  for (int i = 0; i < 5; i++) {
    uint8_t gray = i * 6;
    uint16_t color = (gray << 11) | (gray << 6) | gray;
    tft.fillRect(0, 50 + i * 50, 240, 50, color);
    
    tft.setCursor(10, 65 + i * 50);
    tft.setTextColor(TFT_WHITE - color, color);
    tft.print("Level ");
    tft.println(i);
  }
}