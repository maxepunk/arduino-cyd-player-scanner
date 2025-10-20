# Test Sketch 5: Background Sync (FreeRTOS) - Design Document

**Date**: October 19, 2025
**Status**: Design Phase - NOT YET IMPLEMENTED
**Critical Risk**: SPI Deadlock (Constitution Principle II)

---

## 1. Current Production Scanner Architecture

### ALNScanner0812Working.ino v3.4 (PRODUCTION - DO NOT MODIFY YET)

**Core Components:**
- **RFID Scanning**: Software SPI on GPIO22/27/35 (500ms interval)
- **TFT Display**: VSPI hardware SPI (ST7789, 240x320)
- **SD Card**: VSPI hardware SPI (FAT32, shared with TFT)
- **Audio**: I2S output (non-blocking playback)
- **Touch**: IRQ-based tap detection (GPIO36)

**Critical SPI Pattern (PROVEN SAFE):**
```cpp
// Line 904-946 in production sketch
for (each BMP row) {
  f.read(rowBuffer, rowBytes);  // Step 1: SD read
  tft.startWrite();              // Step 2: Lock TFT
  tft.setAddrWindow(0, y, w, 1); // Step 3: Set window
  tft.pushColor(pixel);          // Step 4: Push pixels
  tft.endWrite();                // Step 5: Release TFT
  yield();                       // Step 6: Allow task switching
}
```

**Why This Works:**
- SD operations complete BEFORE `tft.startWrite()`
- NO SD access between `tft.startWrite()` and `tft.endWrite()`
- `yield()` allows FreeRTOS task switching
- VSPI bus safely shared between TFT and SD

**Scan Loop Timing:**
- RFID scan interval: 500ms (prevents beeping from GPIO27/speaker coupling)
- Audio playback: non-blocking (checked in loop())
- Touch detection: interrupt-driven

---

## 2. What Background Sync Adds

### NEW Orchestrator Components (NOT IN PRODUCTION YET)

**Core 0 Background Task:**
- WiFi connection management
- HTTP client (orchestrator API)
- Queue file operations (SD `/queue.jsonl`)
- Token database sync (SD `/tokens.json`)
- Connection monitoring (every 10s health check)

**Core 1 Main Task (EXISTING + Extensions):**
- **EXISTING**: RFID scan, BMP display, audio playback, touch handling
- **NEW**: Queue/send decision, token metadata lookup, connection state check

**NEW SD Card Access Points:**
- Core 0: Read/write `/queue.jsonl`, read `/tokens.json` (orchestrator sync)
- Core 1: Read `/images/*.bmp`, `/audio/*.wav` (EXISTING), check token metadata (NEW from RAM)

---

## 3. The SPI Deadlock Risk

### Scenario: Both Tasks Access SD Simultaneously

**BAD Example (DEADLOCK):**
```cpp
// Core 1 (Main Task)
tft.startWrite();              // Lock TFT on VSPI
xSemaphoreTake(sdMutex);       // Try to lock SD - BLOCKS
f = SD.open("/images/foo.bmp"); // Never reaches here
f.read(buffer);
xSemaphoreGive(sdMutex);
tft.endWrite();

// Core 0 (Background Task) - HOLDS sdMutex
xSemaphoreTake(sdMutex);       // Has SD lock
f = SD.open("/queue.jsonl");
f.read(line);                  // Reading queue
// Meanwhile Core 1 waiting for sdMutex while holding TFT lock
// DEADLOCK: Core 0 can't use VSPI (TFT locked), Core 1 can't get SD lock
```

**Root Cause:**
- VSPI bus shared by TFT and SD
- If Core 1 locks TFT then tries to lock SD, but Core 0 holds SD lock
- Core 0 can't complete SD operation (needs VSPI, but TFT has it)
- Core 1 can't release TFT (waiting for SD lock)
- **DEADLOCK**

---

## 4. Safe Architecture Design

### Rule #1: SD Read BEFORE TFT Lock

**SAFE Pattern (Like Production Sketch):**
```cpp
// Core 1 (Main Task)
xSemaphoreTake(sdMutex, portMAX_DELAY);
f = SD.open("/images/foo.bmp");
f.read(buffer, size);  // Read ENTIRE row into RAM
f.close();
xSemaphoreGive(sdMutex);  // Release SD BEFORE TFT lock

// NOW render to TFT (no SD access)
tft.startWrite();
tft.pushPixels(buffer);
tft.endWrite();
```

### Rule #2: No Nested Locks

