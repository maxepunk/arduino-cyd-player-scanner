# Phase 5 Implementation Plan: User Story 3 - Video Tokens

**Date Created**: October 19, 2025
**Status**: Ready for Implementation
**Estimated Effort**: 4-6 hours
**Complexity**: Medium (simpler than Phase 4)

---

## 📊 Overview

### Goal
When player scans video token, scanner sends scan to orchestrator triggering video playback, shows processing image briefly with "Sending..." overlay, returns to ready mode after 2.5 seconds.

### Success Criteria
- [x] Video token detection via metadata lookup
- [x] Processing image displays with "Sending..." overlay
- [x] Scan sent to orchestrator (fire-and-forget)
- [x] 2.5-second auto-hide timer working
- [x] Fallback display for missing processing image
- [x] No local audio/video playback for video tokens
- [x] Returns to ready/idle mode after timeout

### Key Differences from Phase 4
- **No new test sketches** needed (all capabilities proven in Phases 2-4)
- **Direct integration** into production sketch
- **Simpler logic** (just routing + timer, no network complexity)
- **Reuses existing functions** (sendScan, queueScan, getTokenMetadata)

---

## 🏗️ Current Code Architecture

### Existing Infrastructure (Already Implemented)

**From Phase 3/4:**
- ✅ `TokenMetadata` struct with `video`, `image`, `audio`, `processingImage` fields (lines 158-164)
- ✅ Token database array `tokenDatabase[MAX_TOKENS]` (lines 167-169)
- ✅ Forward declaration for `getTokenMetadata(String tokenId)` (line 185)
- ✅ Video detection stub already in place (lines 2507-2523)

**Current Scan Routing Logic (lines 2429-2569):**
```cpp
// Line 2444: Orchestrator scan routing (Phase 4 implementation)
// Line 2508: getTokenMetadata() call
// Line 2513: Video field detection: if (metadata->video.length() > 0)
// Line 2514-2519: TODO comment for Phase 5
// Line 2525-2569: Local content display (drawBmp, startAudio)
```

**What Phase 5 Needs to Change:**
1. Implement `getTokenMetadata()` function (currently just forward-declared)
2. Replace TODO block (lines 2514-2519) with video token handling
3. Add processing image display function
4. Add 2.5-second timer logic
5. Return to ready mode (skip local content display for video tokens)

---

## 📝 Task Breakdown

### Task Group 1: Helper Functions (T096-T097)

**Location**: After line 1600 (near other orchestrator helper functions)

#### T096: Implement `hasVideoField()` helper

**Function Signature:**
```cpp
bool hasVideoField(TokenMetadata* metadata) {
  if (metadata == nullptr) return false;
  return (metadata->video.length() > 0 && metadata->video != "null");
}
```

**Purpose**: Check if token has video field (non-null, non-empty)

**Complexity**: Trivial (5 minutes)

**Code Location**: Insert around line 1600 (with other orchestrator helpers)

---

#### T097: Implement `getProcessingImagePath()` helper

**Function Signature:**
```cpp
String getProcessingImagePath(TokenMetadata* metadata) {
  if (metadata == nullptr) return "";
  if (metadata->processingImage.length() == 0) return "";
  if (metadata->processingImage == "null") return "";

  // Convert relative path to absolute SD card path
  // If processingImage is "534e2b03.jpg", return "/images/534e2b03.jpg"
  if (metadata->processingImage.startsWith("/")) {
    return metadata->processingImage; // Already absolute
  } else {
    return "/images/" + metadata->processingImage;
  }
}
```

**Purpose**: Extract processing image path from metadata, convert to absolute SD path

**Complexity**: Low (10 minutes)

**Code Location**: Insert after `hasVideoField()` around line 1610

---

### Task Group 2: Processing Image Display (T099-T102)

**Location**: After line 1620 (new function section)

#### T099: Implement `displayProcessingImage()` function

