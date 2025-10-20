# CLAUDE.md - CYD RFID Scanner Arduino Project

## 🎯 PROJECT STATUS: DEVELOPMENT IN PROGRESS
**Last Updated:** October 19, 2025
**Platform:** Raspberry Pi (Native Linux Arduino CLI)
**Current Work:** Orchestrator integration (specs/003-orchestrator-hardware-integration)
**Production Sketch:** v3.4 stable, v4.0 in development

---

## 📁 SOURCE OF TRUTH

### ✅ **PRODUCTION SKETCH (USE THIS!)**
```
ALNScanner0812Working/
└── ALNScanner0812Working.ino  # v3.4 - PRODUCTION READY
```

**Version:** 3.4
**Status:** ✅ All issues resolved (SPI deadlock, display orientation, color inversion, RFID beeping)
**Last Commit:** Sept 20, 2025 - `7f2b3f6` - "reduce RFID scanning beeping to acceptable levels"

**What it does:**
- RFID card scanning with NDEF text extraction
- BMP image display from SD card based on card UID
- Works on CYD ST7789 displays (Cheap Yellow Display)
- Audio playback via I2S
- Serial debugging interface

### 📚 **DOCUMENTATION (REFERENCE)**
- `CLAUDE.md` - This file (project overview)
- `CYD_COMPATIBILITY_STATUS.md` - Hardware variant details
- `HARDWARE_SPECIFICATIONS.md` - Pin configurations
- `TFT_ESPI_QUICK_REFERENCE.md` - Display library quick ref
- `specs/001-we-are-trying/` - ST7789 fix implementation docs
- `specs/002-audio-debug-project/` - RFID beeping fix docs

### 🧪 **TEST SKETCHES (DEVELOPMENT & DIAGNOSTIC TOOLS)**
```
test-sketches/
├── 01-display-hello/       # Display test
├── 02-display-colors/      # Color test
├── 03-touch-irq/           # Touch detection
├── 07-rfid-init/           # RFID initialization
├── 13-audio-isolation-test/ # Audio debugging
├── 38-wifi-connect/        # ✅ WiFi connection & config parser (Oct 19, 2025)
├── 39-http-client/         # HTTP client for orchestrator API
├── 40-json-parse/          # ArduinoJson token database parsing
├── 41-queue-file/          # Offline queue (JSONL) operations
├── 42-background-sync/     # FreeRTOS background task & queue sync
└── ... [42+ diagnostic sketches]
```

**Note:** Test sketches 38-42 are full development units with production-ready code for orchestrator integration (see DEVELOPMENT METHODOLOGY).

### 🗄️ **ARCHIVE (DEPRECATED - DO NOT USE)**
```
archive/
├── CYD_Multi_ARCHIVED/         # ❌ Abandoned modular implementation
├── deprecated-scripts/          # WSL2/Windows-specific scripts
├── deprecated-docs/             # Old implementation docs
├── deprecated-specs/            # Old spec folders
└── ALNScanner0812Working.backup.ino  # Pre-beeping-fix backup
```

---

## 🔧 DEVELOPMENT ENVIRONMENT

### **Current Platform: Raspberry Pi (Native Linux)**
- **OS:** Debian 12 (Bookworm) on Raspberry Pi 5
- **Arduino CLI:** v1.3.1 (native Linux, installed at `~/bin/arduino-cli`)
- **Serial Ports:** `/dev/ttyUSB0`, `/dev/ttyACM0` (native Linux devices)
- **Libraries:** `~/projects/Arduino/libraries/` (project-local)

### **Migration from WSL2 (Deprecated)**
⚠️ **The WSL2-Windows bridge setup is DEPRECATED.** All WSL-specific scripts and documentation have been moved to `archive/deprecated-scripts/` and `archive/deprecated-docs/`.

**Benefits of Pi over WSL2:**
- ✅ Native Arduino CLI (no Windows bridge needed)
- ✅ Native serial ports (`/dev/ttyUSB0` instead of COM ports)
- ✅ Native serial monitor (`arduino-cli monitor` works directly)
- ✅ No file system translation overhead
- ✅ Direct USB connection to ESP32

---

## 🎨 HARDWARE CONFIGURATION

