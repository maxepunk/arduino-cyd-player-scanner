#pragma once

/**
 * @file NDEFParser.h
 * @brief Pure NDEF text extraction from raw NTAG page data.
 *
 * Extracted from RFIDReader.h extractNDEFTextInternal() for testability.
 * Takes raw NTAG page bytes as input — no hardware dependencies.
 *
 * Single-buffer API: the caller provides one contiguous buffer covering
 * NTAG pages 3..10 (32 bytes). This replaces the earlier two-buffer
 * signature which was an artifact of the previous two-READ protocol
 * implementation; the unified buffer simplifies parsing and extends
 * the TLV-scan range so NDEF TLVs past page 5 are no longer missed.
 */

#include <Arduino.h>
#include "../config.h"

namespace hal {

/**
 * Parse NDEF text record from raw NTAG page data.
 *
 * @param pages3to10  Raw bytes from NTAG pages 3-10 (32 bytes minimum).
 *                    Byte 0 is the first byte of page 3 (CC).
 * @param len         Length of pages3to10 buffer (must be >= 32).
 * @param sak         SAK byte from card selection (0x00 = NTAG/Ultralight).
 * @return            Extracted text, or empty string on failure.
 */
inline String parseNDEFText(const uint8_t* pages3to10, size_t len, uint8_t sak) {
    // Only process NTAG/Ultralight cards (SAK=0x00)
    if (sak != 0x00) {
        LOG_NDEF("[NDEF-PARSE] Not an NTAG (SAK=0x%02X), skipping\n", sak);
        return "";
    }

    if (len < 32) {
        LOG_NDEF("[NDEF-PARSE] Insufficient page data (len=%zu)\n", len);
        return "";
    }

    // Parse TLV structure — look for NDEF Message TLV (0x03).
    // CC is on page 3 (bytes 0-3); TLVs begin at byte 4 (page 4).
    // Scan range extends through byte 27 (end of page 9), leaving room
    // for the length byte at [i+1]. This range is wider than the old
    // two-buffer parser (which only scanned bytes 4..11), so tokens
    // with a longer Lock Control TLV preamble are no longer missed.
    int ndefStart = -1;
    int ndefLength = 0;

    for (int i = 4; i < 28; i++) {
        uint8_t tlvType = pages3to10[i];

        if (tlvType == 0x00) {
            // NULL TLV, skip
            continue;
        } else if (tlvType == 0xFE) {
            // Terminator TLV
            break;
        } else if (tlvType == 0x01) {
            // Lock Control TLV: length byte + <length> bytes of lock data
            if (i + 1 < (int)len) {
                uint8_t lockLen = pages3to10[i + 1];
                i += 1 + lockLen;  // Skip this TLV (for-loop i++ advances further)
            }
        } else if (tlvType == 0x03) {
            // NDEF Message TLV
            if (i + 1 < (int)len) {
                ndefLength = pages3to10[i + 1];
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

    if (ndefStart + ndefLength > (int)len) {
        LOG_NDEF("[NDEF-PARSE] FAILED: NDEF message exceeds buffer\n");
        return "";
    }

    // NDEF message lives at pages3to10[ndefStart..ndefStart+ndefLength]
    const uint8_t* msg = &pages3to10[ndefStart];

    // Parse NDEF record
    if (ndefLength < 7) {
        LOG_NDEF("[NDEF-PARSE] FAILED: NDEF message too short (%d bytes)\n", ndefLength);
        return "";
    }

    uint8_t recordHeader = msg[0];

    // Check TNF=001 (Well-known type)
    if ((recordHeader & 0x07) != 0x01) {
        LOG_NDEF("[NDEF-PARSE] FAILED: wrong TNF=0x%02X (expected 0x01)\n", recordHeader & 0x07);
        return "";
    }

    uint8_t typeLength = msg[1];
    uint8_t payloadLength = msg[2];

    // Must be a Text record (type='T', length=1)
    if (typeLength != 1 || msg[3] != 'T') {
        LOG_NDEF("[NDEF-PARSE] FAILED: not a Text record (typeLen=%d, type='%c')\n", typeLength, msg[3]);
        return "";
    }

    uint8_t langCodeLen = msg[4] & 0x3F;
    int textStart = 5 + langCodeLen;
    int textLength = payloadLength - 1 - langCodeLen;

    if (textLength <= 0 || textStart + textLength > ndefLength) {
        LOG_NDEF("[NDEF-PARSE] FAILED: text bounds invalid (start=%d, len=%d, ndefLen=%d)\n", textStart, textLength, ndefLength);
        return "";
    }

    // Extract the actual text
    String extractedText = "";
    for (int k = 0; k < textLength; k++) {
        extractedText += (char)msg[textStart + k];
    }

    LOG_NDEF("[NDEF-PARSE] Extracted: '%s'\n", extractedText.c_str());
    return extractedText;
}

} // namespace hal
