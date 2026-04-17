#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "services/AssetManifestDiff.h"

using services::manifest::Pending;
using services::manifest::diff;
using services::manifest::buildPath;
using services::manifest::collectOrphans;

void setUp(void) {}
void tearDown(void) {}

// ─── diff(): identifies changed, added, and missing files ─────────────

void test_diff_detects_added_image() {
    DynamicJsonDocument remote(2048);
    DynamicJsonDocument local(2048);
    remote["images"]["kaa001"]["sha1"] = "1111111111111111111111111111111111111111";
    remote["images"]["kaa001"]["size"] = 230454;
    remote["audio"].to<JsonObject>();
    local["images"].to<JsonObject>();
    local["audio"].to<JsonObject>();

    std::vector<Pending> pending = diff(remote, local);
    TEST_ASSERT_EQUAL(1, (int)pending.size());
    TEST_ASSERT_EQUAL_STRING("image", pending[0].type.c_str());
    TEST_ASSERT_EQUAL_STRING("kaa001", pending[0].tokenId.c_str());
    TEST_ASSERT_EQUAL(230454, (int)pending[0].size);
}

void test_diff_skips_unchanged_entries() {
    DynamicJsonDocument remote(2048), local(2048);
    remote["images"]["kaa001"]["sha1"] = "abcabcabcabcabcabcabcabcabcabcabcabcabca";
    remote["images"]["kaa001"]["size"] = 1000;
    remote["audio"].to<JsonObject>();
    local["images"]["kaa001"]["sha1"] = "abcabcabcabcabcabcabcabcabcabcabcabcabca";
    local["images"]["kaa001"]["size"] = 1000;
    local["audio"].to<JsonObject>();

    std::vector<Pending> pending = diff(remote, local);
    TEST_ASSERT_EQUAL(0, (int)pending.size());
}

void test_diff_flags_sha_mismatch() {
    DynamicJsonDocument remote(2048), local(2048);
    remote["images"]["kaa001"]["sha1"] = "1111111111111111111111111111111111111111";
    remote["images"]["kaa001"]["size"] = 1000;
    remote["audio"].to<JsonObject>();
    local["images"]["kaa001"]["sha1"] = "2222222222222222222222222222222222222222";
    local["images"]["kaa001"]["size"] = 1000;
    local["audio"].to<JsonObject>();

    std::vector<Pending> pending = diff(remote, local);
    TEST_ASSERT_EQUAL(1, (int)pending.size());
    TEST_ASSERT_EQUAL_STRING("1111111111111111111111111111111111111111",
                             pending[0].sha1.c_str());
}

void test_diff_preserves_images_before_audio_order() {
    DynamicJsonDocument remote(2048), local(2048);
    remote["images"]["kaa001"]["sha1"] = "a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1";
    remote["images"]["kaa001"]["size"] = 10;
    remote["audio"]["asm031"]["sha1"] = "b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2";
    remote["audio"]["asm031"]["size"] = 20;
    remote["audio"]["asm031"]["ext"] = "wav";
    local.to<JsonObject>(); // empty local

    std::vector<Pending> pending = diff(remote, local);
    TEST_ASSERT_EQUAL(2, (int)pending.size());
    TEST_ASSERT_EQUAL_STRING("image", pending[0].type.c_str());
    TEST_ASSERT_EQUAL_STRING("audio", pending[1].type.c_str());
    TEST_ASSERT_EQUAL_STRING("wav", pending[1].ext.c_str());
}

void test_diff_skips_entries_missing_required_fields() {
    DynamicJsonDocument remote(2048), local(2048);
    // No sha1
    remote["images"]["bad1"]["size"] = 100;
    // No size
    remote["images"]["bad2"]["sha1"] = "c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3";
    // Valid
    remote["images"]["good"]["sha1"] = "d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4d4";
    remote["images"]["good"]["size"] = 1;
    remote["audio"].to<JsonObject>();
    local.to<JsonObject>();

    std::vector<Pending> pending = diff(remote, local);
    TEST_ASSERT_EQUAL(1, (int)pending.size());
    TEST_ASSERT_EQUAL_STRING("good", pending[0].tokenId.c_str());
}

// ─── collectOrphans(): tokenIds present locally but not remotely ──────

void test_collectOrphans_finds_deleted_tokens() {
    DynamicJsonDocument remote(2048), local(2048);
    local["images"]["keep"]["sha1"] = "e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5";
    local["images"]["gone"]["sha1"] = "f6f6f6f6f6f6f6f6f6f6f6f6f6f6f6f6f6f6f6f6";
    remote["images"]["keep"]["sha1"] = "e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5e5";

    std::vector<String> orphans;
    collectOrphans(local["images"].as<JsonObject>(),
                   remote["images"].as<JsonObjectConst>(),
                   orphans);
    TEST_ASSERT_EQUAL(1, (int)orphans.size());
    TEST_ASSERT_EQUAL_STRING("gone", orphans[0].c_str());
}

void test_collectOrphans_treats_missing_remote_section_as_empty() {
    DynamicJsonDocument remote(2048), local(2048);
    local["audio"]["orphan1"]["sha1"] = "a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1";
    local["audio"]["orphan2"]["sha1"] = "b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2";
    // remote has no "audio" section at all

    std::vector<String> orphans;
    collectOrphans(local["audio"].as<JsonObject>(),
                   remote["audio"].as<JsonObjectConst>(),
                   orphans);
    TEST_ASSERT_EQUAL(2, (int)orphans.size());
}

// ─── buildPath(): SD paths match the canonical layout ────────────────

void test_buildPath_image_uses_bmp_extension() {
    String p = buildPath("image", "kaa001", "");
    TEST_ASSERT_EQUAL_STRING("/assets/images/kaa001.bmp", p.c_str());
}

void test_buildPath_audio_uses_provided_ext() {
    String p = buildPath("audio", "asm031", "wav");
    TEST_ASSERT_EQUAL_STRING("/assets/audio/asm031.wav", p.c_str());
    String p2 = buildPath("audio", "rat031", "mp3");
    TEST_ASSERT_EQUAL_STRING("/assets/audio/rat031.mp3", p2.c_str());
}

void test_buildPath_audio_defaults_to_wav_when_ext_missing() {
    String p = buildPath("audio", "asm031", "");
    TEST_ASSERT_EQUAL_STRING("/assets/audio/asm031.wav", p.c_str());
}

// ─── Unity runner ────────────────────────────────────────────────────

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_diff_detects_added_image);
    RUN_TEST(test_diff_skips_unchanged_entries);
    RUN_TEST(test_diff_flags_sha_mismatch);
    RUN_TEST(test_diff_preserves_images_before_audio_order);
    RUN_TEST(test_diff_skips_entries_missing_required_fields);
    RUN_TEST(test_collectOrphans_finds_deleted_tokens);
    RUN_TEST(test_collectOrphans_treats_missing_remote_section_as_empty);
    RUN_TEST(test_buildPath_image_uses_bmp_extension);
    RUN_TEST(test_buildPath_audio_uses_provided_ext);
    RUN_TEST(test_buildPath_audio_defaults_to_wav_when_ext_missing);
    return UNITY_END();
}