### **Target Hardware:** CYD 2.8" ESP32 Display (ST7789 Variant)

**Display:** ST7789, 240x320, Dual USB (Micro + Type-C)
**MCU:** ESP32 (240MHz dual-core)
**Touch:** XPT2046 resistive (IRQ tap detection only)
**Backlight:** GPIO21

### **Pin Configuration (CRITICAL - DO NOT CHANGE)**
```cpp
// RFID (Software SPI - MFRC522)
SCK  = GPIO22
MOSI = GPIO27  // ⚠️ Electrically coupled to speaker, causes beeping
MISO = GPIO35
SS   = GPIO3

// SD Card (Hardware SPI - VSPI)
SCK  = GPIO18
MOSI = GPIO23
MISO = GPIO19
CS   = GPIO5

// Touch
IRQ  = GPIO36  // Tap detection only, no coordinates

// Audio (I2S)
BCLK = GPIO26
LRC  = GPIO25
DIN  = GPIO22
```

---

## ⚙️ COMPILATION & UPLOAD

### **Standard Workflow (Raspberry Pi)**

```bash
# Navigate to sketch
cd ~/projects/Arduino/ALNScanner0812Working

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 .

# Upload (ESP32 connected to Pi via USB)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Monitor serial output
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### **Board Configuration**
- **FQBN:** `esp32:esp32:esp32`
- **Partition Scheme:** `default`
- **Upload Speed:** `921600`
- **Baud Rate:** `115200` (serial monitor)

### **Test Sketch Compilation (with project-local libraries)**

```bash
# Compile test sketch with explicit library paths
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/MFRC522 \
  --library ~/projects/Arduino/libraries/ESP8266Audio \
  --library ~/projects/Arduino/libraries/XPT2046_Touchscreen \
  test-sketches/38-wifi-connect

# Upload and test
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 test-sketches/38-wifi-connect
```

---

## 🛠️ DEVELOPMENT METHODOLOGY

### **Test-First Hardware Development (CRITICAL)**

**Principle:** Test sketches are NOT validation artifacts - they ARE the implementation.

When adding new hardware features:

1. **Create instrumented test sketch FIRST** in `test-sketches/`
   - Full serial debugging with timestamps `[millis()] Event description`
   - Statistics tracking (attempts, success rate, failures)
   - Interactive serial commands (STATUS, TEST, HELP)
   - Memory usage monitoring (`ESP.getFreeHeap()`)

2. **Validate on physical hardware IMMEDIATELY**
   - Upload to CYD device
   - Test via serial monitor commands
   - Verify edge cases (disconnect, timeout, error handling)
   - **DO NOT proceed until test passes on hardware**

3. **Extract production code from validated test**
   - Test sketch contains production-ready, proven code
   - Direct integration into `ALNScanner0812Working.ino`
   - Retain test sketch for future regression testing

### **Serial Instrumentation Pattern**

```cpp
// Statistics tracking
int attempts = 0;
int successes = 0;
unsigned long startTime = 0;

// Event logging with timestamp
Serial.printf("[%lu] EVENT: Connection attempt #%d\n", millis(), attempts);
Serial.printf("        Details: %s\n", details);

// Success metrics
Serial.printf("        Success rate: %d/%d (%.1f%%)\n",
              successes, attempts,
              100.0 * successes / attempts);

