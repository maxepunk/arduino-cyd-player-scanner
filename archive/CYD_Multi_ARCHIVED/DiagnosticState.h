#ifndef DIAGNOSTIC_STATE_H
#define DIAGNOSTIC_STATE_H

#include <Arduino.h>

/**
 * DiagnosticState - System diagnostic information and error tracking
 * 
 * Validation Rules:
 * - Component names must be null-terminated
 * - Error messages must be null-terminated
 * - Circular buffer index must wrap at 10
 * - Debug level must be valid enum value
 */
struct DiagnosticState {
  // Component status
  enum ComponentStatus {
    STATUS_UNKNOWN = 0,
    STATUS_OK = 1,
    STATUS_WARNING = 2,
    STATUS_ERROR = 3,
    STATUS_FAILED = 4
  };
  
  struct ComponentHealth {
    ComponentStatus display;
    ComponentStatus touch;
    ComponentStatus rfid;
    ComponentStatus sdcard;
    ComponentStatus audio;
    uint32_t lastChecked;
  };
  ComponentHealth health;
  
  // Error tracking
  struct ErrorInfo {
    uint32_t code;               // Error code
    char component[16];          // Component name
    char message[64];            // Error message
    uint32_t timestamp;          // When it occurred
  };
  ErrorInfo lastErrors[10];      // Circular buffer
  uint8_t errorIndex;
  
  // Performance metrics
  struct Performance {
    uint32_t bootTimeMs;         // Time to initialize
    uint32_t freeHeap;           // Available memory
    uint32_t largestFreeBlock;   // Largest contiguous block
    float cpuUsagePercent;       // CPU utilization
    uint16_t loopTimeMs;         // Main loop duration
  };
  Performance metrics;
  
  // Debug settings
  enum DebugLevel {
    DEBUG_NONE = 0,
    DEBUG_ERROR = 1,
    DEBUG_WARNING = 2,
    DEBUG_INFO = 3,
    DEBUG_VERBOSE = 4
  };
  DebugLevel debugLevel;
};

#endif // DIAGNOSTIC_STATE_H