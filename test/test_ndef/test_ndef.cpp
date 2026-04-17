#include <unity.h>
#include <Arduino.h>
#include "hal/NDEFParser.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// ─── Helper: Build valid NDEF text record ─────────────────────────────
//
// Constructs a single 32-byte buffer covering NTAG pages 3..10 with a
// valid NDEF text record for the given payload. Buffer byte 0 is the
// first byte of page 3 (CC).
// Layout: CC(4) | TLV(03,len) | Record(D1,01,payloadLen,'T',langLen,'e','n') | text | FE

static void buildValidNDEFPages(uint8_t* pages, const char* text) {
    memset(pages, 0, 32);

    int textLen = strlen(text);
    int langLen = 2;  // "en"
    int payloadLen = 1 + langLen + textLen;  // status byte + lang + text
    int ndefRecordLen = 1 + 1 + 1 + 1 + payloadLen;  // header + typeLen + payloadLen + 'T' + payload
    int tlvLen = ndefRecordLen;

    // Capability Container (page 3)
    pages[0] = 0x01;  // magic
    pages[1] = 0x03;  // version
    pages[2] = 0x00;  // size
    pages[3] = 0x0F;  // access

    // NDEF Message TLV (starts at byte 4 = start of page 4)
    pages[4] = 0x03;              // TLV type = NDEF Message
    pages[5] = (uint8_t)tlvLen;   // TLV length

    // NDEF Record
    int idx = 6;
    pages[idx++] = 0xD1;              // MB=1, ME=1, CF=0, SR=1, IL=0, TNF=001
    pages[idx++] = 0x01;              // Type length = 1
    pages[idx++] = (uint8_t)payloadLen;  // Payload length
    pages[idx++] = 'T';               // Type = Text
    pages[idx++] = (uint8_t)langLen;  // Status byte (UTF-8, lang code len)
    pages[idx++] = 'e';               // Language code
    pages[idx++] = 'n';

    // Text payload
    for (int i = 0; i < textLen && idx < 32; i++) {
        pages[idx++] = (uint8_t)text[i];
    }

    // Terminator TLV
    if (idx < 32) {
        pages[idx] = 0xFE;
    }
}

// ─── Valid NDEF Parsing ───────────────────────────────────────────────

void test_parse_valid_short_text() {
    uint8_t buf[32];
    buildValidNDEFPages(buf, "kaa001");
    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("kaa001", result.c_str());
}

void test_parse_valid_longer_text() {
    uint8_t buf[32];
    buildValidNDEFPages(buf, "ale001");
    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("ale001", result.c_str());
}

void test_parse_text_spanning_pages() {
    // "longtoken" = 9 chars, total record overflows first 16 bytes —
    // exercises the extended single-buffer scan range.
    uint8_t buf[32];
    buildValidNDEFPages(buf, "longtoken");
    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("longtoken", result.c_str());
}

// ─── SAK Rejection ────────────────────────────────────────────────────

