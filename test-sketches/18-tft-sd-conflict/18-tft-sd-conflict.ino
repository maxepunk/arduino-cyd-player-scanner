// Test 18: TFT_eSPI + SD + RFID Component Conflict Test
// Purpose: Identify which component combination causes SD.open() to hang
// Key Test: TFT_eSPI might create conflicting SPIClass for VSPI

#include <TFT_eSPI.h>  // MUST be first - uses VSPI internally
#include <SD.h>
#include <SPI.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// SD Card pins (Hardware VSPI - SAME AS TFT!)
#define SD_CS       5
#define SDSPI_SCK   18
#define SDSPI_MISO  19
#define SDSPI_MOSI  23

// RFID pins (Software SPI)
#define RFID_SS     3
#define RFID_SCK    22
#define RFID_MOSI   27
#define RFID_MISO   35

// Global objects (matching ALNScanner)
TFT_eSPI tft = TFT_eSPI();  // Creates internal SPI for VSPI!
SPIClass SDSPI(VSPI);        // SECOND instance of VSPI - CONFLICT?
AudioOutputI2S *out = nullptr;
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Test tracking
int testPhase = 0;
bool sdWorking = false;
unsigned long hangDetectTime = 0;

// Software SPI (matching ALNScanner exactly)
byte softSPI_transfer(byte data) {
    byte result = 0;
    
    portENTER_CRITICAL(&spiMux);
    
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
    
    portEXIT_CRITICAL(&spiMux);
    
    return result;
}

// Test SD.open() for READ (matching drawBmp behavior)
bool testSDRead(const char* filename, const char* context) {
    Serial.printf("[PHASE %d] %s\n", testPhase, context);
    Serial.printf("  Opening '%s' for READ... ", filename);
    
    hangDetectTime = millis();
    
    // This is EXACTLY what drawBmp does - open for READ
    File f = SD.open(filename);  // <-- THIS IS WHERE IT HANGS
    
    unsigned long elapsed = millis() - hangDetectTime;
    
    if (f) {
        Serial.printf("EXISTS (%lu ms)\n", elapsed);
        
        // Read a few bytes like drawBmp would
        uint8_t header[54];
        size_t bytesRead = f.read(header, 54);
        Serial.printf("  Read %d bytes\n", bytesRead);
        f.close();
        return true;
    } else {
        Serial.printf("NOT FOUND (%lu ms)\n", elapsed);
        // This is normal when BMP doesn't exist
        return false;  // File doesn't exist but SD is working
    }
}