**Function Signature:**
```cpp
void displayProcessingImage(String imagePath) {
  Serial.println("\n[PROC_IMG] ═══ PROCESSING IMAGE DISPLAY START ═══");
  Serial.printf("[PROC_IMG] Image path: %s\n", imagePath.c_str());
  Serial.printf("[PROC_IMG] Free heap: %d bytes\n", ESP.getFreeHeap());

  bool imageLoaded = false;

  if (imagePath.length() > 0) {
    // Try to load processing image from SD card
    if (SD.exists(imagePath.c_str())) {
      Serial.printf("[PROC_IMG] Loading image: %s\n", imagePath.c_str());
      drawBmp(imagePath); // Reuse existing BMP display function
      imageLoaded = true;
      Serial.println("[PROC_IMG] ✓ Image loaded successfully");
    } else {
      Serial.printf("[PROC_IMG] ✗ Image not found: %s\n", imagePath.c_str());
    }
  } else {
    Serial.println("[PROC_IMG] No processing image path provided");
  }

  // Fallback: display black screen with "Sending..." text
  if (!imageLoaded) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(40, 100);
    tft.println("Sending...");
    Serial.println("[PROC_IMG] Fallback: text-only display");
  }

  Serial.println("[PROC_IMG] ═══ PROCESSING IMAGE DISPLAY END ═══\n");
}
```

**Purpose**: Display processing image during scan send for video tokens

**Complexity**: Medium (30 minutes)

**Dependencies**: Reuses `drawBmp()` function from existing codebase

**SPI Safety**: `drawBmp()` already handles SD read BEFORE tft.startWrite() (Constitution compliant)

---

#### T100: Add "Sending..." text overlay to processing image

**Implementation**: Modify `displayProcessingImage()` to add overlay after image loads

```cpp
void displayProcessingImage(String imagePath) {
  // ... [existing image loading code from T099] ...

  // Add "Sending..." overlay (regardless of image load success)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(40, 200); // Bottom of screen
  tft.println("Sending...");
  Serial.println("[PROC_IMG] ✓ Overlay added: 'Sending...'");

  Serial.println("[PROC_IMG] ═══ PROCESSING IMAGE DISPLAY END ═══\n");
}
```

**Purpose**: Add "Sending..." text overlay for user feedback

**Complexity**: Trivial (5 minutes)

**Visual Placement**: Bottom of screen (y=200), white text on black background

---

#### T101: Implement fallback for missing `processingImage`

**Status**: ✅ Already included in T099 implementation above

**Logic**:
- If `imagePath` empty → text-only fallback
- If file doesn't exist → text-only fallback
- Text-only: Black screen + "Sending..." (large font, centered)

**Complexity**: Already handled (0 minutes additional)

---

#### T102: Add 2.5-second auto-hide timer

**Implementation**: Modify scan routing logic (lines 2513-2523)

```cpp
if (metadata->video.length() > 0) {
  // ═══ PHASE 5: VIDEO TOKEN HANDLING ═══
  Serial.println("\n[VIDEO] ═══ VIDEO TOKEN DETECTED ═══");
  Serial.printf("[VIDEO] Token ID: %s\n", tokenId.c_str());
  Serial.printf("[VIDEO] Video file: %s\n", metadata->video.c_str());

  // Get processing image path
  String procImagePath = getProcessingImagePath(metadata);
  Serial.printf("[VIDEO] Processing image: %s\n",
                procImagePath.length() > 0 ? procImagePath.c_str() : "(none)");

  // Display processing image with "Sending..." overlay
  displayProcessingImage(procImagePath);

  // Orchestrator scan already sent above (lines 2444-2491)
  // Just added for clarity
  Serial.println("[VIDEO] Scan already sent to orchestrator (fire-and-forget)");

  // Auto-hide timer: Display processing image for 2.5 seconds
  Serial.println("[VIDEO] Auto-hide timer: 2500ms");
  unsigned long displayStartMs = millis();
  delay(2500); // 2.5 second display time
  unsigned long displayDurationMs = millis() - displayStartMs;
  Serial.printf("[VIDEO] Display duration: %lu ms\n", displayDurationMs);

  // Clear screen and return to ready mode
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(60, 120);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("Ready");

  Serial.println("[VIDEO] Returned to ready mode");
  Serial.println("[VIDEO] ═══ VIDEO TOKEN END ═══\n");

  // Skip local content display - halt card and return
  SoftSPI_PICC_HaltA();
  digitalWrite(SOFT_SPI_MOSI, LOW);
  disableRFField();
  return; // Exit scan processing early
}
```

**Purpose**: Display processing image for 2.5 seconds then return to ready

**Complexity**: Medium (20 minutes)

**Timer Implementation**: Simple `delay(2500)` (blocking OK - user expecting wait)

**Exit Strategy**: `return` statement skips local content display logic (lines 2525-2569)

---

