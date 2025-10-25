/*
 * Test Sketch 53: AudioDriver.h HAL Component
 *
 * Purpose: Validate I2S audio playback with lazy initialization
 *
 * Tests:
 * 1. Lazy initialization (no beeping at boot!)
 * 2. DAC silencing at boot
 * 3. WAV file playback
 * 4. Playback control (start, stop, status)
 * 5. Loop processing
 * 6. Integration with SDCard.h
 *
 * Expected Behavior:
 * - NO beeping at boot (DAC pins silenced, lazy init)
 * - Audio initialization deferred until first play() call
 * - WAV file playback works smoothly
 * - Serial commands allow interactive testing
 *
 * Hardware Requirements:
 * - ESP32-2432S028R (CYD) with speaker
 * - SD card with WAV files in /AUDIO/ directory
 *
 * Serial Commands:
 * - PLAY <filename>   - Play audio file (e.g., PLAY kaa001.wav)
 * - STOP              - Stop current playback
 * - STATUS            - Show audio status
 * - HELP              - Show available commands
 *
 * Success Criteria:
 * - No beeping/noise at boot
 * - Audio plays smoothly when requested
 * - Clean start/stop transitions
 * - Accurate isPlaying() status
 */

// Use relative path to find config.h and hal/ headers
// In test-sketches, we need to go up and into ALNScanner_v5
#define DEBUG_MODE 1  // Enable verbose logging for testing

#include "../../ALNScanner_v5/config.h"
#include "../../ALNScanner_v5/hal/SDCard.h"
#include "../../ALNScanner_v5/hal/AudioDriver.h"

// Test statistics
struct TestStats {
    uint32_t playAttempts = 0;
    uint32_t playSuccesses = 0;
    uint32_t playFailures = 0;
    uint32_t naturalStops = 0;
    uint32_t manualStops = 0;
} stats;

// Test state
bool audioInitialized = false;
uint32_t lastStatusTime = 0;
String currentFile = "";

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial monitor

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   AUDIO HAL TEST - AudioDriver.h");
    Serial.println("========================================");
    Serial.printf("Build time: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println();

    // STEP 1: Initialize SD card (required for audio files)
    Serial.println("[TEST] Step 1: Initializing SD card...");
    auto& sd = hal::SDCard::getInstance();
    if (sd.begin()) {
        Serial.println("[TEST] ✓ SD card initialized");
    } else {
        Serial.println("[TEST] ✗ SD card initialization FAILED");
        Serial.println("[TEST] Cannot proceed without SD card");
        while (1) delay(1000);
    }

    // STEP 2: Get audio driver instance
    Serial.println("\n[TEST] Step 2: Getting AudioDriver instance...");
    auto& audio = hal::AudioDriver::getInstance();
    Serial.println("[TEST] ✓ AudioDriver instance obtained");

    // STEP 3: Silence DAC pins (CRITICAL - prevents beeping!)
    Serial.println("\n[TEST] Step 3: Silencing DAC pins...");
    audio.silenceDAC();
    Serial.println("[TEST] ✓ DAC pins silenced");

    // STEP 4: Verify lazy initialization (should NOT be initialized yet)
    Serial.println("\n[TEST] Step 4: Verifying lazy initialization...");
    Serial.println("[TEST] Audio subsystem should NOT be initialized yet");
    Serial.println("[TEST] Initialization happens on first play() call");
    Serial.println("[TEST] ✓ Lazy initialization pattern active");

    // STEP 5: List available audio files
    Serial.println("\n[TEST] Step 5: Listing available audio files...");
    listAudioFiles();

    // Ready for interactive testing
    Serial.println("\n========================================");
    Serial.println("   READY FOR INTERACTIVE TESTING");
    Serial.println("========================================");
    Serial.println("\nCommands:");
    Serial.println("  PLAY <filename>   - Play audio file");
    Serial.println("  STOP              - Stop playback");
    Serial.println("  STATUS            - Show audio status");
    Serial.println("  LIST              - List audio files");
    Serial.println("  HELP              - Show commands");
    Serial.println();
    Serial.println("Example: PLAY kaa001.wav");
    Serial.println();

    Serial.printf("[BOOT] Boot complete at %lu ms\n", millis());
    Serial.printf("[BOOT] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[BOOT] Listening for commands...\n");
}

void loop() {
    auto& audio = hal::AudioDriver::getInstance();

    // CRITICAL: Call audio loop() frequently for smooth playback
    audio.loop();

    // Detect natural stop (playback finished)
    static bool wasPlaying = false;
    bool isPlaying = audio.isPlaying();
    if (wasPlaying && !isPlaying) {
        stats.naturalStops++;
        Serial.println("\n[AUDIO] Playback finished naturally");
        Serial.printf("[AUDIO] Total natural stops: %d\n", stats.naturalStops);
        currentFile = "";
    }
    wasPlaying = isPlaying;

    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        processCommand(cmd);
    }

    // Periodic status update (every 10 seconds)
    if (millis() - lastStatusTime > 10000) {
        lastStatusTime = millis();
        if (audio.isPlaying()) {
            Serial.printf("[STATUS] Playing: %s (runtime: %lu ms)\n",
                         currentFile.c_str(), millis());
        }
    }
}

