// Project: RFID Card NDEF/BMP Display - v3.4 Fixed NDEF Parsing
// Description: Fixes NDEF TLV parsing to correctly handle Lock Control TLVs
// Version: 3.4 - Corrected NDEF extraction and CollReg handling
// 
// Key Fixes in v3.4:
// 1. Proper TLV parsing - correctly skips Lock Control TLV (0x01)
// 2. Fixed NDEF text extraction from correct offsets
// 3. Improved CollReg handling to reduce warnings
// 4. Added comprehensive NDEF structure debugging

#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ─── Configuration Constants ─────────────────────────────────────
#define RFID_MAX_RETRIES 3
#define RFID_RETRY_DELAY_MS 20
#define RFID_OPERATION_DELAY_US 10
#define RFID_TIMEOUT_MS 100
#define RFID_CLOCK_DELAY_US 2

// ─── Pin Definitions ─────────────────────────────────────────────
// SD Card (VSPI - Hardware SPI Bus 2)
static const uint8_t SDSPI_SCK   = 18;
static const uint8_t SDSPI_MISO  = 19;
static const uint8_t SDSPI_MOSI  = 23;
static const uint8_t SD_CS       = 5;

// Touch Controller
#define TOUCH_CS   33
#define TOUCH_IRQ  36

// RFID Reader (Software SPI)
#define SOFT_SPI_SCK  22
#define SOFT_SPI_MOSI 27
#define SOFT_SPI_MISO 35
#define RFID_SS       3
#define RFID_RST      MFRC522::UNUSED_PIN

// ─── Global Objects ───────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
// MFRC522::Uid uid;  // COMMENTED FOR TESTING - might break serial on ST7789
MFRC522::Uid uid;  // TEMPORARY - declare but don't initialize to test

// ─── FreeRTOS Mutex for Atomic SPI Operations ────────────────────
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// ─── RF Field Control ─────────────────────────────────────────────
static bool rfFieldEnabled = false;  // Track RF field state

void enableRFField() {
    if (!rfFieldEnabled) {
        SoftSPI_WriteRegister(MFRC522::TxControlReg, 0x83);  // Enable antenna (bits 0-1 = 11)
        rfFieldEnabled = true;
        // Removed verbose logging to reduce noise
    }
}

void disableRFField() {
    if (rfFieldEnabled) {
        SoftSPI_WriteRegister(MFRC522::TxControlReg, 0x80);  // Disable antenna (bits 0-1 = 00)
        rfFieldEnabled = false;
        // Removed verbose logging to reduce noise
    }
}

// Keep MOSI pin LOW when not in use to minimize coupling
void silenceSPIPins() {
    digitalWrite(SOFT_SPI_MOSI, LOW);  // Pin 27 - Minimize electrical coupling
}

bool imageIsDisplayed = false;
String lastNDEFText = "";

AudioGeneratorWAV *wav = nullptr;
AudioFileSourceSD *file = nullptr;
AudioOutputI2S *out = nullptr;

volatile bool touchInterruptOccurred = false;
uint32_t lastTouchTime = 0;
bool lastTouchWasValid = false;
const uint32_t DOUBLE_TAP_TIMEOUT = 500;
const uint32_t TOUCH_DEBOUNCE = 50;
uint32_t lastTouchDebounce = 0;
bool sdCardPresent = false;  // Track SD card availability

// ─── RFID Operation Statistics ───────────────────────────────────
struct RFIDStats {
    uint32_t totalScans = 0;
    uint32_t successfulScans = 0;
    uint32_t failedScans = 0;
    uint32_t retryCount = 0;
    uint32_t collisionErrors = 0;
    uint32_t timeoutErrors = 0;
    uint32_t crcErrors = 0;
} rfidStats;

// ─── Software SPI Functions with Better Timing ───────────────────
inline void SPI_ClockDelay() {
    if (RFID_CLOCK_DELAY_US > 0) {
        delayMicroseconds(RFID_CLOCK_DELAY_US);
    }
}

// Atomic SPI transfer - entire byte operation protected from interrupts
byte softSPI_transfer(byte data) {
    byte result = 0;
    
    // Make entire byte transfer atomic to prevent timing corruption
    portENTER_CRITICAL(&spiMux);
    
    for (int i = 0; i < 8; ++i) {
        // Set MOSI
        digitalWrite(SOFT_SPI_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        // Clock high with fixed timing
        delayMicroseconds(2);
        digitalWrite(SOFT_SPI_SCK, HIGH);
        delayMicroseconds(2);
        
        // Read MISO
        if (digitalRead(SOFT_SPI_MISO)) {
            result |= (1 << (7 - i));
        }
        
        // Clock low with fixed timing
        digitalWrite(SOFT_SPI_SCK, LOW);
        delayMicroseconds(2);
    }
    
    portEXIT_CRITICAL(&spiMux);
    
    return result;
}

void SoftSPI_WriteRegister(MFRC522::PCD_Register reg, byte value) {
    digitalWrite(RFID_SS, LOW);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    softSPI_transfer(reg);
    softSPI_transfer(value);
    digitalWrite(RFID_SS, HIGH);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
}

void SoftSPI_WriteRegister(MFRC522::PCD_Register reg, byte count, byte *values) {
    digitalWrite(RFID_SS, LOW);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    softSPI_transfer(reg);
    for (byte i = 0; i < count; i++) {
        softSPI_transfer(values[i]);
    }
    digitalWrite(RFID_SS, HIGH);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
}

byte SoftSPI_ReadRegister(MFRC522::PCD_Register reg) {
    digitalWrite(RFID_SS, LOW);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    softSPI_transfer(reg | 0x80);
    byte value = softSPI_transfer(0);
    digitalWrite(RFID_SS, HIGH);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    silenceSPIPins();  // [SPI-FIX] Silence pins after read
    return value;
}

void SoftSPI_ReadRegister(MFRC522::PCD_Register reg, byte count, byte *values, byte rxAlign = 0) {
    if (count == 0) return;
    
    digitalWrite(RFID_SS, LOW);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    byte address = 0x80 | reg;
    softSPI_transfer(address);
    for (byte i = 0; i < count; i++) {
        values[i] = softSPI_transfer(address);
    }
    digitalWrite(RFID_SS, HIGH);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
}

// ─── Clear and Set Register Bit Masks ────────────────────────────
void SoftSPI_ClearRegisterBitMask(MFRC522::PCD_Register reg, byte mask) {
    byte tmp = SoftSPI_ReadRegister(reg);
    SoftSPI_WriteRegister(reg, tmp & (~mask));
}

void SoftSPI_SetRegisterBitMask(MFRC522::PCD_Register reg, byte mask) {
    byte tmp = SoftSPI_ReadRegister(reg);
    SoftSPI_WriteRegister(reg, tmp | mask);
}

// ─── CRC Calculation ──────────────────────────────────────────────
MFRC522::StatusCode SoftSPI_CalculateCRC(byte *data, byte length, byte *result) {
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    SoftSPI_WriteRegister(MFRC522::DivIrqReg, 0x04);
    SoftSPI_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
    SoftSPI_WriteRegister(MFRC522::FIFODataReg, length, data);
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_CalcCRC);
    
    uint16_t timeout = 5000;
    while (timeout--) {
        byte n = SoftSPI_ReadRegister(MFRC522::DivIrqReg);
        if (n & 0x04) {
            SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
            result[0] = SoftSPI_ReadRegister(MFRC522::CRCResultRegL);
            result[1] = SoftSPI_ReadRegister(MFRC522::CRCResultRegH);
            return MFRC522::STATUS_OK;
        }
        delayMicroseconds(10);
    }
    
    return MFRC522::STATUS_TIMEOUT;
}

