/*
 * Quick SD Card Directory Listing
 * Lists all files and directories on SD card
 */

#include <SD.h>
#include <SPI.h>

#define SD_CS 5

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== SD Card Directory Listing ===\n");

  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR: SD Card Mount Failed");
    while (1) delay(1000);
  }

  Serial.println("SD Card mounted successfully\n");

  // List root directory (3 levels deep)
  listDir(SD, "/", 3);

  // Also try to read config.txt
  Serial.println("\n--- config.txt contents ---");
  File configFile = SD.open("/config.txt");
  if (configFile) {
    while (configFile.available()) {
      Serial.write(configFile.read());
    }
    configFile.close();
  } else {
    Serial.println("Could not open config.txt");
  }
  Serial.println("\n--- end config.txt ---");

  Serial.println("\n=== Complete ===");
}

void loop() {
  delay(1000);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
