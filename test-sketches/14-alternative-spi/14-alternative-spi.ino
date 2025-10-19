// Test 14: Alternative SPI Strategies
// Purpose: Test different approaches to balance timing accuracy vs serial survival
// Compare: Full critical, minimal critical, no critical, different SS pin

#define RFID_SS   3   // Original (conflicts with UART RX)
#define RFID_SS_ALT 4 // Alternative SS pin (if available)
#define RFID_SCK  22
#define RFID_MOSI 27
#define RFID_MISO 35

#define MFRC522_REG_VERSION 0x37

portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Strategy 1: Full critical section (original ALNScanner approach)
byte spi_transfer_full_critical(byte data) {
    byte result = 0;
    
    portENTER_CRITICAL(&spiMux);
    
    for(int i = 0; i < 8; i++) {
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MISO);
        
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    portEXIT_CRITICAL(&spiMux);
    
    return result;
}

// Strategy 2: Minimal critical sections (only around pin changes)
byte spi_transfer_minimal_critical(byte data) {
    byte result = 0;
    
    for(int i = 0; i < 8; i++) {
        // Critical only for output pin changes
        portENTER_CRITICAL(&spiMux);
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        digitalWrite(RFID_SCK, HIGH);
        portEXIT_CRITICAL(&spiMux);
        
        data <<= 1;
        delayMicroseconds(2);
        
        // Read without critical
        result = (result << 1) | digitalRead(RFID_MISO);
        
        // Critical only for clock falling edge
        portENTER_CRITICAL(&spiMux);
        digitalWrite(RFID_SCK, LOW);
        portEXIT_CRITICAL(&spiMux);
        
        delayMicroseconds(2);
    }
    
    return result;
}

// Strategy 3: No critical sections with precise timing
byte spi_transfer_no_critical(byte data) {
    byte result = 0;
    
    for(int i = 0; i < 8; i++) {
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MISO);
        
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    return result;
}

// Strategy 4: Yield between bytes (for multi-byte operations)
byte spi_transfer_with_yield(byte data) {
    byte result = 0;
    
    for(int i = 0; i < 8; i++) {
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MISO);
        
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
        
        // Yield every 4 bits to allow other tasks
        if(i == 3) {
            yield();  // Allow other tasks to run
        }
    }
    
    return result;
}

// Read register with specified strategy
byte read_register(byte reg, byte (*transfer_func)(byte)) {
    digitalWrite(RFID_SS, LOW);
    
    // Send read command
    transfer_func(0x80 | ((reg & 0x3F) << 1));
    
    // Read response
    byte value = transfer_func(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    
    return value;
}

void test_strategy(const char* name, byte (*transfer_func)(byte)) {
    Serial.print("\n");
    Serial.println(name);
    Serial.println("----------------------------------------");
    
    // Test 1: Read version register
    Serial.print("Version register read: ");
    Serial.flush();
    
    byte version = read_register(MFRC522_REG_VERSION, transfer_func);
    Serial.print("0x");
    Serial.print(version, HEX);
    
    if(version == 0x91 || version == 0x92) {
        Serial.println(" ✓ MFRC522 detected");
    } else {
        Serial.println(" ✗ Communication failed");
    }
    
    // Test 2: Serial survival test
    Serial.print("Serial survival test: ");
    Serial.flush();
    
    for(int i = 0; i < 20; i++) {
        read_register(MFRC522_REG_VERSION, transfer_func);
        delayMicroseconds(10);
    }
    
    Serial.println("survived 20 operations ✓");
    
    // Test 3: Consistency test
    Serial.print("Consistency test (10 reads): ");
    bool consistent = true;
    byte firstRead = read_register(MFRC522_REG_VERSION, transfer_func);
    
    for(int i = 0; i < 10; i++) {
        byte v = read_register(MFRC522_REG_VERSION, transfer_func);
        if(v != firstRead) {
            consistent = false;
            break;
        }
        delay(1);
    }
    
    if(consistent) {
        Serial.println("consistent ✓");
    } else {
        Serial.println("inconsistent ✗");
    }
    
    // Test 4: Performance measurement
    Serial.print("Performance (100 reads): ");
    unsigned long startTime = micros();
    
    for(int i = 0; i < 100; i++) {
        read_register(MFRC522_REG_VERSION, transfer_func);
    }
    
    unsigned long duration = micros() - startTime;
    Serial.print(duration);
    Serial.print(" us total, ");
    Serial.print(duration / 100);
    Serial.println(" us per read");
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("TEST14: ALTERNATIVE SPI STRATEGIES");
    Serial.println("==================================");
    Serial.println();
    Serial.println("Testing different SPI implementation strategies");
    Serial.println("to find optimal balance between reliability and");
    Serial.println("serial communication survival.");
    
    // Setup pins
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT);
    
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(RFID_SCK, LOW);
    digitalWrite(RFID_MOSI, LOW);
    
    // Test each strategy
    test_strategy("Strategy 1: FULL CRITICAL SECTION", spi_transfer_full_critical);
    delay(500);
    
    test_strategy("Strategy 2: MINIMAL CRITICAL SECTIONS", spi_transfer_minimal_critical);
    delay(500);
    
    test_strategy("Strategy 3: NO CRITICAL SECTIONS", spi_transfer_no_critical);
    delay(500);
    
    test_strategy("Strategy 4: WITH YIELD", spi_transfer_with_yield);
    delay(500);
    
    // Test alternative SS pin if wired
    Serial.println("\n==================================");
    Serial.println("Alternative SS Pin Test (GPIO4)");
    Serial.println("==================================");
    Serial.println("NOTE: This requires rewiring RFID_SS from GPIO3 to GPIO4");
    Serial.println("Attempting test with GPIO4...");
    
    pinMode(RFID_SS_ALT, OUTPUT);
    digitalWrite(RFID_SS_ALT, HIGH);
    
    // Quick test with alternative pin
    digitalWrite(RFID_SS_ALT, LOW);
    spi_transfer_no_critical(0x80 | ((MFRC522_REG_VERSION & 0x3F) << 1));
    byte altVersion = spi_transfer_no_critical(0x00);
    digitalWrite(RFID_SS_ALT, HIGH);
    
    Serial.print("Alternative pin result: 0x");
    Serial.print(altVersion, HEX);
    if(altVersion == 0x91 || altVersion == 0x92) {
        Serial.println(" ✓ Works with GPIO4!");
    } else {
        Serial.println(" - Not connected/wired");
    }
    
    // Summary
    Serial.println("\n==================================");
    Serial.println("TEST 14 COMPLETE");
    Serial.println("\nSummary:");
    Serial.println("- Full critical: Might break serial on some operations");
    Serial.println("- Minimal critical: Better serial survival");
    Serial.println("- No critical: Best for serial, might affect SPI timing");
    Serial.println("- With yield: Good compromise for long operations");
    Serial.println("- Alt SS pin: Requires rewiring but avoids GPIO3 conflict");
}

void loop() {
    delay(5000);
    Serial.println("Test 14 still running - serial functional");
}