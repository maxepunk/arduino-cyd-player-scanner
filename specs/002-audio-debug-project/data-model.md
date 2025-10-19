# Data Model: Simple Debug Output

**Feature**: 002-audio-debug-project  
**Date**: 2025-09-20  
**Status**: Simplified

## Overview
Simple serial debug output to identify when and why beeping occurs. No complex data structures needed - just strategic Serial.printf() statements.

## Debug Output Points

### 1. Audio Initialization Tracking
```cpp
// Line 1139 in setup()
Serial.printf("[AUDIO] Init at %lu ms\n", millis());
```
**Purpose**: Identify when I2S DMA starts (likely when beeping begins)

### 2. RFID Polling Marker
```cpp
// Line 1204 in loop() 
Serial.printf("[POLL] %lu ms\n", millis());
```
**Purpose**: Correlate beeping rhythm with polling cycle

### 3. Card Detection Event
```cpp
// Line 1222 when card detected
Serial.printf("[CARD] Detected at %lu ms\n", millis());
```
**Purpose**: Confirm beeping stops when card processing begins

### 4. Audio Playback Start
```cpp
// Line 924 in startAudio()
Serial.printf("[PLAY] Audio start at %lu ms\n", millis());
```
**Purpose**: Verify real audio replaces beeping

## Expected Output Pattern

### With Beeping (Current Behavior)
```
[AUDIO] Init at 3145 ms
READY TO SCAN
[POLL] 3245 ms
[POLL] 3345 ms
[POLL] 3445 ms
[User hears beeping synchronized with polls]
[CARD] Detected at 8932 ms
[PLAY] Audio start at 8950 ms
[User: beeping stops, audio plays]
```

### After Fix (Expected)
```
READY TO SCAN  
[POLL] 3145 ms
[POLL] 3245 ms
[POLL] 3345 ms
[User: silence]
[CARD] Detected at 8932 ms
[AUDIO] Init at 8949 ms  <- Deferred init
[PLAY] Audio start at 8950 ms
[User: audio plays, no preceding beeps]
```

## Implementation Approach

No complex data structures. Just:
1. Add 4 debug print statements
2. Observe correlation between init and beeping
3. Apply deferred initialization fix
4. Verify beeping eliminated

Total memory overhead: ~100 bytes for format strings
Total code change: ~10 lines including fix

---
*Simplified for embedded reality - no complex structures needed*