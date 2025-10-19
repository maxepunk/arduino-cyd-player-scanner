// Test 15: Progressive MFRC522 Initialization
// Purpose: Find EXACTLY which register operation breaks serial
// Progressively add initialization steps until serial fails

#define RFID_SS   3
#define RFID_SCK  22
#define RFID_MOSI 27
#define RFID_MISO 35

// MFRC522 Registers (from datasheet)
#define CommandReg      0x01
#define ComIEnReg       0x02
#define ComIrqReg       0x04
#define TxModeReg       0x12
#define RxModeReg       0x13
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define ModeReg         0x11
#define TxAutoReg       0x15
#define VersionReg      0x37

portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Software SPI transfer (using minimal critical sections)
byte spi_transfer(byte data) {
    byte result = 0;
    
    for(int i = 0; i < 8; i++) {
        // Only critical for pin changes
        portENTER_CRITICAL(&spiMux);
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        digitalWrite(RFID_SCK, HIGH);
        portEXIT_CRITICAL(&spiMux);
        
        data <<= 1;
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MISO);
        
        portENTER_CRITICAL(&spiMux);
        digitalWrite(RFID_SCK, LOW);
        portEXIT_CRITICAL(&spiMux);
        
        delayMicroseconds(2);
    }
    
    return result;
}

// Write to MFRC522 register
void write_register(byte reg, byte value) {
    digitalWrite(RFID_SS, LOW);
    
    // Send write command (bit 7 = 0 for write, bits 1-6 = register address, bit 0 = 0)
    spi_transfer((reg & 0x3F) << 1);
    
    // Send value
    spi_transfer(value);
    
    digitalWrite(RFID_SS, HIGH);
}

// Read from MFRC522 register
byte read_register(byte reg) {
    digitalWrite(RFID_SS, LOW);
    
    // Send read command (bit 7 = 1 for read)
    spi_transfer(0x80 | ((reg & 0x3F) << 1));
    
    // Read response
    byte value = spi_transfer(0x00);
    
    digitalWrite(RFID_SS, HIGH);
    
    return value;
}

bool test_serial() {
    Serial.print("  Serial test: ");
    Serial.flush();
    delay(10);
    Serial.println("OK ✓");
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("TEST15: PROGRESSIVE MFRC522 INITIALIZATION");
    Serial.println("==========================================");
    Serial.println();
    Serial.println("Adding init steps one by one until serial fails");
    Serial.println();
    
    // Setup pins
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT);
    
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(RFID_SCK, LOW);
    digitalWrite(RFID_MOSI, LOW);
    
    // Check initial communication
    Serial.println("Step 0: Check version register");
    byte version = read_register(VersionReg);
    Serial.print("  Version: 0x");
    Serial.print(version, HEX);
    if(version == 0x91 || version == 0x92) {
        Serial.println(" ✓ MFRC522 detected");
    } else {
        Serial.println(" ✗ No MFRC522 detected - check wiring!");
        Serial.println("  Cannot proceed with initialization test");
        return;
    }
    test_serial();
    
    // Step 1: Soft Reset (Command Register)
    Serial.println("\nStep 1: Soft Reset");
    write_register(CommandReg, 0x0F);  // SoftReset command
    delay(50);  // Wait for reset to complete
    test_serial();
    
    // Step 2: Timer Configuration (4 registers)
    Serial.println("\nStep 2: Timer Configuration");
    Serial.println("  Writing TModeReg...");
    write_register(TModeReg, 0x80);  // TAuto=1, timer starts automatically
    test_serial();
    
    Serial.println("  Writing TPrescalerReg...");
    write_register(TPrescalerReg, 0xA9);  // TPreScaler = 169
    test_serial();
    
    Serial.println("  Writing TReloadRegH...");
    write_register(TReloadRegH, 0x03);  // Reload timer high byte
    test_serial();
    
    Serial.println("  Writing TReloadRegL...");
    write_register(TReloadRegL, 0xE8);  // Reload timer low byte
    test_serial();
    
    // Step 3: Force 100% ASK modulation
    Serial.println("\nStep 3: ASK Modulation");
    write_register(TxASKReg, 0x40);  // Force 100% ASK
    test_serial();
    
    // Step 4: Configure CRC and Mode
    Serial.println("\nStep 4: Mode Configuration");
    write_register(ModeReg, 0x3D);  // CRC preset value 0x6363
    test_serial();
    
    // Step 5: TX/RX Configuration
    Serial.println("\nStep 5: TX/RX Configuration");
    
    Serial.println("  Writing TxModeReg...");
    write_register(TxModeReg, 0x00);  // TX settings
    test_serial();
    
    Serial.println("  Writing RxModeReg...");
    write_register(RxModeReg, 0x00);  // RX settings
    test_serial();
    
    // Step 6: Antenna Configuration
    Serial.println("\nStep 6: Antenna Configuration");
    
    Serial.println("  Writing TxControlReg...");
    write_register(TxControlReg, 0x80);  // InvTx2RFOn=1, Tx2RFEn=1
    test_serial();
    
    // Step 7: Turn on antenna
    Serial.println("\nStep 7: Enable Antenna");
    byte txControl = read_register(TxControlReg);
    if((txControl & 0x03) != 0x03) {
        Serial.println("  Turning on antenna...");
        write_register(TxControlReg, txControl | 0x03);
    } else {
        Serial.println("  Antenna already on");
    }
    test_serial();
    
    // Step 8: Multiple rapid writes (stress test)
    Serial.println("\nStep 8: Rapid register writes (stress test)");
    Serial.println("  Writing 20 registers rapidly...");
    
    for(int i = 0; i < 20; i++) {
        write_register(TModeReg, 0x80);  // Same register, just testing
        
        if(i % 5 == 4) {
            Serial.print("    ");
            Serial.print(i + 1);
            Serial.print(" writes done");
            test_serial();
        }
    }
    
    // Final check
    Serial.println("\n==========================================");
    Serial.println("INITIALIZATION COMPLETE");
    Serial.println();
    
    // Verify MFRC522 still responds
    version = read_register(VersionReg);
    Serial.print("Final version check: 0x");
    Serial.print(version, HEX);
    if(version == 0x91 || version == 0x92) {
        Serial.println(" ✓ MFRC522 still responding");
    } else {
        Serial.println(" ✗ MFRC522 not responding");
    }
    
    Serial.println("\nResults:");
    Serial.println("- If serial survived all steps: Problem is NOT in init sequence");
    Serial.println("- If serial failed at specific step: That operation is the culprit");
    Serial.println("- Check which step caused failure for root cause");
}

void loop() {
    delay(5000);
    
    // Keep checking MFRC522 and serial
    byte v = read_register(VersionReg);
    Serial.print("Periodic check - Version: 0x");
    Serial.print(v, HEX);
    Serial.println(" - Serial OK");
}