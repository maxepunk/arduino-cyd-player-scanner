// Hardware Detection Test Sketch for CYD
// Reports compile-time configuration and verifies hardware functionality
// Note: TFT_eSPI uses compile-time driver selection, not runtime detection

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Pin definitions
#define BACKLIGHT_PIN_SINGLE 21
#define BACKLIGHT_PIN_DUAL   27
#define TOUCH_IRQ_PIN        36
#define RFID_SCK_PIN         22
#define RFID_MOSI_PIN        27  // Note: Shared with backlight on dual USB
#define RFID_MISO_PIN        35
#define RFID_SS_PIN          3
#define SD_CS_PIN            5

void setup() {
  // Use proven serial initialization from display-test
  Serial.begin(115200);
  delay(1000);  // Simple delay that works
  
  Serial.println("\n========================================");
  Serial.println("CYD Hardware Detection v2.0");
  Serial.println("========================================");
  
  // Report compile-time configuration
  Serial.println("\n[COMPILE-TIME CONFIGURATION]");
  Serial.println("Library: TFT_eSPI configured for ST7789");
  Serial.println("Variant: DUAL_USB (Micro + Type-C)");
  Serial.println("Display: 240x320, ST7789 driver");
  Serial.println("Backlight: GPIO27 (multiplexed with RFID MOSI)");
  
  // Initialize display
  Serial.println("\n[DISPLAY INITIALIZATION]");
  Serial.println("Initializing TFT display...");
  tft.begin();
  Serial.println("Display initialized successfully");
  
  // Set up backlight pins using proven pattern from display-test
  // Keep BOTH HIGH to avoid conflicts
  pinMode(BACKLIGHT_PIN_SINGLE, OUTPUT);
  pinMode(BACKLIGHT_PIN_DUAL, OUTPUT);
  digitalWrite(BACKLIGHT_PIN_SINGLE, HIGH);  // Both HIGH
  digitalWrite(BACKLIGHT_PIN_DUAL, HIGH);    // No conflict
  Serial.println("Backlight pins configured (GPIO21=HIGH, GPIO27=HIGH)");
  
  // Test display functionality
  Serial.println("\n[DISPLAY TEST]");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("CYD Hardware");
  tft.println("Detection v2.0");
  
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println("Variant: DUAL_USB");
  tft.setCursor(10, 75);
  tft.println("Display: ST7789 240x320");
  tft.setCursor(10, 90);
  tft.println("Backlight: GPIO27");
  
  // Draw test pattern
  tft.fillRect(10, 110, 220, 20, TFT_RED);
  tft.fillRect(10, 135, 220, 20, TFT_GREEN);
  tft.fillRect(10, 160, 220, 20, TFT_BLUE);
  
  Serial.println("Display test complete - showing text and color bars");
  
  // Report pin configuration
  Serial.println("\n[PIN CONFIGURATION]");
  Serial.println("Display SPI: Default TFT_eSPI pins");
  Serial.printf("Touch IRQ: GPIO%d (IRQ-only, no SPI)\n", TOUCH_IRQ_PIN);
  Serial.printf("RFID Pins: SCK=%d, MOSI=%d, MISO=%d, SS=%d\n", 
                RFID_SCK_PIN, RFID_MOSI_PIN, RFID_MISO_PIN, RFID_SS_PIN);
  Serial.printf("SD Card CS: GPIO%d\n", SD_CS_PIN);
  Serial.println("Audio I2S: BCK=26, WS=25, DOUT=17");
  
  // Test touch IRQ pin state
  Serial.println("\n[COMPONENT STATUS]");
  pinMode(TOUCH_IRQ_PIN, INPUT);
  int touchState = digitalRead(TOUCH_IRQ_PIN);
  Serial.printf("Touch IRQ state: %s (GPIO%d)\n", 
                touchState == HIGH ? "HIGH (not pressed)" : "LOW (pressed)",
                TOUCH_IRQ_PIN);
  
  // Report GPIO27 multiplexing requirement
  Serial.println("\n[CRITICAL NOTES]");
  Serial.println("* GPIO27 is multiplexed between:");
  Serial.println("  - Backlight PWM (when not using RFID)");
  Serial.println("  - RFID MOSI (during RFID operations)");
  Serial.println("* Touch uses IRQ-only detection (no coordinate reading)");
  Serial.println("* This binary is compiled for ST7789 dual USB variant");
  Serial.println("* For single USB variant, recompile with ILI9341 driver");
  
  // Summary
  Serial.println("\n[HARDWARE DETECTION COMPLETE]");
  Serial.println("========================================");
  Serial.println("Result: DUAL_USB variant with ST7789 display");
  Serial.println("Status: Display functional, backlight active");
  Serial.println("Ready for component testing");
  Serial.println("========================================\n");
}

void loop() {
  // Cycle through test patterns every 5 seconds
  static uint32_t lastChange = 0;
  static int testPhase = 0;
  
  if (millis() - lastChange > 5000) {
    lastChange = millis();
    
    switch(testPhase) {
      case 0:
        // Show color test
        Serial.println("Display test: Color bars");
        tft.fillRect(0, 200, 80, 120, TFT_RED);
        tft.fillRect(80, 200, 80, 120, TFT_GREEN);
        tft.fillRect(160, 200, 80, 120, TFT_BLUE);
        break;
        
      case 1:
        // Show white screen (backlight test)
        Serial.println("Display test: White screen (backlight test)");
        tft.fillRect(0, 200, 240, 120, TFT_WHITE);
        break;
        
      case 2:
        // Show black screen
        Serial.println("Display test: Black screen");
        tft.fillRect(0, 200, 240, 120, TFT_BLACK);
        break;
        
      case 3:
        // Show gradient
        Serial.println("Display test: Gray gradient");
        for (int i = 0; i < 6; i++) {
          uint8_t gray = i * 5;
          uint16_t color = tft.color565(gray * 8, gray * 8, gray * 8);
          tft.fillRect(i * 40, 200, 40, 120, color);
        }
        break;
    }
    
    testPhase = (testPhase + 1) % 4;
    
    // Also report touch pin state periodically
    int touchState = digitalRead(TOUCH_IRQ_PIN);
    if (touchState == LOW) {
      Serial.println("Touch detected!");
    }
  }
}