/**
 * DiagnosticReporter.h - Comprehensive diagnostic reporting system
 * 
 * Implements the diagnostic reporting contract with structured logging,
 * memory tracking, pin state analysis, and SPI diagnostics.
 */

#ifndef DIAGNOSTIC_REPORTER_H
#define DIAGNOSTIC_REPORTER_H

#include <Arduino.h>
#include "DiagnosticState.h"

// Import contract types
enum DiagLevel {
    DIAG_DEBUG = 0,
    DIAG_INFO = 1,
    DIAG_WARNING = 2,
    DIAG_ERROR = 3,
    DIAG_CRITICAL = 4
};

enum DiagCategory {
    CAT_HARDWARE = 0,
    CAT_DISPLAY = 1,
    CAT_TOUCH = 2,
    CAT_RFID = 3,
    CAT_SDCARD = 4,
    CAT_AUDIO = 5,
    CAT_MEMORY = 6,
    CAT_PERFORMANCE = 7
};

struct DiagMessage {
    uint32_t timestamp;
    DiagLevel level;
    DiagCategory category;
    char component[16];
    char message[128];
    
    struct {
        uint32_t errorCode;
        uint32_t value1;
        uint32_t value2;
        float floatValue;
    } extended;
};

struct PinDiagnostic {
    uint8_t pin;
    uint8_t mode;           // INPUT, OUTPUT, INPUT_PULLUP
    uint8_t state;          // HIGH, LOW
    bool isExpected;
    char description[32];
};

struct MemoryDiagnostic {
    uint32_t totalHeap;
    uint32_t freeHeap;
    uint32_t largestFreeBlock;
    uint32_t minimumEverFree;
    float fragmentationPercent;
};

/**
 * Diagnostic Reporter Interface (from contract)
 */
class IDiagnosticReporter {
public:
    virtual ~IDiagnosticReporter() = default;
    virtual void setDiagnosticLevel(DiagLevel level) = 0;
    virtual void report(const DiagMessage& message) = 0;
    virtual void reportf(DiagLevel level, DiagCategory category, 
                         const char* component, const char* format, ...) = 0;
    virtual void reportPinStates(const PinDiagnostic* pins, size_t count) = 0;
    virtual MemoryDiagnostic reportMemory() = 0;
    virtual void reportWiringIssue(uint8_t expectedPin, uint8_t actualState, 
                                   const char* suggestion) = 0;
    virtual void reportSPIDiagnostics(const char* busName, uint32_t success, 
                                      uint32_t failures, uint32_t avgTimeUs) = 0;
    virtual void dumpSystemState(bool includeHistory = true) = 0;
    virtual void clearHistory() = 0;
    virtual size_t getHistory(DiagMessage* buffer, size_t maxCount) = 0;
};

/**
 * Diagnostic History Management
 */
struct DiagnosticHistory {
    static const size_t MAX_MESSAGES = 50;
    DiagMessage messages[MAX_MESSAGES];
    size_t count;
    size_t writeIndex;
    bool bufferFull;
    uint32_t totalMessages;
};

/**
 * SPI Bus Diagnostics
 */
struct SPIDiagnostic {
    char busName[16];
    uint32_t successfulTransfers;
    uint32_t failedTransfers;
    uint32_t totalTransfers;
    uint32_t avgTransferTimeUs;
    uint32_t maxTransferTimeUs;
    uint32_t minTransferTimeUs;
    uint32_t lastErrorCode;
    uint32_t lastActivity;
};

/**
 * System Health Metrics
 */
struct SystemHealth {
    uint32_t bootTime;
    uint32_t uptime;
    uint32_t totalResets;
    uint32_t watchdogResets;
    float averageLoopTime;
    uint32_t maxLoopTime;
    uint32_t criticalErrors;
    uint32_t warnings;
    bool isStable;
};

/**
 * CYD Diagnostic Reporter Implementation
 */
class DiagnosticReporter : public IDiagnosticReporter {
private:
    DiagLevel minLevel;
    DiagnosticHistory history;
    SystemHealth health;
    SPIDiagnostic spiStats[4]; // Support up to 4 SPI buses
    size_t spiCount;
    
    // Memory tracking
    uint32_t lastMemoryCheck;
    MemoryDiagnostic lastMemoryState;
    static const uint32_t MEMORY_CHECK_INTERVAL = 30000; // 30 seconds
    
    // Performance tracking
    uint32_t lastLoopTime;
    uint32_t loopCounter;
    uint32_t performanceStartTime;
    
    // Internal methods
    void addToHistory(const DiagMessage& message);
    const char* levelToString(DiagLevel level) const;
    const char* categoryToString(DiagCategory category) const;
    void formatMessage(const DiagMessage& message, char* buffer, size_t bufferSize) const;
    void updateSystemHealth();
    SPIDiagnostic* findOrCreateSPIStats(const char* busName);
    
    // Pin analysis helpers
    bool isPinConfigurationValid(uint8_t pin, uint8_t expectedMode);
    const char* pinModeToString(uint8_t mode) const;
    const char* pinStateToString(uint8_t state) const;
    
