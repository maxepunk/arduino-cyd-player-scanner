# CYD RFID Scanner Constitution

## Core Principles

### I. Hardware Constraints are Immutable
**Hardware limitations define software boundaries; workarounds must never compromise stability.**

- Pin assignments are fixed by CYD board design and CANNOT be changed without hardware modification
- GPIO27/speaker electrical coupling is a hardware flaw requiring software mitigation (not elimination)
- SPI bus sharing (VSPI for TFT+SD, Software SPI for RFID) is non-negotiable
- All code changes must respect physical electrical constraints
- Performance optimization cannot violate timing requirements of shared buses

**Rationale**: Attempting to "fix" hardware limitations in software leads to instability. Accept constraints, work within them.

### II. SPI Operations Must Never Deadlock (NON-NEGOTIABLE)
**SD card reads MUST complete before TFT locks are acquired.**

Critical rule for VSPI bus sharing:
```cpp
// ✅ CORRECT ORDER
File f = SD.open(path);
f.read(buffer, size);    // SD operation completes FIRST
f.close();
tft.startWrite();        // THEN acquire TFT lock
tft.pushImage(...);
tft.endWrite();

// ❌ WRONG - WILL DEADLOCK
tft.startWrite();
SD.open(path).read(...); // NEVER access SD while TFT locked
```

**Enforcement**:
- All BMP/image loading functions MUST read SD data into RAM first
- TFT operations MUST use pre-loaded buffers, never direct SD reads
- Code reviews MUST verify no SD calls between `startWrite()` and `endWrite()`
- Test sketches for new display features MUST include SD-based content

**Penalty**: System freeze requiring hard reset. This is the #1 stability issue.

### III. Library Modifications Must Be Documented and Isolated
**Modified libraries live in project-local `~/projects/Arduino/libraries/` and are version-controlled.**

Current patches:
- TFT_eSPI: ST7789 color inversion disabled (see CLAUDE.md for current line reference in `ST7789_Init.h`)

Requirements for all library modifications:
- Modified libraries MUST NOT be updated/reinstalled without reapplying patches
- All library modifications MUST be documented in CLAUDE.md with current file paths and line numbers
- Each patch MUST include:
  - Before/after code snippets
  - Root cause analysis explaining why modification is needed
  - Test validation procedure
  - Date of modification and library version modified

**Rationale**: Library updates can silently break critical fixes. Isolation prevents cascading failures. Documentation in CLAUDE.md allows patch locations to be updated as libraries evolve.

### IV. Graceful Degradation Over Crashes
**Component failure MUST NOT crash the system; log errors and continue with reduced functionality.**

Required for all hardware interfaces:
- Display initialization failure → Serial-only mode
- RFID module missing → Display continues working
- SD card missing → Show error on screen, continue scanning
- Touch not responding → Operate without touch input
- Audio failure → Silent operation with visual feedback

Implementation requirements:
- `begin()` failures return `false` and set error state
- Main loop checks component health flags before use
- Serial diagnostics (DIAG command) report all component states
- UI indicates missing components with status messages

**Enforcement**: Every new feature must demonstrate graceful failure in test sketch.

### V. Scan Frequency Directly Impacts User Experience
**RFID scan interval balances responsiveness vs. acceptable beeping levels caused by GPIO27/speaker electrical coupling.**

Hardware constraint:
- GPIO27 (RFID MOSI) is electrically coupled to the speaker circuit (CYD hardware flaw)
- Faster scanning increases beeping frequency (more SPI traffic)
- Slower scanning improves user comfort but reduces responsiveness
- Optimal interval must be determined through physical hardware testing with users

Requirements for scan timing changes:
- MUST test on physical CYD hardware with speaker enabled
- MUST validate with actual users (not developer judgment alone)
- Document rationale for timing change (e.g., "reduced beeping complaints", "improved card detection rate")
- Audio initialization should remain deferred until first playback to minimize startup beeping

