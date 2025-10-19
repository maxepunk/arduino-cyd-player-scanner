/**
 * CYD Model Detector Test Sketch
 * 
 * Purpose: Detect which CYD hardware variant is connected
 * - Single USB (micro) with ILI9341 display
 * - Dual USB (micro + Type-C) with ST7789 display
 * 
 * This sketch attempts to identify the display driver and backlight pin
 * to determine the CYD model without requiring any wiring changes.
 */

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

// Possible backlight pins
#define BACKLIGHT_PIN_21 21
#define BACKLIGHT_PIN_27 27

// Display driver IDs
#define ILI9341_ID 0x9341
#define ST7789_ID  0x8552

// Detection results
uint16_t detectedDriverID = 0;
uint8_t detectedBacklightPin = 0;
String detectedModel = "Unknown";

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("    CYD Model Detection Test Sketch");
    Serial.println("========================================");
    Serial.println();
    
    // Test both backlight pins
    Serial.println("Testing backlight pins...");
    testBacklightPins();
    
    // Initialize display to test driver
    Serial.println("\nInitializing display for driver detection...");
    tft.init();
    
    // Attempt to detect display driver
    Serial.println("Attempting display driver detection...");
    detectDisplayDriver();
    
    // Determine model based on detection
    determineModel();
    
    // Report results
    reportResults();
    
    // Visual confirmation on display
    showVisualConfirmation();
}

void testBacklightPins() {
    // Try GPIO21 first (single USB default)
    pinMode(BACKLIGHT_PIN_21, OUTPUT);
    digitalWrite(BACKLIGHT_PIN_21, HIGH);
    delay(100);
    
    Serial.print("  GPIO21 set HIGH - ");
    
    // Try GPIO27 (dual USB default)
    pinMode(BACKLIGHT_PIN_27, OUTPUT);
    digitalWrite(BACKLIGHT_PIN_27, HIGH);
    delay(100);
    
    Serial.println("GPIO27 set HIGH");
    Serial.println("  Note: Both pins activated for safety");
    
    // For this test, we'll keep both on
    // Real implementation will detect which one actually controls backlight
    detectedBacklightPin = BACKLIGHT_PIN_21; // Default assumption
}

void detectDisplayDriver() {
    // Method 1: Try reading display ID
    uint16_t id = readDisplayID();
    
    if (id == ILI9341_ID) {
        Serial.println("  ✓ ILI9341 detected via ID read");
        detectedDriverID = ILI9341_ID;
    } else if (id == ST7789_ID || id == 0x7789) {
        Serial.println("  ✓ ST7789 detected via ID read");
        detectedDriverID = ST7789_ID;
    } else {
        Serial.printf("  ? Unknown ID: 0x%04X\n", id);
        
        // Method 2: Try command response test
        Serial.println("  Attempting command response test...");
        if (testILI9341Commands()) {
            Serial.println("  ✓ ILI9341 detected via command test");
            detectedDriverID = ILI9341_ID;
        } else if (testST7789Commands()) {
            Serial.println("  ✓ ST7789 detected via command test");
            detectedDriverID = ST7789_ID;
        } else {
            Serial.println("  ✗ Unable to determine driver");
        }
    }
}

uint16_t readDisplayID() {
    // This is a simplified ID read
    // Real implementation would use proper SPI commands
    
    // For TFT_eSPI, we can try to use the readcommand8 function
    // Note: This may not work with all TFT_eSPI configurations
    
    uint8_t id1 = tft.readcommand8(0xD3, 1);
    uint8_t id2 = tft.readcommand8(0xD3, 2);
    
    uint16_t id = (id1 << 8) | id2;
    
    Serial.printf("  Raw ID read: 0x%02X%02X\n", id1, id2);
    
    return id;
}

bool testILI9341Commands() {
    // Test ILI9341-specific command
    // Read power mode (command 0x0A)
    uint8_t powerMode = tft.readcommand8(0x0A);
    
    // ILI9341 typically returns 0x9C for normal operation
    if (powerMode == 0x9C || powerMode == 0x94) {
        Serial.printf("    Power mode: 0x%02X (ILI9341 signature)\n", powerMode);
        return true;
    }
    
    return false;
}

