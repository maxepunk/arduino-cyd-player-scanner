#pragma once

/**
 * @file ConfigService.h
 * @brief Configuration management service for ALNScanner v5.0
 *
 * Handles loading, validation, saving, and runtime editing of device configuration.
 * Integrates with hal::SDCard for persistent storage and models::DeviceConfig for data structure.
 *
 * Key Features:
 * - Singleton pattern for global access
 * - Load configuration from SD card (/config.txt)
 * - Save configuration to SD card
 * - Runtime configuration editing (in-memory)
 * - Device ID auto-generation from MAC address
 * - Full validation of all config fields
 * - Thread-safe SD card access via hal::SDCard
 *
 * Extracted from v4.1 monolithic codebase:
 * - Lines 1295-1320: generateDeviceId()
 * - Lines 1323-1440: parseConfigFile()
 * - Lines 1443-1542: validateConfig()
 * - Lines 3157-3288: Runtime config editing (SET_CONFIG, SAVE_CONFIG)
 */

#include <Arduino.h>
#include <esp_system.h>
#include <esp_mac.h>
#include "../hal/SDCard.h"
#include "../models/Config.h"
#include "../config.h"

namespace services {

/**
 * @class ConfigService
 * @brief Singleton configuration manager
 *
 * Usage Pattern:
 * @code
 * auto& config = services::ConfigService::getInstance();
 *
 * // Load from SD card
 * if (config.loadFromSD()) {
 *     Serial.println("Config loaded successfully");
 * }
 *
 * // Access current configuration
 * models::DeviceConfig& cfg = config.getConfig();
 * Serial.println(cfg.wifiSSID);
 *
 * // Runtime editing
 * config.set("WIFI_SSID", "NewNetwork");
 * config.set("TEAM_ID", "123");
 *
 * // Save to SD card
 * if (config.saveToSD()) {
 *     Serial.println("Config saved successfully");
 * }
 *
 * // Validation
 * if (config.validate()) {
 *     Serial.println("Config is valid");
 * }
 * @endcode
 */
class ConfigService {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the global ConfigService instance
     */
    static ConfigService& getInstance() {
        static ConfigService instance;
        return instance;
    }