    // Memory analysis helpers
    void analyzeMemoryFragmentation(MemoryDiagnostic& memDiag);
    void checkMemoryLeaks();
    
    // Format helpers
    void printTimestamp(uint32_t timestamp) const;
    void printHex(uint32_t value, uint8_t digits = 8) const;
    
public:
    DiagnosticReporter();
    
    // IDiagnosticReporter interface implementation
    void setDiagnosticLevel(DiagLevel level) override;
    void report(const DiagMessage& message) override;
    void reportf(DiagLevel level, DiagCategory category, 
                 const char* component, const char* format, ...) override;
    void reportPinStates(const PinDiagnostic* pins, size_t count) override;
    MemoryDiagnostic reportMemory() override;
    void reportWiringIssue(uint8_t expectedPin, uint8_t actualState, 
                           const char* suggestion) override;
    void reportSPIDiagnostics(const char* busName, uint32_t success, 
                              uint32_t failures, uint32_t avgTimeUs) override;
    void dumpSystemState(bool includeHistory = true) override;
    void clearHistory() override;
    size_t getHistory(DiagMessage* buffer, size_t maxCount) override;
    
    // Additional diagnostic methods
    void updateLoopTime(uint32_t loopTimeUs);
    void reportSystemBoot();
    void reportSystemReset(const char* reason);
    void reportCriticalError(const char* component, const char* error);
    void reportPerformanceMetrics();
    
    // Pin diagnostic helpers
    void checkCriticalPins();
    void validateCYDPinConfiguration();
    void reportGPIO27Conflict();
    
    // Memory monitoring
    void enableMemoryMonitoring(bool enable);
    void checkMemoryHealth();
    void reportMemoryAlert(uint32_t freeHeap, uint32_t threshold);
    
    // Component health tracking
    void reportComponentHealth(const char* component, bool isHealthy, const char* details = nullptr);
    void updateComponentStatus(const char* component, DiagnosticState::ComponentStatus status);
    
    // System state queries
    SystemHealth getSystemHealth() const { return health; }
    bool isSystemStable() const { return health.isStable; }
    uint32_t getErrorCount() const { return history.totalMessages; }
    
    // Debugging and analysis
    void enableVerboseMode(bool enable);
    void reportDebugInfo(const char* component, const char* info);
    void dumpMemoryMap();
    void analyzeCrashDump();
};

/**
 * Helper Classes and Utilities
 */
class DiagnosticTimer {
private:
    uint32_t startTime;
    const char* operation;
    DiagnosticReporter* reporter;
    
public:
    DiagnosticTimer(DiagnosticReporter* rep, const char* op) 
        : reporter(rep), operation(op), startTime(micros()) {}
    
    ~DiagnosticTimer() {
        if (reporter) {
            uint32_t duration = micros() - startTime;
            reporter->reportf(DIAG_DEBUG, CAT_PERFORMANCE, "Timer", 
                            "%s took %lu us", operation, duration);
        }
    }
};

/**
 * Diagnostic Macros for Easy Use
 */
#define DIAG_ERROR(reporter, category, component, ...) \
    (reporter)->reportf(DIAG_ERROR, (category), (component), __VA_ARGS__)

#define DIAG_WARNING(reporter, category, component, ...) \
    (reporter)->reportf(DIAG_WARNING, (category), (component), __VA_ARGS__)

#define DIAG_INFO(reporter, category, component, ...) \
    (reporter)->reportf(DIAG_INFO, (category), (component), __VA_ARGS__)

#define DIAG_DEBUG(reporter, category, component, ...) \
    (reporter)->reportf(DIAG_DEBUG, (category), (component), __VA_ARGS__)

#define DIAG_TIMER(reporter, operation) \
    DiagnosticTimer _timer(reporter, operation)

/**
 * Configuration Constants
 */
namespace DiagConfig {
    // Output formatting
    static const size_t MAX_MESSAGE_LENGTH = 256;
    static const size_t TIMESTAMP_WIDTH = 10;
    static const size_t LEVEL_WIDTH = 8;
    static const size_t CATEGORY_WIDTH = 12;
    
    // Memory thresholds (bytes)
    static const uint32_t LOW_MEMORY_WARNING = 10240;  // 10KB
    static const uint32_t CRITICAL_MEMORY_THRESHOLD = 5120;  // 5KB
    static const float HIGH_FRAGMENTATION_THRESHOLD = 50.0f; // 50%
    
    // Performance thresholds
    static const uint32_t SLOW_LOOP_WARNING_US = 100000; // 100ms
    static const uint32_t CRITICAL_LOOP_TIME_US = 1000000; // 1 second
    
    // Pin validation
    enum CriticalPins {
        DISPLAY_CS = 15,
        DISPLAY_DC = 2,
        DISPLAY_RST = 4,
        BACKLIGHT_21 = 21,
        BACKLIGHT_27 = 27,
        SD_CS = 5,
        TOUCH_CS_PIN = 33,
        TOUCH_IRQ = 36,
        RFID_SS = 3
    };
}

#endif // DIAGNOSTIC_REPORTER_H