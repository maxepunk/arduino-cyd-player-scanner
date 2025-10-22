# Implementation Tasks: Full System Compatibility - ST7789 & ILI9341

**Core Problem - RESOLVED**: ALNScanner works perfectly on ILI9341 but had multiple failures on ST7789:
- ✅ Serial output - FIXED (was blocked by SPI bus deadlock)
- ✅ RFID reader - FIXED (works correctly with proper bus management)
- ✅ Display - Always worked (proves code was running)

**Critical Insight**: The serial failure is our DIAGNOSTIC WINDOW into why ST7789 breaks multiple subsystems.

## Troubleshooting Results (2025-09-19)

### Tests Completed:
1. **Test 01-05**: Basic components (serial, TFT, GPIO3, audio, critical sections) - ✅ ALL WORK
2. **Test 06-07**: Minimal serial tests - ✅ WORK PERFECTLY
3. **Test 08-09**: MFRC522 library inclusion - ✅ WORKS with 3-second delay
4. **Test 10**: TFT_eSPI + MFRC522 + SDSPI combo - ✅ WORKS with ILI9341 config!

### Key Discovery:
**The library combination is NOT the problem!** Test 10 proves that TFT_eSPI (ILI9341 config) + MFRC522 + SDSPI works fine on ST7789 hardware. The issue is specific to ALNScanner's implementation.

### Updated Analysis:
- **Not GPIO3 conflict** - Tests proved GPIO3 usage alone doesn't break serial
- **Not library initialization order** - Test 10 has same globals, works fine
- **Not just MFRC522 delay** - 3-second delay doesn't fix ALNScanner
- **SOMETHING in ALNScanner's MFRC522 initialization code** breaks serial completely

### The Pattern:
- **ILI9341**: Previously worked with different config/environment → Serial ✓ Display ✓ RFID ✓
- **ST7789 with test sketches**: Everything works → Serial ✓ Display ✓
- **ST7789 with ALNScanner**: Partial failure → Serial ✗ Display ✓ RFID ✗
- **ILI9341 with current config**: UNKNOWN - Not tested in current environment

## Phase 1: Software SPI Deep Analysis (2025-09-19)

### Critical Realization
**Serial is just our diagnostic window** - the real goal is working RFID functionality. Our software SPI implementation is the likely culprit, not library incompatibilities or driver mismatches.

### Software SPI Implementation Issues to Test

#### Potential Root Causes:
1. **SPI Protocol Mismatch**
   - MFRC522 requires SPI Mode 0 (CPOL=0, CPHA=0)
   - Clock idle LOW, data sampled on rising edge
   - MSB first transmission
   - Our implementation might have wrong timing/polarity

2. **Critical Section Duration**
   - Each byte transfer holds interrupts for ~48μs
   - MFRC522 init has 20+ register writes = >1ms interrupts disabled
   - UART RX FIFO is only 128 bytes, overflows at 115200 baud if blocked >11ms

3. **GPIO3 (RFID_SS) = UART RX Pin**
   - Manipulating RX pin during SPI operations
   - Inside critical sections preventing UART interrupts
   - Potential electrical/logical conflict

4. **Timing Requirements**
   - MFRC522 needs min 100ns between clock edges
   - Our 2μs delays might be too slow/fast
   - Critical sections might distort timing

### Systematic Test Plan

**T011** - SPI Protocol Verification
**Purpose**: Verify our bit-banging implementation matches MFRC522 SPI Mode 0 requirements
```cpp
// Test 11: test-sketches/11-spi-protocol-verify/11-spi-protocol-verify.ino
// Control variables: NO libraries, NO critical sections initially
// Visual LED feedback, test GPIO3 impact on serial

void setup() {
    Serial.begin(115200);
    delay(3000);  // MFRC522 library delay
    
    // Test 1: GPIO3 (SS/RX) manipulation impact
    Serial.println("Testing GPIO3/RX pin control...");
    digitalWrite(3, HIGH);
    delay(10);
    Serial.println("GPIO3 HIGH - serial survived");
    digitalWrite(3, LOW);
    delay(10);
    Serial.println("GPIO3 LOW - serial survived");
    
    // Test 2: Verify SPI Mode 0 (CPOL=0, CPHA=0)
    // Clock idle LOW, sample on rising edge
    
    // Test 3: Single byte transfer pattern
    // Send 0xAA (10101010) pattern, read response
}
```

