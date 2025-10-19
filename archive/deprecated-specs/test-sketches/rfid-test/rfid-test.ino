// RFID Test Sketch for CYD - Based on ALNScanner0812Working
// Uses proven software SPI implementation that works on micro USB variant

#include <SPI.h>
#include <MFRC522.h>
#include <TFT_eSPI.h>

// RFID Configuration from working sketch
#define RFID_MAX_RETRIES 3
#define RFID_RETRY_DELAY_MS 20
#define RFID_OPERATION_DELAY_US 10
#define RFID_TIMEOUT_MS 100
#define RFID_CLOCK_DELAY_US 2

// RFID pins (software SPI) - EXACTLY as in working sketch
#define SOFT_SPI_SCK  22
#define SOFT_SPI_MOSI 27  // Shared with dual USB backlight
#define SOFT_SPI_MISO 35
#define RFID_SS       3
#define RFID_RST      MFRC522::UNUSED_PIN

// Backlight pins
#define BACKLIGHT_PIN_SINGLE 21
#define BACKLIGHT_PIN_DUAL   27

TFT_eSPI tft = TFT_eSPI();
MFRC522::Uid uid;

// FreeRTOS Mutex for atomic SPI operations (from working sketch)
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// ─── EXACT Software SPI Implementation from ALNScanner0812Working ─────
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

byte SoftSPI_ReadRegister(MFRC522::PCD_Register reg) {
    digitalWrite(RFID_SS, LOW);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    softSPI_transfer(reg | 0x80);
    byte value = softSPI_transfer(0);
    digitalWrite(RFID_SS, HIGH);
    delayMicroseconds(RFID_OPERATION_DELAY_US);
    return value;
}

void SoftSPI_SetRegisterBitMask(MFRC522::PCD_Register reg, byte mask) {
    byte tmp = SoftSPI_ReadRegister(reg);
    SoftSPI_WriteRegister(reg, tmp | mask);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("CYD RFID Test v2.0");
    Serial.println("Using proven ALNScanner0812Working code");
    Serial.println("========================================");
    
    // Initialize display
    tft.begin();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("RFID Test v2.0");
    
    // Set up backlight - both pins HIGH to avoid conflicts
    pinMode(BACKLIGHT_PIN_SINGLE, OUTPUT);
    pinMode(BACKLIGHT_PIN_DUAL, OUTPUT);
    digitalWrite(BACKLIGHT_PIN_SINGLE, HIGH);
    digitalWrite(BACKLIGHT_PIN_DUAL, HIGH);
    
    Serial.println("\n[SETUP] Initializing Software SPI for RFID...");
    pinMode(SOFT_SPI_SCK, OUTPUT);
    pinMode(SOFT_SPI_MOSI, OUTPUT);
    pinMode(SOFT_SPI_MISO, INPUT);
    pinMode(RFID_SS, OUTPUT);
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(SOFT_SPI_SCK, LOW);
    
    Serial.printf("[SETUP] RFID Pins: SCK=%d, MOSI=%d, MISO=%d, SS=%d\n",
                  SOFT_SPI_SCK, SOFT_SPI_MOSI, SOFT_SPI_MISO, RFID_SS);
    
    // Soft reset MFRC522
    Serial.println("[SETUP] Resetting MFRC522...");
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
    delay(100);  // Wait for reset
    
    // Initialize MFRC522 - same settings as working sketch
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    
    // Timer settings
    SoftSPI_WriteRegister(MFRC522::TModeReg, 0x80);
    SoftSPI_WriteRegister(MFRC522::TPrescalerReg, 0xA9);
    SoftSPI_WriteRegister(MFRC522::TReloadRegH, 0x03);
    SoftSPI_WriteRegister(MFRC522::TReloadRegL, 0xE8);
    
    // Force 100% ASK modulation
    SoftSPI_WriteRegister(MFRC522::TxASKReg, 0x40);
    
    // Set CRC preset
    SoftSPI_WriteRegister(MFRC522::ModeReg, 0x3D);
    
    // Configure receiver gain (48dB)
    SoftSPI_WriteRegister(MFRC522::RFCfgReg, 0x70);
    
    // Enable antenna
    byte txControlReg = SoftSPI_ReadRegister(MFRC522::TxControlReg);
    if ((txControlReg & 0x03) != 0x03) {
        SoftSPI_WriteRegister(MFRC522::TxControlReg, txControlReg | 0x03);
    }
    
    delay(10);  // Let antenna stabilize
    
    // Read and display version
    byte version = SoftSPI_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[SETUP] MFRC522 Version: 0x%02X ", version);
    
    switch(version) {
        case 0x92: 
            Serial.println("(v2.0)");
            tft.setCursor(10, 40);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.println("RFID: v2.0 OK");
            break;
        case 0x91: 
            Serial.println("(v1.0)");
            tft.setCursor(10, 40);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.println("RFID: v1.0 OK");
            break;
        case 0x00:
        case 0xFF:
            Serial.println("(NOT DETECTED - CHECK WIRING!)");
            tft.setCursor(10, 40);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("RFID: FAILED!");
            tft.setCursor(10, 60);
            tft.println("Check wiring");
            break;
        default:
            Serial.printf("(unknown: 0x%02X)\n", version);
            tft.setCursor(10, 40);
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.printf("RFID: 0x%02X", version);
            break;
    }
    
    // Verify critical registers
    Serial.println("\n[DIAG] Register Check:");
    Serial.printf("  CommandReg:  0x%02X\n", SoftSPI_ReadRegister(MFRC522::CommandReg));
    Serial.printf("  Status2Reg:  0x%02X\n", SoftSPI_ReadRegister(MFRC522::Status2Reg));
    Serial.printf("  TxControlReg: 0x%02X (antenna should be 0x83)\n", SoftSPI_ReadRegister(MFRC522::TxControlReg));
    Serial.printf("  RFCfgReg:    0x%02X (gain should be 0x70)\n", SoftSPI_ReadRegister(MFRC522::RFCfgReg));
    
    Serial.println("\n[SETUP] Ready to test RFID scanning");
    Serial.println("Place an RFID card near the reader...");
    
    tft.setCursor(10, 100);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("Place card");
    tft.println("near reader");
}

