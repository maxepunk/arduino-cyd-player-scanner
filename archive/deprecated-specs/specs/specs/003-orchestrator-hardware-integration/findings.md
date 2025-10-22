# Orchestrator Integration - Investigation Findings
**Date**: 2025-10-18
**Status**: Analysis Complete - Design Confirmed

---

## 1. PLAYER Interaction Requirements (CONFIRMED)

### 1.1 Player Workflow (Zero Data Entry Required)

```
PLAYER discovers scanner during gameplay
  ↓
Scans RFID memory token
  ↓
Scanner automatically:
  - Reads token ID from RFID
  - Checks if token has video (from cached tokens.json)
  - IF has video AND orchestrator connected:
      → Send POST /api/scan {tokenId, deviceId, teamId}
      → Show "Sending..." modal (2.5s max, fire-and-forget)
  - IF offline OR no video:
      → Queue scan (if has video) OR skip network (if no video)
  ↓
Display image + play audio IMMEDIATELY (local, no network dependency)
  ↓
(Optional) Player double-taps to dismiss and scan next token
```

**No player input required** - scanner is pre-configured by GM with all necessary settings.

### 1.2 Touch Interactions (Minimal)

**Implemented**:
- **Double-tap**: Return to scan mode (dismiss current image/audio)

**Optional additions**:
- **Single tap during idle**: Show status screen (WiFi, Queue size, Last sync)
- **Tap during image display**: Pause/resume audio (if desired)

**NOT needed**:
- ❌ Team selection (GM pre-configures per scanner)
- ❌ Character entry (no player data entry)
- ❌ Network configuration (GM pre-configures via SD card)

---

## 2. GM Setup Workflow (SD Card Configuration)

### 2.1 Configuration File: `/config.txt`

**Format**: Simple key=value pairs (INI-style)

```ini
# ALN Player Scanner Configuration
# Edit on computer, insert SD card, power cycle scanner

# WiFi Settings (REQUIRED)
wifi_ssid=VenueWiFiNetwork
wifi_password=SecurePassword123

# Orchestrator Settings (REQUIRED)
orchestrator_url=http://10.0.0.100:3000

# Device Identification (REQUIRED)
# Unique ID for THIS specific scanner (used for tracking)
device_id=PLAYER_SCANNER_01

# Team Assignment (REQUIRED for multi-team play)
# Scanner sends this teamId with every scan request
# Use different scanners per team, each with its own teamId
team_id=001
```

**Design Decision (Confirmed)**:
- Team ID is **GM-configured per scanner** (not player-selectable)
- Each physical scanner is assigned to a specific team
- Team commitment happens conceptually when GM assigns scanner to team
- Scanner includes teamId in all scan requests to orchestrator

### 2.2 Device ID Strategy

**Hybrid approach**:
1. Check `config.txt` for `device_id` (if GM provided friendly name)
2. If not in config, check `/device_id.txt` (auto-generated on first boot)
3. If neither exists, generate from MAC address and save to `/device_id.txt`

```cpp
String getDeviceId() {
    // Priority 1: config.txt (GM override)
    if (config.deviceID.length() > 0) {
        return config.deviceID;
    }

    // Priority 2: Previously auto-generated
    if (SD.exists("/device_id.txt")) {
        File f = SD.open("/device_id.txt");
        String saved = f.readStringUntil('\n');
        f.close();
        saved.trim();
        if (saved.length() > 0) return saved;
    }

    // Priority 3: Auto-generate from MAC
    String mac = WiFi.macAddress();
    String deviceId = "PLAYER_" + mac.substring(12);  // Last 6 chars
    deviceId.replace(":", "");  // Remove colons

    // Persist for future boots
    File out = SD.open("/device_id.txt", FILE_WRITE);
    out.println(deviceId);
    out.close();

    return deviceId;
}
```

**Benefit**: Each scanner gets unique ID automatically, but GM can override with human-readable names.

