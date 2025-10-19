
# Implementation Plan: CYD Multi-Hardware Compatibility Testing and Verification

**Branch**: `002-we-were-working` | **Date**: 2025-09-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-we-were-working/spec.md`

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
6. Execute Phase 1 → contracts, data-model.md, quickstart.md, agent-specific template file (e.g., `CLAUDE.md` for Claude Code, `.github/copilot-instructions.md` for GitHub Copilot, `GEMINI.md` for Gemini CLI, `QWEN.md` for Qwen Code or `AGENTS.md` for opencode).
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
VerTfying and finalizing a multi-hardware compatible RFID scanner sketch for Cheap Yellow Display (CYD) modules. The system needs to work on both single USB (micro) and dual USB (micro + Type-C) hardware variants, with comprehensive diagnostic capabilities to distinguish hardware from software issues. Primary goal is testing the rebuilt sketch on the currently connected dual port module and ensuring full functionality across all CYD variants.

## Technical Context
**Language/Version**: C++/Arduino Framework (ESP32-WROOM-32)  
**Primary Dependencies**: TFT_eSPI, XPT2046_Touchscreen, MFRC522, ESP8266Audio, SD, ArduinoJson  
**Storage**: SD card (hardware SPI), EEPROM for configuration  
**Testing Environment**: WSL2→Windows Arduino CLI bridge
  - Arduino CLI runs on Windows, accessed from WSL2 Ubuntu
  - Serial ports are Windows COM ports (COM3-COM8)
  - Path translation: `/home/spide/` → `\\wsl.localhost\Ubuntu-22.04\home\spide\`
  - Sequential compilation required (Windows file locking)
**Target Platform**: ESP32 CYD 2.8" resistive touch displays (ILI9341/ST7789)
**Project Type**: embedded/single - Arduino sketch  
**Performance Goals**: Instant RFID scan response (<100ms), smooth audio playback  
**Constraints**: Zero wiring changes between hardware variants, GPIO27 multiplexing for backlight/RFID, software SPI for RFID timing  
**Scale/Scope**: Single sketch supporting multiple hardware variants, comprehensive diagnostics

## Constitution Check
*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Compliance with Arduino CYD Scanner Constitution v1.0.0:**
- [x] Hardware Abstraction First: Runtime detection via display driver ID implemented
- [x] Zero Wiring Change Policy: GPIO27 multiplexing solution maintains pin compatibility
- [x] Diagnostic-First Development: Comprehensive serial API and test sketches planned
- [x] Graceful Degradation: Component failure handling with fallback behavior
- [x] Test Coverage Completeness: Both hardware variants covered in test plan

## Project Structure

### Documentation (this feature)
```
specs/[###-feature]/
├── plan.md              # This file (/plan command output)
├── research.md          # Phase 0 output (/plan command)
├── data-model.md        # Phase 1 output (/plan command)
├── quickstart.md        # Phase 1 output (/plan command)
├── contracts/           # Phase 1 output (/plan command)
└── tasks.md             # Phase 2 output (/tasks command - NOT created by /plan)
```

### Source Code (repository root)
```
# Option 1: Single project (DEFAULT)
src/
├── models/
├── services/
├── cli/
└── lib/

tests/
├── contract/
├── integration/
└── unit/

# Option 2: Web application (when "frontend" + "backend" detected)
backend/
├── src/
│   ├── models/
│   ├── services/
│   └── api/
└── tests/

frontend/
├── src/
│   ├── components/
│   ├── pages/
│   └── services/
└── tests/

# Option 3: Mobile + API (when "iOS/Android" detected)
api/
└── [same as backend above]

ios/ or android/
└── [platform-specific structure]
```

**Structure Decision**: Arduino project structure (embedded system)
```
CYD_Multi_Compatible/             # ACTIVE - Modular implementation
├── CYD_Multi_Compatible.ino     # Main sketch
├── SerialCommands.cpp/h         # Command handlers
├── HardwareDetector.cpp/h       # Variant detection  
├── GPIO27Manager.cpp/h          # Multiplexing logic
├── ComponentInitializer.cpp/h   # Component init
├── DiagnosticReporter.cpp/h     # Diagnostics
├── SoftwareSPI.cpp/h            # RFID software SPI
└── CalibrationManager.cpp/h     # Touch calibration

test-sketches/                    # Component tests
├── hardware-detect/
├── display-test/
├── touch-test/
├── rfid-test/
├── sd-test/
└── audio-test/

ALNScanner0812Working/            # LEGACY - Not used
└── ALNScanner0812Working.ino
```

## Phase 0: Outline & Research
1. **Extract unknowns from Technical Context** above:
   - For each NEEDS CLARIFICATION → research task
   - For each dependency → best practices task
   - For each integration → patterns task

2. **Generate and dispatch research agents**:
   ```
   For each unknown in Technical Context:
     Task: "Research {unknown} for {feature context}"
   For each technology choice:
     Task: "Find best practices for {tech} in {domain}"
   ```

3. **Consolidate findings** in `research.md` using format:
   - Decision: [what was chosen]
   - Rationale: [why chosen]
   - Alternatives considered: [what else evaluated]

**Output**: research.md with all NEEDS CLARIFICATION resolved

## Phase 1: Design & Contracts ✅ COMPLETE
*Prerequisites: research.md complete*

1. **Extract entities from feature spec** → `data-model.md`: ✅
   - HardwareVariant, ComponentStatus, TouchCalibration entities defined
   - Validation rules and state transitions documented
   - EEPROM persistence layout specified

2. **Generate API contracts** from functional requirements: ✅
   - Serial command API defined in OpenAPI format
   - Commands: DIAG, TEST_*, CALIBRATE, WIRING, SCAN, RESET, VERSION
   - Output to `/contracts/serial-api.yaml`

3. **Test scenarios extracted**: ✅
   - Component isolation tests defined
   - Hardware variant detection tests
   - Integration test flow in quickstart

4. **Quickstart guide created**: ✅
   - Step-by-step testing procedure
   - Expected outputs documented
   - Troubleshooting guide included

5. **Agent file updated**: ✅
   - CLAUDE.md updated with project context
   - Testing commands documented
   - Recent changes tracked

**Output**: data-model.md ✅, /contracts/serial-api.yaml ✅, quickstart.md ✅, CLAUDE.md ✅

## Phase 2: Task Planning Approach
*This section describes what the /tasks command will do - DO NOT execute during /plan*

**Task Generation Strategy**:
- Load `.specify/templates/tasks-template.md` as base
- Generate tasks from Phase 1 design docs (contracts, data model, quickstart)
- Hardware detection test sketch → Task 1
- Component test sketches (5) → Tasks 2-6 [P]
- Serial command handlers → Tasks 7-12
- Main sketch hardware adaptation → Tasks 13-18
- Integration testing → Tasks 19-22
- Documentation updates → Task 23

**Ordering Strategy**:
- Test sketches first (enable debugging)
- Serial API implementation (diagnostic capability)
- Main sketch modifications (core functionality)
- Integration and validation last
- Mark [P] for parallel test sketch development

**Estimated Output**: 23 numbered, ordered tasks in tasks.md

**Key Task Categories**:
1. **Diagnostic Infrastructure** (Tasks 1-6): Individual component test sketches
2. **Serial Command API** (Tasks 7-12): Implement commands per contract
3. **Hardware Adaptation** (Tasks 13-16): GPIO27 mux, variant detection, EEPROM calibration
4. **Main Sketch Integration** (Tasks 17-18): Merge all capabilities
5. **Validation** (Tasks 19-22): Test on both hardware variants
6. **Documentation** (Task 23): Update user guides

**IMPORTANT**: This phase is executed by the /tasks command, NOT by /plan

## Phase 3+: Future Implementation
*These phases are beyond the scope of the /plan command*

**Phase 3**: Task execution (/tasks command creates tasks.md)  
**Phase 4**: Implementation (execute tasks.md following constitutional principles)  
**Phase 5**: Validation (run tests, execute quickstart.md, performance validation)

## Complexity Tracking
*Fill ONLY if Constitution Check has violations that must be justified*

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |


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
- [x] Initial Constitution Check: PASS (adapted for embedded project)
- [x] Post-Design Constitution Check: PASS
- [x] All NEEDS CLARIFICATION resolved
- [ ] Complexity deviations documented (N/A - no violations)

---
*Based on Constitution v2.1.1 - See `/memory/constitution.md`*
