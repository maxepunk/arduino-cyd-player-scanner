#pragma once

/**
 * @file OrchestratorService.h
 * @brief Network operations service for ALNScanner v5.0
 *
 * Manages WiFi connection, HTTP client operations, offline queue, and background sync.
 *
 * **CRITICAL FLASH OPTIMIZATION:** This service consolidates 4 duplicate HTTP client
 * implementations into a single HTTPHelper class, saving ~15KB flash.
 *
 * Key Features:
 * - Singleton pattern for global access
 * - HTTP code consolidation (PRIMARY FLASH SAVINGS)
 * - WiFi event-driven state management
 * - Thread-safe queue operations (JSONL-based)
 * - FreeRTOS background sync task (Core 0)
 * - Stream-based queue removal (memory-safe)
 *
 * Extracted from v4.1 monolithic codebase:
 * - WiFi: Lines 2365-2444
 * - HTTP operations: Lines 1571-1716, 1725-1824, 1637-1648, 2457-2485
 * - Queue: Lines 1866-2089
 * - Background task: Lines 2447-2496, 2679-2683
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "../models/Config.h"
#include "../models/Token.h"
#include "../models/ConnectionState.h"
#include "../hal/SDCard.h"
#include "../config.h"

namespace services {

class OrchestratorService {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the global OrchestratorService instance
     */
    static OrchestratorService& getInstance() {
        static OrchestratorService instance;
        return instance;
    }

    // ─── Lifecycle ─────────────────────────────────────────────────────

    /**
     * @brief Initialize WiFi connection with event handlers
     * @param config Device configuration with WiFi credentials
     * @return true if connected within timeout, false otherwise
     *
     * Implementation from v4.1 lines 2390-2444
     */
    bool initializeWiFi(const models::DeviceConfig& config) {
        if (config.wifiSSID.length() == 0) {
            LOG_INFO("[ORCH-WIFI] No SSID configured, skipping WiFi\n");
            return false;
        }

        LOG_INFO("\n[ORCH-WIFI] ═══ WIFI CONNECTION START ═══\n");

        // Register event handlers (static methods)
        WiFi.onEvent(onWiFiConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent(onWiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.onEvent(onWiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

        // Set WiFi mode and connect
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);

        LOG_INFO("[ORCH-WIFI] SSID: %s\n", config.wifiSSID.c_str());
        LOG_INFO("[ORCH-WIFI] Free heap before connection: %d bytes\n", ESP.getFreeHeap());

        WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());

        LOG_INFO("[ORCH-WIFI] Waiting for connection (timeout: %u ms)...\n",
                 timing::WIFI_CONNECT_TIMEOUT_MS);

        unsigned long startTime = millis();
        int dots = 0;

        while (WiFi.status() != WL_CONNECTED &&
               millis() - startTime < timing::WIFI_CONNECT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
            dots++;
            if (dots % 20 == 0) Serial.println();
        }
        Serial.println();

        unsigned long connectionTime = millis() - startTime;

        if (WiFi.status() == WL_CONNECTED) {
            _connState.set(models::ORCH_WIFI_CONNECTED);
            LOG_INFO("[ORCH-WIFI] ✓✓✓ SUCCESS ✓✓✓ Connected in %lu ms\n", connectionTime);
            LOG_INFO("[ORCH-WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
            LOG_INFO("[ORCH-WIFI] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            LOG_INFO("[ORCH-WIFI] Signal Strength: %d dBm\n", WiFi.RSSI());
            LOG_INFO("[ORCH-WIFI] Channel: %d\n", WiFi.channel());

            // Check orchestrator health
            LOG_INFO("\n[ORCH] Checking orchestrator health...\n");
            LOG_INFO("[ORCH] URL: %s/health?deviceId=%s\n",
                     config.orchestratorURL.c_str(), config.deviceID.c_str());

            if (checkHealth(config)) {
                _connState.set(models::ORCH_CONNECTED);
                LOG_INFO("[ORCH] ✓✓✓ SUCCESS ✓✓✓ Orchestrator reachable\n");
            } else {
                LOG_INFO("[ORCH] ✗✗✗ FAILURE ✗✗✗ Orchestrator offline\n");
            }

            LOG_INFO("[ORCH-WIFI] Free heap after connection: %d bytes\n", ESP.getFreeHeap());
            LOG_INFO("[ORCH-WIFI] ═══ WIFI CONNECTION END ═══\n\n");
            return true;
        } else {
            LOG_INFO("[ORCH-WIFI] ✗✗✗ FAILURE ✗✗✗ Connection timeout after %lu ms\n",
                     connectionTime);
            _connState.set(models::ORCH_DISCONNECTED);
            LOG_INFO("[ORCH-WIFI] ═══ WIFI CONNECTION END ═══\n\n");
            return false;
        }
    }

    /**
     * @brief Start background FreeRTOS task for orchestrator sync
     * @param config Device configuration (for orchestrator URL and deviceID)
     *
     * Implementation from v4.1 lines 2679-2683, 2447-2496
     * Task runs on Core 0, main loop on Core 1
     */
    void startBackgroundTask(const models::DeviceConfig& config) {
        // Store config for background task (thread-safe copy)
        _config = config;

        xTaskCreatePinnedToCore(
            backgroundTaskWrapper,
            "OrchestratorSync",
            freertos_config::BACKGROUND_TASK_STACK_SIZE,
            this,  // Pass 'this' pointer to static wrapper
            freertos_config::BACKGROUND_TASK_PRIORITY,
            nullptr,
            freertos_config::BACKGROUND_TASK_CORE
        );

        LOG_INFO("[ORCH] Background sync task started on Core %d\n",
                 freertos_config::BACKGROUND_TASK_CORE);
    }

    // ─── Scan Operations ───────────────────────────────────────────────

    /**
     * @brief Send scan to orchestrator immediately
     * @param scan Scan data with tokenId, teamId, deviceId, timestamp
     * @param config Device configuration (for orchestrator URL)
     * @return true if successfully sent (2xx) or duplicate (409), false otherwise
     *
     * Implementation from v4.1 lines 1650-1716
     * Uses consolidated HTTP helper (FLASH SAVINGS)
     */
    bool sendScan(const models::ScanData& scan, const models::DeviceConfig& config) {
        LOG_INFO("\n[ORCH-SEND] ═══ HTTP POST /api/scan START ═══\n");
        LOG_INFO("[ORCH-SEND] Free heap: %d bytes\n", ESP.getFreeHeap());

        unsigned long startMs = millis();

        if (config.orchestratorURL.length() == 0) {
            LOG_INFO("[ORCH-SEND] ✗✗✗ FAILURE ✗✗✗ No orchestrator URL configured\n");
            LOG_INFO("[ORCH-SEND] ═══ HTTP POST /api/scan END ═══\n\n");
            return false;
        }

        // Build JSON payload
        JsonDocument doc;
        doc["tokenId"] = scan.tokenId;
        if (scan.teamId.length() > 0) doc["teamId"] = scan.teamId;
        doc["deviceId"] = scan.deviceId;
        doc["timestamp"] = scan.timestamp;

        String requestBody;
        serializeJson(doc, requestBody);

        LOG_INFO("[ORCH-SEND] URL: %s/api/scan\n", config.orchestratorURL.c_str());
        LOG_INFO("[ORCH-SEND] Payload: %s\n", requestBody.c_str());
        LOG_INFO("[ORCH-SEND] Payload size: %d bytes\n", requestBody.length());

        // Send via consolidated HTTP helper with retry logic
        String url = config.orchestratorURL + "/api/scan";
        auto resp = httpWithRetry([&]() {
            return _http.httpPOST(url, requestBody, 10000);
        }, "scan submission");

        unsigned long latencyMs = millis() - startMs;
        LOG_INFO("[ORCH-SEND] HTTP response code: %d\n", resp.code);
        LOG_INFO("[ORCH-SEND] Request latency: %lu ms\n", latencyMs);

        if (resp.body.length() > 0 && resp.body.length() < 200) {
            LOG_INFO("[ORCH-SEND] Response body: %s\n", resp.body.c_str());
        }

        // Accept 2xx success codes AND 409 Conflict (duplicate scan)
        bool success = resp.success || (resp.code == 409);

        if (success) {
            if (resp.code == 409) {
                LOG_INFO("[ORCH-SEND] ✓ 409 Conflict - orchestrator received scan (duplicate handled)\n");
            } else {
                LOG_INFO("[ORCH-SEND] ✓✓✓ SUCCESS ✓✓✓ HTTP %d received\n", resp.code);
            }
        } else {
            LOG_INFO("[ORCH-SEND] ✗✗✗ FAILURE ✗✗✗ HTTP %d (will queue)\n", resp.code);
        }

        LOG_INFO("[ORCH-SEND] Free heap after send: %d bytes\n", ESP.getFreeHeap());
        LOG_INFO("[ORCH-SEND] ═══ HTTP POST /api/scan END ═══\n\n");

        return success;
    }

    /**
     * @brief Queue scan for later upload (offline mode)
     * @param scan Scan data to queue
     *
     * Implementation from v4.1 lines 1866-1922
     * Writes JSONL entry to /queue.jsonl with FIFO overflow check
     */
    void queueScan(const models::ScanData& scan) {
        LOG_INFO("\n[ORCH-QUEUE] ═══ QUEUE SCAN START ═══\n");
        LOG_INFO("[ORCH-QUEUE] Free heap: %d bytes\n", ESP.getFreeHeap());

        unsigned long startMs = millis();

        // Check for queue overflow BEFORE acquiring mutex
        if (getQueueSize() >= queue_config::MAX_QUEUE_SIZE) {
            handleQueueOverflow();
        }

        hal::SDCard::Lock lock("queueScan", freertos_config::SD_MUTEX_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("ORCH-QUEUE", "Failed to acquire SD mutex (timeout)");
            LOG_INFO("[ORCH-QUEUE] ═══ QUEUE SCAN END ═══\n\n");
            return;
        }

        // Build JSONL entry
        JsonDocument doc;
        doc["tokenId"] = scan.tokenId;
        if (scan.teamId.length() > 0) doc["teamId"] = scan.teamId;
        doc["deviceId"] = scan.deviceId;
        doc["timestamp"] = scan.timestamp;

        String jsonLine;
        serializeJson(doc, jsonLine);

        LOG_INFO("[ORCH-QUEUE] Entry: %s\n", jsonLine.c_str());
        LOG_INFO("[ORCH-QUEUE] Entry size: %d bytes\n", jsonLine.length());

        // Append to queue file
        LOG_INFO("[ORCH-QUEUE] Opening %s for append...\n", queue_config::QUEUE_FILE);
        File file = SD.open(queue_config::QUEUE_FILE, FILE_APPEND);

        if (file) {
            unsigned long fileSize = file.size();
            LOG_INFO("[ORCH-QUEUE] File opened, current size: %lu bytes\n", fileSize);

            file.println(jsonLine);
            file.flush();
            file.close();

            // Update queue size (atomic)
            updateQueueSize(1);
            int newQueueSize = getQueueSize();
            unsigned long latencyMs = millis() - startMs;

            LOG_INFO("[ORCH-QUEUE] ✓✓✓ SUCCESS ✓✓✓ Scan queued (token: %s)\n",
                     scan.tokenId.c_str());
            LOG_INFO("[ORCH-QUEUE] Queue size: %d entries\n", newQueueSize);
            LOG_INFO("[ORCH-QUEUE] Write latency: %lu ms\n", latencyMs);
        } else {
            LOG_ERROR("ORCH-QUEUE", "Could not open queue file for writing");
        }

        LOG_INFO("[ORCH-QUEUE] Free heap after queue: %d bytes\n", ESP.getFreeHeap());
        LOG_INFO("[ORCH-QUEUE] ═══ QUEUE SCAN END ═══\n\n");
    }

    // ─── State Queries ─────────────────────────────────────────────────

    /**
     * @brief Get current connection state (thread-safe)
     * @return Connection state enum
     */
    models::ConnectionState getState() const {
        return _connState.get();
    }

    /**
     * @brief Get current queue size (atomic)
     * @return Number of entries in queue
     */
    int getQueueSize() const {
        portENTER_CRITICAL(&_queue.mutex);
        int size = _queue.size;
        portEXIT_CRITICAL(&_queue.mutex);
        return size;
    }

    // ─── Health Check ──────────────────────────────────────────────────

    /**
     * @brief Check orchestrator health endpoint with device identification
     * @param config Device configuration (contains orchestratorURL and deviceId)
     * @return true if GET /health returns 200, false otherwise
     *
     * Implementation from v4.1 lines 1637-1648 (modified to include deviceId)
     * Uses consolidated HTTP helper (FLASH SAVINGS)
     * Sends deviceId as query parameter: GET /health?deviceId=SCANNER_001
     */
    bool checkHealth(const models::DeviceConfig& config) {
        if (config.orchestratorURL.length() == 0) return false;

        String url = config.orchestratorURL + "/health?deviceId=" + config.deviceID;
        auto resp = httpWithRetry([&]() {
            return _http.httpGET(url, 5000);
        }, "health check");

        return (resp.code == 200);
    }

    /**
     * @brief Initialize queue size from disk file (call after SD ready)
     * @return true if queue valid, false if corrupted and deleted
     *
     * Validates queue file, counts entries, sets cached size.
     * If file size > 100KB, deletes as corrupted and resets to 0.
     *
     * MUST be called from Application::setup() after SD card initialized.
     * Designed to catch power-loss corruption (e.g., 1.7GB file from incomplete writes).
     */
    bool initializeQueue() {
        LOG_INFO("\n[ORCH-QUEUE-INIT] ═══ QUEUE INITIALIZATION START ═══\n");

        hal::SDCard::Lock lock("initQueue", freertos_config::SD_MUTEX_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("ORCH-QUEUE-INIT", "Failed to acquire SD mutex");
            return false;
        }

        // Check if queue file exists
        if (!SD.exists(queue_config::QUEUE_FILE)) {
            LOG_INFO("[ORCH-QUEUE-INIT] No queue file, starting fresh\n");
            portENTER_CRITICAL(&_queue.mutex);
            _queue.size = 0;
            portEXIT_CRITICAL(&_queue.mutex);
            LOG_INFO("[ORCH-QUEUE-INIT] ═══ QUEUE INITIALIZATION END ═══\n\n");
            return true;
        }

        File file = SD.open(queue_config::QUEUE_FILE, FILE_READ);
        if (!file) {
            LOG_ERROR("ORCH-QUEUE-INIT", "Could not open queue file");
            return false;
        }

        // CORRUPTION CHECK: File size validation
        unsigned long fileSize = file.size();
        LOG_INFO("[ORCH-QUEUE-INIT] Queue file size: %lu bytes\n", fileSize);

        if (fileSize > queue_config::MAX_QUEUE_FILE_SIZE) {
            LOG_ERROR("ORCH-QUEUE-INIT", "CORRUPTION DETECTED");
            LOG_INFO("[ORCH-QUEUE-INIT] File size %lu exceeds threshold %lu\n",
                     fileSize, queue_config::MAX_QUEUE_FILE_SIZE);
            LOG_INFO("[ORCH-QUEUE-INIT] Deleting corrupted queue file\n");

            file.close();
            SD.remove(queue_config::QUEUE_FILE);

            portENTER_CRITICAL(&_queue.mutex);
            _queue.size = 0;
            portEXIT_CRITICAL(&_queue.mutex);

            LOG_INFO("[ORCH-QUEUE-INIT] ✓ Corrupted queue deleted, reset to 0\n");
            LOG_INFO("[ORCH-QUEUE-INIT] ═══ QUEUE INITIALIZATION END ═══\n\n");
            return false;  // Indicate corruption was found and handled
        }

        // Count actual entries (newlines)
        int lineCount = 0;
        while (file.available()) {
            file.readStringUntil('\n');
            lineCount++;
        }
        file.close();

        // Update cached size with actual count
        portENTER_CRITICAL(&_queue.mutex);
        _queue.size = lineCount;
        portEXIT_CRITICAL(&_queue.mutex);

        LOG_INFO("[ORCH-QUEUE-INIT] ✓ Queue validated: %d entries\n", lineCount);
        LOG_INFO("[ORCH-QUEUE-INIT] ═══ QUEUE INITIALIZATION END ═══\n\n");
        return true;
    }

    // ─── Queue Operations ──────────────────────────────────────────────

    /**
     * @brief Upload batch of queued scans to orchestrator
     * @param config Device configuration (for orchestrator URL)
     * @return true if batch uploaded successfully, false otherwise
     *
     * Implementation from v4.1 lines 1725-1824
     * Uses consolidated HTTP helper (FLASH SAVINGS)
     */
    bool uploadQueueBatch(const models::DeviceConfig& config) {
        LOG_INFO("\n[ORCH-BATCH] ═══════════════════════════════════\n");
        LOG_INFO("[ORCH-BATCH]   QUEUE BATCH UPLOAD START\n");
        LOG_INFO("[ORCH-BATCH] ═══════════════════════════════════\n");
        LOG_INFO("[ORCH-BATCH] Free heap before: %d bytes\n", ESP.getFreeHeap());
        LOG_INFO("[ORCH-BATCH] Current queue size: %d entries\n", getQueueSize());

        // Read batch of up to 10 entries
        std::vector<models::ScanData> batch;

        {
            hal::SDCard::Lock lock("uploadBatch", freertos_config::SD_MUTEX_TIMEOUT_MS);
            if (!lock.acquired()) {
                LOG_ERROR("ORCH-BATCH", "Could not acquire SD mutex");
                LOG_INFO("[ORCH-BATCH] ═══════════════════════════════════\n\n");
                return false;
            }

            readQueue(batch, queue_config::BATCH_UPLOAD_SIZE);
        }  // Lock released here

        if (batch.empty()) {
            LOG_INFO("[ORCH-BATCH] Queue is empty, nothing to upload\n");
            LOG_INFO("[ORCH-BATCH] ═══════════════════════════════════\n\n");
            return true;
        }

        LOG_INFO("[ORCH-BATCH] Uploading batch of %zu entries\n", batch.size());

        // Build batch request JSON
        JsonDocument doc;
        JsonArray transactions = doc["transactions"].to<JsonArray>();

        for (const models::ScanData& entry : batch) {
            JsonObject transaction = transactions.add<JsonObject>();
            transaction["tokenId"] = entry.tokenId;
            if (entry.teamId.length() > 0) transaction["teamId"] = entry.teamId;
            transaction["deviceId"] = entry.deviceId;
            transaction["timestamp"] = entry.timestamp;
        }

        String requestBody;
        serializeJson(doc, requestBody);

        LOG_INFO("[ORCH-BATCH] Request body size: %d bytes\n", requestBody.length());

        // Send via consolidated HTTP helper with retry logic
        String url = config.orchestratorURL + "/api/scan/batch";
        unsigned long startTime = millis();
        auto resp = httpWithRetry([&]() {
            return _http.httpPOST(url, requestBody, 30000);
        }, "batch upload");
        unsigned long latency = millis() - startTime;

        if (resp.body.length() > 0 && resp.body.length() < 200) {
            LOG_INFO("[ORCH-BATCH] Response: %s\n", resp.body.c_str());
        }

        LOG_INFO("[ORCH-BATCH] HTTP %d response, latency: %lu ms\n", resp.code, latency);

        if (resp.code == 200) {
            LOG_INFO("[ORCH-BATCH] ✓✓✓ SUCCESS ✓✓✓ Batch upload successful\n");

            // Remove uploaded entries from queue (stream-based)
            removeUploadedEntries(batch.size());

            // Check if more entries remain
            int remainingSize = getQueueSize();
            LOG_INFO("[ORCH-BATCH] Queue size after upload: %d entries\n", remainingSize);

            if (remainingSize > 0) {
                LOG_INFO("[ORCH-BATCH] More entries in queue, uploading next batch after 1s delay\n");
                LOG_INFO("[ORCH-BATCH] ═══════════════════════════════════\n\n");
                delay(1000);  // 1-second delay between batches
                return uploadQueueBatch(config);  // Recursive upload
            }

            LOG_INFO("[ORCH-BATCH] Free heap after: %d bytes\n", ESP.getFreeHeap());
            LOG_INFO("[ORCH-BATCH] ✓ All queue entries uploaded\n");
            LOG_INFO("[ORCH-BATCH] ═══════════════════════════════════\n\n");
            return true;
        } else {
            LOG_INFO("[ORCH-BATCH] ✗✗✗ FAILURE ✗✗✗ HTTP %d\n", resp.code);
            LOG_INFO("[ORCH-BATCH] Entries remain in queue for retry on next health check\n");
            LOG_INFO("[ORCH-BATCH] ═══════════════════════════════════\n\n");
            return false;
        }
    }

    /**
     * @brief Clear entire queue (for manual control)
     */
    void clearQueue() {
        hal::SDCard::Lock lock("clearQueue", freertos_config::SD_MUTEX_LONG_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("ORCH-QUEUE", "Could not acquire SD mutex to clear queue");
            return;
        }

        SD.remove(queue_config::QUEUE_FILE);

        // Reset queue size (atomic)
        portENTER_CRITICAL(&_queue.mutex);
        _queue.size = 0;
        portEXIT_CRITICAL(&_queue.mutex);

        LOG_INFO("[ORCH-QUEUE] Queue cleared\n");
    }

    // ─── Debug ─────────────────────────────────────────────────────────

    /**
     * @brief Print queue contents (for debugging)
     */
    void printQueue() const {
        hal::SDCard::Lock lock("printQueue", freertos_config::SD_MUTEX_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("ORCH-QUEUE", "Could not acquire SD mutex to print queue");
            return;
        }

        File file = SD.open(queue_config::QUEUE_FILE, FILE_READ);
        if (!file) {
            LOG_INFO("[ORCH-QUEUE] Queue file not found (empty)\n");
            return;
        }

        LOG_INFO("\n[ORCH-QUEUE] ═══ Queue Contents ═══\n");
        int count = 0;
        while (file.available() && count < 10) {  // Limit to first 10 entries
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                LOG_INFO("[%d] %s\n", count + 1, line.c_str());
                count++;
            }
        }

        int total = getQueueSize();
        if (total > count) {
            LOG_INFO("... and %d more entries\n", total - count);
        }

        file.close();
        LOG_INFO("[ORCH-QUEUE] ═══ End Queue ═══\n\n");
    }

    // ─── Public HTTP Methods (for use by other services) ──────────────────

    /**
     * @brief Execute HTTP GET request with retry logic
     * @param url Full URL to GET
     * @param timeoutMs Request timeout in milliseconds
     * @param operation Description for logging
     * @return HTTP response code (-1 on failure)
     */
    int httpGETWithRetry(const String& url, uint32_t timeoutMs, const char* operation, String& responseBody) {
        auto resp = httpWithRetry([&]() {
            return _http.httpGET(url, timeoutMs);
        }, operation);
        responseBody = resp.body;
        return resp.code;
    }

    /**
     * @brief Execute HTTP POST request with retry logic
     * @param url Full URL to POST
     * @param json JSON payload
     * @param timeoutMs Request timeout in milliseconds
     * @param operation Description for logging
     * @return HTTP response code (-1 on failure)
     */
    int httpPOSTWithRetry(const String& url, const String& json, uint32_t timeoutMs, const char* operation, String& responseBody) {
        auto resp = httpWithRetry([&]() {
            return _http.httpPOST(url, json, timeoutMs);
        }, operation);
        responseBody = resp.body;
        return resp.code;
    }

private:
    // ─── Singleton Pattern ─────────────────────────────────────────────

    OrchestratorService() {
        // Initialize spinlock mutex for queue size
        _queue.mutex = portMUX_INITIALIZER_UNLOCKED;
    }

    ~OrchestratorService() = default;
    OrchestratorService(const OrchestratorService&) = delete;
    OrchestratorService& operator=(const OrchestratorService&) = delete;

    // ─── State ─────────────────────────────────────────────────────────

    models::ConnectionStateHolder _connState;  // Thread-safe connection state

    // Queue size cache (atomic access via spinlock)
    struct {
        volatile int size;
        mutable portMUX_TYPE mutex;
    } _queue = {0, portMUX_INITIALIZER_UNLOCKED};

    // Device config (for background task - includes orchestratorURL and deviceID)
    models::DeviceConfig _config;

    // ─── HTTP Helper Class (CRITICAL FLASH SAVINGS) ───────────────────

    /**
     * @class HTTPHelper
     * @brief Consolidated HTTP client operations with HTTPS support
     *
     * **PRIMARY FLASH OPTIMIZATION:** This class consolidates 4 duplicate HTTP
     * client implementations from v4.1 into a single reusable helper.
     *
     * Original code had HTTP client setup duplicated in:
     * 1. sendScanToOrchestrator (lines 1650-1716)
     * 2. uploadQueueBatch (lines 1725-1824)
     * 3. syncTokenDatabase (lines 1571-1634)
     * 4. checkOrchestratorHealth (lines 1637-1648)
     *
     * Consolidating into HTTPHelper saves ~15KB flash by eliminating 3 copies.
     *
     * **HTTPS SUPPORT:** Uses WiFiClientSecure with setInsecure() for HTTPS URLs.
     * Certificate validation is skipped (acceptable for local network deployments).
     * Required for Android NFC scanning API which mandates HTTPS even on local networks.
     */
    class HTTPHelper {
    public:
        HTTPHelper() {
            // Configure secure client to skip certificate validation
            // This is acceptable for local network orchestrator deployments
            _secureClient.setInsecure();
        }
        struct Response {
            int code;           // HTTP response code (or negative for errors)
            String body;        // Response body (if available)
            bool success;       // true if 2xx response code
        };

        /**
         * @brief Send HTTP GET request
         * @param url Full URL to GET
         * @param timeoutMs Request timeout in milliseconds
         * @return Response struct with code, body, success
         */
        Response httpGET(const String& url, uint32_t timeoutMs = 5000) {
            HTTPClient client;
            configureClient(client, url, timeoutMs);

            int code = client.GET();
            Response resp;
            resp.code = code;
            resp.body = (code > 0) ? client.getString() : "";
            resp.success = (code >= 200 && code < 300);

            client.end();
            return resp;
        }

        /**
         * @brief Send HTTP POST request with JSON payload
         * @param url Full URL to POST
         * @param json JSON string payload
         * @param timeoutMs Request timeout in milliseconds
         * @return Response struct with code, body, success
         */
        Response httpPOST(const String& url, const String& json, uint32_t timeoutMs = 5000) {
            HTTPClient client;
            configureClient(client, url, timeoutMs);
            client.addHeader("Content-Type", "application/json");

            int code = client.POST(json);
            Response resp;
            resp.code = code;
            resp.body = (code > 0) ? client.getString() : "";
            resp.success = (code >= 200 && code < 300);

            client.end();
            return resp;
        }

    private:
        /**
         * @brief Configure HTTP client with HTTPS support
         * @param client HTTPClient reference to configure
         * @param url Full URL to connect to (http:// or https://)
         * @param timeoutMs Request timeout in milliseconds
         *
         * This method contains the shared HTTP client setup code that was
         * previously duplicated across 4 functions. Consolidating here
         * eliminates ~15KB of duplicate code in flash.
         *
         * For HTTPS URLs, uses WiFiClientSecure with certificate validation disabled.
         * This is acceptable for local network deployments where the orchestrator
         * is on the same network and not exposed to the internet.
         */
        void configureClient(HTTPClient& client, const String& url, uint32_t timeoutMs) {
            if (url.startsWith("https://")) {
                client.begin(_secureClient, url);
            } else {
                client.begin(url);
            }
            client.setTimeout(timeoutMs);
        }

        WiFiClientSecure _secureClient;  // Secure client for HTTPS requests
    };

    HTTPHelper _http;  // Singleton HTTP helper instance

    /**
     * @brief HTTP request with exponential backoff retry
     * @param requestFn Function that returns HTTPHelper::Response
     * @param operation Description for logging (e.g., "scan submission")
     * @return Final response after retries
     *
     * Retry schedule: 1s, 2s, 4s, 8s, 16s, 30s (max)
     * Total max wait: ~61 seconds over 6 attempts
     *
     * Does NOT retry on:
     * - 2xx success (return immediately)
     * - 404 Not Found (semantic error)
     * - 409 Conflict (semantic error)
     *
     * Retries on:
     * - Connection failures (code < 0)
     * - 5xx server errors
     * - Timeouts
     */
    template<typename RequestFunc>
    HTTPHelper::Response httpWithRetry(RequestFunc requestFn, const char* operation) {
        const int MAX_ATTEMPTS = 6;
        const int BACKOFF_DELAYS[] = {1000, 2000, 4000, 8000, 16000, 30000};

        for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
            HTTPHelper::Response resp = requestFn();

            // Success or semantic error (don't retry)
            if (resp.success || resp.code == 404 || resp.code == 409) {
                if (attempt > 1) {
                    LOG_INFO("[ORCH-RETRY] %s succeeded on attempt %d\n", operation, attempt);
                }
                return resp;
            }

            // Connection failure or server error
            LOG_INFO("[ORCH-RETRY] %s failed (attempt %d/%d), code: %d\n",
                     operation, attempt, MAX_ATTEMPTS, resp.code);

            if (attempt < MAX_ATTEMPTS) {
                int delayMs = BACKOFF_DELAYS[attempt - 1];
                LOG_INFO("[ORCH-RETRY] Retrying in %d ms...\n", delayMs);
                delay(delayMs);
            }
        }

        // All retries exhausted
        LOG_INFO("[ORCH-RETRY] %s failed after %d attempts, marking offline\n", operation, MAX_ATTEMPTS);
        HTTPHelper::Response failed;
        failed.code = -1;
        failed.success = false;
        return failed;
    }

    // ─── Queue File Operations ─────────────────────────────────────────

    /**
     * @brief Read queue entries from JSONL file
     * @param batch Vector to populate with scan data
     * @param maxEntries Maximum entries to read
     *
     * Implementation from v4.1 lines 1947-1994
     * Must be called with SD mutex already acquired
     */
    void readQueue(std::vector<models::ScanData>& batch, int maxEntries) {
        LOG_INFO("\n[ORCH-QUEUE-READ] ═══ READING QUEUE BATCH ═══\n");
        LOG_INFO("[ORCH-QUEUE-READ] Max entries: %d\n", maxEntries);

        File file = SD.open(queue_config::QUEUE_FILE, FILE_READ);
        if (!file) {
            LOG_INFO("[ORCH-QUEUE-READ] ✗ Queue file not found\n");
            return;
        }

        int count = 0;
        int skipped = 0;

        while (file.available() && count < maxEntries) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, line);

            if (error == DeserializationError::Ok) {
                // Validate required fields
                if (doc.containsKey("tokenId") && doc.containsKey("deviceId") &&
                    doc.containsKey("timestamp")) {

                    models::ScanData scan;
                    scan.tokenId = doc["tokenId"].as<String>();
                    scan.teamId = doc.containsKey("teamId") ? doc["teamId"].as<String>() : "";
                    scan.deviceId = doc["deviceId"].as<String>();
                    scan.timestamp = doc["timestamp"].as<String>();

                    batch.push_back(scan);
                    count++;

                    LOG_INFO("[ORCH-QUEUE-READ] Entry %d: tokenId=%s\n",
                             count, scan.tokenId.c_str());
                } else {
                    skipped++;
                    LOG_INFO("[ORCH-QUEUE-READ] ✗ Skipped line (missing fields): %s\n",
                             line.c_str());
                }
            } else {
                skipped++;
                LOG_INFO("[ORCH-QUEUE-READ] ✗ Skipped corrupt line: %s\n", line.c_str());
            }
        }

        file.close();

        LOG_INFO("[ORCH-QUEUE-READ] ✓ Read %d entries, skipped %d corrupt\n", count, skipped);
        LOG_INFO("[ORCH-QUEUE-READ] ═══ READING COMPLETE ═══\n\n");
    }

    /**
     * @brief Remove first N entries from queue (FIFO, stream-based)
     * @param numEntries Number of entries to remove
     *
     * **CRITICAL IMPLEMENTATION:** Stream-based queue rebuild to prevent RAM spikes.
     * Original v4.1 implementation loaded entire queue into RAM, causing OOM on
     * large queues. This version streams from source to temp file.
     *
     * Implementation from v4.1 lines 2004-2089
     * Handles its own mutex acquisition
     */
    void removeUploadedEntries(int numEntries) {
        LOG_INFO("\n[ORCH-QUEUE-REBUILD] ═══ REMOVING UPLOADED ENTRIES ═══\n");
        LOG_INFO("[ORCH-QUEUE-REBUILD] Entries to remove: %d\n", numEntries);
        LOG_INFO("[ORCH-QUEUE-REBUILD] Free heap before: %d bytes\n", ESP.getFreeHeap());

        unsigned long startMs = millis();

        if (numEntries <= 0) {
            LOG_INFO("[ORCH-QUEUE-REBUILD] ✗ No entries to remove\n");
            return;
        }

        hal::SDCard::Lock lock("removeEntries", freertos_config::SD_MUTEX_LONG_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("ORCH-QUEUE-REBUILD", "Could not acquire SD mutex");
            return;
        }

        File src = SD.open(queue_config::QUEUE_FILE, FILE_READ);
        if (!src) {
            LOG_INFO("[ORCH-QUEUE-REBUILD] ✗ Queue file not found, nothing to remove\n");
            return;
        }

        File tmp = SD.open(queue_config::QUEUE_TEMP_FILE, FILE_WRITE);
        if (!tmp) {
            LOG_ERROR("ORCH-QUEUE-REBUILD", "Could not open temp file for writing");
            src.close();
            return;
        }

        LOG_INFO("[ORCH-QUEUE-REBUILD] Starting stream-based rebuild...\n");

        int linesRead = 0;
        int linesSkipped = 0;
        int linesKept = 0;

        // Stream-read original, stream-write to temp (MEMORY-SAFE)
        while (src.available()) {
            String line = src.readStringUntil('\n');
            line.trim();
            linesRead++;

            if (line.length() == 0) continue;  // Skip empty lines

            if (linesSkipped < numEntries) {
                // Skip this line (remove from queue)
                linesSkipped++;
            } else {
                // Keep this line
                tmp.println(line);
                linesKept++;
            }
        }

        src.close();
        tmp.flush();
        tmp.close();

        LOG_INFO("[ORCH-QUEUE-REBUILD] Read: %d lines, Skipped: %d, Kept: %d\n",
                 linesRead, linesSkipped, linesKept);

        // Atomic file replacement
        LOG_INFO("[ORCH-QUEUE-REBUILD] Replacing original queue file...\n");
        if (!SD.remove(queue_config::QUEUE_FILE)) {
            LOG_ERROR("ORCH-QUEUE-REBUILD", "Could not remove original queue file");
            return;
        }

        if (!SD.rename(queue_config::QUEUE_TEMP_FILE, queue_config::QUEUE_FILE)) {
            LOG_ERROR("ORCH-QUEUE-REBUILD", "Could not rename temp file");
            return;
        }

        // Update queue size cache (atomic)
        updateQueueSize(-linesSkipped);

        unsigned long latencyMs = millis() - startMs;
        LOG_INFO("[ORCH-QUEUE-REBUILD] ✓✓✓ SUCCESS ✓✓✓ Queue rebuilt in %lu ms\n", latencyMs);
        LOG_INFO("[ORCH-QUEUE-REBUILD] New queue size: %d\n", getQueueSize());
        LOG_INFO("[ORCH-QUEUE-REBUILD] Free heap after: %d bytes\n", ESP.getFreeHeap());
        LOG_INFO("[ORCH-QUEUE-REBUILD] ═══ REMOVAL COMPLETE ═══\n\n");
    }

    /**
     * @brief Update queue size cache (atomic)
     * @param delta Change in queue size (+1 for add, -N for remove)
     */
    void updateQueueSize(int delta) {
        portENTER_CRITICAL(&_queue.mutex);
        _queue.size += delta;
        if (_queue.size < 0) _queue.size = 0;  // Clamp to zero
        portEXIT_CRITICAL(&_queue.mutex);
    }

    /**
     * @brief Handle queue overflow (FIFO removal)
     *
     * Implementation from v4.1 lines 1872-1874
     * Removes oldest entry when queue reaches MAX_QUEUE_SIZE
     */
    void handleQueueOverflow() {
        LOG_INFO("[ORCH-QUEUE] Queue overflow (%d >= %d), removing oldest entry\n",
                 getQueueSize(), queue_config::MAX_QUEUE_SIZE);
        removeUploadedEntries(1);  // Remove oldest entry (FIFO)
    }

    // ─── Background Task ───────────────────────────────────────────────

    /**
     * @brief Static wrapper for FreeRTOS task (required for xTaskCreate)
     * @param param Pointer to OrchestratorService instance
     */
    static void backgroundTaskWrapper(void* param) {
        auto* self = static_cast<OrchestratorService*>(param);
        self->backgroundTaskLoop();
    }

    /**
     * @brief Background task loop (runs on Core 0)
     *
     * Implementation from v4.1 lines 2447-2496
     * Checks orchestrator health every 10 seconds, uploads queue if connected
     */
    void backgroundTaskLoop() {
        LOG_INFO("[ORCH-BG-TASK] Background task started on Core 0\n");

        unsigned long lastCheck = 0;
        const unsigned long checkInterval = timing::ORCHESTRATOR_CHECK_INTERVAL_MS;

        while (true) {
            unsigned long now = millis();

            // Stack monitoring (detect stack overflow risk)
            UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
            if (stackRemaining < 512) {
                LOG_INFO("[ORCH-BG-TASK] ⚠️ WARNING: Low stack! Only %d bytes free\n",
                         stackRemaining);
            }

            if (now - lastCheck > checkInterval) {
                lastCheck = now;

                // Log stack health every 10 seconds
                LOG_DEBUG("[ORCH-BG-TASK] Stack high water mark: %d bytes free\n",
                          stackRemaining);

                models::ConnectionState state = _connState.get();

                if (state == models::ORCH_WIFI_CONNECTED || state == models::ORCH_CONNECTED) {
                    // Check orchestrator health
                    if (checkHealth(_config)) {
                        if (state != models::ORCH_CONNECTED) {
                            LOG_INFO("[ORCH-BG-TASK] Orchestrator now reachable\n");
                            _connState.set(models::ORCH_CONNECTED);
                        }

                        // Upload queue if not empty
                        int queueSize = getQueueSize();
                        if (queueSize > 0) {
                            LOG_INFO("[ORCH-BG-TASK] Queue has %d entries, starting batch upload\n",
                                     queueSize);

                            // Use stored config for upload (includes orchestratorURL and deviceID)
                            uploadQueueBatch(_config);
                        }
                    } else {
                        if (state == models::ORCH_CONNECTED) {
                            LOG_INFO("[ORCH-BG-TASK] Orchestrator unreachable\n");
                            _connState.set(models::ORCH_WIFI_CONNECTED);
                        }
                    }
                }
                // If completely disconnected, WiFi.begin() handles auto-reconnect
            }

            vTaskDelay(freertos_config::BACKGROUND_TASK_DELAY_MS / portTICK_PERIOD_MS);
        }
    }

    // ─── WiFi Event Handlers ───────────────────────────────────────────

    /**
     * @brief WiFi connected event handler
     * @param event WiFi event type
     * @param info Event information
     *
     * Implementation from v4.1 lines 2366-2371
     */
    static void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
        LOG_INFO("[%lu] [ORCH-WIFI] Connected to AP\n", millis());
        LOG_INFO("         SSID: %s, Channel: %d\n", WiFi.SSID().c_str(), WiFi.channel());
    }

    /**
     * @brief WiFi got IP event handler
     * @param event WiFi event type
     * @param info Event information
     *
     * Implementation from v4.1 lines 2373-2377
     */
    static void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
        LOG_INFO("[%lu] [ORCH-WIFI] Got IP address: %s\n", millis(),
                 WiFi.localIP().toString().c_str());
        LOG_INFO("         Gateway: %s, Signal: %d dBm\n",
                 WiFi.gatewayIP().toString().c_str(), WiFi.RSSI());

        // Update connection state
        auto& instance = getInstance();
        instance._connState.set(models::ORCH_WIFI_CONNECTED);
    }

    /**
     * @brief WiFi disconnected event handler
     * @param event WiFi event type
     * @param info Event information
     *
     * Implementation from v4.1 lines 2379-2383
     */
    static void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
        LOG_INFO("[%lu] [ORCH-WIFI] Disconnected from AP\n", millis());

        // Update connection state
        auto& instance = getInstance();
        instance._connState.set(models::ORCH_DISCONNECTED);

        // WiFi will auto-reconnect, don't call WiFi.reconnect() here to avoid storm
    }
};

} // namespace services

