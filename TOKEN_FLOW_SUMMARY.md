# ESP32 Token Data Flow - Executive Summary

**Document:** ESP32_TOKEN_DATA_FLOW_ANALYSIS.md (40 KB, comprehensive tracing)
**Analysis Date:** October 31, 2025
**Thoroughly Traced:** Yes - All 6 stages from source to runtime

---

## Complete Data Flow (6 Stages)

```
SOURCE (ALN-TokenData)
    ↓
    → tokens.json: 12 KB (371 lines, 42 tokens)
    → Object map: {tokenId → {image, audio, video, SF_RFID, SF_ValueRating, ...}}

BACKEND LOAD
    ↓
    → tokenService.js loads from ALN-TokenData submodule
    → Two functions:
      1. loadRawTokens() - Unmodified for API (used by /api/tokens endpoint)
      2. loadTokens() - Transformed for game logic (used by transactionService)

API SERVING  
    ↓
    → GET /api/tokens endpoint (resourceRoutes.js)
    → Returns: {tokens: {...}, count: 42, lastUpdate: ISO8601}
    → HTTP Response: 13 KB (raw JSON + metadata envelope)
    → Available to: Backend, GM Scanner, Player Scanner, ESP32

ESP32 BOOT (Synchronization)
    ↓
    → IF config.txt: SYNC_TOKENS=true AND orchestrator connected:
      1. Download /api/tokens via HTTP GET
      2. Validate size < 50KB (MAX_TOKEN_DB_SIZE)
      3. Save to SD card (/tokens.json) - Byte-for-byte copy
    → ELSE: Use cached version from previous boot

ESP32 LOAD
    ↓
    → TokenService.loadDatabaseFromSD()
    → Parse JSON: {"tokens": {...}}
    → Build std::vector<TokenMetadata>
    → In-memory: tokenId + video field only
    → Memory: ~2 KB for 50 tokens

ESP32 RUNTIME
    ↓
    → RFID scan → tokenService.get(tokenId) → TokenMetadata
    → Display image: /images/{tokenId}.bmp
    → Play audio: /AUDIO/{tokenId}.wav
    → Check if video token: if (token.video != "") { ... }
```

---

## Critical Findings

### Finding 1: Token Source of Truth
- **Location:** `ALN-TokenData/tokens.json` (root of submodule)
- **Size:** 12 KB
- **Format:** JSON object map (tokenId → token object)
- **Submodule Access:** Backend loads directly via relative path (3 levels up)
- **Why Two Submodule Paths:** Fallback for aln-memory-scanner (web scanner) nested copy

### Finding 2: No Automatic Runtime Updates
- **Sync Timing:** Boot time only
- **Trigger:** config.txt `SYNC_TOKENS=true` (default)
- **Behavior:** Download at boot, cache to SD, then static until next reboot
- **Gap:** If tokens updated during 2-hour event, ESP32 won't get them until restart
- **Recommendation:** Add periodic sync task or manual refresh command if needed

### Finding 3: Size Constraints Are Feasible
| Metric | Current | Limit | Status |
|--------|---------|-------|--------|
| tokens.json | 12 KB | 50 KB (HTTP) | ✅ Safe |
| Token count | 42 | 200+ | ⚠️ ~30KB at 100, exceeds 50KB at 200 |
| In-memory (runtime) | 2 KB | Unlimited | ✅ Trivial |
| Flash impact | 0 KB | 92% (1.2MB) | ✅ Not in flash (SD cached) |

### Finding 4: Video Tokens Use Same Paths as Regular Tokens
- **Image Path:** `/images/{tokenId}.bmp` (identical for all tokens)
- **Distinction:** `video` field in metadata (empty string vs filename)
- **Implication:** Same BMP file serves both video and non-video tokens
- **No special paths:** No "video_images" folder or different naming convention

### Finding 5: Token Data Doesn't Stay in ESP32 Flash
- **SD Card Caching:** 13 KB of tokens stored on SD (/tokens.json)
- **Flash Impact:** Zero bytes in ESP32 flash memory
- **Why Matters:** Allows tokens.json to grow beyond flash constraints
- **Flash Status:** 92% used (1.2 MB/1.3 MB), no room for token growth in flash

---

## Data Transformations

### Transform 1: Raw → Searchable (Backend Only)
- Extract group name: "Marcus Sucks (x2)" → "Marcus Sucks"
- Parse multiplier: "(x2)" → 2
- Calculate value: rating × multiplier
- Normalize field names: SF_MemoryType → memoryType
- **Used by:** transactionService for scoring

### Transform 2: Backend → API (Minimal)
- Wrap in envelope: `{ tokens: {...}, count: N, lastUpdate: T }`
- No field filtering or translation
- **Used by:** /api/tokens HTTP endpoint

### Transform 3: API → SD Cache (None)
- Direct byte-for-byte file write
- Preserves backend response format exactly
- **Used by:** SD persistent storage

### Transform 4: SD → Memory (Minimal)
- Parse JSON → extract "tokens" object
- Store only: tokenId + video field
- Discard: Other fields computed at display time
- **Used by:** ESP32 runtime lookup

