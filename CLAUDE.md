# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## 🎯 PROJECT STATUS: v5.0 OOP REFACTOR COMPLETE

**Last Updated:** October 25, 2025
**Platform:** Raspberry Pi 5 (Debian 12 Bookworm, Native Linux Arduino CLI)
**Current Version:** v5.0 (OOP Architecture - Phases 0-5 Complete, Phase 6 Optimization Pending)
**Hardware:** CYD 2.8" ESP32-2432S028R (ST7789 Dual USB Variant)

---

## 📁 ACTIVE SKETCHES - WHICH ONE TO USE?

### 🟢 **PRODUCTION: ALNScanner_v5** (v5.0 - CURRENT)

```
ALNScanner_v5/
├── ALNScanner_v5.ino         # 16 lines (99.6% reduction!)
├── Application.h             # 1,247 lines (main orchestrator)
├── config.h                  # 119 lines (all constants)
├── hal/                      # Hardware Abstraction Layer
│   ├── SDCard.h             # 276 lines (thread-safe RAII)
│   ├── RFIDReader.h         # 916 lines (software SPI + NDEF)
│   ├── DisplayDriver.h      # 440 lines (BMP rendering)
│   ├── AudioDriver.h        # 227 lines (lazy I2S init)
│   └── TouchDriver.h        # 118 lines (WiFi EMI filtering)
├── models/                   # Data Models
│   ├── Config.h             # 87 lines (validation logic)
│   ├── Token.h              # 111 lines (metadata + scanning)
│   └── ConnectionState.h    # 82 lines (thread-safe state)
├── services/                 # Business Logic
│   ├── ConfigService.h      # 547 lines (SD config mgmt)
│   ├── TokenService.h       # 384 lines (token DB queries)
│   ├── OrchestratorService.h # 900+ lines (HTTP + Queue)
│   └── SerialService.h      # 278 lines (command registry)
└── ui/                       # User Interface Layer
    ├── Screen.h             # 217 lines (Template Method base)
    ├── UIStateMachine.h     # 366 lines (state + touch routing)
    └── screens/
        ├── ReadyScreen.h            # 244 lines
        ├── StatusScreen.h           # 319 lines
        ├── TokenDisplayScreen.h     # 379 lines
        └── ProcessingScreen.h       # 264 lines
```

**Status:** ✅ Compiles successfully (1,207,147 bytes / 92% flash)
**Last Updated:** October 25, 2025
**Git Branch:** `refactor/v5-oop-architecture`

**Full Feature Set:** (Same as v4.1)
- **Core RFID Scanning:** MFRC522 on software SPI, NDEF text extraction, 7-byte UID support
- **WiFi Connectivity:** Auto-reconnect, event-driven state management
- **HTTP Orchestrator Integration:**
  - POST `/api/scan` - Real-time scan submission
  - POST `/api/scan/batch` - Queue batch upload (up to 10 entries)
  - GET `/health` - Connection health check
  - GET `/api/tokens` - Token database synchronization
- **Offline Queue System:**
  - JSONL-based persistent queue (`/queue.jsonl`)
  - FIFO overflow protection (max 100 scans)
  - Stream-based queue rebuild (memory-safe)
  - Background batch upload (FreeRTOS Core 0 task)
- **Token Database:** Local JSON database with video token support
- **Display Modes:**
  - Ready screen (with debug mode indicator)
  - Status screen (tap-to-view: WiFi, orchestrator, queue, team info)
  - Video token processing image modal (2.5s auto-hide)
  - Regular token BMP image display
- **Audio Playback:** I2S WAV audio (lazy-initialized to prevent beeping)
- **Touch Handling:** WiFi EMI filtering, double-tap dismiss, single-tap status
- **Configuration:**
  - SD card-based config.txt
  - Runtime config editing via serial commands
  - DEBUG_MODE toggle (GPIO 3 conflict workaround)
  - 30-second boot override for emergency debug access

**Architecture Highlights (v5.0 OOP):**
- **Singleton Pattern:** All HAL and service components use getInstance()
- **Layered Architecture:** HAL → Models → Services → UI → Application
- **Dependency Injection:** Services accept dependencies via constructor/getInstance()
- **Design Patterns:** Strategy (screens), State Machine (UI), Facade (Application), RAII (SD locks)
- **Code Reduction:** Main .ino 3,839 → 16 lines (99.6% reduction)
- **Maintainability:** All files <1,250 lines, clear responsibilities, easy to test
- **Memory Safety:** RAII resource management, thread-safe SD access
- **Dual-core FreeRTOS:** Main loop on Core 1, background sync on Core 0

---

### 🔵 **LEGACY: ALNScanner1021_Orchestrator** (v4.1 - MONOLITHIC BASELINE)

```
ALNScanner1021_Orchestrator/
└── ALNScanner1021_Orchestrator.ino  # v4.1 - 3,839 lines, 92% flash
```

