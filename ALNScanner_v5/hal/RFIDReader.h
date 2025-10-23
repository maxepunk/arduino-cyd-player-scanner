#pragma once

#include <Arduino.h>
#include <MFRC522.h>
#include "../config.h"

/**
 * RFIDReader HAL Component - ESP32 Software SPI + MFRC522 + NDEF Extraction
 *
 * Encapsulates:
 * - Software SPI bit-banging (GPIO 22/27/35)
 * - MFRC522 protocol with cascade support (4/7/10 byte UIDs)
 * - NDEF text record extraction for NTAG cards
 * - Beeping mitigation (GPIO 27 coupling to speaker)
 *
 * CRITICAL GPIO 3 CONFLICT:
 * GPIO 3 is shared between Serial RX and RFID_SS
 * Calling begin() will PERMANENTLY disable Serial RX!
 * Use DEBUG_MODE=true in production to defer initialization.
 *
 * Extracted from ALNScanner1021_Orchestrator v4.1
 * Original lines: 242-874, 2515-2612
 */

namespace hal {

// Statistics tracking
struct RFIDStats {
    uint32_t totalScans = 0;
    uint32_t successfulScans = 0;
    uint32_t failedScans = 0;
    uint32_t retryCount = 0;
    uint32_t collisionErrors = 0;
    uint32_t timeoutErrors = 0;
    uint32_t crcErrors = 0;
};

class RFIDReader {
public:
    static RFIDReader& getInstance() {
        static RFIDReader instance;
        return instance;
    }

    // Initialization
    bool begin();
    bool isInitialized() const { return _initialized; }

    // Scanning operations
    bool detectCard(MFRC522::Uid& uid);
    String extractNDEFText();

    // Field control (beeping mitigation)
    void enableRFField();
    void disableRFField();

    // Low-level operations (public for advanced use)
    void silenceSPIPins();

    // Statistics
    const RFIDStats& getStats() const { return _stats; }
    void resetStats() { _stats = {}; }

private:
    RFIDReader() = default;
    ~RFIDReader() = default;
    RFIDReader(const RFIDReader&) = delete;
    RFIDReader& operator=(const RFIDReader&) = delete;

    // === Software SPI Implementation ===

    inline void spiClockDelay() {
        if (rfid_config::CLOCK_DELAY_US > 0) {
            delayMicroseconds(rfid_config::CLOCK_DELAY_US);
        }
    }

    uint8_t softSPI_Transfer(uint8_t data);

    // === MFRC522 Register Operations ===

    void writeRegister(MFRC522::PCD_Register reg, uint8_t value);
    void writeRegister(MFRC522::PCD_Register reg, uint8_t count, uint8_t* values);
    uint8_t readRegister(MFRC522::PCD_Register reg);
    void readRegister(MFRC522::PCD_Register reg, uint8_t count, uint8_t* values, uint8_t rxAlign = 0);

    void clearRegisterBitMask(MFRC522::PCD_Register reg, uint8_t mask);
    void setRegisterBitMask(MFRC522::PCD_Register reg, uint8_t mask);

    MFRC522::StatusCode calculateCRC(uint8_t* data, uint8_t length, uint8_t* result);

    // === MFRC522 PICC Communication ===

    MFRC522::StatusCode transceiveData(
        uint8_t* sendData,
        uint8_t sendLen,
        uint8_t* backData,
        uint8_t* backLen,
        uint8_t* validBits = nullptr,
        uint8_t rxAlign = 0,
        bool checkCRC = false
    );

    MFRC522::StatusCode requestA(uint8_t* bufferATQA, uint8_t* bufferSize);
    MFRC522::StatusCode select(MFRC522::Uid* uid, uint8_t validBits = 0);
    MFRC522::StatusCode haltA();

    // === NDEF Extraction ===

    bool readPage(uint8_t page, uint8_t* buffer, uint8_t* bufferSize);
    String extractNDEFTextInternal();

    // === State ===

