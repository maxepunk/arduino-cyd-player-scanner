/*
 * Test Sketch 40: JSON Token Database Parsing
 *
 * Purpose: Validate ArduinoJson parsing of token database
 *
 * Tests:
 * - Load tokens.json from SD card
 * - Parse JSON with ArduinoJson v7
 * - Filter fields (tokenId, video, image, audio, processingImage)
 * - Token lookup by tokenId
 * - Memory usage tracking
 *
 * Hardware: CYD ESP32 Display (ST7789)
 *
 * Prerequisites:
 * - SD card with /tokens.json file
 * - Sample file in test-sketches/sample-tokens.json
 *
 * Expected Results:
 * - Parse tokens.json in <2s
 * - Memory usage <20KB for filtered fields
 * - Token lookup working correctly
 * - Display token count on TFT
 */

#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

// SD Card pins (Hardware SPI - VSPI)
#define SD_CS 5

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Token metadata struct (filtered fields only)
struct TokenMetadata {
  String tokenId;
  String video;        // null if not video token
  String image;        // path to image file
  String audio;        // path to audio file
  String processingImage; // path to processing image (for video tokens)
  bool isValid;

  TokenMetadata() : isValid(false) {}
};

// Token database (in-memory map)
// For simplicity, using fixed array (real implementation would use std::map or hash map)
#define MAX_TOKENS 50
TokenMetadata tokenDatabase[MAX_TOKENS];
int tokenCount = 0;

// Statistics
unsigned long parseStartTime = 0;
unsigned long parseDuration = 0;
size_t jsonFileSize = 0;
size_t memoryUsed = 0;

// Forward declarations
void displayStatus(String message, uint16_t color);
bool loadTokenDatabase();
TokenMetadata* getTokenMetadata(String tokenId);
void printTokenStats();
void printTokenDetails(String tokenId);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Test 40: JSON Token Database Parsing ===");

  // Initialize TFT display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  displayStatus("Initializing...", TFT_YELLOW);

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR: SD Card Mount Failed");
    displayStatus("SD Card Error", TFT_RED);
    while (1) delay(1000);
  }

  Serial.println("SD Card mounted successfully");

  // Load and parse token database
  displayStatus("Loading tokens.json...", TFT_YELLOW);

  parseStartTime = millis();
  bool success = loadTokenDatabase();
  parseDuration = millis() - parseStartTime;

  if (!success) {
    Serial.println("ERROR: Failed to load token database");
    displayStatus("Token DB Error", TFT_RED);
    while (1) delay(1000);
  }

  Serial.printf("\nToken database loaded successfully!\n");
  Serial.printf("Parse duration: %lu ms\n", parseDuration);
  Serial.printf("Tokens loaded: %d\n", tokenCount);
  Serial.printf("Memory used (estimated): %zu bytes\n", memoryUsed);

  // Display success on TFT
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 20);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(3);
  tft.println("Tokens Loaded!");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 60);
  tft.printf("Count: %d", tokenCount);

  tft.setCursor(10, 90);
  tft.printf("Time: %lu ms", parseDuration);

  tft.setCursor(10, 120);
  tft.printf("Memory: %zu bytes", memoryUsed);

  tft.setCursor(10, 150);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("Test PASSED!");

  Serial.println("\nType HELP for commands\n");
}

void loop() {
  // Serial command interface
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "STATS") {
      printTokenStats();

    } else if (cmd == "LIST") {
      Serial.println("\n=== All Tokens ===");
      for (int i = 0; i < tokenCount; i++) {
        Serial.printf("%d. %s", i + 1, tokenDatabase[i].tokenId.c_str());
        if (tokenDatabase[i].video.length() > 0 && tokenDatabase[i].video != "null") {
          Serial.print(" [VIDEO]");
        }
        Serial.println();
      }
      Serial.println("==================\n");

    } else if (cmd.startsWith("LOOKUP ")) {
      String tokenId = cmd.substring(7);
      tokenId.trim();
      printTokenDetails(tokenId);

    } else if (cmd == "MEM") {
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      Serial.printf("Heap size: %d bytes\n", ESP.getHeapSize());
      Serial.printf("Token DB memory: %zu bytes\n", memoryUsed);

    } else if (cmd == "HELP") {
      Serial.println("\nAvailable commands:");
      Serial.println("  STATS        - Show token database statistics");
      Serial.println("  LIST         - List all token IDs");
      Serial.println("  LOOKUP <id>  - Show details for specific token");
      Serial.println("  MEM          - Show memory usage");
      Serial.println("  HELP         - Show this help");
      Serial.println("\nExample: LOOKUP 534e2b02");
    }
  }

  delay(100);
}

/**
 * Load token database from SD card /tokens.json
 * Parse with ArduinoJson, filter only needed fields
 */
