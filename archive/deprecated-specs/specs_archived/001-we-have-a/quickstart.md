# CYD Multi-Model Compatibility Quick Start Guide

**Version**: 1.0  
**Date**: 2025-09-18  
**Sketch**: CYD_Multi_Compatible.ino

## Prerequisites

### Hardware Required
- Cheap Yellow Display (CYD) ESP32-2432S028R (any variant)
  - Single USB (micro) with ILI9341 display, OR
  - Dual USB (micro + Type-C) with ST7789 display
- MFRC522 RFID reader (connected as per wiring below)
- MicroSD card (optional but recommended)
- RFID cards/tags for testing

### Software Required
- Arduino IDE 1.8.19+ or 2.x
- ESP32 Board Package (via Board Manager)
- Required Libraries:
  - TFT_eSPI
  - MFRC522
  - XPT2046_Touchscreen
  - ESP8266Audio

### Existing Wiring Configuration
**IMPORTANT**: NO WIRING CHANGES REQUIRED

```
RFID Reader Connections:
- SCK  → GPIO22 (CN1 connector)
- MOSI → GPIO27 (CN1 connector)  
- MISO → GPIO35 (P3 connector)
- SS   → GPIO3/RX (P1 connector)
- 3.3V → 3.3V
- GND  → GND

SD Card (built-in slot):
- Automatically connected via VSPI

Touch Controller (built-in):
- Automatically connected
```

## Installation Steps

### 1. Prepare Arduino IDE

```bash
# Install ESP32 board support
Arduino IDE → Tools → Board → Boards Manager
Search: "esp32"
Install: "esp32 by Espressif Systems"

# Install required libraries
Arduino IDE → Tools → Manage Libraries
Install:
- TFT_eSPI by Bodmer
- MFRC522 by GithubCommunity  
- XPT2046_Touchscreen by Paul Stoffregen
- ESP8266Audio by Earle Philhower
```

### 2. Upload the Sketch

1. Open `CYD_Multi_Compatible.ino` in Arduino IDE
2. Select board: `Tools → Board → ESP32 Arduino → ESP32 Dev Module`
3. Configure settings:
   - Flash Size: 4MB
   - Partition Scheme: Default 4MB with spiffs
   - Upload Speed: 921600
   - Port: [Your COM/USB port]
4. Click Upload button

### 3. Initial Boot & Hardware Detection

Open Serial Monitor (115200 baud) to see:

```
=== CYD Multi-Model Compatibility System ===
[100][INFO][HARDWARE/Detector]: Starting hardware detection...
[150][INFO][HARDWARE/Detector]: Testing display driver...
[200][INFO][HARDWARE/Detector]: Detected ST7789 display (ID: 0x8552)
[250][INFO][HARDWARE/Detector]: Detected CYD Dual USB model
[300][INFO][HARDWARE/Detector]: Backlight on GPIO27
[350][INFO][DISPLAY/Init]: Initializing ST7789 240x320...
[400][SUCCESS][DISPLAY/Init]: Display initialized successfully
[450][INFO][TOUCH/Init]: Initializing XPT2046 touch controller...
[500][SUCCESS][TOUCH/Init]: Touch initialized with stored calibration
[550][INFO][RFID/Init]: Initializing MFRC522 on software SPI...
[600][SUCCESS][RFID/Init]: RFID reader firmware: 0x92
[650][INFO][SDCARD/Init]: Initializing SD card...
[700][SUCCESS][SDCARD/Init]: SD card detected: 8GB
[750][INFO][AUDIO/Init]: Initializing I2S audio...
[800][SUCCESS][AUDIO/Init]: Audio system ready
[850][INFO][SYSTEM]: All components initialized successfully
[900][INFO][SYSTEM]: Ready for operation
```

## Verification Tests

### Test 1: Display Verification

The display should show:
- Color bars (Red, Green, Blue) for 2 seconds
- System information screen with:
  - Detected model type
  - Component status indicators
  - "READY" message

### Test 2: Touch Calibration

1. Touch the screen when prompted
2. If first use on this model:
   - Follow calibration targets (4 corners + center)
   - Calibration auto-saves to EEPROM
3. Test touch by tapping menu items

### Test 3: RFID Reading

1. Place RFID card on reader
2. Serial monitor shows:
```
[5000][INFO][RFID/Reader]: Card detected
[5050][INFO][RFID/Reader]: UID: 04 5B 2C 8A
[5100][INFO][RFID/NDEF]: Text record: "Hello CYD"
```
3. Display shows card UID and NDEF text

### Test 4: SD Card Access

