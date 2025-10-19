#ifndef TOUCH_CONFIG_H
#define TOUCH_CONFIG_H

#include <Arduino.h>

/**
 * TouchConfig - Touch IRQ tap detection configuration
 * 
 * IMPORTANT: Touch uses IRQ-only detection (GPIO36)
 * No coordinate reading is supported or required.
 * 
 * CRITICAL BEHAVIOR: Each physical tap generates TWO interrupts:
 * - First interrupt: finger touches screen (pin goes LOW)
 * - Second interrupt: ~80-130ms later (release or noise)
 * 
 * Validation Rules:
 * - IRQ pin must be 36 (fixed hardware)
 * - doubleTapWindow typically 300-500ms
 * - debounceMs MUST BE 200ms (to filter out second interrupt)
 */
struct TouchConfig {
  // Touch IRQ pin (hardware fixed)
  static const uint8_t IRQ_PIN = 36;
  
  // Runtime state
  volatile bool tapDetected;      // Set by ISR
  uint32_t lastTapTime;           // For double-tap detection
  uint32_t lastDebounceTime;      // For debouncing
  bool lastTapProcessed;          // To track tap handling
  
  // Configuration
  uint16_t doubleTapWindow;       // Max ms between taps (default 500)
  uint16_t debounceMs;            // Debounce time (default 50)
  bool enabled;                   // Touch detection enabled
  
  // Statistics
  uint32_t totalTaps;             // Total tap count
  uint32_t doubleTaps;            // Double-tap count
  
  // Initialize with defaults
  TouchConfig() : 
    tapDetected(false),
    lastTapTime(0),
    lastDebounceTime(0),
    lastTapProcessed(true),
    doubleTapWindow(500),
    debounceMs(200),  // MUST be 200ms to filter double interrupts
    enabled(false),
    totalTaps(0),
    doubleTaps(0) {}
};

#endif // TOUCH_CONFIG_H