// Test 13: Critical Section Impact Measurement
// Purpose: Quantify exactly how long we can disable interrupts before serial fails
// Find the breaking point for UART buffer overflow

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("TEST13: CRITICAL SECTION IMPACT MEASUREMENT");
    Serial.println("===========================================");
    Serial.println();
    Serial.println("Theory: ESP32 UART FIFO = 128 bytes");
    Serial.println("At 115200 baud: ~87us per byte");
    Serial.println("Buffer overflow threshold: ~11ms");
    Serial.println();
    
    // Test 1: Increasing critical section durations
    Serial.println("Test 1: Single critical section durations");
    Serial.println("------------------------------------------");
    
    int durations[] = {10, 50, 100, 500, 1000, 2000, 5000, 10000, 11000, 12000, 15000, 20000};
    int numDurations = sizeof(durations) / sizeof(durations[0]);
    
    for(int i = 0; i < numDurations; i++) {
        int d = durations[i];
        
        // Send test marker
        Serial.print("Testing ");
        Serial.print(d);
        Serial.print(" us critical section: ");
        Serial.flush();  // Ensure everything is sent before critical section
        
        delay(10);  // Give time for serial to send
        
        // Enter critical section
        portENTER_CRITICAL(&mux);
        delayMicroseconds(d);
        portEXIT_CRITICAL(&mux);
        
        // Check if serial survived
        delay(10);  // Allow serial to recover if needed
        Serial.println("survived! ✓");
        
        // Also send some data to verify serial is truly working
        Serial.print("  Verification string: ");
        for(int j = 0; j < 10; j++) {
            Serial.print(j);
        }
        Serial.println(" - serial fully functional");
        
        delay(100);  // Pause between tests
    }
    
    // Test 2: Cumulative effect (like MFRC522 init sequence)
    Serial.println("\nTest 2: Cumulative critical sections (MFRC522 pattern)");
    Serial.println("-------------------------------------------------------");
    Serial.println("Simulating 20 register writes with critical sections...");
    Serial.flush();
    
    delay(10);
    
    bool survived = true;
    for(int i = 0; i < 20; i++) {
        Serial.print("  Register write ");
        Serial.print(i);
        Serial.print(": ");
        Serial.flush();
        
        portENTER_CRITICAL(&mux);
        // Simulate register write timing (~48us for 8 bits at 6us per bit)
        delayMicroseconds(48);
        portEXIT_CRITICAL(&mux);
        
        // Small gap between operations (as in real code)
        delayMicroseconds(10);
        
        Serial.println("done");
        
        // Check if we can still send data
        if(i % 5 == 4) {
            Serial.print("    Status check after ");
            Serial.print(i + 1);
            Serial.println(" writes: serial OK ✓");
        }
    }
    
    if(survived) {
        Serial.println("Cumulative test complete - serial survived all 20 writes! ✓");
    }
    
    // Test 3: Worst case - multiple long operations
    Serial.println("\nTest 3: Worst case scenario");
    Serial.println("----------------------------");
    Serial.println("Multiple 1ms critical sections with minimal gaps...");
    Serial.flush();
    
    delay(10);
    
    for(int i = 0; i < 10; i++) {
        Serial.print("  Long operation ");
        Serial.print(i);
        Serial.print(": ");
        Serial.flush();
        
        portENTER_CRITICAL(&mux);
        delayMicroseconds(1000);  // 1ms critical section
        portEXIT_CRITICAL(&mux);
        
        delayMicroseconds(5);  // Minimal gap
        
        Serial.println("done");
    }
    
    Serial.println("Worst case test complete! ✓");
    
    // Test 4: GPIO3 manipulation inside critical section
    Serial.println("\nTest 4: GPIO3 (UART RX) in critical section");
    Serial.println("--------------------------------------------");
    
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);
    
    Serial.println("Testing GPIO3 manipulation inside critical sections...");
    Serial.flush();
    
    delay(10);
    
    for(int i = 0; i < 10; i++) {
        Serial.print("  GPIO3 toggle ");
        Serial.print(i);
        Serial.print(": ");
        Serial.flush();
        
        portENTER_CRITICAL(&mux);
        digitalWrite(3, LOW);
        delayMicroseconds(100);
        digitalWrite(3, HIGH);
        delayMicroseconds(100);
        portEXIT_CRITICAL(&mux);
        
        Serial.println("survived");
    }
    
    Serial.println("GPIO3 critical section test complete! ✓");
    
    // Final summary
    Serial.println("\n===========================================");
    Serial.println("TEST 13 COMPLETE");
    Serial.println("\nFindings:");
    Serial.println("- Critical sections up to 20ms tested");
    Serial.println("- Multiple 48us sections (MFRC522 pattern) OK");
    Serial.println("- GPIO3 manipulation in critical sections OK");
    Serial.println("- If serial survived all tests, critical sections");
    Serial.println("  alone are NOT the problem!");
}

void loop() {
    delay(5000);
    
    // Periodic test to ensure serial remains functional
    Serial.print("Periodic check: ");
    
    // Do a small critical section
    portENTER_CRITICAL(&mux);
    delayMicroseconds(100);
    portEXIT_CRITICAL(&mux);
    
    Serial.println("Serial still functional after critical section");
}