bool loadTokenDatabase() {
  // Open tokens.json file
  File file = SD.open("/tokens.json", FILE_READ);
  if (!file) {
    Serial.println("ERROR: /tokens.json not found on SD card");
    Serial.println("Please copy test-sketches/sample-tokens.json to SD card as /tokens.json");
    return false;
  }

  jsonFileSize = file.size();
  Serial.printf("tokens.json file size: %zu bytes\n", jsonFileSize);

  if (jsonFileSize > 50000) {
    Serial.println("WARNING: Token database >50KB, may not fit in RAM");
  }

  // Allocate JsonDocument
  // Size: estimate 1KB per token for full JSON, filtered parsing uses less
  JsonDocument doc;

  // Parse JSON
  Serial.println("Parsing JSON...");
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("ERROR: JSON parse failed: %s\n", error.c_str());
    return false;
  }

  Serial.println("JSON parsed successfully");

  // Extract tokens from root object
  JsonObject root = doc.as<JsonObject>();

  tokenCount = 0;
  for (JsonPair kv : root) {
    if (tokenCount >= MAX_TOKENS) {
      Serial.println("WARNING: Token database exceeds MAX_TOKENS limit");
      break;
    }

    const char* tokenId = kv.key().c_str();
    JsonObject tokenObj = kv.value().as<JsonObject>();

    // Parse only filtered fields
    TokenMetadata &token = tokenDatabase[tokenCount];
    token.tokenId = String(tokenId);

    // Extract video field (null or string)
    if (tokenObj["video"].isNull()) {
      token.video = "";
    } else {
      token.video = tokenObj["video"].as<String>();
    }

    // Extract image field
    if (tokenObj["image"].isNull()) {
      token.image = "";
    } else {
      token.image = tokenObj["image"].as<String>();
    }

    // Extract audio field
    if (tokenObj["audio"].isNull()) {
      token.audio = "";
    } else {
      token.audio = tokenObj["audio"].as<String>();
    }

    // Extract processingImage field
    if (tokenObj["processingImage"].isNull()) {
      token.processingImage = "";
    } else {
      token.processingImage = tokenObj["processingImage"].as<String>();
    }

    token.isValid = true;
    tokenCount++;

    // Estimate memory usage (rough calculation)
    memoryUsed += sizeof(TokenMetadata);
    memoryUsed += token.tokenId.length();
    memoryUsed += token.video.length();
    memoryUsed += token.image.length();
    memoryUsed += token.audio.length();
    memoryUsed += token.processingImage.length();
  }

  Serial.printf("Loaded %d tokens\n", tokenCount);
  return true;
}

/**
 * Get token metadata by tokenId
 */
TokenMetadata* getTokenMetadata(String tokenId) {
  for (int i = 0; i < tokenCount; i++) {
    if (tokenDatabase[i].tokenId.equalsIgnoreCase(tokenId)) {
      return &tokenDatabase[i];
    }
  }
  return nullptr;
}

/**
 * Print token database statistics
 */
void printTokenStats() {
  Serial.println("\n=== Token Database Statistics ===");
  Serial.printf("Total tokens: %d\n", tokenCount);
  Serial.printf("JSON file size: %zu bytes\n", jsonFileSize);
  Serial.printf("Parse duration: %lu ms\n", parseDuration);
  Serial.printf("Memory used: %zu bytes (~%.1f KB)\n", memoryUsed, memoryUsed / 1024.0);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  // Count token types
  int videoTokens = 0;
  int imageTokens = 0;
  int audioTokens = 0;

  for (int i = 0; i < tokenCount; i++) {
    if (tokenDatabase[i].video.length() > 0) videoTokens++;
    if (tokenDatabase[i].image.length() > 0) imageTokens++;
    if (tokenDatabase[i].audio.length() > 0) audioTokens++;
  }

  Serial.printf("\nToken Types:\n");
  Serial.printf("  Video tokens: %d\n", videoTokens);
  Serial.printf("  Image tokens: %d\n", imageTokens);
  Serial.printf("  Audio tokens: %d\n", audioTokens);
  Serial.println("==================================\n");
}

/**
 * Print details for specific token
 */
void printTokenDetails(String tokenId) {
  Serial.printf("\n=== Token Details: %s ===\n", tokenId.c_str());

  TokenMetadata* token = getTokenMetadata(tokenId);

  if (token == nullptr) {
    Serial.println("ERROR: Token not found in database");
    Serial.println("Use LIST command to see available tokens");
    Serial.println("=============================\n");
    return;
  }

  Serial.printf("Token ID: %s\n", token->tokenId.c_str());
  Serial.printf("Video: %s\n", token->video.length() > 0 ? token->video.c_str() : "(none)");
  Serial.printf("Image: %s\n", token->image.length() > 0 ? token->image.c_str() : "(none)");
  Serial.printf("Audio: %s\n", token->audio.length() > 0 ? token->audio.c_str() : "(none)");
  Serial.printf("Processing Image: %s\n", token->processingImage.length() > 0 ? token->processingImage.c_str() : "(none)");

  // Determine token type
  if (token->video.length() > 0) {
    Serial.println("Type: VIDEO TOKEN");
  } else if (token->image.length() > 0 || token->audio.length() > 0) {
    Serial.println("Type: LOCAL CONTENT TOKEN");
  } else {
    Serial.println("Type: UNKNOWN (no media)");
  }

  Serial.println("=============================\n");
}

/**
 * Display status message on TFT
 */
void displayStatus(String message, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 60);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(2);
  tft.println(message);
}
