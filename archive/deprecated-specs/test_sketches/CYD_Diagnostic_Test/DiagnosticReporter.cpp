/**
 * DiagnosticReporter Implementation
 * 
 * Concrete implementation providing structured diagnostic output
 * with severity levels, categories, and formatted serial output.
 */

#include "DiagnosticReporter.h"
#include <stdarg.h>

DiagnosticReporter::DiagnosticReporter(DiagLevel initialLevel, bool verbose)
    : currentLevel(initialLevel)
    , historyIndex(0)
    , historyCount(0)
    , lastMemoryReport(0)
    , verboseMode(verbose) {
}

void DiagnosticReporter::begin() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    
    reportf(DIAG_INFO, CAT_HARDWARE, "DiagReporter", 
            "Diagnostic Reporter initialized at 115200 baud");
    reportf(DIAG_INFO, CAT_HARDWARE, "DiagReporter", 
            "Level: %s, Verbose: %s", 
            levelToString(currentLevel), 
            verboseMode ? "ON" : "OFF");
}

void DiagnosticReporter::update() {
    if (verboseMode && (millis() - lastMemoryReport) >= MEMORY_REPORT_INTERVAL_MS) {
        reportMemory();
        lastMemoryReport = millis();
    }
}

void DiagnosticReporter::setDiagnosticLevel(DiagLevel level) {
    currentLevel = level;
    reportf(DIAG_INFO, CAT_HARDWARE, "DiagReporter", 
            "Diagnostic level set to %s", levelToString(level));
}

void DiagnosticReporter::report(const DiagMessage& message) {
    if (message.level >= currentLevel) {
        printFormattedMessage(message);
        addToHistory(message);
    }
}

void DiagnosticReporter::reportf(DiagLevel level, DiagCategory category, 
                                 const char* component, const char* format, ...) {
    if (level < currentLevel) {
        return;
    }
    
    DiagMessage message;
    message.timestamp = getCurrentTimestamp();
    message.level = level;
    message.category = category;
    
    // Copy component name safely
    strncpy(message.component, component, sizeof(message.component) - 1);
    message.component[sizeof(message.component) - 1] = '\0';
    
    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(message.message, sizeof(message.message), format, args);
    va_end(args);
    
    // Clear extended data
    memset(&message.extended, 0, sizeof(message.extended));
    
    report(message);
}

void DiagnosticReporter::reportPinStates(const PinDiagnostic* pins, size_t count) {
    reportf(DIAG_INFO, CAT_HARDWARE, "PinDiag", "=== PIN STATE REPORT ===");
    
    for (size_t i = 0; i < count; i++) {
        const PinDiagnostic& pin = pins[i];
        
        Serial.print("PIN[");
        Serial.print(pin.pin);
        Serial.print("] MODE[");
        Serial.print(pinModeToString(pin.mode));
        Serial.print("] STATE[");
        Serial.print(pin.state == HIGH ? "HIGH" : "LOW");
        Serial.print("] EXPECT[");
        Serial.print(pin.isExpected ? "OK" : "FAIL");
        Serial.print("] DESC[");
        Serial.print(pin.description);
        Serial.println("]");
        
        if (!pin.isExpected) {
            reportf(DIAG_WARNING, CAT_HARDWARE, "PinDiag", 
                    "Pin %d unexpected state: %s", 
                    pin.pin, pin.state == HIGH ? "HIGH" : "LOW");
        }
    }
}