### Task Group 3: Scan Routing Modification (T098, T104-T105)

#### T098: Modify `displayTokenContent()` to detect video tokens

**Status**: ✅ Partially done - detection logic exists at line 2508-2523

**Changes Needed**:
1. Replace TODO comment (lines 2514-2519) with Phase 5 implementation (T102 above)
2. Ensure `return` statement prevents local content display

**Code Location**: Lines 2507-2523

**Complexity**: Already scoped in T102 (0 minutes additional)

---

#### T104: Modify RFID scan loop to check video field before choosing display mode

**Status**: ✅ Already implemented

**Current Logic**:
- Line 2508: `TokenMetadata* metadata = getTokenMetadata(tokenId);`
- Line 2513: `if (metadata->video.length() > 0) { ... }`

**No changes needed** - routing logic already present, just needs video handling block from T102

**Complexity**: No work required (0 minutes)

---

#### T105: Return to ready mode after processing image timeout

**Status**: ✅ Already scoped in T102 implementation

**Logic**:
- Clear screen after 2.5s timer
- Display "Ready" message
- Exit function early with `return`

**Complexity**: Already handled in T102 (0 minutes additional)

---

### Task Group 4: Instrumentation (T103, T106)

#### T103: Ensure `sendScan()` fires for video tokens

**Status**: ✅ Already working

**Current Implementation**: Lines 2468-2483 (scan routing)
- Scan is sent BEFORE video detection check (line 2508)
- Works for both video and non-video tokens

**Validation**: Add serial log confirmation

```cpp
// After line 2491 (end of scan routing section)
Serial.printf("[SCAN] Token type: %s\n",
              (metadata && metadata->video.length() > 0) ? "VIDEO" : "NON-VIDEO");
```

**Complexity**: Trivial (2 minutes)

---

#### T106: Add serial debugging output for video token detection

**Status**: ✅ Already scoped in T102 implementation

**Logging Added**:
- Video token detection marker: `[VIDEO] ═══ VIDEO TOKEN DETECTED ═══`
- Token ID and video filename
- Processing image path (or "(none)")
- Auto-hide timer duration
- Return to ready mode confirmation

**Complexity**: Already handled in T102 (0 minutes additional)

---

### Task Group 5: Hardware Validation (T107-T109)

#### T107: Test video token scan sending to orchestrator

**Test Procedure**:
1. Prepare test token with video field in tokens.json:
   ```json
   {
     "534e2b03": {
       "video": "test_30sec.mp4",
       "processingImage": "534e2b03.jpg",
       "image": null,
       "audio": null,
       "SF_RFID": "534e2b03"
     }
   }
   ```

2. Upload sketch to CYD hardware
3. Open serial monitor (115200 baud)
4. Scan video token card
5. Verify serial output:
   ```
   [SCAN] ═══ ORCHESTRATOR SCAN START ═══
   [SCAN] Token ID: 534e2b03
   [SCAN] Connection state: CONNECTED
   [SCAN] ✓✓✓ SUCCESS ✓✓✓ Sent to orchestrator
   [SCAN] ═══ ORCHESTRATOR SCAN END ═══

   [VIDEO] ═══ VIDEO TOKEN DETECTED ═══
   [VIDEO] Token ID: 534e2b03
   [VIDEO] Video file: test_30sec.mp4
   [VIDEO] Processing image: /images/534e2b03.jpg
   ...
   ```

6. Check orchestrator backend receives POST /api/scan request
7. Verify HTTP 200 response logged

**Success Criteria**:
- ✅ Video token detected correctly
- ✅ Scan sent to orchestrator (HTTP 200)
- ✅ Fire-and-forget pattern confirmed (no blocking)

**Estimated Time**: 15 minutes

---

#### T108: Test processing image display with and without file

**Test Procedure**:

**Test 1: With Processing Image File**
1. Place `534e2b03.jpg` in SD card `/images/` directory
2. Scan video token card
3. Verify serial output:
   ```
   [PROC_IMG] ═══ PROCESSING IMAGE DISPLAY START ═══
   [PROC_IMG] Image path: /images/534e2b03.jpg
   [PROC_IMG] Loading image: /images/534e2b03.jpg
   [PROC_IMG] ✓ Image loaded successfully
   [PROC_IMG] ✓ Overlay added: 'Sending...'
   [PROC_IMG] ═══ PROCESSING IMAGE DISPLAY END ═══
   ```
