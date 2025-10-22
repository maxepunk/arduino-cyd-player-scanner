# ALNScanner v5.0 Refactor Implementation Guide

**Version:** 1.0
**Target:** v4.1 (3839 lines) → v5.0 (Clean Architecture)
**Flash Budget:** 1,209,987 bytes (92%) → ~1,150,000 bytes (87%)
**Status:** Planning Complete, Implementation Pending

---

## I. REFACTOR PRINCIPLES

### A. Mandatory Constraints

1. **Zero Functional Regression** - All v4.1 features must work identically
2. **Incremental Migration** - Each phase must compile and be testable
3. **Hardware Independence** - HAL components must be mockable
4. **Flash Budget** - Each phase tracks flash delta, total must decrease 60KB+
5. **Single Responsibility** - No file >300 lines, no function >50 lines

### B. Design Patterns (Required)

| Pattern | Where | Purpose |
|---------|-------|---------|
| Dependency Injection | Services | Enable testing, reduce coupling |
| Strategy | Screens | Polymorphic UI behavior |
| State Machine | UIStateMachine | Manage screen transitions |
| Facade | Application | Simplify subsystem coordination |
| RAII | SDCard, Mutexes | Automatic resource cleanup |
| Template Method | Screen base class | Enforce render lifecycle |

### C. File Organization Rules

```
Rule 1: Header-only for <100 line classes (easier Arduino CLI compilation)
Rule 2: .cpp separation for >100 lines OR if contains large static data
Rule 3: All pin definitions in config.h (single source of truth)
Rule 4: All string literals >20 chars use PROGMEM via F() macro
Rule 5: No global variables except in Application.h
```

---

## II. ARCHITECTURE REFERENCE

### A. Dependency Graph (Read Bottom-Up)

```
┌─────────────────────────────────────────────────┐
│  Layer 4: Main Entry Point                     │
│  ALNScanner_v5.ino (150 lines)                 │
│  - setup(): Application::setup()                │
│  - loop(): Application::loop()                  │
└─────────────────────────────────────────────────┘
                      ▲
                      │
┌─────────────────────────────────────────────────┐
│  Layer 3: Application Orchestrator             │
│  Application.h (250 lines)                     │
│  - Owns all HAL and Service instances          │
│  - Coordinates RFID → Orchestrator → UI flow   │
└─────────────────────────────────────────────────┘
                      ▲
        ┌─────────────┼─────────────┐
        │             │             │
┌───────▼──────┐ ┌────▼────┐ ┌─────▼──────┐
│  Services    │ │   UI    │ │    HAL     │
│  Layer 2     │ │ Layer 2 │ │  Layer 1   │
└──────────────┘ └─────────┘ └────────────┘

Layer 2 Services:          Layer 2 UI:           Layer 1 HAL:
- OrchestratorService  →   UIStateMachine    →   RFIDReader
- TokenService         →   Screen (base)     →   DisplayDriver
- ConfigService        →   4x Screen impls   →   AudioDriver
- SerialService                              →   TouchDriver
                                             →   SDCard
```

### B. Data Flow (Token Scan Example)

```
1. RFIDReader.scanCard() → ScanResult
2. Application.processRFIDScan(result)
3. TokenService.getMetadata(tokenId) → TokenMetadata
4. OrchestratorService.sendScan(data) → bool
5. IF sendFail: OrchestratorService.queueScan(data)
6. IF hasVideo: UIStateMachine.showProcessing(metadata)
7. ELSE: UIStateMachine.showToken(metadata)
```

---

## III. PHASE-BY-PHASE IMPLEMENTATION

### PHASE 0: Pre-Refactor Preparation

**Objective:** Establish baseline, create structure, verify tooling

#### Step 0.1: Baseline Capture
```bash
# Compile current v4.1 and record metrics
cd ALNScanner1021_Orchestrator
arduino-cli compile --fqbn esp32:esp32:esp32 . > ../baseline_flash.txt
grep "Sketch uses" ../baseline_flash.txt
# Expected: 1,209,987 bytes (92%)

# Create baseline behavior log
./capture_boot.sh > ../baseline_boot.log
# Test all serial commands, save output
```

**Verification:**
- [ ] baseline_flash.txt contains exact byte count
- [ ] baseline_boot.log shows complete boot sequence
- [ ] All serial commands tested and logged

#### Step 0.2: Create Directory Structure
```bash
cd /home/maxepunk/projects/Arduino
mkdir -p ALNScanner_v5/{hal,services,models,ui/screens}
cd ALNScanner_v5
```

**Create these empty files:**
```
ALNScanner_v5/
├── ALNScanner_v5.ino
├── config.h
├── hal/
│   ├── RFIDReader.h
│   ├── DisplayDriver.h
│   ├── AudioDriver.h
│   ├── TouchDriver.h
│   └── SDCard.h
├── services/
│   ├── OrchestratorService.h
│   ├── TokenService.h
│   ├── ConfigService.h
│   └── SerialService.h
├── models/
│   ├── Token.h
│   ├── Config.h
│   └── ConnectionState.h
├── ui/
│   ├── UIStateMachine.h
│   ├── Screen.h
│   └── screens/
│       ├── ReadyScreen.h
│       ├── StatusScreen.h
│       ├── TokenDisplayScreen.h
│       └── ProcessingScreen.h
└── Application.h
```

**Verification:**
- [ ] All directories exist
- [ ] All files created (empty is OK)
- [ ] File count = 20

#### Step 0.3: Extract config.h (Foundation)

**Source:** ALNScanner1021_Orchestrator.ino lines 28-56 (pin definitions)

**Target:** ALNScanner_v5/config.h

