/**
 * @file 55-config-service.ino
 * @brief Test sketch for ConfigService HAL component
 *
 * Tests all ConfigService functionality:
 * - Load configuration from SD card (/config.txt)
 * - Validate configuration fields
 * - Runtime configuration editing (in-memory)
 * - Save configuration to SD card
 * - Device ID auto-generation from MAC
 *
 * Hardware Requirements:
 * - ESP32-2432S028R (CYD)
 * - SD card inserted with config.txt file
 *
 * Serial Commands:
 * - LOAD        - Load configuration from SD card
 * - SAVE        - Save current configuration to SD card
 * - SHOW        - Display current configuration
 * - VALIDATE    - Validate current configuration
 * - GENID       - Generate device ID from MAC
 * - SET:KEY=VAL - Set configuration value (e.g., SET:WIFI_SSID=MyNetwork)
 * - MEM         - Show memory usage
 * - HELP        - Show all commands
 *
 * Test Procedure:
 * 1. Upload sketch to CYD device
 * 2. Open serial monitor (115200 baud)
 * 3. Send LOAD command to load config.txt
 * 4. Send SHOW command to view configuration
 * 5. Send VALIDATE command to check validity
 * 6. Send SET:TEAM_ID=999 to test runtime editing
 * 7. Send SAVE command to persist changes
 * 8. Send GENID command to test MAC-based ID generation
 *
 * Expected Results:
 * - All commands execute without errors
 * - Configuration loads from SD card successfully
 * - Validation catches missing/invalid fields
 * - Runtime editing updates in-memory config
 * - Save persists changes to SD card
 * - Device ID generation produces SCANNER_XXXXXXXXXXXX format
 *
 * Flash Usage Target: < 400KB
 *
 * Version: 1.0
 * Date: 2025-10-22
 */

#include "../../ALNScanner_v5/hal/SDCard.h"
#include "../../ALNScanner_v5/services/ConfigService.h"

// ============================================================================
// SERIAL COMMAND HANDLING
// ============================================================================