**Status:** ⚠️ **ARCHIVED** - Use v5.0 OOP architecture instead
**Purpose:** Reference implementation, baseline for v5.0 refactor
**Last Updated:** October 21, 2025

**Why v5.0 is Better:**
- **Maintainability:** 3,839 lines → 20 modular files
- **Testability:** Each component has test sketch (test-50 through test-60)
- **Extensibility:** New screens/services trivial to add
- **Flash Savings:** Phase 6 optimization will reduce 92% → <87% (target <1,150,000 bytes)
- **Code Quality:** Design patterns, dependency injection, single responsibility

**⚠️ v4.1 Limitations:**
- Single 3,839-line file (unmaintainable)
- 468-line if/else command chain
- Duplicate HTTP code (4 functions, 216 lines)
- Global variables everywhere
- Hard to unit test

---

## 🛠️ DEVELOPMENT COMMANDS

### **Compile Current Sketch (v5.0)**

```bash
cd /home/maxepunk/projects/Arduino/ALNScanner_v5

arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota,UploadSpeed=921600 .
```

**Expected Output:** `Sketch uses 1257147 bytes (61%) of program storage space` (with HTTPS support)

### **Upload to Device**

```bash
# Auto-detect port
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Or manually specify
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32 .
```

### **Serial Monitoring**

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Available Serial Commands (DEBUG_MODE=true):**
- `CONFIG` - Show configuration
- `STATUS` / `DIAG_NETWORK` - Connection status, queue size, memory
- `TOKENS` - Display token database (first 10 entries)
- `SET_CONFIG:KEY=VALUE` - Update config in memory
- `SAVE_CONFIG` - Persist config to SD card
- `REBOOT` - Restart ESP32
- `START_SCANNER` - Initialize RFID (kills serial RX permanently)
- `SIMULATE_SCAN:tokenId` - Test token processing without hardware
- `QUEUE_TEST` - Add 20 mock scans to queue
- `FORCE_UPLOAD` - Manually trigger batch upload
- `SHOW_QUEUE` - Display queue contents
- `HELP` - Show all commands

---

## 🏗️ ARCHITECTURE - v5.0 OOP REFACTOR

### **Layered Architecture Overview**

```
┌─────────────────────────────────────────────────┐
│  Layer 5: Main Entry Point                     │
│  ALNScanner_v5.ino (16 lines)                  │
│  - setup(): Application::setup()                │
│  - loop(): Application::loop()                  │
└─────────────────────────────────────────────────┘
                      ▲
                      │
┌─────────────────────────────────────────────────┐
│  Layer 4: Application Orchestrator             │
│  Application.h (1,247 lines)                   │
│  - Owns all HAL and Service instances          │
│  - Coordinates RFID → Orchestrator → UI flow   │
└─────────────────────────────────────────────────┘
                      ▲
        ┌─────────────┼─────────────┐
        │             │             │
┌───────▼──────┐ ┌────▼────┐ ┌─────▼──────┐
│  Services    │ │   UI    │ │    HAL     │
│  Layer 3     │ │ Layer 3 │ │  Layer 1   │
└──────────────┘ └─────────┘ └────────────┘

Services (4):          UI (6):              HAL (5):
- ConfigService   →   UIStateMachine    →   RFIDReader
- TokenService    →   Screen (base)     →   DisplayDriver
- OrchestratorSvc →   4x Screen impls   →   AudioDriver
- SerialService                         →   TouchDriver
                                       →   SDCard
```

### **Component Responsibilities**

**HAL Layer (Hardware Abstraction):**
- `SDCard.h` - Thread-safe SD operations with RAII locks
- `RFIDReader.h` - Software SPI + NDEF extraction for MFRC522
- `DisplayDriver.h` - TFT rendering + BMP image loading
- `AudioDriver.h` - I2S WAV playback (lazy-initialized)
- `TouchDriver.h` - Interrupt-based touch with WiFi EMI filtering

**Models Layer (Data Structures):**
- `Config.h` - Device configuration with validation
- `Token.h` - Token metadata + scan data structures
- `ConnectionState.h` - Thread-safe orchestrator state

**Services Layer (Business Logic):**
- `ConfigService.h` - Load/save config, runtime editing
- `TokenService.h` - Token database queries + orchestrator sync
- `OrchestratorService.h` - WiFi, HTTP, queue management, background tasks
- `SerialService.h` - Command registry pattern (replaces 468-line if/else chain)

**UI Layer (User Interface):**
- `UIStateMachine.h` - 4-state FSM + touch routing
- `Screen.h` - Template Method base class
- `ReadyScreen.h` - Idle screen with RFID status
- `StatusScreen.h` - Diagnostics display (tap-to-view)
- `TokenDisplayScreen.h` - Token image + audio playback
- `ProcessingScreen.h` - Video token modal (2.5s auto-dismiss)

