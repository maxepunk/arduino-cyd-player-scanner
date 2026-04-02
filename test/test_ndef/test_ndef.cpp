#include <unity.h>
#include <Arduino.h>
#include "hal/NDEFParser.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// ─── Helper: Build valid NDEF text record ─────────────────────────────
//
// Constructs pages3to6 and pages7to10 byte arrays containing a valid
// NTAG215-style NDEF text record with the given text payload.
// Layout: CC(4) | TLV(03,len) | Record(D1,01,payloadLen,'T',langLen,'e','n') | text | FE

static void buildValidNDEFPages(uint8_t* pages3to6, uint8_t* pages7to10, const char* text) {
    memset(pages3to6, 0, 16);
    memset(pages7to10, 0, 16);

    int textLen = strlen(text);
    int langLen = 2;  // "en"
    int payloadLen = 1 + langLen + textLen;  // status byte + lang + text
    int ndefRecordLen = 1 + 1 + 1 + 1 + payloadLen;  // header + typeLen + payloadLen + 'T' + payload
    int tlvLen = ndefRecordLen;

    // Capability Container (page 3)
    pages3to6[0] = 0x01;  // magic
    pages3to6[1] = 0x03;  // version
    pages3to6[2] = 0x00;  // size
    pages3to6[3] = 0x0F;  // access

    // NDEF Message TLV (starts at byte 4)
    pages3to6[4] = 0x03;              // TLV type = NDEF Message
    pages3to6[5] = (uint8_t)tlvLen;   // TLV length

    // NDEF Record
    int idx = 6;
    pages3to6[idx++] = 0xD1;              // MB=1, ME=1, CF=0, SR=1, IL=0, TNF=001
    pages3to6[idx++] = 0x01;              // Type length = 1
    pages3to6[idx++] = (uint8_t)payloadLen;  // Payload length
    pages3to6[idx++] = 'T';               // Type = Text
    pages3to6[idx++] = (uint8_t)langLen;  // Status byte (UTF-8, lang code len)
    pages3to6[idx++] = 'e';               // Language code
    pages3to6[idx++] = 'n';

    // Text payload — may span into pages7to10
    for (int i = 0; i < textLen; i++) {
        if (idx < 16) {
            pages3to6[idx++] = (uint8_t)text[i];
        } else {
            pages7to10[idx - 16] = (uint8_t)text[i];
            idx++;
        }
    }

    // Terminator TLV
    if (idx < 16) {
        pages3to6[idx] = 0xFE;
    } else if (idx - 16 < 16) {
        pages7to10[idx - 16] = 0xFE;
    }
}

// ─── Valid NDEF Parsing ───────────────────────────────────────────────

void test_parse_valid_short_text() {
    uint8_t p1[16], p2[16];
    buildValidNDEFPages(p1, p2, "kaa001");
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("kaa001", result.c_str());
}

void test_parse_valid_longer_text() {
    uint8_t p1[16], p2[16];
    buildValidNDEFPages(p1, p2, "ale001");
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("ale001", result.c_str());
}

void test_parse_text_spanning_pages() {
    // "longtoken" = 9 chars, with header overhead it spans into pages7to10
    uint8_t p1[16], p2[16];
    buildValidNDEFPages(p1, p2, "longtoken");
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("longtoken", result.c_str());
}

// ─── SAK Rejection ────────────────────────────────────────────────────

void test_reject_non_ntag_sak() {
    uint8_t p1[16] = {0}, p2[16] = {0};
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x08);  // MIFARE Classic
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_reject_sak_0x20() {
    uint8_t p1[16] = {0}, p2[16] = {0};
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x20);  // MIFARE DESFire
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ─── Buffer Validation ────────────────────────────────────────────────

