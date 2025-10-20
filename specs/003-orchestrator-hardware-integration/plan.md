# Implementation Plan: Orchestrator Integration for Hardware Scanner

**Branch**: `003-orchestrator-hardware-integration` | **Date**: 2025-10-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/003-orchestrator-hardware-integration/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Port orchestrator integration features from the PWA (aln-memory-scanner) to the ESP32 hardware scanner. The scanner will communicate with a central orchestrator server via WiFi to log all gameplay activity and trigger synchronized video playback. Network operations are invisible and non-blocking - scans that cannot be sent immediately are queued on SD card and synchronized later without player awareness. Critical requirements include: WiFi configuration via SD card config file, offline queue with automatic sync, connection monitoring, and graceful degradation when network is unavailable.

## Technical Context

**Language/Version**: C++ (Arduino framework) for ESP32 core v3.3.2
**Primary Dependencies**: ESP32 WiFi library, HTTPClient, ArduinoJson, SD library, existing TFT_eSPI, MFRC522, ESP8266Audio
**Storage**: SD card (FAT32 filesystem) for config files (`/config.txt`), queue persistence (`/queue.jsonl`), token database cache (`/tokens.json`), and media assets
**Testing**: Serial debugging commands (DIAG, TEST_*), physical hardware testing on CYD ESP32 board, test sketches in `test-sketches/` directory
**Target Platform**: ESP32 microcontroller (240MHz dual-core) on CYD 2.8" display board (ST7789 variant)
**Project Type**: Embedded hardware (single monolithic sketch `ALNScanner0812Working.ino`)
**Performance Goals**: <2s scan-to-orchestrator send latency, <1s local content display, <500ms queue write, <30s WiFi connection, <60s queue sync for 30 scans
**Constraints**: SPI bus sharing (VSPI for TFT+SD, Software SPI for RFID), GPIO27/speaker coupling (hardware flaw), 240x320 display resolution, limited ESP32 RAM (~300KB usable), 5-second HTTP timeout, offline-first operation
**Scale/Scope**: Single-device embedded system, WiFi networking to single orchestrator server, queue capacity 100 scans, token database <50KB JSON

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Principle I: Hardware Constraints are Immutable
✅ **PASS** - Feature adds WiFi/HTTP networking software layer. No pin changes, no hardware modifications. Works within existing ESP32 capabilities (WiFi radio, dual-core processing).

### Principle II: SPI Operations Must Never Deadlock (CRITICAL)
⚠️ **REQUIRES CAREFUL DESIGN** - Queue operations will read/write SD card (`/queue.jsonl`, `/config.txt`, `/tokens.json`). **MUST ensure**:
- All SD reads/writes for queue management complete BEFORE acquiring TFT locks
- Queue writes happen during "background" time (not during active BMP display)
- Status screen display (showing queue count) reads queue metadata from RAM cache, not from SD during TFT rendering
- Token database JSON loaded from SD into RAM at boot, parsed outside of TFT lock context

**Mitigation Strategy**: Use FreeRTOS task separation - background task handles SD I/O for queue/config/sync, main task handles display. Shared state via mutex-protected variables in RAM.

### Principle III: Library Modifications Must Be Documented and Isolated
✅ **PASS** - Using standard ESP32 Arduino libraries (WiFi, HTTPClient, ArduinoJson). No library modifications required. Existing TFT_eSPI modification (ST7789 color inversion fix) unaffected by this feature.

### Principle IV: Graceful Degradation Over Crashes
✅ **PASS** - This feature IS the graceful degradation mechanism:
- WiFi failure → Scanner continues with offline queue mode
- Orchestrator unreachable → Scans queued to SD card, uploaded later
- Config file missing → Display error, continue with RFID scanning
- Token sync failure → Use cached database from SD card
- Queue write failure (SD card removed) → Display error, attempt direct send if WiFi available

### Principle V: Scan Frequency Directly Impacts User Experience
✅ **PASS** - Feature does not modify RFID scan timing (currently 500ms interval). Network operations are fire-and-forget or queued, never block scanning loop.

### Principle VI: Display Orientation Follows BMP Storage Format
✅ **PASS** - Feature does not affect BMP rendering. Only adds status text overlays ("Sending...", "Queued", "Connected", etc.) which use text rendering, not BMP logic.

