# CLAUDE.md

Last verified: 2026-02-06

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

For cross-cutting concerns (scoring logic, operation modes, game modes, token schema), see @../CLAUDE.md.

---

## Project Overview

**Platform:** Raspberry Pi 5 (Debian 12 Bookworm, Native Linux Arduino CLI)
**Hardware:** CYD 2.8" ESP32-2432S028R (ST7789 Dual USB Variant)
**Architecture:** v5.0 OOP (Layered: HAL -> Models -> Services -> UI -> Application)

ESP32 player scanner for NFC token scanning. Reads RFID tokens, displays images/plays audio, and reports scans to the backend orchestrator over HTTP. Offline scans are queued to SD card for batch upload.

---

## Directory Structure

```
arduino-cyd-player-scanner/
├── ALNScanner_v5/                   # Production v5.0 (OOP architecture)
│   ├── ALNScanner_v5.ino           # 16 lines (main entry)
│   ├── Application.h                # ~1,375 lines (orchestrator)
│   ├── config.h                     # 120 lines (all constants)
│   ├── hal/                         # Hardware Abstraction Layer
│   │   ├── SDCard.h                # 270 lines (thread-safe RAII)
│   │   ├── RFIDReader.h            # 917 lines (software SPI + NDEF)
│   │   ├── DisplayDriver.h         # 440 lines (BMP rendering)
│   │   ├── AudioDriver.h           # 241 lines (lazy I2S init)
│   │   └── TouchDriver.h           # 118 lines (WiFi EMI filtering)
│   ├── models/                      # Data Models
│   │   ├── Config.h                # 94 lines (validation logic)
│   │   ├── Token.h                 # 89 lines (metadata + scanning)
│   │   └── ConnectionState.h       # 82 lines (thread-safe state)
│   ├── services/                    # Business Logic
│   │   ├── ConfigService.h         # 552 lines (SD config mgmt)
│   │   ├── TokenService.h          # 381 lines (token DB queries)
│   │   ├── OrchestratorService.h   # ~1,155 lines (HTTP + Queue)
│   │   └── SerialService.h         # 278 lines (command registry)
│   └── ui/                          # User Interface Layer
│       ├── Screen.h                # 217 lines (Template Method base)
│       ├── UIStateMachine.h        # 365 lines (state + touch routing)
│       └── screens/
│           ├── ReadyScreen.h       # 244 lines
│           ├── StatusScreen.h      # 319 lines
│           ├── TokenDisplayScreen.h # 392 lines
│           └── ProcessingScreen.h  # 265 lines
│
├── ALNScanner1021_Orchestrator/    # ARCHIVED v4.1 (monolithic reference)
│
├── sd-card-deploy/                  # SD card content for physical devices
│   ├── config.txt                  # Example device config
│   ├── tokens.json                 # Token database
│   ├── assets/images/              # v5 BMP images (240x320, 24-bit)
│   ├── assets/audio/               # v5 WAV audio files
│   └── images/                     # Legacy image path (pre-v5)
│
├── libraries/                       # Project-local Arduino libraries
│   ├── TFT_eSPI/                   # MODIFIED (ST7789 CYD config)
│   ├── MFRC522/
│   ├── ESP8266Audio/
│   ├── ESP32-audioI2S-master/
│   ├── XPT2046_Touchscreen/
│   └── XPT2046_Bitbang/
│
├── test-sketches/                   # 60+ diagnostic/test sketches
│
├── .claude/
│   ├── agents/
│   └── skills/
│       ├── esp32-skill/            # ESP32 Arduino CLI skill
│       └── subagent-orchestration/
│
├── compile-test.sh                  # Compile helper for test sketches
├── sample_config.txt
└── CLAUDE.md                        # This file
```

---

## Development Commands

### Compile (v5.0)

```bash
cd ALNScanner_v5

arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota,UploadSpeed=921600 .
```

**Note:** When working in the submodule context within ALN-Ecosystem, the path is `arduino-cyd-player-scanner/ALNScanner_v5/`. When working as a standalone clone, adjust accordingly.

### Upload and Monitor

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 ALNScanner_v5
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### Serial Commands (DEBUG_MODE=true)

