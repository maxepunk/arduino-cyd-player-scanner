/*
 * Test Sketch 44: WiFi vs Audio Isolation Test
 *
 * PURPOSE: Determine root cause of GPIO36 spurious IRQs
 *
 * HYPOTHESIS TO TEST:
 * - WiFi EMI causes spurious IRQs on floating GPIO36
 * - Audio circuit idling causes spurious IRQs on GPIO36
 * - OR both contribute
 *
 * TEST MATRIX:
 * State A: WiFi OFF + Audio OFF → Baseline spurious IRQ rate
 * State B: WiFi ON  + Audio OFF → WiFi effect
 * State C: WiFi OFF + Audio ON  → Audio effect
 * State D: WiFi ON  + Audio ON  → Combined effect
 *
 * COMMANDS:
 * - TEST_A, TEST_B, TEST_C, TEST_D → Run 10-second test for each state
 * - STATUS → Show results summary
 * - HELP → Show commands
 */

#include <WiFi.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include <SD.h>
#include <SPI.h>

// ─── Pin Definitions ───────────────────────────────────────────
#define TOUCH_IRQ   36    // Input-only, no internal pull-up possible

// SD Card (VSPI)
#define SD_CS       5
#define SD_SCK      18
#define SD_MISO     19
#define SD_MOSI     23

// Audio (I2S)
#define I2S_BCLK    26
#define I2S_LRC     25
#define I2S_DIN     22

// ─── Global Objects ───────────────────────────────────────────
SPIClass sdSPI(VSPI);
AudioGeneratorWAV *wav = nullptr;
AudioFileSourceSD *audioFile = nullptr;
AudioOutputI2S *audioOut = nullptr;

// ─── Touch IRQ Statistics ─────────────────────────────────────
struct TestResult {
    String testName;
    bool wifiOn;
    bool audioOn;
    uint32_t irqCount;
    unsigned long duration;
    float irqRate;
} testResults[4];

int numTestsCompleted = 0;

volatile uint32_t touchIRQCount = 0;
unsigned long testStartTime = 0;
bool testRunning = false;

// ─── Touch ISR ────────────────────────────────────────────────
void IRAM_ATTR touchISR() {
    touchIRQCount++;
}

// ══════════════════════════════════════════════════════════════
// ═══ SETUP ════════════════════════════════════════════════════
// ══════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n╔══════════════════════════════════════════════════╗");
    Serial.println("║  TEST 44: WiFi vs Audio Isolation Test          ║");
    Serial.println("╚══════════════════════════════════════════════════╝\n");

    Serial.printf("[INIT] Free heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize SD Card for audio
    Serial.println("[SD] Initializing SD card...");
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS, sdSPI)) {
        Serial.println("[SD] ✓ Card mounted");
    } else {
        Serial.println("[SD] ✗ Card mount failed (audio tests will fail)");
    }

    // Initialize Audio Output
    Serial.println("[AUDIO] Initializing I2S...");
    audioOut = new AudioOutputI2S();
    audioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    Serial.println("[AUDIO] ✓ I2S configured");

    // Configure Touch IRQ (GPIO36 - INPUT only, no pull-up)
    Serial.println("[TOUCH] Configuring GPIO36 interrupt...");
    pinMode(TOUCH_IRQ, INPUT);  // GPIO36 can't use INPUT_PULLUP
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ), touchISR, FALLING);
    Serial.println("[TOUCH] ✓ Interrupt attached");

    Serial.println("\n╔══════════════════════════════════════════════════╗");
    Serial.println("║ COMMANDS:                                        ║");
    Serial.println("║   TEST_A  - WiFi OFF + Audio OFF (baseline)     ║");
    Serial.println("║   TEST_B  - WiFi ON  + Audio OFF (WiFi effect)  ║");
    Serial.println("║   TEST_C  - WiFi OFF + Audio ON  (Audio effect) ║");
    Serial.println("║   TEST_D  - WiFi ON  + Audio ON  (combined)     ║");
    Serial.println("║   STATUS  - Show all test results               ║");
    Serial.println("║   HELP    - Show this menu                       ║");
    Serial.println("╚══════════════════════════════════════════════════╝\n");

    Serial.println("READY - Type TEST_A to begin systematic testing\n");
}

