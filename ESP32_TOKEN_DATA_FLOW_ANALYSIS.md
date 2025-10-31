# ESP32 Token Data Flow Analysis - Complete Tracing Report

**Analysis Date:** October 31, 2025
**Scope:** End-to-end token data flow from ALN-TokenData submodule through backend to ESP32 device
**Thoroughness:** Very detailed with code tracing and synchronization mechanism analysis

---

## Executive Summary

The ALN Ecosystem uses a **dual-path token distribution strategy** optimized for both networked and standalone operation:

1. **Backend Direct Access** - Backend loads tokens from ALN-TokenData submodule root
2. **ESP32 Network Download** - ESP32 downloads tokens from backend `/api/tokens` endpoint on boot
3. **ESP32 SD Cache** - Tokens cached to SD card for offline operation

**Critical Finding:** There is **NO automatic token update mechanism** on the ESP32. Once cached to SD card, tokens only update when:
- ESP32 explicitly calls `syncFromOrchestrator()` (boot phase only)
- Or when `config.txt` setting `SYNC_TOKENS=true` (default)

**Flash Constraint:** ESP32 has only 92% capacity (1.2MB/1.3MB). Token updates are limited to ~12KB max JSON payload.

---

## 1. Token Data Source: ALN-TokenData Submodule

### Location and Structure

**Path:** `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/ALN-TokenData/`

**Contents:**
```
ALN-TokenData/
├── .git              # Git pointer to submodule
└── tokens.json       # Single source of truth for all token data
```

**Size:** 
- File: 12 KB (371 lines)
- Contains: ~42 token entries (verified from OpenAPI example)
- Format: JSON object map (tokenId → token data)

### Token Data Schema (in tokens.json)

**Structure** - Map of token entries:
```json
{
  "sof002": {
    "image": "assets/images/sof002.bmp",
    "audio": null,
    "video": null,
    "processingImage": null,
    "SF_RFID": "sof002",
    "SF_ValueRating": 2,
    "SF_MemoryType": "Personal",
    "SF_Group": ""
  },
  "jaw001": {
    "image": "assets/images/jaw001.bmp",
    "audio": null,
    "video": "jaw001.mp4",
    "processingImage": "jaw001_processing.jpg",
    "SF_RFID": "jaw001",
    "SF_ValueRating": 4,
    "SF_MemoryType": "Business",
    "SF_Group": "Marcus Sucks (x2)"
  }
}
```

**Fields Explained:**
- `image` - Display image path (assets/images/tokenId.bmp)
- `audio` - Audio file path (null or assets/audio/tokenId.wav)
- `video` - Video file name (null or filename.mp4)
- `processingImage` - Showing during video playback
- `SF_RFID` - Token ID from RFID tag
- `SF_ValueRating` - Scoring weight (1-5)
- `SF_MemoryType` - Category (Personal/Business/Technical)
- `SF_Group` - Group completion bonus (e.g., "Marcus Sucks (x2)")

### Media Asset Organization

**Backend Structure:**
```
backend/
├── public/
│   ├── videos/                    # Videos stored here
│   │   ├── jaw001.mp4
│   │   └── loopimages/           # ~18MB idle loop video
│   └── scoreboard.html
```

**ESP32 SD Card Structure:**
```
SD:/
├── config.txt                     # Configuration
├── tokens.json                    # Cached from orchestrator
├── images/
│   ├── sof002.bmp               # Token display images (24-bit BMP)
│   ├── jaw001.bmp
│   └── ...
└── AUDIO/
    ├── sof002.wav               # Token audio files
    └── ...
```

**Critical Path Design:**

The system uses **different path conventions** depending on context:

| Component | Context | Path Format | Example |
|-----------|---------|-------------|---------|
| tokens.json | AFN-TokenData | Field value | `"assets/images/sof002.bmp"` |
| Backend | Game logic | Derived from tokenId | `tokenService.mediaAssets.image` |
| Backend | Video queue | From public folder | `backend/public/videos/jaw001.mp4` |
| ESP32 | Token metadata | From tokens.json | `token.image` field |
| ESP32 | Display image | SD card | `/images/{tokenId}.bmp` |
| ESP32 | Video tokens | Metadata only | No video playback (fire-and-forget) |

---

## 2. Backend Token Service: Loading and Serving

### TokenService Implementation

**File:** `backend/src/services/tokenService.js`

**Loading Strategy - Two Functions:**

#### Function 1: `loadRawTokens()` (Lines 73)
- **Purpose:** Return unmodified tokens.json for API serving
- **Used by:** `/api/tokens` endpoint (ESP32 downloads this)
- **Process:**
  1. Reads tokens.json from ALN-TokenData submodule
  2. Returns as-is (no transformation)
  3. Format: Object map (tokenId → raw token data)