**SAFE: Acquire one lock at a time**
```cpp
xSemaphoreTake(sdMutex);
// Do SD operations
xSemaphoreGive(sdMutex);

// Later...
tft.startWrite();
// Do TFT operations
tft.endWrite();
```

**UNSAFE: Never hold TFT while waiting for SD**
```cpp
tft.startWrite();
xSemaphoreTake(sdMutex); // DEADLOCK RISK
```

### Rule #3: Background Task Respects SD Mutex

**SAFE: Background task waits for SD**
```cpp
// Core 0 (Background Task)
if (xSemaphoreTake(sdMutex, 100 / portTICK_PERIOD_MS)) {
  // Got SD lock within 100ms timeout
  f = SD.open("/queue.jsonl");
  while (f.available() && count < 10) {
    String line = f.readStringUntil('\n');
    entries.push_back(parse(line));
  }
  f.close();
  xSemaphoreGive(sdMutex);
} else {
  // Couldn't get lock, main task using SD
  // Skip this sync cycle, try again in 10s
}
```

---

## 5. Test Sketch 5 Design

### Objectives

**Primary Goal:** Prove dual-core task separation prevents SPI deadlocks

**Test Scenarios:**
1. Normal operation: Main scans, background syncs queue
2. Stress test: Rapid scanning while large queue syncs
3. Deadlock test: Force simultaneous SD access attempts
4. Recovery test: WiFi disconnect/reconnect handling
5. Mutex contention: Measure SD lock wait times

### Architecture

```
Core 1 (Main Task - arduino loop())
├── Audio playback processing (non-blocking)
├── Touch interrupt handling (double-tap)
└── RFID scanning loop (500ms interval)
    ├── Detect card (Software SPI)
    ├── Read NDEF text
    ├── Lookup token metadata (RAM - no SD)
    ├── Decision: queue or send
    │   ├── If orchestrator connected → send scan
    │   ├── If send fails → queue scan (SD mutex)
    │   └── If offline → queue scan (SD mutex)
    └── Display content
        ├── Video token → show processing image (SD mutex for BMP)
        └── Local token → show image/play audio (SD mutex)

Core 0 (Background Task - xTaskCreatePinnedToCore)
├── Connection monitoring (every 10s)
│   ├── HTTP GET /health (no SD access)
│   └── Update connection state (shared variable)
├── Queue synchronization (when connected)
│   ├── Check queue size (RAM cache or quick SD read)
│   ├── Read batch (SD mutex - up to 10 entries)
│   ├── HTTP POST /api/scan/batch
│   ├── Remove uploaded entries (SD mutex)
│   └── Repeat if queue not empty (1s delay)
└── Token database sync (on boot, if connected)
    ├── HTTP GET /api/tokens
    └── Write to SD /tokens.json (SD mutex)
```

### Shared Resources

**SD Card Mutex:**
```cpp
SemaphoreHandle_t sdMutex = NULL;

void setup() {
  sdMutex = xSemaphoreCreateMutex();
}

// All SD operations
xSemaphoreTake(sdMutex, portMAX_DELAY);
// SD.open(), read(), write(), close()
xSemaphoreGive(sdMutex);
```

**Connection State (Atomic Updates):**
```cpp
enum ConnectionState {
  DISCONNECTED,
  WIFI_CONNECTED,
  CONNECTED  // WiFi + orchestrator reachable
};

volatile ConnectionState connState = DISCONNECTED;
portMUX_TYPE connStateMux = portMUX_INITIALIZER_UNLOCKED;

void setConnectionState(ConnectionState newState) {
  portENTER_CRITICAL(&connStateMux);
  connState = newState;
  portEXIT_CRITICAL(&connStateMux);
}

ConnectionState getConnectionState() {
  portENTER_CRITICAL(&connStateMux);
  ConnectionState state = connState;
  portEXIT_CRITICAL(&connStateMux);
  return state;
}
```

**Queue Size Cache (RAM):**
```cpp
volatile int queueSizeCached = 0;
portMUX_TYPE queueSizeMux = portMUX_INITIALIZER_UNLOCKED;

void updateQueueSize(int delta) {
  portENTER_CRITICAL(&queueSizeMux);
  queueSizeCached += delta;
  portEXIT_CRITICAL(&queueSizeMux);
}
```

### Test Commands