**Exact Content:**
```cpp
#pragma once

// ═══ HARDWARE CONFIGURATION ═══════════════════════════════════════

// Pin Definitions (CYD ESP32-2432S028R)
namespace pins {
    // SD Card (VSPI - Hardware SPI Bus 2)
    constexpr uint8_t SD_SCK   = 18;
    constexpr uint8_t SD_MISO  = 19;
    constexpr uint8_t SD_MOSI  = 23;
    constexpr uint8_t SD_CS    = 5;

    // Touch Controller (XPT2046)
    constexpr uint8_t TOUCH_CS  = 33;
    constexpr uint8_t TOUCH_IRQ = 36;

    // RFID Reader (Software SPI - MFRC522)
    constexpr uint8_t RFID_SCK  = 22;
    constexpr uint8_t RFID_MOSI = 27;
    constexpr uint8_t RFID_MISO = 35;
    constexpr uint8_t RFID_SS   = 3;

    // Audio (I2S DAC)
    constexpr uint8_t AUDIO_BCLK = 26;
    constexpr uint8_t AUDIO_LRC  = 25;
    constexpr uint8_t AUDIO_DIN  = 22;
}

// ═══ TIMING CONSTANTS ═════════════════════════════════════════════

namespace timing {
    constexpr uint32_t RFID_SCAN_INTERVAL_MS = 500;
    constexpr uint32_t TOUCH_DEBOUNCE_MS = 50;
    constexpr uint32_t DOUBLE_TAP_TIMEOUT_MS = 500;
    constexpr uint32_t TOUCH_PULSE_WIDTH_THRESHOLD_US = 10000;
    constexpr uint32_t PROCESSING_MODAL_TIMEOUT_MS = 2500;
    constexpr uint32_t ORCHESTRATOR_CHECK_INTERVAL_MS = 10000;
}

// ═══ QUEUE CONFIGURATION ══════════════════════════════════════════

namespace queue_config {
    constexpr int MAX_QUEUE_SIZE = 100;
    constexpr int BATCH_UPLOAD_SIZE = 10;
    constexpr const char* QUEUE_FILE = "/queue.jsonl";
    constexpr const char* QUEUE_TEMP_FILE = "/queue.tmp";
}

// ═══ FILE PATHS ═══════════════════════════════════════════════════

namespace paths {
    constexpr const char* CONFIG_FILE = "/config.txt";
    constexpr const char* TOKEN_DB_FILE = "/tokens.json";
    constexpr const char* DEVICE_ID_FILE = "/device_id.txt";
    constexpr const char* IMAGES_DIR = "/images/";
    constexpr const char* AUDIO_DIR = "/AUDIO/";
}

// ═══ SIZE LIMITS ══════════════════════════════════════════════════

namespace limits {
    constexpr int MAX_TOKENS = 50;
    constexpr int MAX_TOKEN_DB_SIZE = 50000; // 50KB
}

// ═══ DEBUG CONFIGURATION ══════════════════════════════════════════

// Compile-time debug flags (reduces flash in production)
#ifndef DEBUG_MODE
  #define LOG_VERBOSE(...) ((void)0)
  #define LOG_DEBUG(...) ((void)0)
#else
  #define LOG_VERBOSE(...) Serial.printf(__VA_ARGS__)
  #define LOG_DEBUG(...) Serial.printf(__VA_ARGS__)
#endif

// Always compile info/error logs
#define LOG_INFO(...) Serial.printf(__VA_ARGS__)
#define LOG_ERROR(tag, msg) logError(F(tag), F(msg))
```

