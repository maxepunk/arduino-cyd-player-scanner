// ═══════════════════════════════════════════════════════════════════════
// ═══ TEST SKETCH: TouchDriver HAL - WiFi EMI Filtering Validation ═════
// ═══════════════════════════════════════════════════════════════════════
//
// Purpose: Validate TouchDriver.h implementation
//
// Test Objectives:
//   1. Touch interrupt fires on GPIO36 FALLING edge
//   2. Pulse width measurement works correctly
//   3. WiFi EMI filtering rejects short pulses (<10ms)
//   4. Real touches accepted (pulse width >=10ms)
//   5. Multiple touches tracked correctly
//
// Hardware Setup:
//   - CYD ESP32-2432S028R
//   - Touch controller: XPT2046 on GPIO36
//   - WiFi enabled (to generate EMI)
//
// Test Procedure:
//   1. Boot device (WiFi connects, generates EMI)
//   2. Observe EMI pulses detected and filtered
//   3. Touch screen physically
//   4. Verify valid touch detected with pulse width >70ms
//   5. Observe EMI continues to be filtered
//
// Serial Commands:
//   - STATS: Show touch statistics
//   - RESET: Reset counters
//
// Expected Results:
//   - WiFi EMI pulses: ~hundreds per minute, all rejected
//   - Real touches: 100% accepted, pulse width 70-200ms
//   - No false positives from WiFi EMI
//
// Reference: ALNScanner1021_Orchestrator v4.1
// Created: October 22, 2025
//
// ═══════════════════════════════════════════════════════════════════════

#include <WiFi.h>

// Include config.h from v5 project
#include "../../ALNScanner_v5/config.h"

// Include TouchDriver HAL
#include "../../ALNScanner_v5/hal/TouchDriver.h"

// ─── Test Configuration ────────────────────────────────────────────────
const char* TEST_SSID = "YourSSID";        // Change this to your network
const char* TEST_PASSWORD = "YourPassword"; // Change this to your password

// ─── Test Statistics ───────────────────────────────────────────────────
struct TouchStats {
    uint32_t totalInterrupts = 0;
    uint32_t validTouches = 0;
    uint32_t emiFiltered = 0;
    uint32_t minPulseWidthUs = 0xFFFFFFFF;
    uint32_t maxPulseWidthUs = 0;
    uint32_t totalPulseWidthUs = 0;
} stats;

// ─── Setup ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("═══════════════════════════════════════════════════════════════");
    Serial.println("═══ TOUCH DRIVER HAL TEST - WiFi EMI Filtering Validation ════");
    Serial.println("═══════════════════════════════════════════════════════════════\n");

    // Initialize TouchDriver
    auto& touch = hal::TouchDriver::getInstance();
    if (touch.begin()) {
        Serial.println("✓ TouchDriver initialized successfully");
    } else {
        Serial.println("✗ TouchDriver initialization FAILED");
        while (1) delay(1000);
    }

    // Connect to WiFi (generates EMI for testing)
    Serial.println("\n[WIFI] Connecting to generate EMI...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(TEST_SSID, TEST_PASSWORD);

    uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected (EMI generation active)");
        Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  Signal: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("\n⚠ WiFi NOT connected (limited EMI testing)");
    }

    Serial.println("\n─────────────────────────────────────────────────────────────");
    Serial.println("TEST READY");
    Serial.println("─────────────────────────────────────────────────────────────");
    Serial.println("• Touch the screen to test valid touch detection");
    Serial.println("• WiFi EMI pulses will be filtered automatically");
    Serial.println("• Commands: STATS, RESET");
    Serial.println("─────────────────────────────────────────────────────────────\n");
}