**Expected Result**: Identify if GPIO3 manipulation breaks serial, verify clock polarity

**T012** - MFRC522 Version Register Read
**Purpose**: Verify basic communication with MFRC522 chip
```cpp
// Test 12: test-sketches/12-mfrc522-version/12-mfrc522-version.ino
// Read version register (0x37) - should return 0x91 or 0x92
// 0x00 or 0xFF = no communication, random = timing issue

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    // Test with NO critical section
    byte version1 = read_register_no_critical(0x37);
    Serial.printf("No critical: 0x%02X\n", version1);
    
    // Test WITH critical section
    byte version2 = read_register_with_critical(0x37);
    Serial.printf("With critical: 0x%02X\n", version2);
    
    // Multiple reads for consistency check
    for(int i = 0; i < 10; i++) {
        byte v = read_register_no_critical(0x37);
        Serial.printf("Read %d: 0x%02X\n", i, v);
    }
}
```

**Expected Result**: Consistent 0x91/0x92 indicates working SPI; 0x00/0xFF = wiring issue

**T013** - Critical Section Impact Measurement
**Purpose**: Quantify exactly how long we can disable interrupts before serial fails
```cpp
// Test 13: test-sketches/13-critical-timing/13-critical-timing.ino
// Find the breaking point for UART buffer overflow

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    // Test increasing critical section durations
    int durations[] = {10, 50, 100, 500, 1000, 5000, 10000};
    
    for(int d : durations) {
        Serial.printf("Testing %d us critical: ", d);
        Serial.flush();
        
        portENTER_CRITICAL(&mux);
        delayMicroseconds(d);
        portEXIT_CRITICAL(&mux);
        
        Serial.println("survived!");
    }
    
    // Test cumulative effect (like MFRC522 init sequence)
    Serial.println("Testing 20 register writes...");
    for(int i = 0; i < 20; i++) {
        portENTER_CRITICAL(&mux);
        // Simulate register write timing
        delayMicroseconds(48);  // ~8 bits × 6us
        portEXIT_CRITICAL(&mux);
        delayMicroseconds(10);  // Gap between operations
    }
    Serial.println("Cumulative test complete!");
}
```

**Expected Result**: Find maximum critical section duration before serial fails (~11ms theoretical)

**T014** - Alternative SPI Strategies
**Purpose**: Test different approaches to balance timing accuracy vs serial survival
```cpp
// Test 14: test-sketches/14-alternative-spi/14-alternative-spi.ino

// Strategy 1: Minimal critical sections (only around pin changes)
// Strategy 2: No critical sections but busy-wait timing
// Strategy 3: Use different SS pin (not GPIO3/RX)

void test_strategies() {
    // Compare MFRC522 communication success rate
    // Measure serial reliability
    // Find optimal compromise
}
```

**T015** - Progressive MFRC522 Initialization
**Purpose**: Find EXACTLY which register operation breaks serial
```cpp
// Test 15: test-sketches/15-progressive-init/15-progressive-init.ino

void progressive_init() {
    Serial.println("Step 1: Soft reset");
    write_register(0x01, 0x0F);
    Serial.println("Survived soft reset");
    
    Serial.println("Step 2: Timer config (4 writes)");
    // TModeReg, TPrescalerReg, TReloadRegH, TReloadRegL
    
    Serial.println("Step 3: RF config (3 writes)");  
    // RFCfgReg, RxThresholdReg, ModGsPReg
    
    // Identify exact breaking point
}
```

## Phase 2: Solution Implementation

Based on test results, implement the optimal solution:

**Option A**: If critical sections are the issue
- Minimize critical section duration
- Use mutex instead of critical sections
- Add yields between register operations

