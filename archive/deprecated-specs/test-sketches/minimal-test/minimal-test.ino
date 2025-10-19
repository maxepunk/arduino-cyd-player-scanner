// Minimal test - NO display libraries
// Just serial and LED blinking

void setup() {
    // Initialize serial first
    Serial.begin(115200);
    
    // Set up built-in LED
    pinMode(2, OUTPUT);  // Built-in LED on most ESP32 boards
    
    // Flash LED to show we're starting
    for(int i = 0; i < 5; i++) {
        digitalWrite(2, HIGH);
        delay(100);
        digitalWrite(2, LOW);
        delay(100);
    }
    
    Serial.println("\n\n=== MINIMAL TEST ===");
    Serial.println("ESP32 is alive!");
    Serial.println("No display libraries loaded");
    
    // Test basic GPIO
    Serial.println("\nTesting GPIOs:");
    
    // Test each pin we care about
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    Serial.println("GPIO21: Set HIGH (backlight single USB)");
    
    pinMode(27, OUTPUT);
    digitalWrite(27, LOW);
    Serial.println("GPIO27: Set LOW (RFID MOSI / backlight dual USB)");
    
    pinMode(22, OUTPUT);
    Serial.println("GPIO22: Configured (RFID SCK)");
    
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);
    Serial.println("GPIO3: Set HIGH (RFID SS)");
    
    pinMode(35, INPUT);
    int miso = digitalRead(35);
    Serial.print("GPIO35: Read ");
    Serial.println(miso);
    
    Serial.println("\n=== Setup Complete ===");
}

void loop() {
    static unsigned long lastTime = 0;
    static int count = 0;
    
    if (millis() - lastTime > 1000) {
        lastTime = millis();
        Serial.print("Heartbeat: ");
        Serial.println(++count);
        
        // Toggle LED
        digitalWrite(2, (count % 2) ? HIGH : LOW);
    }
}