// SD Card Test Sketch for CYD
// Tests SD card operations on hardware SPI

#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <FS.h>

// SD Card pins (hardware SPI)
#define SD_CS  5
#define SD_SCK  18
#define SD_MOSI 23
#define SD_MISO 19

// Backlight pins
#define BACKLIGHT_PIN_SINGLE 21
#define BACKLIGHT_PIN_DUAL   27

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nCYD SD Card Test v1.0");
  Serial.println("=====================");
  
  // Initialize display
  tft.begin();
  
  // Enable backlight (try both pins)
  pinMode(BACKLIGHT_PIN_SINGLE, OUTPUT);
  pinMode(BACKLIGHT_PIN_DUAL, OUTPUT);
  digitalWrite(BACKLIGHT_PIN_SINGLE, HIGH);
  digitalWrite(BACKLIGHT_PIN_DUAL, HIGH);
  
  // Display header
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("SD Card Test");
  
  // Initialize SD card
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  Serial.print("Initializing SD card...");
  
  // Configure SPI for SD card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS)) {
    Serial.println(" Failed!");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("SD Init: FAILED");
    
    Serial.println("Troubleshooting:");
    Serial.println("1. Check SD card is inserted");
    Serial.println("2. Check card is formatted as FAT32");
    Serial.println("3. Verify wiring: CS=5, SCK=18, MOSI=23, MISO=19");
    return;
  }
  
  Serial.println(" Success!");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("SD Init: OK");
  
  // Get card info
  uint8_t cardType = SD.cardType();
  tft.setCursor(10, 55);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  Serial.print("Card Type: ");
  switch(cardType) {
    case CARD_MMC:
      Serial.println("MMC");
      tft.println("Type: MMC");
      break;
    case CARD_SD:
      Serial.println("SDSC");
      tft.println("Type: SDSC");
      break;
    case CARD_SDHC:
      Serial.println("SDHC");
      tft.println("Type: SDHC");
      break;
    default:
      Serial.println("Unknown");
      tft.println("Type: Unknown");
  }
  
  // Get card size
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.print("Card Size: ");
  Serial.print(cardSize);
  Serial.println(" MB");
  
  tft.setCursor(10, 70);
  tft.print("Size: ");
  tft.print(cardSize);
  tft.println(" MB");
  
  uint64_t usedBytes = SD.usedBytes() / 1024;
  tft.setCursor(10, 85);
  tft.print("Used: ");
  tft.print(usedBytes);
  tft.println(" KB");
  
  // Test file operations
  testFileOperations();
  
  // List files
  listFiles();
}

void loop() {
  static uint32_t testTimer = 0;
  static int testPhase = 0;
  
  if (millis() - testTimer > 5000) {
    testTimer = millis();
    
    switch(testPhase) {
      case 0:
        // Write speed test
        speedTestWrite();
        break;
        
      case 1:
        // Read speed test
        speedTestRead();
        break;
        
      case 2:
        // Directory test
        directoryTest();
        break;
        
      case 3:
        // RFID directory structure test
        testRFIDStructure();
        break;
    }
    
    testPhase = (testPhase + 1) % 4;
  }
}

void testFileOperations() {
  Serial.println("\n--- File Operations Test ---");
  
  tft.fillRect(0, 110, 240, 50, TFT_BLACK);
  tft.setCursor(10, 110);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Testing file operations...");
  
  // Write test file
  File testFile = SD.open("/test.txt", FILE_WRITE);
  if (testFile) {
    Serial.println("Writing test.txt...");
    testFile.println("CYD SD Card Test");
    testFile.println("Timestamp: " + String(millis()));
    testFile.close();
    
    tft.setCursor(10, 125);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Write: OK");
  } else {
    Serial.println("Failed to open test.txt for writing");
    tft.setCursor(10, 125);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Write: FAILED");
  }
  
  // Read test file
  testFile = SD.open("/test.txt");
  if (testFile) {
    Serial.println("Reading test.txt:");
    tft.setCursor(10, 140);
    
    while (testFile.available()) {
      String line = testFile.readStringUntil('\n');
      Serial.println("  " + line);
    }
    testFile.close();
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Read: OK");
  } else {
    Serial.println("Failed to open test.txt for reading");
    tft.setCursor(10, 140);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Read: FAILED");
  }
}

