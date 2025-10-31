# Queue Corruption Defense Design

**Date:** 2025-01-31
**Status:** Approved
**Target:** ALNScanner v5.0 (OrchestratorService)

---

## Problem Statement

The ESP32 scanner's offline queue system has no defensive validation, leading to catastrophic corruption:

- **Observed:** 1.7GB queue file (should be <10KB for 100 entries)
- **Root Cause:** Power loss during write, no file size validation
- **Impact:** Queue size cached in RAM (`_queue.size`) diverges from actual file
- **Current State:** Cache initialized to 0, never counted from file

---

## Solution Overview

Add boot-time queue validation with corruption detection and manual recovery commands.

**Design Principles:**
- **YAGNI:** Simple file size check only (no JSON parsing)
- **Fast Boot:** Validation adds <100ms to startup
- **Nuclear Recovery:** Delete corrupt files immediately (cached scans are not critical)
- **Manual Override:** Serial commands for diagnosis and recovery

---

## Architecture

### 1. Boot-Time Validation (Explicit Initialization)

```
Application::setup()
  ↓
Application::initializeServices()
  ↓
orchestrator.initializeQueue()  ← NEW METHOD
  ↓
  ├─ SD file exists?
  │  ├─ No  → Set cache to 0, done
  │  └─ Yes → Continue
  ├─ File size > 100KB?
  │  ├─ Yes → DELETE file, set cache to 0, return false
  │  └─ No  → Continue
  ├─ Count lines (newlines)
  └─ Update cache with actual count
```

**Why explicit initialization:**
- Follows existing HAL pattern (Display.begin(), SD.begin())
- Clear call order in Application::setup()
- Runs after SD card ready, before WiFi connection
- Easy to trace and debug

---

## Implementation Details

### 1. Configuration Constants

**File:** `config.h` (line ~59)

```cpp
namespace queue_config {
    constexpr int MAX_QUEUE_SIZE = 100;
    constexpr int BATCH_UPLOAD_SIZE = 10;
    constexpr unsigned long MAX_QUEUE_FILE_SIZE = 102400;  // NEW: 100KB threshold
    constexpr const char* QUEUE_FILE = "/queue.jsonl";
    constexpr const char* QUEUE_TEMP_FILE = "/queue.tmp";
}
```

**Threshold Rationale:**
- Max 100 entries × ~1000 bytes/entry (generous) = 100KB
- Catches catastrophic corruption (1.7GB case) instantly
- Tolerates legitimate large queue entries (base64 data, etc.)

---

### 2. Queue Initialization Method

**File:** `services/OrchestratorService.h` (add to public section)

```cpp
/**
 * @brief Initialize queue size from disk file (call after SD ready)
 * @return true if queue valid, false if corrupted and deleted
 *
 * Validates queue file, counts entries, sets cached size.
 * If file size > 100KB, deletes as corrupted and resets to 0.
 *
 * MUST be called from Application::setup() after SD card initialized.
 */
bool initializeQueue();
```

**Implementation Flow:**
1. Acquire SD mutex (thread-safe)
2. Check if `/queue.jsonl` exists
   - Not found → Set cache to 0, return true
3. Open file, check `file.size()`
   - Size > 100KB → Delete file, set cache to 0, return false (corruption)
4. Count newlines (actual entries)
5. Update `_queue.size` with actual count
6. Log results, return true

**Logging:**
```
[ORCH-QUEUE-INIT] ═══ QUEUE INITIALIZATION START ═══
[ORCH-QUEUE-INIT] Queue file size: 12543 bytes
[ORCH-QUEUE-INIT] ✓ Queue validated: 87 entries
[ORCH-QUEUE-INIT] ═══ QUEUE INITIALIZATION END ═══
```

---

### 3. Application Integration

**File:** `Application.h` (line ~789, in `initializeServices()`)

**Call Order:**
```cpp
auto& orchestrator = services::OrchestratorService::getInstance();

// 1. Initialize queue (BEFORE WiFi)
orchestrator.initializeQueue();

// 2. Initialize WiFi
orchestrator.initializeWiFi(config.getConfig());

// 3. Continue with tokens, etc.
```

**Display Feedback:**
- Corruption detected → Orange text: "Queue: Reset"
- Valid queue > 0 entries → Orange text: "Queue: 23 pending"
- Empty queue → No message

---

### 4. Serial Commands

#### CLEAR_QUEUE

**Purpose:** Manual recovery when corruption persists or queue needs reset

**Usage:**
```
> CLEAR_QUEUE
⚠️  WARNING: This will delete ALL queued scans!
Type 'YES' to confirm, or anything else to cancel:
> YES
✓ Queue cleared (87 entries deleted)
```

**Safety:**
- Requires typed "YES" confirmation
- 10-second timeout (auto-cancel)
- Reuses existing `clearQueue()` method

---

#### QUEUE_STATUS

**Purpose:** Diagnostic tool to detect cache divergence and file corruption

**Usage:**
```
> QUEUE_STATUS
=== Queue Status (Detailed) ===
Cached size: 87 entries (from RAM)
File size: 12543 bytes
Actual lines: 87 entries (from file)
✓ Cache matches file

First entry: {"tokenId":"kaa001","teamId":"001","deviceId":"SCANNER_001",...}
Last entry: {"tokenId":"jaw002","teamId":"002","deviceId":"SCANNER_001",...}
=================================
```

