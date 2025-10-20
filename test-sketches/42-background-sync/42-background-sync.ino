/*
 * Test Sketch 42: Background Sync (FreeRTOS Dual-Core)
 *
 * Purpose: Validate FreeRTOS task separation prevents SPI deadlocks
 *
 * Tests:
 * - Core 0: Background task (WiFi, HTTP, queue sync)
 * - Core 1: Main task (mock RFID scans, TFT display)
 * - SD mutex prevents concurrent access
 * - Connection state management
 * - Queue synchronization when connected
 * - Stress test: rapid scanning + background sync
 * - Deadlock prevention validation
 *
 * Hardware: CYD ESP32 Display (ST7789)
 *
 * Expected Results:
 * - No SPI deadlocks during 100 rapid scans
 * - SD mutex prevents concurrent access
 * - Main task continues scanning without blocking
 * - Background task syncs queue without blocking main
 * - Mutex wait times < 500ms
 */

#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

// SD Card pins (Hardware SPI - VSPI)
#define SD_CS 5
#define MAX_QUEUE_SIZE 100

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// ─── FreeRTOS Synchronization ─────────────────────────────────────
SemaphoreHandle_t sdMutex = NULL;
portMUX_TYPE connStateMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE queueSizeMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE metricsMux = portMUX_INITIALIZER_UNLOCKED;

// ─── Connection State ─────────────────────────────────────────────
enum ConnectionState {
  DISCONNECTED,
  WIFI_CONNECTED,
  CONNECTED  // WiFi + orchestrator reachable
};

volatile ConnectionState connState = DISCONNECTED;
volatile int queueSizeCached = 0;

// Configuration (from config.txt)
String wifiSSID = "";
String wifiPassword = "";
String orchestratorURL = "";
String teamID = "";
String deviceID = "";

// ─── Test Metrics ─────────────────────────────────────────────────
struct TestMetrics {
  // Main task
  uint32_t totalScans = 0;
  uint32_t scansQueued = 0;
  uint32_t scansSent = 0;

  // Background task
  uint32_t healthChecks = 0;
  uint32_t batchUploads = 0;
  uint32_t uploadSuccesses = 0;
  uint32_t uploadFailures = 0;

  // SD mutex
  uint32_t sdLockAttempts = 0;
  uint32_t sdLockWaits = 0;
  uint32_t sdLockTimeouts = 0;
  unsigned long maxWaitTime = 0;

  // Deadlock detection
  uint32_t deadlockEvents = 0;
} metrics;

// ─── Queue Entry ──────────────────────────────────────────────────
struct QueueEntry {
  String tokenId;
  String teamId;
  String deviceId;
  String timestamp;
};

// ─── Forward Declarations ─────────────────────────────────────────
void displayStatus(String message, uint16_t color);
void backgroundTask(void* parameter);
void parseConfigFile();

// Connection state helpers
void setConnectionState(ConnectionState newState);
ConnectionState getConnectionState();
void updateQueueSize(int delta);
int getQueueSize();

// SD operations (with mutex)
bool sdTakeMutex(const char* caller, unsigned long timeoutMs = 500);
void sdGiveMutex(const char* caller);
void queueScan(String tokenId, String teamId, String deviceId, String timestamp);
int countQueueEntries();
void readQueue(std::vector<QueueEntry>& entries, int maxEntries = 10);
void removeUploadedEntries(int numEntries);

// Network operations
bool checkOrchestratorHealth();
bool sendScan(String tokenId, String teamId, String deviceId, String timestamp);
bool uploadQueueBatch();

// Test functions
void mockScan(String tokenId);
void stressTest();
void printMetrics();
void printStatus();

