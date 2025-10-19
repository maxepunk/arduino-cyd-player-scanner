/**
 * Diagnostic Reporting Contract
 * 
 * Defines the interface for comprehensive diagnostic output
 * and error reporting throughout the system.
 */

#ifndef DIAGNOSTIC_REPORTING_H
#define DIAGNOSTIC_REPORTING_H

#include <Arduino.h>

// Diagnostic severity levels
enum DiagLevel {
    DIAG_DEBUG = 0,
    DIAG_INFO = 1,
    DIAG_WARNING = 2,
    DIAG_ERROR = 3,
    DIAG_CRITICAL = 4
};

// Diagnostic categories
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

/**
 * Diagnostic Message Structure
 */
struct DiagMessage {
    uint32_t timestamp;
    DiagLevel level;
    DiagCategory category;
    char component[16];
    char message[128];
    
    // Optional extended data
    struct {
        uint32_t errorCode;
        uint32_t value1;
        uint32_t value2;
        float floatValue;
    } extended;
};

/**
 * Pin State Information
 */
struct PinDiagnostic {
    uint8_t pin;
    uint8_t mode;           // INPUT, OUTPUT, INPUT_PULLUP
    uint8_t state;          // HIGH, LOW
    bool isExpected;
    char description[32];
};

/**
 * Memory Diagnostic Information
 */
struct MemoryDiagnostic {
    uint32_t totalHeap;
    uint32_t freeHeap;
    uint32_t largestFreeBlock;
    uint32_t minimumEverFree;
    float fragmentationPercent;
};

/**
 * Diagnostic Reporter Interface
 */
class IDiagnosticReporter {
public:
    virtual ~IDiagnosticReporter() = default;
    
    /**
     * Set minimum diagnostic level to report
     * @param level Minimum severity level
     */
    virtual void setDiagnosticLevel(DiagLevel level) = 0;
    
    /**
     * Report a diagnostic message
     * @param message Diagnostic message to report
     */
    virtual void report(const DiagMessage& message) = 0;
    
    /**
     * Quick report helper
     * @param level Severity level
     * @param category Message category
     * @param component Component name
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    virtual void reportf(DiagLevel level, DiagCategory category, 
                         const char* component, const char* format, ...) = 0;
    
    /**
     * Report pin configuration and state
     * @param pins Array of pin diagnostics
     * @param count Number of pins
     */
    virtual void reportPinStates(const PinDiagnostic* pins, size_t count) = 0;
    
    /**
     * Report memory usage statistics
     * @return Memory diagnostic information
     */
    virtual MemoryDiagnostic reportMemory() = 0;
    
    /**
     * Report wiring issue detection
     * @param expectedPin Expected pin number
     * @param actualState Actual pin state
     * @param suggestion Suggested fix
     */
    virtual void reportWiringIssue(uint8_t expectedPin, uint8_t actualState, 
                                   const char* suggestion) = 0;
    
    /**
     * Report SPI communication diagnostics
     * @param busName SPI bus identifier
     * @param success Successful transfers
     * @param failures Failed transfers
     * @param avgTimeUs Average transfer time
     */
    virtual void reportSPIDiagnostics(const char* busName, uint32_t success, 
                                      uint32_t failures, uint32_t avgTimeUs) = 0;
    
    /**
     * Dump complete system state
     * @param includeHistory Include error history
     */
    virtual void dumpSystemState(bool includeHistory = true) = 0;
    
    /**
     * Clear diagnostic history
     */
    virtual void clearHistory() = 0;
    
    /**
     * Get diagnostic history
     * @param buffer Buffer to fill with messages
     * @param maxCount Maximum messages to retrieve
     * @return Number of messages retrieved
     */
    virtual size_t getHistory(DiagMessage* buffer, size_t maxCount) = 0;
};

/**
 * Diagnostic Output Format Contract:
 * 
 * Serial Output Format:
 * [timestamp_ms][LEVEL][CATEGORY/Component]: Message
 * 
 * Example:
 * [1234][ERROR][RFID/MFRC522]: Communication timeout, no response on SPI
 * [1235][INFO][HARDWARE/Detector]: Detected CYD Dual USB model (ST7789)
 * 
 * Pin State Format:
 * PIN[number] MODE[mode] STATE[state] EXPECT[expected] DESC[description]
 * 
 * Wiring Issue Format:
 * WIRING_ERROR: Pin GPIO[n] expected [state] but got [state]
 * SUGGESTION: [specific action to fix]
 * 
 * Requirements:
 * - All output at 115200 baud
 * - Timestamps in milliseconds since boot
 * - Messages must be parseable by automated tools
 * - Critical errors must be visually distinct
 * - Memory reports every 30 seconds in verbose mode
 */

#endif // DIAGNOSTIC_REPORTING_H