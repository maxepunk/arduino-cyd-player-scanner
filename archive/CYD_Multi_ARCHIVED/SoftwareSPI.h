/**
 * SoftwareSPI.h - Software SPI implementation with precise timing control
 * 
 * Provides bit-bang SPI communication for RFID reader with timing control,
 * interrupt protection, and GPIO27 integration for CYD devices.
 */

#ifndef SOFTWARE_SPI_H
#define SOFTWARE_SPI_H

#include <Arduino.h>
#include "RFIDConfig.h"

/**
 * SPI Mode Definitions
 */
enum SoftSPIMode {
    SOFT_SPI_MODE0 = 0,  // CPOL=0, CPHA=0 (Clock idle low, data sampled on rising edge)
    SOFT_SPI_MODE1 = 1,  // CPOL=0, CPHA=1 (Clock idle low, data sampled on falling edge)
    SOFT_SPI_MODE2 = 2,  // CPOL=1, CPHA=0 (Clock idle high, data sampled on falling edge)
    SOFT_SPI_MODE3 = 3   // CPOL=1, CPHA=1 (Clock idle high, data sampled on rising edge)
};

/**
 * SPI Timing Configuration
 */
struct SPITiming {
    uint8_t clockDelayUs;        // Microseconds between clock edges
    uint8_t operationDelayUs;    // Delay after operations
    uint16_t timeoutMs;          // Operation timeout
    uint8_t maxRetries;          // Retry count for failed operations
    bool useInterruptProtection; // Use critical sections
};

/**
 * SPI Transfer Statistics
 */
struct SPIStats {
    uint32_t totalTransfers;
    uint32_t successfulTransfers;
    uint32_t failedTransfers;
    uint32_t timeoutTransfers;
    uint32_t totalBytes;
    uint32_t avgTransferTimeUs;
    uint32_t maxTransferTimeUs;
    uint32_t minTransferTimeUs;
    uint32_t lastTransferTime;
    uint32_t errorCount;
};

/**
 * SPI Pin Configuration
 */
struct SPIPins {
    uint8_t sck;     // Serial Clock
    uint8_t mosi;    // Master Out, Slave In
    uint8_t miso;    // Master In, Slave Out
    uint8_t ss;      // Slave Select (optional, managed externally)
};

/**
 * Software SPI Interface
 */
class ISoftwareSPI {
public:
    virtual ~ISoftwareSPI() = default;
    
    virtual bool initialize(const SPIPins& pins, SoftSPIMode mode = SOFT_SPI_MODE0) = 0;
    virtual bool setTiming(const SPITiming& timing) = 0;
    virtual uint8_t transfer(uint8_t data) = 0;
    virtual bool transfer(uint8_t* txData, uint8_t* rxData, size_t length) = 0;
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual SPIStats getStatistics() const = 0;
    virtual bool isInitialized() const = 0;
};

/**
 * CYD-specific Software SPI Implementation
 */
class SoftwareSPI : public ISoftwareSPI {
private:
    SPIPins pins;
    SoftSPIMode mode;
    SPITiming timing;
    SPIStats stats;
    bool initialized;
    bool pinsClaimed;
    
    // GPIO27 integration
    bool usesGPIO27;
    bool gpio27Acquired;
    
    // Timing constants
    static const uint8_t DEFAULT_CLOCK_DELAY_US = 5;
    static const uint8_t MIN_CLOCK_DELAY_US = 1;
    static const uint8_t MAX_CLOCK_DELAY_US = 100;
    static const uint16_t DEFAULT_TIMEOUT_MS = 100;
    static const uint8_t DEFAULT_MAX_RETRIES = 3;
    
    // Internal methods
    void configurePins();
    void releasePins();
    bool acquireGPIO27();
    void releaseGPIO27();
    void setClock(bool high);
    void setData(bool high);
    bool readData();
    void delayMicroseconds(uint8_t us);
    uint8_t transferByte(uint8_t data);
    bool validateTiming(const SPITiming& timing) const;
    void updateStatistics(uint32_t transferTime, bool success);
    
    // Critical section helpers
    void enterCriticalSection();
    void exitCriticalSection();
    
    // Timing verification
    bool verifyTimingAccuracy();
    void calibrateTiming();
    
public:
    SoftwareSPI();
    ~SoftwareSPI();
    
    // ISoftwareSPI interface implementation
    bool initialize(const SPIPins& pins, SoftSPIMode mode = SOFT_SPI_MODE0) override;
    bool setTiming(const SPITiming& timing) override;
    uint8_t transfer(uint8_t data) override;
    bool transfer(uint8_t* txData, uint8_t* rxData, size_t length) override;
    void begin() override;
    void end() override;
    SPIStats getStatistics() const override { return stats; }
    bool isInitialized() const override { return initialized; }
    