// ─── Improved Communication Function ─────────────────────────────
MFRC522::StatusCode SoftSPI_TransceiveData(
    byte *sendData,
    byte sendLen,
    byte *backData,
    byte *backLen,
    byte *validBits = nullptr,
    byte rxAlign = 0,
    bool checkCRC = false
) {
    byte waitIRq = 0x30;  // RxIRq and IdleIRq
    byte txLastBits = validBits ? *validBits : 0;
    byte bitFraming = (rxAlign << 4) + txLastBits;
    
    // Enable timer ONLY during communication (TAuto=1)
    SoftSPI_WriteRegister(MFRC522::TModeReg, 0x80);  // Enable timer for this transaction
    
    // Stop any active command
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    
    // Clear all interrupt request bits
    SoftSPI_WriteRegister(MFRC522::ComIrqReg, 0x7F);
    
    // Flush FIFO buffer
    SoftSPI_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
    
    // Write data to FIFO
    SoftSPI_WriteRegister(MFRC522::FIFODataReg, sendLen, sendData);
    
    // Store BitFraming
    SoftSPI_WriteRegister(MFRC522::BitFramingReg, bitFraming);
    
    // Execute Transceive command
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Transceive);
    
    // StartSend=1, start transmission
    SoftSPI_SetRegisterBitMask(MFRC522::BitFramingReg, 0x80);
    
    // Wait for completion
    uint32_t timeout = millis() + RFID_TIMEOUT_MS;
    bool completed = false;
    byte irq;
    
    while (millis() < timeout) {
        irq = SoftSPI_ReadRegister(MFRC522::ComIrqReg);
        if (irq & waitIRq) {
            completed = true;
            break;
        }
        if (irq & 0x01) {  // Timeout interrupt
            return MFRC522::STATUS_TIMEOUT;
        }
        yield();  // Let other tasks run
    }
    
    // Stop transmission
    SoftSPI_ClearRegisterBitMask(MFRC522::BitFramingReg, 0x80);
    
    if (!completed) {
        return MFRC522::STATUS_TIMEOUT;
    }
    
    // Check for errors
    byte errorReg = SoftSPI_ReadRegister(MFRC522::ErrorReg);
    
    // Check for collision first (bit 3)
    if (errorReg & 0x08) {  // CollErr bit
        // Clear the collision error
        SoftSPI_WriteRegister(MFRC522::ErrorReg, 0x08);
        
        // Reset bit framing
        SoftSPI_WriteRegister(MFRC522::BitFramingReg, 0x00);
        
        // Clear FIFO
        SoftSPI_SetRegisterBitMask(MFRC522::FIFOLevelReg, 0x80);
        
        return MFRC522::STATUS_COLLISION;
    }
    
    // Check other errors
    if (errorReg & 0x13) {  // BufferOvfl, ParityErr, ProtocolErr
        Serial.printf("[TransceiveData] ErrorReg=0x%02X\n", errorReg);
        return MFRC522::STATUS_ERROR;
    }
    
    // Check collision register for additional collision info
    // CollReg bit 7 (ValuesAfterColl) and bit 5 (CollPosNotValid) are often set together
    // This is normal operation and not an error condition
    byte collReg = SoftSPI_ReadRegister(MFRC522::CollReg);
    if ((collReg & 0x20) && !(collReg & 0x80)) {  // CollPosNotValid without ValuesAfterColl
        // Only log if it's an unusual combination
        Serial.printf("[TransceiveData] Unusual CollReg: 0x%02X\n", collReg);
    }
    
    // Read received data
    if (backData && backLen) {
        byte n = SoftSPI_ReadRegister(MFRC522::FIFOLevelReg);
        if (n > *backLen) {
            return MFRC522::STATUS_NO_ROOM;
        }
        *backLen = n;
        SoftSPI_ReadRegister(MFRC522::FIFODataReg, n, backData, rxAlign);
        
        if (validBits) {
            *validBits = SoftSPI_ReadRegister(MFRC522::ControlReg) & 0x07;
        }
    }
    
    // TIMER-FIX: Disable timer after communication to prevent beeping
    SoftSPI_WriteRegister(MFRC522::TModeReg, 0x00);  // Turn OFF timer
    
    return MFRC522::STATUS_OK;
}

// ─── REQA/WUPA Commands ───────────────────────────────────────────
MFRC522::StatusCode SoftSPI_PICC_REQA_or_WUPA(byte command, byte *bufferATQA, byte *bufferSize) {
    if (!bufferATQA || *bufferSize < 2) return MFRC522::STATUS_NO_ROOM;
    
    // Set ValuesAfterColl bit = 1 (clear values received after collision)
    // This is CRITICAL for proper anticollision!
    SoftSPI_SetRegisterBitMask(MFRC522::CollReg, 0x80);
    delayMicroseconds(100);  // Give time for register to settle
    
    byte validBits = 7;  // Short frame for REQA
    MFRC522::StatusCode status = SoftSPI_TransceiveData(
        &command, 1, bufferATQA, bufferSize, &validBits
    );
    
    if (status != MFRC522::STATUS_OK) return status;
    if (*bufferSize != 2 || validBits != 0) return MFRC522::STATUS_ERROR;
    
    return MFRC522::STATUS_OK;
}

