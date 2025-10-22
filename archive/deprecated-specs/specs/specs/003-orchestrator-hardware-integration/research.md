# Research: Orchestrator Integration for Hardware Scanner

**Feature**: 003-orchestrator-hardware-integration
**Date**: 2025-10-19
**Status**: Phase 0 Complete

## Overview

This document consolidates research findings from hardware specifications, PWA reference implementation analysis, and ESP32 best practices for implementing orchestrator integration on the CYD RFID Scanner.

---

## Hardware Specifications Analysis

### ESP32-2432S028R (CYD) Platform Constraints

**Source**: `HARDWARE_SPECIFICATIONS.md`, `CLAUDE.md`, `constitution.md`

#### Critical SPI Bus Architecture

The CYD board has a **fixed SPI bus architecture** that directly impacts implementation:

- **VSPI (Hardware SPI)**: Shared between TFT display and SD card
  - SD Card: GPIO 5 (SS), 18 (SCK), 19 (MISO), 23 (MOSI)
  - TFT Display: Uses HSPI but shares VSPI for SD operations

- **Software SPI**: RFID (MFRC522)
  - RFID: GPIO 3 (SS), 22 (SCK), 27 (MOSI), 35 (MISO)

**CRITICAL CONSTRAINT**: All SD card reads/writes for queue management, config files, and token database MUST complete BEFORE acquiring TFT display locks to avoid SPI bus deadlock (Constitution Principle II - NON-NEGOTIABLE).

#### Available GPIO Pins

Only 3 easily accessible GPIO pins available:
- GPIO35 (P3) - Input only, no pull-ups
- GPIO22 (P3, CN1) - Used by RFID SCK
- GPIO27 (CN1) - Used by RFID MOSI, **electrically coupled to speaker** (causes beeping)

**Impact**: WiFi/network functionality uses ESP32's built-in WiFi radio (no additional GPIO needed).

#### Memory Constraints

- **RAM**: ~300KB usable (ESP32 dual-core 240MHz)
- **Token database**: Must remain <50KB JSON (fits in RAM for parsing)
- **Queue storage**: SD card only (RAM cache for status display to avoid SD access during TFT rendering)

---

## PWA Reference Implementation Analysis

**Source**: `/home/maxepunk/projects/AboutLastNight/aln-memory-scanner/js/orchestratorIntegration.js`

### Architectural Patterns from PWA

#### 1. Offline Queue Management

```javascript
// PWA Pattern: FIFO queue with 100-item limit
if (this.offlineQueue.length >= this.maxQueueSize) {
  this.offlineQueue.shift(); // Remove oldest if at limit
}
this.offlineQueue.push({
  tokenId, teamId, timestamp: Date.now(), retryCount: 0
});
this.saveQueue(); // Persist to localStorage
```

**Decision for ESP32**:
- Use SD card `/queue.jsonl` instead of localStorage
- JSON Lines format for atomic append operations
- Same 100-item FIFO limit
- Persist queue through power cycles

#### 2. Connection Monitoring

```javascript
// PWA Pattern: Check every 10 seconds
this.connectionCheckInterval = setInterval(() => {
  this.pendingConnectionCheck = this.checkConnection();
}, 10000);
```

**Decision for ESP32**:
- FreeRTOS background task (10-second interval)
- HTTP GET `/health` endpoint for connection checks
- 5-second timeout on health checks (network responsiveness threshold)

#### 3. Batch Upload

```javascript
// PWA Pattern: Process up to 10 at a time
const batch = this.offlineQueue.splice(0, 10);
const response = await fetch(`${this.baseUrl}/api/scan/batch`, {
  method: 'POST',
  body: JSON.stringify({
    transactions: batch.map(item => ({...}))
  })
});
```

**Decision for ESP32**:
- Same 10-item batch size
- POST `/api/scan/batch` with transactions array
- 1-second delay between batches
- Re-queue on failure (idempotent retry)

#### 4. Fire-and-Forget Pattern

```javascript
// PWA ignores response body for immediate scans
try {
  const response = await fetch(`${this.baseUrl}/api/scan`, {...});
  if (!response.ok) throw new Error(...);
  return await response.json(); // Returned but not used
} catch (error) {
  this.queueOffline(tokenId, teamId);
}
```

