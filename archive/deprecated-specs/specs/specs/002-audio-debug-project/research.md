# Research: Audio Beeping During RFID Polling

**Feature**: 002-audio-debug-project  
**Date**: 2025-09-20  
**Status**: Complete

## Executive Summary
The unwanted beeping during RFID polling is caused by AudioOutputI2S DMA reading uninitialized or empty buffers while the audio system is initialized but not actively playing content. The beeping pattern correlates with RFID polling cycles due to timing interactions between software SPI critical sections and I2S DMA operations.

## Research Findings

### 1. ESP32 I2S DMA Buffer Behavior

**Decision**: I2S DMA continuously reads buffers once initialized  
**Rationale**: The ESP32 I2S peripheral operates autonomously via DMA, continuously transferring data from memory to the DAC regardless of buffer content  
**Alternatives considered**: 
- Delayed initialization (init only when needed)
- Silent buffer pre-filling
- DMA pause between operations

**Key Finding**: AudioOutputI2S(0, 1) in setup() starts DMA immediately with potentially uninitialized buffers containing random data that produces audible artifacts.

### 2. AudioOutputI2S Initialization Sequence

**Decision**: Audio system initializes in setup() but buffers aren't filled until first WAV play  
**Rationale**: Current implementation follows ESP8266Audio library patterns  
**Alternatives considered**:
- Lazy initialization on first use
- Pre-fill with silence
- Mute until first real audio

**Key Finding**: The library starts I2S DMA during initialization but doesn't guarantee silent buffers, leading to noise output from random memory content.

### 3. Software SPI Critical Section Impact

**Decision**: Critical sections during RFID polling affect I2S DMA timing  
**Rationale**: Software SPI uses portENTER_CRITICAL/portEXIT_CRITICAL which disables interrupts  
**Alternatives considered**:
- Mutex-based SPI protection
- Reduced critical section duration
- Hardware SPI for RFID (not possible due to pin conflicts)

**Key Finding**: Each RFID poll involves ~20-30 SPI transactions with critical sections, creating a rhythmic pattern of interrupt disabling that may affect I2S DMA buffer switching, creating the synchronized beeping pattern.

### 4. RFID Polling Timing Correlation

**Decision**: 100ms polling interval creates audible rhythm  
**Rationale**: Main loop checks for cards every 100ms as seen in line 1208-1211  
**Alternatives considered**:
- Variable polling intervals
- Event-driven polling
- Continuous polling without delay

**Key Finding**: The beeping stops when a card is read because:
1. Card processing takes longer, breaking the 100ms rhythm
2. Real audio playback overwrites the garbage buffers
3. System state changes from polling to processing mode

## Root Cause Hypothesis

### Primary Hypothesis: Uninitialized I2S Buffers
**Evidence**:
- Beeping starts immediately after power-on (when AudioOutputI2S initializes)
- Beeping has consistent low volume (typical of random data interpreted as audio)
- Beeping stops when real audio plays (buffers get valid data)

**Mechanism**:
1. `setup()` creates `AudioOutputI2S(0, 1)` at line 1139
2. Constructor starts I2S DMA with default (uninitialized) buffers
3. DMA reads garbage data, outputting as low-amplitude noise
4. RFID polling creates timing that modulates the noise into beeps

### Secondary Hypothesis: Critical Section Interference
**Evidence**:
- Beeping rhythm matches RFID polling rate
- Software SPI uses critical sections that disable interrupts
- I2S relies on interrupt-driven DMA buffer switching

**Mechanism**:
1. RFID polling at 100ms intervals
2. Each poll has multiple critical sections (20+ register operations)
3. Critical sections delay I2S interrupt handling
4. Delayed buffer switches create audible glitches perceived as beeps

## Recommended Solutions

### Solution 1: Deferred Audio Initialization (Simplest)
```cpp
// In setup() - comment out immediate initialization
// out = new AudioOutputI2S(0, 1);  // REMOVE THIS

// In startAudio() - initialize on first use
if (!out) {
    out = new AudioOutputI2S(0, 1);
}
```
**Pros**: No DMA until needed, eliminates all spurious audio  
**Cons**: Slight delay on first audio playback

### Solution 2: Silent Buffer Initialization
```cpp
// After creating AudioOutputI2S in setup()
out = new AudioOutputI2S(0, 1);
// Fill buffers with silence
uint8_t silence[512] = {0};
for(int i = 0; i < 8; i++) {  // Fill multiple buffers
    out->ConsumeSample(silence);
}
```
**Pros**: Maintains current initialization timing  
**Cons**: Requires understanding of internal buffer structure

### Solution 3: Mute Until First Play
```cpp
// Add mute control
out = new AudioOutputI2S(0, 1);
out->SetGain(0.0);  // Mute

// In startAudio()
out->SetGain(1.0);  // Unmute for actual playback
```
**Pros**: Simple, non-invasive  
**Cons**: DMA still running (power consumption)

## Testing Strategy

1. **Baseline Capture**:
   - Record current beeping pattern
   - Note exact timing relative to startup
   - Correlate with RFID polling via serial logs

2. **Solution Validation**:
   - Implement each solution with #ifdef blocks
   - Test with/without RFID polling active
   - Verify no audio artifacts during scanning
   - Confirm normal audio playback still works

3. **Side Effect Testing**:
   - Verify RFID scanning performance unchanged
   - Check audio playback latency
   - Monitor power consumption changes

## Implementation Recommendation

**Recommended: Solution 1 (Deferred Initialization)**

This is the simplest, most elegant solution that aligns with the constitution's principles:
- Minimal code change (2 lines)
- Easy to test and revert
- Eliminates root cause completely
- No performance impact on RFID scanning

The slight delay on first audio playback (estimated <50ms) is acceptable given that it only occurs once per card read and eliminates all unwanted audio during scanning.

---
*Research complete - ready for Phase 1 design*