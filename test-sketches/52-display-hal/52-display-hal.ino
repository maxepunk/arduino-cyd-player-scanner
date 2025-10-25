/**
 * @file 52-display-hal.ino
 * @brief Test sketch for DisplayDriver.h HAL component
 *
 * Tests:
 * 1. TFT initialization
 * 2. Screen clearing and basic drawing
 * 3. BMP image rendering (Constitution-compliant SPI pattern)
 * 4. Multiple image loads (stress test for SPI deadlock)
 * 5. Error handling (missing files, bad formats)
 * 6. Direct TFT access for text rendering
 *
 * Hardware Requirements:
 * - CYD ESP32-2432S028R
 * - SD card with test images
 *
 * Expected Results:
 * - Display initializes without errors
 * - Text renders correctly
 * - BMP images display with proper colors
 * - No SPI deadlocks or freezes
 * - Free heap remains stable
 */

// Include v5.0 HAL components
#define DEBUG_MODE 1  // Enable verbose logging

#include "../../ALNScanner_v5/config.h"
#include "../../ALNScanner_v5/hal/SDCard.h"
#include "../../ALNScanner_v5/hal/DisplayDriver.h"

// Test state
struct TestStats {
    uint32_t testsPassed = 0;
    uint32_t testsFailed = 0;
    uint32_t startHeap = 0;
    uint32_t currentHeap = 0;
} stats;

// Helper function to print separator line
void printSeparator() {
    Serial.println("============================================================");
}

// Test helper macros
#define TEST_START(name) \
    Serial.println(""); \
    printSeparator(); \
    Serial.printf("TEST: %s\n", name); \
    printSeparator(); \
    stats.currentHeap = ESP.getFreeHeap();

#define TEST_PASS() \
    stats.testsPassed++; \
    Serial.println("✓ PASS"); \
    reportHeap();

#define TEST_FAIL(reason) \
    stats.testsFailed++; \
    Serial.printf("✗ FAIL: %s\n", reason); \
    reportHeap();

void reportHeap() {
    uint32_t heap = ESP.getFreeHeap();
    int32_t delta = (int32_t)heap - (int32_t)stats.currentHeap;
    Serial.printf("Heap: %d bytes (delta: %+d bytes)\n", heap, delta);
}

void reportStats() {
    Serial.println("");
    printSeparator();
    Serial.println("TEST RESULTS");
    printSeparator();
    Serial.printf("Passed: %d\n", stats.testsPassed);
    Serial.printf("Failed: %d\n", stats.testsFailed);
    Serial.printf("Total:  %d\n", stats.testsPassed + stats.testsFailed);
    Serial.printf("Success Rate: %.1f%%\n",
                  100.0 * stats.testsPassed / (stats.testsPassed + stats.testsFailed));
    Serial.printf("Final Heap: %d bytes (delta: %+d bytes from start)\n",
                  stats.currentHeap, (int32_t)stats.currentHeap - (int32_t)stats.startHeap);
    printSeparator();
}

// ────────────────────────────────────────────────────────────────────────────
// TEST 1: HAL Initialization
// ────────────────────────────────────────────────────────────────────────────
void test_HalInitialization() {
    TEST_START("HAL Initialization");

    // Initialize SD card first (required for DisplayDriver)
    auto& sd = hal::SDCard::getInstance();
    if (!sd.begin()) {
        TEST_FAIL("SD card initialization failed");
        return;
    }
    Serial.println("✓ SD card initialized");

    // Initialize display
    auto& display = hal::DisplayDriver::getInstance();
    if (!display.begin()) {
        TEST_FAIL("Display initialization failed");
        return;
    }
    Serial.println("✓ Display initialized");

    // Verify initialized state
    if (!display.isInitialized()) {
        TEST_FAIL("Display reports not initialized after begin()");
        return;
    }
    Serial.println("✓ Display state verified");

    TEST_PASS();
}

// ────────────────────────────────────────────────────────────────────────────
// TEST 2: Basic Display Operations
// ────────────────────────────────────────────────────────────────────────────
void test_BasicDisplayOperations() {
    TEST_START("Basic Display Operations");

    auto& display = hal::DisplayDriver::getInstance();

    // Test 2.1: Clear screen
    Serial.println("[2.1] Clearing screen to black...");
    display.clear();
    delay(500);
    Serial.println("✓ Clear complete");

    // Test 2.2: Fill with color
    Serial.println("[2.2] Filling screen with red...");
    display.fillScreen(TFT_RED);
    delay(500);
    Serial.println("✓ Red fill complete");

    // Test 2.3: Fill with blue
    Serial.println("[2.3] Filling screen with blue...");
    display.fillScreen(TFT_BLUE);
    delay(500);
    Serial.println("✓ Blue fill complete");

    // Test 2.4: Fill with green
    Serial.println("[2.4] Filling screen with green...");
    display.fillScreen(TFT_GREEN);
    delay(500);
    Serial.println("✓ Green fill complete");

    TEST_PASS();
}