    static bool _initialized;
    static bool _rfFieldEnabled;
    static MFRC522::Uid _currentUid;
    static RFIDStats _stats;
    static portMUX_TYPE _spiMux;
};

// === IMPLEMENTATION ===

// Static member initialization
bool RFIDReader::_initialized = false;
bool RFIDReader::_rfFieldEnabled = false;
MFRC522::Uid RFIDReader::_currentUid = {};
RFIDStats RFIDReader::_stats = {};
portMUX_TYPE RFIDReader::_spiMux = portMUX_INITIALIZER_UNLOCKED;

// === SOFTWARE SPI IMPLEMENTATION ===

uint8_t RFIDReader::softSPI_Transfer(uint8_t data) {
    uint8_t result = 0;

    // Make entire byte transfer atomic to prevent timing corruption
    portENTER_CRITICAL(&_spiMux);

    for (int i = 0; i < 8; ++i) {
        // Set MOSI
        digitalWrite(pins::RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;

        // Clock high with fixed timing
        delayMicroseconds(2);
        digitalWrite(pins::RFID_SCK, HIGH);
        delayMicroseconds(2);

        // Read MISO
        if (digitalRead(pins::RFID_MISO)) {
            result |= (1 << (7 - i));
        }

        // Clock low with fixed timing
        digitalWrite(pins::RFID_SCK, LOW);
        delayMicroseconds(2);
    }

    portEXIT_CRITICAL(&_spiMux);

    return result;
}

void RFIDReader::writeRegister(MFRC522::PCD_Register reg, uint8_t value) {
    digitalWrite(pins::RFID_SS, LOW);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
    softSPI_Transfer(reg);
    softSPI_Transfer(value);
    digitalWrite(pins::RFID_SS, HIGH);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
}

void RFIDReader::writeRegister(MFRC522::PCD_Register reg, uint8_t count, uint8_t* values) {
    digitalWrite(pins::RFID_SS, LOW);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
    softSPI_Transfer(reg);
    for (uint8_t i = 0; i < count; i++) {
        softSPI_Transfer(values[i]);
    }
    digitalWrite(pins::RFID_SS, HIGH);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
}

uint8_t RFIDReader::readRegister(MFRC522::PCD_Register reg) {
    digitalWrite(pins::RFID_SS, LOW);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
    softSPI_Transfer(reg | 0x80);
    uint8_t value = softSPI_Transfer(0);
    digitalWrite(pins::RFID_SS, HIGH);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
    silenceSPIPins();  // Minimize speaker coupling
    return value;
}

void RFIDReader::readRegister(MFRC522::PCD_Register reg, uint8_t count, uint8_t* values, uint8_t rxAlign) {
    if (count == 0) return;

    digitalWrite(pins::RFID_SS, LOW);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
    uint8_t address = 0x80 | reg;
    softSPI_Transfer(address);
    for (uint8_t i = 0; i < count; i++) {
        values[i] = softSPI_Transfer(address);
    }
    digitalWrite(pins::RFID_SS, HIGH);
    delayMicroseconds(rfid_config::OPERATION_DELAY_US);
}

void RFIDReader::clearRegisterBitMask(MFRC522::PCD_Register reg, uint8_t mask) {
    uint8_t tmp = readRegister(reg);
    writeRegister(reg, tmp & (~mask));
}

void RFIDReader::setRegisterBitMask(MFRC522::PCD_Register reg, uint8_t mask) {
    uint8_t tmp = readRegister(reg);
    writeRegister(reg, tmp | mask);
}

MFRC522::StatusCode RFIDReader::calculateCRC(uint8_t* data, uint8_t length, uint8_t* result) {
    writeRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    writeRegister(MFRC522::DivIrqReg, 0x04);
    writeRegister(MFRC522::FIFOLevelReg, 0x80);
    writeRegister(MFRC522::FIFODataReg, length, data);
    writeRegister(MFRC522::CommandReg, MFRC522::PCD_CalcCRC);

    uint16_t timeout = 5000;
    while (timeout--) {
        uint8_t n = readRegister(MFRC522::DivIrqReg);
        if (n & 0x04) {
            writeRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
            result[0] = readRegister(MFRC522::CRCResultRegL);
            result[1] = readRegister(MFRC522::CRCResultRegH);
            return MFRC522::STATUS_OK;
        }
        delayMicroseconds(10);
    }

    return MFRC522::STATUS_TIMEOUT;
}

// === MFRC522 PICC COMMUNICATION ===

MFRC522::StatusCode RFIDReader::transceiveData(
    uint8_t* sendData,
    uint8_t sendLen,
    uint8_t* backData,
    uint8_t* backLen,
    uint8_t* validBits,
    uint8_t rxAlign,
    bool checkCRC
) {
    uint8_t waitIRq = 0x30;  // RxIRq and IdleIRq
    uint8_t txLastBits = validBits ? *validBits : 0;
    uint8_t bitFraming = (rxAlign << 4) + txLastBits;

    // Enable timer ONLY during communication (TAuto=1)
    writeRegister(MFRC522::TModeReg, 0x80);

    // Stop any active command
    writeRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);

    // Clear all interrupt request bits
    writeRegister(MFRC522::ComIrqReg, 0x7F);

    // Flush FIFO buffer
    writeRegister(MFRC522::FIFOLevelReg, 0x80);

    // Write data to FIFO
    writeRegister(MFRC522::FIFODataReg, sendLen, sendData);

    // Store BitFraming
    writeRegister(MFRC522::BitFramingReg, bitFraming);

    // Execute Transceive command
    writeRegister(MFRC522::CommandReg, MFRC522::PCD_Transceive);

    // StartSend=1, start transmission
    setRegisterBitMask(MFRC522::BitFramingReg, 0x80);

    // Wait for completion
    uint32_t timeout = millis() + rfid_config::TIMEOUT_MS;
    bool completed = false;
    uint8_t irq;

    while (millis() < timeout) {
        irq = readRegister(MFRC522::ComIrqReg);
        if (irq & waitIRq) {
            completed = true;
            break;
        }
        if (irq & 0x01) {  // Timeout interrupt
            _stats.timeoutErrors++;
            return MFRC522::STATUS_TIMEOUT;
        }
        yield();  // Let other tasks run
    }

    // Stop transmission
    clearRegisterBitMask(MFRC522::BitFramingReg, 0x80);

    if (!completed) {
        _stats.timeoutErrors++;
        return MFRC522::STATUS_TIMEOUT;
    }

    // Check for errors
    uint8_t errorReg = readRegister(MFRC522::ErrorReg);

    // Check for collision first (bit 3)
    if (errorReg & 0x08) {  // CollErr bit
        // Clear the collision error
        writeRegister(MFRC522::ErrorReg, 0x08);

        // Reset bit framing
        writeRegister(MFRC522::BitFramingReg, 0x00);

        // Clear FIFO
        setRegisterBitMask(MFRC522::FIFOLevelReg, 0x80);

        _stats.collisionErrors++;
        return MFRC522::STATUS_COLLISION;
    }

    // Check other errors
    if (errorReg & 0x13) {  // BufferOvfl, ParityErr, ProtocolErr
        LOG_DEBUG("[TransceiveData] ErrorReg=0x%02X\n", errorReg);
        return MFRC522::STATUS_ERROR;
    }

    // Read received data
    if (backData && backLen) {
        uint8_t n = readRegister(MFRC522::FIFOLevelReg);
        if (n > *backLen) {
            return MFRC522::STATUS_NO_ROOM;
        }
        *backLen = n;
        readRegister(MFRC522::FIFODataReg, n, backData, rxAlign);

        if (validBits) {
            *validBits = readRegister(MFRC522::ControlReg) & 0x07;
        }
    }

    // TIMER-FIX: Disable timer after communication to prevent beeping
    writeRegister(MFRC522::TModeReg, 0x00);

    return MFRC522::STATUS_OK;
}

MFRC522::StatusCode RFIDReader::requestA(uint8_t* bufferATQA, uint8_t* bufferSize) {
    if (!bufferATQA || *bufferSize < 2) return MFRC522::STATUS_NO_ROOM;

    // Set ValuesAfterColl bit = 1 (clear values received after collision)
    // This is CRITICAL for proper anticollision!
    setRegisterBitMask(MFRC522::CollReg, 0x80);
    delayMicroseconds(100);  // Give time for register to settle

    uint8_t command = MFRC522::PICC_CMD_REQA;
    uint8_t validBits = 7;  // Short frame for REQA
    MFRC522::StatusCode status = transceiveData(
        &command, 1, bufferATQA, bufferSize, &validBits
    );

    if (status != MFRC522::STATUS_OK) return status;
    if (*bufferSize != 2 || validBits != 0) return MFRC522::STATUS_ERROR;

    return MFRC522::STATUS_OK;
}

MFRC522::StatusCode RFIDReader::select(MFRC522::Uid* uid, uint8_t validBits) {
    bool uidComplete = false;
    uint8_t cascadeLevel = 1;
    MFRC522::StatusCode result;
    uint8_t buffer[9];
    uint8_t uidIndex = 0;

    // Sanity checks
    if (validBits > 80) {
        return MFRC522::STATUS_INVALID;
    }

    // Clear the stored UID
    memset(uid->uidByte, 0, sizeof(uid->uidByte));
    uid->size = 0;
    uid->sak = 0;

    // Set ValuesAfterColl bit = 1 (clear values received after collision)
    setRegisterBitMask(MFRC522::CollReg, 0x80);
    delayMicroseconds(100);

    // Repeat cascade level loop until UID complete
    while (!uidComplete) {
        uint8_t cmd;
        uint8_t responseBuffer[5];  // For anticollision response
        uint8_t responseLength;

        // Set command for current cascade level
        switch (cascadeLevel) {
            case 1:
                cmd = MFRC522::PICC_CMD_SEL_CL1;
                uidIndex = 0;
                break;

            case 2:
                cmd = MFRC522::PICC_CMD_SEL_CL2;
                uidIndex = 3;
                break;

            case 3:
                cmd = MFRC522::PICC_CMD_SEL_CL3;
                uidIndex = 6;
                break;

            default:
                return MFRC522::STATUS_INTERNAL_ERROR;
        }

        // === Step 1: Anticollision ===
        buffer[0] = cmd;
        buffer[1] = 0x20;  // NVB = 2 bytes (just SEL and NVB)

        LOG_DEBUG("[Select CL%d] Anticollision: cmd=0x%02X, NVB=0x20\n", cascadeLevel, cmd);

        responseLength = sizeof(responseBuffer);
        uint8_t validBitsResponse = 0;

        result = transceiveData(buffer, 2, responseBuffer, &responseLength, &validBitsResponse, 0);

        if (result != MFRC522::STATUS_OK) {
            LOG_DEBUG("[Select CL%d] Anticollision failed: %d\n", cascadeLevel, result);
            return result;
        }

        // Check anticollision response
        if (responseLength != 5 || validBitsResponse != 0) {
            LOG_DEBUG("[Select CL%d] Invalid anticollision response: len=%d, bits=%d\n",
                        cascadeLevel, responseLength, validBitsResponse);
            return MFRC522::STATUS_ERROR;
        }

        // Verify BCC
        uint8_t bcc = responseBuffer[0] ^ responseBuffer[1] ^ responseBuffer[2] ^ responseBuffer[3];
        if (bcc != responseBuffer[4]) {
            LOG_DEBUG("[Select CL%d] BCC check failed\n", cascadeLevel);
            _stats.crcErrors++;
            return MFRC522::STATUS_CRC_WRONG;
        }

        LOG_DEBUG("[Select CL%d] Anticollision OK: %02X %02X %02X %02X (BCC=%02X)\n",
                  cascadeLevel, responseBuffer[0], responseBuffer[1],
                  responseBuffer[2], responseBuffer[3], responseBuffer[4]);

        // === Step 2: Select ===
        buffer[0] = cmd;
        buffer[1] = 0x70;  // NVB = 7 bytes (all known)
        memcpy(&buffer[2], responseBuffer, 4);  // Copy UID bytes
        buffer[6] = bcc;  // BCC

        // Calculate CRC
        result = calculateCRC(buffer, 7, &buffer[7]);
        if (result != MFRC522::STATUS_OK) {
            return result;
        }

        LOG_DEBUG("[Select CL%d] SELECT: sending 9 bytes\n", cascadeLevel);

        // Send SELECT command
        uint8_t sakBuffer[3];
        uint8_t sakLength = sizeof(sakBuffer);
        validBitsResponse = 0;

        result = transceiveData(buffer, 9, sakBuffer, &sakLength, &validBitsResponse, 0);

        if (result != MFRC522::STATUS_OK) {
            LOG_DEBUG("[Select CL%d] SELECT failed: %d\n", cascadeLevel, result);
            return result;
        }

        // Check SAK response
        if (sakLength < 1 || sakLength > 3 || validBitsResponse != 0) {
            LOG_DEBUG("[Select CL%d] Invalid SAK response: len=%d, bits=%d\n",
                        cascadeLevel, sakLength, validBitsResponse);
            return MFRC522::STATUS_ERROR;
        }

        uid->sak = sakBuffer[0];
        LOG_DEBUG("[Select CL%d] SAK=0x%02X\n", cascadeLevel, uid->sak);

        // Process UID bytes based on cascade level
        if (cascadeLevel == 1) {
            if (responseBuffer[0] == 0x88) {
                // CT byte - 7 or 10 byte UID
                uid->uidByte[0] = responseBuffer[1];
                uid->uidByte[1] = responseBuffer[2];
                uid->uidByte[2] = responseBuffer[3];
            } else {
                // No CT - could be 4 byte UID
                uid->uidByte[0] = responseBuffer[0];
                uid->uidByte[1] = responseBuffer[1];
                uid->uidByte[2] = responseBuffer[2];
                uid->uidByte[3] = responseBuffer[3];
            }
        } else {
            // Cascade level 2 or 3 - copy next bytes
            uid->uidByte[uidIndex] = responseBuffer[0];
            uid->uidByte[uidIndex + 1] = responseBuffer[1];
            uid->uidByte[uidIndex + 2] = responseBuffer[2];
            uid->uidByte[uidIndex + 3] = responseBuffer[3];
        }

        // Check if UID is complete
        if (uid->sak & 0x04) {
            // UID not complete, cascade bit set
            cascadeLevel++;
            if (cascadeLevel > 3) {
                LOG_ERROR("RFID", "More than 3 cascade levels!");
                return MFRC522::STATUS_ERROR;
            }
            LOG_DEBUG("[Select] Cascade to level %d\n", cascadeLevel);
        } else {
            // UID complete
            uidComplete = true;
            if (cascadeLevel == 1) {
                uid->size = (responseBuffer[0] == 0x88) ? 7 : 4;
            } else if (cascadeLevel == 2) {
                uid->size = 7;
            } else {
                uid->size = 10;
            }
            LOG_DEBUG("[Select] UID complete, size=%d\n", uid->size);
        }
    }

    return MFRC522::STATUS_OK;
}

MFRC522::StatusCode RFIDReader::haltA() {
    uint8_t cmdBuffer[4];
    cmdBuffer[0] = MFRC522::PICC_CMD_HLTA;
    cmdBuffer[1] = 0;

    MFRC522::StatusCode result = calculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (result != MFRC522::STATUS_OK) {
        return result;
    }

    uint8_t responseBuffer[1];
    uint8_t responseLength = sizeof(responseBuffer);

    result = transceiveData(cmdBuffer, 4, responseBuffer, &responseLength);

    // Timeout is expected for HALT command
    if (result == MFRC522::STATUS_TIMEOUT) {
        return MFRC522::STATUS_OK;
    }

    return result;
}

// === NDEF EXTRACTION ===

bool RFIDReader::readPage(uint8_t page, uint8_t* buffer, uint8_t* bufferSize) {
    uint8_t cmdBuffer[4];
    cmdBuffer[0] = 0x30;  // READ command
    cmdBuffer[1] = page;

    MFRC522::StatusCode status = calculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (status != MFRC522::STATUS_OK) {
        LOG_DEBUG("[NDEF] CRC failed for page %d: %d\n", page, status);
        return false;
    }

    status = transceiveData(cmdBuffer, 4, buffer, bufferSize);

    if (status != MFRC522::STATUS_OK) {
        LOG_DEBUG("[NDEF] Read page %d failed: %d\n", page, status);
        return false;
    }

    return true;
}

String RFIDReader::extractNDEFTextInternal() {
    LOG_INFO("[NDEF] Starting NDEF extraction...\n");
    LOG_DEBUG("[NDEF-DIAG] Pre-extraction heap: %d\n", ESP.getFreeHeap());

    // Only process NTAG/Ultralight cards (SAK=0x00)
    if (_currentUid.sak != 0x00) {
        LOG_DEBUG("[NDEF] Not an NTAG (SAK=0x%02X), skipping\n", _currentUid.sak);
        return "";
    }

    String extractedText = "";

    // Read pages 3-6 first (16 bytes)
    uint8_t buffer[18];
    uint8_t size = sizeof(buffer);

    if (!readPage(3, buffer, &size)) {
        return "";
    }

    LOG_DEBUG("[NDEF] Pages 3-6: ");
    for (int i = 0; i < 16; i++) {
        LOG_DEBUG("%02X ", buffer[i]);
    }
    LOG_DEBUG("\n");

    // Read pages 7-10 to get complete NDEF message
    uint8_t buffer2[18];
    size = sizeof(buffer2);

    // Add delay between reads
    delay(5);

    if (!readPage(7, buffer2, &size)) {
        return "";
    }

    LOG_DEBUG("[NDEF] Pages 7-10: ");
    for (int i = 0; i < 16; i++) {
        LOG_DEBUG("%02X ", buffer2[i]);
    }
    LOG_DEBUG("\n");

    // Parse NDEF structure
    // Look for NDEF TLV (0x03)
    int ndefStart = -1;
    int ndefLength = 0;

    // Scan through the buffer looking for TLV blocks
    for (int i = 4; i < 12; i++) {  // Start after CC
        uint8_t tlvType = buffer[i];

        LOG_DEBUG("[NDEF] Checking position %d: type=0x%02X\n", i, tlvType);

        if (tlvType == 0x00) {
            // NULL TLV, skip
            continue;
        } else if (tlvType == 0xFE) {
            // Terminator TLV
            break;
        } else if (tlvType == 0x01) {
            // Lock Control TLV
            if (i + 1 < 16) {
                uint8_t lockLen = buffer[i + 1];
                LOG_DEBUG("[NDEF] Found Lock Control TLV at %d, length=%d\n", i, lockLen);
                i += 1 + lockLen;  // Skip this TLV
            }
        } else if (tlvType == 0x03) {
            // NDEF Message TLV
            if (i + 1 < 16) {
                ndefLength = buffer[i + 1];
                ndefStart = i + 2;
                LOG_DEBUG("[NDEF] Found NDEF TLV at %d, length=%d, start=%d\n", i, ndefLength, ndefStart);
                break;
            }
        }
    }

    // If we found an NDEF message
    if (ndefStart >= 0 && ndefLength > 0) {
        // Build complete NDEF message from both buffers
        uint8_t ndefMessage[32];
        int msgIdx = 0;

        // Copy from first buffer (starting from ndefStart)
        for (int j = ndefStart; j < 16 && msgIdx < ndefLength; j++) {
            ndefMessage[msgIdx++] = buffer[j];
        }

        // Copy from second buffer if needed
        if (msgIdx < ndefLength) {
            for (int j = 0; j < 16 && msgIdx < ndefLength; j++) {
                ndefMessage[msgIdx++] = buffer2[j];
            }
        }

        LOG_DEBUG("[NDEF] Complete NDEF message: ");
        for (int i = 0; i < ndefLength; i++) {
            LOG_DEBUG("%02X ", ndefMessage[i]);
        }
        LOG_DEBUG("\n");

        // Parse NDEF record
        if (ndefLength >= 7) {
            uint8_t recordHeader = ndefMessage[0];

            LOG_DEBUG("[NDEF] Record header: 0x%02X\n", recordHeader);

            // Check if it's a well-known text record
            if ((recordHeader & 0xF0) == 0xD0) {  // MB=1, ME=1, SR=1, TNF=001 (Well-known)
                uint8_t typeLength = ndefMessage[1];
                uint8_t payloadLength = ndefMessage[2];

                LOG_DEBUG("[NDEF] Type length: %d, Payload length: %d\n", typeLength, payloadLength);

                if (typeLength == 1 && ndefMessage[3] == 'T') {
                    // Text record
                    uint8_t langCodeLen = ndefMessage[4] & 0x3F;

                    LOG_DEBUG("[NDEF] Language code length: %d\n", langCodeLen);

                    // Extract language code for debugging
                    String langCode = "";
                    for (int k = 0; k < langCodeLen && (5 + k) < ndefLength; k++) {
                        langCode += (char)ndefMessage[5 + k];
                    }
                    LOG_DEBUG("[NDEF] Language: %s\n", langCode.c_str());

                    // Extract the actual text
                    int textStart = 5 + langCodeLen;
                    int textLength = payloadLength - 1 - langCodeLen;

                    LOG_DEBUG("[NDEF] Text starts at %d, length %d\n", textStart, textLength);

                    // Extract text
                    for (int k = 0; k < textLength && (textStart + k) < ndefLength; k++) {
                        char c = (char)ndefMessage[textStart + k];
                        extractedText += c;
                    }

                    LOG_INFO("[NDEF] Extracted text: '%s'\n", extractedText.c_str());
                    return extractedText;
                }
            }
        }
    }

    LOG_DEBUG("[NDEF] No valid NDEF text record found\n");
    LOG_DEBUG("[NDEF-DIAG] Post-extraction heap: %d\n", ESP.getFreeHeap());
    return "";
}

// === RF FIELD CONTROL ===

void RFIDReader::enableRFField() {
    if (!_rfFieldEnabled) {
        writeRegister(MFRC522::TxControlReg, 0x83);  // Enable antenna (bits 0-1 = 11)
        _rfFieldEnabled = true;
        LOG_DEBUG("[RF-FIELD] Antenna enabled\n");
    }
}

void RFIDReader::disableRFField() {
    if (_rfFieldEnabled) {
        writeRegister(MFRC522::TxControlReg, 0x80);  // Disable antenna (bits 0-1 = 00)
        _rfFieldEnabled = false;
        LOG_DEBUG("[RF-FIELD] Antenna disabled\n");
    }
}

void RFIDReader::silenceSPIPins() {
    digitalWrite(pins::RFID_MOSI, LOW);  // Pin 27 - Minimize electrical coupling to speaker
}

// === PUBLIC API ===

bool RFIDReader::begin() {
    if (_initialized) {
        LOG_INFO("[RFID-HAL] Already initialized, skipping\n");
        return true;
    }

    LOG_INFO("[RFID-HAL] PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP\n");
    LOG_INFO("[RFID-HAL]   INITIALIZING RFID READER\n");
    LOG_INFO("[RFID-HAL] PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP\n");
    LOG_INFO("[RFID-HAL]    WARNING: Serial RX will be disabled!\n");
    Serial.flush(); // Force send warning before breaking serial
    delay(100);

    // Configure Software SPI pins
    pinMode(pins::RFID_SCK, OUTPUT);
    pinMode(pins::RFID_MOSI, OUTPUT);
    pinMode(pins::RFID_MISO, INPUT);
    pinMode(pins::RFID_SS, OUTPUT);  //   THIS LINE KILLS SERIAL RX (GPIO 3)
    digitalWrite(pins::RFID_SS, HIGH);
    digitalWrite(pins::RFID_SCK, LOW);

    LOG_INFO("[RFID-HAL]  Serial RX now disabled (GPIO 3 reassigned)\n");

    // Soft reset MFRC522
    writeRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
    delay(100);  // Increased delay after reset

    // Initialize MFRC522 with optimized settings for NTAG
    writeRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);

