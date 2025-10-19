// Test 1: Absolute minimal serial test
// Purpose: Verify serial works with ZERO other initialization

void setup() {
    // ONLY serial, nothing else
    Serial.begin(115200);
    
    // Give it time
    delay(2000);
    
    // Try multiple ways to output
    Serial.println("TEST1: Basic serial works!");
    Serial.flush();
    
    // Also try Serial2 in case CYD uses that
    Serial2.begin(115200);
    Serial2.println("TEST1: Serial2 output");
    
    // Prove code is running via LED
    pinMode(2, OUTPUT);  // GPIO2 is TFT_DC but also works as LED
    for(int i = 0; i < 5; i++) {
        digitalWrite(2, HIGH);
        delay(200);
        digitalWrite(2, LOW);
        delay(200);
    }
    
    Serial.println("TEST1: If you see this, serial works");
}

void loop() {
    Serial.println("Loop running");
    delay(1000);
}