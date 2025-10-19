// Audio Isolation Test - Identify the beeping source
// Tests which pins are causing speaker noise

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== CYD Audio Isolation Test ===");
    
    // First, silence ALL potential audio-related pins
    Serial.println("\n1. Silencing all potential audio pins...");
    
    // DAC pins
    pinMode(25, OUTPUT);
    pinMode(26, OUTPUT);
    digitalWrite(25, LOW);
    digitalWrite(26, LOW);
    
    // Common amplifier shutdown/enable pins on CYD boards
    pinMode(4, OUTPUT);   // Sometimes used for amp enable
    digitalWrite(4, LOW); // Try to disable amp
    
    // Pin 27 - The RFID MOSI pin (SUSPECT!)
    pinMode(27, OUTPUT);
    digitalWrite(27, LOW);
    
    // Other potential audio-related pins
    pinMode(0, OUTPUT);   // Sometimes boot pin affects audio
    digitalWrite(0, HIGH); // Keep high to prevent boot issues
    
    Serial.println("All pins set. Listen for 5 seconds...");
    Serial.println("You should hear SILENCE if pins are the issue");
    delay(5000);
    
    Serial.println("\n2. Testing Pin 27 (RFID MOSI) - PRIMARY SUSPECT");
    Serial.println("Toggling pin 27 at 100ms intervals (simulating RFID polling)");
    Serial.println("If you hear beeping now, PIN 27 IS THE CULPRIT!");
}

void loop() {
    // Simulate RFID polling pattern on pin 27
    static uint32_t lastToggle = 0;
    static bool state = false;
    
    if (millis() - lastToggle > 100) {
        lastToggle = millis();
        
        // Simulate software SPI burst (like RFID polling)
        for (int i = 0; i < 8; i++) {
            digitalWrite(27, HIGH);
            delayMicroseconds(2);
            digitalWrite(27, LOW);
            delayMicroseconds(2);
        }
        
        state = !state;
        if (state) {
            Serial.print(".");
        }
    }
    
    // Every 10 seconds, remind user what we're testing
    static uint32_t lastReminder = 0;
    if (millis() - lastReminder > 10000) {
        lastReminder = millis();
        Serial.println("\n\nIf you hear beeping synchronized with the dots above,");
        Serial.println("then Pin 27 is electrically coupled to your speaker circuit!");
        Serial.println("\nPossible solutions:");
        Serial.println("1. Add a pull-down resistor on pin 27");
        Serial.println("2. Keep pin 27 LOW when not actively using RFID");
        Serial.println("3. Use a different pin for RFID MOSI");
        Serial.println("4. Add capacitor filtering on amplifier input");
    }
}