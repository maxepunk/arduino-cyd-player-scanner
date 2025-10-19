// Test 08: Include MFRC522 library but don't use it
// Testing if the library itself breaks serial

#include <SPI.h>
#include <MFRC522.h>

void setup() {
    // LED proof of life
    pinMode(2, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(50);
        digitalWrite(2, LOW);
        delay(50);
    }
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("TEST08: MFRC522 library included");
    Serial.println("If you see this, including MFRC522.h doesn't break serial");
    Serial.flush();
}

void loop() {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(900);
    
    Serial.println("Still running with MFRC522 included...");
    delay(2000);
}