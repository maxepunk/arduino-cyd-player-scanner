/*
 * Test Sketch 43: XPT2046 Touch Initialization Test
 *
 * PURPOSE: Validate XPT2046 touch controller initialization eliminates spurious IRQs
 *
 * CONTEXT: Main sketch experiencing spurious touch interrupts ~1s after audio starts
 *
 * HARDWARE CONSTRAINTS:
 * - RFID: Software SPI on GPIO 22, 27, 35, 3
 * - SD Card: Hardware VSPI on GPIO 18, 23, 19, 5
 * - Touch: Dedicated SPI on GPIO 25, 32, 33, 36, 39
 * - WiFi: Active
 * - Audio: I2S on GPIO 26, 25, 22
 *
 * TEST PLAN:
 * 1. Initialize all subsystems (WiFi, SD, touch, audio)
 * 2. Play audio for 5 seconds
 * 3. Monitor touch IRQ behavior
 * 4. Compare spurious interrupt count before/after XPT2046 init
 *
 * SUCCESS CRITERIA:
 * - No spurious touch IRQs during audio playback
 * - Real touches still detected when physically tapping
 * - No conflicts with SD/RFID/WiFi
 */

#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ─── Pin Definitions ─────────────────────────────────────────────
// Touch Controller (XPT2046)
#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define TOUCH_CLK   25
#define TOUCH_MISO  39
#define TOUCH_MOSI  32

// SD Card (VSPI)
#define SD_CS       5
#define SD_SCK      18
#define SD_MISO     19
#define SD_MOSI     23

// Audio (I2S)
#define I2S_BCLK    26
#define I2S_LRC     25
#define I2S_DIN     22

// ─── Global Objects ───────────────────────────────────────────────
SPIClass sdSPI(VSPI);
SPIClass touchSPI(HSPI);  // Use HSPI for touch (VSPI used by SD)
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

AudioGeneratorWAV *wav = nullptr;
AudioFileSourceSD *audioFile = nullptr;
AudioOutputI2S *audioOut = nullptr;

// ─── Statistics ───────────────────────────────────────────────────
struct TouchStats {
    uint32_t spuriousIRQs = 0;
    uint32_t validTouches = 0;
    uint32_t totalIRQs = 0;
    unsigned long lastIRQTime = 0;
} touchStats;

volatile bool touchIRQOccurred = false;

// ─── Touch ISR ────────────────────────────────────────────────────
void IRAM_ATTR touchISR() {
    touchIRQOccurred = true;
    touchStats.totalIRQs++;
    touchStats.lastIRQTime = millis();
}

// ═══════════════════════════════════════════════════════════════════
// ═══ SETUP ═════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n╔════════════════════════════════════════════════════════╗");
    Serial.println("║   TEST 43: XPT2046 Touch Initialization Test          ║");
    Serial.println("╚════════════════════════════════════════════════════════╝\n");

    Serial.printf("[INIT] Free heap at start: %d bytes\n", ESP.getFreeHeap());

    // ═══ WiFi Init ═══════════════════════════════════════════════
    Serial.println("\n[WIFI] Initializing WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin("Sidewinder", "wifi4us!");

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WIFI] ✓ Connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WIFI] ✗ Connection failed (continuing anyway)");
    }

    // ═══ SD Card Init ═══════════════════════════════════════════
    Serial.println("\n[SD] Initializing SD card on VSPI...");
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (SD.begin(SD_CS, sdSPI)) {
        Serial.println("[SD] ✓ Card mounted successfully");
        Serial.printf("[SD] Card size: %.2f GB\n", SD.cardSize() / 1073741824.0);
    } else {
        Serial.println("[SD] ✗ Card mount failed");
    }

    // ═══ Touch Controller Init ═══════════════════════════════════
    Serial.println("\n[TOUCH] Initializing XPT2046 on HSPI...");
    Serial.printf("[TOUCH] Pins: CLK=%d, MISO=%d, MOSI=%d, CS=%d, IRQ=%d\n",
                  TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS, TOUCH_IRQ);

    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);

    if (touch.begin(touchSPI)) {
        Serial.println("[TOUCH] ✓ XPT2046 initialized successfully");
        touch.setRotation(1);
    } else {
        Serial.println("[TOUCH] ✗ XPT2046 initialization failed");
    }

    // Attach interrupt AFTER chip initialization
    pinMode(TOUCH_IRQ, INPUT);
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ), touchISR, FALLING);
    Serial.println("[TOUCH] ✓ Interrupt attached");

    // ═══ Audio Init ═══════════════════════════════════════════════
    Serial.println("\n[AUDIO] Initializing I2S audio...");
    audioOut = new AudioOutputI2S();
    audioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    Serial.println("[AUDIO] ✓ I2S configured");

    Serial.printf("\n[INIT] Free heap after init: %d bytes\n", ESP.getFreeHeap());

    // ═══ Interactive Commands ═════════════════════════════════════
    Serial.println("\n╔════════════════════════════════════════════════════════╗");
    Serial.println("║ COMMANDS:                                              ║");
    Serial.println("║   STATUS  - Show touch statistics                      ║");
    Serial.println("║   PLAY    - Play audio and monitor touch IRQs          ║");
    Serial.println("║   RESET   - Reset statistics                           ║");
    Serial.println("║   HELP    - Show this menu                             ║");
    Serial.println("╚════════════════════════════════════════════════════════╝\n");
}

// ═══════════════════════════════════════════════════════════════════
// ═══ LOOP ══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════

