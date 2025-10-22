# Orchestrator Integration Analysis
## Hardware Scanner Implementation Research

**Created**: 2025-10-18
**Source PWA**: `/home/maxepunk/projects/AboutLastNight/aln-memory-scanner`
**Target Hardware**: CYD ESP32 RFID Scanner (ST7789, MFRC522)

---

## Executive Summary

The PWA (aln-memory-scanner) serves as a reference implementation for orchestrator integration. The hardware scanner must implement the **networked mode** behavior while respecting ESP32 constraints.

### Critical Understanding: Local vs. Network Playback

**LOCAL PLAYBACK (Image/Audio):**
- ALWAYS immediate, never queued
- Handled entirely on scanner device
- No orchestrator dependency
- Already implemented in hardware (BMP display, I2S audio)

**VIDEO PLAYBACK TRIGGER:**
- Sent to orchestrator via HTTP POST
- Queued offline if orchestrator unavailable
- Orchestrator plays video on shared display
- Scanner shows "processing" modal during send (2.5s timeout)

**This is NOT "queue the scan" - it's "queue the video trigger notification"**

---

## 1. PWA Architecture Analysis

### 1.1 Dual-Mode Operation

The PWA has **two distinct modes**:

**Standalone Mode** (GitHub Pages deployment):
- Path: NOT `/player-scanner/`
- No orchestrator connection
- No offline queue
- All processing local only
- **EXCLUDED from hardware port** (hardware is always networked)

**Networked Mode** (Orchestrator deployment):
- Path: `/player-scanner/`
- Connects to orchestrator
- Offline queue support
- Connection monitoring
- **THIS is what hardware implements**

### 1.2 Key Behavior Patterns

#### Token Processing Flow (index.html:1084-1142)

```
1. Token scanned (RFID or QR)
2. Check: orchestrator.connected && token.video?
   YES → Send to orchestrator (/api/scan)
         Show "processing" modal (2.5s max)
   NO  → Skip orchestrator (no video or offline)
3. ALWAYS display local content (image/audio)
4. Save to scannedTokens collection
5. Haptic feedback
```

**Critical Insight**: Local image/audio display happens **regardless** of orchestrator status. Only video trigger is networked.

#### Offline Queue (orchestratorIntegration.js:98-113)

```javascript
queueOffline(tokenId, teamId) {
  // Enforce queue limit
  if (this.offlineQueue.length >= this.maxQueueSize) {
    this.offlineQueue.shift(); // Remove oldest if at limit
  }

  this.offlineQueue.push({
    tokenId,
    teamId,
    timestamp: Date.now(),
    retryCount: 0
  });

  this.saveQueue(); // Persist to localStorage
}
```

**Queue Behavior**:
- Max 100 items
- FIFO with overflow (oldest discarded)
- Persisted to localStorage (hardware: SD card)
- Batch upload on reconnect (10 at a time)

#### Connection Monitoring (orchestratorIntegration.js:179-210)

```javascript
async checkConnection() {
  try {
    const response = await fetch(`${this.baseUrl}/health`, {
      method: 'GET',
      cache: 'no-cache',
      signal: AbortSignal.timeout(5000)
    });

    const wasOffline = !this.connected;
    this.connected = response.ok;

    if (this.connected && wasOffline) {
      console.log('Connection restored!');
      this.onConnectionRestored(); // Triggers queue processing
    }
  } catch (error) {
    this.connected = false;
  }
}
```

**Monitoring Pattern**:
- Poll `/health` endpoint every 10 seconds
- 5-second timeout
- State change events: `orchestrator:connected` / `orchestrator:disconnected`
- Automatic queue processing on reconnect

---

## 2. Orchestrator API Contracts (OpenAPI)

### 2.1 Token Database Sync

**Endpoint**: `GET /api/tokens`