// Simulate RFID operations
void performRFIDOperations(int numOps = 20) {
    Serial.printf("  Performing %d RFID operations...\n", numOps);
    
    for (int i = 0; i < numOps; i++) {
        digitalWrite(RFID_SS, LOW);
        softSPI_transfer((i & 0x3F) << 1);
        softSPI_transfer(0x00);
        digitalWrite(RFID_SS, HIGH);
        delayMicroseconds(10);
    }
    Serial.println("  RFID operations complete");
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\nTEST18: TFT_eSPI + SD + RFID CONFLICT TEST");
    Serial.println("===========================================");
    Serial.println("Testing component interactions...\n");
    
    // ========== PHASE 1: TFT_eSPI Only ==========
    testPhase = 1;
    Serial.println("PHASE 1: Initialize TFT_eSPI");
    Serial.println("-----------------------------");
    
    tft.begin();  // Initializes VSPI internally!
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("TEST 18");
    Serial.println("  TFT initialized (using VSPI internally)");
    
    // ========== PHASE 2: Add SD Card ==========
    testPhase = 2;
    Serial.println("\nPHASE 2: Add SD Card (ALSO using VSPI)");
    Serial.println("---------------------------------------");
    
    // Initialize SD with our SDSPI instance (SECOND VSPI instance!)
    Serial.println("  Creating SDSPI(VSPI) - potential conflict!");
    SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI);
    
    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("  SD INIT FAILED! TFT_eSPI conflict?");
        tft.println("SD FAIL!");
        return;
    }
    
    Serial.println("  SD initialized successfully");
    tft.println("SD OK");
    
    // Test 1: Can we open files BEFORE any RFID?
    testSDRead("/test.txt", "SD baseline test");
    testSDRead("/IMG/04532ADB82684C81.bmp", "Non-existent BMP (normal case)");
    
    // ========== PHASE 3: Initialize RFID Pins ==========
    testPhase = 3;
    Serial.println("\nPHASE 3: Initialize RFID pins");
    Serial.println("------------------------------");
    
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT_PULLUP);
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(RFID_SCK, LOW);
    Serial.println("  RFID pins configured");
    
    // Test 2: SD still works after RFID pin init?
    testSDRead("/test.txt", "After RFID pin init");
    
    // ========== PHASE 4: RFID Operations ==========
    testPhase = 4;
    Serial.println("\nPHASE 4: Perform RFID operations");
    Serial.println("---------------------------------");
    
    performRFIDOperations(20);
    
    // Test 3: THE CRITICAL TEST - Does SD work after RFID?
    sdWorking = testSDRead("/test.txt", "After RFID operations");
    
    if (!sdWorking) {
        Serial.println("\n*** FAILURE: SD broken after RFID! ***");
        tft.println("SD BROKEN!");
    }
    
    // Try non-existent file (this is what triggers hang in ALNScanner)
    testSDRead("/IMG/NONEXISTENT.bmp", "Non-existent after RFID");
    
    // ========== PHASE 5: Add Audio ==========
    testPhase = 5;
    Serial.println("\nPHASE 5: Add AudioOutputI2S");
    Serial.println("----------------------------");
    
    out = new AudioOutputI2S(0, 1);
    Serial.println("  Audio output created");
    tft.println("Audio OK");
    
    // Test 4: SD after Audio init
    testSDRead("/test.txt", "After Audio init");
    
    // ========== PHASE 6: Full Sequence ==========
    testPhase = 6;
    Serial.println("\nPHASE 6: Full ALNScanner sequence");
    Serial.println("----------------------------------");
    
    // This matches ALNScanner scan sequence:
    // 1. Display shows "scanning"
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("Scanning...");
    
    // 2. RFID operations (card read)
    performRFIDOperations(30);
    
    // 3. Try to display BMP (THIS IS WHERE IT HANGS)
    Serial.println("\nCRITICAL TEST: drawBmp sequence");
    String testPath = "/IMG/04532ADB82684C81.bmp";
    
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    
    // Set 3-second timeout for hang detection
    Serial.printf("  Attempting SD.open('%s')...\n", testPath.c_str());
    unsigned long startTime = millis();
    
    File f = SD.open(testPath);  // <-- HANG POINT IN ALSCANNER
    
    unsigned long elapsed = millis() - startTime;
    
    if (elapsed > 1000) {
        Serial.printf("  WARNING: SD.open() took %lu ms!\n", elapsed);
        Serial.println("  LIKELY HANGING CONDITION DETECTED!");
        tft.println("SD HANG!");
    } else if (f) {
        Serial.printf("  File opened in %lu ms\n", elapsed);
        f.close();
        tft.println("File OK");
    } else {
        Serial.printf("  File not found in %lu ms (normal)\n", elapsed);
        tft.println("Missing:");
        tft.println(testPath);
    }
    
    // ========== FINAL ANALYSIS ==========
    Serial.println("\n========================================");
    Serial.println("TEST RESULTS SUMMARY");
    Serial.println("========================================");
    
    // Check if we can still access SD
    Serial.println("\nFinal SD check:");
    if (testSDRead("/test.txt", "Final test")) {
        Serial.println("SUCCESS: SD still accessible");
    } else {
        Serial.println("FAILURE: SD permanently broken");
    }
    
    Serial.println("\nPOSSIBLE ISSUES IDENTIFIED:");
    Serial.println("1. TFT_eSPI creates internal SPIClass for VSPI");
    Serial.println("2. ALNScanner creates SDSPI(VSPI) - DUPLICATE!");
    Serial.println("3. After RFID critical sections, SPI arbitration fails");
    Serial.println("4. SD.open() waits forever for SPI bus TFT owns");
    
    Serial.println("\nSOLUTION HYPOTHESIS:");
    Serial.println("- Use TFT's SPI instance for SD instead of creating new one");
    Serial.println("- OR: Ensure proper SPI transaction handling");
    Serial.println("- OR: Add timeout to SD operations");
}

void loop() {
    static int loopCount = 0;
    delay(5000);
    
    Serial.printf("\n[LOOP %d] Periodic test\n", ++loopCount);
    
    // Keep testing SD access
    if (testSDRead("/test.txt", "Loop test")) {
        tft.fillScreen(TFT_GREEN);
        delay(100);
        tft.fillScreen(TFT_BLACK);
    } else {
        tft.fillScreen(TFT_RED);
        Serial.println("  SD FAILED IN LOOP!");
    }
}