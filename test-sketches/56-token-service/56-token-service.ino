/**
 * @file 56-token-service.ino
 * @brief Test sketch for TokenService (ALNScanner v5.0)
 *
 * Tests token database loading, lookup, and debug methods.
 * Validates integration with hal::SDCard and models::TokenMetadata.
 *
 * Hardware Requirements:
 * - ESP32-2432S028R (CYD)
 * - SD card with tokens.json (optional)
 *
 * Expected Behavior:
 * 1. Initialize SD card
 * 2. Load token database from SD
 * 3. Print database contents (first 5 tokens)
 * 4. Query specific token by ID
 * 5. Test exists() method
 *
 * Serial Commands (optional):
 * - TOKENS - Print full database
 * - QUERY:tokenId - Query specific token
 * - CLEAR - Clear database
 * - RELOAD - Reload from SD
 */

#include "../../ALNScanner_v5/config.h"
#include "../../ALNScanner_v5/hal/SDCard.h"
#include "../../ALNScanner_v5/models/Token.h"
#include "../../ALNScanner_v5/services/TokenService.h"

// Test configuration
const char* TEST_TOKEN_IDS[] = {"kaa001", "jaw001", "04A1B2C3", "invalid_token"};
const int TEST_TOKEN_COUNT = 4;

void printSeparator() {
    Serial.println("═══════════════════════════════════════════════════════");
}

void testTokenQuery(const char* tokenId) {
    auto& tokenSvc = services::TokenService::getInstance();

    Serial.printf("\n[TEST] Querying token: %s\n", tokenId);

    // Test get()
    const auto* token = tokenSvc.get(tokenId);
    if (token) {
        Serial.println("✓ Token found!");
        token->print();
    } else {
        Serial.printf("⚠ Token not found: %s\n", tokenId);
    }

    // Test exists()
    Serial.printf("Token exists: %s\n", tokenSvc.exists(tokenId) ? "YES" : "NO");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    printSeparator();
    Serial.println("    TokenService Test - ALNScanner v5.0");
    printSeparator();

    // Step 1: Initialize SD card
    Serial.println("\n[STEP 1] Initializing SD card...");
    auto& sd = hal::SDCard::getInstance();
    if (!sd.begin()) {
        Serial.println("✗ SD card not available");
        Serial.println("\nTest FAILED: No SD card detected");
        Serial.println("Insert SD card and reset to retry");
        return;
    }
    Serial.println("✓ SD card initialized");

    // Step 2: Load token database
    Serial.println("\n[STEP 2] Loading token database...");
    auto& tokenSvc = services::TokenService::getInstance();

    if (tokenSvc.loadDatabaseFromSD()) {
        Serial.printf("✓ Token database loaded (%d tokens)\n", tokenSvc.getCount());

        // Print first 5 tokens
        Serial.println("\n[STEP 3] Database contents (first 5 tokens):");
        tokenSvc.printDatabase(5);
    } else {
        Serial.println("⚠ Token database not found (OK for test - will use empty database)");
        Serial.println("\nTo test with real data:");
        Serial.println("1. Create /tokens.json on SD card");
        Serial.println("2. Format: {\"tokens\": {\"kaa001\": {...}}}");
        Serial.println("3. Reset device");
    }

    // Step 4: Test token queries
    Serial.println("\n[STEP 4] Testing token queries...");
    printSeparator();

    for (int i = 0; i < TEST_TOKEN_COUNT; i++) {
        testTokenQuery(TEST_TOKEN_IDS[i]);
    }

    // Step 5: Summary
    printSeparator();
    Serial.println("\n[SUMMARY]");
    Serial.printf("Total tokens in database: %d\n", tokenSvc.getCount());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    printSeparator();
    Serial.println("\n✓ Test Complete");
    Serial.println("\nSerial Commands Available:");
    Serial.println("  TOKENS       - Print full database");
    Serial.println("  QUERY:id     - Query specific token (e.g., QUERY:kaa001)");
    Serial.println("  CLEAR        - Clear database");
    Serial.println("  RELOAD       - Reload from SD");
    Serial.println("  HELP         - Show this help");
    printSeparator();
}

void loop() {
    // Check for serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        auto& tokenSvc = services::TokenService::getInstance();

        if (cmd == "TOKENS") {
            Serial.println("\n[CMD] Printing token database...");
            tokenSvc.printDatabase(100);  // Show all tokens

        } else if (cmd.startsWith("QUERY:")) {
            String tokenId = cmd.substring(6);
            tokenId.trim();
            Serial.printf("\n[CMD] Querying token: %s\n", tokenId.c_str());
            testTokenQuery(tokenId.c_str());

        } else if (cmd == "CLEAR") {
            Serial.println("\n[CMD] Clearing database...");
            tokenSvc.clear();
            Serial.printf("Token count: %d\n", tokenSvc.getCount());

        } else if (cmd == "RELOAD") {
            Serial.println("\n[CMD] Reloading database from SD...");
            if (tokenSvc.loadDatabaseFromSD()) {
                Serial.printf("✓ Loaded %d tokens\n", tokenSvc.getCount());
                tokenSvc.printDatabase(5);
            } else {
                Serial.println("✗ Failed to load database");
            }

        } else if (cmd == "HELP") {
            Serial.println("\n[HELP] Serial Commands:");
            Serial.println("  TOKENS       - Print full database");
            Serial.println("  QUERY:id     - Query specific token (e.g., QUERY:kaa001)");
            Serial.println("  CLEAR        - Clear database");
            Serial.println("  RELOAD       - Reload from SD");
            Serial.println("  HELP         - Show this help");

        } else {
            Serial.printf("\n[ERROR] Unknown command: %s\n", cmd.c_str());
            Serial.println("Type HELP for available commands");
        }
    }

    delay(100);
}
