/*
 * Test 45v3: WiFi EMI Touch IRQ - Production Pulse Width Filter
 *
 * PURPOSE: Filter WiFi EMI from real touches using validated pulse width threshold
 *
 * APPROACH:
 *   - On falling edge IRQ, measure how long GPIO36 stays LOW
 *   - Apply 10ms threshold: >10ms = real touch, <10ms = WiFi EMI
 *
 * VALIDATED THRESHOLD (from diagnostic tests):
 *   - WiFi EMI: 3-10 microseconds (0.003-0.010 ms) [685 samples]
 *   - Real touch: 70-279 milliseconds [5 samples]
 *   - Threshold: 10 milliseconds = 7000x separation = 100% effectiveness expected
 *
 * NO LIBRARIES: Direct GPIO only (avoids SPI conflicts with bitbanged RFID)
 */

#include <WiFi.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions
#define TOUCH_IRQ 36  // Input-only, no pull-down, picks up WiFi EMI
#define SD_CS 5

// PRODUCTION FILTER THRESHOLD (validated from diagnostic tests)
#define PULSE_WIDTH_THRESHOLD_US 10000  // 10 milliseconds

// WiFi config
String wifiSSID = "";
String wifiPassword = "";

// IRQ event tracking
volatile bool irqOccurred = false;
volatile uint32_t irqTime = 0;

// Production filter statistics
struct FilterStats {
  uint32_t totalIRQs = 0;
  uint32_t emiFiltered = 0;      // Rejected as WiFi EMI (<10ms)
  uint32_t touchesAccepted = 0;  // Accepted as real touch (>10ms)
} filterStats;

// Diagnostic statistics (pulse width distribution)
struct DiagStats {
  uint32_t under1ms = 0;
  uint32_t _1to10ms = 0;
  uint32_t _10to50ms = 0;
  uint32_t _50to100ms = 0;
  uint32_t over100ms = 0;
} diagStats;

// Test state
enum Phase { IDLE, BASELINE, TAPTEST, VALIDATION };
Phase currentPhase = IDLE;
uint32_t phaseStartTime = 0;
uint8_t tapsDetected = 0;
uint32_t bootTime = 0;

// ========================================================================
// IRQ HANDLER
// ========================================================================

void IRAM_ATTR touchISR() {
  irqOccurred = true;
  irqTime = micros();
}

// ========================================================================
// CONFIG LOADING
// ========================================================================

bool loadConfig() {
  if (!SD.begin(SD_CS)) {
    Serial.println("[CONFIG] ✗ SD card failed");
    return false;
  }

  File f = SD.open("/config.txt");
  if (!f) {
    Serial.println("[CONFIG] ✗ /config.txt not found");
    return false;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int eq = line.indexOf('=');
    if (eq == -1) continue;

    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);
    key.trim();
    value.trim();

    if (key == "WIFI_SSID") wifiSSID = value;
    else if (key == "WIFI_PASSWORD") wifiPassword = value;
  }

  f.close();

  if (wifiSSID.length() == 0) {
    Serial.println("[CONFIG] ✗ WIFI_SSID not found");
    return false;
  }

  Serial.printf("[CONFIG] ✓ Loaded: %s\n", wifiSSID.c_str());
  return true;
}

// ========================================================================
// PULSE WIDTH MEASUREMENT
// ========================================================================

uint32_t measurePulseWidth() {
  // Measure how long GPIO36 stays LOW
  uint32_t startUs = micros();
  const uint32_t TIMEOUT_US = 500000;  // 500ms max

  // Wait for pin to go HIGH or timeout
  while (digitalRead(TOUCH_IRQ) == LOW) {
    if (micros() - startUs > TIMEOUT_US) {
      return TIMEOUT_US;  // Still LOW after timeout
    }
  }

  return micros() - startUs;
}

