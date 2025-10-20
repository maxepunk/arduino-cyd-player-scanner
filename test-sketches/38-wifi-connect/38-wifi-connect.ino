/*
 * Test Sketch 38: WiFi Connection Test
 *
 * Purpose: Validate WiFi connection from config file
 *
 * Tests:
 * - Config file parser (key=value format)
 * - WiFi connection with credentials
 * - WiFi event handlers (connected/disconnected)
 * - Automatic reconnection
 * - TFT status display
 *
 * Hardware: CYD ESP32 Display (ST7789)
 *
 * Expected Results:
 * - Connects to WiFi within 30s
 * - Displays "Connected ✓" with IP address
 * - Auto-reconnects on disconnect
 */

#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// SD Card pins (Hardware SPI - VSPI)
#define SD_CS 5

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Configuration
String wifiSSID = "";
String wifiPassword = "";

// Connection state
bool wifiConnected = false;
unsigned long lastConnectionAttempt = 0;
unsigned long connectionStartTime = 0;
const unsigned long reconnectInterval = 30000; // 30 seconds

// Statistics
int connectionAttempts = 0;
int successfulConnections = 0;
int disconnections = 0;

// Forward declarations
void parseConfigFile();
void WiFiEvent(WiFiEvent_t event);
void displayStatus(String message, uint16_t color);
void initWiFi();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Test 38: WiFi Connection ===");

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

  if (wifiSSID.length() == 0) {
    Serial.println("ERROR: WIFI_SSID not found in config.txt");
    displayStatus("Config Error:\nMissing WIFI_SSID", TFT_RED);
    while (1) delay(1000);
  }

  Serial.print("WiFi SSID: ");
  Serial.println(wifiSSID);
  Serial.print("WiFi Password: ");
  Serial.println(wifiPassword.length() > 0 ? "********" : "(empty - open network)");

  // Set up WiFi event handler
  WiFi.onEvent(WiFiEvent);

  // Initialize WiFi
  displayStatus("Connecting to WiFi...", TFT_YELLOW);
  initWiFi();
}

void loop() {
  // Check if we need to attempt reconnection
  if (!wifiConnected && (millis() - lastConnectionAttempt > reconnectInterval)) {
    Serial.println("Attempting WiFi reconnection...");
    displayStatus("Reconnecting...", TFT_YELLOW);
    initWiFi();
  }

  // Serial command interface for testing
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "STATUS") {
      printWiFiStatus();
    } else if (cmd == "RECONNECT") {
      Serial.println("Manual reconnection requested");
      WiFi.disconnect();
      delay(100);
      initWiFi();
    } else if (cmd == "MEM") {
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      Serial.printf("Heap size: %d bytes\n", ESP.getHeapSize());
    } else if (cmd == "HELP") {
      Serial.println("\nAvailable commands:");
      Serial.println("  STATUS    - Show WiFi connection status");
      Serial.println("  RECONNECT - Force WiFi reconnection");
      Serial.println("  MEM       - Show memory usage");
      Serial.println("  HELP      - Show this help");
    }
  }

  delay(100);
}

void printWiFiStatus() {
  Serial.println("\n=== WiFi Status ===");
  Serial.printf("Connected: %s\n", wifiConnected ? "YES" : "NO");
  if (wifiConnected) {
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
  }
  Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("\nStatistics:\n");
  Serial.printf("  Connection attempts: %d\n", connectionAttempts);
  Serial.printf("  Successful: %d (%.1f%%)\n",
                successfulConnections,
                connectionAttempts > 0 ? 100.0 * successfulConnections / connectionAttempts : 0);
  Serial.printf("  Disconnections: %d\n", disconnections);
  Serial.printf("  Uptime: %lu seconds\n", millis() / 1000);
  Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("==================\n");
}

/**
 * Parse config.txt file from SD card
 * Format: KEY=VALUE (one per line)
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
    line.trim(); // Remove whitespace and line endings

    if (line.length() == 0) continue; // Skip empty lines

    int separatorIndex = line.indexOf('=');
    if (separatorIndex == -1) {
      Serial.print("Skipping invalid line (no separator): ");
      Serial.println(line);
      continue;
    }

    String key = line.substring(0, separatorIndex);
    String value = line.substring(separatorIndex + 1);

    key.trim();
    value.trim();

    if (key == "WIFI_SSID") {
      wifiSSID = value;
      Serial.print("Found WIFI_SSID: ");
      Serial.println(wifiSSID);
    } else if (key == "WIFI_PASSWORD") {
      wifiPassword = value;
      Serial.print("Found WIFI_PASSWORD: ");
      Serial.println(wifiPassword.length() > 0 ? "********" : "(empty)");
    }
  }

  file.close();
  Serial.println("Config parsing complete");
}

/**
 * Initialize WiFi connection
 */
void initWiFi() {
  lastConnectionAttempt = millis();
  connectionStartTime = millis();
  connectionAttempts++;

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  Serial.printf("\n[%lu] Connection attempt #%d to WiFi: %s\n",
                millis(), connectionAttempts, wifiSSID.c_str());
}

/**
 * WiFi event handler
 * Handles connection and disconnection events
 */
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("[%lu] EVENT: WiFi connected to AP\n", millis());
      Serial.printf("        SSID: %s\n", WiFi.SSID().c_str());
      Serial.printf("        Channel: %d\n", WiFi.channel());
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      {
        wifiConnected = true;
        successfulConnections++;
        unsigned long connectionTime = millis() - connectionStartTime;

        Serial.printf("[%lu] EVENT: Got IP address\n", millis());
        Serial.printf("        IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("        Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("        Subnet: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("        DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("        Signal: %d dBm\n", WiFi.RSSI());
        Serial.printf("        Connection time: %lu ms\n", connectionTime);
        Serial.printf("        Success rate: %d/%d (%.1f%%)\n",
                      successfulConnections, connectionAttempts,
                      100.0 * successfulConnections / connectionAttempts);

        // Display success
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(10, 20);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(3);
        tft.println("Connected!");

        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 60);
        tft.print("SSID: ");
        tft.println(wifiSSID);

        tft.setCursor(10, 90);
        tft.print("IP: ");
        tft.println(WiFi.localIP());

        tft.setCursor(10, 120);
        tft.printf("Time: %lu ms", connectionTime);

        tft.setCursor(10, 150);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println("Test PASSED!");

        Serial.println("\nType HELP for serial commands\n");
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      {
        wifiConnected = false;
        disconnections++;

        Serial.printf("[%lu] EVENT: WiFi disconnected\n", millis());
        Serial.printf("        Total disconnections: %d\n", disconnections);
        displayStatus("WiFi Disconnected\nReconnecting...", TFT_RED);

        // Attempt reconnection
        Serial.println("        Initiating auto-reconnect...");
        WiFi.reconnect();
        lastConnectionAttempt = millis();
      }
      break;

    default:
      Serial.printf("[%lu] EVENT: WiFi event %d\n", millis(), event);
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