**Interactive Serial Commands:**
- `SCAN` - Trigger manual RFID scan simulation
- `STRESS` - Rapid scanning (10 scans in 2 seconds)
- `QUEUE` - Add 20 entries to queue manually
- `CONNECT` - Simulate orchestrator connection
- `DISCONNECT` - Simulate orchestrator disconnect
- `SYNC` - Force queue synchronization
- `STATUS` - Show connection state, queue size, mutex stats
- `STATS` - Show scan count, sync count, mutex contention
- `DEADLOCK` - Force deadlock scenario (if possible)

### Success Criteria

✅ **No SPI deadlocks** during 100 rapid scans + background sync
✅ **SD mutex prevents concurrent access** (measured via contention count)
✅ **Main task continues scanning** without blocking during sync
✅ **Background task syncs queue** without blocking main task
✅ **Connection state updates** correctly (WiFi, orchestrator)
✅ **Queue syncs when orchestrator reachable** (batch upload works)
✅ **Mutex wait times** < 500ms (main task not blocked too long)
✅ **Free heap stable** (no memory leaks after 100 operations)

### Instrumentation

**Metrics to Track:**
```cpp
struct TestMetrics {
  // Main task
  uint32_t totalScans = 0;
  uint32_t scansQueued = 0;
  uint32_t scansSent = 0;

  // Background task
  uint32_t healthChecks = 0;
  uint32_t batchUploads = 0;
  uint32_t uploadSuccesses = 0;
  uint32_t uploadFailures = 0;

  // SD mutex
  uint32_t sdLockAttempts = 0;
  uint32_t sdLockWaits = 0;  // Had to wait for lock
  uint32_t sdLockTimeouts = 0;  // Wait timed out
  unsigned long maxWaitTime = 0;  // Longest wait in ms

  // Deadlock detection
  uint32_t deadlockEvents = 0;
};
```

---

## 6. Integration Considerations

### After Test Sketch 5 Validation

**What Gets Integrated into ALNScanner0812Working.ino:**

1. **FreeRTOS Background Task** (Core 0)
   - Connection monitoring function
   - Queue synchronization function
   - Task creation in `setup()`

2. **SD Mutex Protection** (Both Cores)
   - Wrap all existing SD operations with mutex
   - Maintain SD-before-TFT sequencing

3. **Connection State Management** (Both Cores)
   - Global connection state variable
   - Atomic update functions

4. **Queue Operations** (Both Cores)
   - Main task: queue or send decision
   - Background task: batch upload

5. **Token Metadata Lookup** (Core 1)
   - Load token database into RAM on boot
   - Lookup function (no SD access during scan)

**What MUST NOT Change:**
- Existing RFID scan loop timing (500ms)
- BMP display row-by-row pattern
- Audio playback non-blocking pattern
- Touch interrupt handling

### Integration Sequence

1. Add SD mutex to ALL existing SD operations (BMP, audio)
2. Test production sketch still works (no deadlocks)
3. Add token database loader (boot time, one-time SD read)
4. Add connection state management
5. Add queue decision logic to scan handler
6. Add background task (initially disabled)
7. Enable background task, test with WiFi/orchestrator offline
8. Enable connection monitoring
9. Enable queue sync
10. Final validation: 100 scans with background sync active

---

## 7. Risk Mitigation

### Deadlock Prevention Checklist

- [ ] ALL SD operations use `xSemaphoreTake(sdMutex, timeout)`
- [ ] NO `tft.startWrite()` before SD operations
- [ ] SD mutex timeout set (never wait forever)
- [ ] Background task yields CPU regularly (`delay(100)`)
- [ ] Main task yields CPU after each row (`yield()`)
- [ ] Watchdog timer monitoring for hang detection (optional)

### Rollback Plan

**If deadlocks occur during integration:**
1. Disable background task (comment out `xTaskCreate`)
2. Remove SD mutex from existing code
3. Scanner returns to v3.4 behavior (no orchestrator)
4. Debug test sketch 5 further

---

## 8. Next Steps

**Before Implementation:**
1. Review this design with user for approval ✅ **REQUIRED**
2. Clarify any architecture questions
3. Confirm test scenarios cover all edge cases

**Implementation Order:**
1. Create test sketch 5 skeleton
2. Implement SD mutex wrapper functions
3. Implement shared state (connection, queue size)
4. Implement mock RFID scan trigger
5. Implement background task (minimal)
6. Add stress test scenarios
7. Add instrumentation and metrics
8. Upload to hardware and validate
9. Document results in tasks.md

---

**Design Complete**: October 19, 2025
**Status**: Awaiting User Approval Before Implementation
**Next**: User review → Implementation → Hardware Validation