void processSerialCommand(const String& cmd) {
    if (cmd == "LOAD") {
        Serial.println("\n========================================");
        Serial.println("       LOAD CONFIG FROM SD CARD");
        Serial.println("========================================");

        bool success = services::ConfigService::getInstance().loadFromSD();

        if (success) {
            Serial.println("\n+++ CONFIG LOADED SUCCESSFULLY +++");
            Serial.println("\nUse 'SHOW' to view configuration");
        } else {
            Serial.println("\nXXX CONFIG LOAD FAILED XXX");
            Serial.println("Check that config.txt exists on SD card");
        }
        Serial.println("========================================\n");

    } else if (cmd == "SAVE") {
        Serial.println("\n========================================");
        Serial.println("       SAVE CONFIG TO SD CARD");
        Serial.println("========================================");

        bool success = services::ConfigService::getInstance().saveToSD();

        if (success) {
            Serial.println("\n+++ CONFIG SAVED SUCCESSFULLY +++");
            Serial.println("Changes written to /config.txt");
        } else {
            Serial.println("\nXXX CONFIG SAVE FAILED XXX");
            Serial.println("Check SD card is present and writable");
        }
        Serial.println("========================================\n");

    } else if (cmd == "SHOW") {
        Serial.println("\n========================================");
        Serial.println("       CURRENT CONFIGURATION");
        Serial.println("========================================");

        auto& config = services::ConfigService::getInstance().getConfig();

        Serial.printf("WIFI_SSID: %s\n", config.wifiSSID.length() > 0 ? config.wifiSSID.c_str() : "(not set)");
        Serial.printf("WIFI_PASSWORD: %s\n", config.wifiPassword.length() > 0 ? "***" : "(not set)");
        Serial.printf("ORCHESTRATOR_URL: %s\n", config.orchestratorURL.length() > 0 ? config.orchestratorURL.c_str() : "(not set)");
        Serial.printf("TEAM_ID: %s\n", config.teamID.length() > 0 ? config.teamID.c_str() : "(not set)");
        Serial.printf("DEVICE_ID: %s\n", config.deviceID.length() > 0 ? config.deviceID.c_str() : "(auto-generate)");
        Serial.printf("SYNC_TOKENS: %s\n", config.syncTokens ? "true" : "false");
        Serial.printf("DEBUG_MODE: %s\n", config.debugMode ? "true" : "false");

        Serial.println("========================================\n");

    } else if (cmd == "VALIDATE") {
        Serial.println("\n========================================");
        Serial.println("       VALIDATE CONFIGURATION");
        Serial.println("========================================");

        bool isValid = services::ConfigService::getInstance().validate();

        if (isValid) {
            Serial.println("\n+++ CONFIGURATION IS VALID +++");
        } else {
            Serial.println("\nXXX CONFIGURATION HAS ERRORS XXX");
            Serial.println("See validation output above for details");
        }
        Serial.println("========================================\n");

    } else if (cmd == "GENID") {
        Serial.println("\n========================================");
        Serial.println("       GENERATE DEVICE ID");
        Serial.println("========================================");

        String deviceId = services::ConfigService::getInstance().generateDeviceId();

        Serial.printf("\nGenerated Device ID: %s\n", deviceId.c_str());
        Serial.println("\nThis ID is derived from ESP32 MAC address");
        Serial.println("Use 'SET:DEVICE_ID=<value>' to set custom ID");
        Serial.println("========================================\n");

    } else if (cmd.startsWith("SET:")) {
        String kvPair = cmd.substring(4); // Remove "SET:"
        kvPair.trim();

        int sepIndex = kvPair.indexOf('=');
        if (sepIndex == -1) {
            Serial.println("\nERROR: Invalid format");
            Serial.println("Usage: SET:KEY=VALUE");
            Serial.println("Example: SET:WIFI_SSID=MyNetwork");
            Serial.println("\nAvailable keys:");
            Serial.println("  WIFI_SSID, WIFI_PASSWORD, ORCHESTRATOR_URL");
            Serial.println("  TEAM_ID, DEVICE_ID, SYNC_TOKENS, DEBUG_MODE\n");
            return;
        }

        String key = kvPair.substring(0, sepIndex);
        String value = kvPair.substring(sepIndex + 1);
        key.trim();
        value.trim();

        Serial.println("\n========================================");
        Serial.println("       SET CONFIG (MEMORY ONLY)");
        Serial.println("========================================");
        Serial.printf("Key: %s\n", key.c_str());
        Serial.printf("Value: %s\n", value.c_str());
        Serial.println("");

        bool success = services::ConfigService::getInstance().set(key, value);

        if (success) {
            Serial.printf("+++ %s updated successfully\n", key.c_str());
            Serial.println("\nChanges are in MEMORY ONLY");
            Serial.println("Use 'SAVE' to persist to SD card");
        } else {
            Serial.printf("XXX Unknown key: %s\n", key.c_str());
            Serial.println("\nAvailable keys:");
            Serial.println("  WIFI_SSID, WIFI_PASSWORD, ORCHESTRATOR_URL");
            Serial.println("  TEAM_ID, DEVICE_ID, SYNC_TOKENS, DEBUG_MODE");
        }
        Serial.println("========================================\n");

    } else if (cmd == "MEM") {
        Serial.println("\n========================================");
        Serial.println("       MEMORY USAGE");
        Serial.println("========================================");
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("Heap size: %d bytes\n", ESP.getHeapSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
        Serial.println("========================================\n");

    } else if (cmd == "HELP") {
        Serial.println("\n========================================");
        Serial.println("       AVAILABLE COMMANDS");
        Serial.println("========================================");
        Serial.println("LOAD        - Load config from SD card");
        Serial.println("SAVE        - Save config to SD card");
        Serial.println("SHOW        - Display current config");
        Serial.println("VALIDATE    - Validate config fields");
        Serial.println("GENID       - Generate device ID from MAC");
        Serial.println("SET:KEY=VAL - Set config value");
        Serial.println("              Example: SET:TEAM_ID=123");
        Serial.println("MEM         - Show memory usage");
        Serial.println("HELP        - Show this help");
        Serial.println("========================================\n");

    } else {
        Serial.printf("\nUnknown command: %s\n", cmd.c_str());
        Serial.println("Type HELP for available commands\n");
    }
}

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   ConfigService Test Sketch v1.0");
    Serial.println("   ALNScanner v5.0 HAL Component Test");
    Serial.println("========================================");
    Serial.println("");

    // Step 1: Initialize SD card HAL
    Serial.println("[INIT] Initializing SD card HAL...");
    auto& sd = hal::SDCard::getInstance();

    if (!sd.begin()) {
        Serial.println("\n*** CRITICAL ERROR ***");
        Serial.println("SD card initialization failed!");
        Serial.println("Check that SD card is inserted properly");
        Serial.println("Test cannot proceed without SD card");
        Serial.println("*** HALTING ***\n");
        while (1) {
            delay(1000);
        }
    }

    Serial.println("[INIT] SD card initialized successfully\n");

    // Step 2: Show initial memory state
    Serial.println("[INIT] Initial memory state:");
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  Heap size: %d bytes\n", ESP.getHeapSize());
    Serial.println("");

    // Step 3: Initialize ConfigService (singleton, no explicit init needed)
    Serial.println("[INIT] ConfigService ready (singleton pattern)");
    Serial.println("");

    // Step 4: Show help
    Serial.println("========================================");
    Serial.println("   READY FOR COMMANDS");
    Serial.println("========================================");
    Serial.println("Type HELP for available commands");
    Serial.println("Type LOAD to load config from SD card");
    Serial.println("========================================\n");
}

// ============================================================================
// ARDUINO LOOP
// ============================================================================

void loop() {
    // Check for serial input
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.length() > 0) {
            processSerialCommand(cmd);
        }
    }

    delay(10);
}