// ────────────────────────────────────────────────────────────────────────────
// TEST 3: Direct TFT Access
// ────────────────────────────────────────────────────────────────────────────
void test_DirectTFTAccess() {
    TEST_START("Direct TFT Access");

    auto& display = hal::DisplayDriver::getInstance();
    auto& tft = display.getTFT();

    // Clear screen
    display.fillScreen(TFT_BLACK);

    // Draw text
    Serial.println("Drawing text...");
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("DisplayDriver HAL");
    tft.println("Test Sketch 52");
    tft.println("");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.println("Testing direct TFT access...");
    tft.println("");
    tft.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    delay(2000);
    Serial.println("✓ Text rendered");

    TEST_PASS();
}

// ────────────────────────────────────────────────────────────────────────────
// TEST 4: BMP Image Rendering (Constitution-Compliant)
// ────────────────────────────────────────────────────────────────────────────
void test_BMPRendering() {
    TEST_START("BMP Image Rendering");

    auto& display = hal::DisplayDriver::getInstance();

    // Test with an existing BMP from SD card
    // Note: Adjust path to match your SD card contents
    String testImages[] = {
        "/images/kaa001.bmp",
        "/images/jaw001.bmp",
        "/test.bmp"  // Add a test image if available
    };

    bool foundImage = false;

    for (int i = 0; i < 3; i++) {
        String path = testImages[i];
        Serial.printf("[4.%d] Attempting to load: %s\n", i + 1, path.c_str());

        // Check if file exists first
        {
            hal::SDCard::Lock lock("testBMP");
            if (!lock.acquired()) {
                Serial.println("✗ Could not acquire SD mutex");
                continue;
            }

            if (!SD.exists(path.c_str())) {
                Serial.printf("✗ File not found: %s\n", path.c_str());
                continue;
            }
        }

        // Attempt to render
        unsigned long startMs = millis();
        bool success = display.drawBMP(path);
        unsigned long latencyMs = millis() - startMs;

        if (success) {
            Serial.printf("✓ Image rendered successfully in %lu ms\n", latencyMs);
            foundImage = true;
            delay(2000);  // Show image for 2 seconds
            break;
        } else {
            Serial.printf("✗ Failed to render: %s\n", path.c_str());
        }
    }

    if (!foundImage) {
        TEST_FAIL("No valid BMP images found on SD card");
        return;
    }

    TEST_PASS();
}

// ────────────────────────────────────────────────────────────────────────────
// TEST 5: SPI Deadlock Stress Test
// ────────────────────────────────────────────────────────────────────────────
void test_SPIDeadlockStressTest() {
    TEST_START("SPI Deadlock Stress Test");

    auto& display = hal::DisplayDriver::getInstance();

    Serial.println("Rapidly switching between SD access and TFT operations...");
    Serial.println("This would cause deadlock if SPI pattern is wrong!");

    for (int i = 0; i < 5; i++) {
        Serial.printf("[5.%d] Iteration %d/5\n", i + 1, i + 1);

        // SD read operation
        {
            hal::SDCard::Lock lock("stressTest");
            if (lock.acquired()) {
                File f = SD.open("/config.txt", FILE_READ);
                if (f) {
                    char buf[64];
                    f.read((uint8_t*)buf, sizeof(buf));
                    f.close();
                }
            }
        }

        // TFT operation
        display.fillScreen(i % 2 == 0 ? TFT_BLACK : TFT_BLUE);

        // Try to draw BMP (if available)
        String path = "/images/kaa001.bmp";
        {
            hal::SDCard::Lock lock("stressCheck");
            if (lock.acquired() && SD.exists(path.c_str())) {
                display.drawBMP(path);
            }
        }

        Serial.printf("✓ Iteration %d complete - no deadlock\n", i + 1);
        delay(500);
    }

    Serial.println("✓ Stress test complete - SPI pattern is safe!");
    TEST_PASS();
}