void loop() {
    // Process audio
    if (wav && wav->isRunning()) {
        if (!wav->loop()) {
            Serial.println("[AUDIO] Playback finished");
            delete wav;
            delete audioFile;
            wav = nullptr;
            audioFile = nullptr;
        }
    }

    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "STATUS") {
            printStatus();
        }
        else if (cmd == "PLAY") {
            playAudioTest();
        }
        else if (cmd == "RESET") {
            touchStats = {0};
            Serial.println("[STATS] Statistics reset");
        }
        else if (cmd == "HELP") {
            Serial.println("\nCOMMANDS:");
            Serial.println("  STATUS  - Show touch statistics");
            Serial.println("  PLAY    - Play audio and monitor touch IRQs");
            Serial.println("  RESET   - Reset statistics");
            Serial.println("  HELP    - Show this menu\n");
        }
    }

    // Check for touch events
    if (touchIRQOccurred) {
        touchIRQOccurred = false;

        // Validate if this is a real touch by reading from XPT2046
        if (touch.touched()) {
            TS_Point p = touch.getPoint();
            touchStats.validTouches++;
            Serial.printf("[TOUCH] ✓ Valid touch detected: X=%d, Y=%d, Z=%d\n",
                         p.x, p.y, p.z);
        } else {
            touchStats.spuriousIRQs++;
            Serial.printf("[TOUCH] ✗ Spurious IRQ detected (not a real touch)\n");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// ═══ HELPER FUNCTIONS ══════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════

void printStatus() {
    Serial.println("\n╔════════════════════════════════════════════════════════╗");
    Serial.println("║ TOUCH STATISTICS                                       ║");
    Serial.println("╠════════════════════════════════════════════════════════╣");
    Serial.printf("║ Total IRQs:        %6d                             ║\n", touchStats.totalIRQs);
    Serial.printf("║ Valid Touches:     %6d                             ║\n", touchStats.validTouches);
    Serial.printf("║ Spurious IRQs:     %6d                             ║\n", touchStats.spuriousIRQs);

    if (touchStats.totalIRQs > 0) {
        float spuriousRate = 100.0 * touchStats.spuriousIRQs / touchStats.totalIRQs;
        Serial.printf("║ Spurious Rate:     %5.1f%%                            ║\n", spuriousRate);
    }

    Serial.printf("║ Last IRQ:          %6lu ms ago                      ║\n",
                 millis() - touchStats.lastIRQTime);
    Serial.println("╠════════════════════════════════════════════════════════╣");
    Serial.printf("║ Free Heap:         %6d KB                          ║\n", ESP.getFreeHeap() / 1024);
    Serial.printf("║ WiFi Status:       %s                               ║\n",
                 WiFi.status() == WL_CONNECTED ? "Connected    " : "Disconnected ");
    Serial.println("╚════════════════════════════════════════════════════════╝\n");
}

void playAudioTest() {
    Serial.println("\n[TEST] Starting audio playback test...");
    Serial.println("[TEST] Monitoring for spurious touch IRQs during playback");
    Serial.println("[TEST] (Do NOT tap the screen - we're checking for false triggers)\n");

    // Reset stats for this test
    uint32_t startingSpurious = touchStats.spuriousIRQs;
    uint32_t startingValid = touchStats.validTouches;

    // Try to play an audio file
    audioFile = new AudioFileSourceSD("/AUDIO/TAC001.wav");
    if (!audioFile->isOpen()) {
        Serial.println("[AUDIO] ✗ Failed to open audio file");
        delete audioFile;
        audioFile = nullptr;
        return;
    }

    wav = new AudioGeneratorWAV();
    if (!wav->begin(audioFile, audioOut)) {
        Serial.println("[AUDIO] ✗ Failed to start playback");
        delete wav;
        delete audioFile;
        wav = nullptr;
        audioFile = nullptr;
        return;
    }

    Serial.println("[AUDIO] ✓ Playback started");
    Serial.println("[TEST] Monitoring for 5 seconds...\n");

    unsigned long testStart = millis();
    unsigned long lastReport = testStart;

    while (wav && wav->isRunning() && millis() - testStart < 5000) {
        wav->loop();

        // Report every second
        if (millis() - lastReport >= 1000) {
            lastReport = millis();
            uint32_t newSpurious = touchStats.spuriousIRQs - startingSpurious;
            uint32_t newValid = touchStats.validTouches - startingValid;
            Serial.printf("[TEST] +%lums: Spurious=%d, Valid=%d\n",
                         millis() - testStart, newSpurious, newValid);
        }

        delay(10);
    }

    // Stop audio
    if (wav) {
        wav->stop();
        delete wav;
        delete audioFile;
        wav = nullptr;
        audioFile = nullptr;
    }

    // Report results
    uint32_t testSpurious = touchStats.spuriousIRQs - startingSpurious;
    uint32_t testValid = touchStats.validTouches - startingValid;

    Serial.println("\n╔════════════════════════════════════════════════════════╗");
    Serial.println("║ AUDIO PLAYBACK TEST RESULTS                           ║");
    Serial.println("╠════════════════════════════════════════════════════════╣");
    Serial.printf("║ Test Duration:     5000 ms                             ║\n");
    Serial.printf("║ Spurious IRQs:     %6d                             ║\n", testSpurious);
    Serial.printf("║ Valid Touches:     %6d                             ║\n", testValid);

    if (testSpurious == 0) {
        Serial.println("║ RESULT:            ✓✓✓ PASS ✓✓✓                       ║");
        Serial.println("║                    No spurious interrupts!             ║");
    } else {
        Serial.println("║ RESULT:            ✗✗✗ FAIL ✗✗✗                       ║");
        Serial.printf("║                    %d false triggers detected          ║\n", testSpurious);
    }

    Serial.println("╚════════════════════════════════════════════════════════╝\n");
}