**Response Structure**:
```json
{
  "tokens": {
    "534e2b02": {
      "image": "assets/images/534e2b02.jpg",
      "audio": "assets/audio/534e2b02.mp3",
      "video": null,
      "processingImage": null,
      "SF_RFID": "534e2b02",
      "SF_ValueRating": 3,
      "SF_MemoryType": "Technical",
      "SF_Group": ""
    },
    "534e2b03": {
      "image": null,
      "audio": null,
      "video": "test_30sec.mp4",
      "processingImage": "534e2b03.jpg",
      "SF_RFID": "534e2b03",
      "SF_ValueRating": 3,
      "SF_MemoryType": "Technical",
      "SF_Group": ""
    }
  },
  "count": 2,
  "lastUpdate": "2025-09-30T12:00:00.000Z"
}
```

**Hardware Implications**:
- Scanner needs tokens.json to know which tokens have video
- Must sync on boot / periodically
- Store to SD card for offline operation
- Use `lastUpdate` timestamp to check for updates

**Use Case**: Scanner checks `token.video` field to decide whether to send orchestrator request.

### 2.2 Single Scan Endpoint

**Endpoint**: `POST /api/scan`

**Request**:
```json
{
  "tokenId": "534e2b03",
  "teamId": "001",        // OPTIONAL (players not committed to team yet)
  "deviceId": "PLAYER_SCANNER_01",
  "timestamp": "2025-09-30T14:30:00.000Z"
}
```

**Response** (200 OK):
```json
{
  "status": "accepted",
  "message": "Video queued for playback",
  "tokenId": "534e2b03",
  "mediaAssets": {
    "video": "test_30sec.mp4",
    "image": null,
    "audio": null
  },
  "videoQueued": true
}
```

**Response** (409 Conflict - Video already playing):
```json
{
  "status": "rejected",
  "message": "Video already playing, please wait",
  "tokenId": "534e2b03",
  "waitTime": 30
}
```

**Fire-and-Forget Pattern** (openapi.yaml:294-299):
> Player Scanner **IGNORES response body** by design (ESP32 compatibility).
> Response provided for debugging/logging only.

**Hardware Implications**:
- Send request, don't block on response
- 5-second timeout
- Show "sending" indicator, hide after timeout
- Queue if send fails

### 2.3 Batch Upload Endpoint

**Endpoint**: `POST /api/scan/batch`

**Request**:
```json
{
  "transactions": [
    {
      "tokenId": "534e2b02",
      "teamId": "001",
      "deviceId": "PLAYER_SCANNER_01",
      "timestamp": "2025-09-30T14:25:00.000Z"
    },
    {
      "tokenId": "tac001",
      "deviceId": "PLAYER_SCANNER_01",
      "timestamp": "2025-09-30T14:26:30.000Z"
    }
  ]
}
```

**Response**:
```json
{
  "results": [
    {
      "tokenId": "534e2b02",
      "status": "processed",
      "videoQueued": false
    },
    {
      "tokenId": "tac001",
      "status": "processed",
      "videoQueued": false
    }
  ]
}
```

**Batch Size**: 10 scans per request (orchestratorIntegration.js:142)

### 2.4 Health Check Endpoint

**Endpoint**: `GET /health`

**Response**:
```json
{
  "status": "online",
  "version": "1.0.0",
  "uptime": 3600.5,
  "timestamp": "2025-09-30T12:00:00.000Z"
}
```

**Usage**: Connection monitoring (every 10 seconds)

---

## 3. Hardware Capabilities & Limitations

### 3.1 Current Hardware Scanner Features

**Display**: ST7789 TFT (240x320)
- BMP image display ✅ (already implemented)
- Color text rendering ✅
- Visual feedback indicators ✅

**Audio**: I2S DAC
- WAV playback ✅ (already implemented)
- Background audio during display ✅

**RFID**: MFRC522 (Software SPI)
- Card UID reading ✅
- NDEF text extraction ✅
- Debouncing implemented ✅ (500ms scan interval)

**Storage**: SD Card (FAT32)
- BMP file reading ✅
- Configuration persistence ✅ (via TFT_eSPI User_Setup.h pattern)
- WAV file playback ✅

**Network**: ESP32 WiFi
- HTTP client library available ✅
- JSON parsing (ArduinoJson) available ✅
- TLS/HTTPS capable ✅

