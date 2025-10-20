/*
 * Test Sketch 39: HTTP Client Test
 *
 * Purpose: Validate HTTP communication with orchestrator API
 *
 * Tests:
 * - HTTP GET /health endpoint (connection check)
 * - HTTP POST /api/scan with JSON payload (scan logging)
 * - Status code checking (200-299 = success)
 * - Timeout handling (5 seconds)
 * - Request/response timing
 *
 * Hardware: CYD ESP32 Display (ST7789)
 *
 * Prerequisites:
 * - WiFi connected (reads config.txt)
 * - Orchestrator running and reachable
 *
 * Expected Results:
 * - GET /health returns 200 OK within 5s
 * - POST /api/scan returns 200 OK within 2s
 * - Timeout handling works (disconnected orchestrator)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

// SD Card pins (Hardware SPI - VSPI)
#define SD_CS 5

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Configuration
String wifiSSID = "";
String wifiPassword = "";
String orchestratorURL = "";
String teamID = "";
String deviceID = "";

// Connection state
bool wifiConnected = false;
bool orchestratorReachable = false;

// Statistics
int httpGetAttempts = 0;
int httpGetSuccesses = 0;
int httpPostAttempts = 0;
int httpPostSuccesses = 0;
int timeouts = 0;

// Forward declarations
void parseConfigFile();
void WiFiEvent(WiFiEvent_t event);
void displayStatus(String message, uint16_t color);
void initWiFi();
bool testHealthEndpoint();
bool testScanEndpoint();
void printHttpStats();
String generateDeviceId();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Test 39: HTTP Client ===");

  // Initialize TFT display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  displayStatus("Initializing...", TFT_YELLOW);

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed");
    displayStatus("SD Card Error", TFT_RED);
    while (1) delay(1000);
  }

  Serial.println("SD Card mounted successfully");
  displayStatus("Reading Config...", TFT_YELLOW);

  // Parse configuration file
  parseConfigFile();

  if (orchestratorURL.length() == 0) {
    Serial.println("ERROR: ORCHESTRATOR_URL not found in config.txt");
    displayStatus("Config Error:\nNo ORCHESTRATOR_URL", TFT_RED);
    while (1) delay(1000);
  }

  // Generate device ID if not configured
  if (deviceID.length() == 0) {
    deviceID = generateDeviceId();
    Serial.printf("Auto-generated Device ID: %s\n", deviceID.c_str());
  }

  Serial.printf("Orchestrator URL: %s\n", orchestratorURL.c_str());
  Serial.printf("Team ID: %s\n", teamID.c_str());
  Serial.printf("Device ID: %s\n", deviceID.c_str());

  // Set up WiFi event handler
  WiFi.onEvent(WiFiEvent);

  // Initialize WiFi
  displayStatus("Connecting to WiFi...", TFT_YELLOW);
  initWiFi();

  // Wait for WiFi connection
  unsigned long startWait = millis();
  while (!wifiConnected && (millis() - startWait < 30000)) {
    delay(100);
  }

  if (!wifiConnected) {
    Serial.println("ERROR: WiFi connection failed");
    displayStatus("WiFi Connection\nFailed", TFT_RED);
    while (1) delay(1000);
  }

  Serial.println("\nWiFi connected! Ready for HTTP testing");
  Serial.println("\nType HELP for commands\n");

  displayStatus("Ready\nType HELP", TFT_GREEN);
}

void loop() {
  // Serial command interface
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "HEALTH") {
      Serial.println("\n>>> Testing GET /health endpoint...");
      displayStatus("Testing /health...", TFT_YELLOW);
      bool success = testHealthEndpoint();
      displayStatus(success ? "Health: OK" : "Health: FAIL", success ? TFT_GREEN : TFT_RED);

    } else if (cmd == "SCAN") {
      Serial.println("\n>>> Testing POST /api/scan endpoint...");
      displayStatus("Testing /api/scan...", TFT_YELLOW);
      bool success = testScanEndpoint();
      displayStatus(success ? "Scan: OK" : "Scan: FAIL", success ? TFT_GREEN : TFT_RED);

    } else if (cmd == "STATS") {
      printHttpStats();

    } else if (cmd == "MEM") {
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      Serial.printf("Heap size: %d bytes\n", ESP.getHeapSize());

    } else if (cmd == "CONFIG") {
      Serial.println("\n=== Configuration ===");
      Serial.printf("Orchestrator URL: %s\n", orchestratorURL.c_str());
      Serial.printf("Team ID: %s\n", teamID.c_str());
      Serial.printf("Device ID: %s\n", deviceID.c_str());
      Serial.println("====================\n");

    } else if (cmd == "HELP") {
      Serial.println("\nAvailable commands:");
      Serial.println("  HEALTH  - Test GET /health endpoint");
      Serial.println("  SCAN    - Test POST /api/scan endpoint");
      Serial.println("  STATS   - Show HTTP statistics");
      Serial.println("  CONFIG  - Show configuration");
      Serial.println("  MEM     - Show memory usage");
      Serial.println("  HELP    - Show this help");
    }
  }

  delay(100);
}

/**
 * Test GET /health endpoint
 */