**Decision for ESP32**:
- Send POST `/api/scan` when online
- Check HTTP status code only (200 OK = success)
- Don't parse response body (fire-and-forget)
- Queue immediately on timeout/error

---

## Orchestrator API Contract Analysis

**Source**: `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/backend/contracts/openapi.yaml`

### Endpoints for Hardware Scanner

#### GET /api/tokens

**Purpose**: Fetch token database on boot (optional sync)

**Response**: JSON with tokens object

```json
{
  "tokens": {
    "534e2b02": {
      "image": "assets/images/534e2b02.jpg",
      "audio": "assets/audio/534e2b02.mp3",
      "video": null,
      "processingImage": null,
      "SF_RFID": "534e2b02"
    }
  },
  "count": 42,
  "lastUpdate": "2025-09-30T12:00:00.000Z"
}
```

**Decision**:
- Save to SD card `/tokens.json` if sync succeeds
- Use cached `/tokens.json` if sync fails (pre-loaded in SD card image)
- Load into RAM on boot, parse outside TFT lock context

#### POST /api/scan

**Purpose**: Single token scan (fire-and-forget)

**Request**:
```json
{
  "tokenId": "534e2b03",
  "teamId": "001",  // optional
  "deviceId": "SCANNER_A1B2C3D4E5F6",
  "timestamp": "2025-09-30T14:30:00.000Z"
}
```

**Response Codes**:
- 200 OK: Scan accepted
- 409 Conflict: Video already playing (queue anyway, orchestrator handles)
- 400 Bad Request: Validation error (unexpected, queue for retry)
- 503 Service Unavailable: Offline queue full on orchestrator (rare, queue locally anyway)

**Decision**:
- Check status code only: 200-299 = success, else queue
- Don't parse response body (ESP32 memory savings)

#### POST /api/scan/batch

**Purpose**: Offline queue upload

**Request**:
```json
{
  "transactions": [
    { "tokenId": "...", "teamId": "...", "deviceId": "...", "timestamp": "..." },
    // ... up to 10 items
  ]
}
```

**Decision**:
- Upload on connection restoration (background task)
- Remove from local queue on 200 OK
- Keep in queue on failure, retry on next check (10 seconds)

#### GET /health

**Purpose**: Connection check

**Response**:
```json
{
  "status": "online",
  "version": "1.0.0",
  "uptime": 3600.5,
  "timestamp": "2025-09-30T12:00:00.000Z"
}
```

**Decision**:
- 5-second timeout
- Parse `status` field only
- Connected if 200 OK response within timeout

---

## ESP32 Best Practices Research

### 1. WiFi Connection and Reconnection

**Sources**: Random Nerd Tutorials, ESP32 Forum, Arduino ESP32 documentation (2025)

#### Event-Driven Reconnection (Recommended)

**Key Finding**: Newer Arduino ESP32 core (v3.x) uses `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` event (old `SYSTEM_EVENT_*` constants deprecated).

**Best Practice**:
```cpp
void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi connected");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection, reconnecting...");
      WiFi.reconnect(); // Auto-reconnect
      break;
  }
}
```

**Decision for Implementation**:
- Use WiFi event callbacks for immediate reconnection awareness
- Background task monitors connection every 10 seconds (polling backup)
- Call `WiFi.reconnect()` on disconnection event
- Auto-reconnection is built into arduino-esp32 (enabled by default)

**Alternatives Rejected**:
- Polling-only approach: Introduces 10-second latency to detect disconnection
- ESP.restart() on disconnect: Too disruptive, loses queue state

**Rationale**: Event-driven approach provides immediate awareness of connection loss while background polling ensures recovery if event system fails.

---

### 2. HTTPClient Timeout and Error Handling

**Sources**: ESP32 Forum, GitHub issues (espressif/arduino-esp32), ESP-IDF documentation (2025)

#### Common Issues

- Error -11 (read timeout) is most frequent
- Servers using "Connection: close" can cause issues
- Keep-Alive headers work better with ESP32

#### Best Practices

**Timeout Configuration**:
```cpp
HTTPClient http;
http.setTimeout(5000); // 5 seconds in milliseconds
```

**Retry Logic**:
```cpp
int attempts = 3;
while (attempts > 0) {
  int httpCode = http.POST(payload);
  if (httpCode > 0) break; // Success
  attempts--;
  delay(100);
}
```

