# Implementation Plan: CYD Multi-Model Compatibility

**Branch**: `001-we-have-a` | **Date**: 2025-09-18 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-we-have-a/spec.md`

## Execution Flow (/plan command scope)
```
1. Load feature spec from Input path
   → If not found: ERROR "No feature spec at {path}"
2. Fill Technical Context (scan for NEEDS CLARIFICATION)
   → Detect Project Type from context (web=frontend+backend, mobile=app+api)
   → Set Structure Decision based on project type
3. Fill the Constitution Check section based on the content of the constitution document.
4. Evaluate Constitution Check section below
   → If violations exist: Document in Complexity Tracking
   → If no justification possible: ERROR "Simplify approach first"
   → Update Progress Tracking: Initial Constitution Check
5. Execute Phase 0 → research.md
   → If NEEDS CLARIFICATION remain: ERROR "Resolve unknowns"
6. Execute Phase 1 → contracts, data-model.md, quickstart.md, agent-specific template file
7. Re-evaluate Constitution Check section
   → If new violations: Refactor design, return to Phase 1
   → Update Progress Tracking: Post-Design Constitution Check
8. Plan Phase 2 → Describe task generation approach (DO NOT create tasks.md)
9. STOP - Ready for /tasks command
```

**IMPORTANT**: The /plan command STOPS at step 7. Phases 2-4 are executed by other commands:
- Phase 2: /tasks command creates tasks.md
- Phase 3-4: Implementation execution (manual or via tools)

## Summary
Update ALNScanner0812Working.ino sketch to automatically detect and support all CYD Resistive 2.8" hardware variants (single USB micro, dual USB micro+Type-C) while maintaining full functionality (display, touch, RFID, SD card, audio) without requiring any wiring changes.

## Technical Context
**Language/Version**: Arduino/C++ (ESP32 Arduino Core 2.x)  
**Primary Dependencies**: TFT_eSPI, MFRC522, XPT2046_Touchscreen, SD, ESP8266Audio  
**Storage**: SD Card via SPI, SPIFFS for internal storage  
**Testing**: Serial monitor diagnostics, hardware validation on both CYD variants  
**Target Platform**: ESP32-WROOM-32 on CYD 2.8" Resistive Touch variants  
**Project Type**: single (embedded firmware)  
**Performance Goals**: <100ms touch response, 30fps display refresh, reliable RFID reading  
**Constraints**: NO WIRING CHANGES ALLOWED, 520KB SRAM limit, maintain all original functionality  
**Scale/Scope**: Single sketch supporting 2+ hardware variants, ~2000 lines of code

## Constitution Check
*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Core Principles Compliance:
- [x] **Universal Hardware Compatibility**: Design supports all CYD R 2.8" variants
- [x] **Zero Wiring Change Policy**: All solutions use existing pin configuration
- [x] **Complete Functionality Preservation**: All features maintained (display, touch, RFID, SD, audio)
- [x] **Comprehensive Diagnostic Feedback**: Extensive serial debugging planned
- [x] **Graceful Degradation**: Partial failure handling included

### Hardware Specifications:
- [x] Pin assignments match constitution (GPIO22/27/35/3 for RFID, etc.)
- [x] Both CYD models documented (ILI9341 vs ST7789)
- [x] Backlight variations addressed (GPIO21 vs GPIO27)

### Testing Requirements:
- [x] Multi-model validation requirement acknowledged
- [x] Diagnostic output verification planned
- [x] Functionality checklist defined

**Initial Gate Status**: PASS - No constitutional violations detected

## Project Structure

### Documentation (this feature)
```
specs/001-we-have-a/
├── plan.md              # This file (/plan command output)
├── research.md          # Phase 0 output (/plan command)
├── data-model.md        # Phase 1 output (/plan command)
├── quickstart.md        # Phase 1 output (/plan command)
├── contracts/           # Phase 1 output (/plan command)
└── tasks.md             # Phase 2 output (/tasks command - NOT created by /plan)
```

### Source Code (repository root)
```
# Arduino Project Structure (existing)
/
├── ALNScanner0812Working/       # Original working sketch
│   └── ALNScanner0812Working.ino
├── CYD_Multi_Compatible.ino     # New unified sketch (to be created)
├── libraries/                   # Arduino libraries
│   ├── TFT_eSPI/
│   │   └── User_Setup.h        # Display configuration
│   ├── MFRC522/
│   └── [other libs]/
└── test_sketches/               # Validation sketches
    ├── CYD_Model_Detector.ino
    └── CYD_Diagnostic_Test.ino
