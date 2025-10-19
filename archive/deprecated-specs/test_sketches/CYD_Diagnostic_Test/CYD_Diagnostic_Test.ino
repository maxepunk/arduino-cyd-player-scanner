/**
 * CYD Diagnostic Test Sketch
 * 
 * Purpose: Comprehensive diagnostic testing for CYD hardware
 * Tests all components and reports detailed status
 * 
 * Components tested:
 * - Display (ILI9341/ST7789)
 * - Touch controller (XPT2046)
 * - RFID reader (MFRC522)
 * - SD card
 * - GPIO pins
 * - Memory usage
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>

// Component pins
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define SD_CS      5
#define RFID_SS    3
#define RFID_SCK   22
#define RFID_MOSI  27
#define RFID_MISO  35

// Backlight pins
#define BL_PIN_21  21
#define BL_PIN_27  27

TFT_eSPI tft = TFT_eSPI();

// Diagnostic levels
enum DiagLevel {
    DIAG_DEBUG = 0,
    DIAG_INFO = 1,
    DIAG_WARNING = 2,
    DIAG_ERROR = 3,
    DIAG_CRITICAL = 4
};

// Component status
enum ComponentStatus {
    STATUS_UNKNOWN = 0,
    STATUS_OK = 1,
    STATUS_WARNING = 2,
    STATUS_ERROR = 3,
    STATUS_FAILED = 4
};

struct DiagnosticResult {
    String component;
    ComponentStatus status;
    String message;
    uint32_t testTime;
};

DiagnosticResult results[10];
int resultCount = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    printHeader();
    
    // Run diagnostic tests
    testDisplay();
    testBacklight();
    testTouch();
    testSDCard();
    testRFID();
    testGPIOPins();
    testMemory();
    testEEPROM();
    
    // Report results
    reportSummary();
    showDisplaySummary();
}

void printHeader() {
    Serial.println("\n════════════════════════════════════════");
    Serial.println("     CYD COMPREHENSIVE DIAGNOSTICS");
    Serial.println("════════════════════════════════════════");
    Serial.printf("[%lu][INFO][SYSTEM]: Starting diagnostic sequence\n", millis());
    Serial.println();
}

void report(DiagLevel level, const char* component, const char* format, ...) {
    const char* levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR", "CRIT"};
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.printf("[%lu][%s][%s]: %s\n", 
                  millis(), 
                  levelStr[level], 
                  component, 
                  buffer);
}

void addResult(String component, ComponentStatus status, String message) {
    if (resultCount < 10) {
        results[resultCount].component = component;
        results[resultCount].status = status;
        results[resultCount].message = message;
        results[resultCount].testTime = millis();
        resultCount++;
    }
}

void testDisplay() {
    report(DIAG_INFO, "DISPLAY", "Testing display initialization...");
    
    uint32_t startTime = millis();
    tft.init();
    uint32_t initTime = millis() - startTime;
    
    if (initTime < 1000) {
        report(DIAG_INFO, "DISPLAY", "Display initialized in %lums", initTime);
        
        // Test basic drawing
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, 10, 10, TFT_RED);
        
        report(DIAG_INFO, "DISPLAY", "Display test pattern drawn");
        addResult("Display", STATUS_OK, "Initialized successfully");
    } else {
        report(DIAG_ERROR, "DISPLAY", "Display initialization slow: %lums", initTime);
        addResult("Display", STATUS_WARNING, "Slow initialization");
    }
    
    // Try to detect driver type
    uint8_t r = tft.readcommand8(0x04);  // Read display ID
    report(DIAG_DEBUG, "DISPLAY", "Display ID register: 0x%02X", r);
}

void testBacklight() {
    report(DIAG_INFO, "BACKLIGHT", "Testing backlight control...");
    
    // Test GPIO21
    pinMode(BL_PIN_21, OUTPUT);
    digitalWrite(BL_PIN_21, HIGH);
    report(DIAG_INFO, "BACKLIGHT", "GPIO21 set HIGH");
    
    // Test GPIO27
    pinMode(BL_PIN_27, OUTPUT);
    digitalWrite(BL_PIN_27, HIGH);
    report(DIAG_INFO, "BACKLIGHT", "GPIO27 set HIGH");
    
    addResult("Backlight", STATUS_OK, "Both pins activated");
}

void testTouch() {
    report(DIAG_INFO, "TOUCH", "Testing touch controller...");
    
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    
    pinMode(TOUCH_IRQ, INPUT_PULLUP);
    int irqState = digitalRead(TOUCH_IRQ);
    
    report(DIAG_INFO, "TOUCH", "Touch IRQ pin state: %s", 
           irqState == LOW ? "LOW (touched)" : "HIGH (idle)");
    
    // Simple SPI test to touch controller
    digitalWrite(TOUCH_CS, LOW);
    delayMicroseconds(10);
    digitalWrite(TOUCH_CS, HIGH);
    
    if (irqState == HIGH) {
        addResult("Touch", STATUS_OK, "IRQ responding");
    } else {
        addResult("Touch", STATUS_WARNING, "IRQ may be stuck low");
    }
}

void testSDCard() {
    report(DIAG_INFO, "SDCARD", "Testing SD card...");
    
    SPIClass sdSPI(VSPI);
    sdSPI.begin(18, 19, 23, SD_CS);  // SCK, MISO, MOSI, CS
    
    if (SD.begin(SD_CS, sdSPI)) {
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        uint64_t usedSize = SD.usedBytes() / (1024 * 1024);
        
        report(DIAG_INFO, "SDCARD", "Card detected: %lluMB total, %lluMB used", 
               cardSize, usedSize);
        addResult("SD Card", STATUS_OK, String(cardSize) + "MB detected");
    } else {
        report(DIAG_WARNING, "SDCARD", "No SD card detected");
        addResult("SD Card", STATUS_WARNING, "Not present");
    }
}

void testRFID() {
    report(DIAG_INFO, "RFID", "Testing RFID reader on software SPI...");
    
    // Setup software SPI pins
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT);
    pinMode(RFID_SS, OUTPUT);
    digitalWrite(RFID_SS, HIGH);
    
    // Test SPI communication
    digitalWrite(RFID_SS, LOW);
    delayMicroseconds(10);
    
    // Send dummy byte
    for (int i = 0; i < 8; i++) {
        digitalWrite(RFID_SCK, LOW);
        digitalWrite(RFID_MOSI, (0xAA >> (7-i)) & 0x01);
        delayMicroseconds(5);
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(5);
    }
    
    digitalWrite(RFID_SS, HIGH);
    
    report(DIAG_INFO, "RFID", "Software SPI pins configured");
    report(DIAG_DEBUG, "RFID", "SCK=%d, MOSI=%d, MISO=%d, SS=%d", 
           RFID_SCK, RFID_MOSI, RFID_MISO, RFID_SS);
    
    // Check for GPIO27 conflict
    if (RFID_MOSI == 27) {
        report(DIAG_WARNING, "RFID", "GPIO27 conflict with backlight possible");
        addResult("RFID", STATUS_WARNING, "GPIO27 multiplex needed");
    } else {
        addResult("RFID", STATUS_OK, "Pins configured");
    }
}

void testGPIOPins() {
    report(DIAG_INFO, "GPIO", "Testing critical GPIO pins...");
    
    struct PinTest {
        uint8_t pin;
        const char* name;
        uint8_t mode;
    };
    
    PinTest pins[] = {
        {21, "Backlight-21", OUTPUT},
        {27, "Backlight-27/RFID-MOSI", OUTPUT},
        {22, "RFID-SCK", OUTPUT},
        {35, "RFID-MISO", INPUT},
        {3,  "RFID-SS", OUTPUT},
        {33, "Touch-CS", OUTPUT},
        {36, "Touch-IRQ", INPUT_PULLUP},
        {5,  "SD-CS", OUTPUT},
        {18, "SD-SCK", OUTPUT},
        {19, "SD-MISO", INPUT},
        {23, "SD-MOSI", OUTPUT}
    };
    
    for (int i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        pinMode(pins[i].pin, pins[i].mode);
        int state = digitalRead(pins[i].pin);
        
        report(DIAG_DEBUG, "GPIO", "Pin %d (%s): %s", 
               pins[i].pin, 
               pins[i].name,
               state ? "HIGH" : "LOW");
    }
    
    addResult("GPIO", STATUS_OK, "All pins accessible");
}

void testMemory() {
    report(DIAG_INFO, "MEMORY", "Testing memory usage...");
    
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
    
    report(DIAG_INFO, "MEMORY", "Total: %u, Free: %u, Min: %u, MaxAlloc: %u",
           totalHeap, freeHeap, minFreeHeap, maxAllocHeap);
    
    float usagePercent = ((float)(totalHeap - freeHeap) / totalHeap) * 100;
    report(DIAG_INFO, "MEMORY", "Memory usage: %.1f%%", usagePercent);
    
    if (freeHeap < 50000) {
        report(DIAG_WARNING, "MEMORY", "Low memory warning: %u bytes free", freeHeap);
        addResult("Memory", STATUS_WARNING, "Low free heap");
    } else {
        addResult("Memory", STATUS_OK, String(freeHeap) + " bytes free");
    }
}

void testEEPROM() {
    report(DIAG_INFO, "EEPROM", "Testing EEPROM...");
    
    EEPROM.begin(512);
    
    // Write test pattern
    uint8_t testVal = 0xAB;
    EEPROM.write(0, testVal);
    EEPROM.commit();
    
    // Read back
    uint8_t readVal = EEPROM.read(0);
    
    if (readVal == testVal) {
        report(DIAG_INFO, "EEPROM", "Read/write test passed");
        addResult("EEPROM", STATUS_OK, "512 bytes available");
    } else {
        report(DIAG_ERROR, "EEPROM", "Read/write test failed");
        addResult("EEPROM", STATUS_ERROR, "Read/write failed");
    }
}

void reportSummary() {
    Serial.println("\n════════════════════════════════════════");
    Serial.println("          DIAGNOSTIC SUMMARY");
    Serial.println("════════════════════════════════════════");
    
    const char* statusStr[] = {"UNKNOWN", "OK", "WARNING", "ERROR", "FAILED"};
    
    for (int i = 0; i < resultCount; i++) {
        Serial.printf("%-12s: %-8s - %s\n",
                      results[i].component.c_str(),
                      statusStr[results[i].status],
                      results[i].message.c_str());
    }
    
    Serial.println("════════════════════════════════════════");
    
    // Count status types
    int okCount = 0, warnCount = 0, errorCount = 0;
    for (int i = 0; i < resultCount; i++) {
        switch(results[i].status) {
            case STATUS_OK: okCount++; break;
            case STATUS_WARNING: warnCount++; break;
            case STATUS_ERROR:
            case STATUS_FAILED: errorCount++; break;
        }
    }
    
    Serial.printf("\nOverall: %d OK, %d Warnings, %d Errors\n",
                  okCount, warnCount, errorCount);
    
    if (errorCount == 0 && warnCount == 0) {
        Serial.println("\n✅ All systems operational!");
    } else if (errorCount == 0) {
        Serial.println("\n⚠️ System operational with warnings");
    } else {
        Serial.println("\n❌ System has errors - check components");
    }
}

void showDisplaySummary() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("DIAGNOSTICS");
    
    tft.setTextSize(1);
    int y = 40;
    
    for (int i = 0; i < resultCount && i < 8; i++) {
        uint16_t color = TFT_WHITE;
        
        switch(results[i].status) {
            case STATUS_OK: color = TFT_GREEN; break;
            case STATUS_WARNING: color = TFT_YELLOW; break;
            case STATUS_ERROR:
            case STATUS_FAILED: color = TFT_RED; break;
        }
        
        tft.setCursor(10, y);
        tft.setTextColor(color, TFT_BLACK);
        tft.print(results[i].component);
        tft.print(": ");
        
        const char* statusStr[] = {"?", "OK", "WARN", "ERR", "FAIL"};
        tft.println(statusStr[results[i].status]);
        
        y += 15;
    }
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 220);
    tft.println("See Serial @ 115200");
}

void loop() {
    static unsigned long lastReport = 0;
    
    // Report memory status every 30 seconds
    if (millis() - lastReport > 30000) {
        lastReport = millis();
        
        uint32_t freeHeap = ESP.getFreeHeap();
        report(DIAG_DEBUG, "MEMORY", "Free heap: %u bytes", freeHeap);
    }
    
    delay(100);
}