    // Additional utility methods
    bool setClockFrequency(uint32_t frequencyHz);
    uint32_t getClockFrequency() const;
    bool testConnection();
    void resetStatistics();
    
    // Pin state management
    bool arePinsAvailable(const SPIPins& pins);
    void printPinConfiguration() const;
    bool validatePinConfiguration() const;
    
    // Performance optimization
    void optimizeForSpeed();
    void optimizeForReliability();
    void setMaxClockSpeed();
    
    // GPIO27 specific methods
    bool requiresGPIO27() const { return usesGPIO27; }
    bool isGPIO27Available();
    void forceGPIO27Release();
    
    // Diagnostics and debugging
    void printDiagnostics() const;
    bool performSelfTest();
    void dumpConfiguration() const;
    
    // Advanced transfer methods
    bool transferBlock(const uint8_t* txData, uint8_t* rxData, 
                      size_t length, uint32_t timeoutMs);
    bool transferWithRetry(uint8_t data, uint8_t* response, uint8_t maxRetries);
    
    // RFID-specific helper methods
    bool rfidInit();
    bool rfidSendCommand(uint8_t command, const uint8_t* data, size_t length);
    bool rfidReadResponse(uint8_t* buffer, size_t maxLength, size_t* actualLength);
    bool rfidTransceive(const uint8_t* txData, size_t txLength,
                       uint8_t* rxData, size_t maxRxLength, size_t* actualRxLength);
};

/**
 * SPI Transaction Helper (RAII)
 */
class SPITransaction {
private:
    SoftwareSPI* spi;
    bool active;
    uint32_t startTime;
    
public:
    SPITransaction(SoftwareSPI* spiInstance);
    ~SPITransaction();
    
    bool isActive() const { return active; }
    uint32_t getDuration() const { return millis() - startTime; }
};

/**
 * Predefined Timing Configurations
 */
namespace SPITimingPresets {
    // Fast timing for reliable hardware
    static const SPITiming FAST = {
        .clockDelayUs = 2,
        .operationDelayUs = 1,
        .timeoutMs = 50,
        .maxRetries = 2,
        .useInterruptProtection = true
    };
    
    // Standard timing for most applications
    static const SPITiming STANDARD = {
        .clockDelayUs = 5,
        .operationDelayUs = 2,
        .timeoutMs = 100,
        .maxRetries = 3,
        .useInterruptProtection = true
    };
    
    // Slow timing for problematic hardware
    static const SPITiming SLOW = {
        .clockDelayUs = 10,
        .operationDelayUs = 5,
        .timeoutMs = 200,
        .maxRetries = 5,
        .useInterruptProtection = true
    };
    
    // RFID-optimized timing
    static const SPITiming RFID_OPTIMIZED = {
        .clockDelayUs = 3,
        .operationDelayUs = 2,
        .timeoutMs = 150,
        .maxRetries = 3,
        .useInterruptProtection = true
    };
}

/**
 * Helper Functions and Macros
 */
namespace SPIHelper {
    // Pin validation
    bool isValidPin(uint8_t pin);
    bool isPinAvailable(uint8_t pin);
    bool canPinBeOutput(uint8_t pin);
    bool canPinBeInput(uint8_t pin);
    
    // Timing calculation
    uint32_t calculateFrequency(uint8_t clockDelayUs);
    uint8_t calculateClockDelay(uint32_t frequencyHz);
    
    // Mode conversion
    const char* modeToString(SoftSPIMode mode);
    bool isValidMode(SoftSPIMode mode);
    
    // Statistics formatting
    void printStatistics(const SPIStats& stats);
    void formatTransferRate(uint32_t bytesPerSecond, char* buffer, size_t bufferSize);
}

// Convenience macros
#define SPI_TRANSACTION(spi) SPITransaction _transaction(spi)
#define SPI_CHECK_INIT(spi) do { if (!(spi)->isInitialized()) return false; } while(0)

/**
 * Configuration Constants
 */
namespace SPIConfig {
    // Hardware limits
    static const uint32_t MIN_FREQUENCY_HZ = 1000;        // 1 kHz
    static const uint32_t MAX_FREQUENCY_HZ = 1000000;     // 1 MHz
    static const uint32_t DEFAULT_FREQUENCY_HZ = 100000;  // 100 kHz
    
    // Transfer limits
    static const size_t MAX_TRANSFER_SIZE = 256;          // Maximum bytes per transfer
    static const uint32_t MAX_TRANSFER_TIME_MS = 1000;    // Maximum transfer duration
    
    // Statistics
    static const uint32_t STATS_RESET_INTERVAL = 3600000; // Reset stats every hour
    
    // GPIO constraints for CYD
    static const uint8_t FORBIDDEN_PINS[] = {1, 6, 7, 8, 9, 10, 11}; // Boot/flash pins
    static const size_t FORBIDDEN_PIN_COUNT = 7;
}

#endif // SOFTWARE_SPI_H