MFRC522::StatusCode SoftSPI_PICC_RequestA(byte *bufferATQA, byte *bufferSize) {
    return SoftSPI_PICC_REQA_or_WUPA(MFRC522::PICC_CMD_REQA, bufferATQA, bufferSize);
}

// ─── Improved PICC Select with Cascade Level 2 Support ───────────
MFRC522::StatusCode SoftSPI_PICC_Select(MFRC522::Uid *uid, byte validBits = 0) {
    bool uidComplete = false;
    byte cascadeLevel = 1;
    MFRC522::StatusCode result;
    byte buffer[9];
    byte uidIndex = 0;
    
    // Sanity checks
    if (validBits > 80) {
        return MFRC522::STATUS_INVALID;
    }
    
    // Clear the stored UID
    memset(uid->uidByte, 0, sizeof(uid->uidByte));
    uid->size = 0;
    uid->sak = 0;
    
    // Set ValuesAfterColl bit = 1 (clear values received after collision)
    SoftSPI_SetRegisterBitMask(MFRC522::CollReg, 0x80);
    delayMicroseconds(100);
    
    // Repeat cascade level loop until UID complete
    while (!uidComplete) {
        byte cmd;
        byte responseBuffer[5];  // For anticollision response
        byte responseLength;
        
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
        // Send anticollision command
        buffer[0] = cmd;
        buffer[1] = 0x20;  // NVB = 2 bytes (just SEL and NVB)
        
        Serial.printf("[Select CL%d] Anticollision: sending cmd=0x%02X, NVB=0x20\n", 
                     cascadeLevel, cmd);
        
        responseLength = sizeof(responseBuffer);
        byte validBitsResponse = 0;
        
        result = SoftSPI_TransceiveData(buffer, 2, responseBuffer, &responseLength, &validBitsResponse, 0);
        
        if (result != MFRC522::STATUS_OK) {
            Serial.printf("[Select CL%d] Anticollision failed: %d\n", cascadeLevel, result);
            return result;
        }
        
        // Check anticollision response
        if (responseLength != 5 || validBitsResponse != 0) {
            Serial.printf("[Select CL%d] Invalid anticollision response: len=%d, bits=%d\n", 
                         cascadeLevel, responseLength, validBitsResponse);
            return MFRC522::STATUS_ERROR;
        }
        
        // Verify BCC
        byte bcc = responseBuffer[0] ^ responseBuffer[1] ^ responseBuffer[2] ^ responseBuffer[3];
        if (bcc != responseBuffer[4]) {
            Serial.printf("[Select CL%d] BCC check failed\n", cascadeLevel);
            return MFRC522::STATUS_CRC_WRONG;
        }
        
        Serial.printf("[Select CL%d] Anticollision OK: %02X %02X %02X %02X (BCC=%02X)\n",
                     cascadeLevel, responseBuffer[0], responseBuffer[1], 
                     responseBuffer[2], responseBuffer[3], responseBuffer[4]);
        
        // === Step 2: Select ===
        // Build SELECT command
        buffer[0] = cmd;
        buffer[1] = 0x70;  // NVB = 7 bytes (all known)
        memcpy(&buffer[2], responseBuffer, 4);  // Copy UID bytes
        buffer[6] = bcc;  // BCC
        
        // Calculate CRC
        result = SoftSPI_CalculateCRC(buffer, 7, &buffer[7]);
        if (result != MFRC522::STATUS_OK) {
            return result;
        }
        
        Serial.printf("[Select CL%d] SELECT: sending 9 bytes\n", cascadeLevel);
        
        // Send SELECT command
        byte sakBuffer[3];
        byte sakLength = sizeof(sakBuffer);
        validBitsResponse = 0;
        
        result = SoftSPI_TransceiveData(buffer, 9, sakBuffer, &sakLength, &validBitsResponse, 0);
        
        if (result != MFRC522::STATUS_OK) {
            Serial.printf("[Select CL%d] SELECT failed: %d\n", cascadeLevel, result);
            return result;
        }
        
        // Check SAK response
        if (sakLength < 1 || sakLength > 3 || validBitsResponse != 0) {
            Serial.printf("[Select CL%d] Invalid SAK response: len=%d, bits=%d\n", 
                         cascadeLevel, sakLength, validBitsResponse);
            return MFRC522::STATUS_ERROR;
        }
        
        uid->sak = sakBuffer[0];
        Serial.printf("[Select CL%d] SAK=0x%02X\n", cascadeLevel, uid->sak);
        
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
                Serial.println("[Select] ERROR: More than 3 cascade levels!");
                return MFRC522::STATUS_ERROR;
            }
            Serial.printf("[Select] Cascade to level %d\n", cascadeLevel);
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
            Serial.printf("[Select] UID complete, size=%d\n", uid->size);
        }
    }
    
    return MFRC522::STATUS_OK;
}

// ─── Halt Command ─────────────────────────────────────────────────
MFRC522::StatusCode SoftSPI_PICC_HaltA() {
    byte cmdBuffer[4];
    cmdBuffer[0] = MFRC522::PICC_CMD_HLTA;
    cmdBuffer[1] = 0;
    
    MFRC522::StatusCode result = SoftSPI_CalculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (result != MFRC522::STATUS_OK) {
        return result;
    }
    
    byte responseBuffer[1];
    byte responseLength = sizeof(responseBuffer);
    
    result = SoftSPI_TransceiveData(cmdBuffer, 4, responseBuffer, &responseLength);
    
    // Timeout is expected for HALT command
    if (result == MFRC522::STATUS_TIMEOUT) {
        return MFRC522::STATUS_OK;
    }
    
    return result;
}

