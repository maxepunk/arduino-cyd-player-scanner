// Project: RFID Card NDEF/BMP Display - v4.1 Full Orchestrator
// Description: Implements all 7 user stories for orchestrator integration
// Version: 4.1 - Phases 6-10 Complete (Queue Sync, Overflow, Status Screen)
// 
// Key Features in v4.1:
// 1. [PHASE 6] Background queue upload (auto-sync on Core 0)
// 2. [PHASE 6 FIX] Safe queue removal (stream-based, no RAM spike)
// 3. [PHASE 7] Queue overflow protection (FIFO, 100-scan limit)
// 4. [PHASE 8] Tap-for-Status screen (shows connection, queue, team)
// 5. [PHASE 9] Configurable token sync (SYNC_TOKENS=false)
// 6. [PHASE 10] Full instrumentation for diagnostics (DIAG_NETWORK)

#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <vector>  // Phase 6: std::vector for batch queue operations

// ─── Configuration Constants ─────────────────────────────────────
#define RFID_MAX_RETRIES 3
#define RFID_RETRY_DELAY_MS 20
#define RFID_OPERATION_DELAY_US 10
#define RFID_TIMEOUT_MS 100
#define RFID_CLOCK_DELAY_US 2

// Touch IRQ pulse width filter (validated Oct 19, 2025 - test-45v3)
// Threshold: 10ms separates WiFi EMI (<0.01ms) from real touches (>70ms)
#define TOUCH_PULSE_WIDTH_THRESHOLD_US 10000  // 10 milliseconds

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

// Touch IRQ tracking with pulse width filtering
volatile bool touchInterruptOccurred = false;
volatile uint32_t touchInterruptTime = 0;
uint32_t lastTouchTime = 0;
bool lastTouchWasValid = false;
const uint32_t DOUBLE_TAP_TIMEOUT = 500;
const uint32_t TOUCH_DEBOUNCE = 50;
uint32_t lastTouchDebounce = 0;
bool sdCardPresent = false;  // Track SD card availability

// ─── DEBUG MODE FIX: GPIO 3 Pin Conflict ─────────────────────────
// PROBLEM: GPIO 3 is used by both Serial RX and RFID_SS
//   - Serial.begin() claims GPIO 3 for receiving serial commands
//   - pinMode(RFID_SS, OUTPUT) reconfigures GPIO 3, breaking serial input
// SOLUTION: Boot into "Debug Mode" with RFID disabled
//   - Allows serial commands (STATUS, CONFIG, etc.) to work
//   - Send START_SCANNER command to manually enable RFID (one-way, kills serial RX)
bool rfidInitialized = false; // Tracks whether RFID has been initialized

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

// ══════════════════════════════════════════════════════════════════
// ═══ ORCHESTRATOR INTEGRATION (v4.1) ══════════════════════════════
// ══════════════════════════════════════════════════════════════════

// ─── Configuration (from SD:/config.txt) ─────────────────────────
String wifiSSID = "";
String wifiPassword = "";
String orchestratorURL = "";
String teamID = "";
String deviceID = "";
bool syncTokens = true; // [PHASE 9] Default to true, configurable
bool debugMode = false; // [DEBUG_MODE] Default to production, 30s override available

// ─── Connection State ─────────────────────────────────────────────
enum ConnectionState {
  ORCH_DISCONNECTED,     // WiFi not connected
  ORCH_WIFI_CONNECTED,   // WiFi connected, orchestrator status unknown
  ORCH_CONNECTED         // WiFi + orchestrator both reachable
};

volatile ConnectionState connState = ORCH_DISCONNECTED;
bool statusScreenDisplayed = false; // [PHASE 8] Track status screen state

// ─── FreeRTOS Synchronization for Orchestrator ───────────────────
SemaphoreHandle_t sdMutex = NULL;
portMUX_TYPE connStateMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE queueSizeMux = portMUX_INITIALIZER_UNLOCKED;

volatile int queueSizeCached = 0;

// ─── Queue Configuration ──────────────────────────────────────────
#define MAX_QUEUE_SIZE 100 // [PHASE 7] US5
#define QUEUE_FILE "/queue.jsonl"
#define QUEUE_TEMP_FILE "/queue.tmp" // [PHASE 6 FIX]
#define CONFIG_FILE "/config.txt"
#define TOKEN_DB_FILE "/tokens.json"
#define DEVICE_ID_FILE "/device_id.txt"
#define MAX_TOKEN_DB_SIZE 50000 // [PHASE 9] 50KB limit

// ─── Scan Request (for orchestrator communication) ───────────────
struct ScanRequest {
  String tokenId;
  String teamId;
  String deviceId;
  String timestamp;
};

// ─── Queue Entry (for batch upload) ───────────────────────────────
struct QueueEntry {
  String tokenId;
  String teamId;
  String deviceId;
  String timestamp;
};

// ─── Token Metadata (loaded from tokens.json) ────────────────────
struct TokenMetadata {
  String tokenId;
  String video;           // null if not video token
  String image;           // path to image file
  String audio;           // path to audio file
  String processingImage; // shown during "Sending..." modal for video tokens
};

// Simple array for token metadata (max 50 tokens)
#define MAX_TOKENS 50
TokenMetadata tokenDatabase[MAX_TOKENS];
int tokenDatabaseSize = 0;

// ─── Forward Declarations for Orchestrator Functions ─────────────
void parseConfigFile();
bool validateConfig();
String generateDeviceId();
void saveDeviceId(String devId);
bool syncTokenDatabase();
void initWiFiConnection();
void WiFiEventHandler(WiFiEvent_t event);
void backgroundTask(void* parameter);
bool checkOrchestratorHealth();
bool sendScan(String tokenId, String teamId, String deviceId, String timestamp);
void queueScan(String tokenId, String teamId, String deviceId, String timestamp);
int countQueueEntries();
void readQueue(std::vector<QueueEntry>& entries, int maxEntries);
void removeUploadedEntries(int numEntries);
bool uploadQueueBatch();
void loadTokenDatabase();
TokenMetadata* getTokenMetadata(String tokenId);
String generateTimestamp();

// Phase 5 video token helpers
bool hasVideoField(TokenMetadata* metadata);
String getProcessingImagePath(TokenMetadata* metadata, String tokenId);
void displayProcessingImage(String imagePath);

// Phase 7 queue overflow helpers
void handleQueueOverflow();
void displayQueueFullWarning();

// Phase 8 status screen helpers
void displayStatusScreen();
void dismissStatusScreen();
void drawReadyScreen(); // Helper to redraw ready screen

// Connection state helpers
void setConnectionState(ConnectionState newState);
ConnectionState getConnectionState();
void updateQueueSize(int delta);
int getQueueSize();

// SD mutex helpers
bool sdTakeMutex(const char* caller, unsigned long timeoutMs = 500);
void sdGiveMutex(const char* caller);

// ══════════════════════════════════════════════════════════════════

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
    String s = "/images/";
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
        return "/images/" + cleanText + ".bmp";
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
    touchInterruptTime = micros();  // Record timestamp for pulse width measurement
}

// ─── Touch Pulse Width Measurement (WiFi EMI Filter) ─────────────
// Measures how long GPIO36 stays LOW to distinguish WiFi EMI from real touches
// Validated Oct 19, 2025: WiFi EMI <0.01ms, Real touches >70ms
uint32_t measureTouchPulseWidth() {
    uint32_t startUs = micros();
    const uint32_t TIMEOUT_US = 500000;  // 500ms max

    // Poll GPIO36 until it goes HIGH or timeout
    while (digitalRead(TOUCH_IRQ) == LOW) {
        if (micros() - startUs > TIMEOUT_US) {
            return TIMEOUT_US;  // Still LOW after timeout
        }
    }

    return micros() - startUs;
}

// ══════════════════════════════════════════════════════════════════
// ═══ ORCHESTRATOR HELPER FUNCTIONS (v4.1) ════════════════════════
// ══════════════════════════════════════════════════════════════════

// ─── Connection State Helpers ────────────────────────────────────
void setConnectionState(ConnectionState newState) {
  portENTER_CRITICAL(&connStateMux);
  connState = newState;
  portEXIT_CRITICAL(&connStateMux);
}

ConnectionState getConnectionState() {
  portENTER_CRITICAL(&connStateMux);
  ConnectionState state = connState;
  portEXIT_CRITICAL(&connStateMux);
  return state;
}

void updateQueueSize(int delta) {
  portENTER_CRITICAL(&queueSizeMux);
  queueSizeCached += delta;
  portEXIT_CRITICAL(&queueSizeMux);
}

int getQueueSize() {
  portENTER_CRITICAL(&queueSizeMux);
  int size = queueSizeCached;
  portEXIT_CRITICAL(&queueSizeMux);
  return size;
}

// ─── SD Mutex Helpers ─────────────────────────────────────────────
bool sdTakeMutex(const char* caller, unsigned long timeoutMs) {
  if (!sdMutex) return true; // Mutex not initialized yet (boot phase)

  bool gotLock = xSemaphoreTake(sdMutex, timeoutMs / portTICK_PERIOD_MS) == pdTRUE;

  if (!gotLock) {
    Serial.printf("[MUTEX] %s timed out waiting for SD lock\n", caller);
  }

  return gotLock;
}

void sdGiveMutex(const char* caller) {
  if (sdMutex) {
    xSemaphoreGive(sdMutex);
  }
}

// ─── Utility Functions ────────────────────────────────────────────
String generateTimestamp() {
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  char timestamp[30];
  snprintf(timestamp, sizeof(timestamp),
    "1970-01-01T%02lu:%02lu:%02lu.%03luZ",
    hours % 24, minutes % 60, seconds % 60, ms % 1000);

  return String(timestamp);
}

// ─── Reset Reason Diagnostics ─────────────────────────────────────
void printResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("[BOOT] Reset reason: ");

  switch (reason) {
    case ESP_RST_UNKNOWN:     Serial.println("ESP_RST_UNKNOWN (indeterminate)"); break;
    case ESP_RST_POWERON:     Serial.println("ESP_RST_POWERON (normal power-on)"); break;
    case ESP_RST_EXT:         Serial.println("ESP_RST_EXT (external pin reset)"); break;
    case ESP_RST_SW:          Serial.println("ESP_RST_SW (software reset via esp_restart)"); break;
    case ESP_RST_PANIC:       Serial.println("ESP_RST_PANIC (exception/panic - CRASH!)"); break;
    case ESP_RST_INT_WDT:     Serial.println("ESP_RST_INT_WDT (interrupt watchdog - CODE HUNG!)"); break;
    case ESP_RST_TASK_WDT:    Serial.println("ESP_RST_TASK_WDT (task watchdog - TASK HUNG!)"); break;
    case ESP_RST_WDT:         Serial.println("ESP_RST_WDT (other watchdog - CODE HUNG!)"); break;
    case ESP_RST_DEEPSLEEP:   Serial.println("ESP_RST_DEEPSLEEP (wake from deep sleep)"); break;
    case ESP_RST_BROWNOUT:    Serial.println("ESP_RST_BROWNOUT (brownout reset - POWER ISSUE!)"); break;
    case ESP_RST_SDIO:        Serial.println("ESP_RST_SDIO (SDIO reset)"); break;
    default:                  Serial.printf("UNKNOWN (%d)\n", reason); break;
  }
}

