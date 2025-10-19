// Test 06: LED proof of life BEFORE serial
// Purpose: Prove ESP32 runs even if serial fails

void setup() {
    // LED FIRST - prove we're running
    pinMode(2, OUTPUT);  // GPIO2 - also TFT_DC but works as LED
    
    // Fast blink = we're alive
    for(int i = 0; i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(50);
        digitalWrite(2, LOW);
        delay(50);
    }
    
    // Now try serial
    Serial.begin(115200);
    delay(1000);
    
    // Slower blink = serial initialized
    for(int i = 0; i < 5; i++) {
        digitalWrite(2, HIGH);
        delay(200);
        digitalWrite(2, LOW);
        delay(200);
    }
    
    Serial.println("TEST06: If you see this, serial works");
    Serial.println("But LED should have blinked regardless");
}

void loop() {
    // Heartbeat
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(900);
    
    Serial.println("Heartbeat");
}