// ─── FIXED NDEF Text Extraction for NTAG ─────────────────────────
String extractNDEFText() {
    Serial.println("[NDEF] Starting NDEF extraction...");
    Serial.printf("[NDEF-DIAG] Pre-extraction heap: %d\n", ESP.getFreeHeap());
    
    // Only process NTAG/Ultralight cards (SAK=0x00)
    if (uid.sak != 0x00) {
        Serial.printf("[NDEF] Not an NTAG (SAK=0x%02X), skipping\n", uid.sak);
        return "";
    }
    
    String extractedText = "";
    
    // Read pages 3-6 first (16 bytes)
    byte cmdBuffer[4];
    cmdBuffer[0] = 0x30;  // READ command
    cmdBuffer[1] = 3;     // Start at page 3
    
    MFRC522::StatusCode status = SoftSPI_CalculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[NDEF] CRC failed: %d\n", status);
        return "";
    }
    
    byte buffer[18];
    byte size = sizeof(buffer);
    status = SoftSPI_TransceiveData(cmdBuffer, 4, buffer, &size);
    
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[NDEF] Read pages 3-6 failed: %d\n", status);
        return "";
    }
    
    Serial.print("[NDEF] Pages 3-6: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    
    // Read pages 7-10 to get complete NDEF message
    cmdBuffer[1] = 7;
    status = SoftSPI_CalculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (status != MFRC522::STATUS_OK) {
        return "";
    }
    
    byte buffer2[18];
    size = sizeof(buffer2);
    
    // Add delay between reads
    delay(5);
    
    status = SoftSPI_TransceiveData(cmdBuffer, 4, buffer2, &size);
    
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[NDEF] Read pages 7-10 failed: %d\n", status);
        return "";
    }
    
    Serial.print("[NDEF] Pages 7-10: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X ", buffer2[i]);
    }
    Serial.println();
    
    // Parse NDEF structure properly
    // Pages layout:
    // Page 3: E1 10 12 00 - Capability Container
    // Page 4: 01 03 A0 0C - Lock Control TLV (type=01, len=03, data=A0 0C 34)
    // Page 5: 34 03 12 D1 - End of Lock TLV (34), NDEF TLV (type=03, len=12), NDEF header (D1)
    // Page 6: 01 0E 54 02 - NDEF record header continuation
    // Page 7: 65 6E 35 33 - "en53"
    // Page 8: 3A 34 45 3A - ":4E:"
    // Page 9: 32 42 3A 30 - "2B:0"
    // Page 10: 32 FE 00 00 - "2" + terminator
    
    // Look for NDEF TLV (0x03) - it's at position 9 in our buffer
    int ndefStart = -1;
    int ndefLength = 0;
    
    // Scan through the buffer looking for TLV blocks
    for (int i = 4; i < 12; i++) {  // Start after CC
        byte tlvType = buffer[i];
        
        Serial.printf("[NDEF] Checking position %d: type=0x%02X\n", i, tlvType);
        
        if (tlvType == 0x00) {
            // NULL TLV, skip
            continue;
        } else if (tlvType == 0xFE) {
            // Terminator TLV
            break;
        } else if (tlvType == 0x01) {
            // Lock Control TLV
            if (i + 1 < 16) {
                byte lockLen = buffer[i + 1];
                Serial.printf("[NDEF] Found Lock Control TLV at %d, length=%d\n", i, lockLen);
                i += 1 + lockLen;  // Skip this TLV
            }
        } else if (tlvType == 0x03) {
            // NDEF Message TLV
            if (i + 1 < 16) {
                ndefLength = buffer[i + 1];
                ndefStart = i + 2;
                Serial.printf("[NDEF] Found NDEF TLV at %d, length=%d, start=%d\n", i, ndefLength, ndefStart);
                break;
            }
        }
    }
    
    // If we found an NDEF message
    if (ndefStart >= 0 && ndefLength > 0) {
        // Build complete NDEF message from both buffers
        byte ndefMessage[32];
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
        
        Serial.print("[NDEF] Complete NDEF message: ");
        for (int i = 0; i < ndefLength; i++) {
            Serial.printf("%02X ", ndefMessage[i]);
        }
        Serial.println();
        
        // Parse NDEF record
        if (ndefLength >= 7) {
            byte recordHeader = ndefMessage[0];
            
            Serial.printf("[NDEF] Record header: 0x%02X\n", recordHeader);
            
            // Check if it's a well-known text record
            if ((recordHeader & 0xF0) == 0xD0) {  // MB=1, ME=1, SR=1, TNF=001 (Well-known)
                byte typeLength = ndefMessage[1];
                byte payloadLength = ndefMessage[2];
                
                Serial.printf("[NDEF] Type length: %d, Payload length: %d\n", typeLength, payloadLength);
                
                if (typeLength == 1 && ndefMessage[3] == 'T') {
                    // Text record
                    byte langCodeLen = ndefMessage[4] & 0x3F;
                    
                    Serial.printf("[NDEF] Language code length: %d\n", langCodeLen);
                    
                    // Extract language code for debugging
                    String langCode = "";
                    for (int k = 0; k < langCodeLen && (5 + k) < ndefLength; k++) {
                        langCode += (char)ndefMessage[5 + k];
                    }
                    Serial.printf("[NDEF] Language: %s\n", langCode.c_str());
                    
                    // Extract the actual text
                    int textStart = 5 + langCodeLen;
                    int textLength = payloadLength - 1 - langCodeLen;
                    
                    Serial.printf("[NDEF] Text starts at %d, length %d\n", textStart, textLength);
                    
                    // Extract text
                    for (int k = 0; k < textLength && (textStart + k) < ndefLength; k++) {
                        char c = (char)ndefMessage[textStart + k];
                        extractedText += c;
                    }
                    
                    Serial.printf("[NDEF] Extracted text: '%s'\n", extractedText.c_str());
                    return extractedText;
                }
            }
        }
    }
    
    Serial.println("[NDEF] No valid NDEF text record found");
    Serial.printf("[NDEF-DIAG] Post-extraction heap: %d\n", ESP.getFreeHeap());
    return "";
}

// ─── Helper Functions ─────────────────────────────────────────────
String uidToFilename(const MFRC522::Uid &cardUid) {
    String s = "/IMG/";
    for (byte i = 0; i < cardUid.size; i++) {
        if (cardUid.uidByte[i] < 0x10) s += '0';
        s += String(cardUid.uidByte[i], HEX);
    }
    s.toUpperCase();
    s += ".bmp";
    return s;
}

String ndefToFilename(const String &ndefText) {
    if (ndefText.length() > 0) {
        String cleanText = ndefText;
        cleanText.replace(":", "");  // Remove colons
        cleanText.replace(" ", "");  // Remove spaces
        cleanText.trim();
        cleanText.toUpperCase();
        return "/IMG/" + cleanText + ".bmp";
    }
    return "";
}