**Verification:**
- [ ] File compiles: `arduino-cli compile --preprocess ALNScanner_v5.ino`
- [ ] All constants use constexpr (not #define)
- [ ] Namespace organization clear
- [ ] No magic numbers in other files will be allowed

---

### PHASE 1: HAL Layer Foundation (Week 2)

**Objective:** Extract hardware drivers with clean interfaces

**Dependencies:** config.h must exist

**Flash Budget:** Neutral (code reorganization only)

---

#### COMPONENT 1.1: RFIDReader Class

**Source Lines:** ALNScanner1021_Orchestrator.ino
- Software SPI functions: 242-472
- RFID operations: 474-690
- NDEF extraction: 692-874

**Interface Contract (RFIDReader.h):**
```cpp
#pragma once
#include <MFRC522.h>
#include "config.h"

class RFIDReader {
public:
    struct ScanResult {
        bool success;
        String tokenId;          // NDEF text or UID hex
        uint8_t uid[10];
        uint8_t uidSize;
        uint8_t sak;

        ScanResult() : success(false), uidSize(0), sak(0) {}
    };

    struct Stats {
        uint32_t totalScans;
        uint32_t successfulScans;
        uint32_t failedScans;
        uint32_t retryCount;
        uint32_t collisionErrors;
    };

    // Lifecycle
    RFIDReader();
    bool initialize();           // Call from setup()
    void enableRFField();
    void disableRFField();

    // Scanning
    ScanResult scanCard();       // Blocking, returns immediately if no card
    void halt();                 // Halt current card

    // Diagnostics
    const Stats& getStats() const { return _stats; }
    void dumpRegisters();

private:
    // Software SPI primitives (inline)
    inline byte softSPI_transfer(byte data);
    void SoftSPI_WriteRegister(MFRC522::PCD_Register reg, byte value);
    byte SoftSPI_ReadRegister(MFRC522::PCD_Register reg);
    // ... (all current SoftSPI functions)

    // RFID protocol
    MFRC522::StatusCode PICC_RequestA(byte* bufferATQA, byte* bufferSize);
    MFRC522::StatusCode PICC_Select(MFRC522::Uid* uid);
    String extractNDEFText(const MFRC522::Uid& uid);

    // State
    bool _initialized;
    bool _rfFieldEnabled;
    Stats _stats;
    portMUX_TYPE _spiMux;
};
```

**Implementation Requirements:**

1. **Constructor:** Initialize _initialized=false, _rfFieldEnabled=false, _stats to zero
2. **initialize():**
   - Configure GPIO pins (use pins::RFID_*)
   - Soft reset MFRC522
   - Configure registers (copy from lines 2540-2580)
   - Return true if version register reads 0x92/0x91/0x90/0x88
3. **scanCard():**
   - Return early if !_initialized
   - Enable RF field
   - PICC_RequestA → check ATQA
   - PICC_Select with retry logic (3 attempts)
   - extractNDEFText if SAK=0x00
   - Fallback to UID hex string if no NDEF
   - Update _stats
   - Disable RF field if no card
   - Return ScanResult
4. **Private methods:** Direct copy from v4.1 (lines 242-874) with NO changes to logic

**Migration Checklist:**
- [ ] Copy all SoftSPI functions verbatim (lines 242-472)
- [ ] Copy PICC_RequestA, PICC_Select, extractNDEFText (lines 474-874)
- [ ] Replace all global `rfidStats` with `_stats` member
- [ ] Replace all `RFID_*` pins with `pins::RFID_*`
- [ ] Test: Create minimal .ino that calls scanCard() in loop()

**Verification:**
```cpp
// test/test_rfid.ino
#include "hal/RFIDReader.h"

RFIDReader rfid;

void setup() {
    Serial.begin(115200);
    if (rfid.initialize()) {
        Serial.println("RFID initialized");
    } else {
        Serial.println("RFID init failed");
    }
}

void loop() {
    auto result = rfid.scanCard();
    if (result.success) {
        Serial.printf("Scanned: %s\n", result.tokenId.c_str());
        rfid.halt();
        delay(2000);
    }
    delay(500);
}
```
- [ ] Compiles without errors
- [ ] Physical scan produces tokenId
- [ ] Stats increment correctly

---

#### COMPONENT 1.2: DisplayDriver Class

**Source Lines:**
- BMP drawing: 924-1091
- TFT operations: scattered throughout

**Interface Contract (DisplayDriver.h):**
```cpp
#pragma once
#include <TFT_eSPI.h>
#include "config.h"

class SDCard; // Forward declaration for dependency

class DisplayDriver {
public:
    DisplayDriver(SDCard& sd);

    // Lifecycle
    void initialize();

    // Drawing primitives
    void fillScreen(uint16_t color);
    void setCursor(uint16_t x, uint16_t y);
    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t size);
    void print(const String& text);
    void println(const String& text);

    // High-level rendering
    bool drawBMP(const String& path);      // Returns false if file missing
    void drawText(uint16_t x, uint16_t y, const String& text,
                  uint16_t color, uint8_t size);

    // Direct TFT access (for custom screens)
    TFT_eSPI& tft() { return _tft; }

private:
    TFT_eSPI _tft;
    SDCard& _sd;

    // BMP rendering internals
    struct BMPHeader {
        uint32_t width;
        uint32_t height;
        uint16_t bpp;
        uint32_t dataOffset;
        bool valid;
    };

    BMPHeader parseBMPHeader(File& f);
    void renderBMPRow(File& f, uint32_t y, uint32_t width, uint8_t* rowBuffer);
};
```

**Implementation Requirements:**

1. **Constructor:** Store SDCard reference, initialize TFT
2. **initialize():**
   - tft.init()
   - tft.setRotation(0)
   - fillScreen(TFT_BLACK)
3. **drawBMP():**
   - Acquire SD mutex via _sd.lock()/unlock()
   - Parse BMP header (validate magic bytes, 24-bit, no compression)
   - Allocate row buffer
   - Render bottom-to-top (lines 1043-1085 - CRITICAL SPI ordering)
   - Return true on success, false on error
4. **CRITICAL:** SD read → TFT write ordering (never lock TFT while reading SD)

**Migration Checklist:**
- [ ] Copy drawBmp() logic (lines 924-1091) to drawBMP()
- [ ] Replace SD.open() with _sd.open() wrapper
- [ ] Add mutex acquire/release around SD operations
- [ ] Test: Draw known-good BMP file

**Verification:**
- [ ] Compiles
- [ ] Displays BMP without deadlock
- [ ] Missing file returns false (not crash)

---

#### COMPONENT 1.3: SDCard Class (Mutex Wrapper)

**Source Lines:**
- Mutex helpers: 1240-1257
- SD operations: scattered

**Interface Contract (SDCard.h):**
```cpp
#pragma once
#include <SD.h>
#include <SPI.h>
#include "config.h"

class SDCard {
public:
    SDCard();

    // Lifecycle
    bool initialize();
    bool isPresent() const { return _present; }

    // File operations (mutex-protected)
    File open(const String& path, const char* mode = FILE_READ);
    bool exists(const String& path);
    bool remove(const String& path);
    bool rename(const String& oldPath, const String& newPath);

    // RAII mutex lock
    class Lock {
    public:
        Lock(SDCard& sd, const char* caller, uint32_t timeoutMs = 500);
        ~Lock();
        bool acquired() const { return _acquired; }
    private:
        SDCard& _sd;
        const char* _caller;
        bool _acquired;
    };

private:
    friend class Lock;
    bool _present;
    SemaphoreHandle_t _mutex;
    SPIClass _spi;

    bool takeMutex(const char* caller, uint32_t timeoutMs);
    void giveMutex();
};
```

**Implementation Requirements:**

1. **initialize():**
   - Create mutex: `_mutex = xSemaphoreCreateMutex()`
   - Initialize SPI: `_spi.begin(pins::SD_SCK, pins::SD_MISO, pins::SD_MOSI)`
   - Mount SD: `SD.begin(pins::SD_CS, _spi)`
   - Set _present flag
2. **File operations:** All call takeMutex() before SD.* call, giveMutex() after
3. **Lock RAII:** Constructor calls takeMutex(), destructor calls giveMutex()

**Usage Example:**
```cpp
// Replaces manual mutex management
{
    SDCard::Lock lock(sd, "drawBMP", 1000);
    if (lock.acquired()) {
        File f = sd.open("/image.bmp");
        // ... use file ...
    } // auto-unlock on scope exit
}
```

**Migration Checklist:**
- [ ] Copy mutex helpers (lines 1240-1257) to takeMutex/giveMutex
- [ ] Wrap all SD operations in takeMutex/giveMutex
- [ ] Create Lock class for RAII
- [ ] Test: Concurrent access from 2 tasks

**Verification:**
- [ ] No deadlocks with DisplayDriver
- [ ] Lock timeout works correctly
- [ ] Auto-unlock on exception/early return

---

#### COMPONENT 1.4: AudioDriver Class

**Source Lines:** 1093-1170

**Interface Contract (AudioDriver.h):**
```cpp
#pragma once
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "config.h"

class SDCard; // Forward declaration

class AudioDriver {
public:
    AudioDriver(SDCard& sd);
    ~AudioDriver();

    // Playback control
    bool play(const String& path);    // Returns false if file missing
    void stop();
    bool isPlaying() const;
    void update();                     // Call in loop() to service playback

private:
    SDCard& _sd;
    AudioGeneratorWAV* _wav;
    AudioFileSourceSD* _file;
    AudioOutputI2S* _out;
    bool _initialized;

    void lazyInit(); // Defer I2S init until first use (prevents beeping)
};
```

**Implementation Requirements:**

1. **Constructor:** Initialize pointers to nullptr, _initialized=false
2. **lazyInit():** Create AudioOutputI2S on first play() call (lines 1100-1104)
3. **play():**
   - Call lazyInit() if !_initialized
   - Stop current playback if active
   - Acquire SD lock
   - Open file via _sd.open()
   - Create WAV generator
   - Return success/failure
4. **update():** Call _wav->loop() if playing, stop when complete
5. **Destructor:** Clean up all audio objects

**Migration Checklist:**
- [ ] Copy startAudio/stopAudio logic (lines 1094-1170)
- [ ] Add lazyInit() pattern (prevents GPIO 27 beeping)
- [ ] Replace SD.open() with _sd.open()
- [ ] Test: Play WAV file

**Verification:**
- [ ] No beeping on boot
- [ ] Audio plays correctly
- [ ] Stops cleanly
- [ ] Multiple plays work

---

#### COMPONENT 1.5: TouchDriver Class

**Source Lines:**
- ISR: 1187-1190
- Pulse measurement: 1192-1207
- Touch handling: scattered in loop()

**Interface Contract (TouchDriver.h):**
```cpp
#pragma once
#include "config.h"

class TouchDriver {
public:
    struct TouchEvent {
        bool valid;              // False if EMI or debounced
        uint32_t timestamp;      // millis() when touch occurred

        TouchEvent() : valid(false), timestamp(0) {}
    };

    TouchDriver();

    // Lifecycle
    void initialize();

    // Event handling
    TouchEvent checkTouch();     // Returns event or invalid if no touch

    // Timing helpers
    bool isDoubleTap(uint32_t lastTouchMs) const;

private:
    volatile bool _interruptOccurred;
    volatile uint32_t _interruptTime;
    uint32_t _lastDebounceTime;

    static void IRAM_ATTR touchISR();
    static TouchDriver* _instance; // For ISR access

    uint32_t measurePulseWidth();
    bool isValidTouch(uint32_t pulseWidthUs);
};
```

**Implementation Requirements:**

1. **initialize():**
   - Set _instance = this (for ISR)
   - Configure GPIO: `pinMode(pins::TOUCH_IRQ, INPUT)` (NOT INPUT_PULLUP!)
   - Attach interrupt: `attachInterrupt(digitalPinToInterrupt(pins::TOUCH_IRQ), touchISR, FALLING)`
2. **checkTouch():**
   - Return invalid if !_interruptOccurred
   - Clear flag
   - Measure pulse width (lines 1195-1207)
   - Filter EMI: reject if <10ms pulse (lines 1585-1590)
   - Debounce: reject if <50ms since last (lines 1596-1600)
   - Return valid TouchEvent with timestamp
3. **isDoubleTap():** Check if (millis() - lastTouchMs) < timing::DOUBLE_TAP_TIMEOUT_MS

**Migration Checklist:**
- [ ] Copy ISR (lines 1187-1190)
- [ ] Copy pulse measurement (lines 1195-1207)
- [ ] Copy EMI filter logic (lines 1585-1590)
- [ ] Test: Touch detection without false positives

**Verification:**
- [ ] Physical touch registers
- [ ] WiFi EMI filtered out
- [ ] Debouncing works
- [ ] Double-tap detection accurate

---

### PHASE 1 COMPLETION CHECKLIST

- [ ] All 5 HAL components compile independently
- [ ] Each component has test sketch
- [ ] Flash usage measured: ~1,210,000 bytes (minor increase from class overhead OK)
- [ ] All hardware functionality verified on physical device
- [ ] No global variables (except in test sketches)

**Proceed to Phase 2 only when all boxes checked.**

---

### PHASE 2: Data Models (Week 3 - Days 1-2)

**Objective:** Define clean data structures

**Dependencies:** config.h

**Flash Budget:** Neutral

---

#### MODEL 2.1: ConnectionState.h

**Interface:**
```cpp
#pragma once

enum class ConnectionState : uint8_t {
    DISCONNECTED = 0,
    WIFI_CONNECTED = 1,
    ORCHESTRATOR_CONNECTED = 2
};

// Thread-safe state holder
class ConnectionStateHolder {
public:
    ConnectionStateHolder();

    void set(ConnectionState state);
    ConnectionState get() const;

    const char* toString() const;

private:
    volatile ConnectionState _state;
    portMUX_TYPE _mutex;
};
```

**Migration:** Extract from lines 141-148, add mutex protection (lines 1213-1225)

---

#### MODEL 2.2: Config.h

**Interface:**
```cpp
#pragma once
#include <Arduino.h>

struct DeviceConfig {
    // Network
    String wifiSSID;
    String wifiPassword;
    String orchestratorURL;

    // Identity
    String teamID;
    String deviceID;

    // Behavior
    bool syncTokens;
    bool debugMode;

    DeviceConfig() : syncTokens(true), debugMode(false) {}

    bool isValid() const;
    String getValidationError() const;
};
```

**Migration:** Extract from lines 131-138

---

#### MODEL 2.3: Token.h

**Interface:**
```cpp
#pragma once
#include <Arduino.h>

struct TokenMetadata {
    String tokenId;
    String video;              // Empty if not video token
    String image;              // BMP path
    String audio;              // WAV path
    String processingImage;    // Processing modal image

    bool isVideoToken() const { return video.length() > 0 && video != "null"; }
    String getImagePath() const;
    String getAudioPath() const;
    String getProcessingImagePath() const;
};

struct ScanData {
    String tokenId;
    String teamId;
    String deviceId;
    String timestamp;

    String toJSON() const;
    static ScanData fromJSON(const String& json);
};
```

**Migration:** Extract from lines 182-189 (TokenMetadata), create ScanData from lines 166-172

---

### PHASE 3: Service Layer (Week 3 - Days 3-7)

**Objective:** Extract business logic into services

**Dependencies:** Phase 1 HAL, Phase 2 Models

**Flash Budget:** -20KB (HTTP consolidation, string deduplication)

---

#### SERVICE 3.1: ConfigService

**Source Lines:** 1323-1542

**Interface:**
```cpp
#pragma once
#include "../models/Config.h"
#include "../hal/SDCard.h"

class ConfigService {
public:
    ConfigService(SDCard& sd);

    // Lifecycle
    bool load();                          // Parse from SD
    bool save();                          // Write to SD

    // Accessors
    const DeviceConfig& get() const { return _config; }

    // Runtime editing
    bool set(const String& key, const String& value);

    // Utilities
    String generateDeviceId();            // From MAC address
    bool validate(String* errorOut = nullptr);

private:
    SDCard& _sd;
    DeviceConfig _config;

    void parseLine(const String& line);
};
```

**Implementation Requirements:**

1. **load():** Parse /config.txt (lines 1323-1440), return true if valid
2. **save():** Write config to SD (lines 3236-3288)
3. **set():** Runtime config editing (lines 3157-3234)
4. **validate():** Validation logic (lines 1443-1542)
5. **generateDeviceId():** MAC to device ID (lines 1295-1320)

**Migration Checklist:**
- [ ] Copy parseConfigFile() → load() (lines 1323-1440)
- [ ] Copy validateConfig() → validate() (lines 1443-1542)
- [ ] Copy generateDeviceId() (lines 1295-1320)
- [ ] Copy SAVE_CONFIG logic → save() (lines 3236-3288)
- [ ] Test: Load sample config, validate, save

---

#### SERVICE 3.2: TokenService

**Source Lines:** 2092-2146 (load), 2139-2177 (helpers)

**Interface:**
```cpp
#pragma once
#include "../models/Token.h"
#include "../hal/SDCard.h"
#include <vector>

class TokenService {
public:
    TokenService(SDCard& sd);

    // Lifecycle
    bool loadDatabase();                   // Load from SD

    // Queries
    const TokenMetadata* get(const String& tokenId) const;
    int getCount() const { return _tokens.size(); }

    // Sync from orchestrator
    bool syncFromOrchestrator(const String& orchestratorURL);

private:
    SDCard& _sd;
    std::vector<TokenMetadata> _tokens;   // Dynamic instead of fixed array

    bool parseTokenJSON(const String& json);
};
```

**Implementation Requirements:**

1. **loadDatabase():** Parse tokens.json (lines 2093-2137)
2. **get():** Linear search by tokenId (lines 2139-2146)
3. **syncFromOrchestrator():** HTTP GET + save (lines 1571-1634)

**Migration Checklist:**
- [ ] Copy loadTokenDatabase() → loadDatabase()
- [ ] Copy getTokenMetadata() → get()
- [ ] Copy syncTokenDatabase() → syncFromOrchestrator()
- [ ] Replace fixed array with std::vector
- [ ] Test: Load tokens, lookup by ID

---

#### SERVICE 3.3: OrchestratorService (Complex)

**Source Lines:**
- WiFi: 2365-2444
- HTTP: 1650-1716 (sendScan), 1725-1824 (batch upload)
- Queue: 1866-1922 (write), 1924-1994 (read), 2004-2089 (remove)
- Background task: 2447-2496

**Interface:**
```cpp
#pragma once
#include "../models/Config.h"
#include "../models/Token.h"
#include "../models/ConnectionState.h"
#include "../hal/SDCard.h"
#include <WiFi.h>
#include <HTTPClient.h>

class OrchestratorService {
public:
    OrchestratorService(SDCard& sd, const DeviceConfig& config);

    // Lifecycle
    bool initializeWiFi();                 // Connect to WiFi
    void startBackgroundTask();            // Start FreeRTOS task

    // Scan operations
    bool sendScan(const ScanData& scan);   // HTTP POST, returns false if fail
    void queueScan(const ScanData& scan);  // Write to queue file

    // State
    ConnectionState getState() const;
    int getQueueSize() const;

    // Health
    bool checkHealth();                    // Ping /health endpoint

private:
    SDCard& _sd;
    const DeviceConfig& _config;
    ConnectionStateHolder _connState;

    // Queue management
    struct QueueManager {
        int size;
        portMUX_TYPE sizeMutex;

        QueueManager() : size(0), sizeMutex(portMUX_INITIALIZER_UNLOCKED) {}
        void updateSize(int delta);
        int getSize() const;
    } _queue;

    // HTTP helpers (CONSOLIDATE DUPLICATION)
    class HTTPClientWrapper {
    public:
        bool post(const String& url, const String& json, int* responseCode);
        bool get(const String& url, String* response, int* responseCode);
    private:
        HTTPClient _client;
        void configureClient(const String& url);
    };
    HTTPClientWrapper _http;

    // Queue operations
    int countQueueEntries();
    bool uploadQueueBatch();
    void removeUploadedEntries(int count);

    // Background task
    static void backgroundTaskWrapper(void* param);
    void backgroundTaskLoop();
};
```

**Implementation Requirements:**

1. **initializeWiFi():** WiFi.begin() + event handlers (lines 2365-2444)
2. **sendScan():** HTTP POST to /api/scan (lines 1650-1716)
3. **queueScan():** Append to JSONL file (lines 1866-1922)
4. **uploadQueueBatch():** Batch POST to /api/scan/batch (lines 1725-1824)
5. **removeUploadedEntries():** Stream-based FIFO removal (lines 2004-2089)
6. **backgroundTaskLoop():** Health check + auto-upload (lines 2447-2496)
7. **HTTPClientWrapper:** Consolidate 3 duplicate HTTP setups

**CRITICAL:** This service has most flash savings potential via HTTP consolidation

**Migration Checklist:**
- [ ] Extract WiFi init (lines 2365-2444)
- [ ] Extract sendScan (lines 1650-1716)
- [ ] Extract queueScan (lines 1866-1922)
- [ ] Extract uploadQueueBatch (lines 1725-1824)
- [ ] Extract removeUploadedEntries (lines 2004-2089)
- [ ] Extract background task (lines 2447-2496)
- [ ] Create HTTPClientWrapper (CONSOLIDATE 3 copies)
- [ ] Test: Send scan online
- [ ] Test: Queue scan offline
- [ ] Test: Batch upload
- [ ] Test: Queue removal

**Flash Measurement:** This phase should show -15KB from HTTP consolidation

---

#### SERVICE 3.4: SerialService

**Source Lines:** 2927-3394

**Interface:**
```cpp
#pragma once
#include "../models/Config.h"
#include <functional>

class SerialService {
public:
    using CommandHandler = std::function<void(const String& args)>;

    SerialService();

    // Lifecycle
    void initialize(uint32_t baudRate = 115200);

    // Command registration
    void registerCommand(const String& cmd, CommandHandler handler);

    // Processing
    void processCommands();              // Call in loop()

    // Built-in commands
    void registerBuiltinCommands(/* dependencies */);

private:
    struct Command {
        String name;
        CommandHandler handler;
    };
    std::vector<Command> _commands;

    void handleCommand(const String& cmd);
};
```

**Implementation Requirements:**

1. **initialize():** Serial.begin(baudRate)
2. **registerCommand():** Add to _commands vector
3. **processCommands():** Serial.available() → readStringUntil('\n') → handleCommand()
4. **handleCommand():** Linear search in _commands, call handler
5. **registerBuiltinCommands():** Register all commands from lines 2938-3392

**Migration Checklist:**
- [ ] Copy processSerialCommands() → processCommands() (lines 2933-3394)
- [ ] Convert if/else chain to command registry
- [ ] Test: HELP command
- [ ] Test: STATUS command
- [ ] Test: Unknown command

---

### PHASE 4: UI Layer (Week 4)

**Objective:** State machine for screen management

**Dependencies:** Phase 1 HAL, Phase 2 Models

**Flash Budget:** -10KB (remove duplicate screen rendering)

---

#### UI 4.1: Screen Base Class

**Interface:**
```cpp
#pragma once
#include "../hal/DisplayDriver.h"

class Screen {
public:
    virtual ~Screen() = default;

    // Template method pattern
    void render(DisplayDriver& display) {
        onPreRender(display);
        onRender(display);
        onPostRender(display);
    }

protected:
    virtual void onPreRender(DisplayDriver& display) {}
    virtual void onRender(DisplayDriver& display) = 0; // Pure virtual
    virtual void onPostRender(DisplayDriver& display) {}
};
```

---

#### UI 4.2: Screen Implementations

**ReadyScreen.h:**
```cpp
#pragma once
#include "../Screen.h"

class ReadyScreen : public Screen {
public:
    ReadyScreen(bool rfidReady, bool debugMode);

protected:
    void onRender(DisplayDriver& display) override;

private:
    bool _rfidReady;
    bool _debugMode;
};
```

**Source:** drawReadyScreen() lines 2326-2362

---

**StatusScreen.h:**
```cpp
#pragma once
#include "../Screen.h"
#include "../../models/ConnectionState.h"

class StatusScreen : public Screen {
public:
    struct SystemStatus {
        ConnectionState connState;
        String wifiSSID;
        String localIP;
        int queueSize;
        int maxQueueSize;
        String teamID;
        String deviceID;
    };

    StatusScreen(const SystemStatus& status);

protected:
    void onRender(DisplayDriver& display) override;

private:
    SystemStatus _status;
};
```

**Source:** displayStatusScreen() lines 2238-2315

---

**TokenDisplayScreen.h:**
```cpp
#pragma once
#include "../Screen.h"
#include "../../models/Token.h"
#include "../../hal/AudioDriver.h"

class TokenDisplayScreen : public Screen {
public:
    TokenDisplayScreen(const TokenMetadata& token, SDCard& sd, AudioDriver& audio);

    void update();  // Call in loop() for audio playback

protected:
    void onRender(DisplayDriver& display) override;

private:
    TokenMetadata _token;
    SDCard& _sd;
    AudioDriver& _audio;
};
```

**Source:** processTokenScan() lines 3511-3559

---

**ProcessingScreen.h:**
```cpp
#pragma once
#include "../Screen.h"
#include "../../models/Token.h"

class ProcessingScreen : public Screen {
public:
    ProcessingScreen(const String& imagePath);

    bool hasTimedOut() const;  // Check 2.5s timeout

protected:
    void onRender(DisplayDriver& display) override;

private:
    String _imagePath;
    uint32_t _displayTime;
};
```

**Source:** displayProcessingImage() lines 2179-2235

---

#### UI 4.3: UIStateMachine

**Interface:**
```cpp
#pragma once
#include "Screen.h"
#include "screens/ReadyScreen.h"
#include "screens/StatusScreen.h"
#include "screens/TokenDisplayScreen.h"
#include "screens/ProcessingScreen.h"
#include "../hal/DisplayDriver.h"
#include "../hal/TouchDriver.h"
#include "../hal/AudioDriver.h"
#include <memory>

class UIStateMachine {
public:
    enum class State {
        READY,
        SHOWING_STATUS,
        DISPLAYING_TOKEN,
        PROCESSING_VIDEO
    };

    UIStateMachine(DisplayDriver& display, TouchDriver& touch, AudioDriver& audio, SDCard& sd);

    // State transitions
    void showReady(bool rfidReady, bool debugMode);
    void showStatus(const StatusScreen::SystemStatus& status);
    void showToken(const TokenMetadata& token);
    void showProcessing(const String& imagePath);

    // Event handling
    void handleTouch();
    void update();  // Call in loop()

    State getState() const { return _state; }

private:
    DisplayDriver& _display;
    TouchDriver& _touch;
    AudioDriver& _audio;
    SDCard& _sd;

    State _state;
    std::unique_ptr<Screen> _currentScreen;

    // Touch handling state
    uint32_t _lastTouchTime;
    bool _lastTouchWasValid;

    void transitionTo(State newState, std::unique_ptr<Screen> screen);
    void handleTouchInState(State state);
};
```

**Implementation Requirements:**

1. **showReady():** Create ReadyScreen, transitionTo(READY)
2. **showStatus():** Create StatusScreen, transitionTo(SHOWING_STATUS)
3. **showToken():** Create TokenDisplayScreen, transitionTo(DISPLAYING_TOKEN)
4. **showProcessing():** Create ProcessingScreen, transitionTo(PROCESSING_VIDEO)
5. **handleTouch():**
   - Get touch event from _touch.checkTouch()
   - Route to handleTouchInState(currentState)
   - READY: single-tap → showStatus
   - SHOWING_STATUS: any tap → showReady
   - DISPLAYING_TOKEN: double-tap → showReady
   - PROCESSING_VIDEO: ignored (auto-timeout)
6. **update():**
   - If DISPLAYING_TOKEN: update audio playback
   - If PROCESSING_VIDEO: check timeout, showReady if expired

**Migration:** Extract touch logic from loop() lines 3577-3664

---

### PHASE 5: Application Integration (Week 5)

**Objective:** Wire everything together

**Dependencies:** All previous phases

**Flash Budget:** -15KB (remove global variables, inline small helpers)

---

#### APPLICATION: Application.h

**Interface:**
```cpp
#pragma once
#include "config.h"
#include "hal/RFIDReader.h"
#include "hal/DisplayDriver.h"
#include "hal/AudioDriver.h"
#include "hal/TouchDriver.h"
#include "hal/SDCard.h"
#include "services/ConfigService.h"
#include "services/TokenService.h"
#include "services/OrchestratorService.h"
#include "services/SerialService.h"
#include "ui/UIStateMachine.h"

class Application {
public:
    Application();

    void setup();
    void loop();

private:
    // HAL Layer (order matters for construction)
    SDCard _sd;
    DisplayDriver _display;
    AudioDriver _audio;
    TouchDriver _touch;
    RFIDReader _rfid;

    // Services
    ConfigService _config;
    TokenService _tokens;
    OrchestratorService _orchestrator;
    SerialService _serial;

    // UI
    UIStateMachine _ui;

    // State
    bool _debugMode;
    bool _rfidInitialized;
    uint32_t _lastRFIDScan;

    // Event handlers
    void processRFIDScan();
    void processTouch();
    void processSerialCommands();

    // Initialization helpers
    bool initializeHardware();
    bool initializeServices();
    void registerSerialCommands();
    void startBackgroundTasks();
};
```

**Constructor:**
```cpp
Application::Application()
    : _display(_sd)           // DisplayDriver needs SDCard
    , _audio(_sd)             // AudioDriver needs SDCard
    , _touch()
    , _rfid()
    , _config(_sd)            // ConfigService needs SDCard
    , _tokens(_sd)            // TokenService needs SDCard
    , _orchestrator(_sd, _config.get())  // Needs both
    , _serial()
    , _ui(_display, _touch, _audio, _sd)
    , _debugMode(false)
    , _rfidInitialized(false)
    , _lastRFIDScan(0)
{}
```

**setup() Implementation:**
```cpp
void Application::setup() {
    Serial.begin(115200);

    // Boot override (lines 2627-2677)
    // ... handle DEBUG_MODE override ...

    if (!initializeHardware()) {
        LOG_ERROR("SETUP", "Hardware init failed");
        return;
    }

    if (!initializeServices()) {
        LOG_ERROR("SETUP", "Services init failed");
    }

    registerSerialCommands();
    startBackgroundTasks();

    // Initial screen
    _ui.showReady(_rfidInitialized, _debugMode);
}
```

**loop() Implementation:**
```cpp
void loop() {
    _serial.processCommands();     // Always responsive
    _audio.update();               // Service audio playback
    _ui.update();                  // Check timeouts, update screens

    processTouch();
    processRFIDScan();

    _serial.processCommands();     // Process again for responsiveness
}
```

**processRFIDScan() Logic:**
```cpp
void Application::processRFIDScan() {
    // Guard: skip if not initialized or screen busy
    if (!_rfidInitialized) return;
    if (_ui.getState() != UIStateMachine::State::READY) return;

    // Rate limit (500ms between scans)
    if (millis() - _lastRFIDScan < timing::RFID_SCAN_INTERVAL_MS) return;
    _lastRFIDScan = millis();

    // Scan for card
    auto result = _rfid.scanCard();
    if (!result.success) return;

    // Process scan
    _rfid.halt();

    // Send to orchestrator (or queue if offline)
    ScanData scan{result.tokenId, _config.get().teamID,
                  _config.get().deviceID, generateTimestamp()};

    if (_orchestrator.getState() == ConnectionState::ORCHESTRATOR_CONNECTED) {
        if (!_orchestrator.sendScan(scan)) {
            _orchestrator.queueScan(scan);
        }
    } else {
        _orchestrator.queueScan(scan);
    }

    // Display appropriate screen
    const TokenMetadata* meta = _tokens.get(result.tokenId);
    if (meta && meta->isVideoToken()) {
        String procImg = meta->getProcessingImagePath();
        _ui.showProcessing(procImg);
    } else if (meta) {
        _ui.showToken(*meta);
    } else {
        // Fallback: construct from UID
        TokenMetadata fallback;
        fallback.tokenId = result.tokenId;
        fallback.image = "/images/" + result.tokenId + ".bmp";
        fallback.audio = "/AUDIO/" + result.tokenId + ".wav";
        _ui.showToken(fallback);
    }
}
```

**Migration Checklist:**
- [ ] Create Application class with all members
- [ ] Move setup() logic (lines 2615-2925) to Application::setup()
- [ ] Move loop() logic (lines 3563-3840) to Application::loop()
- [ ] Extract processRFIDScan from loop (lines 3678-3839)
- [ ] Register all serial commands (lines 2938-3392)
- [ ] Test: End-to-end scan flow
- [ ] Test: All serial commands
- [ ] Test: Touch interactions

---

#### MAIN: ALNScanner_v5.ino

**Complete File:**
```cpp
#include "Application.h"

Application app;

void setup() {
    app.setup();
}

void loop() {
    app.loop();
}
```

**Total lines:** 9 (down from 3839!)

**Verification:**
- [ ] Compiles successfully
- [ ] Flash usage measured
- [ ] Physical device boots
- [ ] RFID scanning works
- [ ] All screens display
- [ ] Touch navigation works
- [ ] Serial commands work
- [ ] WiFi connects
- [ ] Orchestrator sends/queues scans

---

### PHASE 6: Optimization (Week 6)

**Objective:** Reduce flash to <87%

**Dependencies:** Phase 5 complete

**Flash Budget:** Final -15KB from PROGMEM + compile flags

---

#### OPTIMIZATION 6.1: PROGMEM Strings

**Task:** Move all string literals >20 chars to flash

**Strategy:**
1. Find all Serial.printf/println with long strings
2. Replace with F() macro
3. Create string tables for repeated messages

**Example:**
```cpp
// Before (RAM):
Serial.println("[QUEUE] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD lock");

// After (Flash):
Serial.println(F("[QUEUE] ✗✗✗ FAILURE ✗✗✗ Could not acquire SD lock"));
```

**Automated Search:**
```bash
grep -r "Serial.print" . | grep -v "F(" | wc -l
# Apply F() macro to all results
```

**Expected Savings:** 15KB

---

#### OPTIMIZATION 6.2: Compile-Time Debug Flags

**Task:** Make verbose logging removable

**config.h Addition:**
```cpp
// Add at top
#ifndef DEBUG_MODE
  #define DEBUG_MODE 0  // Production: disable verbose logs
#endif

#if DEBUG_MODE
  #define LOG_VERBOSE(...) Serial.printf(__VA_ARGS__)
#else
  #define LOG_VERBOSE(...) ((void)0)  // Compile out
#endif
```

**Migration:**
```cpp
// Before:
Serial.printf("[RFID-DIAG] Post-select state:\n");
Serial.printf("  ErrorReg: 0x%02X\n", ...);

// After:
LOG_VERBOSE("[RFID-DIAG] Post-select state:\n");
LOG_VERBOSE("  ErrorReg: 0x%02X\n", ...);
```

**Production Build:**
```bash
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DDEBUG_MODE=0" .
```

**Expected Savings:** 30KB

---

#### OPTIMIZATION 6.3: Inline Small Functions

**Task:** Move getters/setters to header files for inlining

**Example:**
```cpp
// ConnectionStateHolder.h - already inline
inline ConnectionState get() const {
    portENTER_CRITICAL(&_mutex);
    ConnectionState s = _state;
    portEXIT_CRITICAL(&_mutex);
    return s;
}
```

**Apply to:** All single-line accessors

**Expected Savings:** 5KB

---

#### OPTIMIZATION 6.4: Remove Dead Code

**Task:** Identify unused functions

**Tools:**
```bash
# Find defined functions
grep -r "void \|bool \|String " . --include="*.h" > functions.txt

# Cross-reference with usage
for func in $(cat functions.txt); do
    grep -r "$func" . --include="*.cpp" --include="*.ino" || echo "UNUSED: $func"
done
```

**Expected Savings:** 5KB

---

### PHASE 6 COMPLETION CHECKLIST

Final Flash Measurement:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ALNScanner_v5
```

**Success Criteria:**
- [ ] Flash < 1,150,000 bytes (87%)
- [ ] Savings ≥ 60,000 bytes
- [ ] All features work identically to v4.1
- [ ] Code split into files <300 lines each
- [ ] No global variables (except in Application)

**If flash > 87%:** Review each optimization, measure individually, identify remaining duplication

---

## IV. TESTING MATRIX

### A. Component-Level Tests (During Implementation)

| Component | Test Case | Expected Result |
|-----------|-----------|-----------------|
| RFIDReader | Physical card scan | tokenId extracted |
| RFIDReader | No card present | success=false, no hang |
| DisplayDriver | Draw BMP | Image renders |
| DisplayDriver | Missing BMP | Returns false, no crash |
| SDCard | Concurrent access | No deadlock |
| AudioDriver | Play WAV | Audio audible |
| AudioDriver | Missing WAV | Returns false, no crash |
| TouchDriver | Physical touch | valid=true |
| TouchDriver | WiFi active (EMI) | valid=false |
| ConfigService | Load valid config | Returns true |
| ConfigService | Invalid SSID | Validation fails |
| TokenService | Load tokens.json | Count correct |
| OrchestratorService | Online send | HTTP 200 |
| OrchestratorService | Offline queue | File appended |
| UIStateMachine | Tap when ready | Shows status |
| UIStateMachine | Double-tap token | Returns to ready |

### B. Integration Tests (Phase 5)

| Flow | Steps | Expected Result |
|------|-------|-----------------|
| Boot sequence | Power on | Ready screen, WiFi connects |
| Online scan | Scan card | HTTP sent, token displays |
| Offline scan | Disable WiFi, scan | Queued, token displays |
| Queue upload | Re-enable WiFi | Queue clears, HTTP batch sent |
| Status screen | Tap ready screen | Status shows, tap dismisses |
| Video token | Scan video token | Processing modal, 2.5s timeout |
| Serial command | Send "STATUS" | Response printed |
| Config edit | SET_CONFIG, SAVE, REBOOT | Config persists |

### C. Regression Tests (Phase 6)

Compare behavior to v4.1 baseline:
- [ ] Boot time within 10%
- [ ] Scan latency within 10%
- [ ] All 20+ serial commands work
- [ ] Touch responsiveness identical
- [ ] Audio quality identical
- [ ] Display quality identical

---

## V. TROUBLESHOOTING GUIDE

### A. Compilation Errors

| Error | Cause | Fix |
|-------|-------|-----|
| "undeclared identifier" | Missing include | Add #include to file |
| "multiple definition" | Function in header without inline | Move to .cpp or add inline |
| "undefined reference" | Declaration without implementation | Implement function body |
| "circular dependency" | A includes B, B includes A | Use forward declaration |

### B. Runtime Failures

| Symptom | Likely Cause | Debug Steps |
|---------|--------------|-------------|
| Immediate crash | Stack overflow | Check constructor order, reduce local vars |
| Hang on SD access | Mutex deadlock | Review SDCard::Lock usage, check nesting |
| RFID not scanning | GPIO 3 conflict | Verify debugMode logic, check initialization |
| WiFi won't connect | Invalid SSID/password | Test with known-good credentials |
| Touch not responding | Missing interrupt | Check attachInterrupt() call |
| Audio not playing | I2S not initialized | Check lazyInit() called |

### C. Flash Budget Overrun

If final flash > target:

1. **Measure each phase:**
```bash
git checkout phase1
arduino-cli compile . > phase1_flash.txt
git checkout phase2
arduino-cli compile . > phase2_flash.txt
# ... repeat for all phases
diff phase1_flash.txt phase2_flash.txt
```

2. **Find largest contributors:**
```bash
arm-none-eabi-nm -S -C ALNScanner_v5.ino.elf | sort -k2 -r | head -20
```

3. **Apply aggressive optimizations:**
   - Remove all Serial.print from production builds
   - Use -Os flag instead of -O2
   - Disable exception handling
   - Use std::array instead of std::vector where size known

---

## VI. DECISION MATRICES

### A. When to Use .cpp vs Header-Only

```
IF class <100 lines AND no static data:
    → Header-only (inline all methods)
ELSE IF class >300 lines:
    → Separate .h and .cpp
ELSE:
    → Header-only with inline keyword
```

### B. When to Use std::vector vs C Array

```
IF size known at compile time AND <10 elements:
    → C array or std::array
ELSE IF size dynamic OR >10 elements:
    → std::vector
```

### C. When to Use Inheritance vs Composition

```
IF "is-a" relationship (Screen is-a renderable thing):
    → Inheritance (Strategy pattern)
ELSE IF "has-a" relationship (Service has-a HTTPClient):
    → Composition (member variable)
```

---

## VII. MIGRATION PATTERNS

### A. Global Variable → Class Member

**Before:**
```cpp
// In .ino
String wifiSSID = "";
bool debugMode = false;
```

**After:**
```cpp
// In DeviceConfig struct
struct DeviceConfig {
    String wifiSSID;
    bool debugMode;
};

// In Application.h
class Application {
    DeviceConfig _config;
};
```

### B. Naked Mutex → RAII Wrapper

**Before:**
```cpp
if (sdTakeMutex("caller", 500)) {
    File f = SD.open("/file.txt");
    // ... use file ...
    sdGiveMutex("caller");
}
```

**After:**
```cpp
{
    SDCard::Lock lock(sd, "caller", 500);
    if (lock.acquired()) {
        File f = sd.open("/file.txt");
        // ... use file ...
    } // auto-unlock on scope exit
}
```

### C. Function Spaghetti → Service Method

**Before:**
```cpp
bool sendScan(...) { /* 70 lines */ }
void queueScan(...) { /* 50 lines */ }
bool uploadBatch() { /* 100 lines */ }
// All in global scope
```

**After:**
```cpp
class OrchestratorService {
public:
    bool sendScan(...);
    void queueScan(...);
private:
    bool uploadBatch();
};
```

---

## VIII. COMPLETION CRITERIA

### Phase 1: HAL Complete
- [ ] 5 HAL classes compile
- [ ] Each has test sketch
- [ ] All hardware verified on device
- [ ] No global variables

### Phase 2: Models Complete
- [ ] 3 model files compile
- [ ] DeviceConfig validates correctly
- [ ] TokenMetadata helpers work

### Phase 3: Services Complete
- [ ] 4 service classes compile
- [ ] ConfigService loads/saves
- [ ] TokenService queries work
- [ ] OrchestratorService sends/queues
- [ ] SerialService processes commands
- [ ] Flash reduced by 20KB

### Phase 4: UI Complete
- [ ] 5 screen classes compile
- [ ] UIStateMachine routes touches
- [ ] All screen transitions work
- [ ] Flash reduced by 10KB

### Phase 5: Integration Complete
- [ ] Application class compiles
- [ ] Main .ino is <10 lines
- [ ] End-to-end scan flow works
- [ ] All serial commands work
- [ ] Flash reduced by 15KB

### Phase 6: Optimization Complete
- [ ] PROGMEM applied to all long strings
- [ ] DEBUG_MODE compile flag works
- [ ] Small functions inlined
- [ ] Dead code removed
- [ ] Flash < 1,150,000 bytes (87%)
- [ ] Total savings ≥ 60KB

### Final Acceptance
- [ ] All v4.1 features work identically
- [ ] No functional regressions
- [ ] All files <300 lines
- [ ] Code is maintainable (dev survey)
- [ ] Easy to add new token types
- [ ] Easy to add new screens
- [ ] Unit testable (with mocks)

---

## IX. REFERENCES

### A. Original Code Locations (v4.1)

| Component | Line Range | Notes |
|-----------|------------|-------|
| Pin definitions | 40-56 | → config.h |
| Software SPI | 242-472 | → RFIDReader.h |
| RFID protocol | 474-690 | → RFIDReader.h |
| NDEF parsing | 692-874 | → RFIDReader.h |
| BMP rendering | 924-1091 | → DisplayDriver.h |
| Audio playback | 1093-1170 | → AudioDriver.h |
| Touch ISR | 1187-1207 | → TouchDriver.h |
| SD mutex | 1240-1257 | → SDCard.h |
| Config parsing | 1323-1440 | → ConfigService.h |
| Config validation | 1443-1542 | → ConfigService.h |
| Token sync | 1571-1634 | → TokenService.h |
| Send scan | 1650-1716 | → OrchestratorService.h |
| Batch upload | 1725-1824 | → OrchestratorService.h |
| Queue write | 1866-1922 | → OrchestratorService.h |
| Queue remove | 2004-2089 | → OrchestratorService.h |
| Token database | 2092-2146 | → TokenService.h |
| Processing image | 2179-2235 | → ProcessingScreen.h |
| Status screen | 2238-2315 | → StatusScreen.h |
| Ready screen | 2326-2362 | → ReadyScreen.h |
| WiFi init | 2365-2444 | → OrchestratorService.h |
| Background task | 2447-2496 | → OrchestratorService.h |
| RFID init | 2515-2612 | → RFIDReader.h |
| setup() | 2615-2925 | → Application.h |
| Serial commands | 2927-3394 | → SerialService.h |
| processTokenScan | 3413-3559 | → Application.h |
| loop() | 3563-3840 | → Application.h |

### B. Flash Optimization Techniques

1. **F() Macro:** Moves strings to PROGMEM (flash), saves RAM and flash
2. **PROGMEM Tables:** For large const arrays
3. **Compile Flags:** -DDEBUG_MODE=0 removes code
4. **Inline Small Functions:** Eliminates call overhead
5. **String Deduplication:** Use templates for similar messages
6. **HTTP Consolidation:** Single wrapper class
7. **Dead Code Elimination:** -fdata-sections -ffunction-sections

### C. Arduino CLI Commands

```bash
# Compile with verbose output
arduino-cli compile --verbose --fqbn esp32:esp32:esp32 .

# Compile with custom flags
arduino-cli compile \
  --build-property "compiler.cpp.extra_flags=-DDEBUG_MODE=0" \
  --fqbn esp32:esp32:esp32 .

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Monitor
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Generate compilation database (for IDE)
arduino-cli compile --export-binaries --fqbn esp32:esp32:esp32 .
```

---

## X. CHANGE LOG

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-10-22 | Initial implementation guide |

---

**END OF IMPLEMENTATION GUIDE**

*This document is the source of truth for the v5.0 refactor. Update as implementation progresses.*