    /**
     * @brief Load configuration from SD card (/config.txt)
     * @return true if configuration loaded and validated successfully, false otherwise
     *
     * This method:
     * 1. Acquires SD mutex for thread-safe access
     * 2. Opens /config.txt on SD card
     * 3. Parses each line as KEY=VALUE pairs
     * 4. Skips empty lines and comments (# or ;)
     * 5. Updates internal DeviceConfig structure
     * 6. Logs parsing progress and results
     * 7. Validates required fields
     * 8. Auto-generates device ID if not set
     */
    inline bool loadFromSD() {
        LOG_INFO("\n[CONFIG] === CONFIG LOADING START ===\n");
        LOG_INFO("[CONFIG] Free heap: %d bytes\n", ESP.getFreeHeap());

        // Acquire SD card lock
        hal::SDCard::Lock lock("ConfigService::loadFromSD");
        if (!lock.acquired()) {
            LOG_ERROR("CONFIG", "Failed to acquire SD lock");
            return false;
        }

        // Open config file
        File file = SD.open(paths::CONFIG_FILE, FILE_READ);
        if (!file) {
            LOG_ERROR("CONFIG", "config.txt not found on SD card");
            return false;
        }

        LOG_INFO("[CONFIG] config.txt opened successfully\n");
        int lineNum = 0;
        int parsedKeys = 0;

        // Parse line by line
        while (file.available()) {
            String line = file.readStringUntil('\n');
            lineNum++;
            line.trim();

            // Skip empty lines
            if (line.length() == 0) {
                LOG_DEBUG("[CONFIG] Line %d: (empty, skipped)\n", lineNum);
                continue;
            }

            // Skip comments
            if (line.startsWith("#") || line.startsWith(";")) {
                LOG_DEBUG("[CONFIG] Line %d: (comment, skipped)\n", lineNum);
                continue;
            }

            // Parse KEY=VALUE
            int sepIndex = line.indexOf('=');
            if (sepIndex == -1) {
                LOG_DEBUG("[CONFIG] Line %d: (no '=', skipped)\n", lineNum);
                continue;
            }

            String key = line.substring(0, sepIndex);
            String value = line.substring(sepIndex + 1);
            key.trim();
            value.trim();

            LOG_DEBUG("[CONFIG] Line %d: '%s' = '%s'\n", lineNum, key.c_str(), value.c_str());

            // Update configuration
            if (key == "WIFI_SSID") {
                _config.wifiSSID = value;
                parsedKeys++;
            } else if (key == "WIFI_PASSWORD") {
                _config.wifiPassword = value;
                parsedKeys++;
            } else if (key == "ORCHESTRATOR_URL") {
                _config.orchestratorURL = value;
                parsedKeys++;
            } else if (key == "TEAM_ID") {
                _config.teamID = value;
                parsedKeys++;
            } else if (key == "DEVICE_ID") {
                _config.deviceID = value;
                parsedKeys++;
            } else if (key == "SYNC_TOKENS") {
                _config.syncTokens = !(value.equalsIgnoreCase("false") || value == "0");
                LOG_DEBUG("[CONFIG]       SYNC_TOKENS set to %s\n", _config.syncTokens ? "TRUE" : "FALSE");
                parsedKeys++;
            } else if (key == "DEBUG_MODE") {
                _config.debugMode = !(value.equalsIgnoreCase("false") || value == "0");
                LOG_DEBUG("[CONFIG]       DEBUG_MODE set to %s\n", _config.debugMode ? "TRUE" : "FALSE");
                parsedKeys++;
            } else {
                LOG_DEBUG("[CONFIG]         (unknown key, ignored)\n");
            }
        }

        file.close();

        LOG_INFO("[CONFIG] Parsed %d lines, %d recognized keys\n", lineNum, parsedKeys);
        LOG_INFO("[CONFIG] Results:\n");
        LOG_INFO("  WIFI_SSID: %s\n", _config.wifiSSID.length() > 0 ? _config.wifiSSID.c_str() : "(not set)");
        LOG_INFO("  WIFI_PASSWORD: %s\n", _config.wifiPassword.length() > 0 ? "***" : "(not set)");
        LOG_INFO("  ORCHESTRATOR_URL: %s\n", _config.orchestratorURL.length() > 0 ? _config.orchestratorURL.c_str() : "(not set)");
        LOG_INFO("  TEAM_ID: %s\n", _config.teamID.length() > 0 ? _config.teamID.c_str() : "(not set)");
        LOG_INFO("  DEVICE_ID: %s\n", _config.deviceID.length() > 0 ? _config.deviceID.c_str() : "(auto-generate)");
        LOG_INFO("  SYNC_TOKENS: %s\n", _config.syncTokens ? "true" : "false");
        LOG_INFO("  DEBUG_MODE: %s\n", _config.debugMode ? "true" : "false");
        LOG_INFO("[CONFIG] Free heap after parsing: %d bytes\n", ESP.getFreeHeap());

        // Auto-generate device ID if not set
        if (_config.deviceID.length() == 0) {
            _config.deviceID = generateDeviceId();
            LOG_INFO("[CONFIG] Auto-generated device ID: %s\n", _config.deviceID.c_str());
        }

        // Validate configuration
        bool isValid = validate();

        if (isValid) {
            LOG_INFO("[CONFIG] +++ SUCCESS +++ All required fields present\n");
        } else {
            LOG_ERROR("CONFIG", "Missing or invalid required fields");
        }

        LOG_INFO("[CONFIG] === CONFIG LOADING END ===\n\n");
        return isValid;
    }

    /**
     * @brief Save current configuration to SD card (/config.txt)
     * @return true if configuration saved successfully, false otherwise
     *
     * Writes all configuration fields to /config.txt in KEY=VALUE format.
     * Overwrites existing file. Adds header comment with timestamp.
     */
    inline bool saveToSD() {
        LOG_INFO("\n[CONFIG] === CONFIG SAVING START ===\n");

        // Acquire SD card lock
        hal::SDCard::Lock lock("ConfigService::saveToSD", freertos_config::SD_MUTEX_LONG_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("CONFIG", "Failed to acquire SD lock");
            return false;
        }

        // Open config file for writing (overwrites existing)
        File file = SD.open(paths::CONFIG_FILE, FILE_WRITE);
        if (!file) {
            LOG_ERROR("CONFIG", "Failed to open config.txt for writing");
            return false;
        }

        LOG_INFO("[CONFIG] Writing configuration to /config.txt...\n");

        // Write header
        file.println("# NeurAI Memory Scanner Configuration");
        file.println("# Generated by ConfigService::saveToSD()");
        file.println("");

        // Write all configuration fields
        file.printf("WIFI_SSID=%s\n", _config.wifiSSID.c_str());
        file.printf("WIFI_PASSWORD=%s\n", _config.wifiPassword.c_str());
        file.printf("ORCHESTRATOR_URL=%s\n", _config.orchestratorURL.c_str());
        file.printf("TEAM_ID=%s\n", _config.teamID.c_str());

        // Only write DEVICE_ID if set (skip if empty)
        if (_config.deviceID.length() > 0) {
            file.printf("DEVICE_ID=%s\n", _config.deviceID.c_str());
        }

        file.printf("SYNC_TOKENS=%s\n", _config.syncTokens ? "true" : "false");
        file.printf("DEBUG_MODE=%s\n", _config.debugMode ? "true" : "false");

        file.flush();
        file.close();

        LOG_INFO("[CONFIG] +++ SUCCESS +++ Configuration saved to SD card\n");
        LOG_INFO("[CONFIG] === CONFIG SAVING END ===\n\n");
        return true;
    }