**Event Handling**: ESP HTTP Client supports event handlers for non-blocking requests (esp_http_client_config_t::event_handler).

**Decision for Implementation**:
- 5-second timeout for all HTTP requests (scan, batch, health, tokens sync)
- Fire-and-forget pattern: No retry on immediate scans (queue instead)
- Retry only for batch uploads (background task handles retries automatically)
- Don't use persistent connections (fire-and-forget model, short-lived requests)

**Alternatives Rejected**:
- 50-second timeout: Too long, blocks scanner unacceptably
- Persistent connections: Adds complexity, minimal benefit for infrequent requests
- Synchronous retry on immediate scans: Blocks RFID scanning loop

**Rationale**: 5-second timeout balances network reliability with acceptable UX latency. Queue-based resilience is more important than immediate retry attempts.

---

### 3. ArduinoJson Memory Management

**Sources**: ArduinoJson official documentation, ESP32 memory tutorials (2025)

#### Recent Updates (2025)

- **ArduinoJson 7.4**: Tiny string optimization (strings ≤3 chars don't allocate heap)
- **ArduinoJson 7.2**: Performance improvements, reduced memory usage

#### Best Practices

**PSRAM Usage** (if available):
```cpp
struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  }
  void deallocate(void* pointer) override {
    heap_caps_free(pointer);
  }
};
```

**Input Filtering**:
```cpp
// Only parse needed fields
JsonDocument doc;
deserializeJson(doc, jsonInput, DeserializationOption::Filter(filter));
```

**Stream Handling**: Let ArduinoJson copy stream content (avoids manual buffering, ignores punctuation/spaces).

**Decision for Implementation**:
- Use ArduinoJson v7.x (latest stable as of 2025)
- Token database: Statically allocated JsonDocument (capacity ~20KB for 50 tokens)
- Queue entries: Small dynamic JsonDocument (~256 bytes per entry)
- No PSRAM allocator needed (CYD doesn't have external PSRAM)
- Filter token database parsing to extract only: tokenId, video, image, audio, processingImage fields

**Alternatives Rejected**:
- ArduinoJson v6: Older, less optimized
- Manual JSON parsing: Error-prone, more code complexity
- Full token database parsing: Wastes RAM on unused SF_* game metadata fields

**Rationale**: ArduinoJson 7.x provides best balance of memory efficiency and ease of use for ESP32. Filtering reduces RAM usage significantly.

---

### 4. FreeRTOS Tasks and Shared SPI Resources

**Sources**: ESP-IDF FreeRTOS documentation, ESP32 tutorials, GitHub examples (2025)

#### Mutex vs. Spinlock

**Key Finding**:
- **Mutex**: For data shared between tasks only (FreeRTOS scheduler-aware)
- **Spinlock (portMUX_TYPE)**: For data shared by both CPUs and between tasks/ISRs (disables interrupts)

**Best Practice**:
```cpp
SemaphoreHandle_t spiMutex = xSemaphoreCreateMutex();

void backgroundTask(void *parameter) {
  while(1) {
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
      // Access SD card for queue operations
      xSemaphoreGive(spiMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(10000)); // 10 seconds
  }
}
```

**Scheduler Suspension**: On IDF FreeRTOS, `vTaskSuspendAll()` only suspends scheduling on one core. Use mutexes for proper cross-core mutual exclusion.

**Decision for Implementation**:
- FreeRTOS mutex for SD card access (shared between main task and background task)
- Background task on Core 0 (network operations, queue sync)
- Main task on Core 1 (RFID scanning, display rendering)
- Mutex protects: SD card file operations, queue metadata (RAM cache of queue size)

**Architecture**:
```
Core 0 (Background Task):
  - Connection monitoring (10s interval)
  - Queue synchronization
  - SD card writes (config, queue, tokens)

Core 1 (Main Task):
  - RFID scanning loop
  - TFT display rendering
  - Touch input handling
  - SD card reads (only for BMP display, not during TFT lock)
```

**Alternatives Rejected**:
- Single-core implementation: Doesn't utilize ESP32's dual cores
- Spinlocks everywhere: Overkill, disables interrupts unnecessarily
- No synchronization: Will cause SPI bus conflicts and crashes

**Rationale**: Dual-core architecture with mutex synchronization provides best performance without SPI deadlock. Background task handles slow network I/O without blocking RFID scanning.

---

### 5. SD Card FAT32 Append Operations

**Sources**: ESP32 Forum, ESP-IDF FAT documentation, Random Nerd Tutorials (2025)

#### Atomic Write Limitations

**Critical Finding**: SD cards typically don't offer atomicity guarantees. If power loss occurs during FAT sector write, filesystem may be corrupted.

#### Best Practices

**Data Integrity**:
```cpp
File file = SD.open("/queue.jsonl", FILE_APPEND);
file.println(jsonLine); // Append newline-delimited JSON
file.flush();
file.close();
```

**Regular fsync/fflush**: For data loggers writing frequently (every 100ms), send fsync/fflush every 5 seconds to minimize data loss.

**File Handling**: Files opened with fopen/fprintf don't update filesystem metadata until closed (stat() shows zero length until close).

**Error Detection**: Implement checksums or hashes for each entry to detect corruption (scan through file on boot to validate).

**Decision for Implementation**:
- JSON Lines format: Each queue entry is single line (atomic from JSON perspective)
- Flush after every write: `file.flush()` after each `println()`
- Close immediately after write: Don't keep file open
- Validate queue file on boot: Parse each line, skip invalid JSON lines (corruption recovery)
- Accept data loss risk on power failure during write (queue entry may be lost or corrupted)

**Alternatives Rejected**:
- Keep file open: Increases corruption risk, no performance benefit for infrequent writes
- No flush: Data may not persist before power loss
- Complex journaling system: Overkill for queue that already supports loss of oldest entries (FIFO overflow)

**Rationale**: JSON Lines format + flush + close provides best balance of reliability and simplicity. Queue recovery on boot handles corruption gracefully.

---

### 6. JSON Lines (JSONL) Format Best Practices

**Sources**: jsonlines.org, Wikipedia, Technical Explore, JSONL Tools (2025)

#### Format Requirements

- **Newline delimiters**: Use `\n` to delimit JSON objects
- **No comma separators**: Unlike JSON arrays, JSONL objects are NOT comma-separated
- **UTF-8 encoding**: Standard for JSONL files
- **File extensions**: `.jsonl`

#### Best Practices

**Append Operations**: One of JSONL's key advantages - new records easily appended without altering existing data. Ideal for log files, time-series data.

**Validation**: Validate each JSON object before appending to prevent syntax errors.

**Handling Newlines in Data**: Do NOT include newline characters within JSON objects (breaks line-by-line structure). Escape newlines in string fields (`"\\n"`).

**Compression**: Stream compressors like gzip recommended for saving space (`.jsonl.gz`).

**Error Handling**: Reader code should ignore invalid lines OR check last line before continuing to append (handle corruption from interrupted writes).

**Decision for Implementation**:
- Queue file: `/queue.jsonl` on SD card
- Each scan entry: Single-line JSON object with no internal newlines
- Fields: `tokenId`, `teamId`, `deviceId`, `timestamp`
- Write operation: `file.println(jsonString)` (adds `\n` automatically)
- Read operation: Line-by-line parsing, skip invalid JSON lines
- No compression (ESP32 overhead not justified for ~100 entries max)

**Example Queue Entry**:
```json
{"tokenId":"534e2b03","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:30:00.000Z"}
```

**Alternatives Rejected**:
- JSON array: Requires rewriting entire file for each append
- CSV format: Harder to parse, no nested data support
- Binary format: Harder to debug, no human readability

**Rationale**: JSONL provides append-only simplicity, corruption resilience (invalid lines skippable), and excellent tooling support.

---

## Configuration File Format Research

### Best Practices for Non-Technical Users

**Requirement**: Game Masters (non-technical users) must edit configuration file on any text editor.

**Decision**: Simple key=value pairs, one per line, no sections, no comments

**Format**:
```
WIFI_SSID=VenueNetwork
WIFI_PASSWORD=secretpassword123
ORCHESTRATOR_URL=http://10.0.0.100:3000
TEAM_ID=001
DEVICE_ID=SCANNER_01
```

**Required Keys**:
- `WIFI_SSID`: WiFi network name
- `WIFI_PASSWORD`: WiFi password
- `ORCHESTRATOR_URL`: Orchestrator server URL (HTTP only)
- `TEAM_ID`: Team identifier (3-digit number)

**Optional Keys**:
- `DEVICE_ID`: Device identifier (auto-generated from MAC if missing)

**Parser Implementation**:
```cpp
String line;
while (file.available()) {
  line = file.readStringUntil('\n');
  int separatorIndex = line.indexOf('=');
  String key = line.substring(0, separatorIndex);
  String value = line.substring(separatorIndex + 1);
  // Trim whitespace and store
}
```

**Alternatives Rejected**:
- JSON: Requires proper syntax (quotes, braces), intimidating for non-technical users
- INI format with sections: Over-complicated for 5 key-value pairs
- YAML: Whitespace-sensitive, error-prone

**Rationale**: Simplest possible format for non-technical users. Excel, Notepad, vi, nano, TextEdit all handle key=value format perfectly.

---

## Device ID Generation Strategy

### MAC Address-Based Unique Identifier

**Requirement**: Generate unique device ID automatically if not configured by GM.

**ESP32 MAC Address**:
```cpp
uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);
```

**Format Decision**: "SCANNER_" + 12-character uppercase hex MAC without separators

**Example**: `SCANNER_A1B2C3D4E5F6`

**Implementation**:
```cpp
String generateDeviceId() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char deviceId[20];
  sprintf(deviceId, "SCANNER_%02X%02X%02X%02X%02X%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(deviceId);
}
```

**Persistence**: Save to SD card `/device_id.txt` on first generation.

**Alternatives Rejected**:
- UUID: Requires random number generator, 36 characters (too long)
- Sequential numbering: Requires central registration, not autonomous
- Timestamp-based: Not unique if multiple scanners powered on simultaneously

**Rationale**: MAC address is guaranteed unique per device, readable format matches API examples, deterministic (same device = same ID).

---

## Summary of Technology Decisions

| Aspect | Decision | Rationale |
|--------|----------|-----------|
| **WiFi Management** | Event-driven reconnection + 10s polling backup | Immediate disconnection awareness with polling safety net |
| **HTTP Client** | 5-second timeout, fire-and-forget pattern | Balances network reliability with acceptable latency |
| **JSON Parsing** | ArduinoJson v7.x with input filtering | Memory-efficient, mature library, 2025 optimizations |
| **Task Architecture** | Dual-core FreeRTOS with mutex-protected SD access | Utilizes ESP32 capabilities, avoids SPI deadlock |
| **Queue Storage** | JSON Lines (JSONL) on SD card | Append-only simplicity, corruption resilience |
| **Config Format** | Key=value pairs (no sections/comments) | Non-technical user friendly, universally editable |
| **Device ID** | MAC address-based with "SCANNER_" prefix | Unique, readable, deterministic, autonomous |
| **Connection Monitoring** | 10-second interval background task | Matches PWA pattern, reasonable network overhead |
| **Batch Size** | 10 scans per batch upload | Matches PWA pattern, balances HTTP overhead with queue processing speed |
| **Queue Limit** | 100 scans FIFO | Matches PWA pattern, prevents unbounded SD card usage |

---

## Open Questions Resolved

### Q: Does orchestrator API require authentication for player scanner endpoints?
**A**: No authentication - Open API on trusted local network (spec clarification 2025-10-19)

### Q: Should scanner use HTTP or HTTPS for orchestrator communication?
**A**: HTTP only - No encryption on trusted venue network (spec clarification 2025-10-19)

### Q: What is exact format of /config.txt?
**A**: Simple key=value pairs - One per line, no sections or comments (spec clarification 2025-10-19)

### Q: How should scanner handle WiFi disconnection during gameplay?
**A**: Auto-reconnect every 10 seconds - Background task monitors and reconnects automatically (spec clarification 2025-10-19)

### Q: What format should auto-generated device IDs use?
**A**: Hex with prefix - "SCANNER_A1B2C3D4E5F6" (readable, matches API examples) (spec clarification 2025-10-19)

---

## Next Steps (Phase 1)

1. Generate `data-model.md` - Define entities and state machines
2. Generate `contracts/` directory - Config format, queue format, API client specifications
3. Generate `quickstart.md` - GM setup guide for configuration
4. Update agent context - Add new technologies to `.specify/memory/context-claude.md`
5. Re-evaluate Constitution Check post-design

---

**Research Complete**: 2025-10-19
**Phase 0 Status**: ✅ All clarifications resolved, ready for Phase 1 design