// ══════════════════════════════════════════════════════════════
// ═══ LOOP ═════════════════════════════════════════════════════
// ══════════════════════════════════════════════════════════════

void loop() {
    // Process audio if playing
    if (wav && wav->isRunning()) {
        if (!wav->loop()) {
            stopAudio();
        }
    }

    // Monitor test duration
    if (testRunning) {
        unsigned long elapsed = millis() - testStartTime;
        if (elapsed >= 10000) {  // 10-second test
            completeTest();
        }
    }

    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "TEST_A") {
            runTest("TEST_A", false, false);
        }
        else if (cmd == "TEST_B") {
            runTest("TEST_B", true, false);
        }
        else if (cmd == "TEST_C") {
            runTest("TEST_C", false, true);
        }
        else if (cmd == "TEST_D") {
            runTest("TEST_D", true, true);
        }
        else if (cmd == "STATUS") {
            printAllResults();
        }
        else if (cmd == "HELP") {
            printHelp();
        }
    }
}

// ══════════════════════════════════════════════════════════════
// ═══ TEST FUNCTIONS ═══════════════════════════════════════════
// ══════════════════════════════════════════════════════════════

void runTest(String testName, bool enableWiFi, bool enableAudio) {
    if (testRunning) {
        Serial.println("[TEST] Test already running, please wait...");
        return;
    }

    Serial.println("\n╔══════════════════════════════════════════════════╗");
    Serial.printf("║  %s STARTING                                  ║\n", testName.c_str());
    Serial.println("╠══════════════════════════════════════════════════╣");
    Serial.printf("║  WiFi:  %s                                      ║\n", enableWiFi ? "ON " : "OFF");
    Serial.printf("║  Audio: %s                                      ║\n", enableAudio ? "ON " : "OFF");
    Serial.println("║  Duration: 10 seconds                            ║");
    Serial.println("╚══════════════════════════════════════════════════╝\n");

    // Ensure clean state
    stopWiFi();
    stopAudio();
    delay(500);

    // Configure WiFi
    if (enableWiFi) {
        Serial.println("[WIFI] Enabling WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin("Sidewinder", "wifi4us!");

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WIFI] ✓ Connected: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[WIFI] ✗ Connection failed (continuing anyway)");
        }
    }

    // Configure Audio
    if (enableAudio) {
        Serial.println("[AUDIO] Starting continuous audio playback...");
        audioFile = new AudioFileSourceSD("/AUDIO/TAC001.wav");
        if (audioFile->isOpen()) {
            wav = new AudioGeneratorWAV();
            if (wav->begin(audioFile, audioOut)) {
                Serial.println("[AUDIO] ✓ Playback started");
            } else {
                Serial.println("[AUDIO] ✗ Playback failed");
                delete wav;
                delete audioFile;
                wav = nullptr;
                audioFile = nullptr;
            }
        } else {
            Serial.println("[AUDIO] ✗ File open failed");
            delete audioFile;
            audioFile = nullptr;
        }
    }

    // Start test
    touchIRQCount = 0;
    testStartTime = millis();
    testRunning = true;

    Serial.println("\n[TEST] Monitoring GPIO36 IRQs for 10 seconds...\n");
}