**Option B**: If SPI protocol is wrong
- Fix clock polarity/phase
- Adjust timing to match MFRC522 datasheet
- Verify bit order (MSB first)

**Option C**: If GPIO3/RX conflict is the issue  
- Use alternative SS pin (GPIO4, GPIO16, etc.)
- Implement proper UART protection
- Consider hardware modification

## Success Criteria

1. ✅ MFRC522 version register returns 0x91/0x92 consistently
2. ✅ Serial output remains functional during RFID operations
3. ✅ Can complete full MFRC522 initialization sequence
4. ✅ Can successfully read RFID/NFC cards
5. ✅ Solution works on BOTH display variants (when properly configured)


## Test Results and Root Cause Analysis

### Tests Executed (2025-09-19 to 2025-09-20)

#### Hardware Testing Phase

##### Test 11: SPI Protocol Verification ✅
- **Result**: GPIO3 manipulation does NOT break serial
- **Finding**: Toggled GPIO3 1000 times without issue
- **Conclusion**: GPIO3/UART RX conflict theory DISPROVEN

##### Test 12: MFRC522 Version Register ❌ → ✅
- **Initial Result**: Returns 0xF6 (should be 0x91/0x92)
- **Resolution**: Replaced with known-good MFRC522
- **Current Status**: Working module returns 0x92 correctly

##### Test 16: Hardware Diagnostic 🔍
- Confirmed original MFRC522 was defective
- Validated wiring and connections
- Verified soft reset functionality

#### Software Isolation Testing Phase

##### Test 17: SD/RFID Progressive Isolation ✅
- **Result**: NO HANG - SD remains accessible after all RFID operations
- **Finding**: Critical sections alone don't cause SD issues
- **Key Insight**: Simulated RFID operations work perfectly

##### Test 18: TFT_eSPI + SD + RFID Component Test ✅
- **Result**: NO HANG - All components work together
- **Finding**: TFT_eSPI, SD, and simulated RFID coexist fine
- **Key Insight**: Library conflicts ruled out

##### Test 19: Exact RFID Sequence Simulation ✅
- **Result**: NO HANG - Even with exact ALNScanner sequence
- **Finding**: Simulated operations don't reproduce issue
- **Critical Discovery**: Only REAL card reads cause hang

##### Test 20: SD Card Contents Verification ✅
- **Result**: Target file `/IMG/534E2B02.bmp` EXISTS (230,456 bytes)
- **Finding**: File has valid BMP header (starts with 0x42 0x4D)
- **Conclusion**: Not a file-not-found issue

### Deep Dive with Instrumented ALNScanner

#### Exact Hang Location Identified
Through progressive logging, we determined the EXACT sequence:

1. ✅ RFID card detected and read (UID: `04 3E F2 8A 06 1F 91`)
2. ✅ NDEF text extracted: "53:4E:2B:02"
3. ✅ `SD.open("/IMG/534E2B02.bmp")` - **SUCCEEDS**
4. ✅ `f.read(header, 54)` - **SUCCEEDS** 
5. ✅ BMP dimensions parsed: Width=240, Height=320, BPP=24
6. ✅ `f.seek(dataOffset)` - **SUCCEEDS**
7. ✅ `tft.startWrite()` - **SUCCEEDS**
8. ✅ `tft.setAddrWindow(0, 0, 240, 320)` - **SUCCEEDS**
9. ❌ **HANG** - Next line never executes

#### The Critical Line That Never Runs:
```cpp
uint8_t *rowBuffer = (uint8_t*)malloc(rowBytes);  // rowBytes = 720
```

### Root Cause Analysis

#### Why Only Real Card Reads Fail
- **Real RFID operations** involve actual RF field modulation
- Hardware timing and interrupts differ from simulation
- MFRC522 FIFO fills with real data requiring CRC validation
- Critical sections during real operations affect system state differently
- Same test card used consistently (NTAG with NDEF text)

