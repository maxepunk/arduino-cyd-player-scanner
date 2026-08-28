#pragma once
#include <Arduino.h>
#include "../config.h"

// ═══════════════════════════════════════════════════════════════════════
// ═══ TOUCH DRIVER HAL - WiFi EMI Filtering & Interrupt Management ═════
// ═══════════════════════════════════════════════════════════════════════
//
// Hardware: XPT2046 touch controller on GPIO36 (input-only pin)
//
// CRITICAL PROBLEM: WiFi EMI False Triggers
//   - WiFi radio generates electromagnetic interference (EMI)
//   - EMI triggers spurious interrupts on GPIO36
//   - WiFi EMI pulses: <0.01ms duration
//   - Real touch events: >70ms duration
//
// SOLUTION: Pulse Width Measurement (validated Oct 19, 2025)
//   - Measure how long GPIO36 stays LOW after interrupt
//   - Filter out pulses < 10ms (WiFi EMI)
//   - Accept pulses >= 10ms (real touches)
//
// Reference: test-sketches/45-touch-wifi-emi/
// Extracted from: ALNScanner1021_Orchestrator v4.1 (lines 1187-1207, 2867-2871)
//
// ═══════════════════════════════════════════════════════════════════════

namespace hal {

// Forward declaration for ISR access to static members
class TouchDriver;

// Global ISR state (DRAM placement for ISR access)
static volatile bool g_touchInterruptOccurred = false;
static volatile uint32_t g_touchInterruptTime = 0;

// ISR function (outside class for proper IRAM placement)
void IRAM_ATTR touchISR() {
    g_touchInterruptOccurred = true;
    g_touchInterruptTime = micros();
}

class TouchDriver {
public:
    // Singleton access
    static TouchDriver& getInstance() {
        static TouchDriver instance;
        return instance;
    }

    // Initialization
    bool begin() {
        // CRITICAL: GPIO36 is input-only pin - CANNOT use INPUT_PULLUP!
        pinMode(pins::TOUCH_IRQ, INPUT);

        attachInterrupt(
            digitalPinToInterrupt(pins::TOUCH_IRQ),
            touchISR,
            FALLING
        );

        LOG_INFO("[TOUCH-HAL] Touch interrupt configured (GPIO36 FALLING edge)\n");
        return true;
    }

    // Touch Detection API
    bool isTouched() const {
        return g_touchInterruptOccurred;
    }

    /**
     * @brief Non-blocking test for whether the panel is held right now
     *
     * Reads the IRQ line level directly instead of consuming the interrupt
     * flag. The XPT2046 holds TOUCH_IRQ LOW for the whole time the panel is
     * touched, so this stays true for the duration of a press.
     *
     * measurePulseWidth() cannot serve this purpose: it caps at 500ms and
     * blocks while it measures, so it can neither observe a multi-second
     * hold nor be called from a loop that must keep polling RFID and
     * servicing audio.
     */
    bool isPressed() const {
        return digitalRead(pins::TOUCH_IRQ) == LOW;
    }

    void clearTouch() {
        g_touchInterruptOccurred = false;
    }

    uint32_t getTouchTime() const {
        return g_touchInterruptTime;
    }

    // WiFi EMI Filtering
    uint32_t measurePulseWidth() {
        uint32_t startUs = micros();
        const uint32_t TIMEOUT_US = 500000;  // 500ms max measurement

        // Poll GPIO36 until it goes HIGH or timeout expires
        while (digitalRead(pins::TOUCH_IRQ) == LOW) {
            if (micros() - startUs > TIMEOUT_US) {
                return TIMEOUT_US;  // Still LOW after timeout
            }
        }

        return micros() - startUs;
    }

    bool isValidTouch() {
        if (!g_touchInterruptOccurred) {
            return false;
        }

        // Stabilization delay (let GPIO settle)
        delayMicroseconds(100);

        // Measure pulse width
        uint32_t pulseWidthUs = measurePulseWidth();

        // Check against threshold (10ms = 10000 microseconds)
        return (pulseWidthUs >= timing::TOUCH_PULSE_WIDTH_THRESHOLD_US);
    }

private:
    // Private constructor (singleton pattern)
    TouchDriver() = default;
    ~TouchDriver() = default;

    // Prevent copying
    TouchDriver(const TouchDriver&) = delete;
    TouchDriver& operator=(const TouchDriver&) = delete;
};

} // namespace hal