**Serial Debugging**: 115200 baud
- Command interface ✅
- Diagnostics output ✅

### 3.2 Critical Limitations

#### Touch Input: IRQ Tap Detection Only

**From HARDWARE_SPECIFICATIONS.md**:
```
Touch (XPT2046):
- IRQ: GPIO36 (Tap detection only, no coordinates)
```

**Implications for Configuration UX**:
- Cannot implement full touchscreen keyboard
- Cannot use touch-based menus with precise selection
- Cannot implement drag/swipe gestures

**Alternative Input Methods**:
1. **Serial Commands** (recommended for initial setup)
   - `SET_WIFI SSID password`
   - `SET_ORCHESTRATOR url`
   - `SET_TEAM teamId`
   - Easy for tech setup, difficult for field reconfiguration

2. **Touch Tap + Physical Buttons** (if available)
   - Not mentioned in hardware specs - verify availability

3. **Predefined Configuration Files on SD Card**
   - `config.txt` with WiFi/orchestrator settings
   - Edit on computer, insert SD card
   - Scanner reads on boot

4. **BLE Configuration** (future consideration)
   - Use phone app to configure scanner
   - Not in current scope

**Recommended Approach for MVP**:
- Serial commands for technical setup (GM/admin)
- SD card `config.txt` for venue-specific settings
- Simple tap feedback (single-tap = confirm, no complex menus)

#### Memory Constraints

**ESP32 WROOM-32**:
- SRAM: ~520KB (shared with WiFi stack)
- Flash: Depends on partition scheme
- PSRAM: Not mentioned (assume not available)

**Implications**:
- Cannot buffer large JSON responses in RAM
- Must stream/parse tokens.json incrementally
- Offline queue limited by SD card I/O performance
- ArduinoJson may need streaming mode

#### Network Performance

**Venue WiFi Assumptions**:
- Latency: 50-500ms (could spike higher)
- Bandwidth: Sufficient for small JSON payloads
- Reliability: May drop occasionally (hence offline queue)

**Timeout Strategy**:
- HTTP request timeout: 5 seconds (per PWA)
- Connection check timeout: 5 seconds
- Retry with exponential backoff (not immediate retry)

---

## 4. Feature Mapping: PWA → Hardware

### 4.1 Features to Port (In Scope)

| PWA Feature | Hardware Implementation | Notes |
|-------------|------------------------|-------|
| **Video Trigger Send** | HTTP POST to `/api/scan` | Fire-and-forget pattern |
| **Offline Queue** | Queue file on SD card (JSON lines) | Max 100 entries, FIFO overflow |
| **Batch Upload** | HTTP POST to `/api/scan/batch` | On reconnect, 10 per batch |
| **Connection Monitoring** | Periodic `/health` check (10s) | LED status indicator |
| **Device ID** | Generated on first boot, saved to SD | Persistent identifier |
| **Team ID** | Configurable via serial/SD config | Optional field |
| **tokens.json Sync** | Fetch on boot, check `lastUpdate` | Cache to SD card |
| **Local Image Display** | BMP display (already implemented) | Independent of network |
| **Local Audio Playback** | I2S WAV playback (already implemented) | Independent of network |
| **Processing Modal** | "Sending..." text + icon on display | 2.5s timeout |

### 4.2 Features to Exclude (Out of Scope)

| PWA Feature | Reason for Exclusion |
|-------------|---------------------|
| **QR Code Scanning** | Hardware uses RFID only |
| **NFC Scanning** | Browser-specific, hardware has RFID |
| **Standalone Mode** | Hardware is always networked mode |
| **Path-Based Mode Detection** | Hardware mode set by configuration |
| **LocalStorage API** | Use SD card filesystem |
| **Browser Fetch API** | Use ESP32 HTTPClient |
| **Service Workers** | PWA-specific offline caching |
| **Manual Token Entry** | No keyboard input on hardware |
| **Collection Progress Tracking** | Server-side only, not scanner |
| **Touchscreen Menus** | Limited touch detection (IRQ only) |

