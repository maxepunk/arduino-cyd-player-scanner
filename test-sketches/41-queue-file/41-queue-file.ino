/*
 * Test Sketch 41: Queue File Operations
 *
 * Purpose: Validate JSONL queue file operations
 *
 * Tests:
 * - Append scan entries to /queue.jsonl
 * - Read batch of entries (up to 10)
 * - Remove uploaded entries (FIFO)
 * - Count queue size
 * - FIFO overflow handling (100 entry limit)
 * - Queue recovery from corrupt JSON lines
 *
 * Hardware: CYD ESP32 Display (ST7789)
 *
 * Prerequisites:
 * - SD card mounted
 *
 * Expected Results:
 * - Append 100 entries successfully
 * - Overflow removes oldest entry (FIFO)
 * - Batch read returns up to 10 entries
 * - Remove entries works correctly
 * - Corrupt lines skipped gracefully
 * - Queue persists through reboot
 */

#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <vector>

// SD Card pins (Hardware SPI - VSPI)
#define SD_CS 5
#define MAX_QUEUE_SIZE 100

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Queue entry struct
struct QueueEntry {
  String tokenId;
  String teamId;
  String deviceId;
  String timestamp;
};

// Statistics
int appendCount = 0;
int readCount = 0;
int removeCount = 0;
int overflowCount = 0;
int corruptLinesSkipped = 0;

// Forward declarations
void displayStatus(String message, uint16_t color);
void queueScan(String tokenId, String teamId, String deviceId, String timestamp);
void readQueue(std::vector<QueueEntry>& entries, int maxEntries = 10);
void removeUploadedEntries(int numEntries);
int countQueueEntries();
void removeOldestEntry();
String generateTimestamp();
void printQueueStats();
void testAppend();
void testRead();
void testRemove();
void testOverflow();
void testCorruptRecovery();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Test 41: Queue File Operations ===");

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

  // Clean start: remove existing queue file
  if (SD.exists("/queue.jsonl")) {
    SD.remove("/queue.jsonl");
    Serial.println("Removed existing queue.jsonl");
  }

  displayStatus("Queue Tests Ready", TFT_GREEN);

  Serial.println("\nType HELP for commands\n");
}

void loop() {
  // Serial command interface
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "APPEND") {
      testAppend();

    } else if (cmd == "READ") {
      testRead();

    } else if (cmd == "REMOVE") {
      Serial.print("Enter number of entries to remove: ");
      while (!Serial.available()) delay(10);
      int num = Serial.parseInt();
      Serial.println(num);
      testRemove();

    } else if (cmd == "COUNT") {
      int count = countQueueEntries();
      Serial.printf("\nQueue size: %d entries\n", count);

    } else if (cmd == "OVERFLOW") {
      testOverflow();

    } else if (cmd == "CORRUPT") {
      testCorruptRecovery();

    } else if (cmd == "STATS") {
      printQueueStats();

    } else if (cmd == "CLEAR") {
      SD.remove("/queue.jsonl");
      Serial.println("\nQueue cleared\n");
      appendCount = 0;
      readCount = 0;
      removeCount = 0;

    } else if (cmd == "HELP") {
      Serial.println("\nAvailable commands:");
      Serial.println("  APPEND    - Append 5 test entries to queue");
      Serial.println("  READ      - Read up to 10 entries from queue");
      Serial.println("  REMOVE    - Remove uploaded entries");
      Serial.println("  COUNT     - Count queue entries");
      Serial.println("  OVERFLOW  - Test FIFO overflow (100 entry limit)");
      Serial.println("  CORRUPT   - Test corrupt line recovery");
      Serial.println("  STATS     - Show queue statistics");
      Serial.println("  CLEAR     - Clear queue file");
      Serial.println("  HELP      - Show this help");
    }
  }

  delay(100);
}

/**
 * Queue a scan entry to /queue.jsonl
 */
void queueScan(String tokenId, String teamId, String deviceId, String timestamp) {
  unsigned long startTime = millis();

  // Check queue size first (FIFO overflow if at limit)
  int queueSize = countQueueEntries();
  if (queueSize >= MAX_QUEUE_SIZE) {
    Serial.printf("[%lu] Queue full (%d entries), removing oldest\n", millis(), queueSize);
    removeOldestEntry();
    overflowCount++;
  }

  // Create JSON object
  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) {
    doc["teamId"] = teamId;
  }
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  // Serialize to string
  String jsonLine;
  serializeJson(doc, jsonLine);

  // Append to file
  File file = SD.open("/queue.jsonl", FILE_APPEND);
  if (!file) {
    Serial.println("ERROR: Could not open queue.jsonl for append");
    return;
  }

  file.println(jsonLine); // Adds '\n' automatically
  file.flush();           // Force write to SD card
  file.close();

  appendCount++;
  unsigned long duration = millis() - startTime;

  Serial.printf("[%lu] Queued: %s (took %lu ms)\n", millis(), tokenId.c_str(), duration);
}

