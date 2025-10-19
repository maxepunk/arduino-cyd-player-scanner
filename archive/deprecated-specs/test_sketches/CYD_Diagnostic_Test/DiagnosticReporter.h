/**
 * DiagnosticReporter Implementation
 * 
 * Concrete implementation of the diagnostic reporting contract
 * providing structured diagnostic output with severity levels,
 * categories, and formatted serial output at 115200 baud.
 */

#ifndef DIAGNOSTIC_REPORTER_H
#define DIAGNOSTIC_REPORTER_H

#include <Arduino.h>
#include "../../specs/001-we-have-a/contracts/diagnostic-reporting.h"

// Configuration constants
#define DIAG_HISTORY_SIZE 50
#define DIAG_COMPONENT_MAX_LEN 16
#define DIAG_MESSAGE_MAX_LEN 128
#define MEMORY_REPORT_INTERVAL_MS 30000

/**
 * Concrete implementation of IDiagnosticReporter
 */
class DiagnosticReporter : public IDiagnosticReporter {
private:
    DiagLevel currentLevel;
    DiagMessage history[DIAG_HISTORY_SIZE];
    size_t historyIndex;
    size_t historyCount;
    uint32_t lastMemoryReport;
    bool verboseMode;
    
    // Helper methods
    const char* levelToString(DiagLevel level);
    const char* categoryToString(DiagCategory category);
    const char* pinModeToString(uint8_t mode);
    void addToHistory(const DiagMessage& message);
    void printFormattedMessage(const DiagMessage& message);
    void printCriticalBorder();
    uint32_t getCurrentTimestamp();

public:
    /**
     * Constructor
     * @param initialLevel Starting diagnostic level (default: DIAG_INFO)
     * @param verbose Enable verbose mode with periodic memory reports
     */
    DiagnosticReporter(DiagLevel initialLevel = DIAG_INFO, bool verbose = false);
    
    /**
     * Initialize the diagnostic reporter
     * Sets up serial communication at 115200 baud
     */
    void begin();
    
    /**
     * Update function - call in main loop for periodic reports
     */
    void update();
    
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
    
    // Additional utility methods
    void setVerboseMode(bool verbose);
    bool isVerboseMode() const;
    DiagLevel getCurrentLevel() const;
    size_t getHistoryCount() const;
};

#endif // DIAGNOSTIC_REPORTER_H