    /**
     * @brief Set configuration value by key (runtime editing)
     * @param key Configuration key (WIFI_SSID, TEAM_ID, etc.)
     * @param value New value for the key
     * @return true if key recognized and updated, false if key unknown
     *
     * Changes are in-memory only. Call saveToSD() to persist.
     * Boolean keys accept "true"/"false", "1"/"0", case-insensitive.
     */
    inline bool set(const String& key, const String& value) {
        if (key == "WIFI_SSID") {
            _config.wifiSSID = value;
            return true;
        } else if (key == "WIFI_PASSWORD") {
            _config.wifiPassword = value;
            return true;
        } else if (key == "ORCHESTRATOR_URL") {
            _config.orchestratorURL = value;
            return true;
        } else if (key == "TEAM_ID") {
            _config.teamID = value;
            return true;
        } else if (key == "DEVICE_ID") {
            _config.deviceID = value;
            return true;
        } else if (key == "SYNC_TOKENS") {
            _config.syncTokens = !(value.equalsIgnoreCase("false") || value == "0");
            return true;
        } else if (key == "DEBUG_MODE") {
            _config.debugMode = !(value.equalsIgnoreCase("false") || value == "0");
            return true;
        }

        return false; // Unknown key
    }

    /**
     * @brief Validate current configuration
     * @return true if all fields valid, false if any validation fails
     *
     * Checks:
     * - WIFI_SSID: required, 1-32 characters
     * - WIFI_PASSWORD: optional, 0-63 characters
     * - ORCHESTRATOR_URL: required, starts with "http://", 10-200 characters
     * - TEAM_ID: required, exactly 3 digits
     * - DEVICE_ID: optional, 1-100 characters, alphanumeric + underscore
     * - SYNC_TOKENS: always valid (boolean)
     * - DEBUG_MODE: always valid (boolean)
     */
    inline bool validate() const {
        LOG_INFO("\n[VALIDATE] === CONFIG VALIDATION START ===\n");
        bool isValid = true;

        // Validate WIFI_SSID (required, 1-32 characters)
        if (_config.wifiSSID.length() == 0) {
            LOG_INFO("[VALIDATE] X WIFI_SSID is required\n");
            isValid = false;
        } else if (_config.wifiSSID.length() > limits::MAX_SSID_LENGTH) {
            LOG_INFO("[VALIDATE] X WIFI_SSID too long (max %d characters)\n", limits::MAX_SSID_LENGTH);
            isValid = false;
        } else {
            LOG_DEBUG("[VALIDATE] + WIFI_SSID valid: %s\n", _config.wifiSSID.c_str());
        }

        // Validate WIFI_PASSWORD (optional, 0-63 characters)
        if (_config.wifiPassword.length() > limits::MAX_PASSWORD_LENGTH) {
            LOG_INFO("[VALIDATE] X WIFI_PASSWORD too long (max %d characters)\n", limits::MAX_PASSWORD_LENGTH);
            isValid = false;
        } else {
            LOG_DEBUG("[VALIDATE] + WIFI_PASSWORD valid\n");
        }

        // Validate ORCHESTRATOR_URL (required, starts with http://, 10-200 characters)
        if (_config.orchestratorURL.length() == 0) {
            LOG_INFO("[VALIDATE] X ORCHESTRATOR_URL is required\n");
            isValid = false;
        } else if (!_config.orchestratorURL.startsWith("http://")) {
            LOG_INFO("[VALIDATE] X ORCHESTRATOR_URL must start with http://\n");
            isValid = false;
        } else if (_config.orchestratorURL.length() < 10 || _config.orchestratorURL.length() > 200) {
            LOG_INFO("[VALIDATE] X ORCHESTRATOR_URL invalid length (10-200 characters)\n");
            isValid = false;
        } else {
            LOG_DEBUG("[VALIDATE] + ORCHESTRATOR_URL valid: %s\n", _config.orchestratorURL.c_str());
        }

        // Validate TEAM_ID (required, exactly 3 digits)
        if (_config.teamID.length() == 0) {
            LOG_INFO("[VALIDATE] X TEAM_ID is required\n");
            isValid = false;
        } else if (_config.teamID.length() != limits::TEAM_ID_LENGTH) {
            LOG_INFO("[VALIDATE] X TEAM_ID must be exactly %d digits\n", limits::TEAM_ID_LENGTH);
            isValid = false;
        } else {
            // Check all characters are digits
            bool allDigits = true;
            for (int i = 0; i < limits::TEAM_ID_LENGTH; i++) {
                if (!isDigit(_config.teamID[i])) {
                    allDigits = false;
                    break;
                }
            }
            if (!allDigits) {
                LOG_INFO("[VALIDATE] X TEAM_ID must contain only digits\n");
                isValid = false;
            } else {
                LOG_DEBUG("[VALIDATE] + TEAM_ID valid: %s\n", _config.teamID.c_str());
            }
        }

        // Validate DEVICE_ID (optional, 1-100 characters, alphanumeric + underscore)
        if (_config.deviceID.length() > 0) {
            if (_config.deviceID.length() > limits::MAX_DEVICE_ID_LENGTH) {
                LOG_INFO("[VALIDATE] X DEVICE_ID too long (max %d characters)\n", limits::MAX_DEVICE_ID_LENGTH);
                isValid = false;
            } else {
                // Check all characters are alphanumeric or underscore
                bool validChars = true;
                for (unsigned int i = 0; i < _config.deviceID.length(); i++) {
                    char c = _config.deviceID[i];
                    if (!isAlphaNumeric(c) && c != '_') {
                        validChars = false;
                        break;
                    }
                }
                if (!validChars) {
                    LOG_INFO("[VALIDATE] X DEVICE_ID must contain only letters, numbers, and underscores\n");
                    isValid = false;
                } else {
                    LOG_DEBUG("[VALIDATE] + DEVICE_ID valid: %s\n", _config.deviceID.c_str());
                }
            }
        }

        // SYNC_TOKENS and DEBUG_MODE are always valid (boolean)
        LOG_DEBUG("[VALIDATE] + SYNC_TOKENS valid: %s\n", _config.syncTokens ? "true" : "false");
        LOG_DEBUG("[VALIDATE] + DEBUG_MODE valid: %s\n", _config.debugMode ? "true" : "false");

        if (isValid) {
            LOG_INFO("[VALIDATE] +++ SUCCESS +++ All fields valid\n");
        } else {
            LOG_INFO("[VALIDATE] XXX FAILURE XXX Configuration has errors\n");
        }

        LOG_INFO("[VALIDATE] === CONFIG VALIDATION END ===\n\n");
        return isValid;
    }

