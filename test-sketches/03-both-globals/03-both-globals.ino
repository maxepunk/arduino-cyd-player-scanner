// Test 03: Both TFT_eSPI and SPIClass globals like ALNScanner
// Purpose: Test if combination breaks serial

#include <SPI.h>
#include <TFT_eSPI.h>

// BOTH GLOBALS like ALNScanner has
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);

void setup() {
    // Serial FIRST
    Serial.begin(115200);
    delay(2000);  // Longer delay to catch output
    
    Serial.println("\n=== TEST03: Both globals ===");
    Serial.println("Serial works with TFT_eSPI + SPIClass globals");
    
    // Now init display
    Serial.println("Calling tft.init()...");
    tft.init();
    Serial.println("tft.init() done");
    
    // Test SDSPI
    Serial.println("Calling SDSPI.begin()...");
    SDSPI.begin(18, 19, 23);  // SD card pins
    Serial.println("SDSPI.begin() done");
    
    // Now the RFID pins like ALNScanner
    Serial.println("Setting up RFID pins...");
    pinMode(22, OUTPUT);  // SOFT_SPI_SCK
    pinMode(27, OUTPUT);  // SOFT_SPI_MOSI
    pinMode(35, INPUT);   // SOFT_SPI_MISO
    
    Serial.println("About to set GPIO3 as RFID_SS...");
    Serial.flush();
    
    pinMode(3, OUTPUT);   // RFID_SS - THE SUSPECT!
    digitalWrite(3, HIGH);
    
    Serial.println("GPIO3 configured - can you still see this?");
    Serial.flush();
}

void loop() {
    Serial.println("Loop: Serial still works!");
    delay(2000);
}