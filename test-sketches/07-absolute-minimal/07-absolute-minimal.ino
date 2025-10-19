// Test 07: Absolute minimal - ONLY Serial and LED
// No libraries, no globals, nothing else

void setup() {
    // LED proof of life
    pinMode(2, OUTPUT);
    for(int i = 0; i < 20; i++) {  // Long blink so we can't miss it
        digitalWrite(2, HIGH);
        delay(100);
        digitalWrite(2, LOW);
        delay(100);
    }
    
    // ONLY Serial.begin and print
    Serial.begin(115200);
    delay(2000);  // Extra long delay
    
    Serial.println("TEST07: MINIMAL SERIAL TEST");
    Serial.println("If you see this, serial works");
    Serial.flush();
}

void loop() {
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    delay(500);
    
    Serial.println("Loop running...");
    Serial.flush();
    delay(1000);
}