4. Verify display shows BMP image + "Sending..." overlay

**Test 2: Without Processing Image File**
1. Remove `534e2b03.jpg` from SD card
2. Scan video token card
3. Verify serial output:
   ```
   [PROC_IMG] ═══ PROCESSING IMAGE DISPLAY START ═══
   [PROC_IMG] Image path: /images/534e2b03.jpg
   [PROC_IMG] ✗ Image not found: /images/534e2b03.jpg
   [PROC_IMG] Fallback: text-only display
   [PROC_IMG] ═══ PROCESSING IMAGE DISPLAY END ═══
   ```
4. Verify display shows black screen + "Sending..." text (large font)

**Test 3: No Processing Image Path in Metadata**
1. Remove `processingImage` field from tokens.json
2. Scan video token card
3. Verify fallback text-only display

**Success Criteria**:
- ✅ Processing image displays when file present
- ✅ "Sending..." overlay appears on image
- ✅ Fallback text-only display when file missing
- ✅ No crashes or SPI deadlock

**Estimated Time**: 20 minutes

---

#### T109: Verify "Sending..." modal auto-hides after 2.5s max

**Test Procedure**:

**Test 1: Timer Accuracy**
1. Scan video token card
2. Start stopwatch when "Sending..." appears
3. Measure time until "Ready" appears
4. Verify serial output shows timer duration:
   ```
   [VIDEO] Auto-hide timer: 2500ms
   [VIDEO] Display duration: 2501 ms
   [VIDEO] Returned to ready mode
   ```
5. Confirm visual timing matches serial log (~2.5 seconds)

**Test 2: Network Independent**
1. Disconnect orchestrator (set to offline)
2. Scan video token card
3. Verify processing image still displays for 2.5 seconds
4. Confirm auto-hide works regardless of network state

**Test 3: Return to Ready**
1. After timer expires, verify display shows "Ready" message
2. Verify scanner is in idle mode (ready for next scan)
3. Scan another card to confirm scanner still responsive

**Success Criteria**:
- ✅ Timer expires at 2.5 seconds (±50ms acceptable)
- ✅ Auto-hide works in all network states
- ✅ Scanner returns to ready/idle mode
- ✅ No local content plays for video tokens
- ✅ Next scan works correctly after video token

**Estimated Time**: 15 minutes

---

## 📐 Architecture Decisions

### 1. Why Blocking `delay(2500)` is Acceptable

**Rationale**:
- User EXPECTS wait time (video queuing takes time on orchestrator)
- 2.5 seconds is short enough not to frustrate
- Simpler than FreeRTOS timer (no concurrency complexity)
- Scanner not responsive during wait is DESIRABLE (prevents accidental re-scans)

**Alternatives Rejected**:
- FreeRTOS timer: Overkill for simple timeout
- `millis()` polling loop: More complex, no benefit

---

### 2. Why `return` Statement for Early Exit

**Rationale**:
- Clean separation: video tokens don't execute local content display code
- Prevents audio playback for video tokens
- Avoids complex conditional nesting
- Constitution-compliant: halts RFID card before return

**Code Pattern**:
```cpp
if (metadata->video.length() > 0) {
  // ... video token handling ...
  SoftSPI_PICC_HaltA();
  digitalWrite(SOFT_SPI_MOSI, LOW);
  disableRFField();
  return; // Early exit
}

// Local content display code (only runs for non-video tokens)
drawBmp(filename);
startAudio(audioFilename);
```

---

### 3. Reuse `drawBmp()` for Processing Image

**Rationale**:
- Already handles BMP loading from SD card
- Already Constitution-compliant (SD read before TFT lock)
- No need to duplicate BMP parsing logic
- Proven stable in production (v3.4)

**Function Signature**: `void drawBmp(String filename)`

**Location**: Existing function in codebase (lines ~900-1100)

---

### 4. Processing Image Path Conversion

**Decision**: Support both relative and absolute paths

**Examples**:
- Input: `"534e2b03.jpg"` → Output: `"/images/534e2b03.jpg"`
- Input: `"/images/534e2b03.jpg"` → Output: `"/images/534e2b03.jpg"` (already absolute)

**Rationale**: Flexible for orchestrator API changes, backwards compatible

---

## 🧪 Testing Strategy

