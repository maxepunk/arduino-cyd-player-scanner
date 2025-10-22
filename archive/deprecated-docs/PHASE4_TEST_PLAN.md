# Phase 4 Test Plan: User Story 2 - Scanning

**Feature**: 003-orchestrator-hardware-integration
**Date**: October 19, 2025
**Tester**: Claude (AI Assistant)
**Execution**: Serial monitor-based validation

---

## Test Prerequisites

### Hardware Setup
- ✅ ESP32 CYD device connected via /dev/ttyUSB0
- ✅ SD card with config.txt, tokens.json
- ✅ RFID test cards with known UIDs/NDEF data
- ✅ Orchestrator server accessible (for online tests)

### Software Setup
- ✅ Instrumented sketch compiled (v4.0)
- ✅ Serial monitor ready (115200 baud)
- ✅ Background processes killed (clear serial port)

---

## Test Matrix

| Test ID | Description | Orchestrator | Expected Outcome | Status |
|---------|-------------|--------------|------------------|--------|
| T092    | Online scan | ✅ Reachable | HTTP POST succeeds, scan sent | ⬜ Pending |
| T093    | Offline scan | ❌ Offline  | Scan queued to /queue.jsonl | ⬜ Pending |
| T094    | Content display timing | N/A | Local content displays <1s | ⬜ Pending |
| T095    | Network independence | Both | Content never blocked by network | ⬜ Pending |

---

## Test Execution Protocol

### General Procedure

1. **Upload sketch**: `arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .`
2. **Start serial monitor**: `arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200`
3. **Wait for boot**: Look for `═══ ORCHESTRATOR INTEGRATION START ═══`
4. **Execute test**: Scan RFID card at prompt
5. **Capture output**: Save serial log for analysis
6. **Verify metrics**: Check instrumentation markers

### Success Criteria Pattern

Look for these markers in serial output:
- `[SEND] ═══ HTTP POST /api/scan START ═══`
- `[SEND] ✓✓✓ SUCCESS ✓✓✓ HTTP 200 received`
- `[QUEUE] ═══ QUEUE SCAN START ═══`
- `[QUEUE] ✓✓✓ SUCCESS ✓✓✓ Scan queued`
- `[SCAN] Total scan processing latency: <LATENCY> ms`

---

## T092: Online Scan Test

**Objective**: Verify HTTP POST /api/scan succeeds when orchestrator reachable

### Prerequisites
1. Orchestrator server running at configured URL
2. Verify connectivity: `STATUS` command shows `CONNECTED`

### Test Steps
1. **USER ACTION**: Scan an RFID card
2. **OBSERVE**: Watch for `[SCAN] ═══ ORCHESTRATOR SCAN START ═══`
3. **OBSERVE**: Connection state should show `CONNECTED`
4. **OBSERVE**: Look for `[SEND] ═══ HTTP POST /api/scan START ═══`
5. **OBSERVE**: Check for `[SEND] ✓✓✓ SUCCESS ✓✓✓ HTTP 200 received`

### Expected Serial Output
```
[SCAN] ═══ ORCHESTRATOR SCAN START ═══
[SCAN] Free heap: 275XXX bytes
[SCAN] Token ID: <token_id>
[SCAN] Connection state: CONNECTED
[SCAN] Timestamp: 1970-01-01T...
[SCAN] Attempting to send to orchestrator...

[SEND] ═══ HTTP POST /api/scan START ═══
[SEND] Free heap: 275XXX bytes
[SEND] URL: http://10.0.0.100:3000/api/scan
[SEND] Payload: {"tokenId":"<token_id>","teamId":"001",...}
[SEND] Payload size: XX bytes
[SEND] Sending HTTP POST request...
[SEND] HTTP response code: 200
[SEND] Request latency: <LATENCY> ms
[SEND] ✓✓✓ SUCCESS ✓✓✓ HTTP 200 received
[SEND] Free heap after send: 275XXX bytes
[SEND] ═══ HTTP POST /api/scan END ═══

[SCAN] ✓✓✓ SUCCESS ✓✓✓ Sent to orchestrator
[SCAN] Queue size: 0 entries
[SCAN] Total scan processing latency: <LATENCY> ms
[SCAN] Free heap after scan: 275XXX bytes
[SCAN] ═══ ORCHESTRATOR SCAN END ═══
```