String generateDeviceId() {
  // FIXED: Use esp_read_mac() per data-model.md specification
  // This works before WiFi initialization (reads from eFuse)
  Serial.printf("[DEVID] Free heap before generation: %d bytes\n", ESP.getFreeHeap());

  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);

  if (err != ESP_OK) {
    Serial.printf("[DEVID] ✗✗✗ FAILURE ✗✗✗ esp_read_mac() returned error: %d\n", err);
    return "SCANNER_ERROR";
  }

  Serial.printf("[DEVID] MAC bytes read: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // FIXED: Buffer was 20 bytes, but "SCANNER_" (8) + 12 hex digits + NULL = 21 bytes!
  char deviceId[32];  // Increased to 32 bytes for safety
  sprintf(deviceId, "SCANNER_%02X%02X%02X%02X%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf("[DEVID] Generated device ID: %s\n", deviceId);
  Serial.printf("[DEVID] Free heap after generation: %d bytes\n", ESP.getFreeHeap());

  return String(deviceId);
}

// ─── Configuration Parser ─────────────────────────────────────────
void parseConfigFile() {
  Serial.println("\n[CONFIG] ═══ CONFIG PARSING START ═══");
  Serial.printf("[CONFIG] Free heap: %d bytes\n", ESP.getFreeHeap());

  if (!sdTakeMutex("parseConfig", 1000)) {
    Serial.println("[CONFIG] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD lock");
    return;
  }

  File file = SD.open(CONFIG_FILE, FILE_READ);
  if (!file) {
    Serial.println("[CONFIG] ✗✗✗ FAILURE ✗✗✗ config.txt not found");
    sdGiveMutex("parseConfig");
    return;
  }

  Serial.println("[CONFIG] config.txt opened successfully");
  int lineNum = 0;
  int parsedKeys = 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    lineNum++;
    line.trim();

    if (line.length() == 0) {
      Serial.printf("[CONFIG] Line %d: (empty, skipped)\n", lineNum);
      continue;
    }
    
    // [PHASE 10] Skip comments
    if (line.startsWith("#") || line.startsWith(";")) {
      Serial.printf("[CONFIG] Line %d: (comment, skipped)\n", lineNum);
      continue;
    }

    int sepIndex = line.indexOf('=');
    if (sepIndex == -1) {
      Serial.printf("[CONFIG] Line %d: (no '=', skipped)\n", lineNum);
      continue;
    }

    String key = line.substring(0, sepIndex);
    String value = line.substring(sepIndex + 1);
    key.trim();
    value.trim();

    Serial.printf("[CONFIG] Line %d: '%s' = '%s'\n", lineNum, key.c_str(), value.c_str());

    if (key == "WIFI_SSID") { wifiSSID = value; parsedKeys++; }
    else if (key == "WIFI_PASSWORD") { wifiPassword = value; parsedKeys++; }
    else if (key == "ORCHESTRATOR_URL") { orchestratorURL = value; parsedKeys++; }
    else if (key == "TEAM_ID") { teamID = value; parsedKeys++; }
    else if (key == "DEVICE_ID") { deviceID = value; parsedKeys++; }
    // [PHASE 9] Add toggle for token sync
    else if (key == "SYNC_TOKENS") {
      if (value.equalsIgnoreCase("false") || value == "0") {
        syncTokens = false;
        Serial.println("[CONFIG]       SYNC_TOKENS set to FALSE");
      } else {
        syncTokens = true;
        Serial.println("[CONFIG]       SYNC_TOKENS set to TRUE");
      }
      parsedKeys++;
    }
    // [DEBUG_MODE] Add toggle for debug mode
    else if (key == "DEBUG_MODE") {
      if (value.equalsIgnoreCase("false") || value == "0") {
        debugMode = false;
        Serial.println("[CONFIG]       DEBUG_MODE set to FALSE (production)");
      } else {
        debugMode = true;
        Serial.println("[CONFIG]       DEBUG_MODE set to TRUE (debug)");
      }
      parsedKeys++;
    }
    else {
      Serial.printf("[CONFIG]         (unknown key, ignored)\n");
    }
  }

  file.close();
  sdGiveMutex("parseConfig");

  Serial.printf("[CONFIG] Parsed %d lines, %d recognized keys\n", lineNum, parsedKeys);
  Serial.println("[CONFIG] Results:");
  Serial.printf("  WIFI_SSID: %s\n", wifiSSID.length() > 0 ? wifiSSID.c_str() : "(not set)");
  Serial.printf("  WIFI_PASSWORD: %s\n", wifiPassword.length() > 0 ? "***" : "(not set)");
  Serial.printf("  ORCHESTRATOR_URL: %s\n", orchestratorURL.length() > 0 ? orchestratorURL.c_str() : "(not set)");
  Serial.printf("  TEAM_ID: %s\n", teamID.length() > 0 ? teamID.c_str() : "(not set)");
  Serial.printf("  DEVICE_ID: %s\n", deviceID.length() > 0 ? deviceID.c_str() : "(auto-generate)");
  Serial.printf("  SYNC_TOKENS: %s\n", syncTokens ? "true" : "false"); // [PHASE 9]
  Serial.printf("  DEBUG_MODE: %s\n", debugMode ? "true" : "false"); // [DEBUG_MODE]
  Serial.printf("[CONFIG] Free heap after parsing: %d bytes\n", ESP.getFreeHeap());

  // Validate required fields
  bool configValid = true;
  if (wifiSSID.length() == 0) {
    Serial.println("[CONFIG] ✗ WIFI_SSID is required");
    configValid = false;
  }
  if (orchestratorURL.length() == 0) {
    Serial.println("[CONFIG] ✗ ORCHESTRATOR_URL is required");
    configValid = false;
  }
  if (teamID.length() == 0) {
    Serial.println("[CONFIG] ✗ TEAM_ID is required");
    configValid = false;
  }

  if (configValid) {
    Serial.println("[CONFIG] ✓✓✓ SUCCESS ✓✓✓ All required fields present");
  } else {
    Serial.println("[CONFIG] ✗✗✗ FAILURE ✗✗✗ Missing required fields");
  }

  Serial.println("[CONFIG] ═══ CONFIG PARSING END ═══\n");
}

// ─── Configuration Validation ─────────────────────────────────────
bool validateConfig() {
  Serial.println("\n[VALIDATE] ═══ CONFIG VALIDATION START ═══");
  bool isValid = true;

  // Validate WIFI_SSID (required, 1-32 characters)
  if (wifiSSID.length() == 0) {
    Serial.println("[VALIDATE] ✗ WIFI_SSID is required");
    isValid = false;
  } else if (wifiSSID.length() > 32) {
    Serial.println("[VALIDATE] ✗ WIFI_SSID too long (max 32 characters)");
    isValid = false;
  } else {
    Serial.printf("[VALIDATE] ✓ WIFI_SSID valid: %s\n", wifiSSID.c_str());
  }

  // Validate WIFI_PASSWORD (required, 0-63 characters - can be empty for open networks)
  if (wifiPassword.length() > 63) {
    Serial.println("[VALIDATE] ✗ WIFI_PASSWORD too long (max 63 characters)");
    isValid = false;
  } else {
    Serial.println("[VALIDATE] ✓ WIFI_PASSWORD valid");
  }

  // Validate ORCHESTRATOR_URL (required, starts with http://, 10-200 characters)
  if (orchestratorURL.length() == 0) {
    Serial.println("[VALIDATE] ✗ ORCHESTRATOR_URL is required");
    isValid = false;
  } else if (!orchestratorURL.startsWith("http://")) {
    Serial.println("[VALIDATE] ✗ ORCHESTRATOR_URL must start with http://");
    isValid = false;
  } else if (orchestratorURL.length() < 10 || orchestratorURL.length() > 200) {
    Serial.println("[VALIDATE] ✗ ORCHESTRATOR_URL invalid length (10-200 characters)");
    isValid = false;
  } else {
    Serial.printf("[VALIDATE] ✓ ORCHESTRATOR_URL valid: %s\n", orchestratorURL.c_str());
  }

  // Validate TEAM_ID (required, exactly 3 digits)
  if (teamID.length() == 0) {
    Serial.println("[VALIDATE] ✗ TEAM_ID is required");
    isValid = false;
  } else if (teamID.length() != 3) {
    Serial.println("[VALIDATE] ✗ TEAM_ID must be exactly 3 digits");
    isValid = false;
  } else {
    // Check all characters are digits
    bool allDigits = true;
    for (int i = 0; i < 3; i++) {
      if (!isDigit(teamID[i])) {
        allDigits = false;
        break;
      }
    }
    if (!allDigits) {
      Serial.println("[VALIDATE] ✗ TEAM_ID must contain only digits");
      isValid = false;
    } else {
      Serial.printf("[VALIDATE] ✓ TEAM_ID valid: %s\n", teamID.c_str());
    }
  }

  // Validate DEVICE_ID (optional, 1-100 characters, alphanumeric + underscore)
  if (deviceID.length() > 0) {
    if (deviceID.length() > 100) {
      Serial.println("[VALIDATE] ✗ DEVICE_ID too long (max 100 characters)");
      isValid = false;
    } else {
      // Check all characters are alphanumeric or underscore
      bool validChars = true;
      for (unsigned int i = 0; i < deviceID.length(); i++) {
        char c = deviceID[i];
        if (!isAlphaNumeric(c) && c != '_') {
          validChars = false;
          break;
        }
      }
      if (!validChars) {
        Serial.println("[VALIDATE] ✗ DEVICE_ID must contain only letters, numbers, and underscores");
        isValid = false;
      } else {
        Serial.printf("[VALIDATE] ✓ DEVICE_ID valid: %s\n", deviceID.c_str());
      }
    }
  }
  
  // [PHASE 9] Validate SYNC_TOKENS (already parsed as bool, no validation needed)
  Serial.printf("[VALIDATE] ✓ SYNC_TOKENS valid: %s\n", syncTokens ? "true" : "false");

  // [DEBUG_MODE] Validate DEBUG_MODE (already parsed as bool, no validation needed)
  Serial.printf("[VALIDATE] ✓ DEBUG_MODE valid: %s\n", debugMode ? "true" : "false");

  if (isValid) {
    Serial.println("[VALIDATE] ✓✓✓ SUCCESS ✓✓✓ All fields valid");
  } else {
    Serial.println("[VALIDATE] ✗✗✗ FAILURE ✗✗✗ Configuration has errors");
  }

  Serial.println("[VALIDATE] ═══ CONFIG VALIDATION END ═══\n");
  return isValid;
}

// ─── Device ID Persistence ────────────────────────────────────────
void saveDeviceId(String devId) {
  Serial.println("\n[DEVID-SAVE] ═══ SAVE DEVICE ID START ═══");
  Serial.printf("[DEVID-SAVE] Saving device ID: %s\n", devId.c_str());

  if (!sdTakeMutex("saveDevId", 1000)) {
    Serial.println("[DEVID-SAVE] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD lock");
    return;
  }

  File file = SD.open(DEVICE_ID_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("[DEVID-SAVE] ✗✗✗ FAILURE ✗✗✗ Could not open device_id.txt for writing");
    sdGiveMutex("saveDevId");
    return;
  }

  file.println(devId);
  file.flush();
  file.close();
  sdGiveMutex("saveDevId");

  Serial.println("[DEVID-SAVE] ✓✓✓ SUCCESS ✓✓✓ Device ID saved to /device_id.txt");
  Serial.println("[DEVID-SAVE] ═══ SAVE DEVICE ID END ═══\n");
}