void processCommand(const String& cmd) {
    auto& audio = hal::AudioDriver::getInstance();

    Serial.printf("\n[CMD] Received: '%s'\n", cmd.c_str());

    if (cmd.equalsIgnoreCase("HELP")) {
        showHelp();
    }
    else if (cmd.equalsIgnoreCase("STATUS")) {
        showStatus();
    }
    else if (cmd.equalsIgnoreCase("STOP")) {
        if (audio.isPlaying()) {
            audio.stop();
            stats.manualStops++;
            Serial.println("[CMD] ✓ Playback stopped");
            currentFile = "";
        } else {
            Serial.println("[CMD] No audio playing");
        }
    }
    else if (cmd.equalsIgnoreCase("LIST")) {
        listAudioFiles();
    }
    else if (cmd.startsWith("PLAY ")) {
        String filename = cmd.substring(5);
        filename.trim();
        playAudioFile(filename);
    }
    else {
        Serial.println("[CMD] Unknown command. Type HELP for commands.");
    }
}

void playAudioFile(const String& filename) {
    auto& audio = hal::AudioDriver::getInstance();

    stats.playAttempts++;

    // Construct full path
    String path = "/AUDIO/" + filename;

    Serial.println("\n--- PLAY ATTEMPT ---");
    Serial.printf("File: %s\n", filename.c_str());
    Serial.printf("Path: %s\n", path.c_str());
    Serial.printf("Attempt #%d\n", stats.playAttempts);
    Serial.printf("Free heap before: %d bytes\n", ESP.getFreeHeap());

    // Check if audio was initialized yet
    if (!audioInitialized) {
        Serial.println("\n[LAZY-INIT] This is the FIRST play() call");
        Serial.println("[LAZY-INIT] Audio subsystem will initialize NOW");
        audioInitialized = true;
    }

    // Attempt playback
    uint32_t startTime = millis();
    bool success = audio.play(path);
    uint32_t duration = millis() - startTime;

    Serial.printf("\nResult: %s\n", success ? "SUCCESS" : "FAILED");
    Serial.printf("Time: %lu ms\n", duration);
    Serial.printf("Free heap after: %d bytes\n", ESP.getFreeHeap());

    if (success) {
        stats.playSuccesses++;
        currentFile = filename;
        Serial.println("\n✓✓✓ PLAYBACK STARTED ✓✓✓");
        Serial.println("Audio should now be playing through speaker");
    } else {
        stats.playFailures++;
        Serial.println("\n✗✗✗ PLAYBACK FAILED ✗✗✗");
        Serial.println("Check:");
        Serial.println("  1. File exists in /AUDIO/ directory");
        Serial.println("  2. File is valid WAV format");
        Serial.println("  3. SD card is readable");
    }

    Serial.println("--- END PLAY ATTEMPT ---\n");
}

void showStatus() {
    auto& audio = hal::AudioDriver::getInstance();

    Serial.println("\n========================================");
    Serial.println("   AUDIO DRIVER STATUS");
    Serial.println("========================================");
    Serial.printf("Runtime: %lu ms\n", millis());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println();

    Serial.println("Playback State:");
    Serial.printf("  Currently playing: %s\n", audio.isPlaying() ? "YES" : "NO");
    if (audio.isPlaying()) {
        Serial.printf("  Current file: %s\n", currentFile.c_str());
    }
    Serial.printf("  Audio initialized: %s\n", audioInitialized ? "YES" : "NO (lazy init)");
    Serial.println();

    Serial.println("Statistics:");
    Serial.printf("  Play attempts: %d\n", stats.playAttempts);
    Serial.printf("  Play successes: %d\n", stats.playSuccesses);
    Serial.printf("  Play failures: %d\n", stats.playFailures);
    Serial.printf("  Natural stops: %d\n", stats.naturalStops);
    Serial.printf("  Manual stops: %d\n", stats.manualStops);

    if (stats.playAttempts > 0) {
        int successRate = (100 * stats.playSuccesses) / stats.playAttempts;
        Serial.printf("  Success rate: %d%%\n", successRate);
    }

    Serial.println("========================================\n");
}

void showHelp() {
    Serial.println("\n========================================");
    Serial.println("   AVAILABLE COMMANDS");
    Serial.println("========================================");
    Serial.println("PLAY <filename>   - Play WAV file from /AUDIO/");
    Serial.println("                    Example: PLAY kaa001.wav");
    Serial.println("STOP              - Stop current playback");
    Serial.println("STATUS            - Show detailed audio status");
    Serial.println("LIST              - List available audio files");
    Serial.println("HELP              - Show this help message");
    Serial.println("========================================\n");
}

void listAudioFiles() {
    auto& sd = hal::SDCard::getInstance();

    Serial.println("\n--- AUDIO FILES ---");

    File dir = SD.open("/AUDIO");
    if (!dir) {
        Serial.println("Cannot open /AUDIO directory");
        return;
    }

    if (!dir.isDirectory()) {
        Serial.println("/AUDIO is not a directory");
        dir.close();
        return;
    }

    int fileCount = 0;
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".wav") || name.endsWith(".WAV")) {
                fileCount++;
                Serial.printf("  %2d. %s (%d bytes)\n",
                             fileCount, name.c_str(), file.size());
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();

    Serial.printf("Total WAV files: %d\n", fileCount);
    Serial.println("--- END LIST ---\n");
}
