// Test 17: SD/RFID Progressive Interaction Test
// Purpose: Isolate exactly when/why SD.open() hangs after RFID operations
// Tests: Critical sections, mutex state, interrupt state, progressive operations

#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>

// SD Card pins (Hardware VSPI)
#define SD_CS       5
#define SDSPI_SCK   18
#define SDSPI_MISO  19
#define SDSPI_MOSI  23

// RFID pins (Software SPI)
#define RFID_SS     3
#define RFID_SCK    22
#define RFID_MOSI   27
#define RFID_MISO   35

// Test configuration
#define TEST_FILE "/test.txt"
#define USE_CRITICAL_SECTIONS true  // Toggle to test impact

SPIClass SDSPI(VSPI);
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Test states
bool sdWorking = false;
int testNumber = 0;
unsigned long sdOpenTime = 0;

// Software SPI transfer (matching ALNScanner implementation)
byte softSPI_transfer(byte data, bool useCritical = USE_CRITICAL_SECTIONS) {
    byte result = 0;
    
    if (useCritical) {
        portENTER_CRITICAL(&spiMux);
    }
    
    for (int i = 0; i < 8; ++i) {
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        delayMicroseconds(2);
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MISO);
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    if (useCritical) {
        portEXIT_CRITICAL(&spiMux);
    }
    
    return result;
}

// RFID register read
byte readRFIDRegister(byte reg) {
    digitalWrite(RFID_SS, LOW);
    softSPI_transfer(0x80 | ((reg & 0x3F) << 1));
    byte value = softSPI_transfer(0x00);
    digitalWrite(RFID_SS, HIGH);
    return value;
}

// Test SD card access with timeout
bool testSDAccess(const char* context, unsigned long timeoutMs = 1000) {
    Serial.printf("\n[TEST %d] %s\n", ++testNumber, context);
    Serial.print("  Attempting SD.open()... ");
    
    unsigned long startTime = millis();
    
    // Try to open file with manual timeout check
    // Note: SD.open() might block, so we'll test this carefully
    File testFile = SD.open(TEST_FILE, FILE_WRITE);
    
    unsigned long elapsed = millis() - startTime;
    sdOpenTime = elapsed;
    
    if (testFile) {
        Serial.printf("SUCCESS (%lu ms)\n", elapsed);
        testFile.println(context);
        testFile.close();
        
        // Also test reading
        Serial.print("  Attempting SD.open() for read... ");
        startTime = millis();
        testFile = SD.open(TEST_FILE, FILE_READ);
        elapsed = millis() - startTime;
        
        if (testFile) {
            Serial.printf("SUCCESS (%lu ms)\n", elapsed);
            testFile.close();
            return true;
        } else {
            Serial.printf("FAILED (%lu ms)\n", elapsed);
            return false;
        }
    } else {
        Serial.printf("FAILED (%lu ms)\n", elapsed);
        if (elapsed > 900) {
            Serial.println("  WARNING: SD.open() took >900ms, might be hanging!");
        }
        return false;
    }
}