void processPulseWidth(uint32_t widthUs) {
  filterStats.totalIRQs++;

  // ========================================================================
  // PRODUCTION FILTER: Apply 10ms threshold
  // ========================================================================
  bool isRealTouch = (widthUs >= PULSE_WIDTH_THRESHOLD_US);

  if (isRealTouch) {
    filterStats.touchesAccepted++;

    // Production touch handler - log to serial
    Serial.printf("[%lu ms] ✓ TOUCH DETECTED: pulse width = %lu us (%.2f ms)\n",
                 millis(), widthUs, widthUs / 1000.0);

    // Count for tap test validation
    if (currentPhase == TAPTEST || currentPhase == VALIDATION) {
      tapsDetected++;
      Serial.printf("             Tap #%d registered\n", tapsDetected);
    }

  } else {
    filterStats.emiFiltered++;

    // Log EMI only in diagnostic modes
    if (currentPhase == BASELINE || currentPhase == TAPTEST) {
      Serial.printf("[%lu ms] ✗ EMI filtered: pulse width = %lu us (%.2f ms)\n",
                   millis(), widthUs, widthUs / 1000.0);
    }
  }

  // ========================================================================
  // DIAGNOSTIC: Categorize pulse width for analysis
  // ========================================================================
  if (widthUs < 1000) diagStats.under1ms++;
  else if (widthUs < 10000) diagStats._1to10ms++;
  else if (widthUs < 50000) diagStats._10to50ms++;
  else if (widthUs < 100000) diagStats._50to100ms++;
  else diagStats.over100ms++;
}

// ========================================================================
// TEST FUNCTIONS
// ========================================================================

void startBaseline() {
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║        BASELINE: WiFi EMI Filter Validation           ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  Serial.println("║  Duration: 60 seconds                                  ║");
  Serial.println("║  Action: DO NOT TOUCH THE SCREEN                      ║");
  Serial.println("║  Goal: Verify 100% EMI rejection (zero false positives║");
  Serial.println("╚════════════════════════════════════════════════════════╝\n");

  filterStats = {0};
  diagStats = {0};
  tapsDetected = 0;
  currentPhase = BASELINE;
  phaseStartTime = millis();
}

void endBaseline() {
  currentPhase = IDLE;

  float filterEffectiveness = (filterStats.totalIRQs > 0) ?
    (100.0 * filterStats.emiFiltered / filterStats.totalIRQs) : 0.0;

  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║        BASELINE COMPLETE - WiFi EMI Filter Test       ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  Serial.printf("║  Total IRQs:        %6d                           ║\n", filterStats.totalIRQs);
  Serial.printf("║  EMI Filtered:      %6d  (✓ Rejected)             ║\n", filterStats.emiFiltered);
  Serial.printf("║  False Positives:   %6d  (touches detected)       ║\n", filterStats.touchesAccepted);
  Serial.println("╟────────────────────────────────────────────────────────╢");
  Serial.printf("║  Filter Effectiveness: %.1f%% EMI rejection         ║\n", filterEffectiveness);
  Serial.println("╟────────────────────────────────────────────────────────╢");
  Serial.printf("║  Pulse Width Distribution:                             ║\n");
  Serial.printf("║    <1ms:       %6d  (WiFi EMI signature)          ║\n", diagStats.under1ms);
  Serial.printf("║    1-10ms:     %6d                                ║\n", diagStats._1to10ms);
  Serial.printf("║    10-50ms:    %6d                                ║\n", diagStats._10to50ms);
  Serial.printf("║    50-100ms:   %6d                                ║\n", diagStats._50to100ms);
  Serial.printf("║    >100ms:     %6d                                ║\n", diagStats.over100ms);
  Serial.println("╚════════════════════════════════════════════════════════╝\n");

  // Validation result
  if (filterStats.touchesAccepted == 0) {
    Serial.println("[RESULT] ✓✓✓ PERFECT: 100% EMI rejection, zero false positives\n");
  } else {
    Serial.printf("[RESULT] ✗✗✗ FAILED: %d false positives detected\n\n", filterStats.touchesAccepted);
  }
}