String ndefToAudioFilename(const String &ndefText) {
    if (ndefText.length() > 0) {
        String cleanText = ndefText;
        cleanText.replace(":", "");  // Remove colons
        cleanText.replace(" ", "");  // Remove spaces
        cleanText.trim();
        cleanText.toUpperCase();
        return "/AUDIO/" + cleanText + ".wav";
    }
    return "";
}

String uidToAudioFilename(const MFRC522::Uid &cardUid) {
    String s = "/AUDIO/";
    for (byte i = 0; i < cardUid.size; i++) {
        if (cardUid.uidByte[i] < 0x10) s += '0';
        s += String(cardUid.uidByte[i], HEX);
    }
    s.toUpperCase();
    s += ".wav";
    return s;
}

// ─── BMP Drawing Function ─────────────────────────────────────────
void drawBmp(const String &path) {
    if (!sdCardPresent) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.println("No SD Card");
        Serial.println("[BMP] SD card not present");
        return;
    }
    
    // DIAGNOSTIC: Log before potential hang point
    Serial.printf("[SD-DIAG] About to open: %s\n", path.c_str());
    Serial.printf("[SD-DIAG] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[SD-DIAG] millis: %lu\n", millis());
    Serial.flush();  // CRITICAL: Force output before potential hang
    
    File f = SD.open(path);  // <-- HANG POINT
    
    Serial.println("[SD-DIAG] SD.open() returned!");  // Never reached on hang
    
    if (!f) {
        Serial.println("[SD-DIAG] File not found, starting TFT operations");
        Serial.flush();
        
        Serial.println("[TFT-DIAG] About to fillScreen(BLACK)");
        Serial.flush();
        tft.fillScreen(TFT_BLACK);  // <-- SUSPECTED HANG POINT
        
        Serial.println("[TFT-DIAG] fillScreen complete, setting cursor");
        tft.setCursor(0, 0);
        
        Serial.println("[TFT-DIAG] Setting text color");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        
        Serial.println("[TFT-DIAG] Setting text size");
        tft.setTextSize(2);
        
        Serial.println("[TFT-DIAG] Printing 'Missing:'");
        tft.println("Missing:");
        
        Serial.println("[TFT-DIAG] Printing path");
        tft.println(path);
        
        Serial.printf("[BMP] File not found: %s\n", path.c_str());
        Serial.println("[TFT-DIAG] All TFT operations complete");
        return;
    }
    
    Serial.println("[BMP-READ] File opened successfully!");
    Serial.printf("[BMP-READ] File size: %d bytes\n", f.size());
    Serial.flush();
    
    uint8_t header[54];
    Serial.println("[BMP-READ] About to read 54-byte header...");
    Serial.flush();
    
    size_t bytesRead = f.read(header, 54);  // <-- SUSPECTED HANG POINT!
    
    Serial.printf("[BMP-READ] Read %d bytes from header\n", bytesRead);
    Serial.flush();
    
    if (bytesRead != 54 || header[0] != 'B' || header[1] != 'M') {
        Serial.println("[BMP-READ] Invalid BMP header");
        f.close();
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.println("Bad BMP");
        return;
    }
    
    Serial.println("[BMP-PARSE] Parsing BMP dimensions...");
    Serial.flush();
    
    uint32_t width = *(uint32_t*)&header[18];
    uint32_t height = *(uint32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t compression = *(uint32_t*)&header[30];
    uint32_t dataOffset = *(uint32_t*)&header[10];
    
    Serial.printf("[BMP-PARSE] Width: %d, Height: %d, BPP: %d\n", width, height, bpp);
    Serial.flush();
    
    if (bpp != 24 || compression != 0) {
        f.close();
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.println("Unsupported BMP");
        return;
    }
    
    Serial.printf("[BMP-SEEK] Seeking to data offset: %d\n", dataOffset);
    Serial.flush();
    
    if (!f.seek(dataOffset)) {
        f.close();
        return;
    }
    
    // Allocate buffer BEFORE any TFT operations to avoid SPI conflicts
    uint16_t rowBytes = width * 3;
    Serial.printf("[MALLOC-TEST] About to allocate %d bytes...\n", rowBytes);
    Serial.flush();
    uint8_t *rowBuffer = (uint8_t*)malloc(rowBytes);
    Serial.println("[MALLOC-TEST] malloc() returned!");
    Serial.flush();
    
    if (!rowBuffer) {
        Serial.println("[MALLOC-TEST] FAILED - rowBuffer is NULL!");
        Serial.flush();
        f.close();
        return;
    }
    
    Serial.printf("[MALLOC-TEST] SUCCESS - allocated %d bytes at %p\n", rowBytes, rowBuffer);
    Serial.println("[BMP-FIX] Reading BMP row-by-row with proper SPI bus management...");
    Serial.flush();
    
    // Process each row with proper SPI bus management
    for (int y = height - 1; y >= 0; y--) {
        // STEP 1: Read from SD (SD needs SPI)
        if (y == height - 1) {
            Serial.println("[BMP-FIX] Reading first row from SD...");
            Serial.flush();
        }
        
        if (f.read(rowBuffer, rowBytes) != rowBytes) {
            Serial.printf("[BMP-FIX] Failed to read row %d\n", y);
            free(rowBuffer);
            f.close();
            return;
        }
        
        if (y == height - 1) {
            Serial.println("[BMP-FIX] First row read complete! Now writing to TFT...");
            Serial.flush();
        }
        
        // STEP 2: Write to TFT (TFT needs SPI)
        tft.startWrite();  // Lock SPI for TFT
        
        // Set address window for THIS specific row at correct Y position
        tft.setAddrWindow(0, y, width, 1);  // Draw one row at position y
        
        // Write the row data
        uint8_t *p = rowBuffer;
        for (uint32_t x = 0; x < width; x++) {
            uint8_t b = *p++;
            uint8_t g = *p++;
            uint8_t r = *p++;
            tft.pushColor(tft.color565(r, g, b));
        }
        
        tft.endWrite();  // Release SPI
        
        if (y == height - 1) {
            Serial.println("[BMP-FIX] First row written to TFT!");
            Serial.flush();
        }
        
        yield();  // Let other tasks run
    }
    
    free(rowBuffer);
    f.close();
    
    Serial.println("[BMP] Display complete");
}