// ─── Token Database Synchronization ───────────────────────────────
bool syncTokenDatabase() {
  Serial.println("\n[TOKEN-SYNC] ═══ TOKEN DATABASE SYNC START ═══");
  Serial.printf("[TOKEN-SYNC] Free heap: %d bytes\n", ESP.getFreeHeap());

  if (orchestratorURL.length() == 0) {
    Serial.println("[TOKEN-SYNC] ✗ Orchestrator URL not configured");
    return false;
  }

  HTTPClient http;
  String url = orchestratorURL + "/api/tokens";
  Serial.printf("[TOKEN-SYNC] Fetching from: %s\n", url.c_str());

  http.begin(url);
  http.setTimeout(5000);  // 5-second timeout

  int httpCode = http.GET();

  if (httpCode == 200) {
    
    // [PHASE 9] Check file size before reading to RAM
    int size = http.getSize();
    Serial.printf("[TOKEN-SYNC] Received HTTP 200, size: %d bytes\n", size);
    if (size > MAX_TOKEN_DB_SIZE) {
        Serial.printf("[TOKEN-SYNC] ✗✗✗ FAILURE ✗✗✗ Token DB too large (%d > %d bytes)\n", size, MAX_TOKEN_DB_SIZE);
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();

    Serial.printf("[TOKEN-SYNC] Payload read (%d bytes)\n", payload.length());

    // Save to SD card
    if (!sdTakeMutex("syncTokenDB", 1000)) {
      Serial.println("[TOKEN-SYNC] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD lock");
      return false;
    }

    File file = SD.open(TOKEN_DB_FILE, FILE_WRITE);
    if (!file) {
      Serial.println("[TOKEN-SYNC] ✗✗✗ FAILURE ✗✗✗ Could not open tokens.json for writing");
      sdGiveMutex("syncTokenDB");
      return false;
    }

    file.print(payload);
    file.flush();
    file.close();
    sdGiveMutex("syncTokenDB");

    Serial.println("[TOKEN-SYNC] ✓✓✓ SUCCESS ✓✓✓ Token database synced and saved");
    Serial.printf("[TOKEN-SYNC] Free heap after sync: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[TOKEN-SYNC] ═══ TOKEN DATABASE SYNC END ═══\n");
    return true;

  } else {
    Serial.printf("[TOKEN-SYNC] ✗✗✗ FAILURE ✗✗✗ HTTP request failed (code: %d)\n", httpCode);
    http.end();
    Serial.println("[TOKEN-SYNC] ═══ TOKEN DATABASE SYNC END ═══\n");
    return false;
  }
}

// ─── Network Operations ───────────────────────────────────────────
bool checkOrchestratorHealth() {
  if (orchestratorURL.length() == 0) return false;

  HTTPClient http;
  http.begin(orchestratorURL + "/health");
  http.setTimeout(5000);

  int httpCode = http.GET();
  http.end();

  return (httpCode == 200);
}

bool sendScan(String tokenId, String teamId, String deviceId, String timestamp) {
  Serial.println("\n[SEND] ═══ HTTP POST /api/scan START ═══");
  Serial.printf("[SEND] Free heap: %d bytes\n", ESP.getFreeHeap());
  unsigned long startMs = millis();

  if (orchestratorURL.length() == 0) {
    Serial.println("[SEND] ✗✗✗ FAILURE ✗✗✗ No orchestrator URL configured");
    Serial.println("[SEND] ═══ HTTP POST /api/scan END ═══\n");
    return false;
  }

  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) doc["teamId"] = teamId;
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  String requestBody;
  serializeJson(doc, requestBody);

  Serial.printf("[SEND] URL: %s/api/scan\n", orchestratorURL.c_str());
  Serial.printf("[SEND] Payload: %s\n", requestBody.c_str());
  Serial.printf("[SEND] Payload size: %d bytes\n", requestBody.length());

  HTTPClient http;
  http.begin(orchestratorURL + "/api/scan");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  Serial.println("[SEND] Sending HTTP POST request...");
  int httpCode = http.POST(requestBody);
  unsigned long latencyMs = millis() - startMs;

  Serial.printf("[SEND] HTTP response code: %d\n", httpCode);
  Serial.printf("[SEND] Request latency: %lu ms\n", latencyMs);

  // Read response body before closing connection
  String responseBody = "";
  if (httpCode > 0) {
    responseBody = http.getString();
    if (responseBody.length() > 0) {
      Serial.printf("[SEND] Response body: %s\n", responseBody.c_str());
    }
  } else {
    Serial.printf("[SEND] ✗ HTTP Error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  // Accept 2xx success codes AND 409 Conflict (orchestrator received scan, handles duplicate)
  bool success = (httpCode >= 200 && httpCode < 300) || (httpCode == 409);

  if (success) {
    if (httpCode == 409) {
      Serial.println("[SEND] ✓ 409 Conflict - orchestrator received scan (duplicate handled)");
    } else {
      Serial.printf("[SEND] ✓✓✓ SUCCESS ✓✓✓ HTTP %d received\n", httpCode);
    }
  } else {
    Serial.printf("[SEND] ✗✗✗ FAILURE ✗✗✗ HTTP %d (will queue)\n", httpCode);
  }

  Serial.printf("[SEND] Free heap after send: %d bytes\n", ESP.getFreeHeap());
  Serial.println("[SEND] ═══ HTTP POST /api/scan END ═══\n");

  return success;
}

/**
 * Upload queue batch to orchestrator (Phase 6 - US4)
 * Reads up to 10 entries, sends via POST /api/scan/batch
 * Recursively uploads remaining batches with 1s delay
 *
 * @return true if all uploads successful, false on error
 */
bool uploadQueueBatch() {
  Serial.println("\n[BATCH] ═══════════════════════════════════");
  Serial.println("[BATCH]   QUEUE BATCH UPLOAD START");
  Serial.println("[BATCH] ═══════════════════════════════════");
  Serial.printf("[BATCH] Free heap before: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[BATCH] Current queue size: %d entries\n", getQueueSize());

  // Acquire SD mutex to read queue
  if (!sdTakeMutex("uploadBatch", 1000)) {
    Serial.println("[BATCH] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD mutex");
    Serial.println("[BATCH] ═══════════════════════════════════\n");
    return false;
  }

  // Read batch of up to 10 entries
  std::vector<QueueEntry> entries;
  readQueue(entries, 10);

  sdGiveMutex("uploadBatch"); // Release mutex *before* network I/O

  if (entries.empty()) {
    Serial.println("[BATCH] Queue is empty, nothing to upload");
    Serial.println("[BATCH] ═══════════════════════════════════\n");
    return true;
  }

  Serial.printf("[BATCH] Uploading batch of %zu entries\n", entries.size());

  // Create batch request JSON
  JsonDocument doc;
  JsonArray transactions = doc["transactions"].to<JsonArray>();

  for (QueueEntry& entry : entries) {
    JsonObject transaction = transactions.add<JsonObject>();
    transaction["tokenId"] = entry.tokenId;
    if (entry.teamId.length() > 0) transaction["teamId"] = entry.teamId;
    transaction["deviceId"] = entry.deviceId;
    transaction["timestamp"] = entry.timestamp;
  }

  String requestBody;
  serializeJson(doc, requestBody);

  Serial.printf("[BATCH] Request body size: %d bytes\n", requestBody.length());

  // HTTP POST to /api/scan/batch
  HTTPClient http;
  http.begin(orchestratorURL + "/api/scan/batch");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  unsigned long startTime = millis();
  int httpCode = http.POST(requestBody);
  unsigned long latency = millis() - startTime;

  // Read response body before closing
  String responseBody = "";
  if (httpCode > 0) {
    responseBody = http.getString();
    if (responseBody.length() > 0 && responseBody.length() < 200) {
      Serial.printf("[BATCH] Response: %s\n", responseBody.c_str());
    }
  } else {
    Serial.printf("[BATCH] ✗ HTTP Error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  Serial.printf("[BATCH] HTTP %d response, latency: %lu ms\n", httpCode, latency);

  if (httpCode == 200) {
    Serial.println("[BATCH] ✓✓✓ SUCCESS ✓✓✓ Batch upload successful");

    // Remove uploaded entries from queue
    // [PHASE 6 FIX] This function now handles its own mutex
    removeUploadedEntries(entries.size());

    // Check if more entries remain
    int remainingSize = getQueueSize();
    Serial.printf("[BATCH] Queue size after upload: %d entries\n", remainingSize);

    if (remainingSize > 0) {
      Serial.println("[BATCH] More entries in queue, uploading next batch after 1s delay");
      Serial.println("[BATCH] ═══════════════════════════════════\n");
      delay(1000);  // 1-second delay between batches
      return uploadQueueBatch(); // Recursive upload
    }

    Serial.printf("[BATCH] Free heap after: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[BATCH] ✓ All queue entries uploaded");
    Serial.println("[BATCH] ═══════════════════════════════════\n");
    return true;

  } else {
    Serial.printf("[BATCH] ✗✗✗ FAILURE ✗✗✗ HTTP %d\n", httpCode);
    Serial.println("[BATCH] Entries remain in queue for retry on next health check");
    Serial.println("[BATCH] ═══════════════════════════════════\n");
    return false;
  }
}

// ─── Queue Operations ─────────────────────────────────────────────

// [PHASE 7] US5 - Handle Queue Overflow
void handleQueueOverflow() {
  Serial.println("\n[QUEUE-OVFLW] ═══ QUEUE OVERFLOW ═══");
  Serial.printf("[QUEUE-OVFLW] Queue is FULL (%d entries)\n", getQueueSize());
  Serial.println("[QUEUE-OVFLW] Removing oldest entry (FIFO)...");
  
  // [PHASE 6 FIX] This function now handles its own mutex
  // This will block if the background task is also removing
  removeUploadedEntries(1); 
  
  Serial.println("[QUEUE-OVFLW] ✓ Oldest entry removed");
  Serial.println("[QUEUE-OVFLW] ═══ OVERFLOW COMPLETE ═══\n");
  
  // [PHASE 7] Display non-blocking warning on TFT
  displayQueueFullWarning();
}

void displayQueueFullWarning() {
    Serial.println("[TFT-WARN] Displaying 'Queue Full!' warning");

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(40, 200); // Bottom of screen
    tft.println("Queue Full!");

    delay(500); // Show for 500ms (as per spec)

    // Redraw ready screen to clear warning
    // [PHASE 8] Check if status screen is active
    if (statusScreenDisplayed) {
        displayStatusScreen(); // Redraw status screen
    } else if (!imageIsDisplayed) {
        drawReadyScreen(); // Redraw ready screen
    }
    // If imageIsDisplayed, warning will be overwritten anyway
}


void queueScan(String tokenId, String teamId, String deviceId, String timestamp) {
  Serial.println("\n[QUEUE] ═══ QUEUE SCAN START ═══");
  Serial.printf("[QUEUE] Free heap: %d bytes\n", ESP.getFreeHeap());
  unsigned long startMs = millis();

  // [PHASE 7] US5 - Check for queue overflow BEFORE acquiring mutex
  if (getQueueSize() >= MAX_QUEUE_SIZE) {
    handleQueueOverflow();
  }

  if (!sdTakeMutex("queueScan", 500)) {
    Serial.println("[QUEUE] ✗✗✗ FAILURE ✗✗✗ Failed to acquire SD mutex (timeout)");
    Serial.println("[QUEUE] ═══ QUEUE SCAN END ═══\n");
    return;
  }

  // Create JSON
  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) doc["teamId"] = teamId;
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  String jsonLine;
  serializeJson(doc, jsonLine);

  Serial.printf("[QUEUE] Entry: %s\n", jsonLine.c_str());
  Serial.printf("[QUEUE] Entry size: %d bytes\n", jsonLine.length());

  // Append to file
  Serial.printf("[QUEUE] Opening %s for append...\n", QUEUE_FILE);
  File file = SD.open(QUEUE_FILE, FILE_APPEND);

  if (file) {
    unsigned long fileSize = file.size();
    Serial.printf("[QUEUE] File opened, current size: %lu bytes\n", fileSize);

    file.println(jsonLine);
    file.flush();
    file.close();

    updateQueueSize(1);
    int newQueueSize = getQueueSize();
    unsigned long latencyMs = millis() - startMs;

    Serial.printf("[QUEUE] ✓✓✓ SUCCESS ✓✓✓ Scan queued (token: %s)\n", tokenId.c_str());
    Serial.printf("[QUEUE] Queue size: %d entries\n", newQueueSize);
    Serial.printf("[QUEUE] Write latency: %lu ms\n", latencyMs);
  } else {
    Serial.printf("[QUEUE] ✗✗✗ FAILURE ✗✗✗ Could not open %s for writing\n", QUEUE_FILE);
  }

  sdGiveMutex("queueScan");

  Serial.printf("[QUEUE] Free heap after queue: %d bytes\n", ESP.getFreeHeap());
  Serial.println("[QUEUE] ═══ QUEUE SCAN END ═══\n");
}

int countQueueEntries() {
  // Must be called with SD mutex held
  File file = SD.open(QUEUE_FILE, FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) count++;
  }

  file.close();
  return count;
}

/**
 * Read queue entries from JSONL file (Phase 6 - US4)
 * Must be called with SD mutex already acquired
 *
 * @param entries Vector to populate with queue entries
 * @param maxEntries Maximum number of entries to read (default 10 for batch upload)
 */
void readQueue(std::vector<QueueEntry>& entries, int maxEntries) {
  Serial.println("\n[QUEUE-READ] ═══ READING QUEUE BATCH ═══");
  Serial.printf("[QUEUE-READ] Max entries: %d\n", maxEntries);

  File file = SD.open(QUEUE_FILE, FILE_READ);
  if (!file) {
    Serial.println("[QUEUE-READ] ✗ Queue file not found");
    return;
  }

  int count = 0;
  int skipped = 0;

  while (file.available() && count < maxEntries) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, line);

    if (error == DeserializationError::Ok) {
      // Validate required fields
      if (doc.containsKey("tokenId") && doc.containsKey("deviceId") && doc.containsKey("timestamp")) {
        QueueEntry entry;
        entry.tokenId = doc["tokenId"].as<String>();
        entry.teamId = doc.containsKey("teamId") ? doc["teamId"].as<String>() : "";
        entry.deviceId = doc["deviceId"].as<String>();
        entry.timestamp = doc["timestamp"].as<String>();
        entries.push_back(entry);
        count++;

        Serial.printf("[QUEUE-READ] Entry %d: tokenId=%s\n", count, entry.tokenId.c_str());
      } else {
        skipped++;
        Serial.printf("[QUEUE-READ] ✗ Skipped line (missing fields): %s\n", line.c_str());
      }
    } else {
      skipped++;
      Serial.printf("[QUEUE-READ] ✗ Skipped corrupt line: %s\n", line.c_str());
    }
  }

  file.close();

  Serial.printf("[QUEUE-READ] ✓ Read %d entries, skipped %d corrupt\n", count, skipped);
  Serial.println("[QUEUE-READ] ═══ READING COMPLETE ═══\n");
}