/**
 * Read up to maxEntries from queue
 */
void readQueue(std::vector<QueueEntry>& entries, int maxEntries) {
  unsigned long startTime = millis();

  File file = SD.open("/queue.jsonl", FILE_READ);
  if (!file) {
    Serial.println("Queue file not found (empty queue)");
    return;
  }

  int count = 0;
  while (file.available() && count < maxEntries) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue; // Skip empty lines

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, line);

    if (error) {
      // Corruption: skip invalid line
      Serial.printf("WARNING: Skipping invalid JSON line: %s\n", error.c_str());
      corruptLinesSkipped++;
      continue;
    }

    // Validate required fields
    if (!doc.containsKey("tokenId") ||
        !doc.containsKey("deviceId") ||
        !doc.containsKey("timestamp")) {
      Serial.println("WARNING: Skipping entry missing required fields");
      corruptLinesSkipped++;
      continue;
    }

    // Add to batch
    QueueEntry entry;
    entry.tokenId = doc["tokenId"].as<String>();
    entry.teamId = doc.containsKey("teamId") ? doc["teamId"].as<String>() : "";
    entry.deviceId = doc["deviceId"].as<String>();
    entry.timestamp = doc["timestamp"].as<String>();
    entries.push_back(entry);

    count++;
    readCount++;
  }

  file.close();

  unsigned long duration = millis() - startTime;
  Serial.printf("[%lu] Read %d entries from queue (took %lu ms)\n", millis(), count, duration);
}

/**
 * Remove first N entries from queue (after successful upload)
 */
void removeUploadedEntries(int numEntries) {
  unsigned long startTime = millis();

  // Read entire file into memory
  std::vector<String> allLines;
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        allLines.push_back(line);
      }
    }
    file.close();
  }

  Serial.printf("[%lu] Removing %d entries from queue (%zu total)\n",
                millis(), numEntries, allLines.size());

  // Remove first N entries (uploaded batch)
  if (numEntries >= allLines.size()) {
    // All entries uploaded, delete file
    SD.remove("/queue.jsonl");
    Serial.println("All entries removed, queue file deleted");
    removeCount += allLines.size();
    return;
  }

  // Remove first N lines
  allLines.erase(allLines.begin(), allLines.begin() + numEntries);
  removeCount += numEntries;

  // Rewrite file with remaining entries
  SD.remove("/queue.jsonl"); // Delete old file
  file = SD.open("/queue.jsonl", FILE_WRITE);
  if (file) {
    for (String& line : allLines) {
      file.println(line);
    }
    file.flush();
    file.close();
  }

  unsigned long duration = millis() - startTime;
  Serial.printf("[%lu] Removed %d entries, %zu remaining (took %lu ms)\n",
                millis(), numEntries, allLines.size(), duration);
}

/**
 * Count entries in queue
 */
int countQueueEntries() {
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) count++;
  }

  file.close();
  return count;
}

/**
 * Remove oldest (first) entry from queue
 */
void removeOldestEntry() {
  removeUploadedEntries(1);
}

/**
 * Generate ISO 8601 timestamp from uptime
 */
String generateTimestamp() {
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  char timestamp[30];
  snprintf(timestamp, sizeof(timestamp),
    "1970-01-01T%02lu:%02lu:%02lu.%03luZ",
    hours % 24, minutes % 60, seconds % 60, ms % 1000);

  return String(timestamp);
}

/**
 * Print queue statistics
 */
void printQueueStats() {
  Serial.println("\n=== Queue Statistics ===");
  Serial.printf("Queue size: %d entries\n", countQueueEntries());
  Serial.printf("Total appended: %d\n", appendCount);
  Serial.printf("Total read: %d\n", readCount);
  Serial.printf("Total removed: %d\n", removeCount);
  Serial.printf("Overflow events: %d\n", overflowCount);
  Serial.printf("Corrupt lines skipped: %d\n", corruptLinesSkipped);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("=======================\n");
}

/**
 * Test: Append entries
 */