bool testHealthEndpoint() {
  httpGetAttempts++;
  unsigned long startTime = millis();

  HTTPClient http;
  http.begin(orchestratorURL + "/health");
  http.setTimeout(5000); // 5-second timeout

  Serial.printf("[%lu] GET %s/health\n", millis(), orchestratorURL.c_str());

  int httpCode = http.GET();
  unsigned long duration = millis() - startTime;

  Serial.printf("[%lu] Response: HTTP %d (took %lu ms)\n", millis(), httpCode, duration);

  if (httpCode > 0) {
    if (httpCode == 200) {
      httpGetSuccesses++;
      orchestratorReachable = true;

      String payload = http.getString();
      Serial.printf("        Payload: %s\n", payload.c_str());

      // Parse JSON response
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        const char* status = doc["status"];
        const char* version = doc["version"];
        float uptime = doc["uptime"];

        Serial.printf("        Status: %s\n", status ? status : "unknown");
        Serial.printf("        Version: %s\n", version ? version : "unknown");
        Serial.printf("        Uptime: %.1f seconds\n", uptime);
      }

      Serial.println("        Result: ✓ PASS");
      http.end();
      return true;

    } else {
      Serial.printf("        Result: ✗ FAIL (unexpected status code)\n");
    }
  } else {
    timeouts++;
    orchestratorReachable = false;
    Serial.printf("        Error: %s\n", http.errorToString(httpCode).c_str());
    Serial.println("        Result: ✗ FAIL (timeout or connection error)");
  }

  http.end();
  return false;
}

/**
 * Test POST /api/scan endpoint
 */
bool testScanEndpoint() {
  httpPostAttempts++;
  unsigned long startTime = millis();

  // Create JSON payload
  JsonDocument doc;
  doc["tokenId"] = "TEST_TOKEN_001";
  doc["teamId"] = teamID.length() > 0 ? teamID : "001";
  doc["deviceId"] = deviceID;

  // Generate timestamp (simple uptime-based)
  char timestamp[30];
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  snprintf(timestamp, sizeof(timestamp), "1970-01-01T%02lu:%02lu:%02lu.%03luZ",
           hours % 24, minutes % 60, seconds % 60, ms % 1000);
  doc["timestamp"] = timestamp;

  String requestBody;
  serializeJson(doc, requestBody);

  HTTPClient http;
  http.begin(orchestratorURL + "/api/scan");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  Serial.printf("[%lu] POST %s/api/scan\n", millis(), orchestratorURL.c_str());
  Serial.printf("        Payload: %s\n", requestBody.c_str());

  int httpCode = http.POST(requestBody);
  unsigned long duration = millis() - startTime;

  Serial.printf("[%lu] Response: HTTP %d (took %lu ms)\n", millis(), httpCode, duration);

  if (httpCode > 0) {
    if (httpCode >= 200 && httpCode < 300) {
      httpPostSuccesses++;

      String payload = http.getString();
      Serial.printf("        Response body: %s\n", payload.c_str());
      Serial.println("        Result: ✓ PASS");

      http.end();
      return true;

    } else {
      Serial.printf("        Result: ✗ FAIL (status code %d)\n", httpCode);
    }
  } else {
    timeouts++;
    Serial.printf("        Error: %s\n", http.errorToString(httpCode).c_str());
    Serial.println("        Result: ✗ FAIL (timeout or connection error)");
  }

  http.end();
  return false;
}

/**
 * Print HTTP statistics
 */
void printHttpStats() {
  Serial.println("\n=== HTTP Statistics ===");
  Serial.printf("GET /health:\n");
  Serial.printf("  Attempts: %d\n", httpGetAttempts);
  Serial.printf("  Successes: %d (%.1f%%)\n",
                httpGetSuccesses,
                httpGetAttempts > 0 ? 100.0 * httpGetSuccesses / httpGetAttempts : 0);

  Serial.printf("POST /api/scan:\n");
  Serial.printf("  Attempts: %d\n", httpPostAttempts);
  Serial.printf("  Successes: %d (%.1f%%)\n",
                httpPostSuccesses,
                httpPostAttempts > 0 ? 100.0 * httpPostSuccesses / httpPostAttempts : 0);

  Serial.printf("Total timeouts: %d\n", timeouts);
  Serial.printf("Orchestrator: %s\n", orchestratorReachable ? "REACHABLE" : "UNREACHABLE");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("======================\n");
}

/**
 * Generate device ID from MAC address
 */
String generateDeviceId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "SCANNER_" + mac;
}

/**
 * Parse config.txt file from SD card
 */
void parseConfigFile() {
  File file = SD.open("/config.txt", FILE_READ);

  if (!file) {
    Serial.println("ERROR: config.txt not found on SD card");
    displayStatus("Config Error:\nNo config.txt", TFT_RED);
    while (1) delay(1000);
  }

  Serial.println("Parsing config.txt...");

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    int separatorIndex = line.indexOf('=');
    if (separatorIndex == -1) continue;

    String key = line.substring(0, separatorIndex);
    String value = line.substring(separatorIndex + 1);

    key.trim();
    value.trim();

    if (key == "WIFI_SSID") {
      wifiSSID = value;
    } else if (key == "WIFI_PASSWORD") {
      wifiPassword = value;
    } else if (key == "ORCHESTRATOR_URL") {
      orchestratorURL = value;
      Serial.printf("Found ORCHESTRATOR_URL: %s\n", orchestratorURL.c_str());
    } else if (key == "TEAM_ID") {
      teamID = value;
      Serial.printf("Found TEAM_ID: %s\n", teamID.c_str());
    } else if (key == "DEVICE_ID") {
      deviceID = value;
      Serial.printf("Found DEVICE_ID: %s\n", deviceID.c_str());
    }
  }

  file.close();
  Serial.println("Config parsing complete");
}

/**
 * Initialize WiFi connection
 */
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  Serial.printf("Connecting to WiFi: %s\n", wifiSSID.c_str());
}

/**
 * WiFi event handler
 */
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("[%lu] WiFi connected\n", millis());
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifiConnected = true;
      Serial.printf("[%lu] Got IP: %s\n", millis(), WiFi.localIP().toString().c_str());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifiConnected = false;
      Serial.printf("[%lu] WiFi disconnected\n", millis());
      break;

    default:
      break;
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