    /**
     * @brief Get reference to current configuration
     * @return Reference to internal DeviceConfig structure
     *
     * Allows direct access to configuration fields:
     * config.getConfig().wifiSSID, config.getConfig().teamID, etc.
     */
    inline models::DeviceConfig& getConfig() {
        return _config;
    }

    /**
     * @brief Get const reference to current configuration
     * @return Const reference to internal DeviceConfig structure
     */
    inline const models::DeviceConfig& getConfig() const {
        return _config;
    }

    /**
     * @brief Generate device ID from ESP32 MAC address
     * @return Device ID string in format "SCANNER_XXXXXXXXXXXX" (12 hex digits)
     *
     * Uses esp_read_mac() to read MAC from eFuse (works before WiFi init).
     * Format: "SCANNER_" + 12 uppercase hex digits (e.g., "SCANNER_A4CF12F8E390")
     * Returns "SCANNER_ERROR" if MAC read fails.
     *
     * Extracted from v4.1 lines 1295-1320
     */
    inline String generateDeviceId() {
        LOG_INFO("[DEVID] Generating device ID from MAC address...\n");
        LOG_DEBUG("[DEVID] Free heap before generation: %d bytes\n", ESP.getFreeHeap());

        uint8_t mac[6];
        esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);

        if (err != ESP_OK) {
            LOG_ERROR("DEVID", "esp_read_mac() failed");
            Serial.printf("        Error code: %d\n", err);
            return "SCANNER_ERROR";
        }

