#include <unity.h>
#include <Arduino.h>
#include "models/Config.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// ─── DeviceConfig::validate() ─────────────────────────────────────────

void test_validate_valid_config() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.wifiPassword = "password123";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";
    cfg.deviceID = "SCANNER_001";

    TEST_ASSERT_TRUE(cfg.validate());
}

void test_validate_empty_ssid_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_ssid_too_long_fails() {
    models::DeviceConfig cfg;
    // MAX_SSID_LENGTH = 32, use std::string to avoid hand-counting
    cfg.wifiSSID = std::string(33, 'A').c_str();
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_ssid_at_max_length_passes() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = std::string(32, 'A').c_str(); // Exactly MAX_SSID_LENGTH
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_TRUE(cfg.validate());
}

void test_validate_password_too_long_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    // MAX_PASSWORD_LENGTH = 63, use std::string to avoid hand-counting
    cfg.wifiPassword = std::string(64, 'A').c_str();
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_empty_password_passes() {
    // Password is optional (open networks exist)
    models::DeviceConfig cfg;
    cfg.wifiSSID = "OpenNetwork";
    cfg.wifiPassword = "";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_TRUE(cfg.validate());
}

void test_validate_empty_url_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_url_no_protocol_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_url_https_passes() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_TRUE(cfg.validate());
}

void test_validate_url_http_auto_upgrades() {
    // validate() should mutate orchestratorURL from http:// to https://
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "http://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_TRUE(cfg.validate());
    TEST_ASSERT_EQUAL_STRING("https://10.0.0.177:3000", cfg.orchestratorURL.c_str());
}

void test_validate_teamid_exactly_3_digits() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "042";

    TEST_ASSERT_TRUE(cfg.validate());
}

void test_validate_teamid_too_short_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "01";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_teamid_too_long_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "0001";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_teamid_non_digit_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "00a";

    TEST_ASSERT_FALSE(cfg.validate());
}

void test_validate_deviceid_optional() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";
    cfg.deviceID = ""; // Empty is OK (auto-generated from MAC)

    TEST_ASSERT_TRUE(cfg.validate());
}

void test_validate_deviceid_too_long_fails() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";
    // MAX_DEVICE_ID_LENGTH = 100, create 101-char ID
    std::string longId(101, 'X');
    cfg.deviceID = longId.c_str();

    TEST_ASSERT_FALSE(cfg.validate());
}

// ─── DeviceConfig::isComplete() ───────────────────────────────────────

void test_isComplete_with_all_required() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_TRUE(cfg.isComplete());
}

void test_isComplete_missing_ssid() {
    models::DeviceConfig cfg;
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.isComplete());
}

void test_isComplete_missing_url() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.teamID = "001";

    TEST_ASSERT_FALSE(cfg.isComplete());
}

void test_isComplete_missing_teamid() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";

    TEST_ASSERT_FALSE(cfg.isComplete());
}

void test_isComplete_wrong_teamid_length() {
    models::DeviceConfig cfg;
    cfg.wifiSSID = "TestNetwork";
    cfg.orchestratorURL = "https://10.0.0.177:3000";
    cfg.teamID = "01"; // 2 chars, needs 3

    TEST_ASSERT_FALSE(cfg.isComplete());
}

// ─── Default values ───────────────────────────────────────────────────

void test_defaults() {
    models::DeviceConfig cfg;
    TEST_ASSERT_TRUE(cfg.syncTokens);
    TEST_ASSERT_FALSE(cfg.debugMode);
    TEST_ASSERT_EQUAL(0, cfg.wifiSSID.length());
    TEST_ASSERT_EQUAL(0, cfg.deviceID.length());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // validate()
    RUN_TEST(test_validate_valid_config);
    RUN_TEST(test_validate_empty_ssid_fails);
    RUN_TEST(test_validate_ssid_too_long_fails);
    RUN_TEST(test_validate_ssid_at_max_length_passes);
    RUN_TEST(test_validate_password_too_long_fails);
    RUN_TEST(test_validate_empty_password_passes);
    RUN_TEST(test_validate_empty_url_fails);
    RUN_TEST(test_validate_url_no_protocol_fails);
    RUN_TEST(test_validate_url_https_passes);
    RUN_TEST(test_validate_url_http_auto_upgrades);
    RUN_TEST(test_validate_teamid_exactly_3_digits);
    RUN_TEST(test_validate_teamid_too_short_fails);
    RUN_TEST(test_validate_teamid_too_long_fails);
    RUN_TEST(test_validate_teamid_non_digit_fails);
    RUN_TEST(test_validate_deviceid_optional);
    RUN_TEST(test_validate_deviceid_too_long_fails);

    // isComplete()
    RUN_TEST(test_isComplete_with_all_required);
    RUN_TEST(test_isComplete_missing_ssid);
    RUN_TEST(test_isComplete_missing_url);
    RUN_TEST(test_isComplete_missing_teamid);
    RUN_TEST(test_isComplete_wrong_teamid_length);

    // Defaults
    RUN_TEST(test_defaults);

    return UNITY_END();
}
