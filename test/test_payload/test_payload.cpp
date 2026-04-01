#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "services/PayloadBuilder.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// ─── buildScanJson ────────────────────────────────────────────────────

void test_buildScanJson_contains_required_fields() {
    models::ScanData scan("kaa001", "001", "SCANNER_001", "2026-04-01T12:00:00Z");

    String json = services::buildScanJson(scan);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    TEST_ASSERT_EQUAL_STRING("kaa001", doc["tokenId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("001", doc["teamId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("SCANNER_001", doc["deviceId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("esp32", doc["deviceType"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("2026-04-01T12:00:00Z", doc["timestamp"].as<const char*>());
}

void test_buildScanJson_omits_empty_teamId() {
    models::ScanData scan;
    scan.tokenId = "kaa001";
    scan.teamId = ""; // Empty — should be omitted
    scan.deviceId = "SCANNER_001";
    scan.timestamp = "2026-04-01T12:00:00Z";

    String json = services::buildScanJson(scan);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    TEST_ASSERT_FALSE(doc["teamId"].is<const char*>());
}

void test_buildScanJson_includes_nonempty_teamId() {
    models::ScanData scan("kaa001", "TeamAlpha", "SCANNER_001", "2026-04-01T12:00:00Z");

    String json = services::buildScanJson(scan);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    TEST_ASSERT_TRUE(doc["teamId"].is<const char*>());
    TEST_ASSERT_EQUAL_STRING("TeamAlpha", doc["teamId"].as<const char*>());
}

void test_buildScanJson_deviceType_always_esp32() {
    models::ScanData scan;
    scan.tokenId = "test";
    scan.deviceId = "dev";
    scan.timestamp = "ts";
    // deviceType defaults to "esp32" in ScanData constructor

    String json = services::buildScanJson(scan);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    TEST_ASSERT_EQUAL_STRING("esp32", doc["deviceType"].as<const char*>());
}

// ─── parseScanFromJsonl (round-trip) ──────────────────────────────────

void test_roundtrip_serialize_then_parse() {
    models::ScanData original("kaa001", "001", "SCANNER_001", "2026-04-01T12:00:00Z");

    String json = services::buildScanJson(original);

    models::ScanData parsed;
    bool ok = services::parseScanFromJsonl(json, parsed);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(original.tokenId.c_str(), parsed.tokenId.c_str());
    TEST_ASSERT_EQUAL_STRING(original.teamId.c_str(), parsed.teamId.c_str());
    TEST_ASSERT_EQUAL_STRING(original.deviceId.c_str(), parsed.deviceId.c_str());
    TEST_ASSERT_EQUAL_STRING(original.deviceType.c_str(), parsed.deviceType.c_str());
    TEST_ASSERT_EQUAL_STRING(original.timestamp.c_str(), parsed.timestamp.c_str());
}

void test_roundtrip_empty_teamId() {
    models::ScanData original;
    original.tokenId = "kaa001";
    original.teamId = "";
    original.deviceId = "SCANNER_001";
    original.timestamp = "2026-04-01T12:00:00Z";

    String json = services::buildScanJson(original);

    models::ScanData parsed;
    bool ok = services::parseScanFromJsonl(json, parsed);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("", parsed.teamId.c_str());
}

void test_parse_rejects_corrupt_json() {
    models::ScanData scan;
    TEST_ASSERT_FALSE(services::parseScanFromJsonl("not json at all", scan));
}

void test_parse_rejects_missing_tokenId() {
    models::ScanData scan;
    TEST_ASSERT_FALSE(services::parseScanFromJsonl(
        "{\"deviceId\":\"dev\",\"timestamp\":\"ts\"}", scan));
}

void test_parse_rejects_missing_deviceId() {
    models::ScanData scan;
    TEST_ASSERT_FALSE(services::parseScanFromJsonl(
        "{\"tokenId\":\"tok\",\"timestamp\":\"ts\"}", scan));
}

void test_parse_rejects_missing_timestamp() {
    models::ScanData scan;
    TEST_ASSERT_FALSE(services::parseScanFromJsonl(
        "{\"tokenId\":\"tok\",\"deviceId\":\"dev\"}", scan));
}

void test_parse_defaults_deviceType_for_legacy_entries() {
    // Pre-P2.3 queue entries don't have deviceType
    models::ScanData scan;
    bool ok = services::parseScanFromJsonl(
        "{\"tokenId\":\"tok\",\"deviceId\":\"dev\",\"timestamp\":\"ts\"}", scan);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("esp32", scan.deviceType.c_str());
}

void test_parse_reads_explicit_deviceType() {
    models::ScanData scan;
    bool ok = services::parseScanFromJsonl(
        "{\"tokenId\":\"tok\",\"deviceId\":\"dev\",\"deviceType\":\"custom\",\"timestamp\":\"ts\"}", scan);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("custom", scan.deviceType.c_str());
}

void test_parse_rejects_empty_string() {
    models::ScanData scan;
    TEST_ASSERT_FALSE(services::parseScanFromJsonl("", scan));
}

// ─── buildBatchJson ───────────────────────────────────────────────────

void test_buildBatchJson_structure() {
    std::vector<models::ScanData> batch;
    batch.push_back(models::ScanData("tok1", "001", "dev1", "ts1"));
    batch.push_back(models::ScanData("tok2", "", "dev2", "ts2"));

    String json = services::buildBatchJson("SCANNER_001_0", batch);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    TEST_ASSERT_EQUAL_STRING("SCANNER_001_0", doc["batchId"].as<const char*>());
    TEST_ASSERT_TRUE(doc["transactions"].is<JsonArray>());
    TEST_ASSERT_EQUAL(2, doc["transactions"].as<JsonArray>().size());
}

void test_buildBatchJson_transaction_fields() {
    std::vector<models::ScanData> batch;
    batch.push_back(models::ScanData("kaa001", "TeamA", "SCANNER_001", "2026-04-01T12:00:00Z"));

    String json = services::buildBatchJson("batch_1", batch);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    JsonObject t = doc["transactions"][0];
    TEST_ASSERT_EQUAL_STRING("kaa001", t["tokenId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("TeamA", t["teamId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("SCANNER_001", t["deviceId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("esp32", t["deviceType"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("2026-04-01T12:00:00Z", t["timestamp"].as<const char*>());
}

void test_buildBatchJson_omits_empty_teamId_in_transactions() {
    std::vector<models::ScanData> batch;
    models::ScanData scan;
    scan.tokenId = "tok";
    scan.teamId = "";
    scan.deviceId = "dev";
    scan.timestamp = "ts";
    batch.push_back(scan);

    String json = services::buildBatchJson("batch_1", batch);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    JsonObject t = doc["transactions"][0];
    TEST_ASSERT_FALSE(t["teamId"].is<const char*>());
}

void test_buildBatchJson_empty_batch() {
    std::vector<models::ScanData> batch;

    String json = services::buildBatchJson("batch_1", batch);
    JsonDocument doc;
    deserializeJson(doc, json.c_str());

    TEST_ASSERT_EQUAL_STRING("batch_1", doc["batchId"].as<const char*>());
    TEST_ASSERT_EQUAL(0, doc["transactions"].as<JsonArray>().size());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // buildScanJson
    RUN_TEST(test_buildScanJson_contains_required_fields);
    RUN_TEST(test_buildScanJson_omits_empty_teamId);
    RUN_TEST(test_buildScanJson_includes_nonempty_teamId);
    RUN_TEST(test_buildScanJson_deviceType_always_esp32);

    // Round-trip (serialize -> parse)
    RUN_TEST(test_roundtrip_serialize_then_parse);
    RUN_TEST(test_roundtrip_empty_teamId);

    // parseScanFromJsonl
    RUN_TEST(test_parse_rejects_corrupt_json);
    RUN_TEST(test_parse_rejects_missing_tokenId);
    RUN_TEST(test_parse_rejects_missing_deviceId);
    RUN_TEST(test_parse_rejects_missing_timestamp);
    RUN_TEST(test_parse_defaults_deviceType_for_legacy_entries);
    RUN_TEST(test_parse_reads_explicit_deviceType);
    RUN_TEST(test_parse_rejects_empty_string);

    // buildBatchJson
    RUN_TEST(test_buildBatchJson_structure);
    RUN_TEST(test_buildBatchJson_transaction_fields);
    RUN_TEST(test_buildBatchJson_omits_empty_teamId_in_transactions);
    RUN_TEST(test_buildBatchJson_empty_batch);

    return UNITY_END();
}