| Command | Description |
|---------|-------------|
| `CONFIG` | Show configuration |
| `STATUS` / `DIAG_NETWORK` | Connection status, queue size, memory |
| `TOKENS` | Display token database (first 10) |
| `SET_CONFIG:KEY=VALUE` | Update config in memory |
| `SAVE_CONFIG` | Persist config to SD card |
| `REBOOT` | Restart ESP32 |
| `START_SCANNER` | Initialize RFID (kills serial RX permanently) |
| `SIMULATE_SCAN:tokenId` | Test token processing without hardware |
| `QUEUE_TEST` | Add 20 mock scans to queue |
| `FORCE_UPLOAD` | Manually trigger batch upload |
| `SHOW_QUEUE` | Display queue contents |
| `QUEUE_STATUS` | Detailed queue diagnostics |
| `CLEAR_QUEUE` | Delete queue (requires YES confirmation) |

---

## Testing

### PlatformIO Native Tests

Unit tests run on the host machine (Pi 5) using PlatformIO's `native` platform. No ESP32 hardware needed.

```bash
pio test -e native              # Run all tests
pio test -e native -f test_config  # Run only config tests
pio test -e native -f test_token   # Run only token tests
```

**What's tested:**
- `models/Config.h`: `validate()` (SSID/password/URL/teamID validation, http→https auto-upgrade), `isComplete()`, default values
- `models/Token.h`: `cleanTokenId()` (colon/space removal, lowercase, trim), `isVideoToken()`, `getImagePath()`/`getAudioPath()` path construction, `ScanData` validation
- `services/PayloadBuilder.h`: `buildScanJson()` (single scan payload), `parseScanFromJsonl()` (queue deserialization), `buildBatchJson()` (batch upload payload), round-trip serialization
- `hal/NDEFParser.h`: `parseNDEFText()` (TLV parsing, NDEF record extraction, SAK validation, page-spanning text, edge cases)

**What's NOT tested (requires future mock infrastructure):**
- `services/OrchestratorService.h`: HTTP calls, WiFi management, SD queue file I/O, FreeRTOS background task
- `hal/` layer: Hardware-dependent (RFID, display, audio, touch, SD card)
- `ui/` layer: Depends on hal/ and display hardware
- `services/ConfigService.h`: SD card file I/O (needs SD mock)

**Mock infrastructure:** `mock/Arduino.h` provides String class, Serial stubs, isDigit(), F() macro. The real `config.h` compiles as-is (pure constexpr constants).

**Adding new tests:** Create `test/test_<name>/test_<name>.cpp`, include `<unity.h>` and `<Arduino.h>`, use `TEST_ASSERT_*` macros.

### NDEF Diagnostic Logging

Uncomment `#define NDEF_DEBUG` in `config.h` to enable production-safe NDEF byte logging.
Outputs raw page bytes, TLV parse results, and extraction outcomes via Serial TX.
Serial TX remains active even when RFID uses GPIO 3 (only RX is killed).

Use during game sessions to capture byte sequences for test fixtures:
1. Uncomment `#define NDEF_DEBUG` in `config.h`
2. Compile and upload
3. Monitor Serial output during scans: `arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200`
4. Copy `[NDEF-DIAG]` lines for failing scans
5. Re-comment `#define NDEF_DEBUG` after capture

---

## Critical Data Structures

```cpp
// Token Metadata (models/Token.h)
// SIMPLIFIED in v5 - only two fields, paths constructed from tokenId
struct TokenMetadata {
    String tokenId;  // "kaa001" or UID hex
    String video;    // "kaa001.mp4" or "" for non-video

    bool isVideoToken() const;
    String getImagePath() const;  // -> /assets/images/{cleanTokenId}.bmp
    String getAudioPath() const;  // -> /assets/audio/{cleanTokenId}.wav
};

// Scan data (models/Token.h)
struct ScanData {
    String tokenId;     // Required
    String teamId;      // Optional
    String deviceId;    // Required
    String deviceType;  // Required ("esp32")
    String timestamp;   // Required (ISO 8601-ish)
};

// Connection State (models/ConnectionState.h)
enum ConnectionState {
    ORCH_DISCONNECTED,      // WiFi down
    ORCH_WIFI_CONNECTED,    // WiFi up, orchestrator unknown/down
    ORCH_CONNECTED          // WiFi + orchestrator healthy
};
```

---

## SD Card File Paths (CRITICAL)

Code paths are defined in `config.h` (namespace `paths`):