### 2.3 GM Setup Procedure

**Before Event**:
1. GM prepares SD card with `/config.txt`
2. Sets WiFi credentials for venue
3. Sets orchestrator URL (e.g., `http://10.0.0.100:3000`)
4. Sets device_id (e.g., `PLAYER_SCANNER_01`)
5. Sets team_id (e.g., `001` for Team 1)
6. Inserts SD card into scanner
7. Powers on scanner
8. Verifies "Connected ✓" on display
9. Repeats for each scanner (different device_id and team_id per scanner)

**At Venue**:
- If WiFi changed: Update `/config.txt`, power cycle scanner
- Otherwise: Just power on, auto-connects

**During Gameplay**:
- No GM intervention needed
- Players discover scanners and use them
- Each scanner sends its configured teamId automatically

---

## 3. tokens.json Sync Strategy (CONFIRMED)

### 3.1 Sync Timing: On Boot Only

**Boot sequence**:
```
1. Load /config.txt
2. Connect to WiFi
3. Attempt HTTP GET /api/tokens
4. IF successful:
     - Parse response, extract tokens with video flags
     - Save full JSON to /tokens.json (cache)
     - Build lookup table in RAM: Map<tokenId, hasVideo>
   IF failed (offline):
     - Load /tokens.json from SD card (cached from previous sync)
     - Build lookup table from cache
5. Ready to scan
```

**No periodic re-sync** during operation - only on boot.

### 3.2 Streaming Parse (RAM-Safe)

**Never load entire JSON into RAM** - stream and extract only needed data:

```cpp
bool syncTokens() {
    HTTPClient http;
    http.begin(config.orchestratorURL + "/api/tokens");
    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.println("Sync failed, using cache");
        return loadTokensFromCache();
    }

    // Save to SD while parsing
    File cache = SD.open("/tokens.json", FILE_WRITE);
    WiFiClient *stream = http.getStreamPtr();

    // Stream JSON to SD card
    while (stream->available()) {
        char c = stream->read();
        cache.write(c);
    }
    cache.close();

    // Now parse from SD (safer than parsing from stream)
    return loadTokensFromCache();
}

bool loadTokensFromCache() {
    File f = SD.open("/tokens.json");
    if (!f) return false;

    // Use ArduinoJson filter to extract only video field
    StaticJsonDocument<1024> filter;
    filter["tokens"][0]["video"] = true;

    DynamicJsonDocument doc(8192);  // 8KB buffer (enough for ~100 tokens)
    DeserializationError error = deserializeJson(doc, f,
                                    DeserializationOption::Filter(filter));
    f.close();

    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Build lookup table
    tokenHasVideo.clear();
    JsonObject tokens = doc["tokens"];
    for (JsonPair kv : tokens) {
        String tokenId = kv.key().c_str();
        bool hasVideo = !kv.value()["video"].isNull();
        tokenHasVideo[tokenId] = hasVideo;
    }

    Serial.printf("Loaded %d tokens\n", tokenHasVideo.size());
    return true;
}
```

**Memory usage**:
- Parsing buffer: 8KB (temporary during boot)
- Lookup table: ~2.5KB (100 tokens × 25 bytes each)
- **Total persistent**: <3KB

### 3.3 Initial Cache for Development

**Ship with pre-cached tokens.json** on SD card:
- Include current `/tokens.json` in SD card image
- Ensures scanner works offline even on first boot
- Gets updated when orchestrator is available

**SD card structure**:
```
/config.txt          ← GM edits
/tokens.json         ← Cached from sync (or shipped with scanner)
/queue.jsonl         ← Offline queue (created when needed)
/device_id.txt       ← Auto-generated (or GM override)
/images/             ← BMP files (token_id.bmp)
/audio/              ← WAV files (token_id.wav)
```

---

## 4. Offline Queue (Auto-Process in Background)