MemoryDiagnostic DiagnosticReporter::reportMemory() {
    MemoryDiagnostic memDiag;
    
    // Get ESP32 memory information
    memDiag.totalHeap = ESP.getHeapSize();
    memDiag.freeHeap = ESP.getFreeHeap();
    memDiag.largestFreeBlock = ESP.getMaxAllocHeap();
    memDiag.minimumEverFree = ESP.getMinFreeHeap();
    
    // Calculate fragmentation percentage
    if (memDiag.freeHeap > 0) {
        memDiag.fragmentationPercent = 
            100.0f * (1.0f - (float)memDiag.largestFreeBlock / (float)memDiag.freeHeap);
    } else {
        memDiag.fragmentationPercent = 100.0f;
    }
    
    reportf(DIAG_INFO, CAT_MEMORY, "MemReport", 
            "Heap: %lu/%lu bytes (%.1f%% used)", 
            memDiag.totalHeap - memDiag.freeHeap, 
            memDiag.totalHeap,
            100.0f * (memDiag.totalHeap - memDiag.freeHeap) / memDiag.totalHeap);
    
    reportf(DIAG_INFO, CAT_MEMORY, "MemReport", 
            "Largest block: %lu bytes, Min free: %lu bytes", 
            memDiag.largestFreeBlock, memDiag.minimumEverFree);
    
    reportf(DIAG_INFO, CAT_MEMORY, "MemReport", 
            "Fragmentation: %.1f%%", memDiag.fragmentationPercent);
    
    // Warning thresholds
    if (memDiag.freeHeap < 10000) {
        reportf(DIAG_WARNING, CAT_MEMORY, "MemReport", 
                "Low memory warning: only %lu bytes free", memDiag.freeHeap);
    }
    
    if (memDiag.fragmentationPercent > 50.0f) {
        reportf(DIAG_WARNING, CAT_MEMORY, "MemReport", 
                "High fragmentation: %.1f%%", memDiag.fragmentationPercent);
    }
    
    return memDiag;
}

void DiagnosticReporter::reportWiringIssue(uint8_t expectedPin, uint8_t actualState, 
                                           const char* suggestion) {
    Serial.println("WIRING_ERROR: Pin GPIO[" + String(expectedPin) + 
                   "] expected " + (actualState == HIGH ? "LOW" : "HIGH") + 
                   " but got " + (actualState == HIGH ? "HIGH" : "LOW"));
    Serial.println("SUGGESTION: " + String(suggestion));
    
    reportf(DIAG_ERROR, CAT_HARDWARE, "WiringCheck", 
            "GPIO%d wiring issue detected", expectedPin);
}

void DiagnosticReporter::reportSPIDiagnostics(const char* busName, uint32_t success, 
                                              uint32_t failures, uint32_t avgTimeUs) {
    float successRate = 0.0f;
    uint32_t totalTransfers = success + failures;
    
    if (totalTransfers > 0) {
        successRate = 100.0f * success / totalTransfers;
    }
    
    reportf(DIAG_INFO, CAT_HARDWARE, "SPIDiag", 
            "%s: %lu success, %lu failures (%.1f%% success rate)", 
            busName, success, failures, successRate);
    
    reportf(DIAG_INFO, CAT_PERFORMANCE, "SPIDiag", 
            "%s: Average transfer time %lu µs", busName, avgTimeUs);
    
    if (successRate < 95.0f && totalTransfers > 10) {
        reportf(DIAG_WARNING, CAT_HARDWARE, "SPIDiag", 
                "%s: Low success rate %.1f%%", busName, successRate);
    }
    
    if (avgTimeUs > 1000) {
        reportf(DIAG_WARNING, CAT_PERFORMANCE, "SPIDiag", 
                "%s: Slow transfers averaging %lu µs", busName, avgTimeUs);
    }
}

void DiagnosticReporter::dumpSystemState(bool includeHistory) {
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", "=== SYSTEM STATE DUMP ===");
    
    // Basic system info
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", 
            "Uptime: %lu ms", millis());
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", 
            "Free heap: %lu bytes", ESP.getFreeHeap());
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", 
            "CPU frequency: %lu MHz", ESP.getCpuFreqMHz());
    
    // Current diagnostic settings
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", 
            "Diagnostic level: %s", levelToString(currentLevel));
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", 
            "Verbose mode: %s", verboseMode ? "ON" : "OFF");
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", 
            "History entries: %lu/%d", historyCount, DIAG_HISTORY_SIZE);
    
    // Memory report
    reportMemory();
    
    // History dump if requested
    if (includeHistory && historyCount > 0) {
        reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", "=== DIAGNOSTIC HISTORY ===");
        
        size_t startIdx = (historyCount < DIAG_HISTORY_SIZE) ? 0 : historyIndex;
        size_t count = min(historyCount, (size_t)DIAG_HISTORY_SIZE);
        
        for (size_t i = 0; i < count; i++) {
            size_t idx = (startIdx + i) % DIAG_HISTORY_SIZE;
            printFormattedMessage(history[idx]);
        }
    }
    
    reportf(DIAG_INFO, CAT_HARDWARE, "SystemDump", "=== END SYSTEM DUMP ===");
}