1. Insert formatted SD card with test.bmp file
2. System automatically detects and displays:
```
[8000][INFO][SDCARD/File]: Loading test.bmp...
[8100][SUCCESS][SDCARD/File]: Image displayed
```

### Test 5: Audio Playback

1. Place card with audio tag
2. System plays associated WAV file:
```
[10000][INFO][AUDIO/Player]: Playing sound.wav...
[12000][INFO][AUDIO/Player]: Playback complete
```

## Troubleshooting

### Issue: "Display not working"

```
Check serial output for:
[ERROR][DISPLAY/Init]: No display detected

Solutions:
1. Verify display cable connection
2. Check if backlight is on (GPIO21 or GPIO27 HIGH)
3. Try manual model selection (hold touch during boot)
```

### Issue: "Wrong colors on display"

```
Symptom: Colors inverted or wrong (blue shows as yellow)

Fix: Color order auto-detected, but if wrong:
1. Note model detected in serial output  
2. System will try alternate color mode
3. Automatic correction saved to EEPROM
```

### Issue: "RFID not reading"

```
Common causes and fixes:

1. GPIO27 Conflict (Dual USB models):
[WARNING][RFID/SPI]: GPIO27 conflict detected
→ System automatically handles multiplexing

2. Wiring Issue:
[ERROR][RFID/SPI]: No response on SPI bus
SUGGESTION: Check MISO connection on GPIO35
SUGGESTION: Verify 3.3V power to RFID module

3. Timing Issue:
[WARNING][RFID/SPI]: Communication errors: 5
→ System auto-adjusts SPI timing
```

### Issue: "Touch not responding"

```
Diagnostics:
[ERROR][TOUCH/IRQ]: No interrupt on GPIO36

Solutions:
1. Check touch IRQ pin (GPIO36)
2. Recalibrate: Hold top-left corner during boot
3. Check serial for touch coordinates
```

### Issue: "Unknown model detected"

```
[WARNING][HARDWARE/Detector]: Unknown CYD variant
[INFO][HARDWARE/Detector]: Falling back to safe mode

System will:
1. Default to ILI9341 driver
2. Test both backlight pins
3. Provide detailed diagnostics
4. Request manual configuration
```

## Advanced Diagnostics

### Enable Verbose Mode

Hold bottom-right corner during boot for verbose diagnostics:

```
[DEBUG][SPI/Software]: Bit 0: MOSI=1, MISO=0, CLK=↑
[DEBUG][SPI/Software]: Bit 1: MOSI=0, MISO=1, CLK=↓
[DEBUG][MEMORY]: Free heap: 145632 bytes
[DEBUG][PERFORMANCE]: Loop time: 23ms
```

### Pin State Report

Enter 'P' in serial monitor for pin report:

```
PIN[21] MODE[OUTPUT] STATE[HIGH] EXPECT[HIGH] DESC[Backlight]
PIN[27] MODE[OUTPUT] STATE[LOW] EXPECT[LOW] DESC[RFID MOSI]
PIN[35] MODE[INPUT] STATE[HIGH] EXPECT[FLOAT] DESC[RFID MISO]
...
```

### System State Dump

Enter 'D' in serial monitor for complete state:

```
=== SYSTEM STATE DUMP ===
Model: CYD_DUAL_USB
Display: ST7789 @ 40MHz SPI
Touch: Calibrated, Last: (120, 200)
RFID: Active, Reads: 42, Errors: 0
SD: 7.4GB free of 8GB
Audio: Idle
Memory: 145KB free, 12% fragmented
Uptime: 00:05:23
===
```

## Performance Expectations

- **Boot Time**: < 3 seconds to ready
- **Touch Response**: < 50ms latency
- **RFID Read Time**: < 100ms for standard cards
- **Display Refresh**: 30 fps for animations
- **Audio**: 44.1kHz stereo playback

## Support

### Debug Output Interpretation

Format: `[timestamp][LEVEL][CATEGORY/Component]: Message`

Levels:
- DEBUG: Detailed diagnostic info
- INFO: Normal operation events
- WARNING: Non-critical issues
- ERROR: Component failures
- CRITICAL: System-wide problems

### Reporting Issues

Include in bug reports:
1. Complete serial output from boot
2. Hardware model (single/dual USB)
3. Result of system state dump ('D' command)
4. Wiring verification against specification

## Next Steps

1. ✅ Verify all components working
2. ✅ Test with your RFID cards
3. ✅ Load BMP images on SD card
4. ✅ Add WAV files for audio playback
5. 🎉 Deploy unified sketch across all CYDs!

---
*End of Quick Start Guide*