#pragma once

#include <Arduino.h>
#include <MFRC522.h>
#include "../config.h"
#include "NDEFParser.h"

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

/**
 * Result of a single detectCard() call.
 *
 * Distinguishes "no card in field" (idle, expected most of the time)
 * from "card present but comms failed" (card is there but couldn't be
 * selected cleanly, even after retries). Callers use this to choose
 * between "silently wait for next scan cycle" (NoCard) and "surface
 * failure to the user" (CommFailed).
 */
enum class DetectResult {
    NoCard,       // No card in field — fast-path return, no retry spent
    Detected,     // UID populated, card selected, ready for NDEF read
    CommFailed    // Card present but comms broken after retries
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
    DetectResult detectCard(MFRC522::Uid& uid);
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

    // Single-page READ (0x30): returns 4 pages = 16 bytes.
    bool readPage(uint8_t page, uint8_t* buffer, uint8_t* bufferSize);

    // Multi-page FAST_READ (0x3A): returns (endPage - startPage + 1) * 4 bytes
    // in a single exchange. Used by extractNDEFTextInternal() to read pages
    // 3-10 atomically, eliminating the inter-exchange window where card state
    // could drop between two sequential READ commands.
    bool readPagesFast(uint8_t startPage, uint8_t endPage,
                       uint8_t* buffer, uint8_t* bufferSize);

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