### 4.3 Hardware-Specific Additions

| Feature | Purpose |
|---------|---------|
| **LED Status Indicator** | Visual connection state (green/yellow/red) |
| **Serial Configuration Interface** | Setup WiFi/orchestrator without touch UI |
| **SD Card Config File** | `config.txt` for venue settings |
| **Incremental JSON Parsing** | Handle large tokens.json with limited RAM |
| **Queue Persistence on Power Loss** | Flush queue file after each write |
| **Beep Feedback** | Audio confirmation of send/queue (if not intrusive) |

---

## 5. Configuration UX Design (Limited Touch Input)

### 5.1 Setup Workflow

**Initial Setup** (Tech/GM via Serial):
```
1. Connect scanner to computer via USB
2. Open serial monitor (115200 baud)
3. Run commands:
   > SET_WIFI "VenueSSID" "password123"
   > SET_ORCHESTRATOR "http://10.0.0.100:3000"
   > SET_TEAM "001"
   > SAVE_CONFIG
4. Disconnect, power cycle scanner
5. Scanner auto-connects and is ready
```

**Alternative: SD Card Config** (Non-Technical):
```
1. Create config.txt on SD card:
   wifi_ssid=VenueSSID
   wifi_password=password123
   orchestrator_url=http://10.0.0.100:3000
   team_id=001
   device_id=SCANNER_01

2. Insert SD card into scanner
3. Power on - scanner reads config.txt
4. Display shows "Connected" or "Offline" status
```

### 5.2 Status Display (No Touch Required)

**LED/Display Indicators**:
- **Green LED** / "Connected": Orchestrator online
- **Yellow LED** / "Queued (3)": Offline, queue has 3 items
- **Red LED** / "Queue Full!": Offline, queue at 100 items

**During Scan**:
```
[RFID Card Detected]
↓
Display: "Processing..." (2.5s max)
↓
If video token:
  - Send to orchestrator (or queue)
  - Show "Sent ✓" or "Queued ●"
↓
Display: Local image + play audio
```

### 5.3 Minimal Touch Interaction

**Single Tap Functions** (if tap detection is reliable):
- Tap display during idle: Show status screen
  - WiFi SSID: VenueSSID
  - Orchestrator: Connected ✓
  - Queue: 0 items
  - Team: 001

- Tap again: Return to ready screen

**No Complex Menus**: Avoid multi-level navigation due to IRQ-only touch.

---

## 6. Technical Implementation Notes

### 6.1 Token Metadata Storage

**Challenge**: `tokens.json` may be too large for ESP32 RAM.

**Solution Options**:

**Option A: Lightweight Lookup Table** (Recommended)
- On boot, fetch `/api/tokens`
- Parse JSON, extract only: `{tokenId: hasVideo}`
- Store as compact binary table on SD card:
  ```
  534e2b02|0
  534e2b03|1
  tac001|0
  jaw001|1
  ```
- ~100 tokens × 20 bytes = 2KB in RAM

**Option B: Full Caching**
- Cache entire `tokens.json` to SD card
- Check `lastUpdate` timestamp on each boot
- Re-download if changed
- Look up token properties on each scan

**Recommendation**: Option A for MVP (minimal RAM, fast lookup)

### 6.2 Offline Queue File Format

**Location**: `/queue.json` on SD card

**Format**: JSON Lines (newline-delimited JSON objects)
```json
{"tokenId":"534e2b02","teamId":"001","deviceId":"SCANNER_01","timestamp":"2025-10-18T14:25:00Z"}
{"tokenId":"tac001","teamId":"001","deviceId":"SCANNER_01","timestamp":"2025-10-18T14:26:30Z"}
```

**Advantages**:
- Append-only (no JSON array overhead)
- Easy to parse line-by-line
- Resilient to corruption (only last line at risk)

**Operations**:
- **Append**: Open file in append mode, write JSON + newline, flush
- **Read**: Parse each line individually
- **Remove**: After successful batch upload, rewrite file without sent lines

### 6.3 HTTP Client Configuration

**Library**: ESP32 `HTTPClient.h`