// ─── Audio Functions ──────────────────────────────────────────────
void startAudio(const String &path) {
    Serial.println("[AUDIO-DIAG] startAudio() called");
    Serial.printf("[AUDIO-DIAG] Path: %s\n", path.c_str());
    Serial.flush();
    
    // Add lazy initialization with logging
    if (!out) {
        Serial.printf("[AUDIO-FIX] First-time init at %lu ms (was deferred from setup)\n", millis());
        out = new AudioOutputI2S(0, 1);
        Serial.println("[AUDIO-FIX] AudioOutputI2S created successfully");
    } else {
        Serial.println("[AUDIO-DEBUG] Audio already initialized, reusing");
    }
    
    if (!sdCardPresent) {
        Serial.println("[Audio] SD card not present, skipping audio");
        return;
    }
    
    if (wav && wav->isRunning()) {
        Serial.println("[AUDIO-DIAG] Stopping existing audio");
        stopAudio();
    }
    
    Serial.printf("[Audio] Playing: %s\n", path.c_str());
    
    Serial.println("[AUDIO-DIAG] Creating AudioFileSourceSD...");
    Serial.flush();
    file = new AudioFileSourceSD(path.c_str());
    
    if (!file || !file->isOpen()) {
        Serial.println("[Audio] File open failed");
        delete file;
        file = nullptr;
        return;
    }
    
    Serial.println("[AUDIO-DIAG] Audio file opened successfully");
    Serial.println("[AUDIO-DIAG] Creating AudioGeneratorWAV...");
    Serial.flush();
    
    wav = new AudioGeneratorWAV();
    
    Serial.println("[AUDIO-DIAG] Calling wav->begin()...");
    Serial.flush();
    
    if (!wav || !wav->begin(file, out)) {
        Serial.println("[Audio] WAV init failed");
        delete wav;
        wav = nullptr;
        delete file;
        file = nullptr;
    } else {
        Serial.println("[Audio] Playback started");
        Serial.println("[AUDIO-DIAG] Audio system fully initialized");
        Serial.printf("[AUDIO-DIAG] AudioOutputI2S at: %p\n", out);
        Serial.flush();
    }
    
    Serial.println("[AUDIO-DIAG] startAudio() complete");
}

void stopAudio() {
    if (wav && wav->isRunning()) {
        Serial.println("[Audio] Stopping playback");
        wav->stop();
    }
    
    if (wav) {
        delete wav;
        wav = nullptr;
    }
    if (file) {
        delete file;
        file = nullptr;
    }
}

// ─── Diagnostic Functions ─────────────────────────────────────────
void dumpRegisters() {
    Serial.println("\n=== MFRC522 Register Dump ===");
    Serial.printf("CommandReg:     0x%02X\n", SoftSPI_ReadRegister(MFRC522::CommandReg));
    Serial.printf("ComIrqReg:      0x%02X\n", SoftSPI_ReadRegister(MFRC522::ComIrqReg));
    Serial.printf("ErrorReg:       0x%02X\n", SoftSPI_ReadRegister(MFRC522::ErrorReg));
    Serial.printf("Status1Reg:     0x%02X\n", SoftSPI_ReadRegister(MFRC522::Status1Reg));
    Serial.printf("Status2Reg:     0x%02X\n", SoftSPI_ReadRegister(MFRC522::Status2Reg));
    Serial.printf("CollReg:        0x%02X\n", SoftSPI_ReadRegister(MFRC522::CollReg));
    Serial.printf("RFCfgReg:       0x%02X\n", SoftSPI_ReadRegister(MFRC522::RFCfgReg));
    Serial.printf("BitFramingReg:  0x%02X\n", SoftSPI_ReadRegister(MFRC522::BitFramingReg));
    Serial.println("============================\n");
}

// ─── Touch Interrupt Service Routine ─────────────────────────────
void IRAM_ATTR touchISR() {
    touchInterruptOccurred = true;
}