bool testST7789Commands() {
    // Test ST7789-specific behavior
    // ST7789 has different response patterns
    
    // Read display status (command 0x09)
    uint8_t status = tft.readcommand8(0x09);
    
    // ST7789 typically returns different values
    if (status != 0x00 && status != 0xFF) {
        Serial.printf("    Display status: 0x%02X (possible ST7789)\n", status);
        
        // Additional check: ST7789 specific register
        uint8_t rdid1 = tft.readcommand8(0xDA);
        if (rdid1 == 0x85) {
            Serial.println("    RDID1: 0x85 (ST7789 confirmed)");
            return true;
        }
    }
    
    return false;
}

void determineModel() {
    if (detectedDriverID == ILI9341_ID) {
        detectedModel = "CYD Single USB (ILI9341)";
        detectedBacklightPin = BACKLIGHT_PIN_21;
    } else if (detectedDriverID == ST7789_ID) {
        detectedModel = "CYD Dual USB (ST7789)";
        // Note: Some dual USB models use GPIO21, some use GPIO27
        // This would need additional detection logic
        detectedBacklightPin = BACKLIGHT_PIN_27;
    } else {
        detectedModel = "Unknown CYD Model";
        Serial.println("\n⚠ WARNING: Could not determine CYD model");
        Serial.println("  Defaulting to safe ILI9341 mode");
        detectedDriverID = ILI9341_ID;
        detectedBacklightPin = BACKLIGHT_PIN_21;
    }
}

void reportResults() {
    Serial.println("\n========================================");
    Serial.println("         DETECTION RESULTS");
    Serial.println("========================================");
    Serial.print("Model: ");
    Serial.println(detectedModel);
    Serial.printf("Display Driver ID: 0x%04X\n", detectedDriverID);
    Serial.printf("Backlight Pin: GPIO%d\n", detectedBacklightPin);
    Serial.println("========================================");
    
    // Provide configuration recommendations
    Serial.println("\nRecommended Configuration:");
    if (detectedDriverID == ILI9341_ID) {
        Serial.println("  TFT_eSPI User_Setup.h:");
        Serial.println("    #define ILI9341_DRIVER");
        Serial.println("    #define TFT_BL 21");
    } else if (detectedDriverID == ST7789_ID) {
        Serial.println("  TFT_eSPI User_Setup.h:");
        Serial.println("    #define ST7789_DRIVER");
        Serial.println("    #define TFT_BL 27  // or 21, varies by unit");
        Serial.println("    #define TFT_RGB_ORDER TFT_BGR  // May need adjustment");
    }
    
    Serial.println("\nPin Configuration (NO CHANGES NEEDED):");
    Serial.println("  RFID: SCK=22, MOSI=27, MISO=35, SS=3");
    Serial.println("  SD: SCK=18, MOSI=23, MISO=19, CS=5");
    Serial.println("  Touch: CS=33, IRQ=36");
}

void showVisualConfirmation() {
    // Clear screen
    tft.fillScreen(TFT_BLACK);
    
    // Set text properties
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    
    // Display detection results
    tft.setCursor(10, 10);
    tft.println("CYD Model Detector");
    
    tft.setTextSize(1);
    tft.setCursor(10, 40);
    tft.print("Model: ");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println(detectedModel);
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 60);
    tft.printf("Driver: 0x%04X", detectedDriverID);
    
    tft.setCursor(10, 80);
    tft.printf("Backlight: GPIO%d", detectedBacklightPin);
    
    // Draw color test bars
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 110);
    tft.println("Color Test:");
    
    int barHeight = 20;
    int barY = 130;
    
    tft.fillRect(10, barY, 70, barHeight, TFT_RED);
    tft.fillRect(85, barY, 70, barHeight, TFT_GREEN);
    tft.fillRect(160, barY, 70, barHeight, TFT_BLUE);
    
    // Instructions
    tft.setCursor(10, 180);
    tft.println("Check Serial Monitor");
    tft.setCursor(10, 195);
    tft.println("@ 115200 baud for");
    tft.setCursor(10, 210);
    tft.println("detailed results");
}

void loop() {
    // Blink status
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        ledState = !ledState;
        
        // Visual heartbeat
        if (ledState) {
            tft.fillCircle(220, 20, 5, TFT_GREEN);
        } else {
            tft.fillCircle(220, 20, 5, TFT_BLACK);
            tft.drawCircle(220, 20, 5, TFT_GREEN);
        }
        
        // Serial heartbeat
        Serial.print(".");
    }
}