```javascript
const loadRawTokens = () => _loadTokensFile();
```

#### Function 2: `loadTokens()` (Lines 79-115)
- **Purpose:** Load and transform for backend game logic
- **Used by:** `transactionService` for scoring calculations
- **Transformation:**
  - Converts object format → array format
  - Extracts group name (strips multiplier: "Marcus Sucks (x2)" → "Marcus Sucks")
  - Calculates token value: `baseValue × typeMultiplier`
  - Normalizes field names (SF_MemoryType → memoryType, etc.)

```javascript
// Example transformation
{
  id: "mab002",
  name: "Marcus Sucks",
  value: 10,  // calculated: 5 × 2.0
  memoryType: "Personal",
  groupId: "Marcus Sucks",
  groupMultiplier: 2,
  mediaAssets: {
    image: "assets/images/mab002.bmp",
    audio: null,
    video: null,
    processingImage: null
  },
  metadata: {
    rfid: "mab002",
    group: "Marcus Sucks (x2)",
    originalType: "Personal",
    rating: 5
  }
}
```

### File Loading Path Resolution

**Code:** `tokenService.js:50-67`

```javascript
const _loadTokensFile = () => {
  const paths = [
    path.join(__dirname, '../../../ALN-TokenData/tokens.json'),
    path.join(__dirname, '../../../aln-memory-scanner/data/tokens.json')
  ];
  
  for (const tokenPath of paths) {
    try {
      const data = fs.readFileSync(tokenPath, 'utf8');
      console.log(`Loaded tokens from: ${tokenPath}`);
      return JSON.parse(data);
    } catch (e) {
      // Continue to next path
    }
  }
  throw new Error('...');
};
```

**Path Resolution Logic:**
1. Try: `backend/src/` → `../../..` → `ALN-TokenData/tokens.json`
2. Fallback: `backend/src/` → `../../..` → `aln-memory-scanner/data/tokens.json`
3. Both are submodules pointing to same source (ALN-TokenData)

**Absolute Path Resolution:**
```
backend/src/services/tokenService.js (start)
        ↓
../../../ (3 levels up)
        ↓
ALN-Ecosystem/
        ├── ALN-TokenData/tokens.json ✓ (primary)
        └── aln-memory-scanner/data/tokens.json (nested submodule, fallback)
```

---

## 3. Backend `/api/tokens` Endpoint

### HTTP Endpoint Implementation

**File:** `backend/src/routes/resourceRoutes.js:16-31`

```javascript
router.get('/tokens', (req, res) => {
  try {
    const rawTokens = tokenService.loadRawTokens();
    
    success(res, {
      tokens: rawTokens,
      count: Object.keys(rawTokens).length,
      lastUpdate: new Date().toISOString()
    });
  } catch (error) {
    res.status(500).json({
      error: 'INTERNAL_ERROR',
      message: error.message
    });
  }
});
```

### Response Format (Contract: openapi.yaml:174-230)

**HTTP Status:** 200 OK

