#pragma once

#include <Arduino.h>
#include "../config.h"

namespace models {

// Device configuration structure (from v4.1 lines 131-138)
struct DeviceConfig {
    // WiFi credentials
    String wifiSSID;
    String wifiPassword;

    // Orchestrator connection
    String orchestratorURL;

    // Device identification
    String teamID;      // Exactly 3 digits (001-999)
    String deviceID;    // Auto-generated from MAC if not set

    // Feature flags
    bool syncTokens = true;     // Sync token database from orchestrator
    bool debugMode = false;     // Enable serial commands, defer RFID init

    // Default constructor
    DeviceConfig() = default;

    // Validation methods
    bool validate() {
        // Check required fields
        if (wifiSSID.length() == 0 || wifiSSID.length() > limits::MAX_SSID_LENGTH) {
            return false;
        }

        if (wifiPassword.length() > limits::MAX_PASSWORD_LENGTH) {
            return false;
        }

        if (orchestratorURL.length() == 0) {
            return false;
        }

        // Accept both http:// and https:// protocols
        if (!orchestratorURL.startsWith("http://") && !orchestratorURL.startsWith("https://")) {
            return false;
        }

        // Auto-upgrade http:// to https:// (backward compatibility)
        if (orchestratorURL.startsWith("http://")) {
            orchestratorURL.replace("http://", "https://");
            Serial.println("[CONFIG] Auto-upgraded URL: http:// -> https://");
        }

        if (teamID.length() != limits::TEAM_ID_LENGTH) {
            return false;
        }

        // Verify teamID is all digits
        for (int i = 0; i < limits::TEAM_ID_LENGTH; i++) {
            if (!isDigit(teamID[i])) {
                return false;
            }
        }

        // DeviceID can be empty (auto-generated)
        if (deviceID.length() > limits::MAX_DEVICE_ID_LENGTH) {
            return false;
        }

        return true;
    }

    // Check if configuration is complete
    bool isComplete() const {
        return (wifiSSID.length() > 0 &&
                orchestratorURL.length() > 0 &&
                teamID.length() == limits::TEAM_ID_LENGTH);
    }

    // Print configuration (for debugging)
    void print() const {
        Serial.println("\n=== Device Configuration ===");
        Serial.printf("WiFi SSID: %s\n", wifiSSID.length() > 0 ? wifiSSID.c_str() : "(not set)");
        Serial.printf("WiFi Password: %s\n", wifiPassword.length() > 0 ? "***" : "(not set)");
        Serial.printf("Orchestrator URL: %s\n", orchestratorURL.length() > 0 ? orchestratorURL.c_str() : "(not set)");
        Serial.printf("Team ID: %s\n", teamID.length() > 0 ? teamID.c_str() : "(not set)");
        Serial.printf("Device ID: %s\n", deviceID.length() > 0 ? deviceID.c_str() : "(auto-generate)");
        Serial.printf("Sync Tokens: %s\n", syncTokens ? "true" : "false");
        Serial.printf("Debug Mode: %s\n", debugMode ? "true" : "false");
        Serial.println("============================\n");
    }
};

} // namespace models
