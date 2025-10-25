/**
 * Test Sketch: RFIDReader HAL Component
 *
 * Tests:
 * - Initialization with GPIO 3 conflict warning
 * - Card detection (REQA + cascade selection)
 * - 7-byte UID support (NTAG cards)
 * - NDEF text extraction
 * - Statistics tracking
 * - RF field enable/disable control
 * - Beeping mitigation (500ms scan interval)
 *
 * Serial Commands:
 * - STATUS - Show statistics
 * - RESET_STATS - Reset statistics counters
 * - FIELD_ON - Enable RF field
 * - FIELD_OFF - Disable RF field
 * - HELP - Show all commands
 *
 * Hardware: ESP32-2432S028R (CYD)
 * RFID Reader: MFRC522 on Software SPI (GPIO 22/27/35/3)
 */

// Must use absolute paths from sketch directory
#include "../../ALNScanner_v5/config.h"
#include "../../ALNScanner_v5/hal/RFIDReader.h"

using namespace hal;

// Timing
uint32_t lastScanTime = 0;
const uint32_t SCAN_INTERVAL = 500; // 500ms - beeping mitigation

// Statistics display
uint32_t lastStatsDisplay = 0;
const uint32_t STATS_DISPLAY_INTERVAL = 10000; // 10 seconds

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("     RFID READER HAL TEST SKETCH");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("Testing: Software SPI + MFRC522 + NDEF extraction");
    Serial.println("");
    Serial.println("⚠️  CRITICAL GPIO 3 CONFLICT:");
    Serial.println("    GPIO 3 is shared between Serial RX and RFID_SS");
    Serial.println("    Initializing RFID will PERMANENTLY disable Serial RX!");
    Serial.println("");
    Serial.println("Press any key within 30 seconds to SKIP RFID init");
    Serial.println("(keeps serial commands working for testing)");
    Serial.println("");
    Serial.print("Waiting (30s): ");

    // 30-second override window
    bool skipRFID = false;
    uint32_t startTime = millis();
    while (millis() - startTime < 30000) {
        Serial.print(".");
        if (Serial.available()) {
            skipRFID = true;
            Serial.read(); // Consume input
            break;
        }
        delay(1000);
    }
    Serial.println();

    if (skipRFID) {
        Serial.println("\n✓ RFID initialization SKIPPED (Serial RX preserved)");
        Serial.println("Send 'INIT_RFID' command to initialize RFID later");
    } else {
        Serial.println("\n→ No input detected, initializing RFID...");
        initializeRFID();
    }

    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("Type 'HELP' for available commands");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

void loop() {
    handleSerialCommands();

    auto& rfid = RFIDReader::getInstance();

    if (!rfid.isInitialized()) {
        delay(100);
        return;
    }

    // Scan for cards at 500ms interval (beeping mitigation)
    if (millis() - lastScanTime >= SCAN_INTERVAL) {
        lastScanTime = millis();

        MFRC522::Uid uid;
        if (rfid.detectCard(uid)) {
            Serial.printf("\n[%lu] ═══ CARD DETECTED ═══\n", millis());

            // Display UID
            Serial.print("  UID: ");
            for (uint8_t i = 0; i < uid.size; i++) {
                if (uid.uidByte[i] < 0x10) Serial.print("0");
                Serial.print(uid.uidByte[i], HEX);
            }
            Serial.printf(" (%d bytes)\n", uid.size);
            Serial.printf("  SAK: 0x%02X\n", uid.sak);

            // Extract NDEF text
            String ndefText = rfid.extractNDEFText();
            if (ndefText.length() > 0) {
                Serial.printf("  NDEF: '%s'\n", ndefText.c_str());
            } else {
                Serial.println("  NDEF: (no text record found)");
            }

            // Display statistics
            const auto& stats = rfid.getStats();
            float successRate = (stats.totalScans > 0)
                ? (100.0 * stats.successfulScans / stats.totalScans)
                : 0.0;
            Serial.printf("  Success Rate: %.1f%% (%d/%d scans)\n",
                successRate, stats.successfulScans, stats.totalScans);

            Serial.println("═══════════════════════════════\n");
        }
    }

    // Periodic statistics display
    if (millis() - lastStatsDisplay >= STATS_DISPLAY_INTERVAL) {
        lastStatsDisplay = millis();
        displayStatistics();
    }
}