/**
 * [PHASE 6 FIX] REWRITE: Remove first N entries from queue (FIFO)
 * Uses a safe, stream-based approach with a temp file
 * This prevents RAM spikes by NOT loading the whole queue into memory
 * This function now handles its own mutex and is safe to call from any task
 *
 * @param numEntries Number of entries to remove from front of queue
 */
void removeUploadedEntries(int numEntries) {
  Serial.println("\n[QUEUE-REBUILD] ═══ REMOVING UPLOADED ENTRIES ═══");
  Serial.printf("[QUEUE-REBUILD] Entries to remove: %d\n", numEntries);
  Serial.printf("[QUEUE-REBUILD] Free heap before: %d bytes\n", ESP.getFreeHeap());
  unsigned long startMs = millis();

  if (numEntries <= 0) {
      Serial.println("[QUEUE-REBUILD] ✗ No entries to remove");
      return;
  }

  if (!sdTakeMutex("removeEntries", 2000)) {
      Serial.println("[QUEUE-REBUILD] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD mutex");
      return;
  }

  File f = SD.open(QUEUE_FILE, FILE_READ);
  if (!f) {
      Serial.println("[QUEUE-REBUILD] ✗ Queue file not found, nothing to remove");
      sdGiveMutex("removeEntries");
      return;
  }
  
  File tempFile = SD.open(QUEUE_TEMP_FILE, FILE_WRITE);
  if (!tempFile) {
      Serial.println("[QUEUE-REBUILD] ✗✗✗ FAILURE ✗✗✗ Could not open temp file for writing");
      f.close();
      sdGiveMutex("removeEntries");
      return;
  }

  Serial.println("[QUEUE-REBUILD] Starting stream-based rebuild...");

  int linesRead = 0;
  int linesSkipped = 0;
  int linesKept = 0;
  
  // Stream-read original, stream-write to temp
  while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      linesRead++;
      
      if (line.length() == 0) continue; // Skip empty lines

      if (linesSkipped < numEntries) {
          // Skip this line
          linesSkipped++;
      } else {
          // Keep this line
          tempFile.println(line);
          linesKept++;
      }
  }
  
  f.close();
  tempFile.flush();
  tempFile.close();
  
  Serial.printf("[QUEUE-REBUILD] Read: %d lines, Skipped: %d, Kept: %d\n", linesRead, linesSkipped, linesKept);

  // Atomic file replacement
  Serial.println("[QUEUE-REBUILD] Replacing original queue file...");
  if (!SD.remove(QUEUE_FILE)) {
      Serial.println("[QUEUE-REBUILD] ✗✗✗ FAILURE ✗✗✗ Could not remove original queue file");
      sdGiveMutex("removeEntries");
      return;
  }

  if (!SD.rename(QUEUE_TEMP_FILE, QUEUE_FILE)) {
      Serial.println("[QUEUE-REBUILD] ✗✗✗ FAILURE ✗✗✗ Could not rename temp file");
      sdGiveMutex("removeEntries");
      return;
  }

  // Update cache
  updateQueueSize(-linesSkipped);
  
  unsigned long latencyMs = millis() - startMs;
  Serial.printf("[QUEUE-REBUILD] ✓✓✓ SUCCESS ✓✓✓ Queue rebuilt in %lu ms\n", latencyMs);
  Serial.printf("[QUEUE-REBUILD] New queue size: %d\n", getQueueSize());
  Serial.printf("[QUEUE-REBUILD] Free heap after: %d bytes\n", ESP.getFreeHeap());
  
  sdGiveMutex("removeEntries");
  Serial.println("[QUEUE-REBUILD] ═══ REMOVAL COMPLETE ═══\n");
}


// ─── Token Database ───────────────────────────────────────────────
void loadTokenDatabase() {
  if (!sdTakeMutex("loadTokenDB", 1000)) {
    Serial.println("[TOKEN-DB] Could not acquire SD mutex");
    return;
  }

  File file = SD.open(TOKEN_DB_FILE, FILE_READ);
  if (!file) {
    Serial.println("[TOKEN-DB] tokens.json not found");
    sdGiveMutex("loadTokenDB");
    return;
  }

  // Parse JSON (simplified - load into array)
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  sdGiveMutex("loadTokenDB");

  if (error) {
    Serial.printf("[TOKEN-DB] Parse error: %s\n", error.c_str());
    return;
  }

  // Extract tokens
  JsonObject tokens = doc["tokens"];
  tokenDatabaseSize = 0;

  for (JsonPair kv : tokens) {
    if (tokenDatabaseSize >= MAX_TOKENS) break;

    TokenMetadata &token = tokenDatabase[tokenDatabaseSize];
    token.tokenId = String(kv.key().c_str());

    JsonObject tokenData = kv.value();
    token.video = tokenData["video"] | "";
    token.image = tokenData["image"] | "";
    token.audio = tokenData["audio"] | "";
    token.processingImage = tokenData["processingImage"] | "";

    tokenDatabaseSize++;
  }

  Serial.printf("[TOKEN-DB] Loaded %d tokens\n", tokenDatabaseSize);
}

TokenMetadata* getTokenMetadata(String tokenId) {
  for (int i = 0; i < tokenDatabaseSize; i++) {
    if (tokenDatabase[i].tokenId == tokenId) {
      return &tokenDatabase[i];
    }
  }
  return nullptr;
}

// ─── Video Token Detection Helpers (Phase 5) ─────────────────────

// T096: Check if token has video field (non-null, non-empty)
bool hasVideoField(TokenMetadata* metadata) {
  if (metadata == nullptr) return false;
  return (metadata->video.length() > 0 && metadata->video != "null");
}

// T097: Extract processing image path from metadata, convert to absolute SD path
String getProcessingImagePath(TokenMetadata* metadata, String tokenId) {
  if (metadata == nullptr) return "";
  if (metadata->processingImage.length() == 0) return "";
  if (metadata->processingImage == "null") return "";

  // Extract file extension from processingImage field
  // (ignore any path components - just get the extension)
  String extension = "";
  int lastDot = metadata->processingImage.lastIndexOf('.');
  if (lastDot >= 0) {
    extension = metadata->processingImage.substring(lastDot); // e.g., ".jpg", ".png", ".bmp"
  } else {
    extension = ".bmp"; // Default extension matches regular images
  }

  // Construct path from tokenId (same pattern as regular images)
  // Regular images: /images/{tokenId}.bmp
  // Processing images: /images/{tokenId}{extension}
  return "/images/" + tokenId + extension;
}

// T099-T102: Display processing image with "Sending..." overlay and fallback
void displayProcessingImage(String imagePath) {
  Serial.println("\n[PROC_IMG] ═══ PROCESSING IMAGE DISPLAY START ═══");
  Serial.printf("[PROC_IMG] Image path: %s\n", imagePath.length() > 0 ? imagePath.c_str() : "(empty)");
  Serial.printf("[PROC_IMG] Free heap: %d bytes\n", ESP.getFreeHeap());

  bool imageLoaded = false;

  // T099: Load processing image from SD card (if path provided and file exists)
  if (imagePath.length() > 0) {
    Serial.printf("[PROC_IMG] Checking file existence: %s\n", imagePath.c_str());
    // [PHASE 8] Must acquire mutex to check/draw from SD
    if (sdTakeMutex("procImgDisplay", 1000)) {
        if (SD.exists(imagePath.c_str())) {
            Serial.printf("[PROC_IMG] Loading image: %s\n", imagePath.c_str());
            drawBmp(imagePath); // Reuse existing Constitution-compliant BMP display function
            imageLoaded = true;
            Serial.println("[PROC_IMG] ✓ Image loaded successfully");
        } else {
            Serial.printf("[PROC_IMG] ✗ Image file not found: %s\n", imagePath.c_str());
        }
        sdGiveMutex("procImgDisplay");
    } else {
        Serial.println("[PROC_IMG] ✗ Could not get SD mutex for image");
    }
  } else {
    Serial.println("[PROC_IMG] No processing image path provided");
  }

  // T101: Fallback display for missing processingImage
  if (!imageLoaded) {
    Serial.println("[PROC_IMG] Using fallback: text-only display");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(40, 100);
    tft.println("Sending...");
  }

  // T100: Add "Sending..." text overlay (regardless of image load success)
  // Display overlay at bottom of screen for visual feedback
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(40, 200); // Bottom of screen (240x320 display)
  tft.println("Sending...");
  Serial.println("[PROC_IMG] ✓ Overlay added: 'Sending...'");

  Serial.printf("[PROC_IMG] Free heap after display: %d bytes\n", ESP.getFreeHeap());

  // T102: Auto-hide modal after 2.5 seconds
  Serial.println("[PROC_IMG] Starting 2.5s auto-hide timer...");
  unsigned long timerStart = millis();
  delay(2500);
  unsigned long timerActual = millis() - timerStart;
  Serial.printf("[PROC_IMG] ✓ Timer complete (actual: %lu ms)\n", timerActual);

  Serial.println("[PROC_IMG] ═══ PROCESSING IMAGE DISPLAY END ═══\n");
}

// [PHASE 8] US6 - Status Screen Functions
void displayStatusScreen() {
    Serial.println("\n[PHASE8-DIAG] ═══ STATUS SCREEN DISPLAY ═══");
    Serial.printf("[PHASE8-DIAG] Free heap: %d\n", ESP.getFreeHeap());
    unsigned long startMs = millis();
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    
    // Title
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.println("--- DIAGNOSTICS ---");
    tft.println("");
    
    // WiFi Status (FR-039)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("WiFi: ");
    ConnectionState state = getConnectionState();
    if (state == ORCH_DISCONNECTED) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("DISCONNECTED");
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println(wifiSSID);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print("  IP: ");
        tft.println(WiFi.localIP().toString());
    }
    tft.println("");

    // Orchestrator Status (FR-040)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Orchestrator: ");
    if (state == ORCH_CONNECTED) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("CONNECTED");
    } else if (state == ORCH_WIFI_CONNECTED) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.println("OFFLINE");
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("OFFLINE");
    }
    tft.println("");

    // Queue Size (FR-041 & T138)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Queue: ");
    int queueSize = getQueueSize();
    if (queueSize >= MAX_QUEUE_SIZE) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.printf("%d (FULL)\n", queueSize);
    } else if (queueSize > 0) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.printf("%d scans\n", queueSize);
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("0 scans");
    }
    tft.println("");

    // Team ID (FR-042)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Team: ");
    tft.println(teamID);
    
    // Device ID (FR-043)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Device: ");
    tft.println(deviceID);
    tft.println("");
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.println("Tap again to close");

    unsigned long latencyMs = millis() - startMs;
    Serial.printf("[PHASE8-DIAG] ✓ Status screen rendered in %lu ms\n", latencyMs);
}

// [PHASE 8] US6 - Helper to dismiss status screen
void dismissStatusScreen() {
    Serial.println("\n[PHASE8-DIAG] ═══ STATUS SCREEN DISMISS ═══");
    drawReadyScreen();
    lastTouchTime = 0; // Prevent accidental double-tap
    lastTouchWasValid = false;
}

// [PHASE 8] US6 - Helper to draw the main "Ready" screen
void drawReadyScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("NeurAI");
    tft.println("Memory Scanner");
    tft.println("v4.1 Ready"); // Updated version
    tft.println("");

    // [DEBUG MODE FIX] Show different message based on RFID init state
    if (!rfidInitialized) {
        // RFID not initialized - show debug mode instructions
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("DEBUG MODE");
        tft.println("");
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.setTextSize(1);
        tft.println("RFID disabled due to");
        tft.println("GPIO 3 pin conflict");
        tft.println("(Serial RX vs RFID_SS)");
        tft.println("");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println("Send START_SCANNER");
        tft.println("via serial to enable");
        tft.println("RFID scanning");
        tft.setTextSize(2);
    } else {
        // RFID initialized - show ready to scan
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("READY TO SCAN");
    }

    tft.println("");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.println("Tap for Status"); // [PHASE 8] Hint
}


