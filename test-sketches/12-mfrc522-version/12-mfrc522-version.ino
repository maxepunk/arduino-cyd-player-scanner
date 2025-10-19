// Test 12: MFRC522 Version Register Read
// Purpose: Verify basic communication with MFRC522 chip
// Read version register (0x37) - should return 0x91 or 0x92
// 0x00 or 0xFF = no communication, random = timing issue

#define RFID_SS   3   // RFID Chip Select (SHARED WITH UART RX!)
#define RFID_SCK  22  // RFID Clock
#define RFID_MOSI 27  // RFID Master Out Slave In
#define RFID_MISO 35  // RFID Master In Slave Out

// MFRC522 registers
#define MFRC522_REG_VERSION 0x37

portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Software SPI transfer without critical section
byte spi_transfer_no_critical(byte data) {
    byte result = 0;
    
    for(int i = 0; i < 8; i++) {
        // Set MOSI
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        // Clock rising edge
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        
        // Read MISO on rising edge
        result = (result << 1) | digitalRead(RFID_MISO);
        
        // Clock falling edge
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    return result;
}

// Software SPI transfer with critical section
byte spi_transfer_with_critical(byte data) {
    byte result = 0;
    
    portENTER_CRITICAL(&spiMux);
    
    for(int i = 0; i < 8; i++) {
        // Set MOSI
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        // Clock rising edge
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        
        // Read MISO on rising edge
        result = (result << 1) | digitalRead(RFID_MISO);
        
        // Clock falling edge
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    portEXIT_CRITICAL(&spiMux);
    
    return result;
}

// Read MFRC522 register without critical section
byte read_register_no_critical(byte reg) {
    digitalWrite(RFID_SS, LOW);
    
    // Send read command (bit 7 = 1 for read, bits 1-6 = register address, bit 0 = 0)
    spi_transfer_no_critical(0x80 | ((reg & 0x3F) << 1));
    
    // Read response
    byte value = spi_transfer_no_critical(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    
    return value;
}

// Read MFRC522 register with critical section
byte read_register_with_critical(byte reg) {
    digitalWrite(RFID_SS, LOW);
    
    // Send read command (bit 7 = 1 for read, bits 1-6 = register address, bit 0 = 0)
    spi_transfer_with_critical(0x80 | ((reg & 0x3F) << 1));
    
    // Read response
    byte value = spi_transfer_with_critical(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    
    return value;
}

void setup() {
    Serial.begin(115200);
    delay(3000);  // MFRC522 library delay pattern
    
    Serial.println("TEST12: MFRC522 VERSION REGISTER READ");
    Serial.println("======================================");
    Serial.println();
    
    // Setup pins
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT);
    
    // Initial state for SPI Mode 0
    digitalWrite(RFID_SS, HIGH);  // SS idle high
    digitalWrite(RFID_SCK, LOW);  // Clock idle LOW (CPOL=0)
    digitalWrite(RFID_MOSI, LOW);
    
    Serial.println("Expected values:");
    Serial.println("  0x91 or 0x92 = MFRC522 detected and communicating");
    Serial.println("  0x00 or 0xFF = No communication (wiring issue?)");
    Serial.println("  Random values = Timing or protocol issue");
    Serial.println();
    
    // Test 1: Read without critical section
    Serial.println("Test 1: Version register WITHOUT critical section:");
    byte version1 = read_register_no_critical(MFRC522_REG_VERSION);
    Serial.print("  Version register (0x37) = 0x");
    Serial.print(version1, HEX);
    
    if(version1 == 0x91 || version1 == 0x92) {
        Serial.println(" ✓ MFRC522 v1.0 or v2.0 detected!");
    } else if(version1 == 0x00) {
        Serial.println(" ✗ No response (all zeros)");
    } else if(version1 == 0xFF) {
        Serial.println(" ✗ No response (all ones)");
    } else {
        Serial.println(" ? Unexpected value");
    }
    
    delay(100);
    
    // Test 2: Read with critical section
    Serial.println("\nTest 2: Version register WITH critical section:");
    byte version2 = read_register_with_critical(MFRC522_REG_VERSION);
    Serial.print("  Version register (0x37) = 0x");
    Serial.print(version2, HEX);
    
    if(version2 == 0x91 || version2 == 0x92) {
        Serial.println(" ✓ MFRC522 detected!");
    } else if(version2 == 0x00) {
        Serial.println(" ✗ No response (all zeros)");
    } else if(version2 == 0xFF) {
        Serial.println(" ✗ No response (all ones)");
    } else {
        Serial.println(" ? Unexpected value");
    }
    
    // Test 3: Multiple reads for consistency check
    Serial.println("\nTest 3: Consistency check (10 reads without critical):");
    bool consistent = true;
    byte firstValue = read_register_no_critical(MFRC522_REG_VERSION);
    
    for(int i = 0; i < 10; i++) {
        byte v = read_register_no_critical(MFRC522_REG_VERSION);
        Serial.print("  Read ");
        Serial.print(i);
        Serial.print(": 0x");
        Serial.print(v, HEX);
        
        if(v != firstValue) {
            consistent = false;
            Serial.println(" ✗ Inconsistent!");
        } else {
            Serial.println(" ✓");
        }
        
        delay(10);
    }
    
    if(consistent) {
        Serial.println("  All reads consistent ✓");
    } else {
        Serial.println("  Inconsistent reads - timing issue? ✗");
    }
    
    // Test 4: Multiple reads WITH critical section
    Serial.println("\nTest 4: Consistency check (10 reads WITH critical):");
    consistent = true;
    firstValue = read_register_with_critical(MFRC522_REG_VERSION);
    
    for(int i = 0; i < 10; i++) {
        byte v = read_register_with_critical(MFRC522_REG_VERSION);
        Serial.print("  Read ");
        Serial.print(i);
        Serial.print(": 0x");
        Serial.print(v, HEX);
        
        if(v != firstValue) {
            consistent = false;
            Serial.println(" ✗ Inconsistent!");
        } else {
            Serial.println(" ✓");
        }
        
        delay(10);
    }
    
    if(consistent) {
        Serial.println("  All reads consistent ✓");
    } else {
        Serial.println("  Inconsistent reads - critical section doesn't help ✗");
    }
    
    Serial.println("\n======================================");
    Serial.println("TEST 12 COMPLETE");
    Serial.println("\nSummary:");
    Serial.print("  Without critical: 0x");
    Serial.println(version1, HEX);
    Serial.print("  With critical:    0x");
    Serial.println(version2, HEX);
    
    if((version1 == 0x91 || version1 == 0x92) && (version2 == 0x91 || version2 == 0x92)) {
        Serial.println("\n✓ MFRC522 communication working!");
    } else {
        Serial.println("\n✗ MFRC522 communication failed - check wiring");
    }
}

void loop() {
    delay(5000);
    
    // Keep reading version to check if serial survives
    byte v = read_register_no_critical(MFRC522_REG_VERSION);
    Serial.print("Periodic read: 0x");
    Serial.print(v, HEX);
    Serial.println(" - serial still functional");
}