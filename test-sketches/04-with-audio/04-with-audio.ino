// Test 04: Add audio libraries like ALNScanner
// Purpose: Test if ESP8266Audio breaks serial

#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// All the globals like ALNScanner
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
MFRC522::Uid uid;

// Audio pointers
AudioGeneratorWAV *wav = nullptr;
AudioFileSourceSD *file = nullptr;
AudioOutputI2S *out = nullptr;

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== TEST04: With Audio Libraries ===");
    Serial.println("Testing full include set like ALNScanner");
    
    // Display init
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.println("TEST04");
    Serial.println("Display initialized");
    
    // SD init
    SDSPI.begin(18, 19, 23);
    if (!SD.begin(5, SDSPI)) {
        Serial.println("SD init failed (expected if no card)");
    } else {
        Serial.println("SD initialized");
    }
    
    // RFID pins including GPIO3
    pinMode(22, OUTPUT);
    pinMode(27, OUTPUT);
    pinMode(35, INPUT);
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);
    Serial.println("RFID pins configured including GPIO3");
    
    // Audio init
    Serial.println("Initializing audio...");
    out = new AudioOutputI2S(0, 1);
    Serial.println("Audio initialized");
    
    Serial.println("=== SETUP COMPLETE ===");
}

void loop() {
    static int count = 0;
    Serial.print("Loop ");
    Serial.print(count++);
    Serial.println(": All systems running");
    delay(2000);
}