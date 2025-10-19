/**
 * DiagnosticReporter.cpp - Comprehensive diagnostic reporting implementation
 * 
 * Provides structured logging, memory monitoring, pin state analysis,
 * and system health tracking for CYD devices.
 */

#include "DiagnosticReporter.h"
#include <stdarg.h>

DiagnosticReporter::DiagnosticReporter() : minLevel(DIAG_INFO), spiCount(0) {
    // Initialize history
    history.count = 0;
    history.writeIndex = 0;
    history.bufferFull = false;
    history.totalMessages = 0;
    
    // Initialize system health
    health.bootTime = millis();
    health.uptime = 0;
    health.totalResets = 0;
    health.watchdogResets = 0;
    health.averageLoopTime = 0;
    health.maxLoopTime = 0;
    health.criticalErrors = 0;
    health.warnings = 0;
    health.isStable = true;
    
    // Initialize memory tracking
    lastMemoryCheck = 0;
    memset(&lastMemoryState, 0, sizeof(lastMemoryState));
    
    // Initialize performance tracking
    lastLoopTime = 0;
    loopCounter = 0;
    performanceStartTime = millis();
    
    // Clear SPI stats
    for (size_t i = 0; i < 4; i++) {
        memset(&spiStats[i], 0, sizeof(SPIDiagnostic));
    }
}

void DiagnosticReporter::setDiagnosticLevel(DiagLevel level) {
    minLevel = level;
    reportf(DIAG_INFO, CAT_HARDWARE, "Diagnostics", 
            "Diagnostic level set to %s", levelToString(level));
}

void DiagnosticReporter::report(const DiagMessage& message) {
    if (message.level < minLevel) {
        return; // Filter out messages below threshold
    }
    
    // Add to history
    addToHistory(message);
    
    // Update system health metrics
    if (message.level >= DIAG_ERROR) {
        health.criticalErrors++;
        health.isStable = false;
    } else if (message.level >= DIAG_WARNING) {
        health.warnings++;
    }
    
    // Format and output message
    char buffer[DiagConfig::MAX_MESSAGE_LENGTH];
    formatMessage(message, buffer, sizeof(buffer));
    Serial.println(buffer);
}

void DiagnosticReporter::reportf(DiagLevel level, DiagCategory category, 
                                const char* component, const char* format, ...) {
    if (level < minLevel) {
        return;
    }
    
    DiagMessage message;
    message.timestamp = millis();
    message.level = level;
    message.category = category;
    
    // Copy component name
    strncpy(message.component, component, sizeof(message.component) - 1);
    message.component[sizeof(message.component) - 1] = '\0';
    
    // Format message using variadic arguments
    va_list args;
    va_start(args, format);
    vsnprintf(message.message, sizeof(message.message), format, args);
    va_end(args);
    
    // Clear extended data
    memset(&message.extended, 0, sizeof(message.extended));
    
    report(message);
}

void DiagnosticReporter::addToHistory(const DiagMessage& message) {
    history.messages[history.writeIndex] = message;
    history.writeIndex = (history.writeIndex + 1) % DiagnosticHistory::MAX_MESSAGES;
    
    if (!history.bufferFull) {
        history.count++;
        if (history.count >= DiagnosticHistory::MAX_MESSAGES) {
            history.bufferFull = true;
        }
    }
    
    history.totalMessages++;
}