**Rationale**: This is a hardware limitation requiring UX tradeoffs. Numbers change with usage patterns, but the balancing principle remains constant.

### VI. Display Orientation Follows BMP Storage Format
**BMP files store pixels bottom-to-top; row positioning must account for this.**

Critical implementation for BMP rendering:
```cpp
for (int y = height - 1; y >= 0; y--) {  // Iterate BACKWARDS
    f.read(rowBuffer, rowBytes);
    tft.setAddrWindow(0, y, width, 1);   // Position EACH row
    tft.pushPixels((uint16_t*)rowBuffer, pixelsInRow);
}
```

**Rules**:
- Never assume top-to-bottom BMP storage
- Each row MUST be positioned individually via `setAddrWindow()`
- Test with actual BMP files from SD card, not generated images
- Image test files MUST include recognizable orientation markers (text/arrows)

### VII. Test-First for Hardware Features
**Diagnostic test sketches validate hardware before integration into main sketch.**

Process for new features:
1. Create numbered test sketch in `test-sketches/` (e.g., `38-new-feature-test/`)
2. Isolate single hardware component or interaction
3. Test on physical hardware (no emulation)
4. User approval of test results
5. THEN integrate into `ALNScanner0812Working.ino`

Test sketch requirements:
- Single-purpose (test ONE thing)
- Serial output describes all steps
- Clear pass/fail indicators
- Minimal dependencies on other components

**Rationale**: Integration bugs are harder to diagnose. Prove components work in isolation first.

## Development Workflow

### Source of Truth Hierarchy
1. **Production Sketch**: `ALNScanner0812Working/ALNScanner0812Working.ino` - The only deployable code
2. **Documentation**: `CLAUDE.md` - Project status, fixes, and context
3. **Test Sketches**: `test-sketches/*` - Diagnostic and validation tools
4. **Archive**: DO NOT USE - deprecated code for reference only

**Rules**:
- All new features integrate into `ALNScanner0812Working.ino`
- No parallel "alternative" sketches for production use
- Test sketches never become production code
- Archive is read-only

### Compilation and Upload Standards
**Native Linux Arduino CLI on Raspberry Pi is the authoritative toolchain.**