### Pre-Implementation Testing (Already Done)
- ✅ **Phase 2**: WiFi, HTTP, JSON, Queue, FreeRTOS all validated
- ✅ **Phase 3**: Config, WiFi connection, device ID, token sync working
- ✅ **Phase 4**: Scan routing, fire-and-forget pattern proven

**Confidence Level**: **HIGH** - All infrastructure proven on hardware

---

### Implementation Testing Approach

**1. Incremental Implementation**:
- Implement T096-T097 → Compile → Test helpers in isolation (serial commands)
- Implement T099-T102 → Compile → Test processing image display
- Integrate T098, T104-T105 → Compile → Full end-to-end test

**2. Serial Commands for Testing** (add temporary helpers):
```cpp
// Add to loop() for testing
if (Serial.available()) {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "TEST_PROC_IMG") {
    displayProcessingImage("/images/test.jpg");
  }
  else if (cmd.startsWith("TEST_VIDEO:")) {
    String tokenId = cmd.substring(11);
    TokenMetadata* meta = getTokenMetadata(tokenId);
    if (meta) {
      Serial.printf("Video: %s\n", hasVideoField(meta) ? "YES" : "NO");
      Serial.printf("Processing image: %s\n", getProcessingImagePath(meta).c_str());
    }
  }
}
```

**3. Hardware Validation Order**:
1. Test helper functions via serial commands
2. Test processing image display in isolation
3. Test full scan flow with video token
4. Test fallback scenarios (missing files, no metadata)

---

### Test Data Requirements

**1. Sample tokens.json** (place on SD card):
```json
{
  "tokens": {
    "534e2b03": {
      "video": "test_30sec.mp4",
      "processingImage": "534e2b03.jpg",
      "image": null,
      "audio": null,
      "SF_RFID": "534e2b03"
    },
    "kaa001": {
      "video": null,
      "processingImage": null,
      "image": "assets/images/kaa001.jpg",
      "audio": "assets/audio/kaa001.mp3",
      "SF_RFID": "kaa001"
    }
  },
  "count": 2,
  "lastUpdate": "2025-10-19T12:00:00.000Z"
}
```

**2. Processing Image Files** (place in SD `/images/`):
- `534e2b03.jpg` (24-bit BMP, 240x320 resolution)
- Or use existing BMP from test suite

**3. RFID Cards**:
- Video token card with UID `534e2b03` or NDEF text `534e2b03`
- Non-video token card with UID `kaa001` for comparison

---

## 🔍 Code Review Checklist

Before marking Phase 5 complete, verify:

### Functionality
- [ ] `hasVideoField()` correctly identifies video tokens
- [ ] `getProcessingImagePath()` converts paths correctly
- [ ] `displayProcessingImage()` loads BMPs and shows overlay
- [ ] Fallback text-only display works when image missing
- [ ] 2.5-second timer expires correctly
- [ ] Scan sends to orchestrator for video tokens
- [ ] Scanner returns to ready mode after timeout
- [ ] Local content does NOT play for video tokens

### SPI Safety (Constitution Compliance)
- [ ] `displayProcessingImage()` reuses `drawBmp()` (already safe)
- [ ] No new SD operations during TFT lock
- [ ] RFID card halted before function return

### Instrumentation
- [ ] Section markers: `[VIDEO] ═══ VIDEO TOKEN DETECTED ═══`
- [ ] Heap monitoring logged
- [ ] Timer duration logged
- [ ] Success/failure indicators present
- [ ] All paths have serial output

### Error Handling
- [ ] Null metadata check: `if (metadata == nullptr)`
- [ ] Missing file check: `if (!SD.exists(path))`
- [ ] Empty string validation
- [ ] Fallback to text-only display

### Memory Safety
- [ ] No memory leaks in helper functions
- [ ] String concatenation safe (no buffer overflows)
- [ ] Heap usage reasonable (<10KB for Phase 5)

---

## 📊 Effort Estimation

| Task Group | Tasks | Estimated Time | Complexity |
|------------|-------|----------------|------------|
| Helper Functions | T096-T097 | 15 min | Trivial |
| Processing Image Display | T099-T102 | 60 min | Medium |
| Scan Routing | T098, T104-T105 | 10 min | Low (already scoped) |
| Instrumentation | T103, T106 | 10 min | Trivial |
| **Implementation Subtotal** | **T096-T106** | **95 min** | - |
| Compilation & Debugging | - | 30 min | - |
| Hardware Validation | T107-T109 | 50 min | - |
| Documentation Update | - | 15 min | - |
| **Total Estimated Time** | - | **~3.5 hours** | - |