// ─── Main Loop ─────────────────────────────────────────────────────────
void loop() {
    auto& touch = hal::TouchDriver::getInstance();

    // Check for touch interrupt
    if (touch.isTouched()) {
        stats.totalInterrupts++;

        // Validate touch (WiFi EMI filtering)
        if (touch.isValidTouch()) {
            // Valid touch detected
            uint32_t pulseWidthUs = touch.measurePulseWidth();
            stats.validTouches++;
            stats.totalPulseWidthUs += pulseWidthUs;

            // Update min/max
            if (pulseWidthUs < stats.minPulseWidthUs) {
                stats.minPulseWidthUs = pulseWidthUs;
            }
            if (pulseWidthUs > stats.maxPulseWidthUs) {
                stats.maxPulseWidthUs = pulseWidthUs;
            }

            Serial.printf("[TOUCH] ✓ Valid touch detected\n");
            Serial.printf("        Pulse width: %lu us (%.2f ms)\n",
                          pulseWidthUs, pulseWidthUs / 1000.0);
            Serial.printf("        Timestamp: %lu us\n", touch.getTouchTime());
            Serial.printf("        Total valid: %lu, Total interrupts: %lu\n",
                          stats.validTouches, stats.totalInterrupts);
        } else {
            // WiFi EMI filtered
            stats.emiFiltered++;

            // Only log first 10 EMI events to avoid serial buffer flood
            if (stats.emiFiltered <= 10) {
                Serial.printf("[TOUCH] ✗ WiFi EMI filtered (count: %lu)\n",
                              stats.emiFiltered);
            } else if (stats.emiFiltered % 100 == 0) {
                // Log every 100th EMI event
                Serial.printf("[TOUCH] ✗ WiFi EMI filtered (count: %lu)\n",
                              stats.emiFiltered);
            }
        }

        touch.clearTouch();
    }

    // Check for serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "STATS") {
            printStats();
        } else if (cmd == "RESET") {
            resetStats();
            Serial.println("[CMD] Statistics reset");
        } else {
            Serial.println("[CMD] Unknown command. Available: STATS, RESET");
        }
    }

    delay(10);
}

// ─── Print Statistics ──────────────────────────────────────────────────
void printStats() {
    Serial.println("\n═══════════════════════════════════════════════════════════════");
    Serial.println("═══ TOUCH DRIVER STATISTICS ═══════════════════════════════════");
    Serial.println("═══════════════════════════════════════════════════════════════");

    Serial.printf("Total Interrupts:    %lu\n", stats.totalInterrupts);
    Serial.printf("Valid Touches:       %lu\n", stats.validTouches);
    Serial.printf("WiFi EMI Filtered:   %lu\n", stats.emiFiltered);

    if (stats.validTouches > 0) {
        Serial.printf("\nPulse Width Statistics:\n");
        Serial.printf("  Min:               %lu us (%.2f ms)\n",
                      stats.minPulseWidthUs, stats.minPulseWidthUs / 1000.0);
        Serial.printf("  Max:               %lu us (%.2f ms)\n",
                      stats.maxPulseWidthUs, stats.maxPulseWidthUs / 1000.0);
        Serial.printf("  Average:           %lu us (%.2f ms)\n",
                      stats.totalPulseWidthUs / stats.validTouches,
                      (stats.totalPulseWidthUs / stats.validTouches) / 1000.0);
    }

    if (stats.totalInterrupts > 0) {
        float filterEfficiency = 100.0 * stats.emiFiltered / stats.totalInterrupts;
        Serial.printf("\nFilter Efficiency:   %.2f%% (EMI rejected)\n", filterEfficiency);
        Serial.printf("Valid Touch Rate:    %.2f%% (real touches)\n",
                      100.0 * stats.validTouches / stats.totalInterrupts);
    }

    Serial.println("═══════════════════════════════════════════════════════════════\n");
}

// ─── Reset Statistics ──────────────────────────────────────────────────
void resetStats() {
    stats.totalInterrupts = 0;
    stats.validTouches = 0;
    stats.emiFiltered = 0;
    stats.minPulseWidthUs = 0xFFFFFFFF;
    stats.maxPulseWidthUs = 0;
    stats.totalPulseWidthUs = 0;
}
