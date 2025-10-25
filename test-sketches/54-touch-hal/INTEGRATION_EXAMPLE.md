# TouchDriver HAL - Integration Example

## Basic Usage Pattern

```cpp
#include "config.h"
#include "hal/TouchDriver.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);

    // Initialize touch driver
    auto& touch = hal::TouchDriver::getInstance();
    if (!touch.begin()) {
        Serial.println("Touch driver init failed!");
        while(1);
    }

    // Connect WiFi (generates EMI)
    WiFi.begin("SSID", "password");
}

void loop() {
    auto& touch = hal::TouchDriver::getInstance();

    // Check for touch interrupt
    if (touch.isTouched()) {
        // Validate touch (WiFi EMI filtering)
        if (touch.isValidTouch()) {
            // Real touch detected
            uint32_t pulseWidth = touch.measurePulseWidth();
            Serial.printf("Valid touch: %lu us\n", pulseWidth);

            // Handle touch event here
            handleTouchEvent();
        }
        // else: WiFi EMI filtered automatically

        // Clear interrupt flag
        touch.clearTouch();
    }

    delay(10);
}
```

## Debouncing Pattern

```cpp
// Add debouncing for UI interactions
uint32_t lastTouchTime = 0;
const uint32_t DEBOUNCE_MS = 50;

void loop() {
    auto& touch = hal::TouchDriver::getInstance();

    if (touch.isTouched() && touch.isValidTouch()) {
        uint32_t now = millis();

        // Debounce check
        if (now - lastTouchTime > DEBOUNCE_MS) {
            lastTouchTime = now;

            // Handle debounced touch
            onTouchEvent();
        }

        touch.clearTouch();
    }
}
```

## Double-Tap Detection Pattern

```cpp
// From v4.1 orchestrator
uint32_t lastTouchTime = 0;
bool lastTouchWasValid = false;
const uint32_t DOUBLE_TAP_TIMEOUT = 500; // ms
const uint32_t DEBOUNCE_MS = 50;

void loop() {
    auto& touch = hal::TouchDriver::getInstance();
    uint32_t now = millis();

    if (touch.isTouched() && touch.isValidTouch()) {
        // Debounce
        if (now - lastTouchTime < DEBOUNCE_MS) {
            touch.clearTouch();
            return;
        }

        // Check for double-tap
        if (lastTouchWasValid && (now - lastTouchTime) < DOUBLE_TAP_TIMEOUT) {
            // Double-tap detected
            Serial.println("Double-tap!");
            onDoubleTap();
            lastTouchWasValid = false;
        } else {
            // First tap
            Serial.println("Single tap");
            onSingleTap();
            lastTouchWasValid = true;
        }

        lastTouchTime = now;
        touch.clearTouch();
    }

    // Clear single-tap flag after timeout
    if (lastTouchWasValid && (now - lastTouchTime) >= DOUBLE_TAP_TIMEOUT) {
        lastTouchWasValid = false;
    }
}
```

## Statistics Tracking Pattern

```cpp
struct TouchStats {
    uint32_t totalInterrupts = 0;
    uint32_t validTouches = 0;
    uint32_t emiFiltered = 0;
} stats;

void loop() {
    auto& touch = hal::TouchDriver::getInstance();

    if (touch.isTouched()) {
        stats.totalInterrupts++;

        if (touch.isValidTouch()) {
            stats.validTouches++;
            handleTouch();
        } else {
            stats.emiFiltered++;
        }

        touch.clearTouch();
    }
}

void printStats() {
    Serial.printf("Total: %lu, Valid: %lu, EMI: %lu\n",
                  stats.totalInterrupts, stats.validTouches, stats.emiFiltered);

    if (stats.totalInterrupts > 0) {
        float filterRate = 100.0 * stats.emiFiltered / stats.totalInterrupts;
        Serial.printf("EMI filter rate: %.1f%%\n", filterRate);
    }
}
```

## State Machine Integration

