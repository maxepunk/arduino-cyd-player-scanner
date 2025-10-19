// Test 10: TFT_eSPI + MFRC522 combination
// This should reproduce the ALNScanner failure

#include <SPI.h>
#include <TFT_eSPI.h>    // Currently configured for ILI9341
#include <MFRC522.h>

// Globals exactly like ALNScanner
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
MFRC522::Uid uid;

void setup() {
    // LED proof of life FIRST
    pinMode(2, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(100);
        digitalWrite(2, LOW);
        delay(100);
    }
    
    Serial.begin(115200);
    delay(3000);  // Long delay for MFRC522
    
    Serial.println("TEST10: TFT_eSPI + MFRC522 + SDSPI combo");
    Serial.println("This mimics ALNScanner globals");
    Serial.println("If no more output, combo kills serial on ST7789");
    Serial.flush();
    
    // Initialize TFT like ALNScanner
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Test10", 10, 10, 2);
    
    Serial.println("Display initialized - can you see this?");
    Serial.flush();
    
    // Initialize SDSPI like ALNScanner
    SDSPI.begin(18, 19, 23);
    
    Serial.println("SDSPI initialized - serial still working?");
    Serial.flush();
}

void loop() {
    static int count = 0;
    
    // Blink LED
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(900);
    
    // Update display to prove code runs
    tft.drawString("Loop: " + String(count++), 10, 30, 2);
    
    Serial.println("Loop " + String(count) + " - if you see this, serial survived");
    delay(2000);
}