// ─── WiFi Initialization ──────────────────────────────────────────
void WiFiEventHandler(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("[%lu] [WIFI] Connected to AP\n", millis());
      Serial.printf("         SSID: %s, Channel: %d\n", WiFi.SSID().c_str(), WiFi.channel());
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[%lu] [WIFI] Got IP address: %s\n", millis(), WiFi.localIP().toString().c_str());
      Serial.printf("         Gateway: %s, Signal: %d dBm\n", WiFi.gatewayIP().toString().c_str(), WiFi.RSSI());
      setConnectionState(ORCH_WIFI_CONNECTED);
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[%lu] [WIFI] Disconnected from AP\n", millis());
      setConnectionState(ORCH_DISCONNECTED);
      // WiFi will auto-reconnect, don't call WiFi.reconnect() here to avoid storm
      break;

    default:
      break;
  }
}

void initWiFiConnection() {
  if (wifiSSID.length() == 0) {
    Serial.println("[WIFI] No SSID configured, skipping WiFi");
    return;
  }

  Serial.println("\n[WIFI] ═══ WIFI CONNECTION START ═══");

  WiFi.onEvent(WiFiEventHandler);
  WiFi.mode(WIFI_STA);
  Serial.printf("[WIFI] SSID: %s\n", wifiSSID.c_str());
  Serial.printf("[WIFI] Free heap before connection: %d bytes\n", ESP.getFreeHeap());

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  Serial.println("[WIFI] Waiting for connection (timeout: 10s)...");
  unsigned long startTime = millis();
  int dots = 0;

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
    dots++;
    if (dots % 20 == 0) Serial.println();  // New line every 10 seconds
  }
  Serial.println();  // End dots line

  unsigned long connectionTime = millis() - startTime;

  if (WiFi.status() == WL_CONNECTED) {
    setConnectionState(ORCH_WIFI_CONNECTED);
    Serial.printf("[WIFI] ✓✓✓ SUCCESS ✓✓✓ Connected in %lu ms\n", connectionTime);
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[WIFI] Signal Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[WIFI] Channel: %d\n", WiFi.channel());

    // Check orchestrator
    Serial.println("\n[ORCH] Checking orchestrator health...");
    Serial.printf("[ORCH] URL: %s/health\n", orchestratorURL.c_str());

    if (checkOrchestratorHealth()) {
      setConnectionState(ORCH_CONNECTED);
      Serial.println("[ORCH] ✓✓✓ SUCCESS ✓✓✓ Orchestrator reachable");
    } else {
      Serial.println("[ORCH] ✗✗✗ FAILURE ✗✗✗ Orchestrator offline");
    }
  } else {
    Serial.printf("[WIFI] ✗✗✗ FAILURE ✗✗✗ Connection timeout after %lu ms\n", connectionTime);
    setConnectionState(ORCH_DISCONNECTED);
  }

  Serial.printf("[WIFI] Free heap after connection: %d bytes\n", ESP.getFreeHeap());
  Serial.println("[WIFI] ═══ WIFI CONNECTION END ═══\n");
}

// ─── Background Task (Connection Monitor) ────────────────────────
void backgroundTask(void* parameter) {
  Serial.println("[BG-TASK] Background task started on Core 0");

  unsigned long lastCheck = 0;
  const unsigned long checkInterval = 10000; // 10 seconds

  while (true) {
    unsigned long now = millis();

    // Stack monitoring (Phase 6 - check for stack overflow risk)
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
    if (stackRemaining < 512) {
      Serial.printf("[BG-TASK] ⚠️ WARNING: Low stack! Only %d bytes free\n", stackRemaining);
    }

    if (now - lastCheck > checkInterval) {
      lastCheck = now;

      // Log stack health every 10 seconds
      Serial.printf("[BG-TASK] Stack high water mark: %d bytes free\n", stackRemaining);

      ConnectionState state = getConnectionState();

      if (state == ORCH_WIFI_CONNECTED || state == ORCH_CONNECTED) {
        // Check orchestrator health
        if (checkOrchestratorHealth()) {
          if (state != ORCH_CONNECTED) {
            Serial.println("[BG-TASK] Orchestrator now reachable");
            setConnectionState(ORCH_CONNECTED);
          }

          // Phase 6: Upload queue if not empty
          int queueSize = getQueueSize();
          if (queueSize > 0) {
            Serial.printf("[BG-TASK] Queue has %d entries, starting batch upload\n", queueSize);
            uploadQueueBatch();
          }
        } else {
          if (state == ORCH_CONNECTED) {
            Serial.println("[BG-TASK] Orchestrator unreachable");
            setConnectionState(ORCH_WIFI_CONNECTED);
          }
        }
      }
      // [PHASE 10] If completely disconnected, WiFi.begin() is already handling auto-reconnect
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // [PHASE 10] Use vTaskDelay for watchdog
  }
}

// ══════════════════════════════════════════════════════════════════

// ─── DEBUG MODE: Initialize RFID (Deferred to avoid GPIO 3 conflict) ─
/**
 * Initialize the RFID reader (MFRC522 on software SPI)
 *
 * ⚠️  WARNING: This function reconfigures GPIO 3 from Serial RX to OUTPUT
 *    After calling this, serial input commands will NO LONGER WORK!
 *
 * Call this ONLY when:
 *   - User sends START_SCANNER command via serial
 *   - You are ready to sacrifice serial debugging for RFID functionality
 *
 * @return void
 * @modifies rfidInitialized (sets to true)
 * @modifies GPIO 3 (reconfigures from RX to OUTPUT for RFID_SS)
 */
void initializeRFID() {
    // Prevent double initialization
    if (rfidInitialized) {
        Serial.println("[RFID-INIT] Already initialized, skipping");
        return;
    }

    Serial.println("[RFID-INIT] ════════════════════════════════════");
    Serial.println("[RFID-INIT]   INITIALIZING RFID READER");
    Serial.println("[RFID-INIT] ════════════════════════════════════");
    Serial.println("[RFID-INIT] ⚠️  WARNING: Serial RX will be disabled!");
    Serial.flush(); // Force send warning before breaking serial
    delay(100);

    // Configure Software SPI pins
    pinMode(SOFT_SPI_SCK, OUTPUT);
    pinMode(SOFT_SPI_MOSI, OUTPUT);
    pinMode(SOFT_SPI_MISO, INPUT);
    pinMode(RFID_SS, OUTPUT);  // ⚠️ THIS LINE KILLS SERIAL RX (GPIO 3)
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(SOFT_SPI_SCK, LOW);

    Serial.println("[RFID-INIT] ✗ Serial RX now disabled (GPIO 3 reassigned)");

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

    // Update TFT display
    tft.println("RFID OK");

    // Mark as initialized
    rfidInitialized = true;

    Serial.println("[RFID-INIT] ✓ RFID initialized successfully");
    Serial.println("[RFID-INIT] ════════════════════════════════════\n");
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

    // ─── Boot-time Debug Mode Override (30-second window) ─────────────
    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("     BOOT-TIME DEBUG MODE OVERRIDE");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("Send ANY character within 30 seconds to force DEBUG_MODE");
    Serial.println("(Allows serial commands even if config.txt has DEBUG_MODE=false)");
    Serial.println("");
    Serial.print("Waiting (30s): ");

    unsigned long overrideStart = millis();
    bool overrideReceived = false;
    int lastSecond = -1;

    while (millis() - overrideStart < 30000) { // 30-second window
        if (Serial.available()) {
            overrideReceived = true;
            char received = Serial.read();
            Serial.printf("\n\n✓ Override character received: '%c'\n", received);
            break;
        }

        // Print countdown every second
        int currentSecond = (millis() - overrideStart) / 1000;
        if (currentSecond != lastSecond) {
            lastSecond = currentSecond;
            Serial.printf("%d ", 30 - currentSecond);
            if ((30 - currentSecond) % 10 == 0 && currentSecond > 0) {
                Serial.println(); // New line every 10 seconds
                Serial.print("          ");
            }
        }
        delay(100);
    }

    Serial.println("");
    if (overrideReceived) {
        debugMode = true; // FORCE debug mode regardless of config.txt
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("   ✓✓✓ DEBUG MODE OVERRIDE ACTIVE ✓✓✓");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("DEBUG_MODE forced to TRUE (ignores config.txt)");
        Serial.println("Serial commands will work!");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    } else {
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("  No override received");
        Serial.println("  Using config.txt DEBUG_MODE setting");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }

    delay(1000); // Brief pause before continuing boot
    // ───────────────────────────────────────────────────────────────────

    Serial.println("\n━━━ CYD RFID Scanner v4.1 (Full Orchestrator) ━━━");
    Serial.println("Phases 6-10: Queue Sync, Overflow, Status Screen");

    // Print reset reason FIRST - critical for debugging crashes
    printResetReason();

    Serial.printf("[BOOT] Free heap at start: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] Chip model: ESP32, %d cores, %d MHz\n",
                  ESP.getChipCores(), ESP.getCpuFreqMHz());
    
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
    tft.println("v4.1 Booting...");
    
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

    // ═══ ORCHESTRATOR INTEGRATION (v4.1) ═══════════════════════════
    Serial.println("\n[ORCH] ════════════════════════════════════════");
    Serial.println("[ORCH]   ORCHESTRATOR INTEGRATION START");
    Serial.println("[ORCH] ════════════════════════════════════════");
    Serial.printf("[ORCH] Free heap at orchestrator start: %d bytes\n", ESP.getFreeHeap());

    if (sdCardPresent) {
        // Create SD mutex for orchestrator operations
        Serial.println("\n[ORCH] Creating SD mutex for orchestrator operations...");
        sdMutex = xSemaphoreCreateMutex();
        if (sdMutex) {
            Serial.println("[ORCH] ✓ SD mutex created successfully");
        } else {
            Serial.println("[ORCH] ✗✗✗ FATAL ✗✗✗ Failed to create SD mutex");
        }

        // Parse configuration file
        Serial.println("[ORCH] Reading config.txt...");
        parseConfigFile();

        // Validate configuration
        bool configValid = validateConfig();
        if (!configValid) {
            Serial.println("[ORCH] ✗✗✗ Configuration validation failed");
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("Config Error!");
            if (wifiSSID.length() == 0) tft.println("Missing WIFI_SSID");
            if (wifiSSID.length() > 32) tft.println("SSID too long");
            if (orchestratorURL.length() == 0) tft.println("Missing URL");
            if (!orchestratorURL.startsWith("http://")) tft.println("URL must be HTTP");
            if (teamID.length() == 0) tft.println("Missing TEAM_ID");
            if (teamID.length() != 3) tft.println("TEAM_ID: 3 digits");
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            // Continue in offline mode even with invalid config
        }

        // Generate device ID if not configured
        // NOTE: esp_read_mac() reads from eFuse, does NOT require WiFi initialization
        if (deviceID.length() == 0) {
            Serial.println("\n[DEVID] ═══ DEVICE ID GENERATION START ═══");
            deviceID = generateDeviceId();
            Serial.printf("[DEVID] Auto-generated device ID: %s\n", deviceID.c_str());

            // Persist generated device ID to SD card
            saveDeviceId(deviceID);

            Serial.println("[DEVID] ═══ DEVICE ID GENERATION END ═══\n");
        } else {
            Serial.printf("[DEVID] Using configured device ID: %s\n", deviceID.c_str());
        }

        // Initialize queue size cache
        if (sdTakeMutex("queueInit", 1000)) {
            queueSizeCached = countQueueEntries();
            sdGiveMutex("queueInit");
            Serial.printf("[ORCH] Queue size at boot: %d\n", queueSizeCached);
        }

        // Initialize WiFi if configured
        bool tokenSyncSuccess = false;
        if (wifiSSID.length() > 0) {
            tft.println("Connecting WiFi...");
            initWiFiConnection();

            // Display connection status
            ConnectionState state = getConnectionState();
            if (state == ORCH_CONNECTED) {
                tft.println("Connected!");

                // [PHASE 9] Check config before syncing
                if (syncTokens) {
                    tft.println("Syncing tokens...");
                    tokenSyncSuccess = syncTokenDatabase();

                    if (tokenSyncSuccess) {
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.println("Tokens: Synced");
                        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                    } else {
                        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                        tft.println("Tokens: Sync Failed");
                        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                    }
                } else {
                    Serial.println("[ORCH] Token sync disabled in config");
                    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                    tft.println("Tokens: Sync OFF");
                    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                }
            } else if (state == ORCH_WIFI_CONNECTED) {
                tft.println("WiFi OK / Orch Offline");
                tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                tft.println("Tokens: Using Cache");
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            } else {
                tft.println("WiFi Failed");
                tft.setTextColor(TFT_ORANGE, TFT_BLACK);
                tft.println("Tokens: Using Cache");
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            }
        } else {
            Serial.println("[ORCH] No WiFi configured, offline mode");
            tft.println("Offline Mode");
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
            tft.println("Tokens: Using Cache");
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        }

        // Load token database from SD card (either synced or cached)
        Serial.println("[ORCH] Loading token database...");
        loadTokenDatabase();

        // Display token database status on TFT
        if (tokenDatabaseSize > 0) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.printf("Loaded: %d tokens\n", tokenDatabaseSize);
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("No Token Database!");
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        }

        // Create background task on Core 0 (connection monitoring)
        if (wifiSSID.length() > 0) {
            xTaskCreatePinnedToCore(
                backgroundTask,
                "OrchestratorBG",
                16384,  // 16KB stack (proven in test-42)
                NULL,
                1,      // Priority
                NULL,
                0       // Core 0
            );
            Serial.println("[ORCH] Background task created on Core 0");
        }
    } else {
        Serial.println("\n[ORCH] No SD card, orchestrator features disabled");
        tft.println("No Orchestrator");
    }

    Serial.printf("[ORCH] Free heap at orchestrator end: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[ORCH] ════════════════════════════════════════");
    Serial.println("[ORCH]   ORCHESTRATOR INTEGRATION END");
    Serial.println("[ORCH] ════════════════════════════════════════\n");
    // ═══════════════════════════════════════════════════════════════

    // Configure Touch Interrupt
    // CRITICAL: GPIO36 is input-only, cannot use INPUT_PULLUP!
    pinMode(TOUCH_IRQ, INPUT);  // FIX: Use INPUT instead of INPUT_PULLUP
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ), touchISR, FALLING);
    Serial.println("Touch interrupt configured");

    // ─── RFID Initialization (Conditional based on DEBUG_MODE) ────────
    if (debugMode) {
        // Debug mode: defer RFID to allow serial commands
        Serial.println("\n[DEBUG-MODE] ═══════════════════════════════════");
        Serial.println("[DEBUG-MODE]   RFID INITIALIZATION DEFERRED");
        Serial.println("[DEBUG-MODE] ═══════════════════════════════════");
        Serial.println("[DEBUG-MODE] DEBUG_MODE=true (from config or override)");
        Serial.println("[DEBUG-MODE] ");
        Serial.println("[DEBUG-MODE] Serial commands available:");
        Serial.println("[DEBUG-MODE]   - STATUS, CONFIG, HELP");
        Serial.println("[DEBUG-MODE]   - SET_CONFIG, SAVE_CONFIG, REBOOT");
        Serial.println("[DEBUG-MODE]   - START_SCANNER (enables RFID)");
        Serial.println("[DEBUG-MODE] ═══════════════════════════════════\n");

        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.println("RFID: Debug Mode");
    } else {
        // Production mode: initialize RFID immediately
        Serial.println("\n[PRODUCTION] ═══════════════════════════════════");
        Serial.println("[PRODUCTION]   RFID INITIALIZING NOW");
        Serial.println("[PRODUCTION] ═══════════════════════════════════");
        Serial.println("[PRODUCTION] DEBUG_MODE=false");
        Serial.println("[PRODUCTION] Serial input will NOT work after init");
        Serial.flush();

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("RFID: Initializing...");

        initializeRFID(); // Boots directly into scanning mode

        tft.println("RFID: Ready");
        Serial.println("[PRODUCTION] ✓ RFID initialized");
        Serial.println("[PRODUCTION] ═══════════════════════════════════\n");
    }
    
    // Initialize Audio
    Serial.println("Initializing Audio...");
    Serial.printf("[AUDIO-DEBUG] Timestamp: %lu ms - Audio init in setup() SKIPPED (deferred)\n", millis());
    // out = new AudioOutputI2S(0, 1);  // DEFERRED to prevent beeping
    Serial.println("[AUDIO-FIX] Audio initialization deferred until first use");
    
    // [PHASE 8] Draw the ready screen
    drawReadyScreen();
    
    // Add fix validation check at end of setup()
    Serial.println("━━━ Setup Complete ━━━");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[VALIDATION] Audio initialized: %s (should be NO for fix to work)\n", 
                  out ? "YES - FIX FAILED!" : "NO - Fix working");
    if (!out) {
        Serial.println("[SUCCESS] Audio deferred successfully - beeping should be eliminated");
    }
}

