# ESP32 CYD Player Scanner - Comprehensive Architectural Analysis

**Document Version:** 1.0  
**Date:** October 31, 2025  
**Scope:** ALNScanner_v5 OOP Architecture  
**Hardware:** ESP32-2432S028R (CYD Dual USB)  
**Status:** v5.0 Compiles Successfully (92% Flash: 1,207,147 bytes)

---

## EXECUTIVE SUMMARY

The ESP32 CYD Player Scanner is a hardware-based RFID scanner that operates in two modes:

1. **Orchestrator Mode** (Connected): Scans RFID tokens and sends data to backend via HTTP/HTTPS
2. **Standalone Mode** (Offline): Queues scans locally when orchestrator unavailable, auto-uploads when reconnected

The scanner uses an OOP architecture (v5.0) with clear layering: Hardware Abstraction Layer → Models → Services → UI → Application. All communication with the backend is HTTP-based with HTTPS support for secure contexts (Web NFC API compliance).

**Critical Status:** Backend expects HTTPS URLs, but current version only supports HTTP. HTTPS support is implemented via `WiFiClientSecure.setInsecure()` in OrchestratorService but requires testing.

---

## ARCHITECTURE OVERVIEW

### Layered Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 5: Application Orchestrator (Application.h - 1,247 lines) │
│ - Coordinates all subsystems                                     │
│ - Manages RFID → Orchestrator → UI flow                          │
├─────────────────────────────────────────────────────────────────┤
│ Layer 4: Services (Business Logic)                               │
│ ├─ OrchestratorService (900+ lines)  - WiFi, HTTP, queuing     │
│ ├─ TokenService (384 lines)          - Token DB, sync            │
│ ├─ ConfigService (547 lines)         - Configuration mgmt        │
│ └─ SerialService (278 lines)         - Command processing        │
├─────────────────────────────────────────────────────────────────┤
│ Layer 3: Models (Data Structures)                                │
│ ├─ Config.h (87 lines)               - Device configuration      │
│ ├─ Token.h (111 lines)               - Token metadata & scan data│
│ └─ ConnectionState.h (82 lines)      - Thread-safe state        │
├─────────────────────────────────────────────────────────────────┤
│ Layer 2: UI (User Interface)                                     │
│ ├─ UIStateMachine.h (366 lines)      - 4-state FSM              │
│ └─ Screen.h (217 lines) + Implementations - Display templates    │
├─────────────────────────────────────────────────────────────────┤
│ Layer 1: HAL (Hardware Abstraction)                              │
│ ├─ SDCard.h (276 lines)              - Thread-safe RAII         │
│ ├─ RFIDReader.h (916 lines)          - MFRC522 + NDEF           │
│ ├─ DisplayDriver.h (440 lines)       - TFT + BMP rendering      │
│ ├─ AudioDriver.h (227 lines)         - I2S WAV playback         │
│ └─ TouchDriver.h (118 lines)         - XPT2046 controller       │
└─────────────────────────────────────────────────────────────────┘
```

### Directory Structure

```
ALNScanner_v5/
├── ALNScanner_v5.ino              # Main entry (16 lines - delegates to Application)
├── Application.h                   # Main coordinator (1,247 lines)
├── config.h                        # All constants (119 lines)
├── hal/                            # Hardware abstraction layer
│   ├── SDCard.h                   # SD card operations with mutex
│   ├── RFIDReader.h               # MFRC522 RFID reader + NDEF
│   ├── DisplayDriver.h            # ST7789 TFT display
│   ├── AudioDriver.h              # I2S audio playback
│   └── TouchDriver.h              # XPT2046 touch controller
├── models/                         # Data structures
│   ├── Config.h                   # DeviceConfig validation
│   ├── Token.h                    # TokenMetadata + ScanData
│   └── ConnectionState.h          # Thread-safe state holder
├── services/                       # Business logic
│   ├── ConfigService.h            # Config loading/saving
│   ├── TokenService.h             # Token DB queries + sync
│   ├── OrchestratorService.h      # HTTP + WiFi + queue
│   └── SerialService.h            # Command registry
└── ui/                             # User interface
    ├── Screen.h                   # Template method base
    ├── UIStateMachine.h           # 4-state FSM
    └── screens/
        ├── ReadyScreen.h          # Idle state
        ├── StatusScreen.h         # Diagnostics
        ├── TokenDisplayScreen.h   # Token display
        └── ProcessingScreen.h     # Video modal