// ─── Setup Function ───────────────────────────────────────────────
void setup() {
    // LED diagnostic BEFORE serial - prove we're running
    pinMode(2, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(50);
        digitalWrite(2, LOW);
        delay(50);
    }
    
    Serial.begin(115200);
    delay(3000);  // INCREASED: MFRC522 library delays serial init on ST7789
    Serial.println("\n━━━ CYD RFID Scanner v3.4 ━━━");
    Serial.println("Fixed NDEF Parsing + CollReg Handling");
    Serial.printf("Free heap at start: %d bytes\n", ESP.getFreeHeap());
    
    // Silence DAC pins to prevent electrical noise/beeping from RFID polling
    pinMode(25, OUTPUT);
    pinMode(26, OUTPUT);
    digitalWrite(25, LOW);
    digitalWrite(26, LOW);
    Serial.println("[AUDIO-FIX] DAC pins 25/26 set LOW to prevent electrical noise");
    
    // Initialize TFT Display
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("NeurAI");
    tft.println("Memory Scanner");
    tft.println("v3.4 Fixed");
    
    // Initialize SD Card
    Serial.println("Initializing SD card...");
    SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI);
    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("SD Card Mount Failed - Continuing without SD");
        tft.println("SD Fail - No card");
        sdCardPresent = false;
    } else {
        tft.println("SD OK");
        sdCardPresent = true;
    }
    
    // Configure Touch Interrupt
    // CRITICAL: GPIO36 is input-only, cannot use INPUT_PULLUP!
    pinMode(TOUCH_IRQ, INPUT);  // FIX: Use INPUT instead of INPUT_PULLUP
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ), touchISR, FALLING);
    Serial.println("Touch interrupt configured");
    
    // Initialize Software SPI for RFID
    Serial.println("Initializing RFID...");
    // Serial.println("SKIPPING RFID INIT TO TEST SERIAL");
    Serial.flush();
    // RESTORED FOR TESTING WITH GOOD MODULE
    pinMode(SOFT_SPI_SCK, OUTPUT);
    pinMode(SOFT_SPI_MOSI, OUTPUT);
    pinMode(SOFT_SPI_MISO, INPUT);
    pinMode(RFID_SS, OUTPUT);
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(SOFT_SPI_SCK, LOW);
    
    // ALL MFRC522 OPERATIONS RESTORED FOR TESTING
    // Soft reset MFRC522
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
    delay(100);  // Increased delay after reset
    
    // Initialize MFRC522 with optimized settings for NTAG
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    
    // Timer settings for timeout - BUT KEEP TIMER OFF BY DEFAULT
    // TAuto=0 to prevent continuous timer running (potential beep source)
    SoftSPI_WriteRegister(MFRC522::TModeReg, 0x00);      // TAuto=0, timer OFF by default
    SoftSPI_WriteRegister(MFRC522::TPrescalerReg, 0xA9); // TModeReg[3..0] + TPrescalerReg
    SoftSPI_WriteRegister(MFRC522::TReloadRegH, 0x03);   // Reload timer with 0x3E8 = 1000
    SoftSPI_WriteRegister(MFRC522::TReloadRegL, 0xE8);   // 25ms timeout
    Serial.println("[TIMER-FIX] MFRC522 timer disabled by default to prevent beeping");
    
    // Force 100% ASK modulation
    SoftSPI_WriteRegister(MFRC522::TxASKReg, 0x40);
    
    // Set the preset value for the CRC coprocessor
    SoftSPI_WriteRegister(MFRC522::ModeReg, 0x3D);
    
    // === NTAG-Specific Optimizations ===
    
    // Configure receiver gain for maximum sensitivity (48dB)
    // This is critical for NTAG cards
    SoftSPI_WriteRegister(MFRC522::RFCfgReg, 0x70);
    
    // Set receiver threshold for NTAG
    SoftSPI_WriteRegister(MFRC522::RxThresholdReg, 0x84);
    
    // Configure modulation conductance for NTAG
    SoftSPI_WriteRegister(MFRC522::ModGsPReg, 0x3F);
    
    // Clear bit framing register
    SoftSPI_WriteRegister(MFRC522::BitFramingReg, 0x00);
    
    // Set collision register properly (ValuesAfterColl = 1)
    SoftSPI_WriteRegister(MFRC522::CollReg, 0x80);
    
    // RF Field Control - DO NOT enable antenna at startup
    Serial.println("[RF-FIELD] Antenna NOT enabled at startup (deferred for beeping fix)");
    // Previously enabled antenna here, now deferred until actual scanning
    
    // Additional delay to let antenna stabilize
    delay(10);
    
    // Read and display version
    byte version = SoftSPI_ReadRegister(MFRC522::VersionReg);
    Serial.printf("MFRC522 Version: 0x%02X ", version);
    switch(version) {
        case 0x92: Serial.println("(v2.0)"); break;
        case 0x91: Serial.println("(v1.0)"); break;
        case 0x90: Serial.println("(v0.0)"); break;
        case 0x88: Serial.println("(clone)"); break;
        default: Serial.println("(unknown)"); break;
    }
    
    // Verify critical register configuration
    Serial.println("Verifying MFRC522 configuration:");
    byte collReg = SoftSPI_ReadRegister(MFRC522::CollReg);
    Serial.printf("  CollReg: 0x%02X (bit 7 should be 1: %s)\n", 
                  collReg, (collReg & 0x80) ? "OK" : "FAIL");
    byte rfCfg = SoftSPI_ReadRegister(MFRC522::RFCfgReg);
    Serial.printf("  RFCfgReg: 0x%02X (should be 0x70 for max gain)\n", rfCfg);
    
    tft.println("RFID OK");
    
    // Initialize Audio
    Serial.println("Initializing Audio...");
    Serial.printf("[AUDIO-DEBUG] Timestamp: %lu ms - Audio init in setup() SKIPPED (deferred)\n", millis());
    // out = new AudioOutputI2S(0, 1);  // DEFERRED to prevent beeping
    Serial.println("[AUDIO-FIX] Audio initialization deferred until first use");
    
    tft.println("READY TO SCAN");
    
    // Add fix validation check at end of setup()
    Serial.println("━━━ Setup Complete ━━━");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[VALIDATION] Audio initialized: %s (should be NO for fix to work)\n", 
                  out ? "YES - FIX FAILED!" : "NO - Fix working");
    if (!out) {
        Serial.println("[SUCCESS] Audio deferred successfully - beeping should be eliminated");
    }
}