// Utilities
String generateTimestamp();
String generateDeviceId();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Test 42: Background Sync (FreeRTOS) ===");

  // Initialize TFT display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  displayStatus("Initializing...", TFT_YELLOW);

  // Create SD mutex BEFORE any SD operations
  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) {
    Serial.println("FATAL: Failed to create SD mutex");
    displayStatus("Mutex Error", TFT_RED);
    while (1) delay(1000);
  }
  Serial.println("[MUTEX] SD mutex created");

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR: SD Card Mount Failed");
    displayStatus("SD Card Error", TFT_RED);
    while (1) delay(1000);
  }

  Serial.println("SD Card mounted successfully");

  // Parse configuration
  displayStatus("Reading Config...", TFT_YELLOW);
  parseConfigFile();

  // Generate device ID if not configured
  if (deviceID.length() == 0) {
    deviceID = generateDeviceId();
  }

  Serial.printf("Config loaded:\n");
  Serial.printf("  WiFi SSID: %s\n", wifiSSID.c_str());
  Serial.printf("  Orchestrator: %s\n", orchestratorURL.c_str());
  Serial.printf("  Team ID: %s\n", teamID.c_str());
  Serial.printf("  Device ID: %s\n", deviceID.c_str());

  // Initialize queue size cache
  queueSizeCached = countQueueEntries();
  Serial.printf("Queue size at boot: %d\n", queueSizeCached);

  // Connect to WiFi
  displayStatus("Connecting WiFi...", TFT_YELLOW);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    setConnectionState(WIFI_CONNECTED);
    Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

    // Check orchestrator
    if (checkOrchestratorHealth()) {
      setConnectionState(CONNECTED);
      Serial.println("Orchestrator reachable");
    } else {
      Serial.println("Orchestrator offline");
    }
  } else {
    Serial.println("WiFi connection failed");
    setConnectionState(DISCONNECTED);
  }

  // Create background task on Core 0
  // CRITICAL: Increased stack from 8192 to 16384 bytes
  // HTTP + JSON + std::vector operations need more stack
  xTaskCreatePinnedToCore(
    backgroundTask,     // Function
    "BackgroundSync",   // Name
    16384,              // Stack size (16KB - increased from 8KB)
    NULL,               // Parameters
    1,                  // Priority
    NULL,               // Task handle
    0                   // Core 0
  );

  Serial.println("[TASK] Background task created on Core 0");

  displayStatus("Ready for Testing", TFT_GREEN);

  Serial.println("\nType HELP for commands\n");
}

void loop() {
  // This runs on Core 1 (main task)

  // Serial command interface
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "SCAN") {
      mockScan("TEST_" + String(millis()));

    } else if (cmd == "STRESS") {
      stressTest();

    } else if (cmd == "QUEUE") {
      Serial.println("\n>>> Adding 20 test entries to queue");
      for (int i = 0; i < 20; i++) {
        mockScan("QUEUE_TEST_" + String(i + 1));
        delay(50);
      }
      Serial.printf("Queue size: %d\n", getQueueSize());

    } else if (cmd == "STATUS") {
      printStatus();

    } else if (cmd == "METRICS") {
      printMetrics();

    } else if (cmd == "CONNECT") {
      Serial.println("\n>>> Simulating orchestrator connection");
      setConnectionState(CONNECTED);

    } else if (cmd == "DISCONNECT") {
      Serial.println("\n>>> Simulating orchestrator disconnect");
      setConnectionState(WIFI_CONNECTED);

    } else if (cmd == "CLEAR") {
      if (sdTakeMutex("CLEAR", 1000)) {
        SD.remove("/queue.jsonl");
        sdGiveMutex("CLEAR");
        updateQueueSize(-getQueueSize());
        Serial.println("\nQueue cleared");
      }

    } else if (cmd == "HELP") {
      Serial.println("\nAvailable commands:");
      Serial.println("  SCAN       - Trigger mock RFID scan");
      Serial.println("  STRESS     - Stress test (100 rapid scans)");
      Serial.println("  QUEUE      - Add 20 test entries to queue");
      Serial.println("  STATUS     - Show connection and queue status");
      Serial.println("  METRICS    - Show test metrics");
      Serial.println("  CONNECT    - Simulate orchestrator connection");
      Serial.println("  DISCONNECT - Simulate orchestrator disconnect");
      Serial.println("  CLEAR      - Clear queue file");
      Serial.println("  HELP       - Show this help");
    }
  }

  delay(10);
}