```

---

## BACKEND COMMUNICATION PATTERNS

### 1. ALL Backend Endpoints Called by ESP32

#### HTTP Endpoints (OrchestratorService)

| Method | Endpoint | Purpose | Line(s) in OrchestratorService.h | Notes |
|--------|----------|---------|----------------------------------|-------|
| POST | `/api/scan` | Send single scan | 196 | Also accepts 409 Conflict (duplicate) |
| POST | `/api/scan/batch` | Upload queued scans (max 10) | 391 | Batch upload of offline queue |
| GET | `/health?deviceId={id}` | Health check | 328 | Sent every 10s from background task |
| GET | `/api/tokens` | Fetch token database | 214 | Called from TokenService.syncFromOrchestrator() |

#### Request/Response Formats

**POST /api/scan - Single Scan**
```json
{
  "tokenId": "kaa001",      // Required: NDEF text or UID hex
  "teamId": "001",          // Optional
  "deviceId": "SCANNER_001", // Required: Device identifier
  "timestamp": "1970-01-01T00:00:00.000Z" // Required: ISO 8601-ish
}
```
Response: HTTP 200/409 or error code

**POST /api/scan/batch - Batch Upload**
```json
{
  "transactions": [
    { "tokenId": "kaa001", "teamId": "001", "deviceId": "SCANNER_001", "timestamp": "..." },
    { "tokenId": "jaw002", "teamId": "001", "deviceId": "SCANNER_001", "timestamp": "..." },
    ...
  ]
}
```
Response: HTTP 200 on success

**GET /health?deviceId=SCANNER_001**
Response: HTTP 200 if orchestrator healthy

**GET /api/tokens**
Response: HTTP 200 with JSON payload
```json
{
  "tokens": {
    "kaa001": { "video": "kaa001.mp4" },
    "jaw001": { "video": "" },
    ...
  }
}
```

### 2. HTTP Client Implementation

**Location:** OrchestratorService.h lines 514-617 (HTTPHelper class)

```cpp
class HTTPHelper {
    // Consolidated HTTP operations (saves ~15KB flash)
    Response httpGET(const String& url, uint32_t timeoutMs = 5000)
    Response httpPOST(const String& url, const String& json, uint32_t timeoutMs = 5000)
    
    // HTTPS Support via WiFiClientSecure
    void configureClient(HTTPClient& client, const String& url, uint32_t timeoutMs)
        if (url.startsWith("https://")) {
            client.begin(_secureClient, url);  // WiFiClientSecure for HTTPS
        } else {
            client.begin(url);  // Regular HTTP
        }
}
```

**Key Features:**
- Consolidated from 4 duplicate HTTP implementations (saves 15KB flash)
- HTTPS support via `WiFiClientSecure.setInsecure()` (certificate validation disabled)
- 5-second timeout on all requests
- Accepts both HTTP and HTTPS URLs

### 3. URL Construction

**Configuration Source:**
- SD card config.txt: `ORCHESTRATOR_URL=http://10.0.0.177:3000`
- Stored in models::DeviceConfig.orchestratorURL
- Validated to start with `http://` (line 43 in Config.h)

**URL Building (OrchestratorService.h):**
```cpp
// Line 196
String url = config.orchestratorURL + "/api/scan";

// Line 214
String url = orchestratorURL + "/api/tokens";

// Line 328
String url = config.orchestratorURL + "/health?deviceId=" + config.deviceID;

// Line 391
String url = config.orchestratorURL + "/api/scan/batch";
```

**Problem:** Config validation requires `http://` prefix, but backend advertises HTTPS in discovery. HTTPS URLs won't validate. Workaround: Use HTTP URLs in config, and HTTPHelper automatically detects and handles HTTPS.

---

## TOKEN DATA FLOW

### Download and Caching Strategy

```
BOOT SEQUENCE:
├─ [1] ConfigService loads config.txt from SD card
├─ [2] DeviceConfig.syncTokens flag checked (SYNC_TOKENS=true/false)
├─ [3] IF syncTokens==true:
│  └─ TokenService.syncFromOrchestrator() called (line 54 in CLAUDE.md)
│     ├─ GET /api/tokens from orchestrator (line 214 in OrchestratorService.h)
│     ├─ Validate response size (max 50KB, line 227 in TokenService.h)
│     └─ Save JSON to SD card (/tokens.json)
├─ [4] TokenService.loadDatabaseFromSD() called
│  └─ Reads /tokens.json and parses into std::vector<TokenMetadata>
└─ [5] Ready for queries via TokenService.get(tokenId)

OFFLINE OPERATION:
└─ If orchestrator unavailable at boot:
   ├─ syncFromOrchestrator() fails (network error)
   ├─ Existing tokens.json used (cached from last sync)
   └─ Scans queue locally for upload when connection restored
```