void test_reject_non_ntag_sak() {
    uint8_t buf[32] = {0};
    String result = hal::parseNDEFText(buf, 32, 0x08);  // MIFARE Classic
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_reject_sak_0x20() {
    uint8_t buf[32] = {0};
    String result = hal::parseNDEFText(buf, 32, 0x20);  // MIFARE DESFire
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ─── Buffer Validation ────────────────────────────────────────────────

void test_reject_short_buffer() {
    uint8_t buf[20] = {0};
    String result = hal::parseNDEFText(buf, 20, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ─── TLV Parsing Edge Cases ──────────────────────────────────────────

void test_no_ndef_tlv_returns_empty() {
    // All zeros — no TLV type 0x03 found
    uint8_t buf[32] = {0};
    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_terminator_before_ndef() {
    uint8_t buf[32] = {0};
    buf[4] = 0xFE;  // Terminator TLV before any NDEF
    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_lock_control_tlv_skipped() {
    // Lock Control TLV (0x01) with length 0, followed by NDEF Message TLV
    uint8_t buf[32];
    memset(buf, 0, 32);

    buf[4] = 0x01;   // Lock Control TLV
    buf[5] = 0x00;   // Length = 0
    // After skipping: i += 1 + 0 = 1, then i++ makes i=6
    buf[6] = 0x03;   // NDEF Message TLV
    buf[7] = 0x08;   // Length = 8 (header + typeLen + payloadLen + 'T' + payload[4])
    // NDEF record starts at buf[8]
    buf[8]  = 0xD1;  // Record header (MB|ME|SR|TNF=001)
    buf[9]  = 0x01;  // Type length = 1
    buf[10] = 0x04;  // Payload length = 4 (1 status + 1 lang + 2 text)
    buf[11] = 'T';   // Type
    buf[12] = 0x01;  // Status byte: lang code len = 1
    buf[13] = 'e';   // Language code
    buf[14] = 'a';   // Text byte 1
    buf[15] = 'b';   // Text byte 2

    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("ab", result.c_str());
}

// Regression test: TLV that starts beyond byte 11 (the old parser's
// upper bound would have missed this). Triggers only with the wider
// single-buffer scan range. Simulates a token programmed with a
// larger Lock Control TLV preamble.
void test_ndef_tlv_beyond_old_range() {
    uint8_t buf[32];
    memset(buf, 0, 32);

    // Lock Control TLV with length 10 — pushes NDEF TLV past byte 11.
    buf[4] = 0x01;                  // Lock Control TLV type
    buf[5] = 0x0A;                  // Length = 10
    // Lock data occupies buf[6..15] (10 bytes of arbitrary payload)
    for (int i = 6; i < 16; i++) buf[i] = 0xAA;
    // After skipping: i += 1 + 10 = 11, loop i++ makes i=17
    // NDEF TLV therefore lives at buf[17]
    buf[17] = 0x03;                 // NDEF Message TLV
    buf[18] = 0x0A;                 // Length = 10
    buf[19] = 0xD1;                 // Record header (MB|ME|SR|TNF=001)
    buf[20] = 0x01;                 // Type length = 1
    buf[21] = 0x06;                 // Payload length = 6 (status + lang + 4 text)
    buf[22] = 'T';                  // Type = Text
    buf[23] = 0x01;                 // Status byte (lang code len = 1)
    buf[24] = 'e';                  // Language code
    buf[25] = 't';                  // Text
    buf[26] = 'e';
    buf[27] = 's';
    buf[28] = 't';

    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("test", result.c_str());
}

// ─── NDEF Record Parsing Edge Cases ──────────────────────────────────

void test_wrong_tnf_returns_empty() {
    // TNF=100 (External type) instead of TNF=001 (Well-known)
    uint8_t buf[32];
    memset(buf, 0, 32);

    buf[4] = 0x03;   // NDEF Message TLV
    buf[5] = 0x07;   // Length
    buf[6] = 0xD4;   // Record header: TNF=100 (External)
    buf[7] = 0x01;
    buf[8] = 0x04;
    buf[9] = 'T';
    buf[10] = 0x01;
    buf[11] = 'e';
    buf[12] = 'a';
    buf[13] = 'b';

    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_wrong_type_not_T_returns_empty() {
    // Type = 'U' (URI) instead of 'T' (Text)
    uint8_t buf[32];
    memset(buf, 0, 32);

    buf[4] = 0x03;   // NDEF Message TLV
    buf[5] = 0x07;   // Length
    buf[6] = 0xD1;   // Record header: TNF=001
    buf[7] = 0x01;   // Type length = 1
    buf[8] = 0x04;   // Payload length
    buf[9] = 'U';    // Type = URI (not Text)
    buf[10] = 0x01;
    buf[11] = 'e';
    buf[12] = 'a';
    buf[13] = 'b';

    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_ndef_message_too_short() {
    // NDEF message length < 7 (minimum for a text record)
    uint8_t buf[32];
    memset(buf, 0, 32);

    buf[4] = 0x03;   // NDEF Message TLV
    buf[5] = 0x03;   // Length = 3 (too short for text record)

    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// Regression test: readable bytes that pass early TLV parsing but fail
// the record-level validation. Confirms the parser returns "" cleanly
// in this case — important because the RFID retry loop treats empty
// return as a retryable outcome.
void test_malformed_record_returns_empty_cleanly() {
    uint8_t buf[32];
    memset(buf, 0, 32);

    // Well-formed TLV header pointing at nonsense record bytes
    buf[4] = 0x03;   // NDEF Message TLV
    buf[5] = 0x0A;   // Length = 10

    // Record bytes are intentionally inconsistent:
    // - TNF is correct (0x01) but typeLength (0xFF) is absurd
    buf[6]  = 0xD1;  // Record header
    buf[7]  = 0xFF;  // Type length way too large
    buf[8]  = 0x07;  // Payload length
    buf[9]  = 'T';
    buf[10] = 0x01;
    buf[11] = 'e';
    buf[12] = 'x';
    buf[13] = 'y';
    buf[14] = 'z';
    buf[15] = 0x00;

    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// Regression: real-world tokens have a chained NDEF message where record 1
// is a Text record carrying the tokenId and record 2 is a URI record (URL)
// that extends past page 10. Old parser bailed on the whole message because
// the declared TLV length exceeded the 32-byte read buffer. Parser must
// extract the first complete record even when later records are truncated.
// Bytes captured from a real "mar004" token during PR #4 hardware validation.
void test_multi_record_first_text_extractable() {
    uint8_t buf[32] = {
        0xE1, 0x10, 0x3E, 0x00,                           // CC (page 3)
        0x03, 0x45,                                       // TLV: NDEF, len=0x45=69
        0x91, 0x01, 0x09, 'T', 0x02, 'e', 'n',            // Record 1 header (MB=1, ME=0)
        'm', 'a', 'r', '0', '0', '4',                     // Record 1 payload: "mar004"
        0x51, 0x01, 0x34, 'U', 0x04,                      // Record 2 header (URI, truncated)
        'r', 'a', 's', 'p', 'b', 'e', 'r', 'r'            // Record 2 payload (truncated)
    };
    String result = hal::parseNDEFText(buf, 32, 0x00);
    TEST_ASSERT_EQUAL_STRING("mar004", result.c_str());
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
    RUN_TEST(test_ndef_tlv_beyond_old_range);
    RUN_TEST(test_wrong_tnf_returns_empty);
    RUN_TEST(test_wrong_type_not_T_returns_empty);
    RUN_TEST(test_ndef_message_too_short);
    RUN_TEST(test_malformed_record_returns_empty_cleanly);
    RUN_TEST(test_multi_record_first_text_extractable);

    return UNITY_END();
}
