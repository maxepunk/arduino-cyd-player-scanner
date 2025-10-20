# Test 45: WiFi EMI Touch IRQ Filtering - Testing Guide

## Overview

Test-45 validates the consecutive IRQ filtering algorithm designed to eliminate WiFi EMI-induced spurious touch interrupts on GPIO36.

## Root Cause (from Test-44)

| Test | WiFi | Audio | IRQ Rate | Finding |
|------|------|-------|----------|---------|
| A    | OFF  | OFF   | 0.00/sec | ✓ Baseline - GPIO36 stable without WiFi |
| B    | ON   | OFF   | 3.50/sec | ✗ **WiFi EMI causes 350x increase in spurious IRQs** |
| C    | OFF  | ON    | 0.00/sec | ✓ Audio has no effect on IRQs |
| D    | ON   | ON    | 3.50/sec | ✗ Audio doesn't shield from WiFi EMI |

**Hardware Constraint:** GPIO36 is input-only, cannot add pull-down resistor

**Solution:** Software EMI filtering - require 3 consecutive IRQs within 20ms to validate real touch

---

## Test Architecture

### Phase 1: Autonomous Baseline Test (60 seconds)

**Purpose:** Measure WiFi EMI spurious IRQ rate with filter active

**Expectation:** Zero valid touches detected (all WiFi EMI filtered out)

**User Action:** **DO NOT TOUCH SCREEN** - test runs automatically

**TFT Display:**
- Shows countdown timer
- Live statistics (raw IRQs, filtered IRQs, valid touches)

**Serial Output:**
- Statistics every 10 seconds
- Final report at completion

### Phase 2: Interactive Tap Test

**Purpose:** Validate real touch detection with EMI filter active

**Expectation:** 5/5 real touches detected reliably

**User Action:** **TAP SCREEN 5 TIMES** when prompted

**TFT Display:**
- "TAP SCREEN 5 TIMES" instruction
- Touch counter (0/5, 1/5, etc.)
- Visual feedback (green flash on each valid touch)
- Timeout countdown

**Serial Output:**
- Each valid touch logged with timestamp
- Final success/failure report

---

## Upload and Run Instructions

### 1. Upload Sketch to CYD

```bash
cd ~/projects/Arduino/test-sketches/45-wifi-emi-filter

# Upload to CYD
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .
```

### 2. Open Serial Monitor

```bash
# Open serial monitor (115200 baud)
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

You should see initialization output:
```
╔════════════════════════════════════════════════════════╗
║     Test 45: WiFi EMI Touch IRQ Filtering             ║
╠════════════════════════════════════════════════════════╣
║ PURPOSE: Validate consecutive IRQ filtering to        ║
║          eliminate WiFi EMI spurious interrupts       ║
║                                                        ║
║ ALGORITHM:                                             ║
║   - Require 3 IRQs within 20ms to validate touch      ║
║   - WiFi EMI: Isolated single IRQs (~286ms apart)     ║
║   - Real touch: Burst of IRQs (millisecond clusters)  ║
╚════════════════════════════════════════════════════════╝

[INIT] Initializing TFT display...
[INIT] Loading WiFi configuration...
[CONFIG] Reading /config.txt...
[CONFIG] WIFI_SSID: YourNetwork
[CONFIG] WIFI_PASSWORD: ********
[CONFIG] ✓ Configuration loaded successfully
[INIT] Connecting to WiFi (EMI source)...
[WIFI] Connecting to: YourNetwork
.....
[WIFI] ✓ Connected! IP: 10.0.0.137
[WIFI]   Signal: -63 dBm
[INIT] Initializing touch controller...
[INIT] Attaching touch IRQ handler...
[INIT] ✓ Initialization complete

╔════════════════════════════════════════════════════════╗
║          READY FOR TESTING                            ║
╠════════════════════════════════════════════════════════╣
║ Type HELP for available commands                      ║
║ Type BASELINE to start autonomous EMI test            ║
║ Type TAPTEST to start interactive tap test            ║
╚════════════════════════════════════════════════════════╝
```

---

## Running Tests

### Option A: Autonomous Workflow (Recommended)

The tests are designed to run sequentially:

#### Step 1: Run Baseline Test

Type in serial monitor:
```
BASELINE
```

**What happens:**
- 60-second autonomous test begins
- CYD screen shows countdown and live statistics
- Serial monitor prints statistics every 10 seconds
- **DO NOT TOUCH THE SCREEN during this test**

**Expected Output:**
```
╔════════════════════════════════════════════════════════╗
║        PHASE 1: BASELINE TEST (60 SECONDS)            ║
╠════════════════════════════════════════════════════════╣
║ PURPOSE: Measure WiFi EMI spurious IRQ rate           ║
║ EXPECTATION: Zero valid touches detected              ║
║              (All WiFi EMI filtered out)               ║
║                                                        ║
║ >>> DO NOT TOUCH THE SCREEN <<<                       ║
║                                                        ║
║ Countdown will display on screen...                   ║
╚════════════════════════════════════════════════════════╝

