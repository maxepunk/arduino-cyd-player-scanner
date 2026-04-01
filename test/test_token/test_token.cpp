#include <unity.h>
#include <Arduino.h>
#include "models/Token.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// ─── TokenMetadata::cleanTokenId() ────────────────────────────────────

void test_cleanTokenId_lowercase() {
    String result = models::TokenMetadata::cleanTokenId("KAA001");
    TEST_ASSERT_EQUAL_STRING("kaa001", result.c_str());
}

void test_cleanTokenId_removes_colons() {
    // UID hex format from RFID reader: "04:A1:B2:C3:D4:E5:F6"
    String result = models::TokenMetadata::cleanTokenId("04:A1:B2:C3");
    TEST_ASSERT_EQUAL_STRING("04a1b2c3", result.c_str());
}

void test_cleanTokenId_removes_spaces() {
    String result = models::TokenMetadata::cleanTokenId("kaa 001");
    TEST_ASSERT_EQUAL_STRING("kaa001", result.c_str());
}

void test_cleanTokenId_trims_whitespace() {
    String result = models::TokenMetadata::cleanTokenId("  kaa001  ");
    TEST_ASSERT_EQUAL_STRING("kaa001", result.c_str());
}

void test_cleanTokenId_combined() {
    // Real-world: UID with colons, spaces, mixed case, whitespace
    String result = models::TokenMetadata::cleanTokenId("  04:A1:B2 C3:D4  ");
    TEST_ASSERT_EQUAL_STRING("04a1b2c3d4", result.c_str());
}

void test_cleanTokenId_already_clean() {
    String result = models::TokenMetadata::cleanTokenId("kaa001");
    TEST_ASSERT_EQUAL_STRING("kaa001", result.c_str());
}