### Token Metadata Structure

```cpp
struct TokenMetadata {
    String tokenId;          // "kaa001" or UID hex "04A1B2C3"
    String video;            // "kaa001.mp4" or "" (determines UI behavior)
    
    bool isVideoToken();     // true if video.length() > 0
    String getImagePath();   // /assets/images/{tokenId}.bmp
    String getAudioPath();   // /assets/audio/{tokenId}.wav
}
```

**Critical Design:** Images and audio paths are **ALWAYS** constructed from `tokenId` at runtime. The orchestrator's `image`, `audio`, `processingImage` fields are **IGNORED** (line 138-139 in TokenService.h).

### SD Card File Paths

```
/                          # SD card root
├── config.txt             # Configuration (required)
├── tokens.json            # Token database (synced from /api/tokens)
├── device_id.txt          # Persisted device ID
├── queue.jsonl            # Offline scan queue (JSONL format)
├── queue.tmp              # Temporary file (queue rebuild)
├── assets/
│   ├── images/
│   │   ├── kaa001.bmp     # 24-bit BMP, 240x320 pixels
│   │   ├── jaw001.bmp
│   │   └── 04A1B2C3.bmp   # UID-based fallback image
│   └── audio/
│       ├── kaa001.wav     # I2S WAV format
│       └── jaw001.wav
```

**File Format Notes:**
- **tokens.json:** From backend `/api/tokens` endpoint, 50KB max
- **queue.jsonl:** JSONL (JSON Lines), one scan per line, FIFO ordered
- **BMP images:** 24-bit color, 240x320 pixels, ST7789 native format
- **WAV audio:** I2S format, WAV header, 44.1kHz typical

---

## RFID SCANNING FLOW

### Card Detection and Token ID Extraction

```cpp
// Location: Application.h lines 455-560 (processRFIDScan)

1. GUARD CONDITIONS:
   ├─ If RFID not initialized (DEBUG_MODE): skip
   ├─ If UI blocking (showing image/status): skip
   └─ If rate limit < 500ms since last scan: skip

2. RFID DETECTION:
   └─ RFIDReader::detectCard(uid) - MFRC522 ISO14443A scan

3. TOKEN ID EXTRACTION:
   ├─ NDEF text extraction (preferred): "kaa001"
   └─ UID hex fallback (no NDEF): "04A1B2C3D4E5F6"

4. ORCHESTRATOR ROUTING:
   ├─ IF ORCH_CONNECTED: sendScan() → queue on failure
   ├─ IF ORCH_WIFI_CONNECTED or ORCH_DISCONNECTED: queue immediately
   └─ All scans flow through OrchestratorService

5. DISPLAY LOGIC:
   ├─ Lookup token in TokenService
   ├─ IF isVideoToken(): showProcessing() [2.5s modal, auto-dismiss]
   └─ ELSE: showToken() [persistent, tap to dismiss]

6. UI STATE TRANSITIONS:
   ├─ READY → IMAGE (token display)
   ├─ IMAGE → READY (auto-timeout or double-tap)
   └─ READY → STATUS (single-tap)
```

### Scan Data Queue Management

**Offline Queue (OrchestratorService.h lines 233-291):**

```cpp
void queueScan(const models::ScanData& scan) {
    // [1] Check overflow (max 100 entries)
    if (getQueueSize() >= MAX_QUEUE_SIZE) {
        handleQueueOverflow();  // FIFO remove oldest
    }
    
    // [2] Build JSONL entry
    JsonDocument doc;
    doc["tokenId"] = scan.tokenId;
    doc["teamId"] = scan.teamId;      // Optional
    doc["deviceId"] = scan.deviceId;
    doc["timestamp"] = scan.timestamp;
    
    // [3] Append to /queue.jsonl with mutex lock
    hal::SDCard::Lock lock("queueScan", 500ms);
    File file = SD.open("/queue.jsonl", FILE_APPEND);
    file.println(jsonLine);
    file.close();
}
```

**Background Batch Upload (OrchestratorService.h lines 818-872):**

