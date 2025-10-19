// Test 09: Include MFRC522 and declare Uid global
// Testing if MFRC522::Uid breaks serial

#include <SPI.h>
#include <MFRC522.h>

// Global like in ALNScanner
MFRC522::Uid uid;

void setup() {
    // LED proof of life - longer pattern
    pinMode(2, OUTPUT);
    for(int i = 0; i < 5; i++) {
        digitalWrite(2, HIGH);
        delay(200);
        digitalWrite(2, LOW);
        delay(200);
    }
    
    Serial.begin(115200);
    delay(3000);  // LONGER delay to ensure serial is ready
    
    Serial.println("TEST09: MFRC522::Uid global declared");
    Serial.println("If you see this, Uid global doesn't break serial");
    Serial.println("But we might have missed earlier messages!");
    Serial.flush();
}

void loop() {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(900);
    
    Serial.println("Loop with MFRC522::Uid global...");
    delay(2000);
}