    // Timer settings - KEEP TIMER OFF BY DEFAULT (beeping mitigation)
    writeRegister(MFRC522::TModeReg, 0x00);      // TAuto=0, timer OFF by default
    writeRegister(MFRC522::TPrescalerReg, 0xA9); // TModeReg[3..0] + TPrescalerReg
    writeRegister(MFRC522::TReloadRegH, 0x03);   // Reload timer with 0x3E8 = 1000
    writeRegister(MFRC522::TReloadRegL, 0xE8);   // 25ms timeout
    LOG_INFO("[TIMER-FIX] MFRC522 timer disabled by default to prevent beeping\n");

    // Force 100% ASK modulation
    writeRegister(MFRC522::TxASKReg, 0x40);

    // Set the preset value for the CRC coprocessor
    writeRegister(MFRC522::ModeReg, 0x3D);

    // === NTAG-Specific Optimizations ===

    // Configure receiver gain for maximum sensitivity (48dB)
    writeRegister(MFRC522::RFCfgReg, 0x70);

    // Set receiver threshold for NTAG
    writeRegister(MFRC522::RxThresholdReg, 0x84);

    // Configure modulation conductance for NTAG
    writeRegister(MFRC522::ModGsPReg, 0x3F);

    // Clear bit framing register
    writeRegister(MFRC522::BitFramingReg, 0x00);