```cpp
constexpr const char* CONFIG_FILE    = "/config.txt";
constexpr const char* TOKEN_DB_FILE  = "/tokens.json";
constexpr const char* DEVICE_ID_FILE = "/device_id.txt";
constexpr const char* IMAGES_DIR     = "/assets/images/";   // NOT /images/
constexpr const char* AUDIO_DIR      = "/assets/audio/";    // NOT /AUDIO/
```

Token paths are always constructed from tokenId:
- Image: `/assets/images/{cleanTokenId}.bmp`
- Audio: `/assets/audio/{cleanTokenId}.wav`

**SD card layout:**
```
SD:/
├── config.txt
├── tokens.json
├── device_id.txt
├── queue.jsonl              # Auto-created offline queue
└── assets/
    ├── images/
    │   ├── kaa001.bmp       # 240x320, 24-bit BMP
    │   └── jaw001.bmp
    └── audio/
        ├── kaa001.wav
        └── jaw001.wav
```

Video tokens and regular tokens use the same image path. The `video` field in tokens.json determines behavior (2.5s modal vs persistent display + audio).

### config.txt Format

```ini
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=your_password
ORCHESTRATOR_URL=http://10.0.0.177:3000
TEAM_ID=001
DEVICE_ID=SCANNER_FLOOR1_001   # Auto-generated from MAC if not set
SYNC_TOKENS=true                # false to skip token sync
DEBUG_MODE=false                # true to defer RFID init for serial commands
```

---

## Hardware Configuration

### ESP32-2432S028R (Dual USB CYD)

- **Display:** ST7789, 240x320, Dual USB (Micro + Type-C)
- **MCU:** ESP32-WROOM-32 (240MHz dual-core, 4MB flash, 520KB RAM)
- **Partition:** `no_ota` (2MB app space, requires USB for firmware updates)

### Pin Assignments (DO NOT CHANGE)

```cpp
// RFID Reader (Software SPI - MFRC522)
RFID_SCK  = 22   // Via CN1 connector
RFID_MOSI = 27   // Electrically coupled to speaker
RFID_MISO = 35   // Input-only pin (via P3)
RFID_SS   = 3    // CONFLICTS with Serial RX

// SD Card (Hardware SPI - VSPI)
SD_SCK  = 18,  SD_MOSI = 23,  SD_MISO = 19,  SD_CS = 5

// Touch Controller (XPT2046)
TOUCH_CS = 33,  TOUCH_IRQ = 36  // Input-only, NO pull-up

// Audio (I2S DAC)
AUDIO_BCLK = 26,  AUDIO_LRC = 25,  AUDIO_DIN = 22  // Shared with RFID SCK

// Display (TFT_eSPI - HSPI) - configured in libraries/TFT_eSPI/User_Setup.h
TFT_DC=2, TFT_CS=15, TFT_MOSI=13, TFT_MISO=12, TFT_SCK=14, TFT_BL=21
```

---

## Critical Hardware Constraints

### 1. GPIO 3 Conflict (Serial RX vs RFID_SS)

GPIO 3 is used for both Serial RX (UART0 receive) and RFID_SS (chip select).

```
DEBUG_MODE=true  (development):
  Serial RX active, RFID disabled.
  Send START_SCANNER to initialize RFID (kills serial RX permanently).

DEBUG_MODE=false (production):
  RFID initialized at boot, serial RX unavailable.
  30-second boot override: send ANY character to force DEBUG_MODE.
```

### 2. GPIO 27 / Speaker Coupling (RFID Beeping)

GPIO 27 (MFRC522 MOSI) is electrically coupled to the speaker amplifier circuit, causing beeping during RFID scanning. This is a hardware flaw that cannot be fixed without board modification.

**Software mitigations:**
- Scan interval: 500ms (not 100ms) - 80% less beeping
- Defer audio initialization until first playback
- Keep MOSI LOW between scans: `digitalWrite(SOFT_SPI_MOSI, LOW)`
- Disable RF field when idle: `disableRFField()`

### 3. Input-Only Pins (GPIO 34-39)

No pull-ups, no OUTPUT mode:
- GPIO 35 (RFID_MISO) - OK for SPI input
- GPIO 36 (TOUCH_IRQ) - **MUST use `INPUT` NOT `INPUT_PULLUP`**
- GPIO 39 (Touch MISO) - OK for SPI input

---

## Critical Implementation Patterns