void initializeRFID() {
    auto& rfid = RFIDReader::getInstance();

    if (!rfid.begin()) {
        Serial.println("[ERROR] RFID initialization failed!");
        return;
    }

    Serial.println("✓ RFID initialized successfully");
    Serial.println("  Ready to scan cards (500ms interval)");
}

void handleSerialCommands() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    auto& rfid = RFIDReader::getInstance();

    if (cmd == "HELP") {
        Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("     AVAILABLE COMMANDS");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("INIT_RFID     - Initialize RFID reader (kills Serial RX)");
        Serial.println("STATUS        - Show current statistics");
        Serial.println("RESET_STATS   - Reset statistics counters");
        Serial.println("FIELD_ON      - Enable RF field (antenna)");
        Serial.println("FIELD_OFF     - Disable RF field");
        Serial.println("SILENCE_PINS  - Set MOSI LOW (minimize speaker coupling)");
        Serial.println("HELP          - Show this help");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    } else if (cmd == "INIT_RFID") {
        if (rfid.isInitialized()) {
            Serial.println("RFID already initialized");
        } else {
            initializeRFID();
        }

    } else if (cmd == "STATUS") {
        displayStatistics();

    } else if (cmd == "RESET_STATS") {
        rfid.resetStats();
        Serial.println("✓ Statistics reset");

    } else if (cmd == "FIELD_ON") {
        if (!rfid.isInitialized()) {
            Serial.println("[ERROR] RFID not initialized!");
            return;
        }
        rfid.enableRFField();
        Serial.println("✓ RF field enabled");

    } else if (cmd == "FIELD_OFF") {
        if (!rfid.isInitialized()) {
            Serial.println("[ERROR] RFID not initialized!");
            return;
        }
        rfid.disableRFField();
        Serial.println("✓ RF field disabled");

    } else if (cmd == "SILENCE_PINS") {
        if (!rfid.isInitialized()) {
            Serial.println("[ERROR] RFID not initialized!");
            return;
        }
        rfid.silenceSPIPins();
        Serial.println("✓ SPI pins silenced (MOSI LOW)");

    } else {
        Serial.printf("Unknown command: '%s' (type HELP for list)\n", cmd.c_str());
    }
}

void displayStatistics() {
    auto& rfid = RFIDReader::getInstance();

    if (!rfid.isInitialized()) {
        Serial.println("[STATUS] RFID not initialized");
        return;
    }

    const auto& stats = rfid.getStats();

    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("     RFID READER STATISTICS");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.printf("Total Scans:       %d\n", stats.totalScans);
    Serial.printf("Successful:        %d\n", stats.successfulScans);
    Serial.printf("Failed:            %d\n", stats.failedScans);

    if (stats.totalScans > 0) {
        float successRate = 100.0 * stats.successfulScans / stats.totalScans;
        Serial.printf("Success Rate:      %.1f%%\n", successRate);
    } else {
        Serial.println("Success Rate:      N/A");
    }

    Serial.println("\nError Breakdown:");
    Serial.printf("  Collisions:      %d\n", stats.collisionErrors);
    Serial.printf("  Timeouts:        %d\n", stats.timeoutErrors);
    Serial.printf("  CRC Errors:      %d\n", stats.crcErrors);
    Serial.printf("  Retries:         %d\n", stats.retryCount);

    Serial.println("\nSystem Info:");
    Serial.printf("  Free Heap:       %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  Uptime:          %lu seconds\n", millis() / 1000);
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}