void testAppend() {
  Serial.println("\n>>> Test: Append 5 entries");

  for (int i = 0; i < 5; i++) {
    String tokenId = "TEST_TOKEN_" + String(i + 1);
    queueScan(tokenId, "001", "SCANNER_TEST", generateTimestamp());
    delay(100);
  }

  Serial.printf("\nQueue size after append: %d\n", countQueueEntries());
}

/**
 * Test: Read entries
 */
void testRead() {
  Serial.println("\n>>> Test: Read up to 10 entries");

  std::vector<QueueEntry> entries;
  readQueue(entries, 10);

  Serial.println("\nEntries read:");
  for (size_t i = 0; i < entries.size(); i++) {
    Serial.printf("%zu. Token: %s, Team: %s, Device: %s, Time: %s\n",
                  i + 1,
                  entries[i].tokenId.c_str(),
                  entries[i].teamId.length() > 0 ? entries[i].teamId.c_str() : "(none)",
                  entries[i].deviceId.c_str(),
                  entries[i].timestamp.c_str());
  }
}

/**
 * Test: Remove entries
 */
void testRemove() {
  Serial.println("\n>>> Test: Remove first 3 entries");

  int before = countQueueEntries();
  removeUploadedEntries(3);
  int after = countQueueEntries();

  Serial.printf("\nQueue size: %d → %d (removed %d)\n", before, after, before - after);
}

/**
 * Test: FIFO overflow
 */
void testOverflow() {
  Serial.println("\n>>> Test: FIFO Overflow (100 entry limit)");
  Serial.println("Filling queue to 100 entries...");

  // Clear queue first
  SD.remove("/queue.jsonl");

  // Fill to 100
  for (int i = 0; i < 100; i++) {
    String tokenId = "OVERFLOW_" + String(i + 1);
    queueScan(tokenId, "001", "SCANNER_TEST", generateTimestamp());
    if ((i + 1) % 20 == 0) {
      Serial.printf("  Progress: %d/100\n", i + 1);
    }
  }

  int queueSize = countQueueEntries();
  Serial.printf("\nQueue filled: %d entries\n", queueSize);

  // Try to add 101st entry (should trigger overflow)
  Serial.println("\nAdding 101st entry (should trigger overflow)...");
  queueScan("OVERFLOW_101", "001", "SCANNER_TEST", generateTimestamp());

  int finalSize = countQueueEntries();
  Serial.printf("\nFinal queue size: %d (should still be 100)\n", finalSize);
  Serial.printf("Overflow events: %d\n", overflowCount);

  if (finalSize == 100) {
    Serial.println("\n✓ PASS: FIFO overflow working correctly");
  } else {
    Serial.println("\n✗ FAIL: Queue size incorrect");
  }
}

/**
 * Test: Corrupt line recovery
 */
void testCorruptRecovery() {
  Serial.println("\n>>> Test: Corrupt Line Recovery");

  // Clear queue
  SD.remove("/queue.jsonl");

  // Add 3 valid entries
  queueScan("VALID_1", "001", "SCANNER_TEST", generateTimestamp());
  queueScan("VALID_2", "001", "SCANNER_TEST", generateTimestamp());
  queueScan("VALID_3", "001", "SCANNER_TEST", generateTimestamp());

  // Manually inject corrupt line
  File file = SD.open("/queue.jsonl", FILE_APPEND);
  if (file) {
    file.println("{\"invalid json");  // Missing closing brace
    file.println("{\"tokenId\":}");    // Invalid syntax
    file.flush();
    file.close();
    Serial.println("Injected 2 corrupt lines");
  }

  // Add 2 more valid entries
  queueScan("VALID_4", "001", "SCANNER_TEST", generateTimestamp());
  queueScan("VALID_5", "001", "SCANNER_TEST", generateTimestamp());

  Serial.printf("\nTotal lines in file: %d\n", countQueueEntries());

  // Try to read queue (should skip corrupt lines)
  std::vector<QueueEntry> entries;
  int beforeSkipped = corruptLinesSkipped;
  readQueue(entries, 10);

  Serial.printf("\nValid entries read: %zu\n", entries.size());
  Serial.printf("Corrupt lines skipped: %d\n", corruptLinesSkipped - beforeSkipped);

  if (entries.size() == 5 && (corruptLinesSkipped - beforeSkipped) == 2) {
    Serial.println("\n✓ PASS: Corrupt line recovery working");
  } else {
    Serial.println("\n✗ FAIL: Corrupt line recovery issue");
  }
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