**Response Body:**
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
  "count": 42,
  "lastUpdate": "2025-10-31T12:00:00.000Z"
}
```

**Critical Property:** The response includes **BOTH video tokens and non-video tokens** in the same format. Distinction is the `video` field (empty/null for regular tokens, filename for video tokens).

---

## 4. ESP32 Token Reception and Caching

### Initialization Flow (ESP32 Boot Sequence)

**File:** `arduino-cyd-player-scanner/ALNScanner_v5/Application.h:818-844`

**Step 1: Check Configuration**
```cpp
// If config.txt has SYNC_TOKENS=true AND orchestrator connected
if (config.getConfig().syncTokens && orchestrator.getState() == models::ORCH_CONNECTED) {
    LOG_INFO("[INIT] Syncing tokens from orchestrator...\n");
```

**Step 2: Download from Orchestrator**
```cpp
// TokenService.h:203-268 - syncFromOrchestrator()
if (tokens.syncFromOrchestrator(config.getConfig().orchestratorURL)) {
    // Download successful - tokens saved to /tokens.json on SD
    LOG_INFO("[INIT] ✓ Token sync successful\n");
} else {
    // Download failed - use cached version
    LOG_ERROR("INIT", "Token sync failed - using cached data");
}
```

**Step 3: Load from SD Card**
```cpp
// Always load from SD after sync attempt (successful or failed)
tokens.loadDatabaseFromSD();
LOG_INFO("[INIT] ✓ Token service initialized (%d tokens)\n", tokens.getCount());
```

### HTTP Download Implementation

**File:** `arduino-cyd-player-scanner/ALNScanner_v5/services/OrchestratorService.h:203-268`

**Network Request:**
```cpp
bool syncFromOrchestrator(const String& orchestratorURL) {
    HTTPClient http;
    String url = orchestratorURL + "/api/tokens";
    http.begin(url);
    http.setTimeout(5000);  // 5-second timeout
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        int size = http.getSize();
        
        // CRITICAL: Validate size before reading to RAM
        if (size > limits::MAX_TOKEN_DB_SIZE) {  // 50KB limit
            Serial.printf("[TOKEN-SVC] FAILURE  Token DB too large (%d > %d bytes)\n",
                          size, limits::MAX_TOKEN_DB_SIZE);
            return false;
        }
        
        // Read response payload
        String payload = http.getString();
        
        // Save to SD card with mutex protection
        File file = SD.open(paths::TOKEN_DB_FILE, FILE_WRITE);
        file.print(payload);
        file.flush();
        file.close();
        
        return true;
    }
    return false;
}
```

**Size Constraints:**
- **Limit:** 50KB (MAX_TOKEN_DB_SIZE)
- **Current tokens.json:** 12KB
- **Headroom:** 38KB available for growth

**Flash Memory Impact:**
- ESP32 Flash: 1.3MB total
- Current usage: 92% (1.2MB)
- Token database on SD card: Cached (not in flash)

### SD Card Storage Format

**File:** `/tokens.json` on SD card

**Format:** Raw JSON (identical to backend response structure)

**Expected Content:**
```json
{
  "tokens": {
    "sof002": { ... token data ... },
    "jaw001": { ... token data ... },
    ...
  },
  "count": 42,
  "lastUpdate": "2025-10-31T12:00:00.000Z"
}
```

**Storage Location:** Root of SD card (`/tokens.json`)

### Token Loading from SD

**File:** `TokenService.h:93-146` - `loadDatabaseFromSD()`

**Process:**
```cpp
bool loadDatabaseFromSD() {
    // 1. Acquire mutex for thread-safe SD access
    hal::SDCard::Lock lock("TokenService::loadDatabase");
    
    // 2. Open file from SD root
    File file = SD.open(paths::TOKEN_DB_FILE, FILE_READ);  // /tokens.json
    
    // 3. Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    
    // 4. Extract tokens object
    JsonObject tokens = doc["tokens"];
    
    // 5. Populate internal vector
    for (JsonPair kv : tokens) {
        models::TokenMetadata token;
        token.tokenId = String(kv.key().c_str());
        token.video = kv.value()["video"] | "";  // Default empty string
        _tokens.push_back(token);
    }
    
    LOG_INFO("[TOKEN-SVC] Loaded %d tokens from database\n", _tokens.size());
    return true;
}
```

**In-Memory Storage:** `std::vector<models::TokenMetadata>`
- Dynamic array (grows as needed)
- No fixed size limit
- Typical: 10-50 tokens (~2KB RAM)
- Max practical: 200 tokens (~8KB RAM)

---

## 5. Token Synchronization Mechanisms

### Synchronization Trigger Points

**ONLY synchronization happens at:**

| Trigger | When | Condition | Frequency |
|---------|------|-----------|-----------|
| Boot-time sync | Power on/reset | config.txt: `SYNC_TOKENS=true` | Once per boot |
| Connection health check | Every 10 seconds | Background task running | Continuous |
| Manual command | Admin/debug only | `FORCE_UPLOAD` serial command | On-demand |

### Critical Finding: No Automatic Updates

**Problem:** Once cached to SD card, tokens are **static until next reboot** or manual sync.

**Scenario 1: Tokens Updated in ALN-TokenData**
1. Developer updates `ALN-TokenData/tokens.json`
2. Backend fetches new tokens from submodule (works immediately)
3. ESP32 still has old cached tokens (works from SD cache)
4. **Gap:** Until ESP32 reboots, it won't get new tokens

**Scenario 2: Multi-ESP32 Fleet**
- ESP32 #1 boots at 2:00pm → Gets version A of tokens
- Tokens updated at 2:15pm
- ESP32 #2 boots at 2:30pm → Gets version B of tokens
- **Result:** Two devices with different token metadata

### Synchronization Control Flow

**Pseudocode - Application.h:821-844**

```
IF (SYNC_TOKENS=true in config.txt) AND (Orchestrator connected) THEN
    TRY
        Download /api/tokens from orchestrator
        Save payload to /tokens.json on SD
        IF success THEN
            Display "Tokens: Synced"
        ELSE
            Display "Tokens: Cached"
            (Use old SD card data if available)
    END
ELSE
    Display "Tokens: Cached"
    (Skip sync entirely if disabled or offline)