// Interactive commands in loop()
if (Serial.available()) {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd == "STATUS") printStatus();
  else if (cmd == "HELP") printHelp();
}
```

### **Why This Matters**

- **ESP32 hardware differs from simulation**: WiFi timing, SPI behavior, memory constraints
- **Integration bugs are expensive**: Finding issues in 3000-line production sketch vs 200-line test
- **Test sketches become living documentation**: Future developers see exact working implementation
- **Instrumentation aids debugging**: Real-time diagnostics beat guessing every time

### **Example: WiFi Connection Test (Oct 19, 2025)**

**Test sketch:** `test-sketches/38-wifi-connect/`

**Results on hardware:**
- Connection time: 1196 ms
- Signal strength: -70 dBm
- Success rate: 100% (2/2 attempts)
- Auto-reconnect: WORKING
- Free heap: 204 KB

**Interactive commands added:**
- `STATUS` - Full connection info, statistics, memory
- `RECONNECT` - Force disconnection and reconnect test
- `MEM` - Heap usage

**Code is now production-ready** - directly portable to main sketch.

---

## 🧪 SERIAL DEBUGGING INTERFACE

### **Available Commands (115200 baud)**
- `DIAG` - Full system diagnostics (JSON output)
- `TEST_DISPLAY` - Test display with color bars
- `TEST_TOUCH` - Touch IRQ tap detection test
- `TEST_RFID` - RFID module test
- `TEST_SD` - SD card test
- `TEST_AUDIO` - Audio output test
- `SCAN` - Force RFID scan attempt
- `VERSION` - Firmware version
- `WIRING` - Show wiring diagram

---

## 🔥 CRITICAL IMPLEMENTATION FIXES (RESOLVED)

### **1. SPI Bus Deadlock** ✅ FIXED (Sept 20, 2025)
**Problem:** Holding TFT lock while accessing SD card caused system freeze
**Root Cause:** VSPI shared between TFT and SD
**Solution:** Read from SD **FIRST**, then lock TFT

```cpp
// ✅ CORRECT
f.read(buffer, size);      // SD read first
tft.startWrite();          // Then lock TFT
tft.setAddrWindow(x, y, w, h);
tft.endWrite();

// ❌ WRONG - CAUSES DEADLOCK
tft.startWrite();
f.read(buffer, size);  // DEADLOCK HERE!
```

### **2. BMP Display Orientation** ✅ FIXED (Sept 20, 2025)
**Problem:** BMP images displayed upside down
**Root Cause:** BMPs store pixels bottom-to-top
**Solution:** Position each row individually

```cpp
for (int y = height - 1; y >= 0; y--) {
    f.read(rowBuffer, rowBytes);
    tft.startWrite();
    tft.setAddrWindow(0, y, width, 1);  // Position EACH row
    tft.endWrite();
}
```

### **3. ST7789 Color Inversion** ✅ FIXED (Sept 20, 2025)
**Problem:** Inverted colors on ST7789 displays
**Root Cause:** TFT_eSPI library hardcodes `ST7789_INVON` command
**Solution:** Modified `TFT_eSPI/TFT_Drivers/ST7789_Init.h` line 102

**File Modified:**
```
~/projects/Arduino/libraries/TFT_eSPI/TFT_Drivers/ST7789_Init.h
```

**Change:**
```cpp
// Line 102 - COMMENTED OUT
// writecommand(ST7789_INVON);  // Color inversion NOT needed for CYD
```

### **4. RFID Scanning Beeping** ✅ FIXED (Sept 20, 2025)
**Problem:** Continuous beeping during RFID scanning
**Root Cause:** Electrical coupling between GPIO27 (MOSI) and speaker circuit (hardware flaw)
**Solution:** Reduced scan frequency and deferred audio initialization

**Changes:**
- Scan interval: 100ms → 500ms (5x reduction, 80% less beeping)
- Defer audio initialization until first use
- Keep MOSI LOW between scans

---

## 📚 ACTIVE TECHNOLOGIES

### **Core Libraries (Project-Local)**
- **TFT_eSPI** - Display driver (MODIFIED for ST7789 fix)
- **MFRC522** - RFID reader (software SPI)
- **ESP8266Audio** - I2S audio playback
- **SD** - SD card interface (hardware SPI)
- **XPT2046_Touchscreen** - Touch detection

### **Board Support**
- **esp32:esp32@3.3.2** - ESP32 core for Arduino

---

## 🗺️ PROJECT EVOLUTION

### **Timeline**
1. **Sept 18, 2025** - Initial commit, started modular `CYD_Multi_Compatible`
2. **Sept 19, 2025** - Discovered SPI bus conflict root cause
3. **Sept 20, 2025 AM** - Fixed SPI deadlock, BMP orientation, color inversion
4. **Sept 20, 2025 PM** - Fixed RFID beeping, merged into `ALNScanner0812Working`
5. **Oct 18, 2025** - Migrated from WSL2 to Raspberry Pi

### **Key Decisions**
- **Abandoned modular approach:** `CYD_Multi_Compatible` proved too complex, reverted to proven monolithic sketch
- **Migrated to Pi:** Eliminated WSL2 complexity, now using native Linux Arduino CLI
- **Archived deprecated code:** Moved all WSL-specific and experimental code to `archive/`

---

## 🎯 TESTING REQUIREMENTS

### **Hardware Tests**
- ✅ Display: Color bars, text rendering, BMP display
- ✅ Touch: IRQ tap detection (coordinates not used)
- ✅ RFID: Card detection, UID reading, NDEF parsing
- ✅ SD Card: File listing, BMP loading
- ✅ Audio: WAV playback via I2S

### **Validation**
- No component failure should crash system
- Graceful degradation on missing hardware
- Comprehensive serial diagnostics available
- All serial commands functional

---

## 📂 DIRECTORY STRUCTURE

```
~/projects/Arduino/
├── ALNScanner0812Working/          # ✅ PRODUCTION SKETCH
│   └── ALNScanner0812Working.ino   # v3.4 - USE THIS!
│
├── libraries/                       # Project-local libraries
│   ├── TFT_eSPI/                   # ⚠️ MODIFIED (ST7789 fix)
│   ├── MFRC522/
│   ├── ESP8266Audio/
│   └── ... [6 libraries total]
│
├── test-sketches/                   # 🧪 Diagnostic tools (37 sketches)
│   ├── 01-display-hello/
│   ├── 07-rfid-init/
│   └── 13-audio-isolation-test/
│
├── specs/                           # 📝 Implementation documentation
│   ├── 001-we-are-trying/          # ST7789 fix docs
│   └── 002-audio-debug-project/    # RFID beeping fix docs
│
├── archive/                         # 🗄️ DEPRECATED - DO NOT USE
│   ├── CYD_Multi_ARCHIVED/         # Abandoned modular sketch
│   ├── deprecated-scripts/          # WSL2/Windows scripts
│   ├── deprecated-docs/             # Old documentation
│   └── deprecated-specs/            # Old spec folders
│
├── CLAUDE.md                        # 📘 This file
├── CYD_COMPATIBILITY_STATUS.md      # Hardware variants
├── HARDWARE_SPECIFICATIONS.md       # Pin configurations
└── TFT_ESPI_QUICK_REFERENCE.md     # Display library reference
```

---

## 🚀 QUICK START

### **1. Connect Hardware**
- Plug ESP32 CYD into Raspberry Pi via USB
- Verify port: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`

