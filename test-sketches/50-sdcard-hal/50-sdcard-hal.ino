/**
 * Test Sketch: SDCard HAL Component
 *
 * Validates the SDCard.h HAL implementation for ALNScanner v5.0
 *
 * Tests:
 * 1. Singleton pattern works correctly
 * 2. SD card initialization succeeds
 * 3. RAII Lock acquires and releases mutex automatically
 * 4. Manual mutex operations work
 * 5. File read/write with proper locking
 * 6. Multi-threaded access (simulated with background task)
 *
 * Hardware: CYD ESP32-2432S028R with SD card inserted
 * Expected: All tests PASS, files created successfully
 */

// Copy config.h and SDCard.h to local directory for standalone testing
#include "config.h"
#include "hal/SDCard.h"

// Test statistics
struct TestStats {
    int passed = 0;
    int failed = 0;
    int total = 0;
} stats;

// Helper macros
#define TEST_START(name) \
    Serial.printf("\n[TEST %d] %s\n", ++stats.total, name); \
    unsigned long testStart = millis();

#define TEST_PASS() \
    stats.passed++; \
    Serial.printf("✓ PASS (took %lu ms)\n", millis() - testStart);

#define TEST_FAIL(reason) \
    stats.failed++; \
    Serial.printf("✗ FAIL: %s\n", reason);

#define ASSERT(condition, message) \
    if (!(condition)) { \
        TEST_FAIL(message); \
        return; \
    }

// Background task handle for multi-threaded test
TaskHandle_t backgroundTaskHandle = nullptr;
volatile int backgroundWrites = 0;

// Background task that writes to SD card
void backgroundTask(void* parameter) {
    Serial.println("[BG-TASK] Started on Core 0");

    for (int i = 0; i < 10; i++) {
        // Wait a bit between writes
        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Use RAII lock
        hal::SDCard::Lock lock("backgroundTask");
        if (lock.acquired()) {
            File f = SD.open("/bg_test.txt", FILE_APPEND);
            if (f) {
                f.printf("Background write #%d\n", i);
                f.close();
                backgroundWrites++;
                Serial.printf("[BG-TASK] Write #%d completed\n", i + 1);
            }
        }
    }

    Serial.println("[BG-TASK] Completed all writes");
    vTaskDelete(nullptr);
}

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial monitor

    Serial.println("\n╔═══════════════════════════════════════════════════════╗");
    Serial.println("║       SDCard HAL Test Suite - ALNScanner v5.0       ║");
    Serial.println("╚═══════════════════════════════════════════════════════╝");

    runAllTests();

    // Print summary
    Serial.println("\n╔═══════════════════════════════════════════════════════╗");
    Serial.printf("║  Test Summary: %d passed, %d failed (of %d total)    \n",
                  stats.passed, stats.failed, stats.total);
    if (stats.failed == 0) {
        Serial.println("║  ✓✓✓ ALL TESTS PASSED ✓✓✓                           ║");
    } else {
        Serial.println("║  ✗✗✗ SOME TESTS FAILED ✗✗✗                          ║");
    }
    Serial.println("╚═══════════════════════════════════════════════════════╝");
}

void loop() {
    // Tests complete - just idle
    delay(1000);
}

void runAllTests() {
    test01_SingletonPattern();
    test02_Initialization();
    test03_RAIILock();
    test04_ManualMutex();
    test05_FileWrite();
    test06_FileRead();
    test07_NestedLocks();
    test08_LockTimeout();
    test09_MultiThreaded();
    test10_CardInfo();
}

// ═════════════════════════════════════════════════════════════════════
// TEST CASES
// ═════════════════════════════════════════════════════════════════════

void test01_SingletonPattern() {
    TEST_START("Singleton Pattern");

    // Get two references to singleton
    auto& sd1 = hal::SDCard::getInstance();
    auto& sd2 = hal::SDCard::getInstance();

    // They should be the same instance
    ASSERT(&sd1 == &sd2, "Singleton instances are different");

    TEST_PASS();
}

void test02_Initialization() {
    TEST_START("SD Card Initialization");

    auto& sd = hal::SDCard::getInstance();
    bool initialized = sd.begin();

    ASSERT(initialized, "SD card initialization failed - is card inserted?");
    ASSERT(sd.isPresent(), "isPresent() returns false after successful init");

    TEST_PASS();
}

void test03_RAIILock() {
    TEST_START("RAII Lock Acquire/Release");

    auto& sd = hal::SDCard::getInstance();

    // Test lock in nested scope
    {
        hal::SDCard::Lock lock("test03");
        ASSERT(lock.acquired(), "RAII lock failed to acquire mutex");

        Serial.println("  Lock acquired in nested scope");
    }  // Lock should be automatically released here

    Serial.println("  Lock automatically released after scope exit");

    // Verify we can acquire again immediately
    {
        hal::SDCard::Lock lock2("test03_again");
        ASSERT(lock2.acquired(), "Failed to reacquire after RAII release");
    }

    TEST_PASS();
}