Standard commands:
```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=default,UploadSpeed=921600 .

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Monitor
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Constraints**:
- Board: `esp32:esp32:esp32` (standard ESP32, not variants)
- Partition: `default` (no custom partition schemes without documentation)
- Upload speed: Use maximum stable speed for your USB connection (document if different from project default)
- Serial baud: Must match rate hardcoded in sketch (document changes in sketch version notes)
- Port: Linux native devices `/dev/ttyUSB*` or `/dev/ttyACM*` (not Windows COM ports)

**Deprecated**: WSL2/Windows bridge setup (see `archive/deprecated-scripts/`)

### Serial Debugging Protocol
**All sketches must implement comprehensive serial commands for diagnostics.**

Minimum required commands:
- `DIAG` - JSON diagnostics (all component states, versions, errors)
- `VERSION` - Firmware version string
- `WIRING` - Display pin configuration
- Component-specific tests: `TEST_DISPLAY`, `TEST_RFID`, `TEST_SD`, `TEST_TOUCH`, `TEST_AUDIO`

Format requirements:
- Commands are UPPERCASE, newline-terminated
- JSON output for machine parsing, human-readable for manual debugging
- Errors include component name, error code, and troubleshooting hint
- All diagnostics runnable on deployed hardware without recompilation

## Quality Gates

### Pre-Commit Checklist
Before any code changes:
- [ ] SPI order verified (SD reads before TFT locks)
- [ ] No library updates without reapplying patches
- [ ] Component failures tested (disconnect/disable hardware)
- [ ] Serial commands functional
- [ ] Test on actual CYD hardware (no emulation)
- [ ] `DIAG` command shows all-green or expected errors

### Code Review Focus Areas
Reviewers MUST verify:
1. **SPI Safety**: No SD access within `tft.startWrite()` / `tft.endWrite()` blocks
2. **Error Handling**: All `begin()` calls checked, failures logged
3. **Timing Changes**: Scan intervals or delays justified with user impact analysis
4. **Pin Changes**: REJECTED unless hardware modified (requires documentation)
5. **Library Mods**: Patches reapplied, documented, tested

### Integration Requirements
When merging test sketch code into production:
- Preserve existing error handling patterns
- Maintain serial command structure
- Update version number in sketch
- Test ALL existing features (regression check)
- Update `DIAG` output to include new component
- Document integration in commit message

## Hardware-Specific Standards

### Principle: Hardware Constraints Define Software Capabilities
**Physical pin assignments and electrical characteristics are fixed; software must adapt to hardware, not vice versa.**

Immutable hardware constraints (CYD ESP32 board):
- Display resolution: 240x320 (all content must fit or scale)
- SPI bus architecture: VSPI shared between TFT and SD (see Principle II)
- RFID pins: Software SPI on GPIO 22, 27, 35, 3 (cannot change without rewiring)
- Audio pins: I2S on GPIO 26, 25, 22 (hardware fixed)
- GPIO27/speaker coupling: Electrical flaw requiring software mitigation (see Principle V)

### Principle: Content Organization Must Be Predictable
**File naming and directory structure enable reliable lookups; schemes may evolve but must remain deterministic.**

Requirements for content organization:
- Card identifiers must map 1:1 to file names (no ambiguity)
- Directory structure must be documented in CLAUDE.md
- File naming changes require migration plan for existing content
- Case sensitivity must be consistent (filesystem agnostic)

Current implementation (see CLAUDE.md for latest):
- Card ID format: Defined in current sketch version
- Directory structure: `/images/` and `/audio/` (subject to reorganization)
- BMP dimensions: Match display resolution (240x320) or implement scaling
- File naming: 8.3 compatible for FAT32 compatibility

### Principle: Audio Initialization Timing Affects User Experience
**Defer resource-intensive initialization until needed; balance startup speed vs. first-use latency.**

- Audio hardware should initialize on-demand (first playback) to minimize boot beeping
- Initialization failure must not block other functionality (see Principle IV)
- Supported audio formats documented in CLAUDE.md (may expand with codec support)

### Principle: Display Configuration Must Match Hardware Variant
**ST7789 controllers may require different initialization; test on actual hardware, document deviations.**

- Color inversion varies by manufacturer (see Principle III for library patches)
- Display orientation must account for physical board layout (USB port position)
- Backlight control should be configurable (PWM capable pin, brightness adjustable)
- Color format determined by TFT_eSPI library (currently RGB565)

## Governance

### Constitution Authority
- This constitution supersedes ad-hoc coding practices
- Violations require justification documented in commit messages
- Emergency fixes may bypass process but MUST be reviewed post-deployment
- All new contributors MUST read CLAUDE.md and this constitution

### Amendment Process
1. Propose change with rationale (why existing principle fails)
2. Test proposed change on hardware (no theoretical amendments)
3. Document impact on existing code
4. Update constitution with version increment
5. Update CLAUDE.md with amendment date
6. Notify all active developers

### Deprecation Policy
- Deprecated code moves to `archive/` with explanation
- Archive is read-only (no modifications, only reference)
- Deprecated practices documented in CLAUDE.md with alternatives
- Old approaches never reintroduced without evidence of failure

### Emergency Overrides
Allowed only for:
- Critical security vulnerabilities
- Hardware-damaging bugs (overheating, electrical damage)
- Data loss prevention

Emergency fixes:
- Implement immediately
- Document in commit as `[EMERGENCY]`
- Create post-mortem spec in `specs/` within 48 hours
- Update constitution if systemic issue

**Version**: 1.0.0 | **Ratified**: 2025-10-18 | **Last Amended**: 2025-10-18