**Application Layer (Orchestration):**
- `Application.h` - Initializes all subsystems, coordinates event flow

### **User Stories Implemented (v5.0 = v4.1 feature parity)**

| Phase | US  | Feature | Status |
|-------|-----|---------|--------|
| 3 | US1 | WiFi configuration from SD card | ✅ Done |
| 3 | US2 | Auto-reconnect WiFi | ✅ Done |
| 4 | US3 | Send scans to orchestrator (POST /api/scan) | ✅ Done |
| 5 | US4 | Queue offline scans to JSONL file | ✅ Done |
| 6 | US4 | Background batch upload (Core 0 task) | ✅ Done |
| 6 | FIX | Safe queue removal (stream-based) | ✅ Done |
| 7 | US5 | Queue overflow protection (FIFO, max 100) | ✅ Done |
| 8 | US6 | Tap-for-status screen | ✅ Done |
| 9 | US7 | Configurable token sync (SYNC_TOKENS=false) | ✅ Done |
| 10| - | Full instrumentation & diagnostics | ✅ Done |

### **System Flow - Token Scan Processing**

```
┌─────────────────────────────────────────────────────────────┐
│ 1. RFID Detection (Software SPI, 500ms scan interval)      │
│    └─► REQA → Select (cascade) → NDEF extraction           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Token ID Extraction                                      │
│    • NDEF text (preferred): "kaa001", "53:4E:2B:02"        │
│    • UID hex fallback: "04a1b2c3d4e5f6"                    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Orchestrator Routing                                     │
│    ┌─────────────────────────────────────────────────────┐ │
│    │ IF WiFi + Orchestrator Connected:                   │ │
│    │   └─► POST /api/scan (immediate send)               │ │
│    │       ├─► 2xx Success → Display token               │ │
│    │       ├─► 409 Conflict → Display (duplicate OK)     │ │
│    │       └─► Other → Queue for retry                   │ │
│    └─────────────────────────────────────────────────────┘ │
│    ┌─────────────────────────────────────────────────────┐ │
│    │ IF Offline:                                          │ │
│    │   └─► Append to /queue.jsonl (FIFO overflow check)  │ │
│    └─────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Token Metadata Lookup (tokens.json)                     │
│    ┌─────────────────────────────────────────────────────┐ │
│    │ IF hasVideoField() == true:                          │ │
│    │   └─► Display /images/{tokenId}.bmp                 │ │
│    │       + "Sending..." overlay (bottom of screen)     │ │
│    │       + 2.5s auto-hide → Return to ready screen     │ │
│    │   (Fire-and-forget - scan already sent)             │ │
│    └─────────────────────────────────────────────────────┘ │
│    ┌─────────────────────────────────────────────────────┐ │
│    │ ELSE (regular token):                                │ │
│    │   └─► Display /images/{tokenId}.bmp                 │ │
│    │       + Play /AUDIO/{tokenId}.wav                   │ │
│    │       + Wait for double-tap dismiss                 │ │
│    └─────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

**Key Point:** Video vs. Regular tokens use the **same file paths**. The difference is in the **metadata** (`video` field) and the **behavior** (modal timeout vs. persistent display).

### **Background Task (FreeRTOS Core 0)**

```cpp
void backgroundTask(void* parameter) {
  while (true) {
    // Every 10 seconds:
    // 1. Check orchestrator health (GET /health)
    // 2. Update connection state
    // 3. If queue > 0 && connected:
    //    └─► uploadQueueBatch() (up to 10 entries)
    //        └─► POST /api/scan/batch
    //            ├─► 200 OK → removeUploadedEntries() → Recurse
    //            └─► Error → Retry next cycle

    vTaskDelay(100 / portTICK_PERIOD_MS); // Prevent watchdog
  }
}
```

### **Critical Data Structures**

```cpp
// Connection State (atomic access via mutex)
enum ConnectionState {
  ORCH_DISCONNECTED,      // WiFi down
  ORCH_WIFI_CONNECTED,    // WiFi up, orchestrator unknown/down
  ORCH_CONNECTED          // WiFi + orchestrator healthy
};

// Token Metadata (loaded from tokens.json)
struct TokenMetadata {
  String tokenId;           // "kaa001"
  String video;             // "kaa001.mp4" or "" for non-video
  String image;             // "images/kaa001.bmp"
  String audio;             // "audio/kaa001.wav"
  String processingImage;   // "images/kaa001_processing.jpg" (orchestrator path, not used on device)
};