```cpp
enum UIState {
    STATE_IDLE,
    STATE_MENU,
    STATE_PROCESSING
};

UIState currentState = STATE_IDLE;

void loop() {
    auto& touch = hal::TouchDriver::getInstance();

    if (touch.isTouched() && touch.isValidTouch()) {
        switch (currentState) {
            case STATE_IDLE:
                // Touch in idle state shows menu
                currentState = STATE_MENU;
                showMenu();
                break;

            case STATE_MENU:
                // Touch in menu dismisses it
                currentState = STATE_IDLE;
                hideMenu();
                break;

            case STATE_PROCESSING:
                // Touches ignored during processing
                Serial.println("Processing, touch ignored");
                break;
        }

        touch.clearTouch();
    }
}
```

## Error Handling

```cpp
void setup() {
    auto& touch = hal::TouchDriver::getInstance();

    if (!touch.begin()) {
        LOG_ERROR("TouchDriver", "Failed to initialize");

        // Fallback: polling mode without interrupts
        pinMode(pins::TOUCH_IRQ, INPUT);
        Serial.println("Touch driver running in polling mode");
    }
}

void loop() {
    auto& touch = hal::TouchDriver::getInstance();

    // Graceful degradation if interrupt not available
    if (touch.isTouched()) {
        if (touch.isValidTouch()) {
            uint32_t pulseWidth = touch.measurePulseWidth();

            // Warn if pulse width suspicious
            if (pulseWidth >= 500000) { // 500ms timeout
                Serial.println("Warning: Touch pulse timeout");
            }

            handleTouch();
        }
        touch.clearTouch();
    }
}
```

## Memory Footprint

### Flash Usage
- TouchDriver.h: ~200 bytes (header-only, inlined)
- ISR function: ~50 bytes (IRAM)
- Test sketch: 894,307 bytes total

### RAM Usage
- Static variables: 5 bytes (bool + uint32_t)
- Stack: Minimal (singleton pattern, no heap allocation)

## Performance Characteristics

### Interrupt Latency
- IRAM placement: <1 microsecond ISR execution
- Timestamp capture: Uses `micros()` for precision

### Pulse Width Measurement
- Typical real touch: 70-200ms
- WiFi EMI: <0.01ms
- Timeout: 500ms (prevents infinite loop)

### Filter Efficiency
- Validated Oct 19, 2025 (test-sketches/45-touch-wifi-emi/)
- WiFi EMI rejection rate: >95%
- False positive rate: <1% (with proper threshold)

## Critical Constraints

### GPIO36 Limitations
```cpp
// ✓ CORRECT
pinMode(pins::TOUCH_IRQ, INPUT);  // GPIO36 is input-only

// ✗ WRONG - Will cause runtime error
pinMode(pins::TOUCH_IRQ, INPUT_PULLUP);  // GPIO36 has no pull-up!
pinMode(pins::TOUCH_IRQ, OUTPUT);        // GPIO36 cannot be output!
```

### ISR Constraints
```cpp
// ✓ CORRECT - Global variables for ISR access
static volatile bool g_touchInterruptOccurred = false;
void IRAM_ATTR touchISR() { g_touchInterruptOccurred = true; }

// ✗ WRONG - Class static members cause linker errors
class Touch {
    static volatile bool _flag;  // Dangerous for IRAM ISR
    static void IRAM_ATTR isr() { _flag = true; }  // Linker error!
};
```

### WiFi Interaction
```cpp
// WiFi MUST be enabled for EMI filtering to be necessary
WiFi.mode(WIFI_STA);
WiFi.begin("SSID", "password");

// Touch driver works with WiFi off, but EMI filtering is unnecessary
// (no false triggers without WiFi EMI)
```

## Reference Implementation

Full implementation in v4.1 orchestrator:
- Touch ISR: lines 1187-1190
- Pulse measurement: lines 1195-1207
- Initialization: lines 2867-2871
- Touch handling: lines 3577-3664

Extracted to TouchDriver.h: October 22, 2025