**Features:**
- Shows cached vs actual line count
- Warns on cache divergence
- Flags files exceeding 100KB threshold
- Previews first/last entries (truncated to 80 chars)
- Uses SD mutex for thread safety

---

## Edge Cases Handled

| Scenario | Behavior |
|----------|----------|
| Queue file doesn't exist | Initialize to 0, continue normally |
| File size 1.7GB (corruption) | Delete file, reset to 0, log warning |
| File size 99KB (near threshold) | Validate normally, count lines |
| Cache 87, Actual 90 (divergence) | Update cache to 90, log discrepancy |
| SD mutex timeout during init | Log error, keep existing cache |
| Power loss during init | Next boot re-validates (idempotent) |
| CLEAR_QUEUE without confirmation | Cancel, no changes |
| QUEUE_STATUS on empty queue | Show "NOT FOUND" status |

---

## Performance Impact

**Boot Time:**
- File exists check: <1ms
- File size check: <1ms
- Line counting (100 entries): ~50-100ms
- **Total added to boot:** <100ms

**Runtime:**
- No ongoing validation (boot-only)
- Serial commands on-demand only
- No background task changes

**Memory:**
- No new heap allocations
- `MAX_QUEUE_FILE_SIZE` constant: 4 bytes
- No impact on existing `_queue` struct

---

## Testing Plan

### Unit Testing (Test Sketches)

**Test 1: Corruption Detection**
```
1. Create /queue.jsonl with 200KB of junk data
2. Boot device
3. Verify: File deleted, cache set to 0
4. Check serial log for "[ORCH-QUEUE-INIT] CORRUPTION DETECTED"
```

**Test 2: Valid Queue Initialization**
```
1. Create /queue.jsonl with 10 valid JSONL entries (~1KB)
2. Boot device
3. Verify: Cache set to 10
4. Send QUEUE_STATUS command
5. Verify: "Cached size: 10, Actual lines: 10, ✓ Cache matches file"
```

**Test 3: CLEAR_QUEUE Safety**
```
1. Create queue with 5 entries
2. Send CLEAR_QUEUE
3. Type "NO" at confirmation
4. Verify: Queue unchanged
5. Send CLEAR_QUEUE again
6. Type "YES"
7. Verify: Queue deleted, QUEUE_STATUS shows "NOT FOUND"
```

**Test 4: Cache Divergence Detection**
```
1. Boot with 10 entries (cache = 10)
2. Manually add 5 lines to SD card queue file via external tool
3. Send QUEUE_STATUS
4. Verify: "⚠️  WARNING: Cache divergence detected!"
5. Reboot
6. Verify: Cache updated to 15 after initializeQueue()
```

### Integration Testing

**Test 5: Boot After Power Loss**
```
1. Start device, queue 50 scans
2. Pull power during queue write (simulate corruption)
3. Reboot
4. Verify: Initialization detects corruption OR counts valid entries
5. Verify: Device continues normal operation
```

---

## Future Enhancements (Not Included)

**Explicitly NOT implementing (YAGNI):**
- ❌ Backup corrupt files before deletion (adds 1-2s boot time)
- ❌ JSON validation of each line (adds 1-2s boot time, high memory)
- ❌ Periodic background validation (adds overhead, not needed)
- ❌ REBUILD_QUEUE repair command (complex, rarely needed)
- ❌ Line count vs file size ratio check (marginal benefit)

**Rationale:**
Queue contains cached network requests (not critical data). Nuclear recovery (delete and start fresh) is acceptable. Boot-time validation catches power-loss corruption, which is the primary failure mode.

---

## Files Modified

1. **config.h** - Add `MAX_QUEUE_FILE_SIZE` constant
2. **services/OrchestratorService.h** - Add `initializeQueue()` method
3. **Application.h** - Call `initializeQueue()` in `initializeServices()`
4. **Application.h** - Add `CLEAR_QUEUE` and `QUEUE_STATUS` commands in `registerSerialCommands()`

**Estimated Changes:**
- Lines added: ~150
- Lines modified: ~10
- Flash impact: +2-3KB (mostly logging strings)

---

## Success Criteria

- ✅ Boot with 1.7GB corrupt file → Deleted, boots normally
- ✅ Boot with 100 valid entries → Cache matches actual count
- ✅ CLEAR_QUEUE requires "YES" confirmation
- ✅ QUEUE_STATUS shows cache divergence
- ✅ No impact on normal operation (offline queueing, batch upload)
- ✅ Boot time increase <100ms

---

## Implementation Order

1. Add `MAX_QUEUE_FILE_SIZE` to config.h
2. Implement `initializeQueue()` in OrchestratorService.h
3. Add call in Application::initializeServices()
4. Test boot validation with corrupt file
5. Implement CLEAR_QUEUE command
6. Implement QUEUE_STATUS command
7. Test all commands via serial monitor
8. Update CLAUDE.md with new commands

---

**Design approved:** 2025-01-31
**Ready for implementation:** Yes
