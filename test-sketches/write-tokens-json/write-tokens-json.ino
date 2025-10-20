/*
 * Helper: Write tokens.json to SD Card
 * This writes the sample token database to /tokens.json
 */

#include <SD.h>
#include <SPI.h>

#define SD_CS 5

const char* tokensJson = R"JSON({
  "534e2b02": {
    "image": "/images/534e2b02.bmp",
    "audio": "/audio/534E2B02.wav",
    "video": null,
    "processingImage": null,
    "SF_RFID": "534e2b02",
    "SF_ValueRating": 3,
    "SF_MemoryType": "Technical",
    "SF_Group": ""
  },
  "534e2b03": {
    "image": null,
    "audio": null,
    "video": "test_30sec.mp4",
    "processingImage": "/images/placeholder.bmp",
    "SF_RFID": "534e2b03",
    "SF_ValueRating": 3,
    "SF_MemoryType": "Technical",
    "SF_Group": ""
  },
  "tac001": {
    "image": "/images/tac001.bmp",
    "audio": "/audio/tac001.wav",
    "video": null,
    "processingImage": null,
    "SF_RFID": "tac001",
    "SF_ValueRating": 1,
    "SF_MemoryType": "Personal",
    "SF_Group": ""
  },
  "kaa001": {
    "image": "/images/kaa001.bmp",
    "audio": "/audio/kaa001.wav",
    "video": null,
    "processingImage": null,
    "SF_RFID": "kaa001",
    "SF_ValueRating": 1,
    "SF_MemoryType": "Personal",
    "SF_Group": ""
  },
  "asm001": {
    "image": "/images/placeholder.bmp",
    "audio": "/audio/asm001.wav",
    "video": null,
    "processingImage": null,
    "SF_RFID": "asm001",
    "SF_ValueRating": 3,
    "SF_MemoryType": "Personal",
    "SF_Group": "Marcus Sucks (x2)"
  },
  "rat001": {
    "image": "/images/placeholder.bmp",
    "audio": "/audio/rat001.wav",
    "video": null,
    "processingImage": null,
    "SF_RFID": "rat001",
    "SF_ValueRating": 4,
    "SF_MemoryType": "Business",
    "SF_Group": "Marcus Sucks(x2)"
  }
})JSON";

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== Writing tokens.json to SD Card ===\n");

  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR: SD Card Mount Failed");
    while (1) delay(1000);
  }

  Serial.println("SD Card mounted");
  Serial.printf("Writing %d bytes to /tokens.json...\n", strlen(tokensJson));

  File file = SD.open("/tokens.json", FILE_WRITE);
  if (!file) {
    Serial.println("ERROR: Could not open /tokens.json for writing");
    while (1) delay(1000);
  }

  size_t written = file.print(tokensJson);
  file.close();

  Serial.printf("Successfully wrote %d bytes\n", written);
  Serial.println("\nVerifying...");

  // Verify by reading back
  file = SD.open("/tokens.json", FILE_READ);
  if (file) {
    Serial.printf("File size: %d bytes\n", file.size());
    Serial.println("\nFirst 200 characters:");
    for (int i = 0; i < 200 && file.available(); i++) {
      Serial.write(file.read());
    }
    Serial.println("\n\n=== SUCCESS ===");
    file.close();
  } else {
    Serial.println("ERROR: Could not read back file");
  }
}

void loop() {
  delay(1000);
}