[10 sec] Raw IRQs: 35 | Filtered: 35 | Valid: 0
[20 sec] Raw IRQs: 70 | Filtered: 70 | Valid: 0
[30 sec] Raw IRQs: 105 | Filtered: 105 | Valid: 0
[40 sec] Raw IRQs: 140 | Filtered: 140 | Valid: 0
[50 sec] Raw IRQs: 175 | Filtered: 175 | Valid: 0

╔════════════════════════════════════════════════════════╗
║        BASELINE TEST COMPLETE                         ║
╚════════════════════════════════════════════════════════╝

╔════════════════════════════════════════════════════════╗
║            EMI FILTER STATISTICS                      ║
╠════════════════════════════════════════════════════════╣
║ Uptime:           60 seconds
╟────────────────────────────────────────────────────────╢
║ Raw IRQs:         210  (3.50/sec)
║ Filtered (EMI):   210  (3.50/sec)
║ Valid Touches:    0    (0.00/sec)
╟────────────────────────────────────────────────────────╢
║ Filter Effectiveness: 100.0% EMI rejected
╟────────────────────────────────────────────────────────╢
║ WiFi:             ✓ CONNECTED (EMI source active)
║ Signal:           -63 dBm
╟────────────────────────────────────────────────────────╢
║ Validation:       3 IRQs within 20ms = valid touch
║ Current Phase:    IDLE
╚════════════════════════════════════════════════════════╝

[RESULT] ✓✓✓ PASS: Zero valid touches (EMI filtered successfully)
```

**Success Criteria:**
- ✓ Raw IRQ rate: ~3.5/sec (matches test-44 WiFi EMI baseline)
- ✓ Filtered IRQs: ~3.5/sec (all EMI rejected)
- ✓ Valid touches: **ZERO** (no false positives)

#### Step 2: Run Tap Test

Type in serial monitor:
```
TAPTEST
```

**What happens:**
- Interactive test begins
- CYD screen displays: "TAP SCREEN 5 TIMES"
- **NOW TAP THE SCREEN 5 TIMES**
- Each valid touch logs to serial and updates counter
- Test completes after 5 touches OR 60-second timeout

**Expected Output:**
```
╔════════════════════════════════════════════════════════╗
║      PHASE 2: TAP TEST (INTERACTIVE)                  ║
╠════════════════════════════════════════════════════════╣
║ PURPOSE: Validate real touch detection with EMI       ║
║          filtering active                              ║
║                                                        ║
║ >>> PLEASE TAP THE SCREEN 5 TIMES NOW <<<             ║
║                                                        ║
║ Each valid touch will be logged below:                ║
╚════════════════════════════════════════════════════════╝

[1234 ms] ✓ Valid touch #1 detected
[3456 ms] ✓ Valid touch #2 detected
[5678 ms] ✓ Valid touch #3 detected
[7890 ms] ✓ Valid touch #4 detected
[9012 ms] ✓ Valid touch #5 detected

[RESULT] ✓✓✓ SUCCESS: 5/5 touches detected

╔════════════════════════════════════════════════════════╗
║          TAP TEST COMPLETE                            ║
╚════════════════════════════════════════════════════════╝