**Example Code Pattern**:
```cpp
HTTPClient http;
http.begin(orchestratorUrl + "/api/scan");
http.setTimeout(5000);  // 5 second timeout
http.addHeader("Content-Type", "application/json");

String payload = "{\"tokenId\":\"" + tokenId + "\",\"deviceId\":\"" + deviceId + "\"}";
int httpCode = http.POST(payload);

if (httpCode == 200) {
  // Success - video queued
} else if (httpCode == 409) {
  // Video already playing - continue anyway
} else {
  // Failed - queue offline
  queueOffline(tokenId, teamId);
}

http.end();
```

**Fire-and-Forget**: Do NOT block on response parsing. Just check HTTP code.

### 6.4 Background Tasks

**Challenge**: Connection monitoring must not block RFID scanning.

**Solution**: FreeRTOS Tasks (ESP32 native)

**Task Structure**:
```cpp
// Task 1: RFID Scanning (High Priority)
void rfidScanTask(void *parameter) {
  while(1) {
    if (rfid.PICC_IsNewCardPresent()) {
      // Read card, display content, send to orchestrator
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);  // 500ms scan interval
  }
}

// Task 2: Connection Monitoring (Low Priority)
void connectionMonitorTask(void *parameter) {
  while(1) {
    checkOrchestratorConnection();  // Non-blocking HTTP GET
    vTaskDelay(10000 / portTICK_PERIOD_MS);  // 10 second interval
  }
}

// Task 3: Queue Processing (Low Priority)
void queueProcessTask(void *parameter) {
  while(1) {
    if (orchestratorConnected && queueHasItems()) {
      processQueueBatch();  // Send up to 10 items
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);  // Check every 5s
  }
}
```

**Task Priorities**:
- RFID Scanning: Priority 2 (high)
- Display Updates: Priority 1 (medium)
- Connection Monitor: Priority 0 (low)
- Queue Processing: Priority 0 (low)

---

## 7. Edge Cases & Error Handling

### 7.1 Tokens.json Sync Failures

**Scenario**: Scanner boots, orchestrator unreachable, no cached tokens.json

**Behavior**:
- Show error: "Cannot sync token database"
- Fall back to "display UID only" mode
- All scans queued (assume all might have video)
- When connection restored: sync tokens.json, process queue

### 7.2 SD Card Failures

**Scenario**: SD card removed/corrupted during operation

**Behavior**:
- Detect SD card failure
- Disable queueing (queue lost)
- Continue local playback from last loaded content
- Show warning: "SD Card Error - No Queue"

### 7.3 Queue Overflow

**Scenario**: >100 scans while offline

**Behavior**:
- FIFO: Remove oldest scan when adding 101st
- Log warning to serial: "Queue overflow - discarded scan [tokenId]"
- Display: "Queue Full (100)" indicator
- When space available: Resume queueing

### 7.4 Orchestrator Slow Response

**Scenario**: `/api/scan` takes >5 seconds to respond

**Behavior**:
- Timeout after 5 seconds
- Treat as failure, queue scan
- Show "Queued" indicator
- Connection monitor will detect if orchestrator is actually down

### 7.5 WiFi Credentials Wrong

**Scenario**: Scanner configured with invalid WiFi password

**Behavior**:
- WiFi connection fails
- Display: "WiFi Error - Check Config"
- Enter "offline mode" (all scans queued)
- Allow serial reconfiguration without reboot

---

## 8. Testing Strategy

### 8.1 Unit Tests (Arduino Test Framework)

- Token lookup table parsing
- Queue file read/write operations
- JSON serialization/deserialization
- Configuration file parsing

### 8.2 Integration Tests

**Test 1: Online Scan Flow**
1. Configure scanner with valid orchestrator URL
2. Scan token with video (`534e2b03`)
3. Verify HTTP POST sent within 2s
4. Verify local image/audio displayed immediately
5. Verify "Sent ✓" indicator shown

**Test 2: Offline Queue**
1. Configure scanner, then disconnect orchestrator
2. Scan 5 tokens
3. Verify queue.json has 5 entries
4. Reconnect orchestrator
5. Verify batch upload within 30s
6. Verify queue.json cleared