#### The Beeping Pattern Discovery
- Beeping occurs BEFORE card scan (in sync with RFID polling)
- Caused by AudioOutputI2S running from setup with empty buffers
- Indicates DMA/interrupt interference
- Stops when system fully hangs

#### Heap/Memory Corruption Hypothesis
The specific sequence of:
1. AudioOutputI2S DMA running (initialized in setup)
2. Software SPI with critical sections for RFID
3. SD card read operations (hardware VSPI)
4. TFT operations (shares VSPI with SD)
5. malloc() request for 720 bytes

Creates conditions for either:
- Heap corruption from interrupt/DMA conflicts
- Deadlock in memory allocator
- Critical section state preventing malloc from completing

### System Constraints (Critical Context)

1. **Cannot use hardware SPI for RFID** because:
   - Display uses pins 12 (MISO), 13 (MOSI), 14 (SCK)
   - These are the ONLY hardware SPI pins available
   - Must use software SPI with different pins (22/27/35/3)

2. **Software SPI requires critical sections** to:
   - Maintain timing integrity
   - Prevent interrupt corruption of bit-banged signals

3. **Shared VSPI bus** between:
   - TFT_eSPI (display operations)
   - SD card (file access)
   - Potential bus arbitration issues

4. **AudioOutputI2S** creates:
   - Continuous DMA activity
   - Interrupt load
   - Potential conflicts with other peripherals

### What We Disproved
- ❌ GPIO3 conflict theory - Works fine
- ❌ Critical sections breaking serial - Not the issue
- ❌ Software SPI implementation wrong - Code is correct
- ❌ Display driver mismatch - ST7789 driver works
- ❌ SD.open() hanging - Actually succeeds, hang is later
- ❌ Library conflicts (TFT_eSPI vs SD) - Work together fine

### Completed Tasks
- [X] Created and executed diagnostic tests 11-20
- [X] Identified and replaced defective MFRC522 module
- [X] Validated software SPI implementation
- [X] Pinpointed exact hang location (after tft.setAddrWindow, before malloc)
- [X] Verified SD card has required files
- [X] Identified beeping pattern as I2S/RFID interference
- [X] Proved only REAL card reads trigger the issue

### Path Forward - Immediate Actions

#### 1. Confirm malloc() as exact hang point
```cpp
// Add logging right before malloc:
Serial.println("[MALLOC-TEST] About to allocate 720 bytes...");
Serial.flush();
uint8_t *rowBuffer = (uint8_t*)malloc(rowBytes);
Serial.println("[MALLOC-TEST] Allocation complete!");
```

#### 2. Test with pre-allocated buffer
- Replace dynamic allocation with static buffer
- Test if this resolves the hang

#### 3. Test without AudioOutputI2S
- Comment out audio initialization
- Verify if DMA conflict is the cause

#### 4. Implement permanent fix based on results

### Proposed Solutions (Priority Order)

1. **Quick Fix**: Pre-allocate BMP row buffer globally
   - Avoids malloc entirely
   - 720 bytes static allocation acceptable

2. **Better Fix**: Delay AudioOutputI2S initialization
   - Initialize only when actually playing audio
   - Reduces DMA conflicts during critical operations

3. **Best Fix**: Refactor critical section usage
   - Minimize duration of interrupt disable
   - Consider mutex-based approach

4. **Fallback**: Add watchdog timer
   - Detect and recover from hangs
   - Log diagnostic info before reset

### Remaining Work
- [X] Add malloc logging and confirm hang point - COMPLETED
- [X] Found exact hang: f.read() after tft.startWrite() - SD/TFT bus conflict!
- [X] Implement fix: Read BMP data before tft.startWrite() - COMPLETED
- [X] Test fix to verify it resolves the hang - CONFIRMED WORKING
- [ ] Clean up diagnostic logging from ALNScanner
- [ ] Document solution in code comments
- [ ] Update CLAUDE.md with findings

### CRITICAL FINDING AND RESOLUTION (2025-09-20)