void loop() {
    static uint32_t lastScan = 0;
    static uint32_t scanCount = 0;
    
    // Scan every 500ms
    if (millis() - lastScan < 500) {
        return;
    }
    lastScan = millis();
    scanCount++;
    
    // Blink indicator
    if (scanCount % 2 == 0) {
        tft.fillRect(220, 10, 10, 10, TFT_GREEN);
    } else {
        tft.fillRect(220, 10, 10, 10, TFT_BLACK);
    }
    
    // Try to detect a card using simple register test
    // Write test value to FIFO
    SoftSPI_WriteRegister(MFRC522::FIFOLevelReg, 0x80);  // Flush FIFO
    SoftSPI_WriteRegister(MFRC522::FIFODataReg, 0xAA);   // Write test byte
    
    // Read it back
    byte testRead = SoftSPI_ReadRegister(MFRC522::FIFODataReg);
    
    if (testRead == 0xAA) {
        // Communication working
        static bool lastState = false;
        if (!lastState) {
            Serial.println("[TEST] RFID communication verified - FIFO test passed");
            lastState = true;
        }
    } else {
        Serial.printf("[TEST] RFID communication error - wrote 0xAA, read 0x%02X\n", testRead);
    }
    
    // Check if antenna is detecting field changes (indicates card presence)
    byte status2 = SoftSPI_ReadRegister(MFRC522::Status2Reg);
    if (status2 & 0x08) {  // MFCrypto1On bit indicates card communication
        Serial.println("[DETECT] Card field detected!");
        
        tft.fillRect(0, 140, 240, 60, TFT_GREEN);
        tft.setCursor(10, 150);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.setTextSize(2);
        tft.println("CARD DETECTED!");
        
        delay(1000);
        tft.fillRect(0, 140, 240, 60, TFT_BLACK);
    }
    
    // Display scan counter
    tft.fillRect(10, 220, 220, 20, TFT_BLACK);
    tft.setCursor(10, 220);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.printf("Scans: %lu", scanCount);
}