```cpp
// Runs on FreeRTOS Core 0, every 10 seconds
void backgroundTaskLoop() {
    while (true) {
        // Check orchestrator health
        if (checkHealth(_config)) {
            _connState.set(ORCH_CONNECTED);
            
            // If queue has entries
            if (getQueueSize() > 0) {
                uploadQueueBatch(_config);  // POST /api/scan/batch
            }
        }
        vTaskDelay(10000ms);  // Check every 10 seconds
    }
}
```

**Stream-Based Queue Removal (Memory-Safe):**

```cpp
// OrchestratorService.h lines 694-776
void removeUploadedEntries(int numEntries) {
    // Open source and temp files
    File src = SD.open("/queue.jsonl", FILE_READ);
    File tmp = SD.open("/queue.tmp", FILE_WRITE);
    
    // Stream-copy: skip first N lines, keep rest
    while (src.available()) {
        String line = src.readStringUntil('\n');
        if (linesSkipped < numEntries) {
            linesSkipped++;  // Remove
        } else {
            tmp.println(line);  // Keep
        }
    }
    
    // Atomic file replacement
    SD.remove("/queue.jsonl");
    SD.rename("/queue.tmp", "/queue.jsonl");
}

// Memory usage: ~100 bytes (String buffer)
// vs old approach: ~10KB (entire queue in RAM)
```

---

## DEVICE IDENTIFICATION

### Device ID Assignment

**Configuration Source (Config.h lines 9-19):**
```cpp
struct DeviceConfig {
    String teamID;    // From config.txt: TEAM_ID=001 (exactly 3 digits)
    String deviceID;  // From config.txt: DEVICE_ID=... (optional)
};
```

**Device ID Generation (if not in config):**
- Auto-generated from ESP32 MAC address as: `SCANNER_XXXXXXXXXXXX`
- Persisted to /device_id.txt for consistency
- Stored in DeviceConfig.deviceID

**Device Identification in HTTP Requests:**
```
POST /api/scan:
  deviceId: "SCANNER_001" (from config) or "SCANNER_4E7BF4C5D9E8" (auto-generated)

GET /health?deviceId=SCANNER_001
  Used to track device in orchestrator's device list
```

**Backend Contract (from CLAUDE.md in parent repo):**
- Backend expects `deviceId` in scan POST body
- Backend uses `deviceId` to track connected devices
- Backend health check uses `deviceId` parameter for device tracking

---

## CURRENT LIMITATIONS AND HTTPS STATUS

### Flash Memory Constraints (92% Capacity)

```
Current Usage:  1,207,147 bytes / 1,310,720 bytes (92%)
Remaining:      ~103,573 bytes (7.8%)
Critical Threshold: 95% (unsafe zone)

Phase 6 Target: Reduce to <87% (1,150,000 bytes)
Planned Optimizations:
  - PROGMEM strings (F() macro):      -20KB
  - Compile-time DEBUG flags:          -15KB
  - Dead code elimination:             -15KB
  - Function inlining:                 -7KB
```

### HTTP vs HTTPS Status

**Current State:**
- Configuration validation (Config.h line 43): Requires `http://` prefix
- HTTPClient setup (OrchestratorService.h lines 605-612): Auto-detects protocol
  ```cpp
  if (url.startsWith("https://")) {
      client.begin(_secureClient, url);  // Uses WiFiClientSecure
  } else {
      client.begin(url);  // Regular HTTP
  }
  ```
- Certificate validation: **DISABLED** via `setInsecure()` (acceptable for local networks)

**Problem with Backend HTTPS:**
- Backend advertises HTTPS in discovery responses
- But ESP32 config only accepts `http://` URLs
- **Solution:** Backend should provide HTTP redirect server (already exists on port 8000)
- **Workaround:** Users can manually enter HTTPS URLs in config.txt (e.g., `https://10.0.0.177:3000`), though validation should be relaxed

**HTTPS Support Readiness:**
- WiFiClientSecure included (lines 29, 614 in OrchestratorService.h)
- HTTP and HTTPS both supported by HTTPHelper
- Self-signed certificate handling via setInsecure()
- Status: **Ready for testing**, needs config validation update

---

## CRITICAL CONFIGURATION PARAMETERS

### Required Configuration (SD card /config.txt)

