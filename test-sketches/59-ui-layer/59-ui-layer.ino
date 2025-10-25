// ═══════════════════════════════════════════════════════════════════
// UI Layer Integration Test - ALNScanner v5.0
// Tests all UI components: Screen base + 4 screens + UIStateMachine
// ═══════════════════════════════════════════════════════════════════

#include "../../ALNScanner_v5/config.h"
#include "../../ALNScanner_v5/hal/SDCard.h"
#include "../../ALNScanner_v5/hal/DisplayDriver.h"
#include "../../ALNScanner_v5/hal/AudioDriver.h"
#include "../../ALNScanner_v5/hal/TouchDriver.h"
#include "../../ALNScanner_v5/hal/RFIDReader.h"
#include "../../ALNScanner_v5/models/ConnectionState.h"
#include "../../ALNScanner_v5/models/Config.h"
#include "../../ALNScanner_v5/models/Token.h"
#include "../../ALNScanner_v5/ui/UIStateMachine.h"

// Test state
uint32_t lastScreenChange = 0;
uint32_t testStartTime = 0;
int testPhase = 0;

// UI State Machine (created after HAL initialization)
ui::UIStateMachine* uiStateMachine = nullptr;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║  ALNScanner v5.0 - UI Layer Integration Test            ║");
    Serial.println("║  Testing: Screen + UIStateMachine + 4 Screen types      ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝\n");

    // Initialize HAL
    Serial.println("[TEST] Initializing HAL components...");

    auto& sd = hal::SDCard::getInstance();
    if (!sd.begin()) {
        Serial.println("  ⚠️  SD card not available (optional)");
    } else {
        Serial.println("  ✓ SD card initialized");
    }

    auto& display = hal::DisplayDriver::getInstance();
    if (!display.begin()) {
        Serial.println("  ✗ Display initialization failed!");
        return;
    }
    Serial.println("  ✓ Display initialized");

    auto& touch = hal::TouchDriver::getInstance();
    if (!touch.begin()) {
        Serial.println("  ✗ Touch initialization failed!");
        return;
    }
    Serial.println("  ✓ Touch initialized");

    auto& audio = hal::AudioDriver::getInstance();
    audio.silenceDAC();
    Serial.println("  ✓ Audio driver ready\n");

    // Initialize UIStateMachine
    Serial.println("[TEST] Creating UIStateMachine...");
    uiStateMachine = new ui::UIStateMachine(display, touch, audio, sd);
    Serial.println("  ✓ UIStateMachine created\n");

    // Test Phase 1: Ready Screen
    Serial.println("╔═══════════════════════════════════════════════╗");
    Serial.println("║ PHASE 1: Testing Ready Screen                ║");
    Serial.println("╚═══════════════════════════════════════════════╝");

    uiStateMachine->showReady(true, false);
    Serial.println("  ✓ Ready screen displayed (RFID ready, no debug)");
    Serial.printf("  State: %d (should be 0 = READY)\n", (int)uiStateMachine->getState());
    Serial.println("  Visual: Should show 'Ready to Scan' with green indicator\n");

    testStartTime = millis();
    testPhase = 1;
    lastScreenChange = millis();

    Serial.println("Interactive Commands:");
    Serial.println("  1 - Show Ready Screen (RFID ready)");
    Serial.println("  2 - Show Ready Screen (DEBUG mode)");
    Serial.println("  3 - Show Status Screen");
    Serial.println("  4 - Show Token Display Screen");
    Serial.println("  5 - Show Processing Screen");
    Serial.println("  TOUCH - Test touch handling");
    Serial.println("  AUTO - Run automated screen cycle");
    Serial.println("  HELP - Show commands\n");

    Serial.println("Touch the screen or send a command to test...\n");
}