// Queue Entry (JSONL format)
struct QueueEntry {
  String tokenId;    // Required
  String teamId;     // Optional
  String deviceId;   // Required
  String timestamp;  // Required (ISO 8601-ish format)
};
```

---

## 🔧 HARDWARE CONFIGURATION

### **ESP32-2432S028R (Dual USB CYD)**

**Display:** ST7789, 240x320, Dual USB (Micro + Type-C)
**MCU:** ESP32-WROOM-32 (240MHz dual-core, 4MB flash, 520KB RAM)
**Flash Usage:** ~63% (1.26MB / 2MB with no_ota partition) - Comfortable
**RAM Usage:** 15% (52KB / 328KB) - Comfortable
**Partition Scheme:** `no_ota` (2MB app space, no OTA updates - requires USB for firmware updates)

### **Pin Assignments (DO NOT CHANGE - HARDWARE DEPENDENT)**

```cpp
// RFID Reader (Software SPI - MFRC522)
#define SOFT_SPI_SCK  22  // Via CN1 connector
#define SOFT_SPI_MOSI 27  // ⚠️ Electrically coupled to speaker
#define SOFT_SPI_MISO 35  // Input-only pin (via P3)
#define RFID_SS       3   // ⚠️ CONFLICTS with Serial RX

// SD Card (Hardware SPI - VSPI)
#define SDSPI_SCK   18
#define SDSPI_MOSI  23
#define SDSPI_MISO  19
#define SD_CS       5

// Touch Controller (XPT2046)
#define TOUCH_CS   33
#define TOUCH_IRQ  36  // Input-only, NO pull-up available

// Audio (I2S DAC)
#define AUDIO_BCLK 26  // I2S bit clock
#define AUDIO_LRC  25  // I2S word select
#define AUDIO_DIN  22  // I2S data (shared with RFID SCK)

// Display (TFT_eSPI - HSPI)
// Configured in libraries/TFT_eSPI/User_Setup.h
TFT_DC   = 2
TFT_CS   = 15
TFT_MOSI = 13
TFT_MISO = 12
TFT_SCK  = 14
TFT_BL   = 21  // Backlight (also on P3 connector)
```

### **⚠️ CRITICAL HARDWARE CONSTRAINTS**

#### **1. GPIO 3 Conflict (Serial RX vs RFID_SS)**

**Problem:** GPIO 3 is used for both:
- **Serial RX** (UART0 receive) - needed for serial commands
- **RFID_SS** (chip select) - needed for RFID scanning

**Solution:** DEBUG_MODE toggle
```
DEBUG_MODE=true  (development):
  └─► Serial RX active, RFID disabled
      Send START_SCANNER command to initialize RFID
      (kills serial RX permanently)

DEBUG_MODE=false (production):
  └─► RFID initialized at boot, serial RX unavailable
      30-second boot override: send ANY character to force DEBUG_MODE
```

#### **2. GPIO 27 / Speaker Coupling (RFID Beeping)**

**Problem:** GPIO 27 (MFRC522 MOSI) is electrically coupled to speaker amplifier circuit, causing beeping during RFID scanning.

**Hardware Flaw:** Cannot be fixed without board modification.

**Software Mitigation:**
- Scan interval: 100ms → 500ms (5x reduction, 80% less beeping)
- Defer audio initialization until first playback
- Keep MOSI LOW between scans: `digitalWrite(SOFT_SPI_MOSI, LOW);`
- Disable RF field when idle: `disableRFField();`

#### **3. Input-Only Pins**

**GPIO 34-39** are input-only (no pull-ups, no OUTPUT mode):
- GPIO 35 (RFID_MISO) - OK for SPI input
- GPIO 36 (TOUCH_IRQ) - **MUST use `pinMode(TOUCH_IRQ, INPUT)` NOT `INPUT_PULLUP`**
- GPIO 39 (Touch MISO) - OK for SPI input

---

## 🔥 CRITICAL IMPLEMENTATION PATTERNS

### **1. VSPI Bus Initialization Order** ✅ FIXED (October 22, 2025 - v5.0)

**Problem:** SD card and TFT display share the VSPI hardware bus. When initialized in the wrong order, SD file operations fail even though mounting succeeds.

**Root Cause:** The second device to initialize reconfigures VSPI in a way that breaks the first device's file operations.

**Solution:** Initialize Display **BEFORE** SD card

```cpp
// ✅ CORRECT - v5.0 Initialization Order (Application.h lines 681-723)
void Application::initializeEarlyHardware() {
    // 1. Display FIRST - Sets up VSPI for TFT
    auto& display = hal::DisplayDriver::getInstance();
    display.begin();  // tft.init() configures VSPI

    // 2. SD Card SECOND - Final VSPI configuration
    auto& sd = hal::SDCard::getInstance();
    sd.begin();  // SD reconfigures VSPI compatibly

    // Now SD.open() works correctly! ✅
}