void startTapTest() {
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║        TAP TEST: Touch Detection Validation           ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  Serial.println("║  Duration: 60 seconds or 5 taps                        ║");
  Serial.println("║  Action: TAP THE SCREEN 5 TIMES NOW                   ║");
  Serial.println("║  Goal: Verify 100% touch detection + EMI rejection    ║");
  Serial.println("╚════════════════════════════════════════════════════════╝\n");

  filterStats = {0};
  diagStats = {0};
  tapsDetected = 0;
  currentPhase = TAPTEST;
  phaseStartTime = millis();
}

void endTapTest() {
  currentPhase = IDLE;

  float touchDetectionRate = (tapsDetected >= 5) ? 100.0 : (100.0 * tapsDetected / 5.0);
  float emiRejectionRate = (filterStats.totalIRQs > 0) ?
    (100.0 * filterStats.emiFiltered / filterStats.totalIRQs) : 0.0;

  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║        TAP TEST COMPLETE - Filter Validation          ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  Serial.printf("║  Taps Detected:     %6d / 5  (%.0f%%)                ║\n",
                tapsDetected, touchDetectionRate);
  Serial.printf("║  Total IRQs:        %6d                           ║\n", filterStats.totalIRQs);
  Serial.printf("║  EMI Filtered:      %6d  (%.0f%%)                  ║\n",
                filterStats.emiFiltered, emiRejectionRate);
  Serial.printf("║  Touches Accepted:  %6d                           ║\n", filterStats.touchesAccepted);
  Serial.println("╟────────────────────────────────────────────────────────╢");
  Serial.printf("║  Pulse Width Distribution:                             ║\n");
  Serial.printf("║    <1ms:       %6d  (WiFi EMI)                    ║\n", diagStats.under1ms);
  Serial.printf("║    1-10ms:     %6d                                ║\n", diagStats._1to10ms);
  Serial.printf("║    10-50ms:    %6d  (Real touches)                ║\n", diagStats._10to50ms);
  Serial.printf("║    50-100ms:   %6d                                ║\n", diagStats._50to100ms);
  Serial.printf("║    >100ms:     %6d                                ║\n", diagStats.over100ms);
  Serial.println("╚════════════════════════════════════════════════════════╝\n");

  if (tapsDetected >= 5 && emiRejectionRate >= 95.0) {
    Serial.println("[RESULT] ✓✓✓ SUCCESS: All taps detected + EMI filtered\n");
  } else if (tapsDetected >= 5) {
    Serial.println("[RESULT] ⚠️ PARTIAL: All taps detected, but EMI rejection needs improvement\n");
  } else {
    Serial.printf("[RESULT] ✗✗✗ INCOMPLETE: Only %d/5 taps detected\n\n", tapsDetected);
  }
}

// ========================================================================
// SETUP
// ========================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  bootTime = millis();

  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║  Test 45v3: WiFi EMI Touch IRQ - Production Filter    ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  Serial.println("║  VALIDATED THRESHOLD: 10 milliseconds                  ║");
  Serial.println("║  WiFi EMI: <0.01ms | Real Touch: 70-279ms             ║");
  Serial.println("║  Separation: 7000x | Expected: 100% effectiveness     ║");
  Serial.println("╚════════════════════════════════════════════════════════╝\n");

  // Load WiFi config
  if (!loadConfig()) {
    Serial.println("[ERROR] Failed to load config");
    while (1) delay(1000);
  }

  // Connect to WiFi (EMI source)
  Serial.printf("[WIFI] Connecting to %s...\n", wifiSSID.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] ✓ Connected: %s (%d dBm)\n",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\n[WIFI] ✗ Connection failed");
    Serial.println("[ERROR] WiFi EMI is the root cause - need WiFi enabled!");
    while (1) delay(1000);
  }

  // Configure GPIO36 touch IRQ
  pinMode(TOUCH_IRQ, INPUT);
  attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ), touchISR, FALLING);
  Serial.println("[TOUCH] ✓ GPIO36 IRQ configured\n");

  // Show commands
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║  COMMANDS:                                             ║");
  Serial.println("║    BASELINE  - Validate EMI filtering (60s, no touch) ║");
  Serial.println("║    TAPTEST   - Validate touch detection (5 taps)      ║");
  Serial.println("║    VALIDATE  - Combined test (baseline + tap)         ║");
  Serial.println("║    STATUS    - Show filter statistics                 ║");
  Serial.println("║    RESET     - Reset all statistics                   ║");
  Serial.println("╚════════════════════════════════════════════════════════╝\n");
}