// ─── Serial Command Handler (Phase 6 Fix - Non-blocking) ─────────
/**
 * Process serial commands if available
 * Called multiple times per loop() to ensure responsive command processing
 * even when loop() is busy with RFID scanning or image display
 */
void processSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "CONFIG") {
            Serial.println("\n=== Configuration ===");
            Serial.printf("WiFi SSID: %s\n", wifiSSID.c_str());
            Serial.printf("WiFi Password: %s\n", wifiPassword.length() > 0 ? "***" : "(none)");
            Serial.printf("Orchestrator URL: %s\n", orchestratorURL.c_str());
            Serial.printf("Team ID: %s\n", teamID.c_str());
            Serial.printf("Device ID: %s\n", deviceID.c_str());
            Serial.printf("Sync Tokens: %s\n", syncTokens ? "true" : "false");
            Serial.printf("Debug Mode: %s\n", debugMode ? "true" : "false");
            Serial.println("====================\n");

        } else if (cmd == "STATUS" || cmd == "DIAG_NETWORK") { // [PHASE 10]
            ConnectionState state = getConnectionState();
            Serial.println("\n=== Orchestrator Status ===");
            Serial.printf("Connection: ");
            switch (state) {
                case ORCH_DISCONNECTED:
                    Serial.println("DISCONNECTED");
                    break;
                case ORCH_WIFI_CONNECTED:
                    Serial.printf("WIFI_CONNECTED (%s)\n", WiFi.localIP().toString().c_str());
                    break;
                case ORCH_CONNECTED:
                    Serial.printf("CONNECTED (%s + orchestrator)\n", WiFi.localIP().toString().c_str());
                    break;
            }
            Serial.printf("Queue size: %d entries\n", getQueueSize());
            Serial.printf("Token database: %d tokens loaded\n", tokenDatabaseSize);
            Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
            Serial.println("===========================\n");

        } else if (cmd == "TOKENS") {
            Serial.println("\n=== Token Database ===");
            Serial.printf("Total tokens: %d\n", tokenDatabaseSize);
            for (int i = 0; i < min(10, tokenDatabaseSize); i++) {
                Serial.printf("  %s: video=%s, image=%s\n",
                    tokenDatabase[i].tokenId.c_str(),
                    tokenDatabase[i].video.length() > 0 ? tokenDatabase[i].video.c_str() : "(none)",
                    tokenDatabase[i].image.length() > 0 ? tokenDatabase[i].image.c_str() : "(none)");
            }
            if (tokenDatabaseSize > 10) {
                Serial.printf("  ... and %d more\n", tokenDatabaseSize - 10);
            }
            Serial.println("======================\n");

        } else if (cmd.startsWith("TEST_VIDEO:")) {
            // Phase 5: Test video token detection helpers
            String tokenId = cmd.substring(11);
            tokenId.trim();
            Serial.println("\n=== Phase 5: Video Token Test ===");
            Serial.printf("Testing token ID: %s\n", tokenId.c_str());

            TokenMetadata* meta = getTokenMetadata(tokenId);
            if (meta == nullptr) {
                Serial.println("✗ Token not found in database");
            } else {
                Serial.println("✓ Token found in database");
                Serial.printf("  Token ID: %s\n", meta->tokenId.c_str());
                Serial.printf("  Video field: %s\n", meta->video.length() > 0 ? meta->video.c_str() : "(empty)");
                Serial.printf("  Image field: %s\n", meta->image.length() > 0 ? meta->image.c_str() : "(empty)");
                Serial.printf("  Audio field: %s\n", meta->audio.length() > 0 ? meta->audio.c_str() : "(empty)");
                Serial.printf("  Processing image: %s\n", meta->processingImage.length() > 0 ? meta->processingImage.c_str() : "(empty)");

                Serial.println("\n--- Helper Function Tests ---");
                bool isVideo = hasVideoField(meta);
                Serial.printf("hasVideoField(): %s\n", isVideo ? "TRUE (video token)" : "FALSE (non-video token)");

                String procImgPath = getProcessingImagePath(meta, tokenId);
                Serial.printf("getProcessingImagePath(): %s\n", procImgPath.length() > 0 ? procImgPath.c_str() : "(no processing image)");

                if (procImgPath.length() > 0) {
                    bool fileExists = SD.exists(procImgPath.c_str());
                    Serial.printf("  File exists on SD: %s\n", fileExists ? "YES" : "NO");
                }
            }
            Serial.println("==================================\n");

        } else if (cmd.startsWith("TEST_PROC_IMG:")) {
            // Phase 5: Test processing image display
            String imagePath = cmd.substring(14);
            imagePath.trim();
            Serial.println("\n=== Phase 5: Processing Image Display Test ===");
            Serial.printf("Image path: %s\n", imagePath.c_str());

            // Test the displayProcessingImage function
            displayProcessingImage(imagePath);

            Serial.println("\n--- Display Complete ---");
            Serial.println("Verify display shows:");
            Serial.println("  - BMP image (if file exists) + 'Sending...' overlay");
            Serial.println("  - OR fallback text-only 'Sending...' display");
            Serial.println("==================================\n");

        } else if (cmd.startsWith("SIMULATE_SCAN:")) {
            // Simulate RFID scan without physical hardware
            // Useful for testing token processing flow in debug mode
            String tokenId = cmd.substring(14);
            tokenId.trim();

            Serial.println("\n═══════════════════════════════════════════════");
            Serial.println("     SIMULATE TOKEN SCAN (NO RFID HARDWARE)");
            Serial.println("═══════════════════════════════════════════════");
            Serial.println("");

            if (tokenId.length() == 0) {
                Serial.println("✗ ERROR: No tokenId provided");
                Serial.println("");
                Serial.println("Usage: SIMULATE_SCAN:tokenId");
                Serial.println("Example: SIMULATE_SCAN:kaa001");
                Serial.println("");
                Serial.println("This simulates scanning a physical card with the");
                Serial.println("given tokenId, triggering the full processing flow:");
                Serial.println("  1. Send scan to orchestrator (or queue)");
                Serial.println("  2. Lookup metadata");
                Serial.println("  3. Display processing image (video tokens)");
                Serial.println("  4. OR display BMP + audio (regular tokens)");
                Serial.println("═══════════════════════════════════════════════\n");
                return;
            }

            Serial.printf("Token ID: %s\n", tokenId.c_str());
            Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
            Serial.println("");
            Serial.println("Simulating card scan...");
            Serial.println("(No RFID hardware operations performed)");
            Serial.println("");

            // Call the same function used by actual RFID scans
            processTokenScan(tokenId);

            Serial.println("");
            Serial.println("✓ Simulation complete");
            Serial.printf("Free heap after: %d bytes\n", ESP.getFreeHeap());
            Serial.println("═══════════════════════════════════════════════\n");

        } else if (cmd == "QUEUE_TEST") {
            // Phase 6: Add mock entries to queue for testing
            Serial.println("\n=== Phase 6: Queue Test ===");
            Serial.printf("Adding 20 mock scans to queue...\n");
            Serial.printf("Queue size before: %d\n", getQueueSize());

            for (int i = 1; i <= 20; i++) {
                String mockTokenId = "MOCK_" + String(i);
                String timestamp = generateTimestamp();
                queueScan(mockTokenId, teamID, deviceID, timestamp);
                delay(50); // Small delay between queues
            }

            Serial.printf("Queue size after: %d\n", getQueueSize());
            Serial.println("✓ 20 mock scans added to queue");
            Serial.println("===========================\n");

        } else if (cmd == "FORCE_UPLOAD") {
            // Phase 6: Manually trigger batch upload
            Serial.println("\n=== Phase 6: Force Batch Upload ===");
            int queueSize = getQueueSize();
            Serial.printf("Current queue size: %d\n", queueSize);

            if (queueSize == 0) {
                Serial.println("✗ Queue is empty, nothing to upload");
                Serial.println("Try QUEUE_TEST first to add mock entries");
            } else {
                ConnectionState state = getConnectionState();
                if (state != ORCH_CONNECTED) {
                    Serial.println("⚠️  WARNING: Orchestrator not connected");
                    Serial.println("Upload will fail, but you can see the attempt");
                }

                Serial.println("Triggering batch upload...\n");
                bool success = uploadQueueBatch();

                Serial.printf("\nResult: %s\n", success ? "✓ SUCCESS" : "✗ FAILURE");
                Serial.printf("Queue size after: %d\n", getQueueSize());
            }
            Serial.println("====================================\n");

        } else if (cmd == "SHOW_QUEUE") {
            // Phase 6: Display queue file contents
            Serial.println("\n=== Phase 6: Queue Contents ===");
            int queueSize = getQueueSize();
            Serial.printf("Queue size: %d entries\n\n", queueSize);

            if (queueSize == 0) {
                Serial.println("Queue is empty");
            } else {
                if (sdTakeMutex("SHOW_QUEUE", 1000)) {
                    File file = SD.open("/queue.jsonl", FILE_READ);
                    if (file) {
                        int lineNum = 0;
                        Serial.println("Queue entries:");
                        while (file.available() && lineNum < 20) {
                            String line = file.readStringUntil('\n');
                            line.trim();
                            if (line.length() > 0) {
                                lineNum++;
                                Serial.printf("  %d: %s\n", lineNum, line.c_str());
                            }
                        }
                        if (queueSize > 20) {
                            Serial.printf("  ... and %d more entries\n", queueSize - 20);
                        }
                        file.close();
                    } else {
                        Serial.println("✗ Could not open queue file");
                    }
                    sdGiveMutex("SHOW_QUEUE");
                } else {
                    Serial.println("✗ Could not acquire SD mutex");
                }
            }
            Serial.println("================================\n");

        } else if (cmd == "FORCE_OVERFLOW") { // [PHASE 7] Test command
            Serial.println("\n=== Phase 7: Force Overflow Test ===");
            Serial.println("Manually triggering queue overflow (removes 1 oldest)...");
            handleQueueOverflow();
            Serial.printf("Queue size after: %d\n", getQueueSize());
            Serial.println("====================================\n");

        } else if (cmd.startsWith("SET_CONFIG:")) {
            // Level 1: Runtime config editing (write to memory, save with SAVE_CONFIG)
            String kvPair = cmd.substring(11); // Remove "SET_CONFIG:"
            kvPair.trim();

            int sepIndex = kvPair.indexOf('=');
            if (sepIndex == -1) {
                Serial.println("\n✗ ERROR: Invalid format");
                Serial.println("Usage: SET_CONFIG:KEY=VALUE");
                Serial.println("Example: SET_CONFIG:WIFI_SSID=MyNetwork");
                Serial.println("\nAvailable keys:");
                Serial.println("  WIFI_SSID, WIFI_PASSWORD, ORCHESTRATOR_URL");
                Serial.println("  TEAM_ID, DEVICE_ID, SYNC_TOKENS, DEBUG_MODE");
                Serial.println("\nUse SAVE_CONFIG to persist, then REBOOT to apply\n");
                return;
            }

            String key = kvPair.substring(0, sepIndex);
            String value = kvPair.substring(sepIndex + 1);
            key.trim();
            value.trim();

            Serial.println("\n═══════════════════════════════════════════════");
            Serial.println("       SET CONFIG (MEMORY ONLY)");
            Serial.println("═══════════════════════════════════════════════");
            Serial.printf("Key: %s\n", key.c_str());
            Serial.printf("New Value: %s\n", value.c_str());
            Serial.println("");

            bool validKey = false;

            if (key == "WIFI_SSID") {
                wifiSSID = value;
                validKey = true;
                Serial.println("✓ WIFI_SSID updated in memory");
            } else if (key == "WIFI_PASSWORD") {
                wifiPassword = value;
                validKey = true;
                Serial.println("✓ WIFI_PASSWORD updated in memory");
            } else if (key == "ORCHESTRATOR_URL") {
                orchestratorURL = value;
                validKey = true;
                Serial.println("✓ ORCHESTRATOR_URL updated in memory");
            } else if (key == "TEAM_ID") {
                teamID = value;
                validKey = true;
                Serial.println("✓ TEAM_ID updated in memory");
            } else if (key == "DEVICE_ID") {
                deviceID = value;
                validKey = true;
                Serial.println("✓ DEVICE_ID updated in memory");
            } else if (key == "SYNC_TOKENS") {
                if (value.equalsIgnoreCase("false") || value == "0") {
                    syncTokens = false;
                } else {
                    syncTokens = true;
                }
                validKey = true;
                Serial.printf("✓ SYNC_TOKENS updated to %s\n", syncTokens ? "true" : "false");
            } else if (key == "DEBUG_MODE") {
                if (value.equalsIgnoreCase("false") || value == "0") {
                    debugMode = false;
                } else {
                    debugMode = true;
                }
                validKey = true;
                Serial.printf("✓ DEBUG_MODE updated to %s\n", debugMode ? "true" : "false");
            } else {
                Serial.printf("✗ Unknown key: %s\n", key.c_str());
            }

            if (validKey) {
                Serial.println("");
                Serial.println("⚠️  Changes are in MEMORY ONLY");
                Serial.println("   Use 'SAVE_CONFIG' to write to SD card");
                Serial.println("   Use 'REBOOT' to apply changes");
            }
            Serial.println("═══════════════════════════════════════════════\n");

        } else if (cmd == "SAVE_CONFIG") {
            // Level 1: Write current config to SD card
            Serial.println("\n═══════════════════════════════════════════════");
            Serial.println("       SAVE CONFIG TO SD CARD");
            Serial.println("═══════════════════════════════════════════════");

            if (!sdTakeMutex("SAVE_CONFIG", 1000)) {
                Serial.println("✗ Failed to acquire SD mutex");
                Serial.println("═══════════════════════════════════════════════\n");
                return;
            }

            // Write config.txt
            File file = SD.open(CONFIG_FILE, FILE_WRITE);
            if (!file) {
                Serial.println("✗ Failed to open config.txt for writing");
                sdGiveMutex("SAVE_CONFIG");
                Serial.println("═══════════════════════════════════════════════\n");
                return;
            }

            Serial.println("Writing configuration to /config.txt...");
            file.println("# NeurAI Memory Scanner Configuration");
            file.println("# Generated by SAVE_CONFIG command");
            file.println("");

            file.printf("WIFI_SSID=%s\n", wifiSSID.c_str());
            file.printf("WIFI_PASSWORD=%s\n", wifiPassword.c_str());
            file.printf("ORCHESTRATOR_URL=%s\n", orchestratorURL.c_str());
            file.printf("TEAM_ID=%s\n", teamID.c_str());
            if (deviceID.length() > 0) {
                file.printf("DEVICE_ID=%s\n", deviceID.c_str());
            }
            file.printf("SYNC_TOKENS=%s\n", syncTokens ? "true" : "false");
            file.printf("DEBUG_MODE=%s\n", debugMode ? "true" : "false");

            file.flush();
            file.close();
            sdGiveMutex("SAVE_CONFIG");

            Serial.println("✓✓✓ Configuration saved to SD card ✓✓✓");
            Serial.println("");
            Serial.println("Settings written:");
            Serial.printf("  WIFI_SSID: %s\n", wifiSSID.c_str());
            Serial.printf("  WIFI_PASSWORD: %s\n", wifiPassword.length() > 0 ? "***" : "(empty)");
            Serial.printf("  ORCHESTRATOR_URL: %s\n", orchestratorURL.c_str());
            Serial.printf("  TEAM_ID: %s\n", teamID.c_str());
            Serial.printf("  DEVICE_ID: %s\n", deviceID.c_str());
            Serial.printf("  SYNC_TOKENS: %s\n", syncTokens ? "true" : "false");
            Serial.printf("  DEBUG_MODE: %s\n", debugMode ? "true" : "false");
            Serial.println("");
            Serial.println("⚠️  Use 'REBOOT' command to apply changes");
            Serial.println("═══════════════════════════════════════════════\n");

        } else if (cmd == "REBOOT") {
            // Level 1: Restart ESP32 to apply config changes
            Serial.println("\n═══════════════════════════════════════════════");
            Serial.println("       REBOOTING DEVICE");
            Serial.println("═══════════════════════════════════════════════");
            Serial.println("Restarting ESP32 in 2 seconds...");
            Serial.println("New configuration will be applied on boot");
            Serial.flush();

            delay(2000);

            Serial.println("\n>>> Restarting now...");
            Serial.flush();
            delay(100);

            esp_restart(); // Hardware reset

        } else if (cmd == "START_SCANNER") {
            // [DEBUG MODE FIX] Manually initialize RFID (GPIO 3 conflict workaround)
            Serial.println("\n═══════════════════════════════════════════════");
            Serial.println("          STARTING RFID SCANNER");
            Serial.println("═══════════════════════════════════════════════");
            Serial.println("");

            if (rfidInitialized) {
                Serial.println("⚠️  RFID scanner already initialized!");
                Serial.println("✓ Scanner is ready to detect cards");
                Serial.println("");
            } else {
                Serial.println("⚠️  WARNING: This will permanently disable serial input!");
                Serial.println("");
                Serial.println("GPIO 3 will be reconfigured from Serial RX to RFID_SS.");
                Serial.println("After this command, you will NOT be able to send any");
                Serial.println("more serial commands (STATUS, CONFIG, etc.)");
                Serial.println("");
                Serial.println("Initializing RFID in 2 seconds...");
                Serial.flush(); // Ensure this message is sent

                delay(2000); // Give user time to read warning

                Serial.println("");
                Serial.println(">>> Activating RFID now...");
                Serial.flush();

                // Call the RFID init function (will kill serial RX)
                initializeRFID();

                // Update display to show ready state
                drawReadyScreen();

                Serial.println("");
                Serial.println("✓✓✓ RFID SCANNER ACTIVE ✓✓✓");
                Serial.println("");
                Serial.println("You can now scan RFID cards.");
                Serial.println("(This is the last message you will see via serial)");
                Serial.flush();
            }

            Serial.println("═══════════════════════════════════════════════\n");

        } else if (cmd == "HELP") {
            Serial.println("\n═══════════════════════════════════════════════");
            Serial.println("       ORCHESTRATOR DEBUG COMMANDS");
            Serial.println("═══════════════════════════════════════════════");
            Serial.println("");
            Serial.println("⚠️  DEBUG MODE ACTIVE (GPIO 3 conflict workaround)");
            Serial.println("   RFID scanner is disabled to allow serial commands.");
            Serial.println("   Send 'START_SCANNER' when ready to enable RFID.");
            Serial.println("");
            Serial.println("── Scanner Control ──────────────────────────────");
            Serial.println("  START_SCANNER  - [!!!] Enable RFID, KILL serial [!!!]");
            Serial.println("");
            Serial.println("── System Info ──────────────────────────────────");
            Serial.println("  CONFIG         - Show configuration");
            Serial.println("  STATUS         - Show connection status and queue");
            Serial.println("  DIAG_NETWORK   - [PHASE 10] Alias for STATUS");
            Serial.println("  TOKENS         - Show token database (first 10)");
            Serial.println("");
            Serial.println("── Runtime Config (Level 1) ─────────────────────");
            Serial.println("  SET_CONFIG:KEY=VALUE  - Update config in memory");
            Serial.println("  SAVE_CONFIG           - Write config to SD card");
            Serial.println("  REBOOT                - Restart to apply changes");
            Serial.println("");
            Serial.println("  Example workflow:");
            Serial.println("    SET_CONFIG:WIFI_SSID=NewNetwork");
            Serial.println("    SET_CONFIG:DEBUG_MODE=false");
            Serial.println("    SAVE_CONFIG");
            Serial.println("    REBOOT");
            Serial.println("");
            Serial.println("── Testing ──────────────────────────────────────");
            Serial.println("  SIMULATE_SCAN:id - Simulate token scan (full flow)");
            Serial.println("  TEST_VIDEO:id    - Test Phase 5 video helpers");
            Serial.println("  TEST_PROC_IMG:path - Test Phase 5 processing image");
            Serial.println("");
            Serial.println("── Queue Management (Phase 6/7) ─────────────────");
            Serial.println("  QUEUE_TEST     - Add 20 mock scans to queue");
            Serial.println("  FORCE_UPLOAD   - Manually trigger batch upload");
            Serial.println("  SHOW_QUEUE     - Display queue contents (first 20)");
            Serial.println("  FORCE_OVERFLOW - [PHASE 7] Manually remove oldest scan");
            Serial.println("");
            Serial.println("  HELP           - Show this help");
            Serial.println("═══════════════════════════════════════════════\n");
        }
    }
}

