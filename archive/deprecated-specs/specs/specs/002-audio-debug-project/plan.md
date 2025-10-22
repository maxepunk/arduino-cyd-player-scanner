# Implementation Plan: Audio Debug Project - RFID Polling Beep Investigation

**Branch**: `002-audio-debug-project` | **Date**: 2025-09-20 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-audio-debug-project/spec.md`

## Execution Flow (/plan command scope)
```
1. Load feature spec from Input path
   ✓ Loaded spec for audio beeping investigation
2. Fill Technical Context (scan for NEEDS CLARIFICATION)
   ✓ Arduino/ESP32 embedded environment confirmed
   ✓ ALNScanner0812Working.ino as baseline
3. Fill the Constitution Check section
   ✓ Updated with v4.0.0 audio troubleshooting principles
4. Evaluate Constitution Check section
   ✓ All gates align with debugging approach
   → Update Progress Tracking: Initial Constitution Check
5. Execute Phase 0 → research.md
   → Analyzing beeping patterns and I2S/DMA interactions
6. Execute Phase 1 → contracts, data-model.md, quickstart.md
   → Defining diagnostic interfaces and data collection
7. Re-evaluate Constitution Check section
   → Verify design maintains baseline functionality
   → Update Progress Tracking: Post-Design Constitution Check
8. Plan Phase 2 → Describe task generation approach
   → Systematic diagnostic insertion strategy
9. STOP - Ready for /tasks command
```

## Summary
Investigate and resolve unwanted speaker beeping during RFID polling cycles in the ALNScanner0812Working sketch. The beeping appears synchronized with RFID scanning intervals and stops when a card is read, suggesting interaction between AudioOutputI2S DMA operations and RFID polling. Solution must preserve existing functionality while eliminating audio artifacts.

## Technical Context
**Language/Version**: C++/Arduino (ESP32 framework v2.x)  
**Primary Dependencies**: AudioOutputI2S (ESP8266Audio), MFRC522 (software SPI), TFT_eSPI  
**Storage**: SD card for audio files (VSPI hardware SPI)  
**Testing**: Serial output diagnostics via PowerShell monitor at 115200 baud  
**Target Platform**: ESP32 CYD (Cheap Yellow Display) with ST7789/ILI9341  
**Project Type**: embedded (Arduino sketch structure)  
**Performance Goals**: <50μs critical sections, no audio artifacts, maintain RFID scan rate  
**Constraints**: Must debug within ALNScanner0812Working.ino, preserve baseline functionality  
**Scale/Scope**: Single sketch file, ~1400 lines, I2S DMA + software SPI interactions

## Constitution Check (v4.0.0 - Audio Troubleshooting Focus)
*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Audio Debug Project Gates (from Constitution v4.0.0)
- [x] Baseline Preservation: All changes within ALNScanner0812Working.ino?
- [x] In-Situ Debugging: Using #ifdef blocks and conditional compilation?
- [x] Audio System Integration: Analyzing I2S/DMA/RFID interactions?
- [x] Root Cause Analysis: Systematic hypothesis testing planned?
- [x] Incremental Isolation: Progressive debugging within production code?
- [x] Serial Output Diagnostic: All operations logged with timestamps?
- [x] Environment Constraints: WSL2→Windows PowerShell monitoring?
- [x] Diagnostic Code Management: Wrapped in #ifdef DEBUG_AUDIO blocks?
- [x] Timing Discipline: Critical section monitoring included?

## Project Structure

### Documentation (this feature)
```
specs/002-audio-debug-project/
├── spec.md              # Feature specification (complete)
├── plan.md              # This file (/plan command output)
├── research.md          # Phase 0: Audio beeping patterns analysis
├── data-model.md        # Phase 1: Diagnostic data structures
├── quickstart.md        # Phase 1: Testing procedures
├── contracts/           # Phase 1: Diagnostic interfaces
│   ├── serial-api.yaml  # Serial command/response format
│   └── timing-data.yaml # Timing measurement format
└── tasks.md             # Phase 2: Implementation tasks (via /tasks)
```

### Source Code (debugging approach)
```
ALNScanner0812Working/
├── ALNScanner0812Working.ino  # Main sketch (modify with #ifdef blocks)
└── debug_config.h              # Optional: diagnostic configuration

# Diagnostic insertion points:
# - setup(): AudioOutputI2S initialization timing
# - loop(): RFID polling cycle timestamps  
# - Audio buffer state inspection
# - Critical section duration logging
```

**Structure Decision**: Embedded project - all debugging within existing sketch

## Phase 0: Outline & Research

1. **Extract unknowns from Technical Context**:
   - Why does I2S DMA produce beeping with empty buffers?
   - How do software SPI critical sections affect I2S timing?
   - What is the exact correlation between RFID polls and beeps?

2. **Research tasks**:
   - Analyze ESP32 I2S DMA buffer management
   - Study AudioOutputI2S initialization sequence
   - Review software SPI critical section impacts
   - Examine RFID polling timing patterns

3. **Consolidate findings** in `research.md`:
   - I2S DMA behavior with uninitialized buffers
   - Critical section interference patterns
   - Timing correlation analysis methods

**Output**: research.md with beeping root cause hypotheses

## Phase 1: Design & Contracts

1. **Extract diagnostic entities** → `data-model.md`:
   - BeepEvent: timestamp, duration, amplitude
   - RFIDPollEvent: timestamp, result, duration
   - CriticalSectionEvent: start, end, location
   - AudioBufferState: content, underrun flags

2. **Generate diagnostic interfaces** → `/contracts/`:
   - Serial command protocol for diagnostics
   - Timing data output format
   - Event correlation format

3. **Generate test procedures** → `quickstart.md`:
   - Baseline beeping capture procedure
   - Hypothesis testing methodology
   - Result validation criteria

**Output**: Complete diagnostic framework design

## Phase 2: Task Generation Approach
*TO BE EXECUTED BY /tasks COMMAND*

Simple task sequence:
1. **Test Current Behavior**: Confirm beeping occurs
2. **Apply Deferred Init Fix**: Comment line 1139, add init check in startAudio()
3. **Validate Fix**: Test that beeping is eliminated
4. **Verify Audio Works**: Confirm card scanning still triggers audio
5. **Document Results**: Update CLAUDE.md with fix

## Progress Tracking

### Phase Progress
- [x] Phase 0: Research (complete - identified root cause)
- [x] Phase 1: Design & Contracts (simplified for embedded)
- [ ] Phase 2: Task Generation (ready for /tasks command)
- [ ] Phase 3: Implementation (5-minute fix ready)
- [ ] Phase 4: Validation (simple test procedure)

### Constitution Gates
- [x] Initial Constitution Check: PASSED
- [ ] Post-Design Constitution Check: Pending Phase 1

### Complexity Tracking
- No constitution violations detected
- All debugging within existing sketch
- Reversible changes via #ifdef blocks

---
*Ready for Phase 0 execution to begin research*