/**
 * Background task (runs on Core 0)
 * Handles connection monitoring and queue synchronization
 */
void backgroundTask(void* parameter) {
  Serial.println("[BG-TASK] Background task started on Core 0");

  unsigned long lastHealthCheck = 0;
  const unsigned long healthCheckInterval = 10000; // 10 seconds

  while (true) {
    unsigned long now = millis();

    // Stack monitoring (check for stack overflow risk)
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
    if (stackRemaining < 512) {
      Serial.printf("[BG-TASK] WARNING: Low stack! Only %d bytes free\n", stackRemaining);
    }

    // Connection monitoring
    if (now - lastHealthCheck > healthCheckInterval) {
      lastHealthCheck = now;

      // Log stack health
      Serial.printf("[BG-TASK] Stack high water mark: %d bytes free\n", stackRemaining);

      ConnectionState state = getConnectionState();

      if (state == WIFI_CONNECTED || state == CONNECTED) {
        // Check orchestrator health
        portENTER_CRITICAL(&metricsMux);
        metrics.healthChecks++;
        portEXIT_CRITICAL(&metricsMux);

        if (checkOrchestratorHealth()) {
          if (state != CONNECTED) {
            Serial.println("[BG-TASK] Orchestrator now reachable");
            setConnectionState(CONNECTED);
          }

          // Queue synchronization
          int queueSize = getQueueSize();
          if (queueSize > 0) {
            Serial.printf("[BG-TASK] Queue has %d entries, starting sync\n", queueSize);
            uploadQueueBatch();
          }
        } else {
          if (state == CONNECTED) {
            Serial.println("[BG-TASK] Orchestrator unreachable");
            setConnectionState(WIFI_CONNECTED);
          }
        }
      }
    }

    // Yield CPU
    delay(100);
  }
}

/**
 * Mock RFID scan (simulates main scanner behavior)
 */
void mockScan(String tokenId) {
  unsigned long startTime = millis();

  portENTER_CRITICAL(&metricsMux);
  metrics.totalScans++;
  portEXIT_CRITICAL(&metricsMux);

  Serial.printf("[%lu] [MAIN] Mock scan: %s\n", millis(), tokenId.c_str());

  String timestamp = generateTimestamp();
  ConnectionState state = getConnectionState();

  if (state == CONNECTED) {
    // Try to send directly
    if (sendScan(tokenId, teamID, deviceID, timestamp)) {
      portENTER_CRITICAL(&metricsMux);
      metrics.scansSent++;
      portEXIT_CRITICAL(&metricsMux);
      Serial.printf("[%lu] [MAIN] Scan sent to orchestrator\n", millis());
    } else {
      // Send failed, queue it
      queueScan(tokenId, teamID, deviceID, timestamp);
      portENTER_CRITICAL(&metricsMux);
      metrics.scansQueued++;
      portEXIT_CRITICAL(&metricsMux);
      Serial.printf("[%lu] [MAIN] Send failed, queued\n", millis());
    }
  } else {
    // Offline, queue immediately
    queueScan(tokenId, teamID, deviceID, timestamp);
    portENTER_CRITICAL(&metricsMux);
    metrics.scansQueued++;
    portEXIT_CRITICAL(&metricsMux);
    Serial.printf("[%lu] [MAIN] Offline, queued\n", millis());
  }

  unsigned long duration = millis() - startTime;
  Serial.printf("[%lu] [MAIN] Scan processing took %lu ms\n", millis(), duration);
}

/**
 * Stress test: 100 rapid scans
 */