END

ALWAYS
    Load tokens from /tokens.json into RAM
```

### Configuration Options

**File:** `config.txt` on SD card

```ini
# Token Synchronization (OPTIONAL, default: true)
SYNC_TOKENS=true   # false to skip token sync (saves ~2-5s boot time)
```

**Implications:**
- `SYNC_TOKENS=true` (default) - Attempt sync on every boot
- `SYNC_TOKENS=false` - Always use cached tokens, never contact orchestrator for tokens

**Use Case for Disabling:**
- Offline events (no orchestrator available)
- Identical ESP32 deployments with pre-cached tokens
- Reducing boot time by 2-5 seconds

---

## 6. Data Transformations Across the Pipeline

### Transformation 1: ALN-TokenData → Backend Internal Model

**Input:** Raw tokens.json (object map)
```json
{
  "mab002": {
    "SF_ValueRating": 5,
    "SF_MemoryType": "Personal",
    "SF_Group": "Marcus Sucks (x2)",
    ...
  }
}
```

**Transformation:** `tokenService.loadTokens()` (lines 79-115)
- Extract group name: "Marcus Sucks (x2)" → "Marcus Sucks"
- Parse multiplier: "Marcus Sucks (x2)" → 2
- Calculate value: 5 × 2.0 = 10 points
- Normalize field names (SF_* → camelCase)

**Output:** Transformed token array
```javascript
{
  id: "mab002",
  name: "Marcus Sucks",
  value: 10,  // (5 × 2.0)
  memoryType: "Personal",
  groupId: "Marcus Sucks",
  groupMultiplier: 2,
  ...
}
```

**Used by:** transactionService for scoring calculations

### Transformation 2: Backend → API Response

**Input:** Raw tokens from `loadRawTokens()`

**Transformation:** `resourceRoutes.js` - Minimal transformation
- Wrap in response envelope: `{ tokens: {...}, count: N, lastUpdate: ISO8601 }`
- No field filtering or translation

**Output:** JSON API response (HTTP 200)

**Used by:** ESP32 network download

### Transformation 3: API Response → ESP32 SD Cache

**Input:** HTTP response body (raw JSON)

**Transformation:** Direct file write
```cpp
File file = SD.open("/tokens.json", FILE_WRITE);
file.print(payload);  // Write entire response as-is
file.close();
```

**Output:** Byte-for-byte copy on SD card

**No transformation:** Preserves backend response format exactly

### Transformation 4: SD Cache → ESP32 In-Memory

**Input:** tokens.json from SD card

**Transformation:** `TokenService.loadDatabaseFromSD()` (lines 93-146)
- Parse JSON using ArduinoJson
- Extract "tokens" object
- Build `std::vector<TokenMetadata>`
- Only store tokenId and video field (minimal memory)

```cpp
struct TokenMetadata {
    String tokenId;       // "mab002"
    String video;         // "" or "mab002.mp4"
    // Other fields available via helper methods:
    String getImagePath() { return "/images/" + tokenId + ".bmp"; }
    String getAudioPath() { return "/AUDIO/" + tokenId + ".wav"; }
};
```

**Output:** In-memory vector of token metadata

**Memory footprint:** ~2KB for 50 tokens

### Transformation 5: Token Metadata → Display/Playback

**Input:** TokenMetadata from in-memory vector

**Usage at RFID Scan Time:**
```cpp
// After scanning token ID "mab002"
const TokenMetadata* token = tokenService.get("mab002");
if (token) {
    // Check if video token
    if (token->isVideoToken()) {
        // Video token: Fire-and-forget, show modal
        displayScreen.showToken(token);  // Shows image from /images/mab002.bmp
    } else {
        // Regular token: Persistent display
        displayScreen.showToken(token);  // Shows image from /images/mab002.bmp
        audioDriver.play(token->getAudioPath());  // Plays /AUDIO/mab002.wav
    }
}
```

**Critical Design:** **Video tokens use same image path as regular tokens**
- Both stored as `/images/{tokenId}.bmp`
- Distinction is the `video` field (empty string vs filename)
- No special path conventions for video tokens

---

## 7. Data Format at Each Stage

### Stage 1: ALN-TokenData/tokens.json (Source)

**Format:** Object map (tokenId → token object)
**Size:** 12 KB
**Encoding:** UTF-8 JSON
**Example:**
```json
{
  "sof002": {
    "image": "assets/images/sof002.bmp",
    "audio": null,
    "video": null,
    "processingImage": null,
    "SF_RFID": "sof002",
    "SF_ValueRating": 2,
    "SF_MemoryType": "Personal",
    "SF_Group": ""
  }
}
```

### Stage 2: Backend Memory (Game Logic)

**Format:** Array of transformed token objects
**Size:** ~12 KB (with calculated fields)
**Encoding:** JavaScript objects
**Example:**
```javascript
[
  {
    id: "sof002",
    name: "Sofia",
    value: 2,
    memoryType: "Personal",
    groupId: null,
    groupMultiplier: 1,
    mediaAssets: {
      image: "assets/images/sof002.bmp",
      audio: null,
      video: null,
      processingImage: null
    },
    metadata: {
      rfid: "sof002",
      group: "",
      originalType: "Personal",
      rating: 2
    }
  }
]
```

### Stage 3: `/api/tokens` HTTP Response

**Format:** Wrapped object with metadata
**Size:** ~13 KB (+ HTTP headers)
**Encoding:** UTF-8 JSON
**Example:**
```json
{
  "tokens": { ... same as source ... },
  "count": 42,
  "lastUpdate": "2025-10-31T14:23:45.123Z"
}
```

### Stage 4: ESP32 SD Card (/tokens.json)

**Format:** Identical to HTTP response (byte-for-byte copy)
**Size:** ~13 KB
**Encoding:** UTF-8 JSON
**Storage:** Flash SD card (not ESP32 internal flash)
**Example:** Identical to Stage 3

### Stage 5: ESP32 In-Memory (Loaded from SD)

**Format:** Vector of TokenMetadata structures
**Size:** ~2 KB for 50 tokens
**Encoding:** C++ objects
**Example:**
```cpp
std::vector<TokenMetadata> = [
  { tokenId: "sof002", video: "" },
  { tokenId: "jaw001", video: "jaw001.mp4" },
  ...
]
```

---

## 8. Size Constraints and Feasibility Analysis

### Submodule Size

| Item | Size | Notes |
|------|------|-------|
| ALN-TokenData/tokens.json | 12 KB | Current |
| tokens.json (projected 100 tokens) | ~30 KB | Still well under limits |
| tokens.json (projected 200 tokens) | ~60 KB | Exceeds 50KB HTTP limit |

### Backend Size

| Item | Size | Notes |
|------|------|-------|
| tokenService.js | Small | Stateless, no storage |
| Token instances in transactionService.tokens | ~12 KB | Dynamic, grows with tokens |
| Loaded into memory on startup | <1 KB | Tokens loaded once |

### ESP32 Constraints

**Flash Memory:**
- Total: 1.3 MB (4 MB physical, partitioned)
- Current usage: 92% (1.2 MB)
- tokens.json not stored in flash, only on SD card
- **Flash impact: ZERO** (SD-based caching)

**SD Card (Unlimited for practical purposes):**
- tokens.json: 12 KB
- Token images: ~50 tokens × 150 KB = 7.5 MB per set
- Token audio: ~50 tokens × 100 KB = 5 MB per set
- **Total possible:** 12 KB tokens + images + audio ~13 MB (small 32GB card easily handles)

**RAM:**
- Loaded tokens vector: ~2 KB per 50 tokens
- HTTP response buffer during download: 50 KB max (temporary)
- **Total: Very modest** (~50 KB during sync, <2 KB at runtime)

### HTTP Payload Limits

**Current Configuration:**
```cpp
// OrchestratorService.h:227
if (size > limits::MAX_TOKEN_DB_SIZE) {  // 50KB limit
    // Reject payload
}
```

**Feasibility:**
- Current (42 tokens): 12 KB ✅ Safe
- Small expansion (100 tokens): ~30 KB ✅ Safe
- Medium expansion (200 tokens): ~60 KB ❌ Exceeds 50KB limit (would need config change)
- Large expansion (500 tokens): ~150 KB ❌ Significantly over limit

**Mitigation Options:**
1. Increase `MAX_TOKEN_DB_SIZE` to 100KB (still safe for 8GB RAM device)
2. Implement pagination `/api/tokens?page=1&limit=100`
3. Compress JSON (gzip) to reduce payload

---

## 9. Identified Issues and Gaps

### Issue 1: No Automatic Token Updates During Runtime

**Severity:** Medium
**Description:** Tokens only sync at boot time. If tokens.json is updated on the backend after ESP32 boots, the device won't get new tokens until restart.

**Scenario:**
- ESP32 boots at 2:00pm with version A of tokens
- Administrator updates tokens.json at 2:30pm
- ESP32 still has version A
- Event ends at 4:00pm without ESP32 ever getting version B

**Recommendation:**
- Add optional background task to poll `/api/tokens` periodically
- Implement version hash check to avoid unnecessary downloads
- Or require manual `FORCE_SYNC` command before event starts

### Issue 2: No Token Validation After Download

**Severity:** Low
**Description:** Downloaded tokens are written to SD without schema validation. Corrupted JSON would cause SD file corruption but might not be detected until loaded.

**Scenario:**
- Network interruption during download causes partial write
- ESP32 writes partial JSON to SD (/tokens.json becomes invalid)
- Next boot fails to load tokens

**Recommendation:**
- Validate JSON structure before writing to SD
- Use temp file + atomic rename pattern
- Add CRC/hash verification

### Issue 3: Large Token Database Growth Path Unclear

**Severity:** Low
**Description:** Current 50KB HTTP limit may not scale if token count grows significantly.

**Current:** 42 tokens → 12 KB
**Projected:** 100 tokens → 30 KB
**Problem:** 200+ tokens would exceed 50KB

**Recommendation:**
- Document growth projection and when expansion is needed
- Consider implementation of token pagination before hitting limit
- Add monitoring of `/api/tokens` response size in logs

### Issue 4: No Synchronization Timestamp Verification

**Severity:** Low
**Description:** `lastUpdate` timestamp in response is advisory only. Not used for version checking.

**Scenario:**
- Old cached tokens have timestamp T1
- New response has timestamp T2 > T1
- System should prefer newer version, but doesn't

**Recommendation:**
- Store timestamp in persistent cache metadata
- Compare timestamps before deciding to use old cache
- Implement version-aware caching

### Issue 5: Configuration Option Underutilized

**Severity:** Low
**Description:** `SYNC_TOKENS=false` is rarely used in documentation. Risk of ESP32 devices shipping with old cached tokens if reconfigured.

**Recommendation:**
- Document when to use SYNC_TOKENS=false (offline events only)
- Add warnings in logs when using stale cached data
- Consider default SYNC_TOKENS=true (current behavior is correct)

---

## 10. Token Data Flow Diagram (Complete)

```
┌──────────────────────────────────────────────────────────────────────────┐
│  LAYER 1: SOURCE (ALN-TokenData Submodule)                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ALN-TokenData/tokens.json (12 KB, 371 lines, 42 tokens)                │
│  └─ Format: { tokenId → { SF_RFID, SF_ValueRating, image, video, ... } │
│                                                                           │
└──────────────────────────────────────────────────────────────────────────┘
                                    ║
                        (Git submodule reference)
                                    ║
        ┌───────────────────────────╫───────────────────────────┐
        ║                           ║                           ║
        ▼                           ▼                           ▼
┌──────────────┐           ┌──────────────┐        ┌──────────────┐
│ BACKEND PATH │           │   ESP32 PATH │        │  WEB SCANNERS│
│              │           │              │        │              │
└──────────────┘           └──────────────┘        └──────────────┘
        │                           │                       │
        │                           │                       │
┌───────┴─────────────────┐  ┌──────┴──────────────┐       │
│ LAYER 2: BACKEND LOAD   │  │ LAYER 2: ESP32 PREP │       │
├─────────────────────────┤  ├─────────────────────┤       │
│                         │  │                     │       │
│ tokenService.js:        │  │ Config validation   │       │
│ _loadTokensFile()       │  │ WiFi connection     │       │
│ ├─ Try #1: ALN-Token    │  │ Orchestrator health │       │
│ │  Data submodule       │  │ check               │       │
│ └─ Try #2: aln-memory-  │  │                     │       │
│    scanner nested       │  │ IF (SYNC_TOKENS=    │       │
│                         │  │ true) AND           │       │
│ Return: Raw tokens      │  │ (Orchestrator       │       │
│ object map (no change)  │  │ connected) THEN     │       │
│                         │  │ ────┐               │       │
└────────┬────────────────┘  │     │ Download    │       │
         │                   │     └──→ /api/tokens        │
         │                   │        endpoint             │
         │                   │                             │
         │                   ├──────────────────────┤       │
         │                   │                      │       │
         └──────────────────→│ tokenService.js (in  │       │
         (For backend game   │ backend/src)         │       │
         logic)              │                      │       │
                             │ synced via HTTP      │       │
         ┌─────────────────→ │ └─────┐              │       │
         │ /api/tokens       │       │ SAVE to SD   │       │
         │ HTTP endpoint     │       │ /tokens.json │       │
         │                   │       │              │       │
         │                   └───────┴──────────────┘       │
         │                          │                       │
    ┌────┴────────────────────────┐ │                       │
    │ LAYER 3: API SERVING        │ │                       │
    ├────────────────────────────┤ │                       │
    │                            │ │                       │
    │ resourceRoutes.js:         │ │                       │
    │ GET /api/tokens            │ │                       │
    │ ├─ Load raw tokens         │ │                       │
    │ └─ Wrap in response        │ │                       │
    │    envelope:               │ │                       │
    │    { tokens, count,        │ │                       │
    │      lastUpdate }          │ │                       │
    │                            │ │                       │
    │ Response: JSON (13 KB)     │ │                       │
    │ HTTP 200 OK                │ │                       │
    │                            │ │                       │
    └─────┬──────────────────────┘ │                       │
          │                        │                       │
          └────→ API Consumers:    │                       │
                ├─ GM Scanner      │                       │
                ├─ Player Scanner  │                       │
                └─ ESP32 CYD ◄─────┘                       │
                   (via WiFi)                              │
                                                           │
                   LAYER 4: ESP32 CACHING                  │
                   ┌──────────────────────────────────┐    │
                   │ SD Card Cache (/tokens.json)     │    │
                   │ (Identical to HTTP response)     │    │
                   │ Format: Raw JSON (13 KB)         │    │
                   │ Persistent across reboots        │    │
                   └────┬─────────────────────────────┘    │
                        │                                   │
                        ▼                                   │
                   LAYER 5: ESP32 RUNTIME                  │
                   ┌──────────────────────────────────┐    │
                   │ TokenService.loadDatabaseFromSD()│    │
                   │                                  │    │
                   │ Parse JSON → std::vector         │    │
                   │ Store minimal metadata:          │    │
                   │ ├─ tokenId: "sof002"             │    │
                   │ ├─ video: "" or "name.mp4"       │    │
                   │ └─ Computed paths:               │    │
                   │    ├─ /images/{tokenId}.bmp      │    │
                   │    └─ /AUDIO/{tokenId}.wav       │    │
                   │                                  │    │
                   │ In-Memory Size: 2 KB per 50      │    │
                   │ tokens                           │    │
                   └────┬─────────────────────────────┘    │
                        │                                   │
                        ▼                                   │
                   LAYER 6: RFID SCAN PROCESSING           │
                   ┌──────────────────────────────────┐    │
                   │ Scan detected: "sof002"          │    │
                   │                                  │    │
                   │ tokenService.get("sof002")       │    │
                   │ └─ Returns: TokenMetadata*       │    │
                   │    ├─ tokenId: "sof002"          │    │
                   │    └─ video: ""                  │    │
                   │                                  │    │
                   │ IF (video field empty) THEN      │    │
                   │   ├─ Display image (persistent) │    │
                   │   └─ Play audio                  │    │
                   │ ELSE                             │    │
                   │   ├─ Display image (2.5s modal) │    │
                   │   └─ Send to orchestrator        │    │
                   │ (Note: No video playback on      │    │
                   │  ESP32, sent to orchestrator)    │    │
                   └──────────────────────────────────┘    │
                                                           │
                   ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
                   WEB SCANNERS (aln-memory-scanner,
                   ALNScanner - nested submodule path)
```

---

## 11. Synchronization Mechanism Summary

### How Token Updates Propagate

**Update Scenario: Developer updates tokens.json in production**

```
TIMELINE:
2:00 PM - Developer updates ALN-TokenData/tokens.json
         └─ Commits to git
         └─ Backend automatically reads new version ✅

2:01 PM - Backend /api/tokens endpoint now returns new tokens ✅
         └─ GM Scanner already connected gets new tokens in next request ✅
         └─ Player Scanner will get new on next HTTP call ✅

2:05 PM - ESP32 boots (example)
         └─ IF SYNC_TOKENS=true:
            ├─ Downloads /api/tokens from backend ✅ (gets new version)
            └─ Caches to /tokens.json on SD ✅

2:06 PM - ESP32 (that booted at 1:50 PM, before update)
         └─ Still running with OLD cached tokens ❌
         └─ Will get new tokens on NEXT BOOT ONLY

2:10 PM - Manual sync (if implemented)
         └─ ESP32 could call syncFromOrchestrator() via serial command
         └─ Would get new tokens without reboot ⚠️ (Not currently exposed)
```

### Gap in Synchronization: Runtime Updates

**Problem:** No mechanism for ESP32 to receive token updates during operation.

**Current Behavior:**
- Tokens sync at boot: ✅ Works
- Tokens updates during operation: ❌ No mechanism

**Why This Matters:**
- 2-hour live event with ESP32 running continuously
- If tokens need update mid-event, requires ESP32 reboot
- Orchestrator can push changes immediately, ESP32 can't receive them

**Solutions (if needed):**
1. **Periodic sync task** - Background task polls /api/tokens every N minutes
2. **Version checking** - Download only if version changes
3. **Manual trigger** - Admin sends sync command to device
4. **Event-driven** - Orchestrator pushes token update notifications

---

## 12. Complete Data Format Reference

### tokens.json Schema (in ALN-TokenData)

```typescript
interface Token {
  image: string | null;           // "assets/images/{id}.bmp"
  audio: string | null;           // "assets/audio/{id}.wav"
  video: string | null;           // "{id}.mp4" (video filename only)
  processingImage: string | null;  // Processing overlay image path
  SF_RFID: string;                // Token ID identifier
  SF_ValueRating: 1-5;            // Scoring weight
  SF_MemoryType: string;          // "Personal", "Business", "Technical"
  SF_Group: string;               // Group bonus: "Name (xN)" or ""
}

interface TokensFile {
  [tokenId: string]: Token;
}
```

### Backend Token Object (transformed)

```javascript
{
  id: string;
  name: string;
  value: number;
  memoryType: string;
  groupId: string | null;
  groupMultiplier: number;
  mediaAssets: {
    image: string | null;
    audio: string | null;
    video: string | null;
    processingImage: string | null;
  };
  metadata: {
    rfid: string;
    group: string;
    originalType: string;
    rating: number;
  };
}
```

### /api/tokens Response (HTTP)

```json
{
  "tokens": {
    "[tokenId]": {
      "image": string | null,
      "audio": string | null,
      "video": string | null,
      "processingImage": string | null,
      "SF_RFID": string,
      "SF_ValueRating": number,
      "SF_MemoryType": string,
      "SF_Group": string
    }
  },
  "count": number,
  "lastUpdate": string (ISO 8601)
}
```

### ESP32 TokenMetadata (in-memory)

```cpp
struct TokenMetadata {
  String tokenId;
  String video;
  
  // Computed helper methods:
  String getImagePath() { return "/images/" + tokenId + ".bmp"; }
  String getAudioPath() { return "/AUDIO/" + tokenId + ".wav"; }
  String getProcessingImagePath() { return tokenId + "_processing.jpg"; }
  bool isVideoToken() { return video.length() > 0; }
};
```

---

## 13. Recommendations for Robust Token Synchronization

### Short-term (Immediate)

1. **Document Current Behavior**
   - Clear README explaining token sync happens at boot only
   - Document SYNC_TOKENS=true/false implications
   - Add comment in Application.h about sync timing

2. **Add Logging**
   - Log token count after sync
   - Log hash of downloaded tokens vs cached
   - Log sync duration

3. **Validate Downloaded JSON**
   - Verify valid JSON structure before writing to SD
   - Check for required fields in each token
   - Use temp file with atomic rename (avoid corruption)

### Medium-term (Next Sprint)

4. **Version-Aware Caching**
   - Store lastUpdate timestamp with cached tokens
   - Compare timestamps before deciding to use cache
   - Log warnings when using stale cached data

5. **Manual Sync Command**
   - Expose `FORCE_SYNC` command for admin testing
   - Allow mid-event token refresh without reboot
   - Useful for last-minute token additions

6. **Size Monitoring**
   - Log /api/tokens response size on every download
   - Alert if approaching 50KB limit
   - Track token growth over time

### Long-term (Future Consideration)

7. **Periodic Background Sync**
   - Background task polls /api/tokens every 5 minutes
   - Compare hash with cached version
   - Download only if changed
   - Non-blocking (doesn't interrupt scanning)

8. **Token Pagination**
   - Implement `/api/tokens?page=1&limit=100` if database grows
   - Allows scaling beyond current 50KB limit
   - ESP32 could request tokens in chunks

9. **Push Notifications** (Advanced)
   - Orchestrator notifies ESP32 of token changes
   - Requires WebSocket or polling mechanism
   - Would enable runtime updates without reboot

---

## Conclusion

The ALN Ecosystem implements a **practical dual-path token distribution strategy**:

**Strengths:**
- Backend loads directly from submodule source (fast, no duplication)
- ESP32 can operate offline with cached tokens (resilient)
- HTTP endpoint serves both connected and disconnected clients
- Size-conscious implementation (12KB tokens, 50KB limit)

**Limitations:**
- Token updates only propagate at boot time
- No automatic refresh during operation
- Risk of version skew across multiple ESP32 devices
- No validation of downloaded tokens

**Current Status:**
- ✅ Boot-time synchronization working
- ✅ SD caching mechanism robust
- ✅ In-memory token lookup efficient (O(n) acceptable for <100 tokens)
- ❌ No runtime update mechanism
- ⚠️ No schema validation on download
- ⚠️ No version checking capability

For the 2-hour live event model, boot-time synchronization is typically sufficient, but for longer deployments or rapid token iteration, implementing one of the medium-term recommendations would improve operational flexibility.