### **2. Upload Sketch**
```bash
cd ~/projects/Arduino/ALNScanner0812Working
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .
```

### **3. Test**
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Type `DIAG` in serial monitor to run full diagnostics.

### **4. Load Content on SD Card**
- Format SD card as FAT32
- Create `/images/` folder
- Add BMP files (24-bit, 240x320): `<UID>.bmp` (e.g., `04A1B2C3.bmp`)
- Add WAV files to `/audio/`

---

## ⚠️ IMPORTANT NOTES

### **Library Modification Required**
The TFT_eSPI library in `~/projects/Arduino/libraries/TFT_eSPI/` **HAS BEEN MODIFIED** to fix ST7789 color inversion. Do NOT reinstall/update this library without reapplying the fix.

**Modified File:**
```
libraries/TFT_eSPI/TFT_Drivers/ST7789_Init.h (line 102)
```

### **Hardware Limitation**
The GPIO27/speaker electrical coupling is a **hardware design flaw** in the CYD board. The software workaround (reduced scan frequency) is the only solution without hardware modification.

### **Migration Notes**
If returning to WSL2 development, restore scripts from `archive/deprecated-scripts/` and follow documentation in `archive/deprecated-docs/WSL2_ARDUINO_SETUP.md`.

---

## 📞 TROUBLESHOOTING

### **Display shows inverted colors**
- Check `libraries/TFT_eSPI/TFT_Drivers/ST7789_Init.h` line 102 is commented out

### **SPI deadlock / system freeze**
- Verify SD reads happen BEFORE `tft.startWrite()`

### **RFID beeping**
- Check scan interval is 500ms (not 100ms)
- Verify audio initialization is deferred

### **No serial output**
- Check baud rate is 115200
- Verify port: `arduino-cli board list`

---

*For detailed fix history, see `specs/001-we-are-trying/tasks.md` and `specs/002-audio-debug-project/`*