        LOG_DEBUG("[DEVID] MAC bytes read: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        // Buffer size: "SCANNER_" (8) + 12 hex digits + NULL = 21 bytes minimum
        char deviceId[32];
        sprintf(deviceId, "SCANNER_%02X%02X%02X%02X%02X%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        LOG_INFO("[DEVID] Generated device ID: %s\n", deviceId);
        LOG_DEBUG("[DEVID] Free heap after generation: %d bytes\n", ESP.getFreeHeap());

        return String(deviceId);
    }

private:
    // Singleton pattern - private constructors
    ConfigService() = default;
    ~ConfigService() = default;
    ConfigService(const ConfigService&) = delete;
    ConfigService& operator=(const ConfigService&) = delete;

    // Internal configuration storage
    models::DeviceConfig _config;
};

} // namespace services

/**
 * IMPLEMENTATION NOTES (from v4.1 extraction)
 *
 * 1. FILE FORMAT: /config.txt (SD card root)
 *    - KEY=VALUE pairs, one per line
 *    - Comments start with # or ;
 *    - Empty lines ignored
 *    - No whitespace sensitivity (trimmed automatically)
 *
 * 2. REQUIRED FIELDS
 *    - WIFI_SSID: Network name (1-32 characters)
 *    - ORCHESTRATOR_URL: Server URL (must start with "http://")
 *    - TEAM_ID: Team identifier (exactly 3 digits, 001-999)
 *
 * 3. OPTIONAL FIELDS
 *    - WIFI_PASSWORD: Network password (0-63 characters, can be empty for open networks)
 *    - DEVICE_ID: Custom device identifier (auto-generated from MAC if not set)
 *    - SYNC_TOKENS: Enable/disable token database sync (default: true)
 *    - DEBUG_MODE: Enable/disable debug features (default: false)
 *
 * 4. BOOLEAN PARSING
 *    - Accepted as TRUE: "true", "1", "TRUE", any non-zero/non-false value
 *    - Accepted as FALSE: "false", "0", "FALSE"
 *    - Case-insensitive
 *
 * 5. DEVICE ID AUTO-GENERATION
 *    - Uses esp_read_mac() to read MAC from eFuse
 *    - Works before WiFi initialization (reads hardware directly)
 *    - Format: "SCANNER_" + 12 uppercase hex digits
 *    - Example: "SCANNER_A4CF12F8E390"
 *    - Buffer overflow fix: v4.1 increased buffer from 20 to 32 bytes
 *
 * 6. VALIDATION RULES
 *    - WIFI_SSID: 1-32 characters (WiFi spec limit)
 *    - WIFI_PASSWORD: 0-63 characters (WiFi spec limit)
 *    - ORCHESTRATOR_URL: 10-200 characters, must start with "http://"
 *    - TEAM_ID: exactly 3 digits, all numeric
 *    - DEVICE_ID: 1-100 characters, alphanumeric + underscore only
 *
 * 7. THREAD SAFETY
 *    - All SD card access protected by hal::SDCard mutex
 *    - Uses RAII Lock pattern for automatic mutex release
 *    - Safe for dual-core FreeRTOS operation (Core 0 + Core 1)
 *
 * 8. RUNTIME EDITING WORKFLOW
 *    Step 1: config.set("KEY", "VALUE")  // Update in memory
 *    Step 2: config.saveToSD()           // Write to SD card
 *    Step 3: ESP.restart()               // Reboot to apply changes
 *
 * 9. LOGGING STRATEGY
 *    - LOG_INFO: Always compiled, major events (load/save/validate results)
 *    - LOG_DEBUG: DEBUG_MODE only, verbose details (line-by-line parsing)
 *    - LOG_ERROR: Always compiled, critical failures
 *
 * 10. EXTRACTED FROM v4.1 LINES
 *     - Lines 1295-1320: generateDeviceId() implementation
 *     - Lines 1323-1440: parseConfigFile() -> loadFromSD()
 *     - Lines 1443-1542: validateConfig() -> validate()
 *     - Lines 3157-3234: SET_CONFIG serial command -> set()
 *     - Lines 3236-3288: SAVE_CONFIG serial command -> saveToSD()
 *
 * 11. EXAMPLE config.txt
 *     # NeurAI Memory Scanner Configuration
 *     WIFI_SSID=MyNetwork
 *     WIFI_PASSWORD=my_password
 *     ORCHESTRATOR_URL=http://192.168.1.100:3000
 *     TEAM_ID=001
 *     DEVICE_ID=SCANNER_FLOOR1_001
 *     SYNC_TOKENS=true
 *     DEBUG_MODE=false
 */