### Pass/Fail Criteria
- ✅ PASS: HTTP 200 response received, latency <5000ms, heap stable
- ❌ FAIL: HTTP error, timeout, or heap drop >10KB

### Metrics to Record
- HTTP response latency (ms)
- Total scan latency (ms)
- Free heap before/after
- Queue size (should be 0)

---

## T093: Offline Scan Test

**Objective**: Verify scan queuing when orchestrator unreachable

### Prerequisites
1. **USER ACTION**: Stop orchestrator server OR set wrong URL in config.txt
2. Verify connectivity: `STATUS` command shows `WIFI_CONNECTED` or `DISCONNECTED`
3. Check current queue size: `STATUS` command

### Test Steps
1. **USER ACTION**: Scan an RFID card
2. **OBSERVE**: Watch for `[SCAN] ═══ ORCHESTRATOR SCAN START ═══`
3. **OBSERVE**: Connection state should show `DISCONNECTED` or `WIFI_CONNECTED`
4. **OBSERVE**: Look for `[SCAN] Offline/disconnected, queueing immediately`
5. **OBSERVE**: Check for `[QUEUE] ═══ QUEUE SCAN START ═══`
6. **OBSERVE**: Verify `[QUEUE] ✓✓✓ SUCCESS ✓✓✓ Scan queued`

### Expected Serial Output
```
[SCAN] ═══ ORCHESTRATOR SCAN START ═══
[SCAN] Free heap: 275XXX bytes
[SCAN] Token ID: <token_id>
[SCAN] Connection state: DISCONNECTED (or WIFI_CONNECTED)
[SCAN] Timestamp: 1970-01-01T...
[SCAN] Offline/disconnected, queueing immediately

[QUEUE] ═══ QUEUE SCAN START ═══
[QUEUE] Free heap: 275XXX bytes
[QUEUE] Entry: {"tokenId":"<token_id>","teamId":"001",...}
[QUEUE] Entry size: XX bytes
[QUEUE] Opening /queue.jsonl for append...
[QUEUE] File opened, current size: XXX bytes
[QUEUE] ✓✓✓ SUCCESS ✓✓✓ Scan queued (token: <token_id>)
[QUEUE] Queue size: N entries
[QUEUE] Write latency: <LATENCY> ms
[QUEUE] Free heap after queue: 275XXX bytes
[QUEUE] ═══ QUEUE SCAN END ═══

[SCAN] Queue size: N entries
[SCAN] Total scan processing latency: <LATENCY> ms
[SCAN] Free heap after scan: 275XXX bytes
[SCAN] ═══ ORCHESTRATOR SCAN END ═══
```

### Pass/Fail Criteria
- ✅ PASS: Queue entry written, latency <500ms, queue size incremented, heap stable
- ❌ FAIL: File write error, timeout, or heap drop >5KB

### Metrics to Record
- Queue write latency (ms)
- Total scan latency (ms)
- Queue size before/after (should increment by 1)
- Free heap before/after

### Post-Test Verification
**USER ACTION**: Remove SD card, check `/queue.jsonl` file:
- File exists
- Contains valid JSONL entries (one per line)
- Last entry matches scanned token

---

## T094: Content Display Timing Test

**Objective**: Verify local content displays within 1 second of scan

### Prerequisites
1. SD card contains `/images/<token_id>.bmp` and `/audio/<token_id>.wav`
2. Test with both online and offline modes

### Test Steps (Online Mode)
1. **USER ACTION**: Scan card (orchestrator ONLINE)
2. **OBSERVE**: Note timestamp of `[SCAN] ═══ ORCHESTRATOR SCAN START ═══`
3. **OBSERVE**: Note timestamp of `[BMP] Display complete`
4. **MEASURE**: Calculate `displayTime = bmpComplete - scanStart`
5. **VERIFY**: `displayTime < 1000ms`