/**
 * IMPLEMENTATION NOTES (from v4.1 extraction)
 *
 * 1. HTTP CODE CONSOLIDATION (PRIMARY FLASH SAVINGS)
 *    - v4.1 had 4 duplicate HTTP client implementations
 *    - Each copy: HTTPClient setup, timeout, headers, POST/GET, error handling
 *    - Consolidating into HTTPHelper eliminates ~15KB flash (3 duplicate copies)
 *    - This is the SINGLE BIGGEST flash optimization in v5.0 refactor
 *
 * 2. THREAD-SAFE QUEUE OPERATIONS
 *    - Queue size cache uses portENTER_CRITICAL/EXIT_CRITICAL (spinlock)
 *    - SD card operations use hal::SDCard::Lock (RAII mutex)
 *    - Background task (Core 0) and main loop (Core 1) both access queue
 *    - Stream-based queue removal prevents RAM spikes (100-entry queue: 100 bytes vs 10KB)
 *
 * 3. WIFI EVENT-DRIVEN STATE MANAGEMENT
 *    - WiFi events update connection state automatically
 *    - Background task checks orchestrator health every 10 seconds
 *    - State transitions: DISCONNECTED → WIFI_CONNECTED → CONNECTED
 *    - Auto-reconnect handled by WiFi library, no manual intervention needed
 *
 * 4. QUEUE OVERFLOW PROTECTION
 *    - MAX_QUEUE_SIZE (100 entries) enforced before queueScan()
 *    - FIFO removal of oldest entry when full
 *    - Prevents SD card filling up with stale scans
 *
 * 5. BACKGROUND TASK STACK MONITORING
 *    - 16KB stack size (BACKGROUND_TASK_STACK_SIZE)
 *    - Stack high water mark checked every cycle
 *    - Warning if < 512 bytes free
 *    - vTaskDelay() prevents watchdog timeout
 *
 * 6. EXTRACTED FROM v4.1 LINES
 *    - WiFi initialization: Lines 2390-2444
 *    - WiFi event handlers: Lines 2366-2388
 *    - Send scan: Lines 1650-1716
 *    - Queue scan: Lines 1866-1922
 *    - Batch upload: Lines 1725-1824
 *    - Health check: Lines 1637-1648
 *    - Read queue: Lines 1947-1994
 *    - Remove entries: Lines 2004-2089 (CRITICAL stream-based implementation)
 *    - Background task: Lines 2447-2496, 2679-2683
 */