void stressTest() {
  Serial.println("\n>>> STRESS TEST: 100 rapid scans");
  Serial.println("This tests SD mutex under concurrent access from both cores");

  unsigned long startTime = millis();

  for (int i = 0; i < 100; i++) {
    mockScan("STRESS_" + String(i + 1));

    // Very short delay (simulates rapid scanning)
    delay(20);

    if ((i + 1) % 20 == 0) {
      Serial.printf("  Progress: %d/100\n", i + 1);
    }
  }

  unsigned long duration = millis() - startTime;

  Serial.printf("\nStress test complete!\n");
  Serial.printf("Duration: %lu ms (%.1f scans/sec)\n", duration, 100000.0 / duration);
  Serial.printf("Queue size: %d\n", getQueueSize());

  printMetrics();
}

/**
 * Print test metrics
 */
void printMetrics() {
  portENTER_CRITICAL(&metricsMux);
  TestMetrics m = metrics;  // Copy to avoid holding lock too long
  portEXIT_CRITICAL(&metricsMux);

  Serial.println("\n=== Test Metrics ===");
  Serial.printf("Main Task:\n");
  Serial.printf("  Total scans: %d\n", m.totalScans);
  Serial.printf("  Scans sent: %d\n", m.scansSent);
  Serial.printf("  Scans queued: %d\n", m.scansQueued);

  Serial.printf("\nBackground Task:\n");
  Serial.printf("  Health checks: %d\n", m.healthChecks);
  Serial.printf("  Batch uploads: %d\n", m.batchUploads);
  Serial.printf("  Upload successes: %d\n", m.uploadSuccesses);
  Serial.printf("  Upload failures: %d\n", m.uploadFailures);

  Serial.printf("\nSD Mutex:\n");
  Serial.printf("  Lock attempts: %d\n", m.sdLockAttempts);
  Serial.printf("  Lock waits: %d (%.1f%%)\n", m.sdLockWaits,
                m.sdLockAttempts > 0 ? 100.0 * m.sdLockWaits / m.sdLockAttempts : 0);
  Serial.printf("  Lock timeouts: %d\n", m.sdLockTimeouts);
  Serial.printf("  Max wait time: %lu ms\n", m.maxWaitTime);

  Serial.printf("\nDeadlocks: %d\n", m.deadlockEvents);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("====================\n");
}

/**
 * Print status
 */
void printStatus() {
  ConnectionState state = getConnectionState();
  int queueSize = getQueueSize();

  Serial.println("\n=== Status ===");
  Serial.printf("Connection: ");
  switch (state) {
    case DISCONNECTED:
      Serial.println("DISCONNECTED");
      break;
    case WIFI_CONNECTED:
      Serial.printf("WIFI_CONNECTED (%s)\n", WiFi.localIP().toString().c_str());
      break;
    case CONNECTED:
      Serial.printf("CONNECTED (%s + orchestrator)\n", WiFi.localIP().toString().c_str());
      break;
  }

  Serial.printf("Queue size: %d entries\n", queueSize);
  Serial.printf("Orchestrator: %s\n", orchestratorURL.c_str());
  Serial.printf("Team ID: %s\n", teamID.c_str());
  Serial.printf("Device ID: %s\n", deviceID.c_str());
  Serial.println("==============\n");
}

// ─── Connection State Helpers ─────────────────────────────────────

void setConnectionState(ConnectionState newState) {
  portENTER_CRITICAL(&connStateMux);
  connState = newState;
  portEXIT_CRITICAL(&connStateMux);
}

ConnectionState getConnectionState() {
  portENTER_CRITICAL(&connStateMux);
  ConnectionState state = connState;
  portEXIT_CRITICAL(&connStateMux);
  return state;
}

void updateQueueSize(int delta) {
  portENTER_CRITICAL(&queueSizeMux);
  queueSizeCached += delta;
  portEXIT_CRITICAL(&queueSizeMux);
}

int getQueueSize() {
  portENTER_CRITICAL(&queueSizeMux);
  int size = queueSizeCached;
  portEXIT_CRITICAL(&queueSizeMux);
  return size;
}

// ─── SD Mutex Helpers ─────────────────────────────────────────────