---

## Synchronization Mechanisms

### When Tokens Sync
1. **Boot Time (Primary)**
   - Condition: config.txt `SYNC_TOKENS=true` AND orchestrator reachable
   - Frequency: Once per boot/restart
   - Action: Download `/api/tokens`, save to `/tokens.json`

2. **Fallback (If Sync Fails)**
   - Uses cached tokens from previous boot
   - Displays "Tokens: Cached" on screen

3. **Manual (Future - Not Implemented)**
   - Potential: `FORCE_SYNC` serial command (not exposed)
   - Could enable mid-event refresh without reboot

### What Triggers Updates
| Trigger | Status | Frequency |
|---------|--------|-----------|
| Boot | ✅ Implemented | Once per boot |
| Runtime polling | ❌ Not implemented | N/A |
| Push notification | ❌ Not implemented | N/A |
| Manual command | ⚠️ Code exists, not exposed | N/A |

### Why No Runtime Updates
- Requires polling `/api/tokens` in background task
- Need version/hash checking to avoid constant downloads
- Not critical for 2-hour event (boot once, run once)
- Would use extra WiFi bandwidth and power

---

## Token Paths Across the System

### In ALN-TokenData/tokens.json
```json
{
  "sof002": {
    "image": "assets/images/sof002.bmp",      // Source path convention
    "audio": "assets/audio/sof002.wav",
    "video": null
  }
}
```

### On ESP32 SD Card
```
/tokens.json                    (API response, byte-for-byte)
/images/sof002.bmp             (Token display image)
/AUDIO/sof002.wav              (Token audio)
```

### In Backend Game Logic
```javascript
token.mediaAssets.image         // Derived from tokenId
token.mediaAssets.video         // From SF_ValueRating calculation
```

### Video Files (Backend Only)
```
backend/public/videos/jaw001.mp4           (Video playback on orchestrator)
backend/public/videos/loopimages/          (18 MB idle loop)
```

**Note:** ESP32 doesn't handle video playback. It sends scan to orchestrator, which queues video on backend.

---

## Identified Issues & Gaps

| Issue | Severity | Status |
|-------|----------|--------|
| No runtime token updates | Medium | ❌ Not implemented |
| No validation on download | Low | ❌ Could add |
| No version timestamp check | Low | ❌ Could add |
| 50KB limit growth path unclear | Low | ⚠️ Documented here |
| Configuration underutilized | Low | ⚠️ Documented here |

---

## File References (Absolute Paths)

**Source:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/ALN-TokenData/tokens.json`

**Backend Loading:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/backend/src/services/tokenService.js` (Lines 49-115)

**Backend API Serving:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/backend/src/routes/resourceRoutes.js` (Lines 16-31)

**ESP32 Download & Cache:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/arduino-cyd-player-scanner/ALNScanner_v5/services/OrchestratorService.h` (Lines 203-268)

**ESP32 Loading:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/arduino-cyd-player-scanner/ALNScanner_v5/services/TokenService.h` (Lines 93-146)

**ESP32 Initialization Flow:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/arduino-cyd-player-scanner/ALNScanner_v5/Application.h` (Lines 818-844)

**API Contract:**
- `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/backend/contracts/openapi.yaml` (Lines 151-230)

---

## Recommendations (Priority Order)

### Immediate (Documentation)
1. Document that token sync is boot-time only
2. Add comments in Application.h explaining sync flow
3. Update sample_config.txt with SYNC_TOKENS explanation

### Short-term (Robustness)
4. Add validation of downloaded JSON before writing to SD
5. Use temp file + atomic rename (avoid corruption)
6. Add logging of token count and hash after sync
7. Implement version-aware caching (store lastUpdate with cache)

### Medium-term (Functionality)  
8. Expose manual `FORCE_SYNC` command for testing
9. Add periodic background token polling (optional, every 5 min)
10. Monitor `/api/tokens` response size in logs

### Long-term (Scaling)
11. If tokens exceed 100, implement `/api/tokens?page=1&limit=100`
12. Consider gzip compression if scaling further
13. Implement push notifications for token updates

---

## Conclusion

The token data flow is **well-designed for the 2-hour live event model**:

**Strengths:**
- ✅ Backend loads directly from submodule (no duplication)
- ✅ API endpoint available to all clients
- ✅ ESP32 has offline caching (works without orchestrator)
- ✅ Size-conscious implementation (12KB tokens, 50KB limit)
- ✅ Boot-time sync sufficient for single-session events

**Limitations:**
- ❌ No runtime updates (requires reboot to get new tokens)
- ⚠️ No schema validation on download
- ⚠️ No version checking capability

**For typical About Last Night operation:** Current implementation is solid. Boot devices once, run for 2 hours, tokens static throughout event. If real-time token iteration is needed during event, recommend manual `FORCE_SYNC` command (infrastructure exists, just not exposed).

---

**Full Analysis:** See ESP32_TOKEN_DATA_FLOW_ANALYSIS.md (40 KB)
**Last Updated:** October 31, 2025
