// Test 01: Absolute minimal serial test
// Purpose: Verify serial works with NO other code

void setup() {
    // Absolute minimum - just serial
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    
    Serial.println("TEST01: Serial is working!");
    Serial.println("If you see this, basic serial works");
    
    // Blink built-in LED to show code is running
    pinMode(2, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(100);
        digitalWrite(2, LOW);
        delay(100);
        Serial.print("Blink ");
        Serial.println(i);
    }
}

void loop() {
    static int counter = 0;
    Serial.print("Loop: ");
    Serial.println(counter++);
    delay(1000);
}