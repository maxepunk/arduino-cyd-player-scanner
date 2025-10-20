# Arduino CLI & ESP32 CYD Development Workflow Guide

**Platform**: Raspberry Pi 5 (Debian 12 Bookworm)
**Hardware**: ESP32 CYD (ESP32-2432S028R) - Cheap Yellow Display
**Last Updated**: October 19, 2025

---

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [Arduino CLI Basics](#arduino-cli-basics)
3. [Serial Monitoring Tools](#serial-monitoring-tools)
4. [Development Workflows](#development-workflows)
5. [Boot Capture Strategies](#boot-capture-strategies)
6. [Debugging Patterns](#debugging-patterns)
7. [Common Patterns](#common-patterns)
8. [Troubleshooting](#troubleshooting)

---

## Hardware Overview

### ESP32-2432S028R (CYD) Specifications

- **MCU**: ESP32-D0WD-V3 (dual-core 240MHz, 520KB SRAM)
- **Display**: ST7789 2.8" TFT 240x320 (SPI)
- **Touch**: XPT2046 resistive touchscreen
- **USB**: Dual ports (Micro + Type-C), CH340 USB-to-serial converter
- **Typical Serial Port**: `/dev/ttyUSB0` (may be `/dev/ttyACM0`)

### Important Hardware Behaviors

**Auto-Reset on Upload**:
- Arduino-cli triggers hardware reset via RTS/DTR pins after upload
- Device boots immediately after upload completes
- Boot sequence happens in ~1-3 seconds

**Serial Port Exclusivity**:
- Only ONE process can have exclusive access to `/dev/ttyUSB0` at a time
- `arduino-cli upload` requires exclusive access (closes any active monitors)
- `arduino-cli monitor`, `screen`, `minicom` all require exclusive access

**USB-to-Serial Behavior**:
- CH340 converter does NOT reset on serial connection (good for boot capture)
- Serial buffer may drop early boot messages if connection not established

---

## Arduino CLI Basics

### Installation & Configuration

**Verify Installation**:
```bash
arduino-cli version
# Should show v1.3.1 or later
```

**Configuration File**: `~/.arduino15/arduino-cli.yaml`

**ESP32 Core**: Installed at `~/.arduino15/packages/esp32/`

**Project-Local Libraries**: `~/projects/Arduino/libraries/`
- TFT_eSPI (MODIFIED for ST7789 color inversion fix)
- MFRC522, ESP8266Audio, XPT2046_Touchscreen, ArduinoJson

### Core Commands

**Compile**:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/ArduinoJson \
  [sketch-directory]
```

**Upload**:
```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 [sketch-directory]
```

**Monitor**:
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### FQBN Options

**Standard** (default partition):
```
esp32:esp32:esp32
```

**With Options**:
```
esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600
```

### Finding the Serial Port

```bash
# List USB devices
arduino-cli board list

# Or check kernel messages after plugging device
dmesg | tail -20

# Expected output:
# [12345.678] ch341-uart ttyUSB0: ch341-uart converter now attached to ttyUSB0
```

---

## Serial Monitoring Tools

### Option 1: arduino-cli monitor (Built-in)

**Pros**: Integrated, simple, works out of box
**Cons**: Requires exclusive port access, cannot capture boot if started after upload

**Basic Usage**:
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Capture to File**:
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200 | tee output.txt
```

**Quiet Mode** (cleaner output for file capture):
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200 --quiet | tee output.txt
```

**Exit**: `Ctrl+C`

### Option 2: screen (Recommended for Boot Capture)

**Pros**: Does NOT reset device on connect, can be started before upload, detachable
**Cons**: Less user-friendly, requires learning screen commands

**Basic Usage**:
```bash
screen /dev/ttyUSB0 115200
```

**Detach Session**: `Ctrl+A` then `D`

**Reattach**:
```bash
screen -r
```

**List Sessions**:
```bash
screen -ls
```

**Kill Session**:
```bash
screen -X -S [session-id] quit
```

**Capture to File** (logging mode):
```bash
screen -L -Logfile boot-log.txt /dev/ttyUSB0 115200
```

### Option 3: minicom

**Pros**: Feature-rich, can capture to file, does not reset on connect
**Cons**: Requires configuration

**Basic Usage**:
```bash
minicom -D /dev/ttyUSB0 -b 115200
```

**With Capture**:
```bash
minicom -D /dev/ttyUSB0 -b 115200 -C /tmp/capture.txt
```

**Exit**: `Ctrl+A` then `X`

### Option 4: cat/tee (Simple Read-Only)

**Pros**: Simplest possible, good for testing
**Cons**: Cannot send data to device, no configurability

**Basic Usage**:
```bash
cat /dev/ttyUSB0 | tee output.txt
```

**Set Baud Rate First**:
```bash
stty -F /dev/ttyUSB0 115200
cat /dev/ttyUSB0 | tee output.txt
```

---

## Development Workflows

### Workflow 1: Standard Development (No Boot Capture Needed)

**Use When**: Testing code during loop(), not setup()

```bash
# Terminal 1: Compile and upload
cd ~/projects/Arduino/test-sketches/42-background-sync
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/ArduinoJson \
  .

arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Wait for upload to complete

# Terminal 2: Start monitor AFTER upload
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Result**: See serial output from loop(), miss setup() and boot messages

### Workflow 2: Boot Sequence Capture (screen method) ✅ RECOMMENDED

**Use When**: Debugging setup() initialization, WiFi connection, crashes on boot

```bash
# Terminal 1: Start screen FIRST (stays connected through upload)
screen -L -Logfile /tmp/boot-capture.txt /dev/ttyUSB0 115200

# Terminal 2: Upload sketch (triggers reset)
cd ~/projects/Arduino/test-sketches/42-background-sync
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Device resets after upload, screen captures boot sequence
# Terminal 1 shows:
# ets Jun  8 2016 00:22:57
# rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
# [boot messages]
# === Test 42: Background Sync ===
# [your serial output]

# Detach screen: Ctrl+A then D
# Boot log saved to /tmp/boot-capture.txt
```

**Key Insight**: `screen` does NOT reset device on connection (unlike arduino-cli monitor), so starting it before upload captures everything.

### Workflow 3: Interactive Development with Persistent Monitor

**Use When**: Rapid iteration, uploading frequently

```bash
# Terminal 1: Start screen session
screen /dev/ttyUSB0 115200

# Detach: Ctrl+A then D

# Terminal 2: Development loop
cd ~/projects/Arduino/test-sketches/42-background-sync

# Edit code...

arduino-cli compile --fqbn esp32:esp32:esp32 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/ArduinoJson \
  .

# Upload (screen session stays active in background)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Wait 2 seconds for device boot

# Reattach screen to see output
screen -r

# Repeat: Detach, edit, compile, upload, reattach
```

### Workflow 4: One-Shot Upload + Monitor

**Use When**: Quick test, don't care about boot messages

```bash
cd ~/projects/Arduino/test-sketches/42-background-sync

arduino-cli compile --fqbn esp32:esp32:esp32 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/ArduinoJson \
  . && \
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 . && \
sleep 3 && \
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Note**: `sleep 3` waits for device to boot before starting monitor (still misses boot messages)

---

## Boot Capture Strategies

### Strategy 1: screen Pre-Connected (BEST)

```bash
# 1. Start screen logging
screen -L -Logfile /tmp/boot-$(date +%Y%m%d-%H%M%S).txt /dev/ttyUSB0 115200

# 2. Upload sketch (in another terminal)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# 3. Screen captures complete boot sequence
```

**Captures**:
- ✅ ROM bootloader messages
- ✅ ESP32 reset reason
- ✅ Boot mode and partition info
- ✅ setup() initialization
- ✅ All Serial.print() from beginning

### Strategy 2: Upload + Immediate screen

```bash
# Upload first
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 . && sleep 1 && screen /dev/ttyUSB0 115200
```

**Captures**:
- ❌ ROM bootloader (too early)
- ❌ setup() (finishes in <1 second)
- ✅ loop() output
- ⚠️ **Misses critical boot info**

### Strategy 3: Software Reset via Serial Command

**If sketch implements reset command**:

```bash
# With monitor already running, send reset command
echo "RESET" > /dev/ttyUSB0

# OR if using arduino-cli monitor, type "RESET" when prompted
```

**Captures**:
- ✅ Everything (monitor already connected)
- ⚠️ **Requires sketch to support reset command** (e.g., `ESP.restart()`)

### Strategy 4: Manual Hardware Reset (NOT AVAILABLE REMOTELY)

**Physical access only**: Press reset button while monitor connected

---

## Debugging Patterns

### Pattern 1: Verify Serial Works (Baseline Test)

**Problem**: No serial output at all

**Test**:
```bash
# Upload known-working sketch (e.g., test-sketches/42-background-sync)
screen /dev/ttyUSB0 115200

# In another terminal:
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 test-sketches/42-background-sync

# Expected: See boot messages + test sketch output
# If nothing: Check baud rate, serial port, USB cable
```

### Pattern 2: Identify Boot Crash

**Problem**: Sketch uploaded but device resets continuously

**Debug**:
```bash
screen -L -Logfile /tmp/crash-debug.txt /dev/ttyUSB0 115200

# Upload sketch
# Look for reset reason in captured log:
# - rst:0x10 (RTCWDT_RTC_RESET) = Watchdog timeout
# - rst:0x3 (SW_RESET) = Software reset
# - rst:0x4 (DEEPSLEEP_RESET) = Deep sleep
# - rst:0x5 (POWERON_RESET) = Power on
# - Backtrace = Exception/crash
```

### Pattern 3: Add Instrumentation to Sketch

**Best Practice**: Comprehensive serial logging

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);  // Allow serial to stabilize

    Serial.println("\n\n=== BOOT START ===");
    Serial.printf("[BOOT] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] Chip model: %s\n", ESP.getChipModel());

    // Print reset reason
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("[BOOT] Reset reason: %d\n", reason);

    Serial.println("[INIT] Initializing display...");
    // ... initialization code
    Serial.println("[INIT] Display OK");

    Serial.println("[INIT] Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("[ERROR] SD card failed!");
    } else {
        Serial.println("[INIT] SD card OK");
    }

    Serial.println("=== BOOT COMPLETE ===\n");
}

void loop() {
    // Use timestamps for timing-sensitive debugging
    Serial.printf("[%lu] Event occurred\n", millis());
}
```

### Pattern 4: Capture Only Boot, Then Exit

```bash
# Start screen, upload, wait for boot, kill screen
screen -L -Logfile /tmp/boot.txt /dev/ttyUSB0 115200 &
SCREEN_PID=$!
sleep 1

arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

sleep 5  # Wait for boot to complete
kill $SCREEN_PID

# Analyze boot log
cat /tmp/boot.txt
```

---

## Common Patterns

### Interactive Serial Commands

**In Sketch**:
```cpp
void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "STATUS") {
            printStatus();
        } else if (cmd == "HELP") {
            printHelp();
        } else if (cmd == "RESET") {
            ESP.restart();
        }
    }
}
```

**Testing**:
```bash
# Using arduino-cli monitor - just type commands
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
# Type: STATUS<enter>

# Using screen - same
screen /dev/ttyUSB0 115200
# Type: STATUS<enter>

# Using echo (one-shot command)
echo "STATUS" > /dev/ttyUSB0
```

### Compile + Upload Shorthand

**Create alias in ~/.bashrc**:
```bash
alias acu='arduino-cli compile --fqbn esp32:esp32:esp32 . && arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .'
```

**Usage**:
```bash
cd test-sketches/42-background-sync
acu
```

### Library Path Shorthand

**Create variable**:
```bash
export ARDUINO_LIBS="--library ~/projects/Arduino/libraries/TFT_eSPI --library ~/projects/Arduino/libraries/ArduinoJson --library ~/projects/Arduino/libraries/MFRC522 --library ~/projects/Arduino/libraries/ESP8266Audio --library ~/projects/Arduino/libraries/XPT2046_Touchscreen"

# Use:
arduino-cli compile --fqbn esp32:esp32:esp32 $ARDUINO_LIBS .
```

---

## Troubleshooting

### Problem: No Serial Output After Upload

**Symptoms**: Upload successful, but no serial output in monitor

**Checks**:
1. **Correct baud rate**: Sketch uses `Serial.begin(115200)`, monitor also 115200
2. **Serial initialized**: Check sketch has `Serial.begin()` in `setup()`
3. **Delay after Serial.begin**: Add `delay(1000)` after `Serial.begin(115200)`
4. **Wrong port**: Verify `/dev/ttyUSB0` is correct with `arduino-cli board list`
5. **USB cable**: Some cables are power-only (no data)

**Test**:
```bash
# Check if data is coming on port
cat /dev/ttyUSB0
# If nothing, problem is hardware/sketch
# If garbage, problem is baud rate
```

### Problem: "Port is busy" or "Permission denied"

**Symptoms**: Cannot upload or monitor

**Cause**: Another process has exclusive lock on `/dev/ttyUSB0`

**Fix**:
```bash
# Find process using port
lsof /dev/ttyUSB0

# Kill screen sessions
screen -ls
screen -X -S [session-id] quit

# Kill arduino-cli monitor
pkill -f "arduino-cli monitor"

# Kill minicom
pkill minicom

# As last resort, unplug/replug USB
```

### Problem: Boot Messages Missing

**Symptoms**: Monitor shows loop() output but not setup()

**Cause**: Monitor started after boot completed

**Fix**: Use screen pre-connection method (see "Boot Capture Strategies")

### Problem: Garbled Serial Output

**Symptoms**: Random characters, unreadable text

**Cause**: Baud rate mismatch

**Fix**:
```bash
# Check sketch:
Serial.begin(115200);  // Note the rate

# Use same rate in monitor:
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
screen /dev/ttyUSB0 115200
minicom -D /dev/ttyUSB0 -b 115200
```

### Problem: Upload Hangs at "Connecting..."

**Symptoms**: Upload never completes, stuck on "Connecting...."

**Possible Causes**:
1. Wrong board selected (FQBN mismatch)
2. Boot button needs to be held (should not be needed for CYD)
3. Serial port in use by another process

**Fix**:
```bash
# Kill any process using port
lsof /dev/ttyUSB0 | grep -v COMMAND | awk '{print $2}' | xargs kill

# Unplug/replug USB

# Try upload again
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .
```

### Problem: Sketch Compiles But Crashes on Boot

**Symptoms**: Device resets continuously, or stops responding

**Debug Steps**:
```bash
# 1. Capture boot sequence
screen -L -Logfile /tmp/crash.txt /dev/ttyUSB0 115200
# Upload in another terminal
# Check log for:
# - Reset reason (rst:0x...)
# - Backtrace
# - Last successful Serial.println()

# 2. Add Serial.println() throughout setup()
# Identify where crash occurs

# 3. Check reset reason code
# rst:0x10 = Watchdog (infinite loop or blocking operation)
# rst:0x3 = Software reset (ESP.restart() or intentional)
# Backtrace = Exception (null pointer, stack overflow)
```

### Problem: Serial Port Disappears During Upload

**Symptoms**: Upload fails with "port not found" halfway through

**Cause**: Loose USB connection, USB power issues, or bad cable

**Fix**:
1. Use different USB cable (must support data)
2. Use different USB port (avoid USB hubs)
3. Check `dmesg` for USB disconnect messages

---

## Quick Reference

### Essential Commands

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 .

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Monitor (boot capture)
screen -L -Logfile /tmp/boot.txt /dev/ttyUSB0 115200

# Monitor (standard)
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Find port
arduino-cli board list

# Kill screen
screen -X -S [session-id] quit
```

### Recommended Workflow (Phase 2.5 Debugging)

```bash
# Terminal 1: Start screen for boot capture
cd ~/projects/Arduino/ALNScanner0812Working
screen -L -Logfile /tmp/main-sketch-boot-$(date +%H%M%S).txt /dev/ttyUSB0 115200

# Terminal 2: Upload instrumented sketch
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 \
  --library ~/projects/Arduino/libraries/TFT_eSPI \
  --library ~/projects/Arduino/libraries/MFRC522 \
  --library ~/projects/Arduino/libraries/ESP8266Audio \
  --library ~/projects/Arduino/libraries/XPT2046_Touchscreen \
  --library ~/projects/Arduino/libraries/ArduinoJson \
  .

arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Terminal 1: Watch boot sequence in real-time
# Detach when done: Ctrl+A then D
# Boot log saved to /tmp/main-sketch-boot-*.txt
```

---

**Document Version**: 1.0
**Last Updated**: October 19, 2025
**Project**: ALN Hardware Scanner Orchestrator Integration