// ❌ WRONG - v5.0 Initial Attempt (BROKEN)
void Application::initializeEarlyHardware() {
    // 1. SD Card first ❌
    auto& sd = hal::SDCard::getInstance();
    sd.begin();  // SD configures VSPI

    // 2. Display second ❌
    auto& display = hal::DisplayDriver::getInstance();
    display.begin();  // TFT reconfigures VSPI, breaks SD!

    // SD.open() fails! ❌
}
```

**⚠️ CRITICAL WARNING:** Any code that changes this initialization order will break SD card file operations. This includes Phase 6 optimizations!

### **2. SPI Bus Deadlock Prevention** ✅ FIXED (Sept 20, 2025)

**Problem:** SD card and TFT share VSPI bus. Holding TFT lock while reading SD causes system freeze.

**Solution:** Read from SD **FIRST**, then lock TFT

```cpp
// ✅ CORRECT - BMP Display Pattern
for (int y = height - 1; y >= 0; y--) {
    // STEP 1: Read from SD (VSPI needs to be free)
    f.read(rowBuffer, rowBytes);

    // STEP 2: Lock TFT and write pixel data
    tft.startWrite();
    tft.setAddrWindow(0, y, width, 1);
    // ... push pixels ...
    tft.endWrite();

    yield();  // Let other tasks run
}

// ❌ WRONG - CAUSES DEADLOCK
tft.startWrite();  // Locks VSPI
f.read(buffer, size);  // DEADLOCK - SD needs VSPI!
```

### **3. FreeRTOS Thread-Safe SD Access**

**Problem:** Main loop (Core 1) and background task (Core 0) both access SD card.

**Solution:** SD mutex protection

```cpp
// Global mutex
SemaphoreHandle_t sdMutex = xSemaphoreCreateMutex();

// Access pattern
bool sdTakeMutex(const char* caller, unsigned long timeoutMs) {
    return xSemaphoreTake(sdMutex, timeoutMs / portTICK_PERIOD_MS) == pdTRUE;
}

void sdGiveMutex(const char* caller) {
    xSemaphoreGive(sdMutex);
}