void completeTest() {
    testRunning = false;
    unsigned long duration = millis() - testStartTime;

    // Store results
    TestResult &result = testResults[numTestsCompleted];
    result.testName = (numTestsCompleted == 0) ? "TEST_A" :
                      (numTestsCompleted == 1) ? "TEST_B" :
                      (numTestsCompleted == 2) ? "TEST_C" : "TEST_D";
    result.wifiOn = WiFi.status() == WL_CONNECTED;
    result.audioOn = (wav && wav->isRunning());
    result.irqCount = touchIRQCount;
    result.duration = duration;
    result.irqRate = (touchIRQCount * 1000.0) / duration;  // IRQs per second

    // Print results
    Serial.println("\n╔══════════════════════════════════════════════════╗");
    Serial.println("║  TEST COMPLETE                                   ║");
    Serial.println("╠══════════════════════════════════════════════════╣");
    Serial.printf("║  Test:          %s                             ║\n", result.testName.c_str());
    Serial.printf("║  Duration:      %lu ms                          ║\n", result.duration);
    Serial.printf("║  IRQ Count:     %d                              ║\n", result.irqCount);
    Serial.printf("║  IRQ Rate:      %.2f IRQs/sec                   ║\n", result.irqRate);
    Serial.println("╚══════════════════════════════════════════════════╝\n");

    numTestsCompleted++;

    // Clean up
    stopWiFi();
    stopAudio();
}

void printAllResults() {
    if (numTestsCompleted == 0) {
        Serial.println("\n[STATUS] No tests completed yet. Run TEST_A first.\n");
        return;
    }

    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║                    ALL TEST RESULTS                          ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");

    for (int i = 0; i < numTestsCompleted; i++) {
        TestResult &r = testResults[i];
        Serial.printf("║ %s: WiFi=%s, Audio=%s → %.2f IRQs/sec (%d total)  ║\n",
                     r.testName.c_str(),
                     r.wifiOn ? "ON " : "OFF",
                     r.audioOn ? "ON " : "OFF",
                     r.irqRate,
                     r.irqCount);
    }

    Serial.println("╚══════════════════════════════════════════════════════════════╝\n");

    // Analysis
    if (numTestsCompleted >= 2) {
        Serial.println("╔══════════════════════════════════════════════════════════════╗");
        Serial.println("║                      ANALYSIS                                ║");
        Serial.println("╠══════════════════════════════════════════════════════════════╣");

        if (numTestsCompleted >= 4) {
            float baseline = testResults[0].irqRate;  // A: WiFi OFF, Audio OFF
            float wifiEffect = testResults[1].irqRate;  // B: WiFi ON, Audio OFF
            float audioEffect = testResults[2].irqRate;  // C: WiFi OFF, Audio ON
            float combined = testResults[3].irqRate;  // D: WiFi ON, Audio ON

            Serial.printf("║ Baseline (WiFi OFF, Audio OFF): %.2f IRQs/sec           ║\n", baseline);
            Serial.printf("║ WiFi Impact: %.2f → %.2f (%.1fx)                        ║\n",
                         baseline, wifiEffect, wifiEffect / (baseline + 0.01));
            Serial.printf("║ Audio Impact: %.2f → %.2f (%.1fx)                       ║\n",
                         baseline, audioEffect, audioEffect / (baseline + 0.01));
            Serial.printf("║ Combined: %.2f IRQs/sec                                  ║\n", combined);

            Serial.println("║                                                              ║");

            if (wifiEffect > baseline * 1.5) {
                Serial.println("║ ✓ WiFi is a significant contributor to spurious IRQs    ║");
            }
            if (audioEffect < baseline * 0.5) {
                Serial.println("║ ✓ Audio playback REDUCES spurious IRQs                  ║");
            }
        }

        Serial.println("╚══════════════════════════════════════════════════════════════╝\n");
    }
}

void printHelp() {
    Serial.println("\nCOMMANDS:");
    Serial.println("  TEST_A  - WiFi OFF + Audio OFF (baseline)");
    Serial.println("  TEST_B  - WiFi ON  + Audio OFF (WiFi effect)");
    Serial.println("  TEST_C  - WiFi OFF + Audio ON  (Audio effect)");
    Serial.println("  TEST_D  - WiFi ON  + Audio ON  (combined)");
    Serial.println("  STATUS  - Show all test results with analysis");
    Serial.println("  HELP    - Show this menu\n");
}

// ─── Helper Functions ─────────────────────────────────────────

void stopWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
    }
}

void stopAudio() {
    if (wav) {
        if (wav->isRunning()) wav->stop();
        delete wav;
        wav = nullptr;
    }
    if (audioFile) {
        delete audioFile;
        audioFile = nullptr;
    }
}
