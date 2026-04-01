#pragma once

/**
 * @file PayloadBuilder.h
 * @brief Pure JSON construction functions for orchestrator communication.
 *
 * Extracted from OrchestratorService.h to eliminate duplication (sendScan and
 * queueScan had identical JSON construction) and enable native unit testing.
 *
 * These functions have NO I/O dependencies — only ArduinoJson + model types.
 * Tested in test/test_payload/ with real ArduinoJson on native platform.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "../models/Token.h"

namespace services {

/**
 * Build JSON string for a single scan (POST /api/scan body and JSONL queue entry).
 * teamId is omitted when empty (matches backend contract: optional field, not null).
 */
inline String buildScanJson(const models::ScanData& scan) {
    JsonDocument doc;
    doc["tokenId"] = scan.tokenId.c_str();
    if (scan.teamId.length() > 0) doc["teamId"] = scan.teamId.c_str();
    doc["deviceId"] = scan.deviceId.c_str();
    doc["deviceType"] = scan.deviceType.c_str();
    doc["timestamp"] = scan.timestamp.c_str();

    std::string buf;
    serializeJson(doc, buf);
    return String(buf.c_str());
}

/**
 * Parse a single JSONL queue line back into ScanData.
 * Returns true if parsing succeeded and all required fields are present.
 * Missing deviceType defaults to "esp32" (backward compat for pre-P2.3 queue entries).
 */
inline bool parseScanFromJsonl(const String& line, models::ScanData& scan) {
    if (line.length() == 0) return false;

    JsonDocument doc;
    // Pass c_str() so ArduinoJson reads from a const char* (no stream interface needed)
    DeserializationError error = deserializeJson(doc, line.c_str());
    if (error != DeserializationError::Ok) return false;

    // Required fields
    if (!doc["tokenId"].is<const char*>() || !doc["deviceId"].is<const char*>() ||
        !doc["timestamp"].is<const char*>()) {
        return false;
    }

    scan.tokenId = doc["tokenId"].as<const char*>();
    scan.teamId = doc["teamId"].is<const char*>() ? doc["teamId"].as<const char*>() : "";
    scan.deviceId = doc["deviceId"].as<const char*>();
    scan.deviceType = doc["deviceType"].is<const char*>() ? doc["deviceType"].as<const char*>() : "esp32";
    scan.timestamp = doc["timestamp"].as<const char*>();
    return true;
}

/**
 * Build JSON string for batch upload (POST /api/scan/batch body).
 * Each transaction in the array follows the same schema as single scan.
 */
inline String buildBatchJson(const String& batchId,
                             const std::vector<models::ScanData>& batch) {
    JsonDocument doc;
    doc["batchId"] = batchId.c_str();
    JsonArray transactions = doc["transactions"].to<JsonArray>();

    for (const auto& entry : batch) {
        JsonObject t = transactions.add<JsonObject>();
        t["tokenId"] = entry.tokenId.c_str();
        if (entry.teamId.length() > 0) t["teamId"] = entry.teamId.c_str();
        t["deviceId"] = entry.deviceId.c_str();
        t["deviceType"] = entry.deviceType.c_str();
        t["timestamp"] = entry.timestamp.c_str();
    }

    std::string buf;
    serializeJson(doc, buf);
    return String(buf.c_str());
}

} // namespace services