╔════════════════════════════════════════════════════════╗
║            EMI FILTER STATISTICS                      ║
╠════════════════════════════════════════════════════════╣
║ Uptime:           10 seconds
╟────────────────────────────────────────────────────────╢
║ Raw IRQs:         50  (5.00/sec)
║ Filtered (EMI):   35  (3.50/sec)
║ Valid Touches:    5   (0.50/sec)
╟────────────────────────────────────────────────────────╢
║ Filter Effectiveness: 70.0% EMI rejected
╟────────────────────────────────────────────────────────╢
║ WiFi:             ✓ CONNECTED (EMI source active)
║ Signal:           -63 dBm
╚════════════════════════════════════════════════════════╝
```

**Success Criteria:**
- ✓ 5/5 touches detected (100% sensitivity)
- ✓ No false positives (WiFi EMI still filtered during taps)

---

### Option B: Manual Commands

#### Check Current Status Anytime

Type in serial monitor:
```
STATUS
```

Shows live statistics without running tests.

#### Reset Statistics

Type in serial monitor:
```
RESET
```

Clears all counters (useful for re-running tests).

#### View Available Commands

Type in serial monitor:
```
HELP
```

---

## Success Criteria Summary

### Baseline Test (PASS if):
1. **Zero valid touches detected** over 60 seconds
2. Raw IRQ rate: **~3.5/sec** (matches test-44 WiFi EMI baseline)
3. Filtered IRQ rate: **~3.5/sec** (100% EMI rejection)
4. Filter effectiveness: **>95%**

### Tap Test (PASS if):
1. **5/5 real touches detected** within timeout
2. No spurious valid touches between deliberate taps
3. Responsive feedback (each tap detected immediately)

---

## Expected Test Results

### Baseline Test Statistics

| Metric | Expected Value | Rationale |
|--------|---------------|-----------|
| Duration | 60 seconds | Long enough to establish statistical baseline |
| Raw IRQs | ~210 total (~3.5/sec) | WiFi EMI rate from test-44 |
| Filtered IRQs | ~210 total (~3.5/sec) | All WiFi EMI rejected |
| Valid Touches | **0** | No false positives from EMI |
| Filter Effectiveness | **100%** | Perfect EMI rejection |

### Tap Test Statistics

| Metric | Expected Value | Rationale |
|--------|---------------|-----------|
| Target Touches | 5 | Sufficient sample for validation |
| Timeout | 60 seconds | Generous window for user interaction |
| Valid Touches Detected | **5/5** | 100% real touch sensitivity |
| False Positives | **0** | WiFi EMI still filtered during test |

---

## Troubleshooting

### WiFi Not Connected

**Symptom:** Serial monitor shows `[WIFI] ✗ Connection timeout`

**Impact:** Test may not be valid - WiFi EMI is the root cause being tested!

**Solution:**
1. Check `/config.txt` on SD card has correct `WIFI_SSID` and `WIFI_PASSWORD`
2. Verify WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Ensure WiFi network is within range

### Baseline Test Shows Valid Touches (Unexpected)

**Symptom:** Valid touch count > 0 during baseline (no touching)

**Possible Causes:**
1. EMI threshold too low (need to increase `MIN_CONSECUTIVE_IRQS`)
2. Physical vibration/movement triggering touch controller
3. User accidentally touched screen during test

**Solution:**
- Type `RESET` and rerun `BASELINE` test
- Ensure CYD device is stable and not moving
- Absolutely do not touch screen during baseline

### Tap Test Misses Real Touches

**Symptom:** <5 touches detected despite tapping 5 times

**Possible Causes:**
1. EMI threshold too high (need to decrease `MIN_CONSECUTIVE_IRQS` or increase `IRQ_VALIDATION_WINDOW_MS`)
2. Light taps not generating sufficient IRQ burst
3. Touch controller malfunction

**Solution:**
- Try firmer taps (not too hard, just deliberate)
- Type `RESET` and rerun `TAPTEST`
- Verify touch works in test-43 (basic touch test)

---

## Algorithm Tuning Parameters

If test results require adjustment:

**File:** `test-sketches/45-wifi-emi-filter/45-wifi-emi-filter.ino`

**Line ~45-46:**
```cpp
const uint32_t IRQ_VALIDATION_WINDOW_MS = 20;  // Time window for burst detection
const uint8_t MIN_CONSECUTIVE_IRQS = 3;        // Minimum IRQs to validate touch
```

**Conservative (fewer false positives, may miss light taps):**
```cpp
const uint32_t IRQ_VALIDATION_WINDOW_MS = 15;  // Shorter window
const uint8_t MIN_CONSECUTIVE_IRQS = 4;        // More IRQs required
```

**Aggressive (more sensitive, may allow some EMI through):**
```cpp
const uint32_t IRQ_VALIDATION_WINDOW_MS = 25;  // Longer window
const uint8_t MIN_CONSECUTIVE_IRQS = 2;        // Fewer IRQs required
```

**Recommended default (validated by test-44 data):**
```cpp
const uint32_t IRQ_VALIDATION_WINDOW_MS = 20;  // Real touches cluster within ~20ms
const uint8_t MIN_CONSECUTIVE_IRQS = 3;        // WiFi EMI: isolated single IRQs (~286ms apart)
```

---

## Next Steps After Validation

Once test-45 passes both phases:

1. **Document Results:** Save serial monitor output showing PASS status
2. **Integration:** Apply EMI filter algorithm to main sketch (`ALNScanner0812Working.ino`)
3. **Resume Phase 4:** Continue orchestrator integration (User Story 2)

---

## Quick Reference: Serial Commands

| Command | Action | When to Use |
|---------|--------|-------------|
| `BASELINE` | Run 60-second autonomous EMI test | Start of testing session |
| `TAPTEST` | Run interactive 5-tap validation | After baseline passes |
| `STATUS` | Show current statistics | Check progress anytime |
| `RESET` | Clear all counters | Re-run tests from clean state |
| `HELP` | Show command list | Reference available commands |

---

**Test Created:** 2025-10-19
**Purpose:** Validate WiFi EMI filtering before integration into main sketch
**Constitution Principle VII:** Test-first hardware validation (NON-NEGOTIABLE)