### 1. VSPI Bus Initialization Order

SD card and TFT display share the VSPI hardware bus. **Display MUST initialize before SD card.**

```cpp
// CORRECT - Application.h initializeEarlyHardware() (line ~697)
void Application::initializeEarlyHardware() {
    // 1. Display FIRST - Sets up VSPI for TFT
    display.begin();

    // 2. SD Card SECOND - Final VSPI configuration
    sd.begin();
}
```

Reversing this order causes SD file operations to fail even though mounting succeeds. Any code that changes this initialization order (including optimizations) will break the device.

### 2. SPI Bus Deadlock Prevention

SD card and TFT share VSPI. Read from SD FIRST, then lock TFT:

```cpp
// CORRECT - BMP Display Pattern
for (int y = height - 1; y >= 0; y--) {
    f.read(rowBuffer, rowBytes);    // SD read (VSPI free)
    tft.startWrite();                // Lock TFT
    tft.setAddrWindow(0, y, width, 1);
    // ... push pixels ...
    tft.endWrite();                  // Release TFT
    yield();
}

// WRONG - CAUSES DEADLOCK
tft.startWrite();              // Locks VSPI
f.read(buffer, size);          // DEADLOCK - SD needs VSPI!
```

### 3. FreeRTOS Thread-Safe SD Access

Main loop (Core 1) and background task (Core 0) both access SD card. All SD access must go through the mutex:

```cpp
if (sdTakeMutex("queueScan", 500)) {
    File f = SD.open("/queue.jsonl", FILE_APPEND);
    f.println(jsonLine);
    f.close();
    sdGiveMutex("queueScan");
}
```

### 4. Stream-Based Queue Removal

Queue removal uses stream-based file rebuild (temp file) instead of loading the entire queue into RAM. This keeps memory usage at ~100 bytes vs ~10KB for the full queue.

---

## Token Scan Processing Flow

```
1. RFID Detection (Software SPI, 500ms interval)
   -> REQA -> Select (cascade) -> NDEF extraction

2. Token ID Extraction
   - NDEF text (preferred): "kaa001"
   - UID hex fallback: "04a1b2c3d4e5f6"

3. Orchestrator Routing
   - IF connected: POST /api/scan (immediate)
     Payload: {tokenId, teamId?, deviceId, deviceType:"esp32", timestamp}
     - 2xx -> Display token
     - 409 -> Display (duplicate OK for player scanners)
     - Other -> Queue for retry
   - IF offline: Append to /queue.jsonl

4. Token Display
   - Video token (video field set): Show /assets/images/{id}.bmp + "Sending..." overlay, 2.5s auto-hide
   - Regular token: Show /assets/images/{id}.bmp + Play /assets/audio/{id}.wav, double-tap dismiss
```

### Background Task (FreeRTOS Core 0)

Every 10 seconds: check orchestrator health (GET /health), update connection state, upload queued scans in batches of 10 via POST /api/scan/batch.

---

## TFT_eSPI Configuration

**Location:** `libraries/TFT_eSPI/User_Setup.h`

```cpp
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_RGB_ORDER TFT_BGR  // Critical for CYD dual USB
#define TFT_BL 21
```

**Driver:** `TFT_Drivers/ST7789_Init.h` line 102: `writecommand(ST7789_INVON)` is ACTIVE. Current configuration works correctly. Do NOT modify unless you observe color inversion issues.

---

## Troubleshooting

| Symptom | First Check | Fix |
|---------|-------------|-----|
| Won't compile / Flash >95% | Partition scheme | Use `PartitionScheme=no_ota` |
| Black screen | TFT_eSPI config | Check User_Setup.h, backlight pin 21 |
| No RFID scanning | DEBUG_MODE setting | Send START_SCANNER or set DEBUG_MODE=false |
| Queue not uploading | WiFi/orchestrator | Check STATUS, verify /health endpoint |
| Continuous beeping | RFID scan interval | Should be 500ms, not 100ms |
| Serial commands ignored | GPIO 3 conflict | Set DEBUG_MODE=true or use boot override |
| Colors inverted | TFT_RGB_ORDER | Should be TFT_BGR for dual USB CYD |
| HTTPS heap corruption | WiFiClientSecure reuse | Create fresh WiFiClientSecure per request |
| "ArduinoJson.h: No such file" | Global install | `arduino-cli lib install ArduinoJson` |