### Principle VII: Test-First for Hardware Features
⚠️ **REQUIRED** - Must create test sketches BEFORE integration:
1. `test-sketches/XX-wifi-connect/` - Test WiFi connection from config file
2. `test-sketches/XX-http-client/` - Test HTTP POST to orchestrator endpoints
3. `test-sketches/XX-json-parse/` - Test ArduinoJson parsing of token database
4. `test-sketches/XX-queue-file/` - Test JSONL queue append/read/delete operations on SD card
5. `test-sketches/XX-background-sync/` - Test FreeRTOS task for connection monitoring and queue upload

**Gate Status**: ⚠️ **CONDITIONAL PASS** - May proceed to Phase 0 research, but implementation BLOCKED until:
- Test sketches validate WiFi, HTTP, JSON, queue file I/O independently
- SPI deadlock mitigation strategy (FreeRTOS task separation) validated in test sketch
- User approval of test results before integration into `ALNScanner0812Working.ino`

## Project Structure

### Documentation (this feature)

```
specs/003-orchestrator-hardware-integration/
├── spec.md              # Feature specification (user input)
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
│   ├── config-format.md # Config file specification
│   ├── queue-format.md  # Queue JSONL specification
│   └── api-client.md    # Orchestrator API client contract
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

**Embedded Hardware Project Structure** (Arduino sketch-based):

```
~/projects/Arduino/
├── ALNScanner0812Working/              # ✅ PRODUCTION SKETCH (integration target)
│   └── ALNScanner0812Working.ino       # v3.4 → v4.0 (orchestrator integration)
│       ├── [Existing code sections]
│       │   ├── Pin definitions
│       │   ├── Hardware initialization (TFT, RFID, SD, Audio, Touch)
│       │   ├── RFID scanning loop
│       │   ├── BMP display functions
│       │   ├── Audio playback
│       │   └── Serial debugging commands
│       └── [NEW code sections for this feature]
│           ├── WiFi configuration and connection management
│           ├── Config file parser (/config.txt)
│           ├── Token database loader (/tokens.json)
│           ├── HTTP client for orchestrator API
│           ├── Queue manager (JSONL append/read/batch)
│           ├── FreeRTOS background task (connection monitor + sync)
│           ├── Status screen display (WiFi/orchestrator/queue info)
│           └── Processing image display ("Sending..." modal)
│
├── test-sketches/                       # 🧪 TEST SKETCHES (validation before integration)
│   ├── [Existing 37 diagnostic sketches]
│   ├── 38-wifi-connect/                # NEW: WiFi connection from config
│   │   └── 38-wifi-connect.ino
│   ├── 39-http-client/                 # NEW: HTTP POST to orchestrator
│   │   └── 39-http-client.ino
│   ├── 40-json-parse/                  # NEW: ArduinoJson token database parsing
│   │   └── 40-json-parse.ino
│   ├── 41-queue-file/                  # NEW: JSONL queue operations on SD
│   │   └── 41-queue-file.ino
│   └── 42-background-sync/             # NEW: FreeRTOS task for queue sync
│       └── 42-background-sync.ino
│
├── libraries/                           # Project-local Arduino libraries
│   ├── TFT_eSPI/                       # ⚠️ MODIFIED (existing ST7789 fix)
│   ├── MFRC522/                        # Existing RFID library
│   ├── ESP8266Audio/                   # Existing audio library
│   ├── SD/                             # ESP32 core SD library (standard)
│   ├── WiFi/                           # ESP32 core WiFi library (standard)
│   ├── HTTPClient/                     # ESP32 core HTTP library (standard)
│   └── ArduinoJson/                    # NEW: JSON parsing library (v6 or v7)
│
├── specs/                               # Feature specifications and implementation docs
│   ├── 001-we-are-trying/              # ST7789 fix documentation
│   ├── 002-audio-debug-project/        # RFID beeping fix documentation
│   └── 003-orchestrator-hardware-integration/  # THIS FEATURE
│
└── [Other project files: CLAUDE.md, constitution, hardware specs, etc.]
```

**Structure Decision**: Embedded Arduino project with monolithic sketch architecture. All new networking code integrates directly into `ALNScanner0812Working.ino` following the project's established pattern of adding functional sections within a single `.ino` file. Test-first approach validates each capability independently in `test-sketches/` before integration. No separate source directories - Arduino sketch model co-locates all code in single compilation unit.

## Complexity Tracking

*Fill ONLY if Constitution Check has violations that must be justified*

**No constitutional violations.** All principles pass or have mitigation strategies identified. Gate status is CONDITIONAL PASS - may proceed to research phase with requirement to create test sketches before implementation.

---

## Post-Design Constitution Re-Evaluation

**Date**: 2025-10-19
**Phase**: After Phase 1 (research.md, data-model.md, contracts, quickstart.md complete)
**Status**: ✅ ALL GATES PASS

### Principle II: SPI Operations Must Never Deadlock (CRITICAL)

**Initial Status**: ⚠️ REQUIRES CAREFUL DESIGN

**Post-Design Resolution**: ✅ **PASS - Mitigation Strategy Validated**

**Design Decisions**:
1. **FreeRTOS Task Separation** (research.md, data-model.md):
   - Core 0 (Background Task): All SD card operations for queue, config, token database
   - Core 1 (Main Task): TFT display rendering, RFID scanning
   - Mutex protection for SD card access between tasks

2. **SD Read/Write Sequencing** (contracts/queue-format.md, api-client.md):
   - Queue writes: Open file → Write line → Flush → Close immediately (no TFT locks held)
   - Queue reads: Read batch into RAM → Close file → Then upload via HTTP (no SD access during network I/O)
   - Token database: Loaded into RAM on boot (parsed outside TFT rendering context)
   - Configuration: Read once on boot before TFT operations begin

3. **RAM Caching for Status Display** (data-model.md):
   - Queue size cached in RAM variable (updated on append/remove)
   - Status screen reads RAM cache, not SD card during TFT rendering
   - Avoids SD access between `tft.startWrite()` and `tft.endWrite()`

**Validation Required**: Test sketches (42-background-sync, 41-queue-file) MUST demonstrate SD I/O and TFT rendering can coexist without deadlock.

**Gate Status**: ✅ **PASS** - Architecture prevents SPI deadlock via task separation and SD-first sequencing

---

### Principle VII: Test-First for Hardware Features

**Initial Status**: ⚠️ REQUIRED - Must create test sketches BEFORE integration

**Post-Design Resolution**: ✅ **PASS - Test Suite Defined**

**Test Sketches Planned** (see Project Structure in plan.md):

1. **38-wifi-connect/** - WiFi connection from config file
   - Validates: Configuration parser, WiFi.begin(), event-driven reconnection
   - Success criteria: Connect to configured WiFi within 30s, display "Connected"

2. **39-http-client/** - HTTP POST to orchestrator endpoints
   - Validates: HTTPClient library, 5s timeout, request/response handling
   - Success criteria: Send POST /api/scan, receive 200 OK, handle timeout gracefully

3. **40-json-parse/** - ArduinoJson token database parsing
   - Validates: JSON deserialization, filtered fields, memory usage
   - Success criteria: Parse 50-token database (50KB), extract video/image/audio fields, <20KB RAM usage

4. **41-queue-file/** - JSONL queue operations on SD card
   - Validates: Append, read, remove operations, FIFO overflow, corruption recovery
   - Success criteria: Append 100 entries, read batch of 10, handle corrupt lines gracefully

5. **42-background-sync/** - FreeRTOS task for queue sync
   - Validates: Dual-core task separation, mutex protection, SD-TFT coexistence
   - Success criteria: Background task uploads queue while main task renders TFT, no SPI deadlock

**Implementation Blocker**: ⚠️ Integration into `ALNScanner0812Working.ino` BLOCKED until all 5 test sketches pass user approval.

**Gate Status**: ✅ **PASS** - Test suite defined, matches test-first principle

---

### All Other Principles

**Principles I, III, IV, V, VI**: ✅ **PASS** - No changes from initial evaluation

- **Principle I** (Hardware Constraints): No pin changes, uses built-in WiFi radio
- **Principle III** (Library Modifications): No modifications, uses standard ESP32 libraries
- **Principle IV** (Graceful Degradation): Offline queue IS the graceful degradation
- **Principle V** (Scan Frequency): No changes to RFID scan timing (500ms interval)
- **Principle VI** (Display Orientation): No changes to BMP rendering logic

---

## Final Gate Status

✅ **ALL GATES PASS** - Ready to proceed to Phase 2 (implementation planning with /speckit.tasks)

**Conditions for Implementation**:
1. Create 5 test sketches (38-wifi-connect through 42-background-sync)
2. Validate each test sketch on physical CYD hardware
3. Obtain user approval for each test result
4. ONLY THEN integrate into `ALNScanner0812Working.ino`

**Next Command**: `/speckit.tasks` to generate implementation task breakdown

---

**Planning Complete**: 2025-10-19
**Phase 1 Status**: ✅ Complete - All design artifacts generated, Constitution validated

