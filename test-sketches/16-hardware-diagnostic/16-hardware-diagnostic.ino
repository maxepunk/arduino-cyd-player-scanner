// Test 16: MFRC522 Hardware Diagnostic
// Purpose: Diagnose MFRC522 connection issues
// Tests: Power, wiring, reset sequence, MISO/MOSI swap detection

#define RFID_SS   3   // Chip Select
#define RFID_SCK  22  // Clock
#define RFID_MOSI 27  // Master Out Slave In
#define RFID_MISO 35  // Master In Slave Out

// Additional test pin for RST if available
#define RFID_RST  -1  // RST tied to 3.3V with jumper on module!

// MFRC522 registers
#define CommandReg     0x01
#define VersionReg     0x37
#define TestPinEnReg   0x33
#define TestPinValueReg 0x34

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("TEST16: MFRC522 HARDWARE DIAGNOSTIC");
    Serial.println("====================================");
    Serial.println();
    Serial.println("User confirms: MFRC522 LED is ON (power OK)");
    Serial.println("User confirms: RST pin tied to VCC with jumper");
    Serial.println();
    Serial.println("IMPORTANT: RST tied HIGH means:");
    Serial.println("- Module never gets hardware reset");
    Serial.println("- Must use software reset command");
    Serial.println("- Power cycle needed for clean reset");
    Serial.println();
    
    // Setup pins
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT_PULLUP);  // Enable pullup to detect floating
    
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(RFID_SCK, LOW);
    digitalWrite(RFID_MOSI, LOW);
    
    // Test 1: Check MISO line state
    Serial.println("Test 1: MISO Line State Check");
    Serial.println("------------------------------");
    Serial.print("MISO pin state (with pullup): ");
    int misoState = digitalRead(RFID_MISO);
    Serial.println(misoState ? "HIGH" : "LOW");
    
    if(misoState == HIGH) {
        Serial.println("✓ MISO pulled high (normal if MFRC522 not driving it)");
    } else {
        Serial.println("⚠ MISO is LOW - could indicate short or active drive");
    }
    
    // Test 2: Test for MISO/MOSI swap
    Serial.println("\nTest 2: MISO/MOSI Swap Detection");
    Serial.println("---------------------------------");
    Serial.println("Sending pattern on MOSI, checking for echo on MISO...");
    
    digitalWrite(RFID_SS, LOW);  // Select device
    delay(1);
    
    // Send alternating pattern
    bool swapDetected = true;
    for(int i = 0; i < 8; i++) {
        int sendBit = i % 2;
        digitalWrite(RFID_MOSI, sendBit);
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        int recvBit = digitalRead(RFID_MISO);
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
        
        if(recvBit != sendBit) {
            swapDetected = false;
        }
    }
    
    digitalWrite(RFID_SS, HIGH);
    
    if(swapDetected) {
        Serial.println("⚠ WARNING: MISO seems to echo MOSI - pins may be swapped!");
    } else {
        Serial.println("✓ No direct echo detected (pins likely correct)");
    }
    
    // Test 3: Try both standard and inverted register read
    Serial.println("\nTest 3: Register Read Methods");
    Serial.println("------------------------------");
    
    // Method A: Standard MFRC522 protocol
    Serial.print("Standard read (0x80 | addr<<1): ");
    byte versionA = readRegisterStandard(VersionReg);
    Serial.print("0x");
    Serial.println(versionA, HEX);
    
    // Method B: Alternative addressing (some clones)
    Serial.print("Alternative read (addr | 0x80): ");
    byte versionB = readRegisterAlternative(VersionReg);
    Serial.print("0x");
    Serial.println(versionB, HEX);
    
    // Method C: Try with MISO/MOSI swapped in software
    Serial.print("Swapped pins read: ");
    byte versionC = readRegisterSwapped(VersionReg);
    Serial.print("0x");
    Serial.println(versionC, HEX);
    
    // Test 4: Soft reset attempt (CRITICAL with RST tied HIGH!)
    Serial.println("\nTest 4: Soft Reset Attempt");
    Serial.println("--------------------------");
    Serial.println("CRITICAL: Since RST is tied HIGH, software reset is ESSENTIAL!");
    Serial.println("Sending soft reset command (0x0F to CommandReg)...");
    writeRegister(CommandReg, 0x0F);  // SoftReset
    delay(50);  // Wait for reset to complete
    
    Serial.print("Version after reset: ");
    byte versionAfterReset = readRegisterStandard(VersionReg);
    Serial.print("0x");
    Serial.println(versionAfterReset, HEX);
    
    if(versionAfterReset == 0x91 || versionAfterReset == 0x92) {
        Serial.println("✓ SUCCESS! Soft reset fixed communication!");
    }
    
    // Test 5: Test pin functionality (if MFRC522 is responding)
    Serial.println("\nTest 5: Internal Test Pins");
    Serial.println("---------------------------");
    
    // Enable test pins
    writeRegister(TestPinEnReg, 0x80);  // Enable test pin output
    byte testValue = readRegisterStandard(TestPinValueReg);
    Serial.print("Test pin value: 0x");
    Serial.println(testValue, HEX);
    
    // Test 6: Wiring diagram
    Serial.println("\n====================================");
    Serial.println("WIRING CHECKLIST");
    Serial.println("====================================");
    Serial.println("CYD (ESP32) -> MFRC522 Module:");
    Serial.println("  GPIO3  (SS)   -> SDA (chip select)");
    Serial.println("  GPIO22 (SCK)  -> SCK (clock)");
    Serial.println("  GPIO27 (MOSI) -> MOSI");
    Serial.println("  GPIO35 (MISO) -> MISO");
    Serial.println("  3.3V          -> 3.3V");
    Serial.println("  GND           -> GND");
    Serial.println("  RST           -> 3.3V (via jumper on module)");
    Serial.println();
    
    // Analysis
    Serial.println("====================================");
    Serial.println("DIAGNOSTIC SUMMARY");
    Serial.println("====================================");
    
    if(versionA == 0x91 || versionA == 0x92) {
        Serial.println("✓ MFRC522 DETECTED with standard protocol!");
    } else if(versionB == 0x91 || versionB == 0x92) {
        Serial.println("✓ MFRC522 DETECTED with alternative protocol!");
    } else if(versionC == 0x91 || versionC == 0x92) {
        Serial.println("⚠ MFRC522 DETECTED but MISO/MOSI SWAPPED!");
    } else if(versionA == 0x00 && versionB == 0x00 && versionC == 0x00) {
        Serial.println("✗ No response - check power and SDA/SCK connections");
    } else if(versionA == 0xFF && versionB == 0xFF && versionC == 0xFF) {
        Serial.println("✗ All HIGH - check MISO connection (may be floating)");
    } else {
        Serial.println("✗ Unknown response - possible wiring issue or incompatible module");
        Serial.print("  Got values: 0x");
        Serial.print(versionA, HEX);
        Serial.print(", 0x");
        Serial.print(versionB, HEX);
        Serial.print(", 0x");
        Serial.println(versionC, HEX);
    }
    
    Serial.println("\nPOSSIBLE ISSUES:");
    if(versionA == 0xF6 || versionB == 0xF6) {
        Serial.println("- Getting 0xF6: Timing issue or wrong protocol");
    }
    if(misoState == LOW) {
        Serial.println("- MISO stuck LOW: Short circuit or wrong connection");
    }
    if(swapDetected) {
        Serial.println("- MISO/MOSI may be swapped at hardware level");
    }
}

