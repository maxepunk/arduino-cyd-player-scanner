/**
 * Unit tests for models/Config.h
 *
 * The orchestrator build's config had hard requirements (SSID, URL, 3-digit
 * team) that could refuse to boot. This prop is assembled by someone who is
 * not the developer, from a hand-edited text file, in a venue the developer
 * is not standing in — so validation here is deliberately permissive, and
 * these tests exist to keep it that way.
 *
 * The volume clamp is the one piece of real logic: SetGain() accepts up to
 * 4.0 but clips hard on the internal DAC, and a stray digit in config.txt
 * must not produce either a refusal to boot or a blast of distortion.
 */

#include <unity.h>
#include <Arduino.h>
#include "../../ALNScanner_v5/models/Config.h"

using namespace models;

// ─── defaults ──────────────────────────────────────────────────────────

void test_defaults_are_usable() {
    DeviceConfig c;
    TEST_ASSERT_FALSE(c.debugMode);
    TEST_ASSERT_EQUAL_STRING("", c.deviceID.c_str());
    TEST_ASSERT_EQUAL_FLOAT(limits::DEFAULT_VOLUME, c.volume);
}

void test_default_config_validates() {
    // A missing or empty config.txt must still yield a working prop.
    DeviceConfig c;
    TEST_ASSERT_TRUE(c.validate());
}

void test_isComplete_always_true() {
    DeviceConfig c;
    TEST_ASSERT_TRUE(c.isComplete());
}

// ─── deviceID ──────────────────────────────────────────────────────────

void test_empty_deviceID_is_valid() {
    // Blank means "derive from MAC", not "invalid".
    DeviceConfig c;
    c.deviceID = "";
    TEST_ASSERT_TRUE(c.validate());
}

void test_normal_deviceID_is_valid() {
    DeviceConfig c;
    c.deviceID = "SCANNER_004";
    TEST_ASSERT_TRUE(c.validate());
}

void test_overlong_deviceID_rejected() {
    DeviceConfig c;
    for (int i = 0; i <= limits::MAX_DEVICE_ID_LENGTH; i++) {
        c.deviceID += "x";
    }
    TEST_ASSERT_FALSE(c.validate());
}

// ─── volume clamping ───────────────────────────────────────────────────

void test_volume_in_range_untouched() {
    DeviceConfig c;
    c.volume = 1.5f;
    TEST_ASSERT_TRUE(c.validate());
    TEST_ASSERT_EQUAL_FLOAT(1.5f, c.volume);
}

void test_volume_above_max_is_clamped_not_rejected() {
    DeviceConfig c;
    c.volume = 9.0f;
    TEST_ASSERT_TRUE(c.validate());               // still boots
    TEST_ASSERT_EQUAL_FLOAT(limits::MAX_VOLUME, c.volume);
}

void test_negative_volume_is_clamped_not_rejected() {
    DeviceConfig c;
    c.volume = -3.0f;
    TEST_ASSERT_TRUE(c.validate());
    TEST_ASSERT_EQUAL_FLOAT(limits::MIN_VOLUME, c.volume);
}

void test_volume_at_bounds_untouched() {
    DeviceConfig lo;
    lo.volume = limits::MIN_VOLUME;
    TEST_ASSERT_TRUE(lo.validate());
    TEST_ASSERT_EQUAL_FLOAT(limits::MIN_VOLUME, lo.volume);

    DeviceConfig hi;
    hi.volume = limits::MAX_VOLUME;
    TEST_ASSERT_TRUE(hi.validate());
    TEST_ASSERT_EQUAL_FLOAT(limits::MAX_VOLUME, hi.volume);
}

void test_zero_volume_is_valid() {
    // Silencing a prop is a legitimate operational choice, not an error.
    DeviceConfig c;
    c.volume = 0.0f;
    TEST_ASSERT_TRUE(c.validate());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, c.volume);
}

// ─── no networking requirements survive ────────────────────────────────

void test_validate_is_idempotent() {
    // validate() mutates (clamps), so running it twice must be stable —
    // it runs on load and again after SET_CONFIG.
    DeviceConfig c;
    c.volume = 5.0f;
    TEST_ASSERT_TRUE(c.validate());
    const float once = c.volume;
    TEST_ASSERT_TRUE(c.validate());
    TEST_ASSERT_EQUAL_FLOAT(once, c.volume);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_defaults_are_usable);
    RUN_TEST(test_default_config_validates);
    RUN_TEST(test_isComplete_always_true);

    RUN_TEST(test_empty_deviceID_is_valid);
    RUN_TEST(test_normal_deviceID_is_valid);
    RUN_TEST(test_overlong_deviceID_rejected);

    RUN_TEST(test_volume_in_range_untouched);
    RUN_TEST(test_volume_above_max_is_clamped_not_rejected);
    RUN_TEST(test_negative_volume_is_clamped_not_rejected);
    RUN_TEST(test_volume_at_bounds_untouched);
    RUN_TEST(test_zero_volume_is_valid);

    RUN_TEST(test_validate_is_idempotent);

    return UNITY_END();
}