void DiagnosticReporter::clearHistory() {
    historyIndex = 0;
    historyCount = 0;
    reportf(DIAG_INFO, CAT_HARDWARE, "DiagReporter", "Diagnostic history cleared");
}

size_t DiagnosticReporter::getHistory(DiagMessage* buffer, size_t maxCount) {
    if (!buffer || maxCount == 0 || historyCount == 0) {
        return 0;
    }
    
    size_t copyCount = min(maxCount, historyCount);
    size_t startIdx = (historyCount < DIAG_HISTORY_SIZE) ? 0 : historyIndex;
    
    for (size_t i = 0; i < copyCount; i++) {
        size_t idx = (startIdx + i) % DIAG_HISTORY_SIZE;
        buffer[i] = history[idx];
    }
    
    return copyCount;
}

void DiagnosticReporter::setVerboseMode(bool verbose) {
    verboseMode = verbose;
    reportf(DIAG_INFO, CAT_HARDWARE, "DiagReporter", 
            "Verbose mode %s", verbose ? "enabled" : "disabled");
}

bool DiagnosticReporter::isVerboseMode() const {
    return verboseMode;
}

DiagLevel DiagnosticReporter::getCurrentLevel() const {
    return currentLevel;
}

size_t DiagnosticReporter::getHistoryCount() const {
    return historyCount;
}

// Private helper methods

const char* DiagnosticReporter::levelToString(DiagLevel level) {
    switch (level) {
        case DIAG_DEBUG: return "DEBUG";
        case DIAG_INFO: return "INFO";
        case DIAG_WARNING: return "WARNING";
        case DIAG_ERROR: return "ERROR";
        case DIAG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* DiagnosticReporter::categoryToString(DiagCategory category) {
    switch (category) {
        case CAT_HARDWARE: return "HARDWARE";
        case CAT_DISPLAY: return "DISPLAY";
        case CAT_TOUCH: return "TOUCH";
        case CAT_RFID: return "RFID";
        case CAT_SDCARD: return "SDCARD";
        case CAT_AUDIO: return "AUDIO";
        case CAT_MEMORY: return "MEMORY";
        case CAT_PERFORMANCE: return "PERFORMANCE";
        default: return "UNKNOWN";
    }
}

const char* DiagnosticReporter::pinModeToString(uint8_t mode) {
    switch (mode) {
        case INPUT: return "INPUT";
        case OUTPUT: return "OUTPUT";
        case INPUT_PULLUP: return "INPUT_PULLUP";
        default: return "UNKNOWN";
    }
}

void DiagnosticReporter::addToHistory(const DiagMessage& message) {
    history[historyIndex] = message;
    historyIndex = (historyIndex + 1) % DIAG_HISTORY_SIZE;
    
    if (historyCount < DIAG_HISTORY_SIZE) {
        historyCount++;
    }
}

void DiagnosticReporter::printFormattedMessage(const DiagMessage& message) {
    // Critical errors get special treatment
    if (message.level == DIAG_CRITICAL) {
        printCriticalBorder();
    }
    
    // Format: [timestamp_ms][LEVEL][CATEGORY/Component]: Message
    Serial.print("[");
    Serial.print(message.timestamp);
    Serial.print("][");
    Serial.print(levelToString(message.level));
    Serial.print("][");
    Serial.print(categoryToString(message.category));
    Serial.print("/");
    Serial.print(message.component);
    Serial.print("]: ");
    Serial.println(message.message);
    
    if (message.level == DIAG_CRITICAL) {
        printCriticalBorder();
    }
}

void DiagnosticReporter::printCriticalBorder() {
    Serial.println("*** CRITICAL ERROR ***");
}

uint32_t DiagnosticReporter::getCurrentTimestamp() {
    return millis();
}