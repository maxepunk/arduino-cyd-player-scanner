#pragma once

/**
 * @file NDEFParser.h
 * @brief Pure NDEF text extraction from raw page data.
 *
 * Extracted from RFIDReader.h extractNDEFTextInternal() for testability.
 * Takes raw NTAG page bytes as input — no hardware dependencies.
 */

#include <Arduino.h>
#include "../config.h"

namespace hal {

/**
 * Parse NDEF text record from raw NTAG page data.
 *
 * @param pages3to6   Raw bytes from NTAG pages 3-6 (16 bytes minimum)
 * @param len1        Length of pages3to6 buffer
 * @param pages7to10  Raw bytes from NTAG pages 7-10 (16 bytes minimum)
 * @param len2        Length of pages7to10 buffer
 * @param sak         SAK byte from card selection (0x00 = NTAG/Ultralight)
 * @return            Extracted text, or empty string on failure
 */
inline String parseNDEFText(const uint8_t* pages3to6, size_t len1,
                            const uint8_t* pages7to10, size_t len2,
                            uint8_t sak) {
    // Only process NTAG/Ultralight cards (SAK=0x00)
    if (sak != 0x00) {
        LOG_NDEF("[NDEF-PARSE] Not an NTAG (SAK=0x%02X), skipping\n", sak);
        return "";
    }

    if (len1 < 16 || len2 < 16) {
        LOG_NDEF("[NDEF-PARSE] Insufficient page data (len1=%zu, len2=%zu)\n", len1, len2);
        return "";
    }

    // Parse TLV structure — look for NDEF Message TLV (0x03)
    int ndefStart = -1;
    int ndefLength = 0;

    // Scan through pages3to6 looking for TLV blocks (start after CC at byte 4)
    for (int i = 4; i < 12; i++) {
        uint8_t tlvType = pages3to6[i];

        if (tlvType == 0x00) {
            // NULL TLV, skip
            continue;
        } else if (tlvType == 0xFE) {
            // Terminator TLV
            break;
        } else if (tlvType == 0x01) {
            // Lock Control TLV
            if (i + 1 < 16) {
                uint8_t lockLen = pages3to6[i + 1];
                i += 1 + lockLen;  // Skip this TLV
            }
        } else if (tlvType == 0x03) {
            // NDEF Message TLV
            if (i + 1 < 16) {
                ndefLength = pages3to6[i + 1];
                ndefStart = i + 2;
                break;
            }
        }
    }

    LOG_NDEF("[NDEF-PARSE] TLV result: ndefStart=%d, ndefLength=%d\n", ndefStart, ndefLength);

    if (ndefStart < 0 || ndefLength <= 0) {
        LOG_NDEF("[NDEF-PARSE] FAILED: no NDEF TLV found\n");
        return "";
    }

    // Build complete NDEF message from both page buffers
    uint8_t ndefMessage[32];
    int msgIdx = 0;

    // Copy from first buffer (starting from ndefStart)
    for (int j = ndefStart; j < 16 && msgIdx < ndefLength; j++) {
        ndefMessage[msgIdx++] = pages3to6[j];
    }

    // Copy from second buffer if needed
    if (msgIdx < ndefLength) {
        for (int j = 0; j < 16 && msgIdx < ndefLength; j++) {
            ndefMessage[msgIdx++] = pages7to10[j];
        }
    }

    // Parse NDEF record
    if (ndefLength < 7) {
        LOG_NDEF("[NDEF-PARSE] FAILED: NDEF message too short (%d bytes)\n", ndefLength);
        return "";
    }

    uint8_t recordHeader = ndefMessage[0];

    // Check TNF=001 (Well-known type)
    if ((recordHeader & 0x07) != 0x01) {
        LOG_NDEF("[NDEF-PARSE] FAILED: wrong TNF=0x%02X (expected 0x01)\n", recordHeader & 0x07);
        return "";
    }

    uint8_t typeLength = ndefMessage[1];
    uint8_t payloadLength = ndefMessage[2];

    // Must be a Text record (type='T', length=1)
    if (typeLength != 1 || ndefMessage[3] != 'T') {
        LOG_NDEF("[NDEF-PARSE] FAILED: not a Text record (typeLen=%d, type='%c')\n", typeLength, ndefMessage[3]);
        return "";
    }

    uint8_t langCodeLen = ndefMessage[4] & 0x3F;
    int textStart = 5 + langCodeLen;
    int textLength = payloadLength - 1 - langCodeLen;

    if (textLength <= 0 || textStart + textLength > ndefLength) {
        LOG_NDEF("[NDEF-PARSE] FAILED: text bounds invalid (start=%d, len=%d, ndefLen=%d)\n", textStart, textLength, ndefLength);
        return "";
    }

    // Extract the actual text
    String extractedText = "";
    for (int k = 0; k < textLength && (textStart + k) < ndefLength; k++) {
        extractedText += (char)ndefMessage[textStart + k];
    }

    LOG_NDEF("[NDEF-PARSE] Extracted: '%s'\n", extractedText.c_str());
    return extractedText;
}

} // namespace hal
