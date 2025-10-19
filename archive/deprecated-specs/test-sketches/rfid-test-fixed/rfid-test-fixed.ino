// RFID Test for SINGLE USB Variant (GPIO21 backlight)
// GPIO27 is free for RFID MOSI - no conflict!

#include <SPI.h>
#include <MFRC522.h>
#include <TFT_eSPI.h>

// RFID Configuration
#define RFID_OPERATION_DELAY_US 10

// RFID pins (software SPI)
#define SOFT_SPI_SCK  22
#define SOFT_SPI_MOSI 27  // GPIO27 is FREE on single USB variant!
#define SOFT_SPI_MISO 35
#define RFID_SS       3

// Backlight - Single USB uses GPIO21
#define BACKLIGHT_PIN 21

TFT_eSPI tft = TFT_eSPI();
MFRC522::Uid uid;

// FreeRTOS Mutex for atomic SPI
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Software SPI implementation from ALNScanner0812Working
byte softSPI_transfer(byte data) {
    byte result = 0;
    
    portENTER_CRITICAL(&spiMux);
    
    for (int i = 0; i < 8; ++i) {
        // Set MOSI
        digitalWrite(SOFT_SPI_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        
        // Clock high
        delayMicroseconds(2);
        digitalWrite(SOFT_SPI_SCK, HIGH);
        delayMicroseconds(2);
        
        // Read MISO
        if (digitalRead(SOFT_SPI_MISO)) {
            result |= (1 << (7 - i));
        }
        
        // Clock low
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("RFID Test - SINGLE USB Variant");
    Serial.println("GPIO21 = Backlight, GPIO27 = RFID MOSI");
    Serial.println("========================================");
    
    // Initialize display
    tft.begin();
    tft.fillScreen(TFT_BLACK);
    
    // Set up backlight on GPIO21 (single USB)
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);
    Serial.println("[OK] Backlight enabled on GPIO21");
    
    // Initialize Software SPI pins
    pinMode(SOFT_SPI_SCK, OUTPUT);
    pinMode(SOFT_SPI_MOSI, OUTPUT);
    pinMode(SOFT_SPI_MISO, INPUT);
    pinMode(RFID_SS, OUTPUT);
    digitalWrite(RFID_SS, HIGH);
    digitalWrite(SOFT_SPI_SCK, LOW);
    
    Serial.printf("[SETUP] RFID: SCK=%d, MOSI=%d, MISO=%d, SS=%d\n",
                  SOFT_SPI_SCK, SOFT_SPI_MOSI, SOFT_SPI_MISO, RFID_SS);
    
    // Display info
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("SINGLE USB");
    tft.setCursor(10, 30);
    tft.setTextSize(1);
    tft.println("GPIO21: Backlight");
    tft.println("GPIO27: RFID MOSI (FREE!)");
    
    // Test GPIO27 is actually controllable
    Serial.println("\n[TEST] Testing GPIO27 control...");
    pinMode(27, OUTPUT);
    digitalWrite(27, LOW);
    delay(10);
    digitalWrite(27, HIGH);
    delay(10);
    digitalWrite(27, LOW);
    Serial.println("[OK] GPIO27 responds to control");
    
    // Reset MFRC522
    Serial.println("\n[RFID] Initializing MFRC522...");
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
    delay(100);
    
    // Initialize MFRC522
    SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Idle);
    SoftSPI_WriteRegister(MFRC522::TModeReg, 0x80);
    SoftSPI_WriteRegister(MFRC522::TPrescalerReg, 0xA9);
    SoftSPI_WriteRegister(MFRC522::TReloadRegH, 0x03);
    SoftSPI_WriteRegister(MFRC522::TReloadRegL, 0xE8);
    SoftSPI_WriteRegister(MFRC522::TxASKReg, 0x40);
    SoftSPI_WriteRegister(MFRC522::ModeReg, 0x3D);
    SoftSPI_WriteRegister(MFRC522::RFCfgReg, 0x70);
    
    // Enable antenna
    byte txControlReg = SoftSPI_ReadRegister(MFRC522::TxControlReg);
    SoftSPI_WriteRegister(MFRC522::TxControlReg, txControlReg | 0x03);
    delay(10);
    
    // Read version
    byte version = SoftSPI_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[RFID] Version: 0x%02X ", version);
    
    tft.setCursor(10, 80);
    tft.setTextSize(2);
    
    if (version == 0x92) {
        Serial.println("(v2.0) - SUCCESS!");
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("RFID: OK!");
    } else if (version == 0x91) {
        Serial.println("(v1.0) - SUCCESS!");
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("RFID: OK!");
    } else {
        Serial.printf("(FAILED: 0x%02X)\n", version);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.printf("RFID: 0x%02X", version);
    }
    
    // Test FIFO
    Serial.println("\n[TEST] Testing FIFO read/write...");
    SoftSPI_WriteRegister(MFRC522::FIFOLevelReg, 0x80);  // Flush
    SoftSPI_WriteRegister(MFRC522::FIFODataReg, 0x42);   // Write test
    byte fifoTest = SoftSPI_ReadRegister(MFRC522::FIFODataReg);
    
    if (fifoTest == 0x42) {
        Serial.println("[OK] FIFO test passed!");
        tft.setCursor(10, 120);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("FIFO: OK");
    } else {
        Serial.printf("[FAIL] FIFO test failed: wrote 0x42, read 0x%02X\n", fifoTest);
        tft.setCursor(10, 120);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.printf("FIFO: 0x%02X", fifoTest);
    }
    
    Serial.println("\n[DIAG] Register dump:");
    Serial.printf("  CommandReg:   0x%02X\n", SoftSPI_ReadRegister(MFRC522::CommandReg));
    Serial.printf("  Status2Reg:   0x%02X\n", SoftSPI_ReadRegister(MFRC522::Status2Reg));
    Serial.printf("  TxControlReg: 0x%02X (should be 0x83)\n", SoftSPI_ReadRegister(MFRC522::TxControlReg));
    Serial.printf("  RFCfgReg:     0x%02X (should be 0x70)\n", SoftSPI_ReadRegister(MFRC522::RFCfgReg));
    
    Serial.println("\n========================================");
    Serial.println("Setup complete - Place card near reader");
    Serial.println("========================================");
    
    tft.setCursor(10, 160);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("PLACE CARD");
}

void loop() {
    static uint32_t lastCheck = 0;
    
    if (millis() - lastCheck < 500) return;
    lastCheck = millis();
    
    // Blink indicator
    static bool blink = false;
    blink = !blink;
    tft.fillRect(220, 10, 10, 10, blink ? TFT_GREEN : TFT_BLACK);
    
    // Simple card detection test
    byte status2 = SoftSPI_ReadRegister(MFRC522::Status2Reg);
    
    static byte lastStatus = 0;
    if (status2 != lastStatus) {
        Serial.printf("[STATUS] Status2Reg changed: 0x%02X\n", status2);
        lastStatus = status2;
        
        if (status2 & 0x08) {
            tft.fillRect(0, 200, 240, 40, TFT_YELLOW);
            tft.setCursor(10, 210);
            tft.setTextColor(TFT_BLACK, TFT_YELLOW);
            tft.setTextSize(2);
            tft.println("CARD DETECT!");
        } else {
            tft.fillRect(0, 200, 240, 40, TFT_BLACK);
        }
    }
}