// ────────────────────────────────────────────────────────────────────────────
// TEST 6: Error Handling
// ────────────────────────────────────────────────────────────────────────────
void test_ErrorHandling() {
    TEST_START("Error Handling");

    auto& display = hal::DisplayDriver::getInstance();

    // Test 6.1: Missing file
    Serial.println("[6.1] Testing missing file...");
    bool result = display.drawBMP("/nonexistent.bmp");
    if (!result) {
        Serial.println("✓ Correctly handled missing file");
    } else {
        TEST_FAIL("Should have failed for missing file");
        return;
    }
    delay(2000);

    // Test 6.2: Invalid path
    Serial.println("[6.2] Testing invalid path...");
    result = display.drawBMP("");
    if (!result) {
        Serial.println("✓ Correctly handled invalid path");
    } else {
        TEST_FAIL("Should have failed for invalid path");
        return;
    }

    TEST_PASS();
}

// ────────────────────────────────────────────────────────────────────────────
// Serial Commands
// ────────────────────────────────────────────────────────────────────────────
void processSerialCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "HELP") {
        Serial.println("\nAvailable Commands:");
        Serial.println("  HELP       - Show this help");
        Serial.println("  STATUS     - Show current status");
        Serial.println("  TEST_ALL   - Run all tests");
        Serial.println("  TEST_1     - Test HAL initialization");
        Serial.println("  TEST_2     - Test basic display operations");
        Serial.println("  TEST_3     - Test direct TFT access");
        Serial.println("  TEST_4     - Test BMP rendering");
        Serial.println("  TEST_5     - Test SPI deadlock prevention");
        Serial.println("  TEST_6     - Test error handling");
        Serial.println("  STATS      - Show test statistics");
        Serial.println("  REBOOT     - Restart ESP32");
    }
    else if (cmd == "STATUS") {
        Serial.println("\nStatus:");
        Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("  Tests passed: %d\n", stats.testsPassed);
        Serial.printf("  Tests failed: %d\n", stats.testsFailed);

        auto& sd = hal::SDCard::getInstance();
        Serial.printf("  SD card present: %s\n", sd.isPresent() ? "Yes" : "No");

        auto& display = hal::DisplayDriver::getInstance();
        Serial.printf("  Display initialized: %s\n", display.isInitialized() ? "Yes" : "No");
    }
    else if (cmd == "TEST_ALL") {
        stats.testsPassed = 0;
        stats.testsFailed = 0;
        stats.startHeap = ESP.getFreeHeap();

        test_HalInitialization();
        test_BasicDisplayOperations();
        test_DirectTFTAccess();
        test_BMPRendering();
        test_SPIDeadlockStressTest();
        test_ErrorHandling();

        reportStats();
    }
    else if (cmd == "TEST_1") test_HalInitialization();
    else if (cmd == "TEST_2") test_BasicDisplayOperations();
    else if (cmd == "TEST_3") test_DirectTFTAccess();
    else if (cmd == "TEST_4") test_BMPRendering();
    else if (cmd == "TEST_5") test_SPIDeadlockStressTest();
    else if (cmd == "TEST_6") test_ErrorHandling();
    else if (cmd == "STATS") reportStats();
    else if (cmd == "REBOOT") ESP.restart();
    else {
        Serial.printf("Unknown command: %s\n", cmd.c_str());
        Serial.println("Type HELP for available commands");
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Arduino Setup
// ────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial monitor

    Serial.println("\n\n");
    printSeparator();
    Serial.println("DisplayDriver HAL Test Sketch");
    Serial.println("ALNScanner v5.0 - Test 52");
    printSeparator();
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Chip model: %s\n", ESP.getChipModel());
    Serial.printf("CPU frequency: %d MHz\n", ESP.getCpuFreqMHz());
    printSeparator();

    stats.startHeap = ESP.getFreeHeap();

    // Run all tests automatically
    Serial.println("\nRunning all tests automatically...");
    Serial.println("Type HELP for manual test commands");
    delay(2000);

    test_HalInitialization();
    test_BasicDisplayOperations();
    test_DirectTFTAccess();
    test_BMPRendering();
    test_SPIDeadlockStressTest();
    test_ErrorHandling();

    reportStats();

    Serial.println("\nSetup complete. Type HELP for commands.");
}

// ────────────────────────────────────────────────────────────────────────────
// Arduino Loop
// ────────────────────────────────────────────────────────────────────────────
void loop() {
    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        processSerialCommand(cmd);
    }

    delay(100);
}
