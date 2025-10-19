# Tasks: CYD Multi-Model Compatibility

**Input**: Design documents from `/specs/001-we-have-a/`
**Prerequisites**: plan.md (required), research.md, data-model.md, contracts/

## Execution Flow (main)
```
1. Load plan.md from feature directory
   → If not found: ERROR "No implementation plan found"
   → Extract: tech stack, libraries, structure
2. Load optional design documents:
   → data-model.md: Extract entities → model tasks
   → contracts/: Each file → contract test task
   → research.md: Extract decisions → setup tasks
3. Generate tasks by category:
   → Setup: project init, dependencies, linting
   → Tests: contract tests, integration tests
   → Core: models, services, CLI commands
   → Integration: DB, middleware, logging
   → Polish: unit tests, performance, docs
4. Apply task rules:
   → Different files = mark [P] for parallel
   → Same file = sequential (no [P])
   → Tests before implementation (TDD)
5. Number tasks sequentially (T001, T002...)
6. Generate dependency graph
7. Create parallel execution examples
8. Validate task completeness:
   → All contracts have tests?
   → All entities have models?
   → All endpoints implemented?
9. Return: SUCCESS (tasks ready for execution)
```

## Format: `[ID] [P?] Description`
- **[P]**: Can run in parallel (different files, no dependencies)
- Include exact file paths in descriptions

## Path Conventions
- **Arduino Project**: Main sketch at root, test sketches in `test_sketches/`
- **Libraries**: Modified in `libraries/` subdirectories
- Path structure follows Arduino conventions

## Phase 3.1: Setup & Diagnostics
- [ ] T001 Create test sketch structure: `test_sketches/CYD_Model_Detector/CYD_Model_Detector.ino`
- [ ] T002 Create diagnostic test: `test_sketches/CYD_Diagnostic_Test/CYD_Diagnostic_Test.ino`
- [ ] T003 [P] Implement serial diagnostic system per diagnostic-reporting.h in `test_sketches/CYD_Diagnostic_Test/DiagnosticReporter.cpp`
- [ ] T004 [P] Create hardware detection probe sketch in `test_sketches/Hardware_Probe/Hardware_Probe.ino`
- [ ] T005 Backup original sketch: Copy `ALNScanner0812Working/ALNScanner0812Working.ino` to `ALNScanner0812Working/ALNScanner0812Working_backup.ino`

## Phase 3.2: Tests First (TDD) ⚠️ MUST COMPLETE BEFORE 3.3
**CRITICAL: These tests MUST be written and MUST FAIL before ANY implementation**
- [ ] T006 [P] Hardware detection contract test per hardware-detection.h in `test_sketches/Test_Hardware_Detection/Test_Hardware_Detection.ino`
- [ ] T007 [P] Component initialization contract test per component-initialization.h in `test_sketches/Test_Component_Init/Test_Component_Init.ino`
- [ ] T008 [P] Diagnostic reporting contract test per diagnostic-reporting.h in `test_sketches/Test_Diagnostics/Test_Diagnostics.ino`
- [ ] T009 [P] Display driver detection test for ILI9341 in `test_sketches/Test_ILI9341_Detection/Test_ILI9341_Detection.ino`
- [ ] T010 [P] Display driver detection test for ST7789 in `test_sketches/Test_ST7789_Detection/Test_ST7789_Detection.ino`
- [ ] T011 [P] GPIO27 multiplexing test in `test_sketches/Test_GPIO27_Mux/Test_GPIO27_Mux.ino`
- [ ] T012 [P] Touch calibration persistence test in `test_sketches/Test_Touch_Calibration/Test_Touch_Calibration.ino`
- [ ] T013 [P] RFID software SPI timing test in `test_sketches/Test_RFID_Timing/Test_RFID_Timing.ino`

## Phase 3.3: Core Implementation (ONLY after tests are failing)
- [ ] T014 Create main unified sketch file: `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T015 [P] Implement HardwareConfig struct from data-model.md in `CYD_Multi_Compatible/HardwareConfig.h`
- [ ] T016 [P] Implement DisplayConfig struct from data-model.md in `CYD_Multi_Compatible/DisplayConfig.h`
- [ ] T017 [P] Implement TouchConfig struct from data-model.md in `CYD_Multi_Compatible/TouchConfig.h`
- [ ] T018 [P] Implement RFIDConfig struct from data-model.md in `CYD_Multi_Compatible/RFIDConfig.h`
- [ ] T019 [P] Implement DiagnosticState struct from data-model.md in `CYD_Multi_Compatible/DiagnosticState.h`
- [ ] T020 Implement HardwareDetector class per hardware-detection.h in `CYD_Multi_Compatible/HardwareDetector.cpp`
- [ ] T021 Implement display driver ID detection in `CYD_Multi_Compatible/HardwareDetector.cpp` (detectDisplayDriver method)
- [ ] T022 Implement backlight pin detection logic in `CYD_Multi_Compatible/HardwareDetector.cpp` (detectBacklightPin method)
- [ ] T023 [P] Create GPIO27Manager class for pin multiplexing in `CYD_Multi_Compatible/GPIO27Manager.cpp`
- [ ] T024 Implement ComponentInitializer class per component-initialization.h in `CYD_Multi_Compatible/ComponentInitializer.cpp`
- [ ] T025 Implement display initialization with runtime driver selection in `CYD_Multi_Compatible/ComponentInitializer.cpp` (initDisplay method)
- [ ] T026 Implement touch controller initialization with calibration loading in `CYD_Multi_Compatible/ComponentInitializer.cpp` (initTouch method)
- [ ] T027 Implement RFID initialization with software SPI in `CYD_Multi_Compatible/ComponentInitializer.cpp` (initRFID method)
- [ ] T028 [P] Implement DiagnosticReporter class per diagnostic-reporting.h in `CYD_Multi_Compatible/DiagnosticReporter.cpp`
- [ ] T029 [P] Implement EEPROM calibration storage in `CYD_Multi_Compatible/CalibrationManager.cpp`
- [ ] T030 [P] Create software SPI implementation with timing control in `CYD_Multi_Compatible/SoftwareSPI.cpp`

## Phase 3.4: Integration & Component Assembly
- [ ] T031 Integrate hardware detection into main sketch setup() in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T032 Implement component initialization sequence in setup() following constitution order in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T033 Add GPIO27 multiplexing logic to RFID operations in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T034 Integrate diagnostic reporting throughout initialization in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T035 Port RFID reading logic from ALNScanner0812Working.ino with software SPI in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T036 Port NDEF text extraction from ALNScanner0812Working.ino in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T037 Port BMP image display from ALNScanner0812Working.ino in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T038 Port audio playback functionality from ALNScanner0812Working.ino in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T039 Implement error recovery procedures per constitution in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T040 Add serial command interface for diagnostics ('P' for pins, 'D' for dump) in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`