// Standard MFRC522 read
byte readRegisterStandard(byte reg) {
    digitalWrite(RFID_SS, LOW);
    
    // Send: 1XXXXXX0 where XXXXXX = address
    spiTransfer(0x80 | ((reg & 0x3F) << 1));
    byte value = spiTransfer(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    return value;
}

// Alternative addressing
byte readRegisterAlternative(byte reg) {
    digitalWrite(RFID_SS, LOW);
    
    // Send: address with bit 7 set
    spiTransfer(reg | 0x80);
    byte value = spiTransfer(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    return value;
}

// Try with MISO/MOSI swapped
byte readRegisterSwapped(byte reg) {
    digitalWrite(RFID_SS, LOW);
    
    // Send on MISO, read from MOSI (swapped)
    spiTransferSwapped(0x80 | ((reg & 0x3F) << 1));
    byte value = spiTransferSwapped(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    return value;
}

// Standard write
void writeRegister(byte reg, byte value) {
    digitalWrite(RFID_SS, LOW);
    
    // Send: 0XXXXXX0 where XXXXXX = address
    spiTransfer((reg & 0x3F) << 1);
    spiTransfer(value);
    
    digitalWrite(RFID_SS, HIGH);
}

// Normal SPI transfer
byte spiTransfer(byte data) {
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

// Swapped SPI transfer (MISO/MOSI reversed)
byte spiTransferSwapped(byte data) {
    byte result = 0;
    
    for(int i = 0; i < 8; i++) {
        // Send on MISO pin (35), read from MOSI pin (27)
        digitalWrite(RFID_MISO, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MOSI);
        
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    return result;
}

void loop() {
    delay(5000);
    
    // Keep checking
    byte v = readRegisterStandard(VersionReg);
    Serial.print("Periodic check: 0x");
    Serial.print(v, HEX);
    
    if(v == 0x91 || v == 0x92) {
        Serial.println(" - MFRC522 responding!");
    } else {
        Serial.println(" - Still not responding");
    }
}