    // Set collision register properly (ValuesAfterColl = 1)
    writeRegister(MFRC522::CollReg, 0x80);

    // RF Field Control - DO NOT enable antenna at startup (deferred for beeping fix)
    LOG_INFO("[RF-FIELD] Antenna NOT enabled at startup (deferred until scan)\n");

    // Additional delay to let settings stabilize
    delay(10);

    // Read and display version
    uint8_t version = readRegister(MFRC522::VersionReg);
    LOG_INFO("[RFID-HAL] MFRC522 Version: 0x%02X ", version);
    switch(version) {
        case 0x92: LOG_INFO("(v2.0)\n"); break;
        case 0x91: LOG_INFO("(v1.0)\n"); break;
        case 0x90: LOG_INFO("(v0.0)\n"); break;
        case 0x88: LOG_INFO("(clone)\n"); break;
        default: LOG_INFO("(unknown)\n"); break;
    }

    // Verify critical register configuration
    LOG_INFO("[RFID-HAL] Verifying configuration:\n");
    uint8_t collReg = readRegister(MFRC522::CollReg);
    LOG_INFO("  CollReg: 0x%02X (bit 7 should be 1: %s)\n",
              collReg, (collReg & 0x80) ? "OK" : "FAIL");
    uint8_t rfCfg = readRegister(MFRC522::RFCfgReg);
    LOG_INFO("  RFCfgReg: 0x%02X (should be 0x70 for max gain)\n", rfCfg);