void test04_ManualMutex() {
    TEST_START("Manual Mutex Operations");

    auto& sd = hal::SDCard::getInstance();

    // Manual acquire
    ASSERT(sd.takeMutex("test04", 500), "Manual takeMutex failed");

    Serial.println("  Mutex manually acquired");

    // Manual release
    sd.giveMutex("test04");
    Serial.println("  Mutex manually released");

    // Verify we can acquire again
    ASSERT(sd.takeMutex("test04_again", 500), "Failed to reacquire after manual release");
    sd.giveMutex("test04_again");

    TEST_PASS();
}

void test05_FileWrite() {
    TEST_START("File Write with RAII Lock");

    const char* testFile = "/test_write.txt";
    const char* testData = "Hello from SDCard HAL!";

    // Delete old test file if exists
    SD.remove(testFile);

    // Write with RAII lock
    {
        hal::SDCard::Lock lock("test05");
        ASSERT(lock.acquired(), "Failed to acquire lock for write");

        File f = SD.open(testFile, FILE_WRITE);
        ASSERT(f, "Failed to open file for writing");

        size_t written = f.println(testData);
        f.close();

        ASSERT(written > 0, "Failed to write data to file");
        Serial.printf("  Wrote %d bytes to %s\n", written, testFile);
    }

    // Verify file exists
    ASSERT(SD.exists(testFile), "File does not exist after write");

    TEST_PASS();
}

void test06_FileRead() {
    TEST_START("File Read with RAII Lock");

    const char* testFile = "/test_write.txt";

    // Read with RAII lock
    String content;
    {
        hal::SDCard::Lock lock("test06");
        ASSERT(lock.acquired(), "Failed to acquire lock for read");

        File f = SD.open(testFile, FILE_READ);
        ASSERT(f, "Failed to open file for reading");

        content = f.readString();
        f.close();

        Serial.printf("  Read %d bytes: %s", content.length(), content.c_str());
    }

    ASSERT(content.length() > 0, "Read zero bytes from file");
    ASSERT(content.indexOf("SDCard HAL") >= 0, "File content doesn't match expected");

    TEST_PASS();
}

void test07_NestedLocks() {
    TEST_START("Nested RAII Locks (should fail)");

    // First lock should succeed
    hal::SDCard::Lock lock1("test07_outer");
    ASSERT(lock1.acquired(), "First lock failed");
    Serial.println("  First lock acquired");

    // Second lock should timeout (same task can't acquire mutex twice)
    hal::SDCard::Lock lock2("test07_inner", 100);  // Short timeout

    // This is expected behavior - FreeRTOS mutexes are NOT recursive
    if (!lock2.acquired()) {
        Serial.println("  Second lock failed as expected (non-recursive mutex)");
        TEST_PASS();
    } else {
        TEST_FAIL("Second lock succeeded (mutex should not be recursive)");
    }
}

void test08_LockTimeout() {
    TEST_START("Lock Timeout Handling");

    // Hold lock in main task
    hal::SDCard::Lock lock1("test08_holder");
    ASSERT(lock1.acquired(), "Failed to acquire initial lock");

    Serial.println("  Lock held by main task");

    // Try to acquire with very short timeout (should fail)
    hal::SDCard::Lock lock2("test08_waiter", 50);  // 50ms timeout

    if (!lock2.acquired()) {
        Serial.println("  Second lock timed out as expected");
        TEST_PASS();
    } else {
        TEST_FAIL("Second lock should have timed out");
    }
}

void test09_MultiThreaded() {
    TEST_START("Multi-Threaded Access (Core 0 + Core 1)");

    // Reset counter
    backgroundWrites = 0;

    // Delete old test file
    SD.remove("/bg_test.txt");

    // Create background task on Core 0
    xTaskCreatePinnedToCore(
        backgroundTask,
        "bgTask",
        4096,
        nullptr,
        1,
        &backgroundTaskHandle,
        0  // Core 0
    );

    ASSERT(backgroundTaskHandle != nullptr, "Failed to create background task");
    Serial.println("  Background task created on Core 0");

    // Main task writes on Core 1 (where setup() runs)
    for (int i = 0; i < 10; i++) {
        delay(100);

        hal::SDCard::Lock lock("test09_main");
        if (lock.acquired()) {
            File f = SD.open("/main_test.txt", FILE_APPEND);
            if (f) {
                f.printf("Main task write #%d\n", i);
                f.close();
                Serial.printf("  [MAIN] Write #%d completed\n", i + 1);
            }
        }
    }

    // Wait for background task to complete
    Serial.println("  Waiting for background task...");
    delay(2000);

    // Verify both tasks wrote successfully
    ASSERT(backgroundWrites == 10, "Background task didn't complete all writes");
    Serial.printf("  Background task completed %d writes\n", backgroundWrites);

    // Verify files exist
    ASSERT(SD.exists("/bg_test.txt"), "Background task file not found");
    ASSERT(SD.exists("/main_test.txt"), "Main task file not found");

    TEST_PASS();
}

void test10_CardInfo() {
    TEST_START("SD Card Information");

    auto& sd = hal::SDCard::getInstance();
    ASSERT(sd.isPresent(), "SD card not present");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
    uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);

    Serial.printf("  Card size: %llu MB\n", cardSize);
    Serial.printf("  Total space: %llu MB\n", totalBytes);
    Serial.printf("  Used space: %llu MB\n", usedBytes);
    Serial.printf("  Free space: %llu MB\n", totalBytes - usedBytes);

    ASSERT(cardSize > 0, "Card size is zero");

    TEST_PASS();
}