## Phase 3.5: Hardware-Specific Testing & Validation
- [ ] T041 Test on single USB CYD with ILI9341 display - verify all functions per quickstart.md Test 1-5
- [ ] T042 Test on dual USB CYD with ST7789 display - verify all functions per quickstart.md Test 1-5
- [ ] T043 Verify GPIO27 multiplexing works on dual USB model (backlight + RFID)
- [ ] T044 Validate touch calibration saves and loads correctly on both models
- [ ] T045 Test diagnostic output completeness per constitution requirements
- [ ] T046 Verify graceful degradation with disconnected RFID module
- [ ] T047 Verify graceful degradation with missing SD card
- [ ] T048 Test recovery procedures for display initialization failures

## Phase 3.6: Polish & Documentation
- [ ] T049 [P] Optimize software SPI timing for reliable RFID reads in `CYD_Multi_Compatible/SoftwareSPI.cpp`
- [ ] T050 [P] Add memory usage diagnostics and reporting in `CYD_Multi_Compatible/DiagnosticReporter.cpp`
- [ ] T051 [P] Create troubleshooting guide addendum in `docs/Troubleshooting_Guide.md`
- [ ] T052 [P] Document pin configuration per model in `docs/Hardware_Variants.md`
- [ ] T053 Add verbose mode toggle (hold corner during boot) in `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- [ ] T054 Verify sketch stays under 520KB SRAM usage limit
- [ ] T055 Run complete quickstart.md verification sequence on both hardware variants
- [ ] T056 Update ALNScanner0812Working README with migration instructions

## Dependencies
- Setup (T001-T005) must complete first
- Tests (T006-T013) before any implementation (T014-T030)
- Core structs (T015-T019) can be parallel, before classes
- HardwareDetector (T020-T022) before ComponentInitializer (T024-T027)
- GPIO27Manager (T023) before RFID integration (T035)
- All core implementation before integration (T031-T040)
- Integration complete before hardware testing (T041-T048)
- All testing complete before polish (T049-T056)

## Parallel Execution Examples
```
# Phase 3.2 - Launch all test sketches together:
Task: "Hardware detection contract test in test_sketches/Test_Hardware_Detection/"
Task: "Component initialization contract test in test_sketches/Test_Component_Init/"
Task: "Diagnostic reporting contract test in test_sketches/Test_Diagnostics/"
Task: "ILI9341 detection test in test_sketches/Test_ILI9341_Detection/"
Task: "ST7789 detection test in test_sketches/Test_ST7789_Detection/"
Task: "GPIO27 multiplexing test in test_sketches/Test_GPIO27_Mux/"
Task: "Touch calibration test in test_sketches/Test_Touch_Calibration/"
Task: "RFID timing test in test_sketches/Test_RFID_Timing/"

# Phase 3.3 - Launch all struct implementations together:
Task: "Implement HardwareConfig struct in CYD_Multi_Compatible/HardwareConfig.h"
Task: "Implement DisplayConfig struct in CYD_Multi_Compatible/DisplayConfig.h"
Task: "Implement TouchConfig struct in CYD_Multi_Compatible/TouchConfig.h"
Task: "Implement RFIDConfig struct in CYD_Multi_Compatible/RFIDConfig.h"
Task: "Implement DiagnosticState struct in CYD_Multi_Compatible/DiagnosticState.h"
```

## Critical Path
1. Hardware detection (T020-T022) - Blocks everything
2. Component initialization (T024-T027) - Blocks integration
3. GPIO27 Manager (T023) - Blocks RFID on dual USB
4. Main sketch integration (T031-T040) - Sequential, critical
5. Hardware testing (T041-T042) - Final validation

## Notes
- **[P]** tasks modify different files and can run simultaneously
- All test sketches must fail initially (TDD approach)
- GPIO27 multiplexing is critical for dual USB model
- Maintain Zero Wiring Change Policy throughout
- Each task produces testable output via Serial Monitor
- Diagnostic output at 115200 baud for all operations
- Commit after each phase completes successfully

## Validation Checklist
*GATE: Verified before execution*

- [x] All contracts (3 files) have corresponding tests (T006-T008)
- [x] All entities (5 structs) have implementation tasks (T015-T019)
- [x] All tests come before implementation (Phase 3.2 before 3.3)
- [x] Parallel tasks modify different files only
- [x] Each task specifies exact file path
- [x] No parallel tasks modify same file
- [x] Hardware testing covers both CYD variants
- [x] Constitution requirements addressed (diagnostics, graceful degradation)