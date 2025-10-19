// Test 19: Exact ALNScanner RFID Sequence with Error Paths
// Purpose: Replicate EXACTLY what ALNScanner does during RFID operations
// Focus: PICC_RequestA, PICC_Select with retries, extractNDEFText, error handling

#include <TFT_eSPI.h>
#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>  // For constants only

// SD Card pins (Hardware VSPI)
#define SD_CS       5
#define SDSPI_SCK   18
#define SDSPI_MISO  19
#define SDSPI_MOSI  23

// RFID pins (Software SPI)
#define RFID_SS     3
#define RFID_SCK    22
#define RFID_MOSI   27
#define RFID_MISO   35

// Global objects (EXACT match to ALNScanner)
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
MFRC522::Uid uid;  // This might be the issue!
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Test tracking
bool sdWorking = false;
int testPhase = 0;
String lastNDEFText = "";

// === EXACT ALNScanner Software SPI Implementation ===
byte softSPI_transfer(byte data) {
    byte result = 0;
    
    portENTER_CRITICAL(&spiMux);
    
    for (int i = 0; i < 8; ++i) {
        digitalWrite(RFID_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        delayMicroseconds(2);
        digitalWrite(RFID_SCK, HIGH);
        delayMicroseconds(2);
        result = (result << 1) | digitalRead(RFID_MISO);
        digitalWrite(RFID_SCK, LOW);
        delayMicroseconds(2);
    }
    
    portEXIT_CRITICAL(&spiMux);
    
    return result;
}

// === EXACT ALNScanner RFID Register Functions ===
void SoftSPI_WriteRegister(byte reg, byte value) {
    digitalWrite(RFID_SS, LOW);
    softSPI_transfer((reg & 0x3F) << 1);
    softSPI_transfer(value);
    digitalWrite(RFID_SS, HIGH);
}

byte SoftSPI_ReadRegister(byte reg) {
    digitalWrite(RFID_SS, LOW);
    softSPI_transfer(0x80 | ((reg & 0x3F) << 1));
    byte value = softSPI_transfer(0x00);
    digitalWrite(RFID_SS, HIGH);
    return value;
}

void SoftSPI_SetRegisterBitMask(byte reg, byte mask) {
    byte tmp = SoftSPI_ReadRegister(reg);
    SoftSPI_WriteRegister(reg, tmp | mask);
}

// === EXACT ALNScanner CRC Calculation (memory intensive!) ===
MFRC522::StatusCode SoftSPI_CalculateCRC(byte *data, byte length, byte *result) {
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    SoftSPI_WriteRegister(MFRC522::DivIrqReg, 0x04);
    SoftSPI_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
    
    for (byte i = 0; i < length; i++) {
        SoftSPI_WriteRegister(MFRC522::FIFODataReg, data[i]);
    }
    
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_CalcCRC);
    
    uint16_t timeout = 5000;
    while (timeout--) {
        byte n = SoftSPI_ReadRegister(MFRC522::DivIrqReg);
        if (n & 0x04) break;
        delayMicroseconds(10);
    }
    
    if (timeout == 0) {
        Serial.println("  CRC timeout!");
        return MFRC522::STATUS_TIMEOUT;
    }
    
    result[0] = SoftSPI_ReadRegister(MFRC522::CRCResultRegL);
    result[1] = SoftSPI_ReadRegister(MFRC522::CRCResultRegH);
    
    return MFRC522::STATUS_OK;
}

// === EXACT ALNScanner Communication Function ===
MFRC522::StatusCode SoftSPI_CommunicateWithPICC(byte command, byte *sendData, byte sendLen,
                                                byte *backData, byte *backLen, 
                                                byte *validBits = nullptr, 
                                                byte rxAlign = 0, 
                                                bool checkCRC = false) {
    // Clear all interrupts
    SoftSPI_WriteRegister(MFRC522::ComIrqReg, 0x7F);
    
    // Flush FIFO
    SoftSPI_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
    
    // Write data to FIFO
    for (byte i = 0; i < sendLen; i++) {
        SoftSPI_WriteRegister(MFRC522::FIFODataReg, sendData[i]);
    }
    
    // Configure BitFramingReg
    byte txLastBits = validBits ? *validBits : 0;
    byte bitFraming = (rxAlign << 4) + txLastBits;
    SoftSPI_WriteRegister(MFRC522::BitFramingReg, bitFraming);
    
    // Execute command
    SoftSPI_WriteRegister(MFRC522::CommandReg, command);
    
    if (command == MFRC522::PCD_Transceive) {
        SoftSPI_SetRegisterBitMask(MFRC522::BitFramingReg, 0x80);  // StartSend=1
    }
    
    // Wait for completion
    uint32_t timeout = millis() + 100;  // 100ms timeout
    bool completed = false;
    byte irq;
    
    while (millis() < timeout) {
        irq = SoftSPI_ReadRegister(MFRC522::ComIrqReg);
        if (irq & 0x30) {  // RxIRq or IdleIRq
            completed = true;
            break;
        }
        if (irq & 0x01) {  // Timer expired
            return MFRC522::STATUS_TIMEOUT;
        }
    }
    
    if (!completed) {
        Serial.println("  Communication timeout!");
        return MFRC522::STATUS_TIMEOUT;
    }
    
    // Check for errors
    byte error = SoftSPI_ReadRegister(MFRC522::ErrorReg);
    if (error & 0x13) {  // BufferOvfl ParityErr ProtocolErr
        Serial.printf("  Error register: 0x%02X\n", error);
        return MFRC522::STATUS_ERROR;
    }
    
    // Read data from FIFO
    byte n = SoftSPI_ReadRegister(MFRC522::FIFOLevelReg);
    if (n > *backLen) {
        Serial.printf("  Buffer overflow: %d > %d\n", n, *backLen);
        return MFRC522::STATUS_NO_ROOM;
    }
    
    *backLen = n;
    for (byte i = 0; i < n; i++) {
        backData[i] = SoftSPI_ReadRegister(MFRC522::FIFODataReg);
    }
    
    // Get valid bits
    if (validBits) {
        *validBits = SoftSPI_ReadRegister(MFRC522::ControlReg) & 0x07;
    }
    
    // Check collision
    if (error & 0x08) {
        Serial.println("  Collision detected!");
        return MFRC522::STATUS_COLLISION;
    }
    
    return MFRC522::STATUS_OK;
}