// ========================================================================
// LOOP
// ========================================================================

void loop() {
  // Process IRQ events
  if (irqOccurred) {
    irqOccurred = false;

    // Small delay to ensure pin has stabilized
    delayMicroseconds(100);

    // Measure pulse width
    uint32_t widthUs = measurePulseWidth();
    processPulseWidth(widthUs);
  }

  // Monitor test phase timeouts
  if (currentPhase == BASELINE && millis() - phaseStartTime >= 60000) {
    endBaseline();
  }

  if (currentPhase == TAPTEST) {
    if (tapsDetected >= 5 || millis() - phaseStartTime >= 60000) {
      endTapTest();
    }
  }

  // Process serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "BASELINE") {
      startBaseline();
    }
    else if (cmd == "TAPTEST") {
      startTapTest();
    }
    else if (cmd == "RESET") {
      filterStats = {0};
      diagStats = {0};
      tapsDetected = 0;
      Serial.println("[RESET] ✓ All statistics cleared\n");
    }
    else if (cmd == "STATUS") {
      uint32_t uptime = (millis() - bootTime) / 1000;
      float emiRate = (uptime > 0) ? (float)filterStats.emiFiltered / uptime : 0.0;
      float touchRate = (uptime > 0) ? (float)filterStats.touchesAccepted / uptime : 0.0;
      float filterEffectiveness = (filterStats.totalIRQs > 0) ?
        (100.0 * filterStats.emiFiltered / filterStats.totalIRQs) : 0.0;

      Serial.println("\n╔════════════════════════════════════════════════════════╗");
      Serial.println("║               PRODUCTION FILTER STATUS                 ║");
      Serial.println("╠════════════════════════════════════════════════════════╣");
      Serial.printf("║  Uptime:           %6d seconds                     ║\n", uptime);
      Serial.println("╟────────────────────────────────────────────────────────╢");
      Serial.printf("║  Total IRQs:       %6d  (%.2f/sec)                 ║\n",
                   filterStats.totalIRQs, emiRate + touchRate);
      Serial.printf("║  EMI Filtered:     %6d  (%.2f/sec)                 ║\n",
                   filterStats.emiFiltered, emiRate);
      Serial.printf("║  Touches Accepted: %6d  (%.2f/sec)                 ║\n",
                   filterStats.touchesAccepted, touchRate);
      Serial.println("╟────────────────────────────────────────────────────────╢");
      Serial.printf("║  Filter Effectiveness: %.1f%% EMI rejection          ║\n", filterEffectiveness);
      Serial.println("╟────────────────────────────────────────────────────────╢");
      Serial.printf("║  Current Phase:    %s                              ║\n",
                   currentPhase == BASELINE ? "BASELINE" :
                   currentPhase == TAPTEST ? "TAP TEST" :
                   currentPhase == VALIDATION ? "VALIDATION" : "IDLE       ");
      Serial.printf("║  Taps Detected:    %6d                            ║\n", tapsDetected);
      Serial.println("╚════════════════════════════════════════════════════════╝\n");
    }
  }
}
