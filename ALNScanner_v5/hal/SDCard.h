#pragma once

/**
 * @file SDCard.h
 * @brief Thread-safe SD card HAL component for ALNScanner v5.0
 *
 * Provides RAII-based mutex protection for SD card access across FreeRTOS cores.
 * All other HAL components (Display, Audio, RFID) depend on this foundation.
 *
 * Key Features:
 * - Singleton pattern for global access
 * - RAII Lock class for automatic mutex management
 * - FreeRTOS mutex protection for Core 0/Core 1 synchronization
 * - Comprehensive logging for debugging
 *
 * Extracted from v4.1 monolithic codebase (lines 105, 151, 1241-1257, 2709-2718)
 */

#include <SD.h>
#include <SPI.h>
#include <freertos/semphr.h>
#include "../config.h"

namespace hal {

/**
 * @class SDCard
 * @brief Singleton SD card manager with thread-safe access
 *
 * Usage Pattern:
 *
 * Basic RAII Lock (recommended):
 * @code
 * auto& sd = hal::SDCard::getInstance();
 * {
 *     hal::SDCard::Lock lock("myFunction");
 *     if (lock.acquired()) {
 *         File f = SD.open("/file.txt", FILE_WRITE);
 *         f.println("data");
 *         f.close();
 *     }
 * }  // Lock automatically released here
 * @endcode
 *
 * Manual Mutex (for special cases):
 * @code
 * if (sd.takeMutex("caller", 500)) {
 *     // ... SD operations ...
 *     sd.giveMutex("caller");
 * }
 * @endcode
 */
class SDCard {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the global SDCard instance
     */
    static SDCard& getInstance() {
        static SDCard instance;
        return instance;
    }

    /**
     * @brief Initialize SD card and create mutex
     * @return true if SD card mounted successfully, false otherwise
     *
     * This method:
     * 1. Creates the FreeRTOS mutex for thread synchronization
     * 2. Initializes the VSPI hardware SPI bus
     * 3. Attempts to mount the SD card
     * 4. Sets _present flag based on mount success
     */
    inline bool begin() {
        LOG_INFO("[SD-HAL] Initializing SD card...\n");

        // Create mutex for thread-safe operations (Core 0 + Core 1)
        if (!_mutex) {
            _mutex = xSemaphoreCreateMutex();
            if (!_mutex) {
                LOG_ERROR("SD-HAL", "Failed to create SD mutex");
                return false;
            }
            LOG_INFO("[SD-HAL] Mutex created successfully\n");
        }

        // Initialize VSPI hardware SPI bus for SD card
        // Note: TFT also uses VSPI, so SD operations must not hold TFT lock
        _spi.begin(pins::SD_SCK, pins::SD_MISO, pins::SD_MOSI);

        // Attempt SD card mount
        if (!SD.begin(pins::SD_CS, _spi)) {
            LOG_ERROR("SD-HAL", "SD card mount failed - no card present");
            _present = false;
            return false;
        }

        _present = true;
        LOG_INFO("[SD-HAL] SD card mounted successfully\n");

        // Report card information
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        LOG_INFO("[SD-HAL] Card size: %llu MB\n", cardSize);

        return true;
    }

    /**
     * @brief Check if SD card is present and mounted
     * @return true if SD card is available, false otherwise
     */
    inline bool isPresent() const {
        return _present;
    }

    /**
     * @class Lock
     * @brief RAII lock for automatic mutex management
     *
     * Constructor acquires mutex, destructor releases it automatically.
     * Prevents forgetting to release mutex in complex control flow.
     */
    class Lock {
    public:
        /**
         * @brief Acquire SD mutex with timeout
         * @param caller Name of calling function (for debugging)
         * @param timeoutMs Maximum time to wait for mutex (default: 500ms)
         */
        Lock(const char* caller, uint32_t timeoutMs = freertos_config::SD_MUTEX_TIMEOUT_MS)
            : _acquired(false), _caller(caller) {

            auto& sd = SDCard::getInstance();
            _acquired = sd.takeMutex(caller, timeoutMs);

            if (!_acquired) {
                LOG_ERROR("SD-HAL", "RAII Lock failed to acquire mutex");
            }
        }

        /**
         * @brief Release SD mutex automatically
         */
        ~Lock() {
            if (_acquired) {
                auto& sd = SDCard::getInstance();
                sd.giveMutex(_caller);
            }
        }