// Usage
if (sdTakeMutex("queueScan", 500)) {
    File f = SD.open("/queue.jsonl", FILE_APPEND);
    f.println(jsonLine);
    f.close();
    sdGiveMutex("queueScan");
}
```

### **4. Stream-Based Queue Removal (Memory Safe)**

**Problem:** Original implementation loaded entire queue into RAM, causing OOM on large queues.

**Solution:** Stream-based rebuild with temp file

```cpp
void removeUploadedEntries(int numEntries) {
    File src = SD.open("/queue.jsonl", FILE_READ);
    File tmp = SD.open("/queue.tmp", FILE_WRITE);

    int skipped = 0;
    while (src.available()) {
        String line = src.readStringUntil('\n');
        if (skipped < numEntries) {
            skipped++;  // Skip (remove) this line
        } else {
            tmp.println(line);  // Keep this line
        }
    }

    src.close();
    tmp.close();
    SD.remove("/queue.jsonl");
    SD.rename("/queue.tmp", "/queue.jsonl");
}
```

**Memory Impact:** ~100 bytes (String buffer) vs ~10KB (entire queue in RAM)

---

## 📚 PROJECT-LOCAL LIBRARIES (MODIFIED)

### **TFT_eSPI** ⚠️ **MODIFIED CONFIGURATION**

**Location:** `~/projects/Arduino/libraries/TFT_eSPI/`

**Configuration File:** `User_Setup.h`
```cpp
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_RGB_ORDER TFT_BGR  // Critical for CYD dual USB
#define TFT_BL 21              // Backlight GPIO
// ... see TFT_ESPI_QUICK_REFERENCE.md for full config
```

**Driver File Status:** `TFT_Drivers/ST7789_Init.h`
- Line 102: `writecommand(ST7789_INVON);` is **ACTIVE** (not commented)
- **Current configuration works correctly** - images display with proper colors
- Do NOT modify this line unless you encounter color inversion issues

**⚠️ NOTE:** If TFT_ESPI_QUICK_REFERENCE.md states line 102 should be commented, the documentation is outdated and needs correction.

### **Other Libraries (Unmodified)**

- **MFRC522** - RFID reader library
- **ESP8266Audio** - I2S audio playback (WAV generator)
- **XPT2046_Touchscreen** - Touch controller (IRQ only, coordinates unused)
- **ArduinoJson** - JSON parsing (installed globally via `arduino-cli lib install ArduinoJson`)

**Project-Local Libraries (8 total):**
```
libraries/
├── TFT_eSPI/                   # ⚠️ MODIFIED config
├── MFRC522/
├── ESP8266Audio/
├── ESP32-audioI2S-master/
├── XPT2046_Touchscreen/
└── XPT2046_Bitbang/
```

---

## 📂 DIRECTORY STRUCTURE

```
/home/maxepunk/projects/Arduino/
├── ALNScanner_v5/                   # 🟢 CURRENT v5.0 (OOP architecture)
│   ├── ALNScanner_v5.ino           # 16 lines (main entry)
│   ├── Application.h                # 1,247 lines (orchestrator)
│   ├── config.h                     # 119 lines (constants)
│   ├── hal/                         # 5 HAL components
│   ├── models/                      # 3 data models
│   ├── services/                    # 4 services
│   └── ui/                          # 6 UI components
│
├── ALNScanner1021_Orchestrator/    # 🔵 ARCHIVED v4.1 (monolithic)
│   └── ALNScanner1021_Orchestrator.ino  # 3,839 lines
│
├── archive/
│   └── ALNScanner0812Working.backup # 🔴 DEPRECATED v3.4
│
├── libraries/                       # Project-local (8 libraries)
│   ├── TFT_eSPI/                   # ⚠️ MODIFIED (ST7789 config)
│   ├── MFRC522/
│   ├── ESP8266Audio/
│   ├── ESP32-audioI2S-master/
│   ├── XPT2046_Touchscreen/
│   └── XPT2046_Bitbang/
│
├── test-sketches/                   # 60+ diagnostic sketches
│   ├── 01-display-hello/
│   ├── 38-wifi-connect/            # WiFi test
│   ├── 39-http-client/             # HTTP client test
│   ├── 40-json-parse/              # ArduinoJson test
│   ├── 41-queue-file/              # Queue operations test
│   ├── 42-background-sync/         # FreeRTOS background task
│   ├── 50-sdcard-hal/              # v5.0: SDCard HAL test
│   ├── 51-rfid-hal/                # v5.0: RFIDReader HAL test
│   ├── 52-display-hal/             # v5.0: DisplayDriver HAL test
│   ├── 53-audio-hal/               # v5.0: AudioDriver HAL test
│   ├── 54-touch-hal/               # v5.0: TouchDriver HAL test
│   ├── 55-config-service/          # v5.0: ConfigService test
│   ├── 56-token-service/           # v5.0: TokenService test
│   ├── 58-orchestrator-service/    # v5.0: OrchestratorService test
│   ├── 58-serial-service/          # v5.0: SerialService test
│   ├── 59-ui-layer/                # v5.0: UI integration test
│   ├── 60-application/             # v5.0: Full application test
│   └── ... (45+ more diagnostic sketches)
│
├── .claude/
│   ├── agents/                      # Subagent orchestration
│   └── skills/
│       ├── esp32-arduino/          # ESP32 Arduino CLI skill
│       └── subagent-orchestration/ # Parallel agent workflows
│
├── CLAUDE.md                        # 📘 This file
├── REFACTOR_IMPLEMENTATION_GUIDE.md # v5.0 refactor roadmap
├── HARDWARE_SPECIFICATIONS.md       # CYD pinout reference
├── TFT_ESPI_QUICK_REFERENCE.md     # TFT_eSPI config guide
├── sample_config.txt                # Example SD card config
└── baseline_flash.txt               # v4.1 baseline (1,209,987 bytes)
```

---

## 🧪 TEST-FIRST DEVELOPMENT METHODOLOGY

### **Core Principle**

**Test sketches are NOT validation artifacts - they ARE the implementation.**

When adding new hardware features:

1. **Create instrumented test sketch FIRST** in `test-sketches/`
   ```cpp
   // Full serial debugging with timestamps
   Serial.printf("[%lu] EVENT: Connection attempt #%d\n", millis(), attempts);

   // Statistics tracking
   int successRate = (attempts > 0) ? (100 * successes / attempts) : 0;
   Serial.printf("Success rate: %d/%d (%d%%)\n", successes, attempts, successRate);

   // Interactive serial commands
   if (Serial.available()) {
       String cmd = Serial.readStringUntil('\n');
       if (cmd == "STATUS") printStatus();
       else if (cmd == "TEST") runTest();
   }

   // Memory monitoring
   Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
   ```

2. **Validate on physical hardware IMMEDIATELY**
   - Upload to CYD device via `arduino-cli upload`
   - Test via serial monitor commands
   - Verify edge cases (disconnect, timeout, errors)
   - **DO NOT proceed until test passes on hardware**

3. **Extract production code from validated test**
   - Test sketch contains production-ready, proven code
   - Direct copy/paste into main sketch
   - Retain test sketch for regression testing

### **Why This Matters**

- **ESP32 hardware ≠ simulation:** WiFi timing, SPI contention, memory fragmentation
- **Early bug detection:** 200-line test sketch vs 3800-line production sketch
- **Living documentation:** Future devs see exact working implementation
- **Real-time diagnostics:** Instrumentation beats guessing every time

### **Example: WiFi Connection Test**

**Test Sketch:** `test-sketches/38-wifi-connect/`

**Hardware Results (Oct 19, 2025):**
```
Connection time: 1196 ms
Signal strength: -70 dBm
Success rate: 100% (2/2 attempts)
Auto-reconnect: WORKING
Free heap: 204 KB
```

**Interactive Commands Added:**
- `STATUS` - Full connection info
- `RECONNECT` - Force disconnect/reconnect test
- `MEM` - Heap usage

**Outcome:** Code proven on hardware → Copied to v4.1 orchestrator → Works in production

---

## 🎯 SD CARD CONFIGURATION

### **config.txt Format**

```ini
# WiFi Configuration (REQUIRED)
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=your_password