// ─── Token Scan Processing (Extracted for Reuse) ─────────────────
/**
 * Process a token scan (orchestrator send + display)
 *
 * This function contains the complete token processing flow:
 * - Send scan to orchestrator (or queue if offline)
 * - Check metadata for video tokens
 * - Display processing image for video tokens OR BMP/audio for regular tokens
 *
 * Called by:
 * - loop() after successful RFID card read
 * - SIMULATE_SCAN command for testing without physical hardware
 *
 * @param tokenId The token identifier (from NDEF text or UID)
 * @return void
 * @modifies imageIsDisplayed (sets to true for regular tokens)
 */
void processTokenScan(String tokenId) {
    // ═══ ORCHESTRATOR SCAN ROUTING ═══════════════════════════════════
    Serial.println("\n[SCAN] ═══ ORCHESTRATOR SCAN START ═══");
    Serial.printf("[SCAN] Free heap: %d bytes\n", ESP.getFreeHeap());
    unsigned long scanStartMs = millis();

    Serial.printf("[SCAN] Token ID: %s\n", tokenId.c_str());

    // Create scan request
    ConnectionState state = getConnectionState();
    String timestamp = generateTimestamp();

    Serial.printf("[SCAN] Connection state: ");
    switch (state) {
        case ORCH_DISCONNECTED:
            Serial.println("DISCONNECTED");
            break;
        case ORCH_WIFI_CONNECTED:
            Serial.println("WIFI_CONNECTED");
            break;
        case ORCH_CONNECTED:
            Serial.println("CONNECTED");
            break;
    }
    Serial.printf("[SCAN] Timestamp: %s\n", timestamp.c_str());

    // Route scan based on connection state
    bool scanSent = false;
    if (state == ORCH_CONNECTED) {
        Serial.println("[SCAN] Attempting to send to orchestrator...");
        scanSent = sendScan(tokenId, teamID, deviceID, timestamp);

        if (scanSent) {
            Serial.println("[SCAN] ✓✓✓ SUCCESS ✓✓✓ Sent to orchestrator");
        } else {
            Serial.println("[SCAN] ✗ Send failed, queueing");
            queueScan(tokenId, teamID, deviceID, timestamp);
        }
    } else {
        Serial.printf("[SCAN] Offline/disconnected, queueing immediately\n");
        queueScan(tokenId, teamID, deviceID, timestamp);
    }

    unsigned long scanLatencyMs = millis() - scanStartMs;

    Serial.printf("[SCAN] Queue size: %d entries\n", getQueueSize());
    Serial.printf("[SCAN] Total scan processing latency: %lu ms\n", scanLatencyMs);
    Serial.printf("[SCAN] Free heap after scan: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[SCAN] ═══ ORCHESTRATOR SCAN END ═══\n");
    // ═══════════════════════════════════════════════════════════════════

    // ═══ LOCAL FILE PATH CONSTRUCTION ═════════════════════════════════
    // Construct paths from tokenId (treat as NDEF text format)
    String filename = ndefToFilename(tokenId);        // e.g., "/images/kaa001.bmp"
    String audioFilename = ndefToAudioFilename(tokenId);  // e.g., "/AUDIO/kaa001.wav"

    // ═══ VIDEO TOKEN DETECTION ═══════════════════════════════════════
    // Check metadata for video token
    TokenMetadata* metadata = getTokenMetadata(tokenId);
    if (metadata != nullptr) {
        Serial.printf("[TOKEN] Found metadata for tokenId: %s\n", tokenId.c_str());
        Serial.printf("[TOKEN]   video: %s\n", metadata->video.length() > 0 ? metadata->video.c_str() : "(none)");

        // Video token routing - check if has video field
        if (hasVideoField(metadata)) {
            Serial.println("\n[VIDEO] ═══ VIDEO TOKEN PROCESSING START ═══");
            Serial.printf("[VIDEO] Token ID: %s\n", tokenId.c_str());
            Serial.printf("[VIDEO] Video URL: %s\n", metadata->video.c_str());
            Serial.printf("[VIDEO] Free heap: %d bytes\n", ESP.getFreeHeap());

            // Scan already sent to orchestrator - fire-and-forget
            Serial.println("[VIDEO] ✓ Scan already sent to orchestrator (fire-and-forget)");

            // Display processing image with "Sending..." modal (2.5s)
            String procImgPath = getProcessingImagePath(metadata, tokenId);
            Serial.printf("[VIDEO] Processing image path: %s\n",
                          procImgPath.length() > 0 ? procImgPath.c_str() : "(no processing image)");

            displayProcessingImage(procImgPath);  // Includes 2.5s auto-hide timer

            // Return to ready mode after processing image timeout
            Serial.println("[VIDEO] Modal timeout complete, returning to ready mode");

            // Clear display and show ready screen
            drawReadyScreen();

            Serial.printf("[VIDEO] Free heap after video processing: %d bytes\n", ESP.getFreeHeap());
            Serial.println("[VIDEO] ═══ VIDEO TOKEN PROCESSING END ═══\n");

            // Early return - skip local content display (BMP/audio)
            Serial.println("[VIDEO] ✓ Skipping local content display for video token");
            return;  // Exit function immediately
        }
    } else {
        Serial.printf("[TOKEN] No metadata found for tokenId: %s (will use local files if present)\n", tokenId.c_str());
    }
    // ═══════════════════════════════════════════════════════════════════

    // ═══ REGULAR TOKEN DISPLAY ═══════════════════════════════════════
    // Display token ID on screen
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("Token Scanned:");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println(tokenId);
    delay(1000);

    Serial.printf("[DISPLAY] BMP: %s\n", filename.c_str());
    Serial.printf("[DISPLAY] Audio: %s\n", audioFilename.c_str());

    // Display the image
    Serial.println("[DISPLAY] About to call drawBmp()");
    Serial.printf("[DISPLAY] Filename: %s\n", filename.c_str());
    Serial.flush();

    // Must acquire mutex to draw from SD
    if (sdTakeMutex("drawBmpScan", 1000)) {
        drawBmp(filename);
        sdGiveMutex("drawBmpScan");
    } else {
        Serial.println("[DISPLAY] ✗ Failed to get SD mutex for drawBmp");
    }
    Serial.println("[DISPLAY] drawBmp() returned");

    // Start audio playback
    Serial.println("[DISPLAY] About to call startAudio()");
    Serial.printf("[DISPLAY] Audio filename: %s\n", audioFilename.c_str());
    Serial.flush();

    // Must acquire mutex to play from SD
    if (sdTakeMutex("startAudioScan", 1000)) {
        startAudio(audioFilename);
        sdGiveMutex("startAudioScan");
    } else {
        Serial.println("[DISPLAY] ✗ Failed to get SD mutex for startAudio");
    }
    Serial.println("[DISPLAY] startAudio() returned");

    // Update state
    imageIsDisplayed = true;
    touchInterruptOccurred = false;
    lastTouchWasValid = false;

    Serial.println("[SCAN] Token processing complete");
    // ═══════════════════════════════════════════════════════════════════
}

