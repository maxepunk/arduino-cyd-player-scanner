// Test 20: SD Card Contents Checker
// Purpose: List all files on the SD card to verify what exists
// Focus: See if /IMG/534E2B02.bmp actually exists

#include <SD.h>
#include <SPI.h>

// SD Card pins (Hardware VSPI)
#define SD_CS       5
#define SDSPI_SCK   18
#define SDSPI_MISO  19
#define SDSPI_MOSI  23

SPIClass SDSPI(VSPI);

void printDirectory(File dir, int numTabs = 0) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // No more files
            break;
        }
        
        // Print indentation
        for (uint8_t i = 0; i < numTabs; i++) {
            Serial.print("  ");
        }
        
        // Print name
        Serial.print(entry.name());
        
        if (entry.isDirectory()) {
            Serial.println("/");
            printDirectory(entry, numTabs + 1);
        } else {
            // Print size
            Serial.print(" (");
            Serial.print(entry.size());
            Serial.println(" bytes)");
        }
        entry.close();
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\nTEST20: SD CARD CONTENTS CHECKER");
    Serial.println("=================================\n");
    
    // Initialize SD card
    Serial.println("Initializing SD card...");
    SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI);
    
    if (!SD.begin(SD_CS, SDSPI)) {
        Serial.println("SD Card initialization FAILED!");
        return;
    }
    
    Serial.println("SD Card initialized successfully.\n");
    
    // Get card info
    uint8_t cardType = SD.cardType();
    Serial.print("Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else if (cardType == CARD_UNKNOWN) {
        Serial.println("UNKNOWN");
    } else {
        Serial.println("NONE");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("Card Size: %lluMB\n", cardSize);
    
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    Serial.printf("Total: %llu bytes, Used: %llu bytes\n\n", totalBytes, usedBytes);
    
    // List all files
    Serial.println("=== COMPLETE FILE LISTING ===");
    File root = SD.open("/");
    printDirectory(root);
    root.close();
    
    Serial.println("\n=== CHECKING SPECIFIC PATHS ===");
    
    // Check for specific directories
    const char* checkDirs[] = {"/IMG", "/AUDIO", "/IMG/"};
    for (int i = 0; i < 3; i++) {
        Serial.printf("Checking directory '%s': ", checkDirs[i]);
        File dir = SD.open(checkDirs[i]);
        if (dir) {
            if (dir.isDirectory()) {
                Serial.println("EXISTS (directory)");
                
                // List contents
                Serial.println("  Contents:");
                File entry;
                int count = 0;
                while ((entry = dir.openNextFile())) {
                    Serial.print("    - ");
                    Serial.print(entry.name());
                    if (!entry.isDirectory()) {
                        Serial.print(" (");
                        Serial.print(entry.size());
                        Serial.print(" bytes)");
                    }
                    Serial.println();
                    entry.close();
                    count++;
                }
                if (count == 0) {
                    Serial.println("    [EMPTY DIRECTORY]");
                }
            } else {
                Serial.println("EXISTS (but is a file!)");
            }
            dir.close();
        } else {
            Serial.println("DOES NOT EXIST");
        }
    }
    
    // Check for the specific file we're looking for
    Serial.println("\n=== CHECKING TARGET FILE ===");
    const char* targetFile = "/IMG/534E2B02.bmp";
    Serial.printf("Looking for: %s\n", targetFile);
    
    File f = SD.open(targetFile);
    if (f) {
        Serial.printf("  FOUND! Size: %d bytes\n", f.size());
        
        // Read first few bytes
        uint8_t header[10];
        size_t bytesRead = f.read(header, 10);
        Serial.printf("  First %d bytes: ", bytesRead);
        for (int i = 0; i < bytesRead; i++) {
            Serial.printf("%02X ", header[i]);
        }
        Serial.println();
        
        f.close();
    } else {
        Serial.println("  NOT FOUND!");
        
        // Try variations
        Serial.println("\nTrying variations:");
        const char* variations[] = {
            "/IMG/534E2B02.BMP",
            "/img/534E2B02.bmp",
            "/534E2B02.bmp",
            "/IMG/534e2b02.bmp"
        };
        
        for (int i = 0; i < 4; i++) {
            Serial.printf("  Trying '%s': ", variations[i]);
            File test = SD.open(variations[i]);
            if (test) {
                Serial.println("FOUND!");
                test.close();
            } else {
                Serial.println("Not found");
            }
        }
    }
    
    Serial.println("\n=== TEST COMPLETE ===");
}

void loop() {
    // Nothing to do
    delay(5000);
}