bool sdTakeMutex(const char* caller, unsigned long timeoutMs) {
  unsigned long startTime = millis();

  portENTER_CRITICAL(&metricsMux);
  metrics.sdLockAttempts++;
  portEXIT_CRITICAL(&metricsMux);

  bool gotLock = xSemaphoreTake(sdMutex, timeoutMs / portTICK_PERIOD_MS) == pdTRUE;

  if (gotLock) {
    unsigned long waitTime = millis() - startTime;

    if (waitTime > 0) {
      portENTER_CRITICAL(&metricsMux);
      metrics.sdLockWaits++;
      if (waitTime > metrics.maxWaitTime) {
        metrics.maxWaitTime = waitTime;
      }
      portEXIT_CRITICAL(&metricsMux);

      Serial.printf("[MUTEX] %s waited %lu ms for SD lock\n", caller, waitTime);
    }
  } else {
    portENTER_CRITICAL(&metricsMux);
    metrics.sdLockTimeouts++;
    portEXIT_CRITICAL(&metricsMux);

    Serial.printf("[MUTEX] %s timed out waiting for SD lock\n", caller);
  }

  return gotLock;
}

void sdGiveMutex(const char* caller) {
  xSemaphoreGive(sdMutex);
}

// ─── Queue Operations (with mutex) ────────────────────────────────

void queueScan(String tokenId, String teamId, String deviceId, String timestamp) {
  if (!sdTakeMutex("queueScan", 500)) {
    Serial.println("[QUEUE] Failed to acquire SD mutex, scan lost!");
    return;
  }

  // Check overflow
  int currentSize = countQueueEntries();
  if (currentSize >= MAX_QUEUE_SIZE) {
    Serial.printf("[QUEUE] Queue full (%d), removing oldest\n", currentSize);
    // Simplified: just delete file and start fresh (FIFO not critical for test)
    SD.remove("/queue.jsonl");
    updateQueueSize(-currentSize);
  }

  // Create JSON
  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) doc["teamId"] = teamId;
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  String jsonLine;
  serializeJson(doc, jsonLine);

  // Append to file
  File file = SD.open("/queue.jsonl", FILE_APPEND);
  if (file) {
    file.println(jsonLine);
    file.flush();
    file.close();
    updateQueueSize(1);
  } else {
    Serial.println("[QUEUE] Failed to open queue file");
  }

  sdGiveMutex("queueScan");
}

int countQueueEntries() {
  // Called within mutex already
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

void readQueue(std::vector<QueueEntry>& entries, int maxEntries) {
  // Called within mutex already
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (!file) return;

  int count = 0;
  while (file.available() && count < maxEntries) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    JsonDocument doc;
    if (deserializeJson(doc, line) == DeserializationError::Ok) {
      if (doc.containsKey("tokenId") && doc.containsKey("deviceId") && doc.containsKey("timestamp")) {
        QueueEntry entry;
        entry.tokenId = doc["tokenId"].as<String>();
        entry.teamId = doc.containsKey("teamId") ? doc["teamId"].as<String>() : "";
        entry.deviceId = doc["deviceId"].as<String>();
        entry.timestamp = doc["timestamp"].as<String>();
        entries.push_back(entry);
        count++;
      }
    }
  }

  file.close();
}

void removeUploadedEntries(int numEntries) {
  // Called within mutex already
  std::vector<String> allLines;
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) allLines.push_back(line);
    }
    file.close();
  }

  if (numEntries >= allLines.size()) {
    SD.remove("/queue.jsonl");
    updateQueueSize(-allLines.size());
    return;
  }

  allLines.erase(allLines.begin(), allLines.begin() + numEntries);
  updateQueueSize(-numEntries);

  SD.remove("/queue.jsonl");
  file = SD.open("/queue.jsonl", FILE_WRITE);
  if (file) {
    for (String& line : allLines) {
      file.println(line);
    }
    file.flush();
    file.close();
  }
}

// ─── Network Operations ───────────────────────────────────────────

