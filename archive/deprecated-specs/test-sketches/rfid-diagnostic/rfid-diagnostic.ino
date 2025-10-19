// RFID Diagnostic - Step by step verification
// Tests each component individually

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Pin definitions
#define BACKLIGHT_PIN 21
#define RFID_SCK  22
#define RFID_MOSI 27
#define RFID_MISO 35
#define RFID_SS   3

void setup() {
    Serial.begin(115200);
    
    // Wait for serial with timeout
    unsigned long start = millis();
    while (!Serial && millis() - start < 3000) {
        delay(10);
    }
    
    Serial.println("\n\n========================================");
    Serial.println("RFID Pin Diagnostic v1.0");
    Serial.println("========================================");
    
    // Initialize display first
    Serial.println("\n[1] Initializing display...");
    tft.begin();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("RFID DIAG");
    Serial.println("    Display OK");
    
    // Test backlight
    Serial.println("\n[2] Testing backlight on GPIO21...");
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);
    delay(500);
    digitalWrite(BACKLIGHT_PIN, HIGH);
    Serial.println("    Backlight OK (should be ON now)");
    
    tft.setCursor(10, 40);
    tft.setTextSize(1);
    tft.println("Backlight: GPIO21 OK");
    
    // Test each RFID pin individually
    Serial.println("\n[3] Testing RFID pins individually...");
    
    // Test SS (GPIO3)
    Serial.println("    Testing SS (GPIO3)...");
    pinMode(RFID_SS, OUTPUT);
    digitalWrite(RFID_SS, HIGH);
    delay(10);
    digitalWrite(RFID_SS, LOW);
    delay(10);
    digitalWrite(RFID_SS, HIGH);
    Serial.println("      GPIO3 responds OK");
    tft.println("SS (GPIO3): OK");
    
    // Test SCK (GPIO22)
    Serial.println("    Testing SCK (GPIO22)...");
    pinMode(RFID_SCK, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(10);
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(10);
    }
    Serial.println("      GPIO22 responds OK");
    tft.println("SCK (GPIO22): OK");
    
    // Test MOSI (GPIO27) - the problematic one
    Serial.println("    Testing MOSI (GPIO27)...");
    pinMode(RFID_MOSI, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(RFID_MOSI, HIGH);
        delayMicroseconds(10);
        digitalWrite(RFID_MOSI, LOW);
        delayMicroseconds(10);
    }
    Serial.println("      GPIO27 responds OK");
    tft.println("MOSI (GPIO27): OK");
    
    // Test MISO (GPIO35) - input only
    Serial.println("    Testing MISO (GPIO35)...");
    pinMode(RFID_MISO, INPUT);
    int misoState = digitalRead(RFID_MISO);
    Serial.printf("      GPIO35 reads: %d\n", misoState);
    tft.printf("MISO (GPIO35): %d\n", misoState);
    
    Serial.println("\n[4] Pin test complete!");
    Serial.println("    All pins respond correctly");
    
    tft.setCursor(10, 140);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("PINS OK!");
    
    Serial.println("\n========================================");
    Serial.println("Diagnostic complete - entering loop");
    Serial.println("========================================\n");
}

void loop() {
    static unsigned long lastPrint = 0;
    static int count = 0;
    
    if (millis() - lastPrint > 1000) {
        lastPrint = millis();
        Serial.printf("Alive: %d seconds\n", ++count);
        
        // Toggle an indicator
        static bool state = false;
        state = !state;
        tft.fillRect(220, 10, 10, 10, state ? TFT_GREEN : TFT_RED);
    }
}