void loop() {
    if (!uiStateMachine) return;  // Safety check

    // Update UI state machine
    uiStateMachine->update();

    // Handle touch events
    uiStateMachine->handleTouch();

    // Check for automated test progression
    if (testPhase > 0 && (millis() - lastScreenChange > 5000)) {
        testPhase++;
        lastScreenChange = millis();

        switch (testPhase) {
            case 2:
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║ PHASE 2: Testing Debug Mode Ready Screen     ║");
                Serial.println("╚═══════════════════════════════════════════════╝");
                uiStateMachine->showReady(false, true);
                Serial.println("  ✓ Debug mode ready screen displayed");
                Serial.println("  Visual: Should show 'DEBUG MODE' with red warning\n");
                break;

            case 3:
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║ PHASE 3: Testing Status Screen               ║");
                Serial.println("╚═══════════════════════════════════════════════╝");
                {
                    ui::StatusScreen::SystemStatus status;
                    status.connState = models::ORCH_WIFI_CONNECTED;
                    status.wifiSSID = "TestNetwork";
                    status.localIP = "192.168.1.100";
                    status.queueSize = 5;
                    status.maxQueueSize = 100;
                    status.teamID = "001";
                    status.deviceID = "SCANNER_TEST_001";

                    uiStateMachine->showStatus(status);
                    Serial.println("  ✓ Status screen displayed");
                    Serial.println("  Visual: Should show WiFi, queue, team info\n");
                }
                break;

            case 4:
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║ PHASE 4: Testing Token Display Screen        ║");
                Serial.println("╚═══════════════════════════════════════════════╝");
                {
                    models::TokenMetadata token;
                    token.tokenId = "test001";
                    token.video = "";  // Regular token
                    token.image = "images/test001.bmp";
                    token.audio = "audio/test001.wav";

                    uiStateMachine->showToken(token);
                    Serial.println("  ✓ Token display screen shown");
                    Serial.println("  Visual: Should show token image (or 'Missing' if not on SD)\n");
                }
                break;

            case 5:
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║ PHASE 5: Testing Processing Screen           ║");
                Serial.println("╚═══════════════════════════════════════════════╝");
                uiStateMachine->showProcessing("/images/processing.bmp");
                Serial.println("  ✓ Processing screen displayed");
                Serial.println("  Visual: Should show image + 'Sending...' overlay");
                Serial.println("  Auto-dismiss in 2.5 seconds...\n");
                break;

            case 6:
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║ TEST CYCLE COMPLETE                           ║");
                Serial.println("╚═══════════════════════════════════════════════╝");
                Serial.printf("Total test time: %lu ms\n", millis() - testStartTime);
                Serial.println("\n✓✓✓ ALL UI COMPONENTS TESTED ✓✓✓");
                Serial.println("\nSend 'AUTO' to repeat test cycle");
                testPhase = 0;  // Stop auto-progression
                break;
        }
    }

    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "1") {
            Serial.println("\n→ Showing Ready Screen (RFID ready)");
            uiStateMachine->showReady(true, false);

        } else if (cmd == "2") {
            Serial.println("\n→ Showing Ready Screen (DEBUG mode)");
            uiStateMachine->showReady(false, true);

        } else if (cmd == "3") {
            Serial.println("\n→ Showing Status Screen");
            ui::StatusScreen::SystemStatus status;
            status.connState = models::ORCH_CONNECTED;
            status.wifiSSID = "TestWiFi";
            status.localIP = "192.168.1.50";
            status.queueSize = 0;
            status.maxQueueSize = 100;
            status.teamID = "042";
            status.deviceID = "SCANNER_DEMO";
            uiStateMachine->showStatus(status);

        } else if (cmd == "4") {
            Serial.println("\n→ Showing Token Display Screen");
            models::TokenMetadata token;
            token.tokenId = "demo_token";
            token.video = "";
            token.image = "images/demo.bmp";
            token.audio = "audio/demo.wav";
            uiStateMachine->showToken(token);

        } else if (cmd == "5") {
            Serial.println("\n→ Showing Processing Screen");
            uiStateMachine->showProcessing("/images/processing.bmp");

        } else if (cmd == "TOUCH") {
            Serial.println("\n→ Testing touch handling");
            Serial.println("Touch the screen to trigger state transitions...");

        } else if (cmd == "AUTO") {
            Serial.println("\n→ Starting automated test cycle");
            testPhase = 1;
            lastScreenChange = millis();
            testStartTime = millis();
            uiStateMachine->showReady(true, false);

        } else if (cmd == "HELP") {
            Serial.println("\n=== Available Commands ===");
            Serial.println("  1 - Show Ready Screen (RFID ready)");
            Serial.println("  2 - Show Ready Screen (DEBUG mode)");
            Serial.println("  3 - Show Status Screen");
            Serial.println("  4 - Show Token Display Screen");
            Serial.println("  5 - Show Processing Screen");
            Serial.println("  TOUCH - Test touch handling");
            Serial.println("  AUTO - Run automated screen cycle");
            Serial.println("  HELP - Show this help");
            Serial.println("==========================\n");
        } else {
            Serial.printf("Unknown command: %s (send HELP for commands)\n", cmd.c_str());
        }
    }

    delay(10);
}
