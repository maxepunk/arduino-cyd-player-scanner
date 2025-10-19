# Data Model: Software SPI Implementation Fix

This is a software timing/interrupt issue, NOT a display configuration problem. The critical factors are:
1. Critical section duration during SPI operations
2. GPIO3 usage as both RFID_SS and UART RX
3. MFRC522 communication protocol correctness

## The Real Technical Data

### Software SPI Timing Parameters
```cpp
// Current problematic implementation
portENTER_CRITICAL(&spiMux);  // Disables ALL interrupts
// 8 bits × 6μs = 48μs per byte
// 20+ register writes = >1ms total
portEXIT_CRITICAL(&spiMux);
```

### Hardware Pin Conflicts
```
GPIO3: RFID_SS (our usage) + UART RX (ESP32 hardware function)
GPIO22: RFID SCK (software SPI)
GPIO27: RFID MOSI (software SPI)
GPIO35: RFID MISO (software SPI)
```

### MFRC522 Communication Requirements
```
SPI Mode: 0 (CPOL=0, CPHA=0)
Clock: Idle LOW, sample on rising edge
Timing: Min 100ns between clock edges
Version Register: Should return 0x91 or 0x92
```

## The Diagnostic Workflow

1. **Run Test 11**: Verify SPI protocol and GPIO3 impact
2. **Run Test 12**: Check MFRC522 communication (version register)
3. **Run Test 13**: Measure critical section impact on serial
4. **Run Test 14**: Try alternative SPI strategies
5. **Run Test 15**: Find exact breaking point in init sequence
6. **Apply Fix**: Based on test results
7. **Verify**: RFID functionality preserved while serial works

## What Could Go Wrong

### Critical Sections Too Long
**Symptom**: Complete serial failure, display works
**Current State**: This is what we're experiencing
**Fix**: Minimize critical section duration

### GPIO3/RX Conflict
**Symptom**: Serial corrupted when RFID operations occur
**Fix**: Use different SS pin (requires wiring change)

### SPI Protocol Mismatch
**Symptom**: MFRC522 returns 0x00, 0xFF, or random values
**Fix**: Correct clock polarity/phase implementation

## The Solution Approach

### Option A: Fix Critical Sections (Primary)
```cpp
// Minimize interrupt blocking
// Only critical around actual pin changes
// Add yields between operations
```

### Option B: Alternative SS Pin (Secondary)
```cpp
// Change from GPIO3 to GPIO4
#define RFID_SS 4  // Instead of 3
// Requires physical rewiring
```

### Option C: Protocol Correction (If Needed)
```cpp
// Ensure proper SPI Mode 0
// Clock idle LOW
// Sample on rising edge
```

## Success Confirmation

### What "Working" Looks Like
1. Serial: Full output during RFID operations
2. MFRC522: Version register returns 0x91/0x92
3. RFID: Successfully reads cards
4. Display: Continues to render correctly
5. No crashes or hangs during operation

The key: Balanced solution that preserves RFID functionality while maintaining serial communication.