#### SPI Bus Conflict - RESOLVED
**The hang is NOT in malloc!** It's in the first `f.read()` call AFTER `tft.startWrite()`.
- TFT and SD share the VSPI bus
- `tft.startWrite()` locks the SPI bus for TFT use
- Attempting to read from SD while TFT has the bus causes deadlock
- Only occurs with REAL RFID card reads (not simulated operations)

**Solution**: Restructured the `drawBmp()` function to properly manage SPI bus access:
1. **OLD CODE**: Read BMP header → `tft.startWrite()` → Read/display rows (DEADLOCK!)
2. **NEW CODE**: Read BMP header → Loop for each row:
   - Read row data from SD (while TFT doesn't have bus)
   - `tft.startWrite()` → Write row to display → `tft.endWrite()`
   - Release bus between rows

✅ **SPI FIX CONFIRMED WORKING** - No more hangs!

#### Display Issues - IN PROGRESS

**New Problem Introduced**: Attempted display fixes BROKE BMP rendering:
1. **BMP Loop Direction Change**: Changed `for (int y = height - 1; y >= 0; y--)` to `for (int y = 0; y < height; y++)`
   - **CRITICAL BUG**: The condition `if (y == height - 1)` for `setAddrWindow()` is never true with new loop!
   - **Result**: BMP data written to nowhere - display shows nothing
   
2. **Color Inversion Attempt**: Added `tft.invertDisplay(true)` 
   - **Status**: Colors still inverted - may have made it worse
   - **Issue**: Didn't verify actual driver being used at compile time

**Configuration Confusion**:
- User_Setup.h shows `ST7789_DRIVER` defined
- But earlier docs mention using ILI9341 driver on ST7789 (wrong but "works")
- Need to investigate what's actually being compiled

#### Key Insights
1. **SPI Bus Arbitration**: Must properly manage shared VSPI bus between SD and TFT
2. **Real vs Simulated**: Issue only manifests with actual RFID operations (RF field modulation, hardware interrupts)
3. **Critical Sections**: Software SPI for RFID doesn't interfere if bus management is correct
4. **Serial Output**: Now works reliably - was being blocked by the deadlock

## FINAL RESOLUTION - ALL ISSUES FIXED (2025-09-20)

### Three Critical Fixes Applied:

1. **SPI Bus Deadlock Fix** ✅
   - **Problem**: TFT and SD share VSPI bus. Calling `tft.startWrite()` then `f.read()` caused deadlock
   - **Solution**: Read entire BMP row from SD first, then lock/write/unlock TFT for each row
   - **File**: ALNScanner0812Working.ino lines 872-916

2. **BMP Orientation Fix** ✅
   - **Problem**: Single `setAddrWindow()` call with auto-increment displayed BMP upside-down
   - **Solution**: Call `setAddrWindow(0, y, width, 1)` for each row at correct Y position
   - **File**: ALNScanner0812Working.ino line 896

3. **Color Inversion Fix** ✅
   - **Problem**: ST7789_Init.h hardcodes `writecommand(ST7789_INVON)` causing inverted colors
   - **Solution**: Comment out line 102 in ST7789_Init.h (library modification required)
   - **File**: /Arduino/libraries/TFT_eSPI/TFT_Drivers/ST7789_Init.h line 102

### Final Test Results:
- ✅ RFID scanning works perfectly
- ✅ Serial output fully functional
- ✅ BMP displays with correct orientation
- ✅ Colors display correctly (no inversion)
- ✅ Audio playback works
- ✅ Touch double-tap works
- ✅ No system hangs or deadlocks

### Key Learnings:
1. **SPI Bus Management**: When TFT and SD share a bus, never hold TFT lock while accessing SD
2. **Library Behavior**: ST7789 driver hardcodes inversion - must be modified at library level
3. **BMP Row Management**: Each row needs individual positioning, not bulk auto-increment
4. **Real vs Simulated**: Hardware RFID operations expose timing issues that simulations don't

---
*Completed: 2025-09-20*
*Solution verified on ST7789 CYD hardware (dual USB variant)*