const char* DiagnosticReporter::levelToString(DiagLevel level) const {
    switch (level) {
        case DIAG_DEBUG: return "DEBUG";
        case DIAG_INFO: return "INFO";
        case DIAG_WARNING: return "WARNING";
        case DIAG_ERROR: return "ERROR";
        case DIAG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* DiagnosticReporter::categoryToString(DiagCategory category) const {
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

void DiagnosticReporter::formatMessage(const DiagMessage& message, char* buffer, size_t bufferSize) const {
    snprintf(buffer, bufferSize, "[%lu][%s][%s/%s]: %s",
             message.timestamp,
             levelToString(message.level),
             categoryToString(message.category),
             message.component,
             message.message);
}

void DiagnosticReporter::reportPinStates(const PinDiagnostic* pins, size_t count) {
    reportf(DIAG_INFO, CAT_HARDWARE, "PinDiag", "Reporting %zu pin states", count);
    
    for (size_t i = 0; i < count; i++) {
        const PinDiagnostic& pin = pins[i];
        
        Serial.print(F("PIN["));
        Serial.print(pin.pin);
        Serial.print(F("] MODE["));
        Serial.print(pinModeToString(pin.mode));
        Serial.print(F("] STATE["));
        Serial.print(pinStateToString(pin.state));
        Serial.print(F("] EXPECT["));
        Serial.print(pin.isExpected ? F("OK") : F("ERROR"));
        Serial.print(F("] DESC["));
        Serial.print(pin.description);
        Serial.println(F("]"));
        
        if (!pin.isExpected) {
            reportf(DIAG_WARNING, CAT_HARDWARE, "PinDiag",
                    "Pin %d unexpected state: %s", pin.pin, pin.description);
        }
    }
}

const char* DiagnosticReporter::pinModeToString(uint8_t mode) const {
    switch (mode) {
        case INPUT: return "INPUT";
        case OUTPUT: return "OUTPUT";
        case INPUT_PULLUP: return "INPUT_PULLUP";
        default: return "UNKNOWN";
    }
}

const char* DiagnosticReporter::pinStateToString(uint8_t state) const {
    return (state == HIGH) ? "HIGH" : "LOW";
}

MemoryDiagnostic DiagnosticReporter::reportMemory() {
    MemoryDiagnostic memDiag;
    
    // Get ESP32 memory information
    memDiag.freeHeap = ESP.getFreeHeap();
    memDiag.totalHeap = ESP.getHeapSize();
    memDiag.largestFreeBlock = ESP.getMaxAllocHeap();
    memDiag.minimumEverFree = ESP.getMinFreeHeap();
    
    // Calculate fragmentation percentage
    analyzeMemoryFragmentation(memDiag);
    
    // Report memory state
    reportf(DIAG_INFO, CAT_MEMORY, "Monitor",
            "Free: %lu/%lu bytes, Largest: %lu, MinEver: %lu, Frag: %.1f%%",
            memDiag.freeHeap, memDiag.totalHeap, memDiag.largestFreeBlock,
            memDiag.minimumEverFree, memDiag.fragmentationPercent);
    
    // Check for memory issues
    if (memDiag.freeHeap < DiagConfig::CRITICAL_MEMORY_THRESHOLD) {
        reportf(DIAG_CRITICAL, CAT_MEMORY, "Monitor",
                "Critical low memory: %lu bytes remaining", memDiag.freeHeap);
    } else if (memDiag.freeHeap < DiagConfig::LOW_MEMORY_WARNING) {
        reportf(DIAG_WARNING, CAT_MEMORY, "Monitor",
                "Low memory warning: %lu bytes remaining", memDiag.freeHeap);
    }
    
    if (memDiag.fragmentationPercent > DiagConfig::HIGH_FRAGMENTATION_THRESHOLD) {
        reportf(DIAG_WARNING, CAT_MEMORY, "Monitor",
                "High memory fragmentation: %.1f%%", memDiag.fragmentationPercent);
    }
    
    lastMemoryState = memDiag;
    lastMemoryCheck = millis();
    
    return memDiag;
}

void DiagnosticReporter::analyzeMemoryFragmentation(MemoryDiagnostic& memDiag) {
    if (memDiag.freeHeap > 0) {
        float fragmentation = 100.0f * (1.0f - ((float)memDiag.largestFreeBlock / (float)memDiag.freeHeap));
        memDiag.fragmentationPercent = fragmentation;
    } else {
        memDiag.fragmentationPercent = 100.0f;
    }
}

void DiagnosticReporter::reportWiringIssue(uint8_t expectedPin, uint8_t actualState, 
                                          const char* suggestion) {
    Serial.print(F("WIRING_ERROR: Pin GPIO"));
    Serial.print(expectedPin);
    Serial.print(F(" expected different state but got "));
    Serial.println(pinStateToString(actualState));
    
    Serial.print(F("SUGGESTION: "));
    Serial.println(suggestion);
    
    reportf(DIAG_ERROR, CAT_HARDWARE, "Wiring",
            "GPIO%d wiring issue - %s", expectedPin, suggestion);
}

void DiagnosticReporter::reportSPIDiagnostics(const char* busName, uint32_t success, 
                                             uint32_t failures, uint32_t avgTimeUs) {
    SPIDiagnostic* stats = findOrCreateSPIStats(busName);
    
    if (stats) {
        stats->successfulTransfers += success;
        stats->failedTransfers += failures;
        stats->totalTransfers = stats->successfulTransfers + stats->failedTransfers;
        stats->avgTransferTimeUs = avgTimeUs;
        stats->lastActivity = millis();
        
        // Update min/max times
        if (avgTimeUs > stats->maxTransferTimeUs) {
            stats->maxTransferTimeUs = avgTimeUs;
        }
        if (avgTimeUs < stats->minTransferTimeUs || stats->minTransferTimeUs == 0) {
            stats->minTransferTimeUs = avgTimeUs;
        }
    }
    
    float successRate = 0;
    if (success + failures > 0) {
        successRate = 100.0f * success / (success + failures);
    }
    
    reportf(DIAG_INFO, CAT_HARDWARE, "SPI",
            "%s: Success=%lu, Failed=%lu, Rate=%.1f%%, AvgTime=%luus",
            busName, success, failures, successRate, avgTimeUs);
    
    if (failures > 0) {
        reportf(DIAG_WARNING, CAT_HARDWARE, "SPI",
                "%s has %lu failed transfers", busName, failures);
    }
}

SPIDiagnostic* DiagnosticReporter::findOrCreateSPIStats(const char* busName) {
    // Find existing stats
    for (size_t i = 0; i < spiCount; i++) {
        if (strncmp(spiStats[i].busName, busName, sizeof(spiStats[i].busName)) == 0) {
            return &spiStats[i];
        }
    }
    
    // Create new stats entry
    if (spiCount < 4) {
        strncpy(spiStats[spiCount].busName, busName, sizeof(spiStats[spiCount].busName) - 1);
        spiStats[spiCount].busName[sizeof(spiStats[spiCount].busName) - 1] = '\0';
        return &spiStats[spiCount++];
    }
    
    return nullptr; // No more slots available
}

void DiagnosticReporter::dumpSystemState(bool includeHistory) {
    Serial.println(F("=== SYSTEM STATE DUMP ==="));
    
    // System health
    updateSystemHealth();
    Serial.print(F("Boot time: "));
    Serial.print(health.bootTime);
    Serial.println(F("ms"));
    
    Serial.print(F("Uptime: "));
    Serial.print(health.uptime);
    Serial.println(F("ms"));
    
    Serial.print(F("System stable: "));
    Serial.println(health.isStable ? F("YES") : F("NO"));
    
    Serial.print(F("Critical errors: "));
    Serial.println(health.criticalErrors);
    
    Serial.print(F("Warnings: "));
    Serial.println(health.warnings);
    
    // Memory state
    MemoryDiagnostic memDiag = reportMemory();
    
    // SPI diagnostics
    Serial.println(F("\n--- SPI Bus Status ---"));
    for (size_t i = 0; i < spiCount; i++) {
        const SPIDiagnostic& spi = spiStats[i];
        Serial.print(spi.busName);
        Serial.print(F(": Total="));
        Serial.print(spi.totalTransfers);
        Serial.print(F(", Success="));
        Serial.print(spi.successfulTransfers);
        Serial.print(F(", Failed="));
        Serial.print(spi.failedTransfers);
        Serial.print(F(", AvgTime="));
        Serial.print(spi.avgTransferTimeUs);
        Serial.println(F("us"));
    }
    
    // Critical pin states
    Serial.println(F("\n--- Critical Pin Status ---"));
    checkCriticalPins();
    
    // Performance metrics
    Serial.println(F("\n--- Performance Metrics ---"));
    Serial.print(F("Average loop time: "));
    Serial.print(health.averageLoopTime);
    Serial.println(F("us"));
    
    Serial.print(F("Max loop time: "));
    Serial.print(health.maxLoopTime);
    Serial.println(F("us"));
    
    // History
    if (includeHistory) {
        Serial.println(F("\n--- Recent Messages ---"));
        size_t count = history.bufferFull ? DiagnosticHistory::MAX_MESSAGES : history.count;
        size_t startIndex = history.bufferFull ? history.writeIndex : 0;
        
        for (size_t i = 0; i < count; i++) {
            size_t index = (startIndex + i) % DiagnosticHistory::MAX_MESSAGES;
            const DiagMessage& msg = history.messages[index];
            
            char buffer[DiagConfig::MAX_MESSAGE_LENGTH];
            formatMessage(msg, buffer, sizeof(buffer));
            Serial.println(buffer);
        }
    }
    
    Serial.println(F("=== END SYSTEM STATE ==="));
}

void DiagnosticReporter::clearHistory() {
    history.count = 0;
    history.writeIndex = 0;
    history.bufferFull = false;
    // Don't reset totalMessages - keep lifetime counter
    
    reportf(DIAG_INFO, CAT_HARDWARE, "Diagnostics", "Message history cleared");
}

size_t DiagnosticReporter::getHistory(DiagMessage* buffer, size_t maxCount) {
    if (!buffer || maxCount == 0) {
        return 0;
    }
    
    size_t available = history.bufferFull ? DiagnosticHistory::MAX_MESSAGES : history.count;
    size_t toCopy = (maxCount < available) ? maxCount : available;
    
    size_t startIndex = history.bufferFull ? history.writeIndex : 0;
    
    for (size_t i = 0; i < toCopy; i++) {
        size_t index = (startIndex + i) % DiagnosticHistory::MAX_MESSAGES;
        buffer[i] = history.messages[index];
    }
    
    return toCopy;
}

void DiagnosticReporter::updateSystemHealth() {
    health.uptime = millis() - health.bootTime;
    
    // Update average loop time
    if (loopCounter > 0) {
        uint32_t totalTime = millis() - performanceStartTime;
        health.averageLoopTime = (totalTime * 1000) / loopCounter; // Convert to microseconds
    }
    
    // Check system stability
    health.isStable = (health.criticalErrors == 0) && 
                     (health.maxLoopTime < DiagConfig::CRITICAL_LOOP_TIME_US);
}

void DiagnosticReporter::updateLoopTime(uint32_t loopTimeUs) {
    lastLoopTime = loopTimeUs;
    loopCounter++;
    
    if (loopTimeUs > health.maxLoopTime) {
        health.maxLoopTime = loopTimeUs;
    }
    
    if (loopTimeUs > DiagConfig::SLOW_LOOP_WARNING_US) {
        reportf(DIAG_WARNING, CAT_PERFORMANCE, "Loop",
                "Slow loop detected: %lu us", loopTimeUs);
    }
    
    if (loopTimeUs > DiagConfig::CRITICAL_LOOP_TIME_US) {
        reportf(DIAG_CRITICAL, CAT_PERFORMANCE, "Loop",
                "Critical slow loop: %lu us", loopTimeUs);
        health.isStable = false;
    }
}

void DiagnosticReporter::checkCriticalPins() {
    PinDiagnostic pins[] = {
        {DiagConfig::DISPLAY_CS, OUTPUT, digitalRead(DiagConfig::DISPLAY_CS), true, "Display CS"},
        {DiagConfig::DISPLAY_DC, OUTPUT, digitalRead(DiagConfig::DISPLAY_DC), true, "Display DC"},
        {DiagConfig::BACKLIGHT_21, OUTPUT, digitalRead(DiagConfig::BACKLIGHT_21), true, "Backlight 21"},
        {DiagConfig::SD_CS, OUTPUT, digitalRead(DiagConfig::SD_CS), true, "SD CS"},
        {DiagConfig::TOUCH_CS, OUTPUT, digitalRead(DiagConfig::TOUCH_CS), true, "Touch CS"},
        {DiagConfig::TOUCH_IRQ, INPUT_PULLUP, digitalRead(DiagConfig::TOUCH_IRQ), true, "Touch IRQ"}
    };
    
    // Validate pin configurations
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        pins[i].isExpected = isPinConfigurationValid(pins[i].pin, pins[i].mode);
    }
    
    reportPinStates(pins, sizeof(pins) / sizeof(pins[0]));
}

bool DiagnosticReporter::isPinConfigurationValid(uint8_t pin, uint8_t expectedMode) {
    // This is a simplified check - actual implementation would need 
    // to read the actual GPIO configuration registers
    return true; // Placeholder
}

void DiagnosticReporter::reportSystemBoot() {
    health.bootTime = millis();
    health.totalResets++;
    
    reportf(DIAG_INFO, CAT_HARDWARE, "System", "System boot complete, reset count: %lu", 
            health.totalResets);
}

void DiagnosticReporter::reportSystemReset(const char* reason) {
    reportf(DIAG_WARNING, CAT_HARDWARE, "System", "System reset: %s", reason);
    health.totalResets++;
}

void DiagnosticReporter::reportCriticalError(const char* component, const char* error) {
    reportf(DIAG_CRITICAL, CAT_HARDWARE, component, "CRITICAL: %s", error);
    health.criticalErrors++;
    health.isStable = false;
}