    // Mark as initialized
    _initialized = true;

    LOG_INFO("[RFID-HAL]  RFID initialized successfully\n");
    LOG_INFO("[RFID-HAL] PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP\n\n");

    return true;
}

bool RFIDReader::detectCard(MFRC522::Uid& uid) {
    if (!_initialized) {
        LOG_ERROR("RFID", "Not initialized!");
        return false;
    }

    _stats.totalScans++;

    // Enable RF field for scanning
    enableRFField();

    // Request card
    uint8_t bufferATQA[2];
    uint8_t bufferSize = sizeof(bufferATQA);

    MFRC522::StatusCode status = requestA(bufferATQA, &bufferSize);

    if (status != MFRC522::STATUS_OK) {
        disableRFField();  // Disable when not scanning (beeping mitigation)
        silenceSPIPins();
        _stats.failedScans++;
        return false;
    }

    // Select card (cascade selection for 7-byte UIDs)
    status = select(&_currentUid);

    if (status != MFRC522::STATUS_OK) {
        disableRFField();
        silenceSPIPins();
        _stats.failedScans++;
        return false;
    }

    // Success - copy UID to output
    memcpy(&uid, &_currentUid, sizeof(MFRC522::Uid));
    _stats.successfulScans++;

    LOG_INFO("[RFID] Card detected: ");
    for (uint8_t i = 0; i < uid.size; i++) {
        LOG_INFO("%02X", uid.uidByte[i]);
    }
    LOG_INFO(" (SAK=0x%02X)\n", uid.sak);

    return true;
}

String RFIDReader::extractNDEFText() {
    if (!_initialized) {
        LOG_ERROR("RFID", "Not initialized!");
        return "";
    }

    String result = extractNDEFTextInternal();

    // Halt card and disable field after extraction
    haltA();
    disableRFField();
    silenceSPIPins();

    return result;
}

} // namespace hal