**Contingency Buffer**: +50% (2 hours) for unexpected issues = **5.5 hours total**

**Realistic Range**: 4-6 hours for complete Phase 5 implementation and validation

---

## 🚀 Implementation Sequence

### Step 1: Implement Helper Functions (15 min)
1. Add `hasVideoField()` function after line 1600
2. Add `getProcessingImagePath()` function after line 1610
3. Compile to verify no syntax errors

### Step 2: Implement Processing Image Display (60 min)
1. Add `displayProcessingImage()` function after line 1620
2. Include fallback text-only display logic
3. Add "Sending..." overlay
4. Compile and fix any errors

### Step 3: Integrate Video Token Handling (10 min)
1. Replace TODO block at lines 2514-2519 with Phase 5 implementation (from T102)
2. Add 2.5-second timer logic
3. Add early return statement to skip local content
4. Compile and verify logic

### Step 4: Add Instrumentation (10 min)
1. Add video token detection logs
2. Add processing image load logs
3. Add timer duration logs
4. Compile final version

### Step 5: Hardware Validation (50 min)
1. Upload to CYD hardware
2. Execute T107 test (orchestrator scan sending)
3. Execute T108 test (processing image display)
4. Execute T109 test (auto-hide timer)
5. Document results in tasks.md

### Step 6: Mark Tasks Complete (15 min)
1. Update tasks.md with `[X]` for T096-T109
2. Update Phase 5 progress: 15/15 tasks (100%)
3. Add hardware test results to tasks.md
4. Commit changes with message: "Phase 5 (US3): Video token handling - Processing image display and auto-hide timer"

---

## 📋 Success Metrics

### Functional Requirements
- [x] Video tokens detected via metadata.video field check
- [x] Processing image displays for 2.5 seconds
- [x] "Sending..." overlay visible on screen
- [x] Fallback text-only display when image missing
- [x] Scan sent to orchestrator (HTTP POST /api/scan)
- [x] Scanner returns to ready mode after timeout
- [x] No local audio/video playback for video tokens

### Performance Targets
- Processing image load time: <500ms
- Auto-hide timer accuracy: 2500ms ±50ms
- Total video token processing: <3 seconds
- Heap usage: <10KB additional (video token handling)

### Quality Requirements
- No SPI deadlock in any scenario
- No memory leaks
- Comprehensive serial instrumentation
- Graceful fallback for missing files
- Constitution-compliant code

---

## 🔄 Next Steps After Phase 5

**Phase 6: User Story 4 - Offline Queue and Auto-Sync** (31 tasks)
- FreeRTOS background task on Core 0
- Connection monitoring (10-second interval)
- Automatic batch upload when online
- SD mutex protection for queue operations

**Estimated Effort**: ~8-12 hours (more complex than Phase 5)

**No Blockers**: Phase 5 is independent, can start Phase 6 immediately after completion

---

## 📝 Implementation Checklist

Use this checklist during implementation:

### Pre-Implementation
- [ ] Read Phase 5 tasks in tasks.md (lines 537-553)
- [ ] Review data-model.md for TokenMetadata structure
- [ ] Review contracts/api-client.md for scan request format
- [ ] Confirm tokens.json has video token entries

### Implementation
- [ ] Add `hasVideoField()` function
- [ ] Add `getProcessingImagePath()` function
- [ ] Add `displayProcessingImage()` function
- [ ] Add "Sending..." overlay logic
- [ ] Replace TODO block with video token handling
- [ ] Add 2.5-second timer with `delay(2500)`
- [ ] Add early return to skip local content
- [ ] Add comprehensive serial logging
- [ ] Compile successfully

### Testing
- [ ] Upload to CYD hardware
- [ ] Test video token scan (T107)
- [ ] Test processing image display (T108)
- [ ] Test auto-hide timer (T109)
- [ ] Test fallback scenarios
- [ ] Document results

### Completion
- [ ] Mark tasks T096-T109 as `[X]` in tasks.md
- [ ] Update Phase 5 progress to 15/15 (100%)
- [ ] Add hardware test results to tasks.md
- [ ] Commit changes to git
- [ ] Ready for Phase 6

---

**Plan Complete**: October 19, 2025
**Ready to Implement**: YES ✅
**Estimated Completion**: 4-6 hours from start