    // Use WUPA (Wake-Up, Type A) instead of REQA so we detect cards that
    // are in HALT state as well as IDLE. REQA ignores HALT-state cards,
    // which can leave a card stranded if the previous scan halted it but
    // the card is still physically in the field.
    uint8_t command = MFRC522::PICC_CMD_WUPA;
    uint8_t validBits = 7;  // Short frame for WUPA (same framing as REQA)
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

// FAST_READ (0x3A) — reads pages [startPage..endPage] inclusive in a single
// exchange. Response is (endPage - startPage + 1) * 4 data bytes plus 2 CRC
// bytes. For NTAG215 pages 3-10, that's 32 + 2 = 34 bytes, well within the
// MFRC522's 64-byte FIFO.
//
// Collapses two sequential READ commands into one exchange, eliminating the
// inter-exchange window where card RF-coupling wobble can drop the card out
// of ACTIVE state (the dominant historical failure mode).
bool RFIDReader::readPagesFast(uint8_t startPage, uint8_t endPage,
                                uint8_t* buffer, uint8_t* bufferSize) {
    uint8_t cmdBuffer[5];
    cmdBuffer[0] = 0x3A;       // FAST_READ command
    cmdBuffer[1] = startPage;
    cmdBuffer[2] = endPage;

    MFRC522::StatusCode status = calculateCRC(cmdBuffer, 3, &cmdBuffer[3]);
    if (status != MFRC522::STATUS_OK) {
        LOG_DEBUG("[NDEF] FAST_READ CRC failed: %d\n", status);
        return false;
    }

    status = transceiveData(cmdBuffer, 5, buffer, bufferSize);

    if (status != MFRC522::STATUS_OK) {
        LOG_DEBUG("[NDEF] FAST_READ [%d..%d] failed: %d\n", startPage, endPage, status);
        return false;
    }

    uint8_t expectedBytes = (endPage - startPage + 1) * 4;
    if (*bufferSize < expectedBytes) {
        LOG_DEBUG("[NDEF] FAST_READ short response: got %d, expected >= %d\n",
                  *bufferSize, expectedBytes);
        return false;
    }

    return true;
}

String RFIDReader::extractNDEFTextInternal() {
    LOG_INFO("[NDEF] Starting NDEF extraction...\n");
    LOG_DEBUG("[NDEF-DIAG] Pre-extraction heap: %d\n", ESP.getFreeHeap());

    LOG_NDEF("[NDEF-DIAG] SAK=0x%02X, UID=", _currentUid.sak);
    for (int i = 0; i < _currentUid.size; i++) {
        LOG_NDEF("%02X", _currentUid.uidByte[i]);
    }
    LOG_NDEF("\n");

    // SAK pre-check (avoid unnecessary page reads for non-NTAG cards)
    if (_currentUid.sak != 0x00) {
        LOG_DEBUG("[NDEF] Not an NTAG (SAK=0x%02X), skipping\n", _currentUid.sak);
        return "";
    }

    // Retry loop: FAST_READ pages 3..10 as a single exchange. On failure,
    // pause briefly; after the first retry, try re-Select to recover from
    // card state drop (HALT or IDLE) that can happen if RF coupling blips
    // during the exchange.
    for (uint8_t attempt = 1; attempt <= rfid_config::MAX_RETRIES; attempt++) {
        uint8_t buffer[34];  // 32 data bytes (pages 3..10) + 2 CRC
        uint8_t size = sizeof(buffer);

        if (readPagesFast(3, 10, buffer, &size)) {
            LOG_DEBUG("[NDEF] Pages 3-10 via FAST_READ: ");
            for (int i = 0; i < 32; i++) LOG_DEBUG("%02X ", buffer[i]);
            LOG_DEBUG("\n");

            LOG_NDEF("[NDEF-DIAG] Pages 3-10 raw: ");
            for (int i = 0; i < 32; i++) LOG_NDEF("%02X ", buffer[i]);
            LOG_NDEF("\n");

            // Delegate parsing to pure function (testable without hardware)
            String result = hal::parseNDEFText(buffer, 32, _currentUid.sak);
            if (result.length() > 0) {
                if (attempt > 1) {
                    LOG_INFO("[NDEF-RETRY] Recovered on attempt %d\n", attempt);
                    _stats.retryCount += (attempt - 1);
                }
                return result;
            }
            // Bytes were readable but parser rejected them (malformed TLV,
            // truncated NDEF, etc). Probably transient bit-error — retry.
            LOG_INFO("[NDEF-RETRY] attempt %d: bytes read, parse failed\n", attempt);
        } else {
            LOG_INFO("[NDEF-RETRY] attempt %d: FAST_READ failed\n", attempt);
        }

        // Don't delay or reSelect after the final attempt — just return.
        if (attempt >= rfid_config::MAX_RETRIES) {
            break;
        }

        delay(rfid_config::RETRY_DELAY_MS);

        // After a failed read, the card may have dropped from ACTIVE back to
        // IDLE/HALT. Re-do REQA(WUPA)+Select to put it back into ACTIVE state
        // before the next FAST_READ attempt.
        uint8_t bufferATQA[2];
        uint8_t atqaSize = sizeof(bufferATQA);
        if (requestA(bufferATQA, &atqaSize) == MFRC522::STATUS_OK &&
            select(&_currentUid) == MFRC522::STATUS_OK) {
            LOG_INFO("[NDEF-RETRY] reSelect recovery OK before attempt %d\n", attempt + 1);
        } else {
            LOG_INFO("[NDEF-RETRY] reSelect failed before attempt %d\n", attempt + 1);
            // Don't break — next FAST_READ will try anyway; this just means
            // the card may have genuinely left the field.
        }
    }

    LOG_INFO("[NDEF-FAIL] All %d attempts exhausted\n", rfid_config::MAX_RETRIES);
    return "";
}

// === RF FIELD CONTROL ===

void RFIDReader::enableRFField() {
    if (!_rfFieldEnabled) {
        writeRegister(MFRC522::TxControlReg, 0x83);  // Enable antenna (bits 0-1 = 11)
        _rfFieldEnabled = true;
        // Settling time for the RF field to stabilize. NTAG passive tags
        // draw power from the field; they need a few ms to boot and respond
        // to REQA/WUPA reliably after a cold-field start. The guard above
        // means this delay only runs on actual OFF->ON transition, not on
        // every call.
        delay(rfid_config::ANTENNA_SETTLE_MS);
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
    LOG_INFO("[RFID-HAL] �  WARNING: Serial RX will be disabled!\n");
    Serial.flush(); // Force send warning before breaking serial
    delay(100);

    // Configure Software SPI pins
    pinMode(pins::RFID_SCK, OUTPUT);
    pinMode(pins::RFID_MOSI, OUTPUT);
    pinMode(pins::RFID_MISO, INPUT);
    pinMode(pins::RFID_SS, OUTPUT);  // � THIS LINE KILLS SERIAL RX (GPIO 3)
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

DetectResult RFIDReader::detectCard(MFRC522::Uid& uid) {
    if (!_initialized) {
        LOG_ERROR("RFID", "Not initialized!");
        return DetectResult::CommFailed;
    }

    _stats.totalScans++;

    // Enable RF field (includes settling delay on OFF->ON transition)
    enableRFField();

    // Retry loop. The first attempt fast-exits on timeout (no card in field)
    // so idle loop iterations don't waste MAX_RETRIES * RETRY_DELAY_MS on the
    // common no-card case. Subsequent attempts retry on any non-OK status,
    // since getting past REQA once implies a card IS present and any error
    // after that is a comms issue worth retrying.
    for (uint8_t attempt = 1; attempt <= rfid_config::MAX_RETRIES; attempt++) {
        uint8_t bufferATQA[2];
        uint8_t bufferSize = sizeof(bufferATQA);

        MFRC522::StatusCode status = requestA(bufferATQA, &bufferSize);

        if (status == MFRC522::STATUS_TIMEOUT && attempt == 1) {
            // No card in field — normal idle case. Fast return, no retry spent.
            disableRFField();
            silenceSPIPins();
            return DetectResult::NoCard;
        }

        if (status != MFRC522::STATUS_OK) {
            LOG_INFO("[RFID-RETRY] requestA attempt %d failed (status=%d)\n",
                     attempt, status);
            if (attempt < rfid_config::MAX_RETRIES) {
                delay(rfid_config::RETRY_DELAY_MS);
            }
            continue;
        }

        // REQA/WUPA succeeded — try to select
        status = select(&_currentUid);
        if (status == MFRC522::STATUS_OK) {
            // Success
            memcpy(&uid, &_currentUid, sizeof(MFRC522::Uid));
            _stats.successfulScans++;
            if (attempt > 1) {
                LOG_INFO("[RFID-RETRY] detectCard recovered on attempt %d\n", attempt);
                _stats.retryCount += (attempt - 1);
            }

            LOG_INFO("[RFID] Card detected: ");
            for (uint8_t i = 0; i < uid.size; i++) {
                LOG_INFO("%02X", uid.uidByte[i]);
            }
            LOG_INFO(" (SAK=0x%02X)\n", uid.sak);

            return DetectResult::Detected;
        }

        LOG_INFO("[RFID-RETRY] select attempt %d failed (status=%d)\n",
                 attempt, status);
        if (attempt < rfid_config::MAX_RETRIES) {
            delay(rfid_config::RETRY_DELAY_MS);
        }
    }

    // All retries exhausted — card was present but we couldn't talk to it.
    disableRFField();
    silenceSPIPins();
    _stats.failedScans++;
    LOG_INFO("[RFID-FAIL] detectCard: all %d attempts exhausted\n",
             rfid_config::MAX_RETRIES);
    return DetectResult::CommFailed;
}

String RFIDReader::extractNDEFText() {
    if (!_initialized) {
        LOG_ERROR("RFID", "Not initialized!");
        return "";
    }

    String result = extractNDEFTextInternal();

    // Only halt the card on SUCCESS. On failure we leave the card in
    // ACTIVE state so that either the in-function reSelect or a quick
    // external retry can still reach it. The disableRFField() call that
    // follows removes RF power, which forces the card back to IDLE —
    // so on the next scan cycle, WUPA + settle-delay will wake it
    // cleanly regardless of what state it was in.
    if (result.length() > 0) {
        haltA();
    }
    disableRFField();
    silenceSPIPins();

    return result;
}

} // namespace hal
