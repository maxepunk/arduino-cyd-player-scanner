# Tasks: CYD Multi-Hardware Compatibility Testing and Verification

**Input**: Design documents from `/specs/002-we-were-working/`
**Prerequisites**: plan.md (required), research.md, data-model.md, contracts/serial-api.yaml
**Implementation Path**: CYD_Multi_Compatible modular sketch (NOT ALNScanner0812Working)

## Critical Implementation Notes
- **CHANGE FROM ORIGINAL PLAN**: Using CYD_Multi_Compatible directory instead of ALNScanner0812Working
- **PRIORITY**: Hardware verification MUST happen before sketch fixes
- **KEY INSIGHT**: CYD_Multi_Compatible already has modular architecture with HardwareDetector, GPIO27Manager, etc.
- **TOUCH DISCOVERY**: Touch uses IRQ-only detection (GPIO36) - NO coordinate reading possible or needed
- **REFERENCE**: ALNScanner0812Working demonstrates working IRQ-only touch implementation

## 🚨 WSL2→Windows Environment Context 🚨
**Arduino CLI runs on Windows, accessed from WSL2 Ubuntu**
- **Serial Ports**: Use Windows COM ports (COM3-COM8), NOT Linux /dev/tty*
- **Board Detection**: `arduino-cli board list` shows Windows COM ports
- **File Paths**: Auto-translate from WSL2 to Windows UNC paths
- **Compilation**: Must be sequential - Windows file locking prevents parallel builds
- **Serial Monitor**: ⚠️ **CRITICAL** - `arduino-cli monitor` DOES NOT WORK in WSL2
  - **MUST USE**: `powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1`
  - **Location**: Project root directory
  - **Workflow**: Compile → Upload → Sleep 1s → PowerShell monitor

## Execution Flow (main)
```
1. Hardware verification phase (T001-T006)
   → Use test sketches to verify hardware is functioning
   → Confirm wiring matches expected configuration
   → Document any hardware-specific requirements
2. Fix compilation errors (T011-T015)
   → Resolve naming conflicts (SPI_MODE0)
   → Remove TOUCH_CS references (touch is IRQ-only)
   → Fix API mismatches in DiagnosticState
   → Update SerialCommands for IRQ-only touch
3. Upload and test (T016-T020)
   → Upload to connected CYD (COM8 or appropriate port)
   → Execute all serial commands
   → Verify GPIO27 multiplexing on dual USB
   → Test IRQ-only touch detection
4. Validate functionality (T026-T030)
   → RFID scanning with actual cards
   → Touch tap detection (no calibration)
   → Component graceful degradation
```

## Format: `[ID] [P?] Description`
- **[P]**: Can run in parallel (different files, no dependencies)
- Include exact file paths in descriptions

## Path Conventions
- **Main Sketch**: `CYD_Multi_Compatible/CYD_Multi_Compatible.ino`
- **Test Sketches**: `test-sketches/{component}-test/{component}-test.ino`
- **Serial Commands**: `CYD_Multi_Compatible/SerialCommands.cpp`
- **Libraries**: Windows location C:\Users\spide\Documents\Arduino\libraries\

## Phase 0: Documentation Alignment 📚 IN PROGRESS

- [x] T000A Update CLAUDE.md for IRQ-only touch (completed)
- [x] T000B Update data-model.md to replace TouchCalibration with TouchState (completed)  
- [x] T000C Update research.md with environment and touch findings (completed)
- [x] T000D Update contracts/serial-api.yaml for IRQ-only touch (completed)
- [x] T000E Update quickstart.md with correct workflows (completed)
- [x] T000F Update plan.md to remove XPT2046_Touchscreen dependency and calibration references
- [x] T000G Update tasks.md comprehensively 

## Phase 1: Hardware Verification & Wiring Check 🔧 IN PROGRESS

- [x] T001 Verify Arduino CLI and ESP32 board package installed (completed)
- [x] T002 Verify required libraries: TFT_eSPI MFRC522 ESP8266Audio SD ArduinoJson (completed - XPT2046_Touchscreen NOT needed)
- [x] T003 Identify connected CYD port using `arduino-cli board list` on Windows COM ports (completed - COM8)
- [x] T004 Upload and run test-sketches/display-test/display-test.ino to verify display and backlight (completed - display working)
- [x] T005 Clean up touch tests and create proper IRQ-only touch test sketch (completed)
  - **DISCOVERY**: XPT2046 SPI interface NOT CONNECTED on CYD boards
  - **SOLUTION**: Use IRQ-only (GPIO36) for tap/double-tap detection
  - **COMPLETED**: Created test-sketches/touch-irq-test/touch-irq-test.ino
  - **VERIFIED**: Single-tap and double-tap detection working with 200ms debounce
  - **NOTE**: Each physical tap generates 2 interrupts (press + release), debounce filters the release
- [x] T006 Ensure consistent serial output from hardware detection and document the output to confirm variant (single vs dual USB).  Hardware detection test MUST work consistently before this task is completed. (COMPLETED - Dual USB ST7789 variant confirmed)

## Phase 2: Test Sketch Validation [ ] 🧪

- [ ] T007 [ ] Upload test-sketches/rfid-test/rfid-test.ino and verify RFID communication with GPIO27 multiplexing
- [ ] T008 [ ] Upload test-sketches/sd-test/sd-test.ino and verify SD card read/write operations
- [ ] T009 [ ] Upload test-sketches/audio-test/audio-test.ino and verify I2S audio output
- [ ] T010 Document any wiring issues discovered and create variant-specific wiring guide if needed