**Test 3: Queue Overflow**
1. Disconnect orchestrator
2. Scan 105 tokens
3. Verify queue size stays at 100
4. Verify oldest 5 scans discarded

**Test 4: Power Cycle with Queue**
1. Queue 10 scans offline
2. Power cycle scanner
3. Verify queue persists
4. Reconnect orchestrator
5. Verify queued scans uploaded

**Test 5: tokens.json Sync**
1. Boot scanner with orchestrator online
2. Verify `/api/tokens` fetched
3. Verify lookup table created
4. Scan token, verify video check works
5. Update tokens.json on server
6. Reboot scanner
7. Verify new data fetched

### 8.3 Field Tests

- Venue WiFi reliability testing
- Multiple scanners simultaneous use
- Long-duration operation (2+ hours)
- Network congestion scenarios

---

## 9. Open Questions / Needs Clarification

1. **Team ID Assignment**: How do players select team?
   - Via serial command pre-game?
   - Via orchestrator (outside scanner scope)?
   - **Clarification**: OpenAPI says teamId is optional for player scanners - team commitment happens at GM Scanner

2. **Device ID Format**: Auto-generate or manually configure?
   - Suggest: `PLAYER_[MAC_ADDRESS_LAST_6]`
   - Example: `PLAYER_A1B2C3`

3. **Configuration Priority**: Serial vs SD card config - which overrides?
   - Suggest: SD card config.txt overrides serial (easier field updates)

4. **Status LED**: Physical LED available? Or on-screen only?
   - Check hardware schematic

5. **Touch Tap Reliability**: Is IRQ tap detection reliable enough for status display?
   - Test in prototype phase

6. **Token Sync Frequency**: Only on boot, or periodic check?
   - Suggest: Boot + manual "SYNC_TOKENS" serial command
   - Auto-sync only if `lastUpdate` timestamp changes

7. **Queue Processing Timing**: Process queue immediately on reconnect, or wait for idle?
   - Suggest: Immediate (like PWA), but pause during active scan

---

## 10. Implementation Phases (Recommended)

### Phase 1: Network Connectivity (No Queue)
- WiFi connection management
- Configuration via serial commands
- HTTP POST to `/api/scan` (fire-and-forget)
- Success/failure indicators on display
- **Deliverable**: Scanner sends scans when online, fails silently when offline

### Phase 2: Offline Queue
- Queue file on SD card (JSON lines format)
- Queue append on send failure
- Connection monitoring task
- Batch upload on reconnect
- **Deliverable**: Scanner queues scans offline, syncs when reconnected

### Phase 3: Token Database Sync
- Fetch `/api/tokens` on boot
- Build lightweight lookup table (tokenId → hasVideo)
- Cache to SD card
- Check `lastUpdate` for updates
- **Deliverable**: Scanner only sends requests for tokens with video

### Phase 4: Configuration UX
- SD card `config.txt` support
- Status display screen (tap to view)
- LED status indicators
- Improved error messages
- **Deliverable**: Non-technical setup via SD card

### Phase 5: Polish & Optimization
- Queue overflow handling
- Better error recovery
- Performance optimization (reduce scan delays)
- Field testing refinements
- **Deliverable**: Production-ready scanner

---

## 11. Conclusion

The orchestrator integration is **well-defined and feasible** for hardware implementation. Key success factors:

1. **Respect the separation**: Local playback (image/audio) is independent of network
2. **Fire-and-forget**: Don't block on orchestrator responses
3. **Queue resilience**: Use append-only file format, handle power loss gracefully
4. **Minimal UX**: Accept touch limitations, use serial + SD card config
5. **Incremental sync**: Don't load full tokens.json into RAM
6. **Background tasks**: Use FreeRTOS to prevent blocking RFID scanning

The PWA provides an excellent reference implementation. The API contracts are clear, and the hardware has all necessary capabilities.

**Next Step**: Create detailed feature specification with user stories and acceptance criteria.