```ini
# WiFi Credentials
WIFI_SSID=NetworkName           # 1-32 chars (required)
WIFI_PASSWORD=password          # 0-63 chars

# Orchestrator Connection
ORCHESTRATOR_URL=http://10.0.0.177:3000    # Must start with http://
TEAM_ID=001                                  # Exactly 3 digits (required)

# Optional
DEVICE_ID=SCANNER_FLOOR1        # Auto-generated from MAC if not set
SYNC_TOKENS=true                # true: fetch from /api/tokens on boot
DEBUG_MODE=false                # true: defer RFID init for serial commands
```

### Validation Rules (Config.h lines 29-64)

| Field | Validation | Min/Max | Notes |
|-------|-----------|---------|-------|
| WIFI_SSID | Non-empty, length 1-32 | Required | 32-char WiFi standard |
| WIFI_PASSWORD | Length 0-63 | Optional | Can be empty for open networks |
| ORCHESTRATOR_URL | Must start with `http://` | Required | HTTP only (HTTPS auto-detected) |
| TEAM_ID | Exactly 3 digits (0-9) | Required | Pattern: `^[0-9]{3}$` |
| DEVICE_ID | Alphanumeric + underscore, length 1-100 | Optional | Auto-generated if empty |
| SYNC_TOKENS | true/false | Optional | Default: true |
| DEBUG_MODE | true/false | Optional | Default: false |

---

## INITIALIZATION SEQUENCE

### Boot Flow (Application.h lines 110-293)

```
PHASE 1: Serial Communication & Boot Override (1-30 seconds)
├─ Initialize Serial at 115200 baud
├─ 30-second DEBUG_MODE override window
│  └─ If any character received: force DEBUG_MODE=true
└─ Print boot banner with ESP32 info

PHASE 2: Early Hardware (Display + SD card)
├─ Initialize TFT_eSPI DisplayDriver
│  └─ CRITICAL: Must initialize BEFORE SD card (VSPI bus issue)
├─ Initialize SDCard
│  └─ Now SD.open() works correctly
└─ Print reset reason for diagnostics

PHASE 3: Configuration Loading
├─ ConfigService.loadFromSD() reads /config.txt
├─ Validate all required fields
├─ Apply boot override (if character received in Phase 1)
└─ Print loaded configuration

PHASE 4: Late Hardware (Touch, Audio, RFID)
├─ TouchDriver.begin() - interrupt-based, no dependencies
├─ AudioDriver.begin() - lazy-initialized (deferred until first use)
└─ IF !DEBUG_MODE: RFIDReader.begin()
   ELSE: RFID deferred until START_SCANNER command

PHASE 5: Service Layer Initialization
├─ TokenService.syncFromOrchestrator() [if SYNC_TOKENS=true]
│  └─ GET /api/tokens → save /tokens.json
├─ TokenService.loadDatabaseFromSD()
│  └─ Parse /tokens.json into std::vector
├─ OrchestratorService.initializeWiFi()
│  ├─ WiFi.begin(SSID, password)
│  ├─ Wait for connection (timeout: 10s)
│  └─ Check orchestrator health (GET /health?deviceId=...)
└─ SerialService initialization (command registry)

PHASE 6: Background Task & UI
├─ OrchestratorService.startBackgroundTask()
│  └─ FreeRTOS task on Core 0 for queue upload
├─ UIStateMachine initialization
│  └─ Display ReadyScreen (idle state)
└─ Ready for RFID scanning

Total Time: 15-25 seconds (depending on WiFi, token sync, orchestrator response)
```

---

## DESIGN PATTERNS USED

### 1. Singleton Pattern (All HAL Components & Services)

```cpp
// OrchestratorService.h lines 47-50
static OrchestratorService& getInstance() {
    static OrchestratorService instance;
    return instance;
}

// Usage throughout codebase
auto& orchestrator = services::OrchestratorService::getInstance();
```

**Benefit:** Single global instance, lazy initialization, thread-safe (static initialization)

### 2. Facade Pattern (Application Class)

```cpp
// Application.h - Coordinates all subsystems
Application app;
void setup() { app.setup(); }
void loop() { app.loop(); }
```

**Benefit:** Complex subsystem interactions hidden behind simple API

### 3. Template Method Pattern (UI Screens)

```cpp
// Screen.h - Base class with virtual methods
class Screen {
    virtual void begin();    // Setup
    virtual void update();   // Periodic update
    virtual void handleTouch();  // Touch input
    virtual void end();      // Cleanup
};

// Subclasses implement specific behavior
class ReadyScreen : public Screen { ... };
class TokenDisplayScreen : public Screen { ... };
```