// ─── Main Loop ────────────────────────────────────────────────────
void loop() {
    // Process serial commands (responsive - called multiple times per loop)
    processSerialCommands();

    // Process audio if playing
    if (wav && wav->isRunning()) {
        if (!wav->loop()) {
            stopAudio();
        }
    }

    // Check for commands before image display handling
    processSerialCommands();

    // [PHASE 8] US6 - Rewritten Touch Handling Logic
    if (touchInterruptOccurred) {
        touchInterruptOccurred = false;

        // ═══ WIFI EMI FILTER (validated Oct 19, 2025) ═══
        delayMicroseconds(100); // Stabilization delay
        uint32_t pulseWidthUs = measureTouchPulseWidth();

        if (pulseWidthUs < TOUCH_PULSE_WIDTH_THRESHOLD_US) {
            // WiFi EMI rejected - pulse width too brief
            // [VALIDATION FIX] Comment out logging to prevent serial buffer flood
            // Serial.printf("[TOUCH-FILTER] EMI pulse rejected (%d us)\n", pulseWidthUs);
            return;
        }
        Serial.printf("[TOUCH-FILTER] Valid touch pulse (%d us)\n", pulseWidthUs);
        // ════════════════════════════════════════════════

        // REAL TOUCH DETECTED - Continue with debouncing
        uint32_t now = millis();
        if (now - lastTouchDebounce < TOUCH_DEBOUNCE) {
            Serial.println("[TOUCH-DIAG] Debounced");
            return;
        }
        lastTouchDebounce = now;
        
        // --- State-based Touch Routing ---

        // 1. If Status Screen is displayed, any tap dismisses it (FR-044)
        if (statusScreenDisplayed) {
            Serial.println("[PHASE8-DIAG] Tap detected: Dismissing status screen");
            dismissStatusScreen();
            statusScreenDisplayed = false;
            lastTouchWasValid = false; // Reset double-tap logic
            return;
        }
        
        // 2. If Image is displayed, check for double-tap (FR-036)
        if (imageIsDisplayed) {
            if (lastTouchWasValid && (now - lastTouchTime) < DOUBLE_TAP_TIMEOUT) {
                // Double-tap detected
                Serial.println("[Touch] Double-tap - returning to scan mode");
                stopAudio();
                imageIsDisplayed = false;
                lastTouchWasValid = false;
                lastNDEFText = "";
                
                // [PHASE 8] Reset display to ready screen
                drawReadyScreen();
                
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
                Serial.println("[Touch] First tap registered (on image)");
            }
            return; // Stay in this state
        }
        
        // 3. If Idle (neither screen displayed), single-tap shows status (FR-038)
        if (!imageIsDisplayed && !statusScreenDisplayed) {
            // Use the single-tap tracking logic
            if (lastTouchWasValid && (now - lastTouchTime) < DOUBLE_TAP_TIMEOUT) {
                // This is a double-tap, but we are idle, so ignore
                lastTouchWasValid = false;
            } else {
                Serial.println("[PHASE8-DIAG] Tap detected: Displaying status screen");
                displayStatusScreen();
                statusScreenDisplayed = true;
                lastTouchTime = now;
                lastTouchWasValid = true; // Use this to track the tap
            }
            return;
        }

    } else if (lastTouchWasValid && (millis() - lastTouchTime) >= DOUBLE_TAP_TIMEOUT) {
        lastTouchWasValid = false; // Clear single-tap flag
    }
    
    // If we are displaying content or status, do not scan for RFID
    if (imageIsDisplayed || statusScreenDisplayed) {
        return;
    }

    // ═══════════════════════════════════════════════════════════════════
    // ═══ DEBUG MODE FIX: Guard RFID Scanning ═══════════════════════════
    // ═══════════════════════════════════════════════════════════════════
    // Only run RFID scanner if it has been initialized via START_SCANNER
    // This prevents GPIO 3 conflict at boot (Serial RX vs RFID_SS)
    if (!rfidInitialized) {
        // RFID not initialized yet - skip scanning and return
        // User must send START_SCANNER command via serial first
        return;
    }
    // ═══════════════════════════════════════════════════════════════════

    // RFID Scanning Mode (only runs if rfidInitialized == true)
    rfidStats.totalScans++;
    
    // Small delay between scans to let the field stabilize
    // INCREASED TO 500ms to reduce beeping frequency
    static uint32_t lastScanTime = 0;

    // Check for commands during RFID wait period (responsive command processing)
    processSerialCommands();

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

    // ═══ EXTRACT TOKEN ID ═════════════════════════════════════════════
    // Extract tokenId for orchestrator (NDEF text or UID hex)
    String tokenId;
    if (ndefText.length() > 0) {
        tokenId = ndefText;
        Serial.printf("[RFID] Using NDEF text as tokenId: '%s'\n", ndefText.c_str());
        lastNDEFText = ndefText;
    } else {
        // Convert UID to hex string for tokenId
        tokenId = "";
        for (byte i = 0; i < uid.size; i++) {
            char hex[3];
            sprintf(hex, "%02x", uid.uidByte[i]);
            tokenId += String(hex);
        }
        Serial.printf("[RFID] No NDEF text, using UID as tokenId: %s\n", tokenId.c_str());
    }
    // ═══════════════════════════════════════════════════════════════════

    // ═══ PROCESS TOKEN SCAN (REFACTORED) ══════════════════════════════
    // Call the extracted helper function to handle orchestrator sending + display
    // This function is also used by SIMULATE_SCAN for testing without hardware
    processTokenScan(tokenId);
    // ═══════════════════════════════════════════════════════════════════
    
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