// === CRITICAL: EXACT ALNScanner PICC_Select with RETRY LOGIC ===
MFRC522::StatusCode SoftSPI_PICC_Select(MFRC522::Uid *uid, byte validBits = 0) {
    Serial.println("  Executing PICC_Select with retry logic...");
    
    // This is where ALNScanner does complex collision handling!
    const int MAX_SELECT_RETRIES = 3;
    
    for (int attempt = 0; attempt < MAX_SELECT_RETRIES; attempt++) {
        if (attempt > 0) {
            Serial.printf("  Retry %d/%d\n", attempt, MAX_SELECT_RETRIES - 1);
            
            // CRITICAL: Reset sequence that might corrupt state!
            SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
            delay(10 * attempt);  // Progressive backoff
            
            // Clear ALL error flags - POTENTIAL ISSUE!
            SoftSPI_WriteRegister(MFRC522::ErrorReg, 0x1F);
            
            // Re-configure collision register
            SoftSPI_SetRegisterBitMask(MFRC522::CollReg, 0x80);
            
            // Re-enable antenna
            SoftSPI_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);
            delay(2);
        }
        
        // Simulate select operation (simplified)
        byte buffer[9];
        buffer[0] = MFRC522::PICC_CMD_SEL_CL1;
        buffer[1] = 0x70;  // NVB
        
        // Add some fake UID bytes
        for (int i = 0; i < 4; i++) {
            buffer[2 + i] = random(256);
        }
        
        byte bufferSize = 6;
        MFRC522::StatusCode result = SoftSPI_CommunicateWithPICC(
            MFRC522::PCD_Transceive, buffer, bufferSize, 
            buffer, &bufferSize, nullptr, 0, false
        );
        
        if (result == MFRC522::STATUS_OK) {
            // Fake successful select
            uid->size = 4;
            for (int i = 0; i < 4; i++) {
                uid->uidByte[i] = buffer[2 + i];
            }
            uid->sak = 0x00;  // NTAG
            Serial.println("  Select succeeded");
            return MFRC522::STATUS_OK;
        }
        
        if (result == MFRC522::STATUS_COLLISION) {
            Serial.println("  Collision during select");
            // Continue retry loop
        }
    }
    
    Serial.println("  Select failed after all retries");
    return MFRC522::STATUS_ERROR;
}

// === EXACT ALNScanner extractNDEFText (MEMORY INTENSIVE!) ===
String extractNDEFText() {
    Serial.println("  Starting NDEF extraction (memory intensive)...");
    
    if (uid.sak != 0x00) {
        Serial.println("  Not NTAG, skipping");
        return "";
    }
    
    String extractedText = "";
    
    // Read pages 3-6 (simplified - real one does more)
    byte cmdBuffer[4];
    cmdBuffer[0] = 0x30;  // READ command
    cmdBuffer[1] = 3;      // Start at page 3
    
    // Calculate CRC (memory allocation!)
    MFRC522::StatusCode status = SoftSPI_CalculateCRC(cmdBuffer, 2, &cmdBuffer[2]);
    if (status != MFRC522::STATUS_OK) {
        Serial.println("  CRC calculation failed");
        return "";
    }
    
    // Simulate reading and string building
    byte buffer[16];
    for (int i = 0; i < 16; i++) {
        buffer[i] = random(256);  // Simulate garbage from bad MFRC522
    }
    
    // String operations that might fragment memory
    for (int i = 0; i < 10; i++) {
        extractedText += "TEST";
        extractedText += String(i);
        extractedText += "_";
    }
    
    // More string manipulation
    extractedText.trim();
    extractedText.toUpperCase();
    
    Serial.printf("  Built NDEF string: %s (length=%d)\n", 
                  extractedText.c_str(), extractedText.length());
    
    return extractedText;
}