# Orchestrator Configuration (REQUIRED)
ORCHESTRATOR_URL=http://10.0.0.177:3000
TEAM_ID=001  # Exactly 3 digits (001-999)

# Device Configuration (OPTIONAL)
DEVICE_ID=SCANNER_FLOOR1_001  # Auto-generated from MAC if not set

# Token Synchronization (OPTIONAL, default: true)
SYNC_TOKENS=true   # false to skip token sync (saves ~2-5s boot time)

# Debug Mode (OPTIONAL, default: false)
DEBUG_MODE=false   # true to defer RFID init for serial commands
```

### **File Structure on SD Card**

```
SD:/
├── config.txt              # Configuration (REQUIRED)
├── tokens.json             # Token database (synced from orchestrator)
├── device_id.txt           # Persisted device ID
├── queue.jsonl             # Offline scan queue (auto-created)
├── images/
│   ├── kaa001.bmp          # Token image (240x320, 24-bit BMP)
│   ├── jaw001.bmp          # Another token image (same format)
│   └── 04A1B2C3.bmp        # UID-based image fallback
└── AUDIO/
    ├── kaa001.wav          # Token audio
    └── 04A1B2C3.wav        # UID-based audio fallback
```

**Image File Naming Rules:**
- **ALL images are BMP files** (24-bit color, 240x320 pixels)
- **Same naming pattern:** `/images/{tokenId}.bmp`
- **No special suffixes** - video tokens and regular tokens use identical file paths
- **Distinction is in metadata:** `video` field in tokens.json determines behavior

**Example:**
- `kaa001.bmp` with `video: ""` → Regular token (persistent display + audio)
- `jaw001.bmp` with `video: "jaw001.mp4"` → Video token (2.5s modal + "Sending..." overlay)

---

## 🐛 TROUBLESHOOTING

### **Compilation Issues**

**"Sketch too big" / Flash at 100%:**
```bash
# Check current usage
arduino-cli compile --fqbn esp32:esp32:esp32 . | grep "Sketch uses"

# If >95%, consider:
# 1. Remove unused libraries from sketch
# 2. Use partition scheme with more app space
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota .
# (Trades OTA capability for 2MB app space)
```

**"ArduinoJson.h: No such file":**
```bash
# Install globally (not project-local)
arduino-cli lib install ArduinoJson
```

### **Runtime Issues**

**RFID not scanning / No serial commands working:**
- Check DEBUG_MODE in config.txt
- If false: RFID initializes at boot, serial RX unavailable
- If true: Send `START_SCANNER` command to enable RFID

**Queue grows infinitely / Upload never happens:**
- Check WiFi connection: `STATUS` command
- Check orchestrator health: `curl http://ORCHESTRATOR_URL/health`
- Manually trigger upload: `FORCE_UPLOAD` command
- Check Core 0 task is running: Look for `[BG-TASK]` log messages

**Display shows black screen:**
- Wrong TFT_eSPI configuration (see TFT_ESPI_QUICK_REFERENCE.md)
- Backlight pin mismatch (try GPIO 21 or GPIO 27)

**Colors inverted / wrong:**
- Check `TFT_RGB_ORDER` in User_Setup.h (should be `TFT_BGR` for dual USB CYD)
- Current v4.1 sketch works correctly with `ST7789_INVON` command active
- Only modify ST7789_Init.h if you actually observe color inversion issues

---

## 🔬 SERIAL INSTRUMENTATION PATTERNS

### **Event Logging with Timestamps**

```cpp
Serial.printf("[%lu] [WIFI] Connected to AP\n", millis());
Serial.printf("         SSID: %s, Channel: %d\n", WiFi.SSID().c_str(), WiFi.channel());
```

### **Statistics Tracking**

```cpp
struct RFIDStats {
    uint32_t totalScans = 0;
    uint32_t successfulScans = 0;
    uint32_t failedScans = 0;
    uint32_t collisionErrors = 0;
} rfidStats;

// Update
rfidStats.totalScans++;
if (success) rfidStats.successfulScans++;

// Report
Serial.printf("Success rate: %d/%d (%.1f%%)\n",
    rfidStats.successfulScans,
    rfidStats.totalScans,
    100.0 * rfidStats.successfulScans / rfidStats.totalScans);
```

### **Diagnostic Sections**

```cpp
Serial.println("\n[TOKEN-SYNC] ═══ TOKEN DATABASE SYNC START ═══");
Serial.printf("[TOKEN-SYNC] Free heap: %d bytes\n", ESP.getFreeHeap());
// ... operation ...
Serial.println("[TOKEN-SYNC] ✓✓✓ SUCCESS ✓✓✓ Token database synced");
Serial.println("[TOKEN-SYNC] ═══ TOKEN DATABASE SYNC END ═══\n");
```