**Benefit:** Consistent screen lifecycle, easy to add new screens

### 4. State Machine Pattern (UIStateMachine)

```cpp
// UIStateMachine.h - 4-state FSM
enum UIState {
    READY,          // Idle, waiting for scan
    IMAGE,          // Showing token/processing screen
    STATUS,         // Showing diagnostics
    PROCESSING      // Video modal
};

void update() {
    // Handle timeouts, state transitions
    if (currentScreen.isTimedOut()) {
        transitionTo(READY);
    }
}
```

**Benefit:** Clear state transitions, prevents invalid state combinations

### 5. RAII Pattern (SD Card Locking)

```cpp
// SDCard.h - Mutex lock with automatic release
class Lock {
public:
    Lock(const char* caller, unsigned long timeoutMs) {
        acquired_ = sdTakeMutex(caller, timeoutMs);
    }
    ~Lock() {
        if (acquired_) sdGiveMutex(caller);
    }
};

// Usage - lock released automatically
{
    hal::SDCard::Lock lock("queueScan", 500);
    if (lock.acquired()) {
        File f = SD.open("/queue.jsonl", FILE_APPEND);
        // ...
    }
}  // Lock auto-released here
```

**Benefit:** No manual mutex release needed, exception-safe

### 6. Command Registry Pattern (SerialService)

```cpp
// SerialService.h - Replaces 468-line if/else chain from v4.1
class SerialService {
    void registerCommand(const String& name, CommandHandler handler);
    void processCommands();
};

// Registration
serial.registerCommand("CONFIG", [](const String& args) {
    configService.printConfig();
});

serial.registerCommand("START_SCANNER", [](const String& args) {
    rfidReader.begin();  // Initialize RFID
});
```

**Benefit:** Extensible command system, no massive if/else chains

---

## KEY FILES AND LINE REFERENCES

### HTTP Communication

| Operation | File | Lines | Details |
|-----------|------|-------|---------|
| Send single scan | OrchestratorService.h | 169-224 | POST /api/scan |
| Queue scan | OrchestratorService.h | 233-291 | Append to /queue.jsonl |
| Batch upload | OrchestratorService.h | 344-429 | POST /api/scan/batch (max 10) |
| Health check | OrchestratorService.h | 325-332 | GET /health?deviceId=X |
| HTTP Client setup | OrchestratorService.h | 554-612 | HTTPHelper class |
| HTTPS Support | OrchestratorService.h | 605-612 | WiFiClientSecure auto-detection |

### Token Data Flow

| Operation | File | Lines | Details |
|-----------|------|-------|---------|
| Sync from orchestrator | TokenService.h | 203-268 | GET /api/tokens → save /tokens.json |
| Load from SD | TokenService.h | 93-146 | Parse /tokens.json into vector |
| Token lookup | TokenService.h | 160-167 | Linear search by tokenId |
| Token metadata | Token.h | 10-54 | Paths auto-constructed from tokenId |

### RFID Scanning

| Operation | File | Lines | Details |
|-----------|------|-------|---------|
| Card detection | Application.h | 455-560 | processRFIDScan() full flow |
| Send/queue decision | Application.h | 517-532 | Route based on connection state |
| Token display | Application.h | 534-559 | Video vs regular token logic |
| NDEF extraction | RFIDReader.h | (varies) | extractNDEFText() method |

### Configuration

| Operation | File | Lines | Details |
|-----------|------|-------|---------|
| Config structure | Config.h | 9-85 | DeviceConfig with validation |
| Validation rules | Config.h | 29-64 | Validate method |
| Config load | ConfigService.h | (varies) | Load from /config.txt |
| Config save | ConfigService.h | (varies) | Persist to /config.txt |

---

## MISMATCHES WITH BACKEND EXPECTATIONS

### 1. Configuration URL Validation

**Issue:** Config validation requires `http://` prefix (Config.h line 43)
```cpp
if (!orchestratorURL.startsWith("http://")) {
    return false;  // Config invalid
}
```

**Backend Reality:** Advertises HTTPS URLs in discovery responses

**Impact:** 
- Users cannot configure HTTPS URLs via config.txt
- HTTPS requests work (HTTPHelper auto-detects), but config validation fails
- Workaround: Users can manually edit config.txt to use HTTP redirect server (port 8000)

**Recommendation:** Relax validation to accept both `http://` and `https://` prefixes