// ─── Main Loop ────────────────────────────────────────────────────
void loop() {
    // Process audio if playing
    if (wav && wav->isRunning()) {
        if (!wav->loop()) {
            stopAudio();
        }
    }
    
    // Handle image display state
    if (imageIsDisplayed) {
        if (touchInterruptOccurred) {
            touchInterruptOccurred = false;
            uint32_t now = millis();
            
            if (now - lastTouchDebounce < TOUCH_DEBOUNCE) {
                return;
            }
            
            lastTouchDebounce = now;
            
            if (lastTouchWasValid && (now - lastTouchTime) < DOUBLE_TAP_TIMEOUT) {
                // Double-tap detected
                Serial.println("[Touch] Double-tap - returning to scan mode");
                stopAudio();
                imageIsDisplayed = false;
                lastTouchWasValid = false;
                lastNDEFText = "";
                
                // Reset display
                tft.fillScreen(TFT_BLACK);
                tft.setCursor(0, 0);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.setTextSize(2);
                tft.println("NeurAI");
                tft.println("Memory Scanner");
                tft.println("READY TO SCAN");
                
                // Print statistics
                Serial.println("\n━━━ Session Statistics ━━━");
                Serial.printf("Total scans: %d\n", rfidStats.totalScans);
                Serial.printf("Successful: %d\n", rfidStats.successfulScans);
                Serial.printf("Failed: %d\n", rfidStats.failedScans);
                Serial.printf("Retries: %d\n", rfidStats.retryCount);
                Serial.printf("Collisions: %d\n", rfidStats.collisionErrors);
            } else {
                lastTouchTime = now;
                lastTouchWasValid = true;
                Serial.println("[Touch] First tap registered");
            }
        } else if (lastTouchWasValid && (millis() - lastTouchTime) >= DOUBLE_TAP_TIMEOUT) {
            lastTouchWasValid = false;
        }
        
        return;
    }
    
    // RFID Scanning Mode
    rfidStats.totalScans++;
    
    // Small delay between scans to let the field stabilize
    // INCREASED TO 500ms to reduce beeping frequency
    static uint32_t lastScanTime = 0;
    if (millis() - lastScanTime < 500) {  // Changed from 100ms to 500ms
        // Keep MOSI pin LOW between scans to minimize coupling
        digitalWrite(SOFT_SPI_MOSI, LOW);
        return;
    }
    
    lastScanTime = millis();
    
    // Enable RF field for this scan attempt
    enableRFField();
    delay(2);  // Let field stabilize
    
    // Request A
    byte atqa[2];
    byte atqaLen = sizeof(atqa);
    MFRC522::StatusCode status = SoftSPI_PICC_RequestA(atqa, &atqaLen);
    
    if (status != MFRC522::STATUS_OK) {
        // CRITICAL FIX: Set MOSI pin LOW after RFID operations to prevent audio coupling
        digitalWrite(SOFT_SPI_MOSI, LOW);
        // Disable RF field when no card detected
        disableRFField();
        return;  // No card detected
    }
    
    Serial.println("\n[RFID] Card detected!");
    Serial.printf("[RFID] ATQA: %02X %02X\n", atqa[0], atqa[1]);
    
    // Check ATQA to determine card type
    bool isNTAG = false;
    if (atqa[0] == 0x44) {
        Serial.println("[RFID] NTAG/Ultralight detected (expecting 7-byte UID)");
        isNTAG = true;
        
        // NTAG cards need extra time after REQA
        delay(5);  // Frame Waiting Time for NTAG
    } else if (atqa[0] == 0x04) {
        Serial.println("[RFID] MIFARE Classic 1K detected");
    }
    
    // Try to select the card with retry logic
    const int MAX_SELECT_RETRIES = 3;
    for (int attempt = 0; attempt < MAX_SELECT_RETRIES; attempt++) {
        if (attempt > 0) {
            Serial.printf("[RFID] Retry %d/%d\n", attempt, MAX_SELECT_RETRIES - 1);
            
            // Reset sequence
            SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
            delay(10 * attempt);  // Progressive backoff
            
            // Clear all error flags
            SoftSPI_WriteRegister(MFRC522::ErrorReg, 0x1F);
            
            // Re-configure collision register
            SoftSPI_SetRegisterBitMask(MFRC522::CollReg, 0x80);
            
            // Re-enable antenna
            SoftSPI_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);
            delay(2);
        }
        
        status = SoftSPI_PICC_Select(&uid, 0);
        
        if (status == MFRC522::STATUS_OK) {
            break;  // Success!
        }
        
        if (status == MFRC522::STATUS_COLLISION) {
            rfidStats.collisionErrors++;
            Serial.println("[RFID] Collision detected, retrying...");
            
            // For NTAG, add extra delay
            if (isNTAG) {
                delay(10);
            }
        }
    }
    
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[RFID] Select failed after %d attempts: %d\n", MAX_SELECT_RETRIES, status);
        rfidStats.failedScans++;
        
        // Dump registers for debugging
        if (status == MFRC522::STATUS_COLLISION) {
            dumpRegisters();
        }
        
        // Reset MFRC522 state
        SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
        // CRITICAL FIX: Set MOSI pin LOW after failed RFID operations
        digitalWrite(SOFT_SPI_MOSI, LOW);
        // Disable RF field after failed select
        disableRFField();
        return;
    }
    
    // Card selected successfully
    rfidStats.successfulScans++;
    Serial.print("[RFID] UID: ");
    for (byte i = 0; i < uid.size; i++) {
        Serial.printf("%02X ", uid.uidByte[i]);
    }
    Serial.printf("(size=%d, SAK=0x%02X)\n", uid.size, uid.sak);
    
    // DIAGNOSTIC: Log MFRC522 state after successful read
    Serial.printf("[RFID-DIAG] Post-select state:\n");
    Serial.printf("  ErrorReg: 0x%02X\n", SoftSPI_ReadRegister(MFRC522::ErrorReg));
    Serial.printf("  FIFOLevelReg: 0x%02X\n", SoftSPI_ReadRegister(MFRC522::FIFOLevelReg));
    Serial.printf("  ComIrqReg: 0x%02X\n", SoftSPI_ReadRegister(MFRC522::ComIrqReg));
    Serial.printf("  Status2Reg: 0x%02X\n", SoftSPI_ReadRegister(MFRC522::Status2Reg));
    
    // Try to read NDEF text
    String ndefText = extractNDEFText();
    
    String filename;
    String audioFilename;
    
    if (ndefText.length() > 0) {
        Serial.printf("[RFID] Using NDEF text: '%s'\n", ndefText.c_str());
        Serial.printf("[RFID] Using NDEF text: '%s'\n", ndefText.c_str());
        Serial.printf("[RFID] Cleaned for filename: '%s'\n", ndefText.c_str());
        filename = ndefToFilename(ndefText);
        audioFilename = ndefToAudioFilename(ndefText);
        lastNDEFText = ndefText;
        
        // Display the extracted text on screen
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.println("NDEF Found:");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.println(ndefText);
        delay(1000);
    } else {
        Serial.println("[RFID] No NDEF text, using UID");
        filename = uidToFilename(uid);
        audioFilename = uidToAudioFilename(uid);
    }
    
    Serial.printf("[RFID] BMP: %s\n", filename.c_str());
    Serial.printf("[RFID] Audio: %s\n", audioFilename.c_str());
    
    // Display the image
    Serial.println("[DIAG] About to call drawBmp()");
    Serial.printf("[DIAG] Filename: %s\n", filename.c_str());
    Serial.flush();
    drawBmp(filename);
    Serial.println("[DIAG] drawBmp() returned");  // Will we see this?
    
    // Start audio playback
    Serial.println("[DIAG] About to call startAudio()");
    Serial.printf("[DIAG] Audio filename: %s\n", audioFilename.c_str());
    Serial.flush();
    startAudio(audioFilename);
    Serial.println("[DIAG] startAudio() returned");
    
    // Update state
    imageIsDisplayed = true;
    touchInterruptOccurred = false;
    lastTouchWasValid = false;
    
    // Halt the card
    Serial.println("[DIAG] About to halt card");
    SoftSPI_PICC_HaltA();
    Serial.println("[DIAG] Card halted");
    
    // CRITICAL FIX: Ensure MOSI pin is LOW after all RFID operations
    digitalWrite(SOFT_SPI_MOSI, LOW);
    Serial.println("[AUDIO-FIX] Pin 27 (MOSI) set LOW to prevent audio coupling");
    
    // Disable RF field after successful card processing
    disableRFField();
    
    Serial.println("[RFID] Card processing complete");
}