---

## 🎓 LEARNING FROM PROJECT HISTORY

### **Key Decisions Made**

1. **Abandoned Modular Approach** (Sept 20, 2025)
   - Attempted: `CYD_Multi_Compatible` with separate modules
   - Reality: ESP32 memory too tight, overhead too high
   - Decision: Monolithic sketch with functional sections (v4.1)

2. **Migrated WSL2 → Raspberry Pi** (Oct 18, 2025)
   - Eliminated: Windows-Linux USB bridge complexity
   - Gained: Native Arduino CLI, native serial ports
   - Archived: All WSL2-specific scripts and docs

3. **v5.0 OOP Refactor** (October 22-25, 2025)
   - Problem: v4.1 monolithic 3,839-line file unmaintainable
   - Solution: Layered OOP architecture (HAL → Services → UI → Application)
   - Implementation: 6 parallel subagent agents, 5 phases completed in 3 days
   - Result: 20 modular files, 99.6% main .ino reduction, same flash usage
   - **CRITICAL FIX:** VSPI bus initialization order (Display before SD)

4. **Test-First Development** (Ongoing)
   - Pattern: Validated test sketch → Production code
   - Success: WiFi (test-38), HTTP (test-39), Queue (test-41), HAL (test-50-54), Services (test-55-58), UI (test-59), App (test-60)
   - Result: Fewer integration bugs, faster development

5. **Stream-Based Queue Rebuild** (v4.1 Phase 6 Fix)
   - Problem: OOM on queue removal (entire queue in RAM)
   - Solution: File streaming with temp file
   - Impact: 100-entry queue from 10KB RAM → 100 bytes RAM

---

## 🚀 NEXT DEVELOPMENT PRIORITIES

Based on v5.0 status (92% flash, Phase 6 optimization pending):

1. **Phase 6: Flash Optimization** (CRITICAL - TARGET <87%)
   - Current: 1,207,147 bytes (92%)
   - Target: <1,150,000 bytes (87%)
   - Required savings: 57,147 bytes
   - Tactics:
     - PROGMEM strings (F() macro) - Expected: -20KB
     - Compile-time DEBUG_MODE flags - Expected: -15KB
     - Dead code elimination - Expected: -15KB
     - Inline small functions - Expected: -7KB
   - **⚠️ MUST NOT regress VSPI initialization order (Display before SD)**

2. **Token Database Expansion**
   - Current: 50-token limit (std::vector-based)
   - Future: File-based lookup for 100+ tokens

3. **Enhanced Diagnostics**
   - HTTP request/response logging
   - Queue operation statistics
   - RFID scan success rate trends

4. **Production Hardening**
   - Watchdog timer implementation
   - Crash recovery and auto-restart
   - SD card corruption detection

5. **Code Quality Improvements**
   - Unit tests for service layer (mocked HAL)
   - Integration tests for UI state machine
   - Memory leak detection (valgrind-style)

---

## 📞 GETTING HELP

### **Documentation Priority**

1. This file (CLAUDE.md) - Architecture and commands
2. HARDWARE_SPECIFICATIONS.md - CYD pinout and constraints
3. TFT_ESPI_QUICK_REFERENCE.md - Display configuration (⚠️ needs update)
4. test-sketches/{number}-{feature}/ - Working examples
5. sample_config.txt - Configuration reference

### **Common Issues by Symptom**

| Symptom | First Check | Fix |
|---------|-------------|-----|
| Won't compile | Flash usage >95% | Remove unused libraries or change partition |
| Black screen | TFT_eSPI config | Check User_Setup.h, backlight pin |
| No RFID scanning | DEBUG_MODE setting | Send START_SCANNER or set DEBUG_MODE=false |
| Queue not uploading | WiFi/orchestrator | Check STATUS, verify /health endpoint |
| Continuous beeping | RFID scan interval | Should be 500ms, not 100ms |
| Serial commands ignored | GPIO 3 conflict | Set DEBUG_MODE=true or use boot override |

### **Known Documentation Issues**

- **TFT_ESPI_QUICK_REFERENCE.md** may contain outdated information about ST7789_Init.h modifications
- Current v4.1 sketch works correctly with default TFT_eSPI driver file (ST7789_INVON command active)
- Update TFT_ESPI_QUICK_REFERENCE.md to reflect actual working configuration

---

**Project Version:** v5.0 (OOP Architecture)
**Compilation Status:** ✅ Compiles (1,207,147 bytes / 92% flash)
**Last Verified:** October 25, 2025
**Git Branch:** `refactor/v5-oop-architecture`
**Phase Status:** Phases 0-5 Complete, Phase 6 Optimization Pending

*For detailed implementation history, see:*
- `REFACTOR_IMPLEMENTATION_GUIDE.md` - v5.0 refactor roadmap
- `git log` - Commit history
- `test-sketches/50-60/` - Component test sketches
