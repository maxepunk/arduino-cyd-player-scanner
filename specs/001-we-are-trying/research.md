# Research: ALNScanner Software SPI Implementation Issues

**Date**: 2025-09-19  
**Engineer**: Senior embedded systems analysis

## Executive Summary
ALNScanner0812Working.ino previously worked on ILI9341 CYD variants but currently fails on ST7789 hardware with complete serial failure and non-responsive RFID. Through systematic testing, we've identified the issue is in the software SPI implementation's critical sections, NOT display driver incompatibility.

## Core Problem Analysis

### Test Results Summary
- Tests 01-05: Basic components work individually (serial, TFT, GPIO3, audio, critical sections)
- Tests 08-09: MFRC522 library inclusion works with 3-second delay
- Test 10: TFT_eSPI + MFRC522 + SDSPI combination works fine
- **ALNScanner**: Complete serial failure even with all fixes applied

### The Actual Issue
The software SPI implementation in ALNScanner (lines 87-116):
```cpp
portENTER_CRITICAL(&spiMux);  // DISABLES ALL INTERRUPTS
// 8 bit transfers with 6us delays = ~48us interrupts disabled
portEXIT_CRITICAL(&spiMux);
```
Combined with MFRC522 initialization (20+ register writes) creates cumulative interrupt blocking that causes UART buffer overflow.

## Key Technical Findings

### Why GPIO3 as RFID_SS is Problematic
1. GPIO3 is the hardware UART RX pin on ESP32
2. Manipulating RX pin inside critical sections disrupts UART
3. Even though test 03 showed GPIO3 usage alone works, the combination with critical sections is fatal

### Critical Section Timing Analysis
- Each byte transfer: ~48μs with interrupts disabled
- MFRC522 init sequence: 20+ register writes
- Cumulative blocking: >1ms of disabled interrupts
- ESP32 UART FIFO: 128 bytes
- At 115200 baud: ~87μs per byte
- Buffer overflow threshold: ~11ms

### Current Configuration Status
- Using ILI9341_DRIVER for ST7789 hardware (wrong but "works")
- Display renders because ST7789 tolerates ILI9341 init commands
- System operates in marginal state, more sensitive to timing issues

## Solution Approaches

### Option 1: Fix Critical Sections (PRIMARY)
**Approach**: Minimize or eliminate critical sections in software SPI
**Implementation Options**:
- Use minimal critical sections only around pin changes
- Replace with mutex for better interrupt handling
- Add yields between register operations
- Reduce delays inside critical sections

### Option 2: Alternative SS Pin (SECONDARY)
**Approach**: Use different pin instead of GPIO3 for RFID_SS
**Candidates**: GPIO4, GPIO16, GPIO17 (unused pins)
**Pros**: Avoids RX pin conflict
**Cons**: Requires wiring change (violates zero-change goal)

### Option 3: Fix SPI Protocol (IF NEEDED)
**Approach**: Verify and correct SPI Mode 0 implementation
- Ensure CPOL=0 (clock idle low)
- Ensure CPHA=0 (sample on rising edge)
- Verify MSB-first transmission
- Adjust timing to MFRC522 datasheet specs

## Systematic Test Plan

### Test 11: SPI Protocol Verification
- Test GPIO3 manipulation impact on serial
- Verify SPI Mode 0 implementation
- Check clock polarity and phase

### Test 12: MFRC522 Version Register
- Read version register (should return 0x91/0x92)
- Test with and without critical sections
- Multiple reads for consistency

### Test 13: Critical Section Timing
- Measure maximum safe critical section duration
- Test cumulative effect of multiple operations
- Find UART buffer overflow threshold

### Test 14: Alternative Strategies
- Minimal critical sections (only pin changes)
- No critical sections with busy-wait timing
- Different SS pin options

### Test 15: Progressive MFRC522 Init
- Identify exact register operation that breaks serial
- Test each initialization step separately
- Find minimal working configuration

## Expected Outcomes

### If Critical Sections are the Problem
- Serial will fail when cumulative critical section time exceeds ~11ms
- Removing/minimizing critical sections will restore serial
- RFID timing might become less reliable without protection

### If GPIO3/RX Conflict is the Problem  
- Using alternative SS pin will fix both serial and RFID
- But requires wiring change (undesirable)

### If SPI Protocol is Wrong
- MFRC522 version register will return 0x00, 0xFF, or random values
- Fixing clock polarity/phase will restore communication

## Decision Summary

**Primary Focus**: Fix software SPI critical sections
**Secondary**: Verify SPI protocol correctness
**Last Resort**: Alternative SS pin (requires wiring change)
**Implementation**: Systematic testing to identify root cause, then targeted fix
**Risk**: Medium - must preserve RFID functionality while fixing serial

---
*Updated: 2025-09-19*
*Focus shifted from display driver issues to software SPI implementation*