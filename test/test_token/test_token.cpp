/**
 * Unit tests for models/Token.h
 *
 * TokenMetadata carries only a tokenId on this build. Everything else —
 * whether a ghost has an image, whether it has audio — is decided by what
 * is actually on the SD card at scan time, not by the token database. The
 * path-construction rules tested here are therefore the whole contract
 * between a tokenId and its files, and the only thing standing between a
 * card-maker's filename and a silent ghost.
 */

#include <unity.h>
#include <Arduino.h>
#include "../../ALNScanner_v5/models/Token.h"

using namespace models;

// ─── cleanTokenId ──────────────────────────────────────────────────────

void test_cleanTokenId_lowercases() {
    TEST_ASSERT_EQUAL_STRING("ghost01", TokenMetadata::cleanTokenId("GHOST01").c_str());
}

void test_cleanTokenId_removes_colons() {
    TEST_ASSERT_EQUAL_STRING("04a1b2", TokenMetadata::cleanTokenId("04:A1:B2").c_str());
}

void test_cleanTokenId_removes_spaces() {
    TEST_ASSERT_EQUAL_STRING("ghost01", TokenMetadata::cleanTokenId("ghost 01").c_str());
}

void test_cleanTokenId_trims_whitespace() {
    TEST_ASSERT_EQUAL_STRING("ghost01", TokenMetadata::cleanTokenId("  ghost01  ").c_str());
}

void test_cleanTokenId_idempotent() {
    // A tag written already-clean must not be altered — the card-maker's
    // filenames are matched against this exact output.
    String once = TokenMetadata::cleanTokenId("ghost01");
    TEST_ASSERT_EQUAL_STRING("ghost01", TokenMetadata::cleanTokenId(once).c_str());
}

void test_cleanTokenId_empty_stays_empty() {
    TEST_ASSERT_EQUAL_STRING("", TokenMetadata::cleanTokenId("").c_str());
}

// ─── path construction ─────────────────────────────────────────────────

void test_getImagePath_constructed_from_tokenId() {
    TokenMetadata t;
    t.tokenId = "ghost01";
    TEST_ASSERT_EQUAL_STRING("/assets/images/ghost01.bmp", t.getImagePath().c_str());
}

void test_getAudioPath_constructed_from_tokenId() {
    TokenMetadata t;
    t.tokenId = "ghost01";
    TEST_ASSERT_EQUAL_STRING("/assets/audio/ghost01.wav", t.getAudioPath().c_str());
}

void test_paths_use_cleaned_tokenId() {
    // A tag written with stray case or punctuation must still resolve to the
    // lowercase filenames the card actually carries.
    TokenMetadata t;
    t.tokenId = "GHOST 01";
    TEST_ASSERT_EQUAL_STRING("/assets/images/ghost01.bmp", t.getImagePath().c_str());
    TEST_ASSERT_EQUAL_STRING("/assets/audio/ghost01.wav", t.getAudioPath().c_str());
}

void test_image_and_audio_paths_differ_only_by_dir_and_ext() {
    // Asserted in full rather than by suffix: these exact strings are what
    // the card-maker's filenames have to match, so the whole path is the
    // contract, not just the extension.
    TokenMetadata t;
    t.tokenId = "ghost42";
    TEST_ASSERT_EQUAL_STRING("/assets/images/ghost42.bmp", t.getImagePath().c_str());
    TEST_ASSERT_EQUAL_STRING("/assets/audio/ghost42.wav", t.getAudioPath().c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_cleanTokenId_lowercases);
    RUN_TEST(test_cleanTokenId_removes_colons);
    RUN_TEST(test_cleanTokenId_removes_spaces);
    RUN_TEST(test_cleanTokenId_trims_whitespace);
    RUN_TEST(test_cleanTokenId_idempotent);
    RUN_TEST(test_cleanTokenId_empty_stays_empty);

    RUN_TEST(test_getImagePath_constructed_from_tokenId);
    RUN_TEST(test_getAudioPath_constructed_from_tokenId);
    RUN_TEST(test_paths_use_cleaned_tokenId);
    RUN_TEST(test_image_and_audio_paths_differ_only_by_dir_and_ext);

    return UNITY_END();
}