void test_reject_short_buffer() {
    uint8_t p1[8] = {0}, p2[16] = {0};
    String result = hal::parseNDEFText(p1, 8, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ─── TLV Parsing Edge Cases ──────────────────────────────────────────

void test_no_ndef_tlv_returns_empty() {
    // All zeros — no TLV type 0x03 found
    uint8_t p1[16] = {0}, p2[16] = {0};
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_terminator_before_ndef() {
    uint8_t p1[16] = {0}, p2[16] = {0};
    p1[4] = 0xFE;  // Terminator TLV before any NDEF
    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_lock_control_tlv_skipped() {
    // Lock Control TLV (0x01) with length 0, followed by NDEF Message TLV
    uint8_t p1[16], p2[16];
    memset(p1, 0, 16);
    memset(p2, 0, 16);

    p1[4] = 0x01;   // Lock Control TLV
    p1[5] = 0x00;   // Length = 0
    // After skipping: i += 1 + 0 = 1, then i++ makes i=6
    p1[6] = 0x03;   // NDEF Message TLV
    p1[7] = 0x08;   // Length = 8 (header + typeLen + payloadLen + 'T' + payload[4])
    // NDEF record starts at p1[8]
    p1[8]  = 0xD1;  // Record header (MB|ME|SR|TNF=001)
    p1[9]  = 0x01;  // Type length = 1
    p1[10] = 0x04;  // Payload length = 4 (1 status + 1 lang + 2 text)
    p1[11] = 'T';   // Type
    p1[12] = 0x01;  // Status byte: lang code len = 1
    p1[13] = 'e';   // Language code
    p1[14] = 'a';   // Text byte 1
    p1[15] = 'b';   // Text byte 2 (spans to last byte of first page buffer)

    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("ab", result.c_str());
}

// ─── NDEF Record Parsing Edge Cases ──────────────────────────────────

void test_wrong_tnf_returns_empty() {
    // TNF=100 (External type) instead of TNF=001 (Well-known)
    uint8_t p1[16], p2[16];
    memset(p1, 0, 16);
    memset(p2, 0, 16);

    p1[4] = 0x03;   // NDEF Message TLV
    p1[5] = 0x07;   // Length
    p1[6] = 0xD4;   // Record header: TNF=100 (External)
    p1[7] = 0x01;
    p1[8] = 0x04;
    p1[9] = 'T';
    p1[10] = 0x01;
    p1[11] = 'e';
    p1[12] = 'a';
    p1[13] = 'b';

    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_wrong_type_not_T_returns_empty() {
    // Type = 'U' (URI) instead of 'T' (Text)
    uint8_t p1[16], p2[16];
    memset(p1, 0, 16);
    memset(p2, 0, 16);

    p1[4] = 0x03;   // NDEF Message TLV
    p1[5] = 0x07;   // Length
    p1[6] = 0xD1;   // Record header: TNF=001
    p1[7] = 0x01;   // Type length = 1
    p1[8] = 0x04;   // Payload length
    p1[9] = 'U';    // Type = URI (not Text)
    p1[10] = 0x01;
    p1[11] = 'e';
    p1[12] = 'a';
    p1[13] = 'b';

    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_ndef_message_too_short() {
    // NDEF message length < 7 (minimum for a text record)
    uint8_t p1[16], p2[16];
    memset(p1, 0, 16);
    memset(p2, 0, 16);

    p1[4] = 0x03;   // NDEF Message TLV
    p1[5] = 0x03;   // Length = 3 (too short for text record)

    String result = hal::parseNDEFText(p1, 16, p2, 16, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ─── Main ─────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_valid_short_text);
    RUN_TEST(test_parse_valid_longer_text);
    RUN_TEST(test_parse_text_spanning_pages);
    RUN_TEST(test_reject_non_ntag_sak);
    RUN_TEST(test_reject_sak_0x20);
    RUN_TEST(test_reject_short_buffer);
    RUN_TEST(test_no_ndef_tlv_returns_empty);
    RUN_TEST(test_terminator_before_ndef);
    RUN_TEST(test_lock_control_tlv_skipped);
    RUN_TEST(test_wrong_tnf_returns_empty);
    RUN_TEST(test_wrong_type_not_T_returns_empty);
    RUN_TEST(test_ndef_message_too_short);

    return UNITY_END();
}