void listFiles() {
  Serial.println("\n--- Directory Listing ---");
  
  tft.fillRect(0, 160, 240, 100, TFT_BLACK);
  tft.setCursor(10, 160);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("Files:");
  
  File root = SD.open("/");
  int fileCount = 0;
  int yPos = 175;
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    
    fileCount++;
    String fileName = entry.name();
    
    Serial.print(entry.isDirectory() ? "[DIR] " : "[FILE] ");
    Serial.print(fileName);
    Serial.print(" (");
    Serial.print(entry.size());
    Serial.println(" bytes)");
    
    if (yPos < 250) {
      tft.setCursor(10, yPos);
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      
      if (entry.isDirectory()) {
        tft.print("[D] ");
      }
      tft.print(fileName);
      
      if (!entry.isDirectory()) {
        tft.print(" ");
        tft.print(entry.size());
        tft.print("B");
      }
      
      yPos += 10;
    }
    
    entry.close();
  }
  
  root.close();
  
  Serial.print("Total files: ");
  Serial.println(fileCount);
}

void speedTestWrite() {
  Serial.println("\n--- Write Speed Test ---");
  
  tft.fillRect(0, 260, 240, 40, TFT_BLACK);
  tft.setCursor(10, 265);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Testing write speed...");
  
  File testFile = SD.open("/speed.bin", FILE_WRITE);
  if (testFile) {
    byte buffer[512];
    for (int i = 0; i < 512; i++) {
      buffer[i] = i & 0xFF;
    }
    
    uint32_t startTime = millis();
    for (int i = 0; i < 100; i++) {
      testFile.write(buffer, 512);
    }
    uint32_t endTime = millis();
    testFile.close();
    
    float speed = (512.0 * 100.0) / (endTime - startTime);
    Serial.print("Write speed: ");
    Serial.print(speed);
    Serial.println(" KB/s");
    
    tft.setCursor(10, 280);
    tft.print("Write: ");
    tft.print(speed, 1);
    tft.println(" KB/s");
    
    SD.remove("/speed.bin");
  }
}

void speedTestRead() {
  Serial.println("\n--- Read Speed Test ---");
  
  // First create a test file
  File testFile = SD.open("/readtest.bin", FILE_WRITE);
  if (testFile) {
    byte buffer[512];
    for (int i = 0; i < 100; i++) {
      testFile.write(buffer, 512);
    }
    testFile.close();
    
    // Now test read speed
    testFile = SD.open("/readtest.bin");
    if (testFile) {
      uint32_t startTime = millis();
      while (testFile.available()) {
        testFile.read(buffer, 512);
      }
      uint32_t endTime = millis();
      testFile.close();
      
      float speed = (512.0 * 100.0) / (endTime - startTime);
      Serial.print("Read speed: ");
      Serial.print(speed);
      Serial.println(" KB/s");
      
      tft.fillRect(0, 295, 240, 15, TFT_BLACK);
      tft.setCursor(10, 295);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      tft.print("Read: ");
      tft.print(speed, 1);
      tft.println(" KB/s");
    }
    
    SD.remove("/readtest.bin");
  }
}

void directoryTest() {
  Serial.println("\n--- Directory Test ---");
  
  if (!SD.exists("/test_dir")) {
    SD.mkdir("/test_dir");
    Serial.println("Created /test_dir");
  }
  
  File testFile = SD.open("/test_dir/subfile.txt", FILE_WRITE);
  if (testFile) {
    testFile.println("Subdirectory file test");
    testFile.close();
    Serial.println("Created /test_dir/subfile.txt");
  }
}

void testRFIDStructure() {
  Serial.println("\n--- RFID Directory Structure Test ---");
  
  // Create RFID directory structure
  if (!SD.exists("/rfid")) {
    SD.mkdir("/rfid");
    Serial.println("Created /rfid directory");
  }
  
  // Create sample card directory
  String cardUID = "047C8A5A";
  String cardPath = "/rfid/" + cardUID;
  
  if (!SD.exists(cardPath)) {
    SD.mkdir(cardPath);
    Serial.println("Created " + cardPath + " directory");
    
    // Create placeholder files
    File configFile = SD.open(cardPath + "/config.txt", FILE_WRITE);
    if (configFile) {
      configFile.println("Card configuration");
      configFile.println("UID: " + cardUID);
      configFile.close();
      Serial.println("Created config file for card");
    }
  }
}