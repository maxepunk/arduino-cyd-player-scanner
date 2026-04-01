#include <unity.h>
#include <Arduino.h>

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// Verify mock Arduino String works
void test_string_basic() {
    String s("Hello");
    TEST_ASSERT_EQUAL(5, s.length());
    TEST_ASSERT_EQUAL_STRING("Hello", s.c_str());
}

void test_string_replace() {
    String s("foo:bar:baz");
    s.replace(":", "");
    TEST_ASSERT_EQUAL_STRING("foobarbaz", s.c_str());
}

void test_string_toLowerCase() {
    String s("ABC123");
    s.toLowerCase();
    TEST_ASSERT_EQUAL_STRING("abc123", s.c_str());
}

void test_string_trim() {
    String s("  hello  ");
    s.trim();
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
}

void test_string_startsWith() {
    String s("https://example.com");
    TEST_ASSERT_TRUE(s.startsWith("https://"));
    TEST_ASSERT_FALSE(s.startsWith("http://"));
}

void test_isDigit() {
    TEST_ASSERT_TRUE(isDigit('0'));
    TEST_ASSERT_TRUE(isDigit('9'));
    TEST_ASSERT_FALSE(isDigit('a'));
    TEST_ASSERT_FALSE(isDigit(' '));
}

// Verify real config.h compiles with our mock
#include "config.h"

void test_config_constants() {
    TEST_ASSERT_EQUAL(3, limits::TEAM_ID_LENGTH);
    TEST_ASSERT_EQUAL(32, limits::MAX_SSID_LENGTH);
    TEST_ASSERT_EQUAL_STRING("/assets/images/", paths::IMAGES_DIR);
    TEST_ASSERT_EQUAL_STRING("/assets/audio/", paths::AUDIO_DIR);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_string_basic);
    RUN_TEST(test_string_replace);
    RUN_TEST(test_string_toLowerCase);
    RUN_TEST(test_string_trim);
    RUN_TEST(test_string_startsWith);
    RUN_TEST(test_isDigit);
    RUN_TEST(test_config_constants);
    return UNITY_END();
}