### 2. Team ID Format

**ESP32 Requirement:** Exactly 3 digits (Config.h lines 47-56, Token.h)
```cpp
// Must be exactly 3 digits: 001-999
if (teamID.length() != 3) return false;
for (int i = 0; i < 3; i++) {
    if (!isDigit(teamID[i])) return false;
}
```

**Backend Reality:** Unknown - review CLAUDE.md of parent repo for expectations

**Recommendation:** Verify backend accepts 3-digit team IDs, document format

### 3. Device ID Uniqueness

**ESP32 Approach:** 
- Optional in config.txt
- Auto-generated from MAC if not provided
- Persisted to /device_id.txt

**Backend Expectation:** `deviceId` sent in every scan

**Current Status:** ✓ Compatible - deviceId always available (either configured or auto-generated)

### 4. Timestamp Format

**ESP32 Format:** `1970-01-01THH:MM:SS.mmmZ` (placeholder, uses millis())
**Backend Reality:** Likely expects actual timestamp

**Issue:** No RTC on ESP32, timestamp is relative to boot time

**Recommendation:** 
- Backend should use server-side timestamp for event logging
- ESP32 timestamp can be used for offline queue ordering only
- If accuracy needed, backend should sync time via NTP or HTTP response

---

## OFFLINE QUEUE MECHANICS

### Queue Size Tracking

```cpp
// OrchestratorService.h lines 506-509
struct {
    volatile int size;           // Atomic counter
    mutable portMUX_TYPE mutex;  // Spinlock
} _queue = {0, portMUX_INITIALIZER_UNLOCKED};

// Atomic update (safe for Core 0 + Core 1)
portENTER_CRITICAL(&_queue.mutex);
_queue.size += delta;
portEXIT_CRITICAL(&_queue.mutex);
```

### Queue Overflow Protection (FIFO)

```cpp
// OrchestratorService.h lines 795-799
void handleQueueOverflow() {
    if (getQueueSize() >= MAX_QUEUE_SIZE) {  // 100 entries
        removeUploadedEntries(1);  // Remove oldest entry
        LOG_INFO("[ORCH-QUEUE] Queue overflow, removing oldest entry\n");
    }
}
```

**Behavior:** When queue reaches 100 entries, oldest entry is automatically removed (FIFO)

### Batch Upload Logic

```
Background Task (10s interval):
├─ Check connection state
├─ IF connected:
│  └─ IF queue.size > 0:
│     └─ uploadQueueBatch(config)
│        ├─ Read first 10 entries
│        ├─ POST /api/scan/batch
│        ├─ IF 200 OK:
│        │  └─ removeUploadedEntries(10)
│        │     └─ Stream-based rebuild (memory safe)
│        │     └─ Repeat if more entries remain
│        └─ IF error:
│           └─ Retry next cycle (entries remain in queue)
```

---

## WIRELESS CONNECTIVITY FLOW

### WiFi Connection States

```cpp
// ConnectionState.h
enum ConnectionState {
    ORCH_DISCONNECTED,      // No WiFi, offline
    ORCH_WIFI_CONNECTED,    // WiFi up, orchestrator down
    ORCH_CONNECTED          // WiFi up, orchestrator up
};
```

### State Transitions (OrchestratorService.h lines 883-921)

```cpp
// WiFi events trigger state changes
onWiFiConnected()  → ORCH_WIFI_CONNECTED
onWiFiGotIP()      → ORCH_WIFI_CONNECTED (with IP details)
checkHealth()      → ORCH_CONNECTED (if /health returns 200)
onWiFiDisconnected() → ORCH_DISCONNECTED

// Background task monitors continuously
backgroundTaskLoop() every 10s:
    if (WiFi connected) {
        checkHealth()
        if (response 200) {
            uploadQueueBatch()  // Any queued scans
        }
    }
```

### Auto-Reconnect Behavior

```cpp
// OrchestratorService.h lines 82
WiFi.begin(SSID, password);
// WiFi library handles auto-reconnect automatically
// No manual intervention needed
// On disconnect event: state → ORCH_DISCONNECTED
// On reconnect: state → ORCH_WIFI_CONNECTED → ORCH_CONNECTED (after health check)
```

---

## CRITICAL IMPLEMENTATION NOTES

### 1. VSPI Bus Initialization Order

**CRITICAL:** Display MUST initialize BEFORE SD card (Application.h lines 697-723)

