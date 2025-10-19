// Test exact RFID initialization from ALNScanner
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// Exact same setup as ALNScanner
#define SOFT_SPI_SCK  22
#define SOFT_SPI_MOSI 27
#define SOFT_SPI_MISO 35
#define RFID_SS       3
#define RFID_OPERATION_DELAY_US 10

TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
MFRC522::Uid uid;
static portMUX_TYPE spiMux = portMUX_INITIALIZER_UNLOCKED;

// Simplified software SPI transfer (just the structure)
byte softSPI_transfer(byte data) {
    byte result = 0;
    portENTER_CRITICAL(&spiMux);
    // Simulate SPI transfer
    for (int i = 0; i < 8; ++i) {
        digitalWrite(SOFT_SPI_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        delayMicroseconds(2);
        digitalWrite(SOFT_SPI_SCK, HIGH);
        delayMicroseconds(2);
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
  Serial.println("\n=== RFID Init Test ===");
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("RFID Init Test");
  
  Serial.println("Before RFID pin init...");
  
  // Initialize Software SPI pins for RFID
  pinMode(SOFT_SPI_SCK, OUTPUT);
  pinMode(SOFT_SPI_MOSI, OUTPUT);
  pinMode(SOFT_SPI_MISO, INPUT);
  pinMode(RFID_SS, OUTPUT);
  digitalWrite(RFID_SS, HIGH);
  digitalWrite(SOFT_SPI_SCK, LOW);
  
  Serial.println("After pin init, before soft reset...");
  
  // Soft reset MFRC522
  SoftSPI_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
  delay(100);
  
  Serial.println("After soft reset, reading version...");
  
  // Read version like ALNScanner does
  byte version = SoftSPI_ReadRegister(MFRC522::VersionReg);
  Serial.printf("MFRC522 Version: 0x%02X\n", version);
  
  Serial.println("RFID init complete!");
}

void loop() {
  Serial.println("RFID initialized, serial still works!");
  delay(1000);
}