### Test Steps (Offline Mode)
1. **USER ACTION**: Scan card (orchestrator OFFLINE)
2. **OBSERVE**: Same timing measurements as online mode
3. **VERIFY**: Offline timing ≈ online timing (±100ms acceptable)

### Expected Serial Output
```
[SCAN] ═══ ORCHESTRATOR SCAN START ═══  <-- T1
...
[SCAN] ═══ ORCHESTRATOR SCAN END ═══
...
[BMP] Display complete                   <-- T2
```

**Target**: T2 - T1 < 1000ms

### Pass/Fail Criteria
- ✅ PASS: Content displays <1000ms after scan start, both modes similar
- ❌ FAIL: Display >1000ms or significant offline penalty (>200ms difference)

### Metrics to Record
- Online display latency (ms)
- Offline display latency (ms)
- Latency delta (online vs offline)

---

## T095: Network Independence Test

**Objective**: Verify local content NEVER blocked by network operations

### Test Scenario 1: Slow Orchestrator
**Setup**: Configure orchestrator with artificial 3-second delay

1. **USER ACTION**: Scan card
2. **OBSERVE**: `[SEND]` section shows 3000+ms latency
3. **OBSERVE**: Content displays BEFORE HTTP response received
4. **VERIFY**: Content visible while HTTP request in progress

### Test Scenario 2: Timeout
**Setup**: Configure unreachable orchestrator (5s timeout)

1. **USER ACTION**: Scan card
2. **OBSERVE**: `[SEND]` section shows timeout error after 5000ms
3. **OBSERVE**: Content displays IMMEDIATELY (not after timeout)
4. **VERIFY**: Content visible while timeout occurs

### Expected Behavior
Content display is NOT blocked by:
- HTTP request latency
- HTTP timeout (5000ms)
- Queue write operations

### Pass/Fail Criteria
- ✅ PASS: Content displays <1s regardless of network state
- ❌ FAIL: Content blocked waiting for HTTP or queue operations

### Metrics to Record
- Content display time (ms)
- HTTP operation time (ms)
- Verify: contentDisplayTime < httpOperationTime

---

## Test Execution Log Template

```
═══════════════════════════════════════════════════════════════
TEST: T092 - Online Scan Test
DATE: 2025-10-19
TIME: HH:MM:SS
═══════════════════════════════════════════════════════════════

PRE-TEST STATE:
- Orchestrator: [REACHABLE/UNREACHABLE]
- Queue size: [N] entries
- Free heap: [XXXXX] bytes

TEST EXECUTION:
[Paste complete serial output from scan event]

METRICS OBSERVED:
- HTTP latency: [XXX] ms
- Scan latency: [XXX] ms
- Heap before: [XXXXX] bytes
- Heap after: [XXXXX] bytes
- Heap delta: [±XXX] bytes
- Queue size after: [N] entries

PASS/FAIL: [✅ PASS / ❌ FAIL]
NOTES: [Any observations, anomalies, or issues]

═══════════════════════════════════════════════════════════════
```

---

## Post-Test Verification Checklist

After completing all tests, verify:

- [ ] All test logs captured
- [ ] Metrics recorded in table above
- [ ] Pass/fail status marked
- [ ] `/queue.jsonl` inspected (if offline tests run)
- [ ] Heap stability confirmed (no leaks)
- [ ] No unexpected resets or crashes
- [ ] Serial output clean (no ERROR messages)

---

## Next Steps After Testing

1. Mark todos as complete
2. Update `specs/003-orchestrator-hardware-integration/tasks.md`:
   - Mark T091-T095 as `[X]` complete
   - Add test results summary
3. Commit changes with test results
4. Proceed to Phase 5 (User Story 3 - Background Sync)

---

**Test Plan Version**: 1.0
**Created**: 2025-10-19
**Status**: Ready for Execution
