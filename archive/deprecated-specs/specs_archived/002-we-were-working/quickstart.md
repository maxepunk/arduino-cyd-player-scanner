# Quick Start Guide: CYD Multi-Hardware Testing

**Time Required**: 15 minutes  
**Prerequisites**: Arduino CLI installed, CYD module connected via USB

## Step 1: Initial Setup (2 min)

### 1.1 Connect Your CYD Module
```bash
# Connect CYD via USB cable
# Check Windows COM port (Arduino CLI runs on Windows)
arduino-cli board list
# Example: COM8
```

### 1.2 Verify Arduino CLI
```bash
# Check arduino-cli is installed
arduino-cli version

# Install ESP32 board support if needed
arduino-cli core install esp32:esp32
```

## Step 2: Upload Diagnostic Sketch (3 min)

### 2.1 Compile, Upload and Monitor
```bash
# Navigate to project directory
cd ~/projects/Arduino

# Compile, upload and monitor in one command (replace COM8 with your port)
arduino-cli compile --upload -p COM8 --fqbn esp32:esp32:esp32 test-sketches/hardware-detect && sleep 1 && powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1
```

### 2.3 Expected Output
```
CYD Hardware Detection v1.0
==========================
Detecting display controller...
Display: ST7789 detected
USB Configuration: DUAL_USB (Micro + Type-C)
Backlight GPIO: 27
Touch: IRQ pin GPIO36 detected
RFID pins: SCK=22, MOSI=27, MISO=35, SS=3
SD Card: Detected, 8GB available
Audio: I2S configured

Hardware Variant: DUAL_USB
Ready for component testing
```

## Step 3: Component Testing (5 min)

### 3.1 Test Each Component
In the serial monitor, type these commands:

```
TEST_DISPLAY
# Screen should show color bars

TEST_TOUCH
# Tap screen to test - detects single and double taps (IRQ-only)

TEST_RFID  
# Place RFID card on reader when ready

TEST_SD
# Lists files on SD card

TEST_AUDIO
# Plays test tone (if speaker connected)
```

### 3.2 Run Full Diagnostics
```
DIAG
```

Expected JSON response shows all components:
```json
{
  "hardware": {
    "variant": "DUAL_USB",
    "display": "ST7789",
    "backlight_pin": 27
  },
  "components": {
    "display": {"status": "OK"},
    "touch": {"status": "OK"},
    "rfid": {"status": "OK"},
    "sd_card": {"status": "OK"},
    "audio": {"status": "OK"}
  }
}
```

## Step 4: Upload Main Sketch (3 min)

### 4.1 Compile and Upload Main Sketch
```bash
# Compile and upload the main RFID scanner (replace COM8 with your port)
arduino-cli compile --upload -p COM8 --fqbn esp32:esp32:esp32 CYD_Multi_Compatible && sleep 1 && powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1
```

### 4.2 Verify Operation
1. Screen displays "Ready for RFID"
2. Touch works via tap detection (no calibration needed)
3. Double-tap to access menu (if implemented)
4. System ready for RFID scanning

## Step 5: Test RFID Functionality (2 min)

### 5.1 Prepare Test Card
Place an RFID card/tag near the reader

### 5.2 Expected Behavior
- Beep sound (if audio connected)
- Card UID displays on screen
- Image loads from SD (if exists at /rfid/{uid}/image.bmp)
- Audio plays from SD (if exists at /rfid/{uid}/audio.mp3)

### 5.3 Monitor Serial Output
```
Card detected: UID=04:7C:8A:5A
Loading image: /rfid/047C8A5A/image.bmp
Loading audio: /rfid/047C8A5A/audio.mp3
Display updated
Audio playing...
```

## Troubleshooting Quick Reference

### No Display Output
```
WIRING
# Shows correct wiring diagram for your variant
```

### Touch Not Working  
```
TEST_TOUCH
# Tests IRQ pin tap detection (no calibration available)
```

### RFID Not Reading
```
TEST_RFID
# Check for hardware/wiring issues
```

### Complete System Check
```
DIAG
# Full diagnostic report
```

## Success Criteria Checklist

- [ ] Hardware variant correctly detected (SINGLE_USB or DUAL_USB)
- [ ] All components show "OK" status in diagnostics
- [ ] Display shows clear image without artifacts
- [ ] Touch detects taps reliably (IRQ-only mode)
- [ ] RFID cards scan within 1 second
- [ ] Images load and display when card scanned
- [ ] Audio plays when card scanned (if speaker attached)
- [ ] No wiring changes needed between hardware variants

## Next Steps

1. **If all tests pass**: Your CYD is fully configured and working
2. **If specific component fails**: Run `TEST_{COMPONENT}` for detailed diagnostics
3. **If wiring issues suspected**: Run `WIRING` command for your variant's diagram
4. **For different CYD variant**: Same sketch works without modifications!

---
*Quick Start Complete - System Ready for Production Use*