// Check system state
void checkSystemState() {
    Serial.println("\n=== SYSTEM STATE ===");
    
    // Check if we can still print (interrupts working)
    Serial.println("  Serial: Working");
    
    // Check mutex state (can we acquire it?)
    Serial.print("  Mutex test: ");
    portENTER_CRITICAL(&spiMux);
    Serial.print("Acquired... ");
    portEXIT_CRITICAL(&spiMux);
    Serial.println("Released OK");
    
    // Check interrupt state
    Serial.print("  Interrupts: ");
    if (xPortInIsrContext()) {
        Serial.println("In ISR context (BAD!)");
    } else {
        Serial.println("Normal context (OK)");
    }
    
    // Check free heap
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    
    Serial.println("====================\n");
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\nTEST17: SD/RFID INTERACTION ISOLATION");
    Serial.println("=====================================");
    Serial.printf("Critical sections: %s\n", USE_CRITICAL_SECTIONS ? "ENABLED" : "DISABLED");
    Serial.println();
    
    // Initialize RFID pins
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT_PULLUP);
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(RFID_SCK, LOW);
    
    // Initialize SD card
    Serial.println("Initializing SD card...");
    SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI);
    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("SD Card initialization FAILED!");
        Serial.println("Cannot proceed with test.");
        return;
    }
    Serial.println("SD Card initialized successfully.\n");
    
    // === PROGRESSIVE TESTS ===
    
    // Test 1: Baseline - SD works before any RFID operations
    sdWorking = testSDAccess("BASELINE - Before any RFID operations");
    if (!sdWorking) {
        Serial.println("CRITICAL: SD not working at baseline!");
        return;
    }
    
    // Test 2: After single SPI transfer
    Serial.println("\nPerforming single software SPI transfer...");
    digitalWrite(RFID_SS, LOW);
    byte dummy = softSPI_transfer(0xFF);
    digitalWrite(RFID_SS, HIGH);
    Serial.printf("  SPI result: 0x%02X\n", dummy);
    
    sdWorking = testSDAccess("After single SPI transfer");
    checkSystemState();
    if (!sdWorking) {
        Serial.println("FAILURE: Single SPI transfer broke SD!");
        return;
    }
    
    // Test 3: After multiple SPI transfers
    Serial.println("\nPerforming 100 software SPI transfers...");
    digitalWrite(RFID_SS, LOW);
    for (int i = 0; i < 100; i++) {
        softSPI_transfer(0xAA);
    }
    digitalWrite(RFID_SS, HIGH);
    
    sdWorking = testSDAccess("After 100 SPI transfers");
    if (!sdWorking) {
        Serial.println("FAILURE: Multiple SPI transfers broke SD!");
        checkSystemState();
        return;
    }
    
    // Test 4: After RFID register read
    Serial.println("\nReading MFRC522 version register...");
    byte version = readRFIDRegister(0x37);
    Serial.printf("  Version: 0x%02X\n", version);
    
    sdWorking = testSDAccess("After RFID register read");
    if (!sdWorking) {
        Serial.println("FAILURE: RFID register read broke SD!");
        checkSystemState();
        return;
    }
    
    // Test 5: After RFID soft reset
    Serial.println("\nSending MFRC522 soft reset...");
    digitalWrite(RFID_SS, LOW);
    softSPI_transfer(0x01 << 1);  // CommandReg
    softSPI_transfer(0x0F);        // SoftReset
    digitalWrite(RFID_SS, HIGH);
    delay(50);
    
    sdWorking = testSDAccess("After RFID soft reset");
    if (!sdWorking) {
        Serial.println("FAILURE: RFID soft reset broke SD!");
        checkSystemState();
        return;
    }
    
    // Test 6: After extensive RFID initialization (simulate ALNScanner)
    Serial.println("\nPerforming full RFID initialization sequence...");
    
    // Simulate the 20+ register writes from ALNScanner
    for (int i = 0; i < 20; i++) {
        digitalWrite(RFID_SS, LOW);
        softSPI_transfer((i & 0x3F) << 1);  // Write to various registers
        softSPI_transfer(0x00);              // Dummy data
        digitalWrite(RFID_SS, HIGH);
        delayMicroseconds(10);
        
        if (i % 5 == 4) {
            Serial.printf("  %d register writes done...\n", i + 1);
        }
    }
    
    sdWorking = testSDAccess("After full RFID initialization");
    if (!sdWorking) {
        Serial.println("FAILURE: Full RFID init broke SD!");
        checkSystemState();
        
        // Try to recover
        Serial.println("\n=== RECOVERY ATTEMPTS ===");
        
        // Attempt 1: Re-initialize SD
        Serial.println("Attempting SD re-initialization...");
        if (SD.begin(SD_CS, SDSPI)) {
            Serial.println("  SD.begin() succeeded");
            if (testSDAccess("After SD re-init")) {
                Serial.println("  RECOVERY SUCCESSFUL!");
            } else {
                Serial.println("  Recovery failed - SD.open() still broken");
            }
        } else {
            Serial.println("  SD.begin() failed");
        }
        
        // Attempt 2: Clear any stuck critical section
        Serial.println("\nClearing potential stuck critical section...");
        // Force exit critical section (might already be clear)
        portEXIT_CRITICAL(&spiMux);
        if (testSDAccess("After critical section clear")) {
            Serial.println("  RECOVERY via critical clear SUCCESSFUL!");
        }
        
        return;
    }
    
    // Test 7: Continuous operations
    Serial.println("\nTesting continuous RFID/SD interleaving...");
    for (int i = 0; i < 5; i++) {
        // Do RFID operation
        digitalWrite(RFID_SS, LOW);
        softSPI_transfer(0xFF);
        digitalWrite(RFID_SS, HIGH);
        
        // Test SD
        char context[50];
        sprintf(context, "Interleave iteration %d", i + 1);
        sdWorking = testSDAccess(context);
        
        if (!sdWorking) {
            Serial.printf("FAILURE at iteration %d\n", i + 1);
            checkSystemState();
            break;
        }
    }
    
    // Final summary
    Serial.println("\n========================================");
    Serial.println("TEST COMPLETE");
    Serial.println("========================================");
    
    if (sdWorking) {
        Serial.println("SUCCESS: SD card remained accessible throughout all tests!");
        Serial.printf("Average SD.open() time: %lu ms\n", sdOpenTime);
    } else {
        Serial.println("FAILURE: SD card became inaccessible");
        Serial.println("See test output above for failure point");
    }
    
    // Final state check
    checkSystemState();
}

void loop() {
    delay(5000);
    
    // Periodic SD test
    static int loopCount = 0;
    Serial.printf("\n[LOOP %d] Periodic SD test...\n", ++loopCount);
    
    if (testSDAccess("Periodic test")) {
        Serial.println("  SD still working");
    } else {
        Serial.println("  SD FAILED!");
        checkSystemState();
        
        // Try recovery
        Serial.println("  Attempting recovery...");
        SD.end();
        delay(100);
        if (SD.begin(SD_CS, SDSPI)) {
            Serial.println("  SD re-initialized");
        }
    }
}