### 4.1 Queue File: `/queue.jsonl`

**JSON Lines format** (newline-delimited):
```json
{"tokenId":"abc123","deviceId":"PLAYER_01","teamId":"001","timestamp":"2025-10-18T14:25:00Z"}
{"tokenId":"xyz789","deviceId":"PLAYER_01","teamId":"001","timestamp":"2025-10-18T14:27:15Z"}
```

**Append operation** (fast, atomic):
```cpp
void queueScan(String tokenId) {
    File f = SD.open("/queue.jsonl", FILE_APPEND);
    if (f) {
        StaticJsonDocument<256> doc;
        doc["tokenId"] = tokenId;
        doc["deviceId"] = getDeviceId();
        doc["teamId"] = config.teamID;
        doc["timestamp"] = getISOTimestamp();

        serializeJson(doc, f);
        f.println();  // Newline separator
        f.close();

        queueSize++;  // Track size
    }
}
```

### 4.2 Background Queue Processing (FreeRTOS Task)

**Auto-process when connection restored** - invisible to player:

```cpp
void queueProcessTask(void *parameter) {
    while (true) {
        // Wait for notification from connection monitor
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!orchestratorConnected || queueSize == 0) continue;

        Serial.printf("Processing %d queued scans...\n", queueSize);

        // Read up to 10 scans for batch
        std::vector<String> batch;
        File f = SD.open("/queue.jsonl");
        while (f.available() && batch.size() < 10) {
            batch.push_back(f.readStringUntil('\n'));
        }
        f.close();

        // Build batch JSON
        String payload = "{\"transactions\":[";
        for (int i = 0; i < batch.size(); i++) {
            if (i > 0) payload += ",";
            payload += batch[i];
        }
        payload += "]}";

        // Send batch
        HTTPClient http;
        http.begin(config.orchestratorURL + "/api/scan/batch");
        http.setTimeout(10000);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(payload);
        http.end();

        if (httpCode == 200) {
            // Success - remove processed lines
            removeFirstNLines("/queue.jsonl", batch.size());
            queueSize -= batch.size();

            Serial.printf("Batch uploaded, %d remain\n", queueSize);

            // Continue processing if more remain
            if (queueSize > 0) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }
        } else {
            // Failed - leave queue, will retry later
            Serial.printf("Batch upload failed: %d\n", httpCode);
        }

        // Wait for next connection restore event
    }
}
```

**Player experience**: Invisible - queue uploads happen in background, no blocking or notifications.

### 4.3 Queue Overflow (FIFO)

**Limit**: 100 scans

**Behavior when full**:
```cpp
void queueScan(String tokenId) {
    // Check size before queueing
    if (queueSize >= 100) {
        // Remove oldest (first line)
        removeFirstNLines("/queue.jsonl", 1);
        queueSize--;

        Serial.println("Queue full - oldest scan discarded");
        // Brief flash on display: "Queue Full!" (200ms, then disappear)
    }

    // Now append new scan
    // ... (as above)
}
```

**Player sees**: Brief yellow flash "Queue Full!" but scan still succeeds (local display works)

---

## 5. Status Feedback (Display + Audio Only)

### 5.1 Available Feedback (No LEDs/Buttons During Gameplay)

**Available**:
- ✅ TFT Display (240x320 color)
- ✅ Speaker (I2S audio - beep patterns)
- ✅ Touch IRQ (double-tap interaction)

**Not available** (covered by case):
- ❌ RGB LEDs
- ❌ Physical buttons

### 5.2 Status Display Patterns

**Idle/Ready Screen**:
```
┌──────────────────────┐
│   NeurAI Scanner     │
│                      │
│   READY TO SCAN      │
│                      │
│   ●  Connected       │  ← Green dot (text color)
│   Queue: 0           │
│                      │
│   Tap for status     │
└──────────────────────┘
```