```cpp
// ✅ CORRECT - Application.h
void initializeEarlyHardware() {
    auto& display = hal::DisplayDriver::getInstance();
    display.begin();  // TFT sets up VSPI
    
    auto& sd = hal::SDCard::getInstance();
    sd.begin();  // SD reconfigures VSPI compatibly
}

// ❌ WRONG - SD first breaks file operations
```

**Impact:** Reversing this order causes SD.open() to fail even though mount succeeds

### 2. GPIO 3 Conflict (Serial RX vs RFID_SS)

**Problem:** GPIO 3 shared between Serial RX and RFID chip select

**Solution:**
```cpp
DEBUG_MODE=true:  Serial RX active, RFID deferred (send START_SCANNER)
DEBUG_MODE=false: RFID active at boot, Serial RX unavailable
Boot Override:    30-second window to force DEBUG_MODE=true
```

### 3. Memory Safety - Offline Queue

**Old Approach (v4.1):** Load entire queue into RAM
```cpp
std::vector<String> lines;
while (file.available()) {
    lines.push_back(file.readStringUntil('\n'));  // ← OOM risk!
}
```

**New Approach (v5.0):** Stream-based processing
```cpp
while (file.available()) {
    String line = file.readStringUntil('\n');
    if (shouldKeep) {
        tmp.println(line);  // ← Always just 1 line in memory
    }
}
```

**Savings:** 100-entry queue from 10KB → 100 bytes RAM

### 4. Thread-Safe Queue Operations

- Queue size counter uses spinlock (portENTER_CRITICAL/EXIT_CRITICAL)
- SD operations use RAII lock (hal::SDCard::Lock)
- Background task (Core 0) and main loop (Core 1) coordinate via these mechanisms
- No race conditions or deadlocks

---

## SUMMARY TABLE

| Aspect | Details |
|--------|---------|
| **Flash Usage** | 1,207,147 bytes / 1,310,720 (92%) |
| **HTTP Endpoints** | 4 total (POST /api/scan, POST /api/scan/batch, GET /health, GET /api/tokens) |
| **Configuration Method** | SD card /config.txt (required fields: SSID, password, orchestrator URL, team ID) |
| **Device ID** | Optional in config; auto-generated from MAC if not set |
| **Token Sync** | GET /api/tokens endpoint, max 50KB, cached to /tokens.json |
| **Offline Queueing** | JSONL format, FIFO ordering, max 100 entries, auto-overflow protection |
| **Batch Upload** | POST /api/scan/batch, max 10 scans per request |
| **Health Check** | GET /health?deviceId=X, every 10 seconds from background task |
| **HTTPS Support** | Supported via WiFiClientSecure.setInsecure(), auto-detected by URL prefix |
| **URL Validation** | Requires http:// prefix (relaxed to https:// recommended) |
| **Team ID Format** | Exactly 3 digits (001-999) |
| **RFID Scan Interval** | 500ms (rate-limited to reduce GPIO 27 beeping) |
| **Background Task** | FreeRTOS Core 0, 16KB stack, checks health & uploads queue every 10s |
| **UI State Machine** | 4 states: READY, IMAGE, STATUS, PROCESSING |
| **Touch Handling** | EMI-filtered, debounced 50ms, double-tap detection |

---

## RECOMMENDATIONS FOR BACKEND INTEGRATION

1. **HTTPS Configuration:**
   - Relax ESP32 config validation to accept `https://` URLs
   - Test with self-signed certificates (setInsecure() already in place)
   - Consider providing HTTP redirect from port 8000 as fallback

2. **Team ID Format:**
   - Confirm backend accepts exactly 3-digit team IDs
   - Document in API contract

3. **Timestamp Handling:**
   - Document that ESP32 sends placeholder timestamps (millis()-based)
   - Recommend backend uses server-side timestamp for event logging
   - Consider NTP sync if absolute timestamps needed

4. **Error Handling:**
   - Document expected HTTP status codes for /api/scan (200, 409 already handled)
   - Clarify behavior for invalid tokenIds
   - Define max batch size expectations (currently 10)

5. **Device Tracking:**
   - Verify backend tracks `deviceId` correctly
   - Consider indexing by (deviceId, timestamp) for multi-device deployments

6. **Token Database:**
   - Confirm format of /api/tokens response matches spec
   - Set reasonable size limit (currently 50KB)
   - Document image/audio path construction (always from tokenId, not from response)

---

**Document Complete**