```

**Structure Decision**: Single Arduino sketch with runtime hardware detection

## Phase 0: Outline & Research
1. **Extract unknowns from Technical Context**:
   - Hardware detection methodology for CYD variants
   - Pin conflict resolution for GPIO27 (RFID MOSI vs backlight)
   - Display driver auto-detection (ILI9341 vs ST7789)
   - Touch calibration differences between models
   - Software SPI timing optimization

2. **Research tasks**:
   - Investigate ESP32 GPIO characteristics for model detection
   - Research TFT_eSPI runtime driver switching capabilities
   - Analyze software SPI timing requirements for RFID reliability
   - Study touch controller calibration storage methods
   - Review audio I2S compatibility across models

3. **Consolidate findings** in `research.md`:
   - Hardware detection strategy
   - Display driver selection approach
   - Pin multiplexing solution for GPIO27
   - Touch calibration methodology
   - Performance optimization techniques

**Output**: research.md with all technical decisions documented

## Phase 1: Design & Contracts
*Prerequisites: research.md complete*

1. **Extract entities** → `data-model.md`:
   - HardwareConfig (model type, pin mappings, driver selection)
   - DisplayConfig (driver, resolution, color order, backlight pin)
   - TouchConfig (calibration values, pin assignments)
   - RFIDConfig (SPI pins, timing parameters)
   - DiagnosticState (component status, error messages)

2. **Generate configuration contracts**:
   - Hardware detection interface specification
   - Component initialization sequence
   - Diagnostic reporting format
   - Error recovery procedures

3. **Generate test scenarios**:
   - Model detection validation
   - Display initialization tests
   - Touch calibration verification
   - RFID communication tests
   - Audio playback validation

4. **Create quickstart.md**:
   - Hardware setup verification steps
   - Model detection confirmation
   - Component testing sequence
   - Troubleshooting guide

5. **Update CLAUDE.md**:
   - Add CYD hardware specifics
   - Document pin configurations
   - Include testing requirements

**Output**: data-model.md, contracts/, test scenarios, quickstart.md, CLAUDE.md

## Phase 2: Task Planning Approach
*This section describes what the /tasks command will do - DO NOT execute during /plan*

**Task Generation Strategy**:
- Hardware detection implementation tasks [P]
- Display driver abstraction tasks [P]
- Touch controller calibration tasks
- RFID software SPI optimization tasks
- Diagnostic system implementation tasks [P]
- Integration and validation tasks

**Ordering Strategy**:
1. Core hardware detection first
2. Display and touch subsystems
3. RFID and SD card integration
4. Audio system compatibility
5. Diagnostic and error handling
6. Full system integration tests

**Estimated Output**: 20-25 numbered tasks focusing on incremental compatibility additions

**IMPORTANT**: This phase is executed by the /tasks command, NOT by /plan

## Phase 3+: Future Implementation
*These phases are beyond the scope of the /plan command*

**Phase 3**: Task execution (/tasks command creates tasks.md)  
**Phase 4**: Implementation (execute tasks following Zero Wiring Change Policy)  
**Phase 5**: Validation (test on both CYD variants, verify all functionality)

## Complexity Tracking
*No violations - constitution fully complied with*

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| None | - | - |

## Progress Tracking
*This checklist is updated during execution flow*

**Phase Status**:
- [x] Phase 0: Research complete (/plan command)
- [x] Phase 1: Design complete (/plan command)
- [x] Phase 2: Task planning complete (/plan command - describe approach only)
- [ ] Phase 3: Tasks generated (/tasks command)
- [ ] Phase 4: Implementation complete
- [ ] Phase 5: Validation passed

**Gate Status**:
- [x] Initial Constitution Check: PASS
- [x] Post-Design Constitution Check: PASS
- [x] All NEEDS CLARIFICATION resolved
- [x] Complexity deviations documented (none needed)

---
*Based on Constitution v1.0.0 - See `.specify/memory/constitution.md`*