void test_cleanTokenId_empty_string() {
    String result = models::TokenMetadata::cleanTokenId("");
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ─── TokenMetadata::isVideoToken() ────────────────────────────────────

void test_isVideoToken_with_video() {
    models::TokenMetadata token;
    token.video = "kaa001.mp4";
    TEST_ASSERT_TRUE(token.isVideoToken());
}

void test_isVideoToken_empty_string() {
    models::TokenMetadata token;
    token.video = "";
    TEST_ASSERT_FALSE(token.isVideoToken());
}

void test_isVideoToken_null_string() {
    // tokens.json has "video": null which maps to "null" string in ArduinoJson
    models::TokenMetadata token;
    token.video = "null";
    TEST_ASSERT_FALSE(token.isVideoToken());
}

void test_isVideoToken_default() {
    models::TokenMetadata token;
    // Default-constructed String is empty
    TEST_ASSERT_FALSE(token.isVideoToken());
}

// ─── TokenMetadata::getImagePath() / getAudioPath() ──────────────────

void test_getImagePath() {
    models::TokenMetadata token;
    token.tokenId = "kaa001";
    TEST_ASSERT_EQUAL_STRING("/assets/images/kaa001.bmp", token.getImagePath().c_str());
}

void test_getAudioPath() {
    models::TokenMetadata token;
    token.tokenId = "kaa001";
    TEST_ASSERT_EQUAL_STRING("/assets/audio/kaa001.wav", token.getAudioPath().c_str());
}

void test_getImagePath_cleans_id() {
    // Path construction uses cleanTokenId internally
    models::TokenMetadata token;
    token.tokenId = "04:A1:B2:C3";
    TEST_ASSERT_EQUAL_STRING("/assets/images/04a1b2c3.bmp", token.getImagePath().c_str());
}

void test_getAudioPath_cleans_id() {
    models::TokenMetadata token;
    token.tokenId = "KAA001";
    TEST_ASSERT_EQUAL_STRING("/assets/audio/kaa001.wav", token.getAudioPath().c_str());
}

// ─── ScanData ─────────────────────────────────────────────────────────

void test_scandata_default_deviceType() {
    models::ScanData scan;
    TEST_ASSERT_EQUAL_STRING("esp32", scan.deviceType.c_str());
}

void test_scandata_constructor() {
    models::ScanData scan("kaa001", "001", "SCANNER_001", "2026-04-01T12:00:00Z");
    TEST_ASSERT_EQUAL_STRING("kaa001", scan.tokenId.c_str());
    TEST_ASSERT_EQUAL_STRING("001", scan.teamId.c_str());
    TEST_ASSERT_EQUAL_STRING("SCANNER_001", scan.deviceId.c_str());
    TEST_ASSERT_EQUAL_STRING("esp32", scan.deviceType.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-04-01T12:00:00Z", scan.timestamp.c_str());
}

void test_scandata_isValid_complete() {
    models::ScanData scan("kaa001", "", "SCANNER_001", "2026-04-01T12:00:00Z");
    TEST_ASSERT_TRUE(scan.isValid());
}

void test_scandata_isValid_no_teamid() {
    // teamId is optional
    models::ScanData scan;
    scan.tokenId = "kaa001";
    scan.deviceId = "SCANNER_001";
    scan.timestamp = "2026-04-01T12:00:00Z";
    TEST_ASSERT_TRUE(scan.isValid());
}

void test_scandata_isValid_missing_tokenId() {
    models::ScanData scan;
    scan.deviceId = "SCANNER_001";
    scan.timestamp = "2026-04-01T12:00:00Z";
    TEST_ASSERT_FALSE(scan.isValid());
}

void test_scandata_isValid_missing_deviceId() {
    models::ScanData scan;
    scan.tokenId = "kaa001";
    scan.timestamp = "2026-04-01T12:00:00Z";
    TEST_ASSERT_FALSE(scan.isValid());
}

void test_scandata_isValid_missing_timestamp() {
    models::ScanData scan;
    scan.tokenId = "kaa001";
    scan.deviceId = "SCANNER_001";
    TEST_ASSERT_FALSE(scan.isValid());
}

void test_scandata_isValid_missing_deviceType() {
    models::ScanData scan;
    scan.tokenId = "kaa001";
    scan.deviceId = "SCANNER_001";
    scan.timestamp = "2026-04-01T12:00:00Z";
    scan.deviceType = ""; // Override default
    TEST_ASSERT_FALSE(scan.isValid());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // cleanTokenId
    RUN_TEST(test_cleanTokenId_lowercase);
    RUN_TEST(test_cleanTokenId_removes_colons);
    RUN_TEST(test_cleanTokenId_removes_spaces);
    RUN_TEST(test_cleanTokenId_trims_whitespace);
    RUN_TEST(test_cleanTokenId_combined);
    RUN_TEST(test_cleanTokenId_already_clean);
    RUN_TEST(test_cleanTokenId_empty_string);

    // isVideoToken
    RUN_TEST(test_isVideoToken_with_video);
    RUN_TEST(test_isVideoToken_empty_string);
    RUN_TEST(test_isVideoToken_null_string);
    RUN_TEST(test_isVideoToken_default);

    // Path construction
    RUN_TEST(test_getImagePath);
    RUN_TEST(test_getAudioPath);
    RUN_TEST(test_getImagePath_cleans_id);
    RUN_TEST(test_getAudioPath_cleans_id);

    // ScanData
    RUN_TEST(test_scandata_default_deviceType);
    RUN_TEST(test_scandata_constructor);
    RUN_TEST(test_scandata_isValid_complete);
    RUN_TEST(test_scandata_isValid_no_teamid);
    RUN_TEST(test_scandata_isValid_missing_tokenId);
    RUN_TEST(test_scandata_isValid_missing_deviceId);
    RUN_TEST(test_scandata_isValid_missing_timestamp);
    RUN_TEST(test_scandata_isValid_missing_deviceType);

    return UNITY_END();
}
