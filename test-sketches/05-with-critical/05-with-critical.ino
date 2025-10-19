// Test 05: Test FreeRTOS critical sections
// Purpose: Test if portMUX critical sections break serial

#include <SPI.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>

TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
MFRC522::Uid uid;

// FreeRTOS mutex like ALNScanner
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Simulate software SPI with critical section
byte softSPI_transfer(byte data) {
    byte result = 0;
    
    Serial.println("About to enter critical section...");
    Serial.flush();
    
    // CRITICAL SECTION - disables interrupts!
    portENTER_CRITICAL(&spiMux);
    
    // Simulate SPI bit-banging
    for (int i = 0; i < 8; ++i) {
        digitalWrite(27, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        delayMicroseconds(2);
        digitalWrite(22, HIGH);
        delayMicroseconds(2);
        if (digitalRead(35)) {
            result |= (1 << (7 - i));
        }
        digitalWrite(22, LOW);
        delayMicroseconds(2);
    }
    
    portEXIT_CRITICAL(&spiMux);
    
    Serial.println("Exited critical section");
    Serial.flush();
    
    return result;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== TEST05: Critical Sections ===");
    
    // Setup pins
    pinMode(22, OUTPUT);  // SCK
    pinMode(27, OUTPUT);  // MOSI
    pinMode(35, INPUT);   // MISO
    pinMode(3, OUTPUT);   // SS
    digitalWrite(3, HIGH);
    
    Serial.println("Pins configured");
    
    // Test critical section
    Serial.println("Testing software SPI with critical section...");
    byte result = softSPI_transfer(0x55);
    Serial.print("Transfer result: 0x");
    Serial.println(result, HEX);
    
    Serial.println("=== Critical section test complete ===");
}

void loop() {
    Serial.println("Loop: Critical sections didn't break serial!");
    delay(2000);
}