## Phase 3: Fix Compilation Errors 🔨

- [ ] T011 Fix naming conflicts in CYD_Multi_Compatible/SoftwareSPI.h - rename SPIMode enum to avoid Arduino conflicts
- [ ] T012 Fix DiagnosticState structure in CYD_Multi_Compatible/DiagnosticState.h - add missing member variables
- [ ] T013 Fix SerialCommands.cpp to use correct member names and constants from HardwareConfig
- [ ] T014 Update ComponentInitializer.cpp to remove TOUCH_CS and use IRQ-only
- [ ] T015 Compile CYD_Multi_Compatible successfully with `arduino-cli compile --fqbn esp32:esp32:esp32`

## Phase 4: Serial Command Implementation & Testing 📡

- [ ] T016 Upload fixed CYD_Multi_Compatible sketch to connected CYD device
- [ ] T017 Test DIAG command - verify JSON output matches contracts/serial-api.yaml schema
- [ ] T018 Test TEST_DISPLAY command - verify color bars appear on screen
- [ ] T019 Test TEST_TOUCH command - verify tap/double-tap detection via IRQ
- [ ] T020 Test TEST_RFID command - verify MFRC522 version detection

## Phase 5: Advanced Commands & Features ⚡

- [ ] T021 Test TEST_SD command - verify card detection and file operations
- [ ] T022 Test TEST_AUDIO command - verify I2S initialization
- [ ] T023 CALIBRATE command removed - touch uses IRQ-only, no calibration needed
- [ ] T024 Test WIRING command - verify correct ASCII art diagram for detected variant
- [ ] T025 Test VERSION command - verify firmware version display

## Phase 6: Hardware Adaptation Validation 🎯

- [ ] T026 Test GPIO27 multiplexing on dual USB variant - verify no backlight flicker during RFID ops
- [ ] T027 Test SCAN command with actual RFID card - verify <100ms response time
- [ ] T028 Test component graceful degradation - disconnect SD card and verify system continues
- [ ] T029 Power cycle device and verify system configuration persists from EEPROM
- [ ] T030 Test RESET command - verify system restarts properly

## Phase 7: Cross-Variant Testing 🔄

- [ ] T031 If single USB variant available, upload same sketch and verify functionality
- [ ] T032 Document any variant-specific behaviors or requirements
- [ ] T033 Update quickstart.md with actual test results and timing measurements
- [ ] T034 Create troubleshooting guide based on discovered issues

## Dependencies
- Hardware verification (T003-T006) MUST complete before any uploads
- Test sketches (T007-T010) validate hardware before main sketch
- Compilation fixes (T011-T015) required before serial command testing
- Basic commands (T016-T020) before advanced features (T021-T025)
- All testing before documentation updates (T032-T034)

## Parallel Execution Examples

### After hardware verification:
```bash
# Can test all component sketches in parallel:
Task: "Upload and test RFID sketch"
Task: "Upload and test SD card sketch"  
Task: "Upload and test audio sketch"
```

### After compilation fixes:
```bash
# Can test multiple serial commands simultaneously:
Task: "Test TEST_DISPLAY command"
Task: "Test TEST_TOUCH command"
Task: "Test DIAG command"
```

## Critical Path for Dual USB Testing
Since dual USB variant is connected, prioritize:
1. T004 - Display test (confirm ST7789 and GPIO27 backlight)
2. T007 - RFID test with GPIO27 multiplexing (most complex)
3. T026 - Validate multiplexing in main sketch
4. T027 - RFID scanning with real cards

## Success Criteria
- [ ] Hardware variant correctly detected
- [ ] All test sketches run successfully
- [ ] Main sketch compiles without errors
- [ ] All serial commands respond per specification
- [ ] RFID cards scan with GPIO27 multiplexing working
- [ ] No backlight flicker on dual USB variant
- [ ] Touch tap detection works reliably (IRQ-only mode)

## Known Issues to Fix
1. **SPI_MODE0 conflict**: Arduino core already defines this
2. **TOUCH_CS conflict**: TFT_eSPI User_Setup.h defines this
3. **DiagnosticState missing members**: touchInitialized, rfidInitialized, etc.
4. **HardwareConfig mismatches**: displayDriver vs displayDriverID
5. **MODEL_* constants**: Not defined, should use CYD_MODEL_*
6. **Touch uses IRQ-only detection**: XPT2046 SPI interface not connected on CYD
   - Touch IRQ (GPIO36) confirmed working for tap detection
   - Solution: Implement tap/double-tap using interrupt only (see ALNScanner0812Working)
7. **Serial Monitor**: Must use PowerShell script (reliable-serial-monitor.ps1) instead of arduino-cli monitor due to WSL2 limitations

## Command Reference for Testing
```bash
# Upload test sketch
arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32 test-sketches/display-test

# Monitor serial output (MUST use PowerShell, not arduino-cli monitor)
powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1

# Combined compile, upload and monitor workflow
arduino-cli compile --upload -p COM8 --fqbn esp32:esp32:esp32 test-sketches/display-test && sleep 1 && powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1

# Compile main sketch
arduino-cli compile --fqbn esp32:esp32:esp32 CYD_Multi_Compatible

# Upload main sketch
arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32 CYD_Multi_Compatible
```

---
*Updated to reflect actual implementation path using CYD_Multi_Compatible modular architecture*