
# Implementation Plan: CYD Display Configuration Setup

**Branch**: `001-we-are-trying` | **Date**: 2025-09-19 | **Spec**: `/specs/001-we-are-trying/spec.md`
**Input**: Feature specification from `/specs/001-we-are-trying/spec.md`

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
Fix ALNScanner0812Working.ino's complete failure on ST7789 CYD variant (serial loss, RFID non-responsive). Through systematic testing, we've identified this is NOT a display driver issue but a software SPI implementation problem where critical sections block interrupts too long, causing UART buffer overflow.

## Technical Context
**Language/Version**: C++/Arduino (ESP32-Arduino Core 2.x)  
**Primary Dependencies**: TFT_eSPI (display - compile-time config), MFRC522, ESP8266Audio, SD  
**Storage**: SD card via hardware SPI (images/audio)  
**Testing**: Compile both variants, test on actual hardware, serial debug at 115200  
**Target Platform**: ESP32-WROOM-32 on CYD 2.8" boards (focus on ST7789 variant)
**Project Type**: single (embedded - software SPI timing fix)  
**Performance Goals**: Maintain RFID functionality while fixing serial communication  
**Constraints**: GPIO3 is both RFID_SS and UART RX; critical sections block interrupts  
**Scale/Scope**: Fix software SPI implementation in ALNScanner (lines 87-116, 965-1031)
**Engineering Approach**: Minimize critical section duration or find alternative approach

## Constitution Check
*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Arduino CYD Project Gates (from Constitution v2.1.0)
- [x] Display Configuration First: EVOLVED - Issue is actually software SPI, not display
- [x] Simplicity Over Complexity: YES - Fix critical sections, don't rewrite SPI
- [x] Diagnostic-First: YES - Created systematic test suite (tests 01-15)
- [x] Graceful Degradation: MAINTAINING - Fix must preserve RFID functionality
- [x] Test Coverage: YES - Comprehensive test plan to isolate root cause
- [x] Verification: YES - Will test each hypothesis systematically
- [x] Environment: YES - Using established WSL2→Windows Arduino CLI workflow
- [x] Workspace Hygiene: YES - Created organized test-sketches directory

**Updated Interpretation**: Initial diagnosis was wrong (display issue). Through testing we found the real problem (software SPI critical sections). Constitution principles still apply but to different problem domain.

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

**Structure Decision**: [DEFAULT to Option 1 unless Technical Context indicates web/mobile app]

## Phase 0: Outline & Research (COMPLETED)
1. **Identified the ACTUAL problem**:
   - NOT a display driver issue (test 10 proved libraries work)
   - Software SPI critical sections block interrupts too long
   - GPIO3 conflict (RFID_SS vs UART RX)
   - UART buffer overflow from cumulative interrupt blocking

2. **Research completed**:
   - Tested basic components individually (tests 01-05) ✓
   - Tested library combinations (tests 08-10) ✓
   - Identified critical section timing as root cause
   - Calculated UART overflow threshold (~11ms)

3. **Engineering decisions documented**:
   - Primary: Fix critical sections (minimize duration)
   - Secondary: Alternative SS pin (requires rewiring)
   - Fallback: Protocol correction if SPI Mode 0 wrong

**Output**: research.md, troubleshooting-findings.md completed

## Phase 1: Design & Contracts (UPDATED)
*Prerequisites: research.md complete*

1. **Technical Model** → `data-model.md`:
   - Software SPI timing parameters ✓
   - Hardware pin conflicts documented ✓
   - MFRC522 communication requirements ✓

2. **Test Protocols** → `tasks.md`:
   - Test 11-15 systematic diagnostic plan ✓
   - Progressive isolation approach ✓
   - Expected results for each test ✓

3. **Quickstart Guide** → `quickstart.md`:
   - Diagnostic test sequence ✓
   - Potential solutions based on results ✓
   - No misleading "just change driver" advice ✓

4. **Documentation Updates**:
   - CLAUDE.md updated with findings ✓
   - troubleshooting-findings.md created ✓
   - All 001 documents aligned with reality ✓

**Output**: All design documents updated to reflect software SPI issue

## Phase 2: Task Planning Approach (READY FOR EXECUTION)
*This section describes what needs to happen next*

**Next Steps** (tests to write and execute):
1. Write Test 11: SPI Protocol Verification
2. Write Test 12: MFRC522 Version Register Read
3. Write Test 13: Critical Section Impact Measurement
4. Write Test 14: Alternative SPI Strategies
5. Write Test 15: Progressive MFRC522 Init
6. Execute tests systematically
7. Apply fix based on results
8. Verify RFID functionality preserved

**Test-Driven Approach**:
- Each test isolates specific hypothesis
- Results guide solution implementation
- No guessing - data-driven decisions

**Time Estimate**: 30-45 minutes for test execution and analysis

**Status**: Ready to write and execute diagnostic tests

## Phase 3+: Future Implementation
*These phases are beyond the scope of the /plan command*

**Phase 3**: Task execution (/tasks command creates tasks.md)  
**Phase 4**: Implementation (execute tasks.md following constitutional principles)  
**Phase 5**: Validation (run tests, execute quickstart.md, performance validation)

## Complexity Tracking
*Fill ONLY if Constitution Check has violations that must be justified*

**NO VIOLATIONS** - This implementation follows the constitution's core principle:
"Simplicity Over Complexity"

- ✅ One line config change (display driver)
- ✅ Maybe one line code change (invertDisplay)
- ✅ No architecture changes
- ✅ No new abstractions
- ✅ Preserves all working RFID code


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
- [x] Initial Constitution Check: PASS (pragmatic interpretation)
- [x] Post-Design Constitution Check: PASS (minimal complexity)
- [x] All NEEDS CLARIFICATION resolved
- [x] Complexity deviations documented (none needed)

---
*Based on Constitution v2.1.0 - See `.specify/memory/constitution.md`*