**During Scan** (if has video and connected):
```
┌──────────────────────┐
│                      │
│   Sending...         │  ← Shows for 2.5s max
│                      │
└──────────────────────┘
    ↓ (immediate, doesn't wait)
┌──────────────────────┐
│  [Token Image]       │
│                      │
│  [Playing Audio]     │
│                      │
│  Double-tap to exit  │
└──────────────────────┘
```

**Status Screen** (single tap during idle):
```
┌──────────────────────┐
│  Scanner Status      │
│                      │
│  WiFi: VenueWiFi ✓   │
│  Server: Connected   │
│  Queue: 3 scans      │
│  Team: 001           │
│  Device: PLAYER_01   │
│                      │
│  Tap to dismiss      │
└──────────────────────┘
```

### 5.3 Audio Feedback (Minimal Beeps)

**Very short beeps** (avoid interfering with memory audio):
- **Send success**: Single beep (50ms, 1200Hz) - barely noticeable
- **Queued**: Two quick beeps (50ms each, 1000Hz)
- **Queue full**: Low beep (100ms, 600Hz)

**No beep for**:
- Tokens without video (nothing to send)
- Background queue processing (silent)

---

## 6. RAM Budget & Conservative Estimates

### 6.1 Known Allocations

**During scan operation**:
- WiFi stack: ~100-150KB (always resident)
- BMP row buffer: 720 bytes (allocated/freed per row)
- Audio streaming: ~5-10KB (estimate, inside library)
- HTTP client buffers: ~4KB (temporary during requests)
- ArduinoJson doc: 8KB (during boot sync only)
- Lookup table: ~2.5KB (persistent)

**Total persistent during gameplay**: ~120-170KB

**Available for operations**: ~250-350KB free heap (estimate)

### 6.2 Safe Margins

**Assumptions**:
- Total SRAM: ~520KB
- WiFi overhead: 150KB (worst case)
- Audio buffers: 10KB (worst case)
- Our persistent data: 3KB (lookup table)
- **Minimum free heap target**: 200KB

**Conclusion**: tokens.json full sync (8KB buffer) is safe even during operation, but we'll do it at boot for simplicity.

---

## 7. Implementation Phases

### Phase 1: Network Connectivity (No Queue)
**Goal**: Scanner sends scans when online, fails silently when offline

- WiFi connection from config.txt
- HTTP POST to /api/scan (fire-and-forget)
- Success/failure indicators on display
- **Deliverable**: Basic networked scanning

### Phase 2: Offline Queue
**Goal**: Queue scans offline, auto-sync when reconnected

- Queue to /queue.jsonl on send failure
- Background connection monitoring task
- Auto batch upload when online
- **Deliverable**: Resilient offline operation

### Phase 3: Token Database Sync
**Goal**: Only send scans for tokens with video

- Fetch /api/tokens on boot
- Cache to /tokens.json
- Build lookup table
- Check before sending
- **Deliverable**: Smart video detection

### Phase 4: Polish & UX
**Goal**: Production-ready scanner

- Status screen (tap to view)
- Better error messages
- Queue overflow handling
- Field testing refinements
- **Deliverable**: Production scanner

---

## 8. Confirmed Design Decisions

✅ **Team ID**: GM-configured per scanner (in config.txt)
✅ **No player character entry**: Players only scan and view
✅ **Token sync**: On boot only (use cache if offline)
✅ **Queue processing**: Auto in background (invisible to player)
✅ **tokens.json cache**: Ship with initial cache, update from orchestrator when available
✅ **Status feedback**: Display + audio only (no LEDs/buttons accessible)
✅ **Configuration**: SD card `/config.txt` edited by GM before gameplay

---

## 9. Ready for Specification

All design decisions confirmed. Ready to write formal feature specification with:
- User stories (GM setup, Player usage)
- Acceptance criteria
- Technical requirements
- API contracts
- Test scenarios

**Proceed to spec.md creation.**
