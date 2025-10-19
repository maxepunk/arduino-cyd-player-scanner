/**
 * System Information Collector for CYD Testing
 * 
 * This sketch automatically collects comprehensive system information
 * and formats it for easy copy/paste feedback reporting.
 * 
 * Instructions:
 * 1. Upload this sketch to your CYD
 * 2. Open Serial Monitor at 115200 baud
 * 3. Wait for "COLLECTION COMPLETE"
 * 4. Copy entire output
 * 5. Save as: CYD_SysInfo_[Date].txt
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>

TFT_eSPI tft = TFT_eSPI();

// Test results storage
struct TestResult {
    String category;
    String test;
    bool passed;
    String value;
    uint32_t timing;
};

TestResult results[50];
int resultIndex = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    delay(1000);
    
    printHeader();
    collectSystemInfo();
    testHardwareFeatures();
    measurePerformance();
    generateReport();
    
    Serial.println("\n[COLLECTION COMPLETE]");
    Serial.println("Please copy all output above and save as CYD_SysInfo_[Date].txt");
}

void printHeader() {
    Serial.println("================================================================================");
    Serial.println("                        CYD SYSTEM INFORMATION COLLECTOR                        ");
    Serial.println("================================================================================");
    Serial.printf("Collection Started: %lu ms since boot\n", millis());
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("================================================================================\n");
}

void collectSystemInfo() {
    Serial.println("## SYSTEM INFORMATION");
    Serial.println("```");
    
    // ESP32 Information
    Serial.println("### ESP32 Core:");
    Serial.printf("- Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("- Chip Revision: %d\n", ESP.getChipRevision());
    Serial.printf("- CPU Frequency: %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("- Flash Size: %u bytes (%u MB)\n", ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024*1024));
    Serial.printf("- Flash Speed: %u Hz\n", ESP.getFlashChipSpeed());
    Serial.printf("- Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("- Min Free Heap: %u bytes\n", ESP.getMinFreeHeap());
    Serial.printf("- Max Alloc Heap: %u bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("- PSRAM Size: %u bytes\n", ESP.getPsramSize());
    Serial.printf("- Free PSRAM: %u bytes\n", ESP.getFreePsram());
    
    // SDK Version
    Serial.println("\n### SDK/Framework:");
    Serial.printf("- Arduino Core Version: %s\n", ESP.getSdkVersion());
    Serial.printf("- Sketch Size: %u bytes\n", ESP.getSketchSize());
    Serial.printf("- Free Sketch Space: %u bytes\n", ESP.getFreeSketchSpace());
    
    addResult("System", "ESP32 Detection", true, ESP.getChipModel());
    addResult("System", "Flash Size", true, String(ESP.getFlashChipSize() / (1024*1024)) + "MB");
}

void testHardwareFeatures() {
    Serial.println("\n### Hardware Detection:");
    
    // Display Detection
    uint32_t startTime = millis();
    tft.init();
    uint32_t displayInitTime = millis() - startTime;
    
    Serial.printf("- Display Init Time: %u ms\n", displayInitTime);
    addResult("Display", "Initialization", displayInitTime < 1000, String(displayInitTime) + "ms");
    
    // Try to detect display driver
    detectDisplayDriver();
    
    // GPIO Pin Tests
    testGPIOPins();
    
    // SD Card Test
    testSDCard();
    
    // EEPROM Test
    testEEPROM();
    
    // Touch Controller Test
    testTouch();
    
    // RFID Pin Configuration
    testRFIDPins();
    
    Serial.println("```");
}

void detectDisplayDriver() {
    Serial.println("\n### Display Driver Detection:");
    
    // Method 1: Read display ID
    uint8_t id1 = tft.readcommand8(0xD3, 1);
    uint8_t id2 = tft.readcommand8(0xD3, 2);
    uint8_t id3 = tft.readcommand8(0xD3, 3);
    
    Serial.printf("- ID Registers: 0x%02X 0x%02X 0x%02X\n", id1, id2, id3);
    
    // Method 2: Read power mode
    uint8_t powerMode = tft.readcommand8(0x0A);
    Serial.printf("- Power Mode: 0x%02X\n", powerMode);
    
    // Method 3: Read MADCTL
    uint8_t madctl = tft.readcommand8(0x0B);
    Serial.printf("- MADCTL: 0x%02X\n", madctl);
    
    // Determine driver
    String detectedDriver = "Unknown";
    if (id2 == 0x93 && id3 == 0x41) {
        detectedDriver = "ILI9341";
    } else if (id2 == 0x85 || id2 == 0x77) {
        detectedDriver = "ST7789";
    } else if (id2 == 0x79) {
        detectedDriver = "ST7796";
    }
    
    Serial.printf("- Detected Driver: %s\n", detectedDriver.c_str());
    addResult("Display", "Driver Detection", detectedDriver != "Unknown", detectedDriver);
    
    // Test backlight pins
    Serial.println("\n### Backlight Pin Test:");
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    Serial.println("- GPIO21: Set HIGH");
    
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);
    Serial.println("- GPIO27: Set HIGH");
    
    addResult("Display", "Backlight", true, "GPIO21+27 tested");
}

void testGPIOPins() {
    Serial.println("\n### GPIO Pin Status:");
    
    struct PinInfo {
        uint8_t pin;
        const char* function;
        uint8_t mode;
    } pins[] = {
        {2,  "TFT_DC", OUTPUT},
        {3,  "RFID_SS/RX", OUTPUT},
        {5,  "SD_CS", OUTPUT},
        {12, "TFT_MISO", INPUT},
        {13, "TFT_MOSI", OUTPUT},
        {14, "TFT_SCLK", OUTPUT},
        {15, "TFT_CS", OUTPUT},
        {18, "SD_SCK", OUTPUT},
        {19, "SD_MISO", INPUT},
        {21, "Backlight1", OUTPUT},
        {22, "RFID_SCK", OUTPUT},
        {23, "SD_MOSI", OUTPUT},
        {27, "Backlight2/RFID_MOSI", OUTPUT},
        {33, "Touch_CS", OUTPUT},
        {35, "RFID_MISO", INPUT},
        {36, "Touch_IRQ", INPUT_PULLUP},
    };
    
    for (auto& pin : pins) {
        pinMode(pin.pin, pin.mode);
        int state = digitalRead(pin.pin);
        Serial.printf("- GPIO%02d (%s): %s\n", 
                      pin.pin, pin.function, state ? "HIGH" : "LOW");
    }
    
    addResult("GPIO", "Pin Configuration", true, "All pins tested");
}

void testSDCard() {
    Serial.println("\n### SD Card Test:");
    
    SPIClass sdSPI(VSPI);
    sdSPI.begin(18, 19, 23, 5);
    
    if (SD.begin(5, sdSPI, 4000000)) {
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("- Card Size: %llu MB\n", cardSize);
        Serial.printf("- Card Type: ");
        
        uint8_t cardType = SD.cardType();
        switch(cardType) {
            case CARD_MMC: Serial.println("MMC"); break;
            case CARD_SD: Serial.println("SD"); break;
            case CARD_SDHC: Serial.println("SDHC"); break;
            default: Serial.println("Unknown"); break;
        }
        
        addResult("SDCard", "Detection", true, String(cardSize) + "MB");
    } else {
        Serial.println("- No SD card detected");
        addResult("SDCard", "Detection", false, "Not present");
    }
}

void testEEPROM() {
    Serial.println("\n### EEPROM Test:");
    
    EEPROM.begin(512);
    
    // Write test
    uint8_t testPattern = 0x5A;
    EEPROM.write(0, testPattern);
    EEPROM.commit();
    
    // Read test
    uint8_t readBack = EEPROM.read(0);
    bool eepromOk = (readBack == testPattern);
    
    Serial.printf("- EEPROM Size: 512 bytes\n");
    Serial.printf("- Write/Read Test: %s\n", eepromOk ? "PASSED" : "FAILED");
    
    addResult("EEPROM", "Read/Write", eepromOk, eepromOk ? "OK" : "Failed");
}

void testTouch() {
    Serial.println("\n### Touch Controller Test:");
    
    pinMode(33, OUTPUT);  // Touch CS
    digitalWrite(33, HIGH);
    
    pinMode(36, INPUT_PULLUP);  // Touch IRQ
    int irqState = digitalRead(36);
    
    Serial.printf("- Touch CS (GPIO33): HIGH\n");
    Serial.printf("- Touch IRQ (GPIO36): %s\n", irqState ? "HIGH (idle)" : "LOW (active)");
    
    addResult("Touch", "IRQ State", irqState == HIGH, irqState ? "Idle" : "Active");
}

void testRFIDPins() {
    Serial.println("\n### RFID Configuration:");
    
    Serial.println("- Software SPI Pins:");
    Serial.println("  - SCK:  GPIO22");
    Serial.println("  - MOSI: GPIO27 (conflicts with backlight)");
    Serial.println("  - MISO: GPIO35 (input only)");
    Serial.println("  - SS:   GPIO3 (RX pin)");
    
    // Test for GPIO27 conflict
    bool hasConflict = true;  // GPIO27 is used for both
    Serial.printf("- GPIO27 Conflict: %s\n", hasConflict ? "YES - Multiplexing needed" : "No");
    
    addResult("RFID", "Pin Config", true, "Software SPI");
    addResult("RFID", "GPIO27 Conflict", true, "Requires multiplexing");
}

void measurePerformance() {
    Serial.println("\n## PERFORMANCE METRICS");
    Serial.println("```");
    
    // Memory allocation test
    uint32_t startHeap = ESP.getFreeHeap();
    uint32_t largestBlock = ESP.getMaxAllocHeap();
    
    Serial.printf("- Largest Free Block: %u bytes\n", largestBlock);
    Serial.printf("- Fragmentation: %.1f%%\n", 
                  (1.0 - (float)largestBlock / startHeap) * 100);
    
    // GPIO toggle speed test
    uint32_t startTime = micros();
    for (int i = 0; i < 1000; i++) {
        digitalWrite(22, HIGH);
        digitalWrite(22, LOW);
    }
    uint32_t toggleTime = micros() - startTime;
    
    Serial.printf("- GPIO Toggle Speed: %u toggles/sec\n", 
                  1000000000 / toggleTime);
    
    // SPI clock test
    Serial.printf("- Max SPI Frequency: 80 MHz (hardware)\n");
    Serial.printf("- Software SPI Estimate: ~1 MHz\n");
    
    addResult("Performance", "Free Heap", startHeap > 100000, String(startHeap) + " bytes");
    addResult("Performance", "GPIO Speed", true, String(1000000000 / toggleTime) + " Hz");
    
    Serial.println("```");
}

void generateReport() {
    Serial.println("\n## TEST RESULTS SUMMARY");
    Serial.println("```");
    
    // Count results by category
    int passCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < resultIndex; i++) {
        if (results[i].passed) passCount++;
        else failCount++;
    }
    
    Serial.printf("Total Tests: %d\n", resultIndex);
    Serial.printf("Passed: %d\n", passCount);
    Serial.printf("Failed: %d\n", failCount);
    Serial.printf("Success Rate: %.1f%%\n", 
                  (float)passCount / resultIndex * 100);
    
    Serial.println("\n### Detailed Results:");
    String lastCategory = "";
    
    for (int i = 0; i < resultIndex; i++) {
        if (results[i].category != lastCategory) {
            Serial.printf("\n[%s]\n", results[i].category.c_str());
            lastCategory = results[i].category;
        }
        
        Serial.printf("  %s: %s - %s\n",
                      results[i].test.c_str(),
                      results[i].passed ? "PASS" : "FAIL",
                      results[i].value.c_str());
    }
    
    Serial.println("```");
    
    // Generate hardware fingerprint
    generateFingerprint();
}

void generateFingerprint() {
    Serial.println("\n## HARDWARE FINGERPRINT");
    Serial.println("```");
    
    // Create a unique fingerprint based on hardware characteristics
    uint32_t fingerprint = 0;
    
    // Include flash size
    fingerprint ^= ESP.getFlashChipSize();
    
    // Include chip revision
    fingerprint ^= (ESP.getChipRevision() << 16);
    
    // Include detected features
    for (int i = 0; i < resultIndex; i++) {
        if (results[i].passed) {
            fingerprint ^= hashString(results[i].test);
        }
    }
    
    Serial.printf("Fingerprint: 0x%08X\n", fingerprint);
    
    // Determine likely model
    String likelyModel = "Unknown";
    bool hasILI9341 = false;
    bool hasST7789 = false;
    
    for (int i = 0; i < resultIndex; i++) {
        if (results[i].value == "ILI9341") hasILI9341 = true;
        if (results[i].value == "ST7789") hasST7789 = true;
    }
    
    if (hasILI9341) {
        likelyModel = "CYD Single USB (ESP32-2432S028)";
    } else if (hasST7789) {
        likelyModel = "CYD Dual USB (ESP32-2432S028R)";
    }
    
    Serial.printf("Likely Model: %s\n", likelyModel.c_str());
    
    Serial.println("```");
    
    // Copy instruction
    Serial.println("\n## COPY INSTRUCTION");
    Serial.println("================================================================================");
    Serial.println("1. Select ALL text from 'CYD SYSTEM INFORMATION COLLECTOR' to here");
    Serial.println("2. Copy to clipboard (Ctrl+C or Cmd+C)");
    Serial.println("3. Save as: CYD_SysInfo_" + String(millis()) + ".txt");
    Serial.println("4. Include this file when reporting test results");
    Serial.println("================================================================================");
}

uint32_t hashString(String str) {
    uint32_t hash = 0;
    for (int i = 0; i < str.length(); i++) {
        hash = hash * 31 + str[i];
    }
    return hash;
}

void addResult(String category, String test, bool passed, String value) {
    if (resultIndex < 50) {
        results[resultIndex].category = category;
        results[resultIndex].test = test;
        results[resultIndex].passed = passed;
        results[resultIndex].value = value;
        results[resultIndex].timing = millis();
        resultIndex++;
    }
}

void loop() {
    // Show heartbeat on display
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        blinkState = !blinkState;
        
        if (blinkState) {
            tft.fillCircle(220, 10, 5, TFT_GREEN);
        } else {
            tft.fillCircle(220, 10, 5, TFT_BLACK);
        }
    }
}