        /**
         * @brief Check if mutex was successfully acquired
         * @return true if lock is held, false if acquisition timed out
         */
        inline bool acquired() const {
            return _acquired;
        }

        // Prevent copying (RAII locks must not be copied)
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;

    private:
        bool _acquired;
        const char* _caller;
    };

    /**
     * @brief Manually acquire SD mutex (low-level)
     * @param caller Name of calling function (for debugging)
     * @param timeoutMs Maximum time to wait for mutex
     * @return true if mutex acquired, false if timeout occurred
     *
     * Use RAII Lock class instead whenever possible.
     */
    inline bool takeMutex(const char* caller, unsigned long timeoutMs) {
        // Handle case where mutex not yet initialized (boot phase)
        if (!_mutex) {
            LOG_INFO("[SD-HAL] Mutex not initialized, allowing unprotected access\n");
            return true;
        }

        bool gotLock = xSemaphoreTake(_mutex, timeoutMs / portTICK_PERIOD_MS) == pdTRUE;

        if (!gotLock) {
            LOG_ERROR("SD-HAL", "Mutex timeout waiting for lock");
            Serial.printf("        Caller: %s, Timeout: %lu ms\n", caller, timeoutMs);
        } else {
            LOG_DEBUG("[SD-HAL] Mutex acquired by %s\n", caller);
        }

        return gotLock;
    }

    /**
     * @brief Manually release SD mutex (low-level)
     * @param caller Name of calling function (for debugging)
     *
     * Use RAII Lock class instead whenever possible.
     */
    inline void giveMutex(const char* caller) {
        if (_mutex) {
            xSemaphoreGive(_mutex);
            LOG_DEBUG("[SD-HAL] Mutex released by %s\n", caller);
        }
    }

private:
    // Singleton pattern - private constructors
    SDCard() = default;
    ~SDCard() = default;
    SDCard(const SDCard&) = delete;
    SDCard& operator=(const SDCard&) = delete;

    // Static members
    static SPIClass _spi;           // VSPI bus for SD card
    static SemaphoreHandle_t _mutex;
    static bool _present;
};

// Static member initialization
SPIClass SDCard::_spi(VSPI);
SemaphoreHandle_t SDCard::_mutex = nullptr;
bool SDCard::_present = false;

} // namespace hal

/**
 * IMPLEMENTATION NOTES (from v4.1 extraction)
 *
 * 1. MUTEX CRITICAL FOR DUAL-CORE OPERATION
 *    - Main loop (Core 1) handles RFID scanning, display updates, config loading
 *    - Background task (Core 0) handles queue uploads, token sync
 *    - Both cores access SD card concurrently � Race conditions without mutex
 *
 * 2. SPI BUS DEADLOCK PREVENTION (CRITICAL PATTERN)
 *    - SD card and TFT_eSPI both use VSPI hardware SPI bus
 *    - NEVER hold TFT lock while reading from SD (causes system freeze)
 *    - Pattern: Read SD first, THEN lock TFT for display
 *    Example:
 *      File f = SD.open("image.bmp");
 *      f.read(buffer, size);      // SD read FIRST
 *      f.close();
 *      tft.startWrite();           // TFT lock SECOND
 *      tft.pushImage(...);
 *      tft.endWrite();
 *
 * 3. RAII PATTERN ADVANTAGES
 *    - Automatic mutex release even if function returns early
 *    - Exception-safe (if ESP32 had exceptions, but pattern still useful)
 *    - Prevents forgetting to release mutex
 *    - Clear scope of critical section
 *
 * 4. TIMEOUT VALUES
 *    - Standard timeout: 500ms (SD_MUTEX_TIMEOUT_MS)
 *    - Long operations: 2000ms (SD_MUTEX_LONG_TIMEOUT_MS)
 *    - Boot phase: Mutex may be nullptr, allow unprotected access
 *
 * 5. LOGGING LEVELS
 *    - LOG_INFO: Always compiled, used for major events
 *    - LOG_DEBUG: Compiled only in DEBUG_MODE, verbose mutex operations
 *    - LOG_ERROR: Always compiled, critical failures
 *
 * 6. EXTRACTED FROM v4.1 LINES
 *    - Line 105: sdCardPresent global variable
 *    - Line 151: sdMutex global variable
 *    - Lines 1241-1257: sdTakeMutex() and sdGiveMutex() functions
 *    - Lines 2709-2718: SD card initialization in setup()
 *    - Lines 2728-2729: Mutex creation for orchestrator operations
 */