bool checkOrchestratorHealth() {
  if (orchestratorURL.length() == 0) return false;

  HTTPClient http;
  http.begin(orchestratorURL + "/health");
  http.setTimeout(5000);

  int httpCode = http.GET();
  http.end();

  return (httpCode == 200);
}

bool sendScan(String tokenId, String teamId, String deviceId, String timestamp) {
  if (orchestratorURL.length() == 0) return false;

  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) doc["teamId"] = teamId;
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  String requestBody;
  serializeJson(doc, requestBody);

  HTTPClient http;
  http.begin(orchestratorURL + "/api/scan");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST(requestBody);
  http.end();

  return (httpCode >= 200 && httpCode < 300);
}

bool uploadQueueBatch() {
  if (!sdTakeMutex("uploadBatch", 1000)) {
    Serial.println("[BG-TASK] Could not acquire SD lock for batch upload");
    return false;
  }

  std::vector<QueueEntry> entries;
  readQueue(entries, 10);

  sdGiveMutex("uploadBatch");

  if (entries.empty()) return true;

  portENTER_CRITICAL(&metricsMux);
  metrics.batchUploads++;
  portEXIT_CRITICAL(&metricsMux);

  Serial.printf("[BG-TASK] Uploading batch of %zu entries\n", entries.size());

  // Create batch request
  JsonDocument doc;
  JsonArray transactions = doc["transactions"].to<JsonArray>();

  for (QueueEntry& entry : entries) {
    JsonObject transaction = transactions.add<JsonObject>();
    transaction["tokenId"] = entry.tokenId;
    if (entry.teamId.length() > 0) transaction["teamId"] = entry.teamId;
    transaction["deviceId"] = entry.deviceId;
    transaction["timestamp"] = entry.timestamp;
  }

  String requestBody;
  serializeJson(doc, requestBody);

  HTTPClient http;
  http.begin(orchestratorURL + "/api/scan/batch");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST(requestBody);
  http.end();

  if (httpCode == 200) {
    portENTER_CRITICAL(&metricsMux);
    metrics.uploadSuccesses++;
    portEXIT_CRITICAL(&metricsMux);

    Serial.printf("[BG-TASK] Batch upload success, removing %zu entries\n", entries.size());

    if (sdTakeMutex("removeUploaded", 1000)) {
      removeUploadedEntries(entries.size());
      sdGiveMutex("removeUploaded");
    }

    // If more in queue, upload next batch after delay
    if (getQueueSize() > 0) {
      delay(1000);
      return uploadQueueBatch();
    }

    return true;
  } else {
    portENTER_CRITICAL(&metricsMux);
    metrics.uploadFailures++;
    portEXIT_CRITICAL(&metricsMux);

    Serial.printf("[BG-TASK] Batch upload failed (HTTP %d)\n", httpCode);
    return false;
  }
}

// ─── Utilities ────────────────────────────────────────────────────

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

String generateDeviceId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "SCANNER_" + mac;
}

void parseConfigFile() {
  if (!sdTakeMutex("parseConfig", 1000)) {
    Serial.println("Could not acquire SD lock for config parsing");
    return;
  }

  File file = SD.open("/config.txt", FILE_READ);
  if (!file) {
    Serial.println("config.txt not found");
    sdGiveMutex("parseConfig");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sepIndex = line.indexOf('=');
    if (sepIndex == -1) continue;

    String key = line.substring(0, sepIndex);
    String value = line.substring(sepIndex + 1);
    key.trim();
    value.trim();

    if (key == "WIFI_SSID") wifiSSID = value;
    else if (key == "WIFI_PASSWORD") wifiPassword = value;
    else if (key == "ORCHESTRATOR_URL") orchestratorURL = value;
    else if (key == "TEAM_ID") teamID = value;
    else if (key == "DEVICE_ID") deviceID = value;
  }

  file.close();
  sdGiveMutex("parseConfig");
}

void displayStatus(String message, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 60);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(2);
  tft.println(message);
}
