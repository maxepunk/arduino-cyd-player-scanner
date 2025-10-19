# ESP32-2432S028R Dual USB CYD Configuration & Test Plan

## Updated TFT_eSPI Configuration

The `libraries/TFT_eSPI/User_Setup.h` has been configured with the following settings for your dual-USB CYD:

### Display Driver
- **Driver**: ST7789 (not ILI9341 like single-USB version)
- **Resolution**: 240x320
- **Color Order**: BGR (may need adjustment if colors are wrong)
- **Inversion**: OFF (may need to toggle if display looks inverted)

### Pin Configuration
```cpp
// Display pins (HSPI)
#define TFT_MISO 12  // Display MISO
#define TFT_MOSI 13  // Display MOSI  
#define TFT_SCLK 14  // Display SCK
#define TFT_CS   15  // Display chip select
#define TFT_DC    2  // Display data/command
#define TFT_RST  -1  // Reset tied to ESP32 RST
#define TFT_BL   27  // Backlight (GPIO27 for dual-USB, not GPIO21!)

// Touch pins (XPT2046)
#define TOUCH_CS 33  // Touch chip select
```

## Your Current Wiring (from projectspecs.md)

### MFRC522 RFID Reader Connections:
- **P3 Connector**: GPIO35 (MISO - input only!)
- **CN1 Connector**: GPIO22 (SCK), GPIO27 (MOSI), 3.3V, GND
- **P1 Connector**: RX pin (SDA)

### ✅ CONFLICT RESOLVED:
- **Your CYD uses GPIO21 for backlight** (not GPIO27)
- **GPIO27 is free for RFID MOSI** as per your wiring
- No changes needed to your physical wiring!

## Recommended Wiring Changes

### Option 1: Move RFID MOSI (Recommended)
Change your CN1 wiring:
- Move MFRC522 MOSI from GPIO27 to GPIO21 (now free since backlight moved)
- Keep SCK on GPIO22
- Keep MISO on GPIO35

### Option 2: Use Different Backlight Control
- Disconnect backlight control (always on)
- Or use a different GPIO for backlight

## Test Sketches

### Test 1: Display Basic Test
```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("CYD Dual USB Display Test");
  
  tft.init();
  tft.setRotation(0);
  
  // Test backlight
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH); // Turn on backlight
  
  // Color test
  tft.fillScreen(TFT_RED);
  delay(1000);
  tft.fillScreen(TFT_GREEN);
  delay(1000);
  tft.fillScreen(TFT_BLUE);
  delay(1000);
  
  // Text test
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.println("CYD Dual USB");
  tft.println("ST7789 Display");
  tft.println("Test OK!");
}

void loop() {
  // Nothing
}
```

### Test 2: Touch Calibration
```cpp
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>

#define TOUCH_CS 33
#define TOUCH_IRQ 36

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  ts.begin();
  ts.setRotation(1);
  
  tft.setCursor(0, 0);
  tft.println("Touch Test");
  tft.println("Touch screen to");
  tft.println("see coordinates");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    
    tft.fillRect(0, 60, 240, 40, TFT_BLACK);
    tft.setCursor(0, 60);
    tft.printf("X:%d Y:%d Z:%d", p.x, p.y, p.z);
    
    // Draw a circle where touched (needs calibration)
    int x = map(p.x, 0, 4095, 0, 240);
    int y = map(p.y, 0, 4095, 0, 320);
    tft.fillCircle(x, y, 5, TFT_GREEN);
  }
  delay(50);
}
```

### Test 3: SD Card Test
```cpp
#include <SD.h>
#include <SPI.h>

#define SD_CS 5

void setup() {
  Serial.begin(115200);
  Serial.println("SD Card Test");
  
  SPIClass spi = SPIClass(VSPI);
  spi.begin(18, 19, 23, SD_CS);
  
  if (!SD.begin(SD_CS, spi)) {
    Serial.println("Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void loop() {}
```

### Test 4: RFID Test (AFTER fixing GPIO27 conflict!)
```cpp
#include <SPI.h>
#include <MFRC522.h>

// Adjust these based on your final wiring
#define RST_PIN -1    // Not connected
#define SS_PIN   3    // RX pin (or change based on your wiring)

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  delay(4);
  mfrc522.PCD_DumpVersionToSerial();
  Serial.println("Scan PICC to see UID...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  Serial.print("UID tag :");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();
  
  mfrc522.PICC_HaltA();
}
```

## Testing Procedure

1. **First**: Fix the GPIO27 conflict in your wiring
2. **Run Test 1**: Verify display works and colors are correct
   - If colors are wrong, toggle `TFT_RGB_ORDER` in User_Setup.h
   - If display is inverted, toggle `TFT_INVERSION_ON/OFF`
3. **Run Test 2**: Check touch functionality and calibration
4. **Run Test 3**: Verify SD card access
5. **Run Test 4**: Test RFID after fixing wiring

## Troubleshooting

### Display Issues:
- **No display**: Check backlight GPIO27 is HIGH
- **Wrong colors**: Change `TFT_RGB_ORDER` between `TFT_RGB` and `TFT_BGR`
- **Inverted colors**: Toggle `TFT_INVERSION_ON` or `TFT_INVERSION_OFF`
- **Garbled display**: Reduce `SPI_FREQUENCY` in User_Setup.h

### Touch Issues:
- **No touch response**: Check TOUCH_IRQ on GPIO36
- **Wrong position**: Touch needs calibration for dual-USB variant
- **Intermittent**: May have SPI conflicts with SD card

### RFID Issues:
- **No detection**: Fix GPIO27 conflict first!
- **Intermittent reads**: Software SPI timing may need adjustment

## Next Steps

After successful testing:
1. Port the ALNScanner0812Working code with pin remapping
2. Update software SPI pins for RFID to match your final wiring
3. Adjust touch calibration values
4. Test full integration

Remember: The main differences from single-USB CYD are:
- ST7789 driver (not ILI9341)
- Backlight on GPIO27 (not GPIO21)
- May need color/inversion adjustments
- Touch calibration different