// Test SD access
bool testSD(const char* context) {
    Serial.printf("\n[PHASE %d] Testing SD: %s\n", testPhase, context);
    Serial.print("  Opening /test.txt... ");
    
    unsigned long start = millis();
    File f = SD.open("/test.txt");
    unsigned long elapsed = millis() - start;
    
    if (f) {
        Serial.printf("SUCCESS (%lu ms)\n", elapsed);
        f.close();
        return true;
    } else {
        Serial.printf("FAILED (%lu ms)\n", elapsed);
        if (elapsed > 100) {
            Serial.println("  WARNING: Slow SD response!");
        }
        return false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\nTEST19: EXACT RFID SEQUENCE TEST");
    Serial.println("=================================");
    
    // Initialize display
    testPhase = 1;
    Serial.println("\nPHASE 1: Initialize TFT");
    tft.begin();
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("TEST 19");
    
    // Initialize SD
    testPhase = 2;
    Serial.println("\nPHASE 2: Initialize SD");
    SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI);
    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("  SD init FAILED!");
        return;
    }
    Serial.println("  SD initialized");
    
    // Test baseline
    testSD("Baseline before RFID");
    
    // Initialize RFID pins
    testPhase = 3;
    Serial.println("\nPHASE 3: Initialize RFID");
    pinMode(RFID_SS, OUTPUT);
    pinMode(RFID_SCK, OUTPUT);
    pinMode(RFID_MOSI, OUTPUT);
    pinMode(RFID_MISO, INPUT_PULLUP);
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(RFID_SCK, LOW);
    
    // RFID initialization sequence
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
    delay(100);
    SoftSPI_WriteRegister(MFRC522::TModeReg, 0x80);
    SoftSPI_WriteRegister(MFRC522::TPrescalerReg, 0xA9);
    SoftSPI_WriteRegister(MFRC522::TReloadRegH, 0x03);
    SoftSPI_WriteRegister(MFRC522::TReloadRegL, 0xE8);
    Serial.println("  RFID initialized");
    
    testSD("After RFID init");
    
    // === CRITICAL TEST: Full RFID Scan Sequence ===
    testPhase = 4;
    Serial.println("\nPHASE 4: EXACT ALNScanner scan sequence");
    Serial.println("----------------------------------------");
    
    // Step 1: PICC_RequestA (not shown but implied)
    Serial.println("Simulating PICC_RequestA...");
    byte atqa[2] = {0x44, 0x00};  // NTAG response
    
    // Step 2: PICC_Select with retries (CRITICAL!)
    MFRC522::StatusCode status = SoftSPI_PICC_Select(&uid, 0);
    
    // Step 3: extractNDEFText (memory intensive!)
    String ndefText = extractNDEFText();
    lastNDEFText = ndefText;
    
    // Step 4: String manipulation for filenames
    String filename = "/IMG/";
    if (ndefText.length() > 0) {
        String cleanText = ndefText;
        cleanText.replace(":", "");
        cleanText.replace(" ", "");
        cleanText.trim();
        cleanText.toUpperCase();
        filename += cleanText + ".bmp";
    } else {
        // UID to filename
        for (byte i = 0; i < uid.size; i++) {
            if (uid.uidByte[i] < 0x10) filename += '0';
            filename += String(uid.uidByte[i], HEX);
        }
        filename.toUpperCase();
        filename += ".bmp";
    }
    
    Serial.printf("Generated filename: %s\n", filename.c_str());
    
    // === THE CRITICAL MOMENT: drawBmp SD.open() ===
    testPhase = 5;
    Serial.println("\nPHASE 5: THE HANG TEST - drawBmp sequence");
    Serial.println("------------------------------------------");
    
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    
    Serial.printf("Attempting SD.open('%s')...\n", filename.c_str());
    unsigned long startHang = millis();
    
    File f = SD.open(filename);  // THIS IS WHERE ALNSANNER HANGS!
    
    unsigned long elapsedHang = millis() - startHang;
    
    if (elapsedHang > 100) {
        Serial.printf("*** WARNING: SD.open() took %lu ms! ***\n", elapsedHang);
        Serial.println("*** HANG CONDITION DETECTED! ***");
        tft.println("HANG!");
    } else if (f) {
        Serial.printf("File opened in %lu ms\n", elapsedHang);
        f.close();
        tft.println("OK");
    } else {
        Serial.printf("File not found in %lu ms (normal)\n", elapsedHang);
        tft.println("Missing:");
        tft.println(filename);
    }
    
    // Final test
    sdWorking = testSD("After drawBmp sequence");
    
    // === RESULTS ===
    Serial.println("\n========================================");
    Serial.println("TEST COMPLETE");
    Serial.println("========================================");
    
    if (sdWorking) {
        Serial.println("SD survived all operations");
    } else {
        Serial.println("SD FAILED - hang condition reproduced!");
        
        // Memory analysis
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("Last NDEF length: %d\n", lastNDEFText.length());
    }
}

void loop() {
    delay(5000);
    testSD("Periodic check");
}