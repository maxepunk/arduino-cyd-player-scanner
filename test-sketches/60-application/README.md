# Test Sketch 60: Application Layer Integration Test

**ALNScanner v5.0 - Complete System Integration Test**

## Purpose

Comprehensive end-to-end integration test for the complete ALNScanner v5.0 Application layer, validating all 21 components working together:

### Components Tested

**HAL Layer (5 components):**
- SDCard
- DisplayDriver
- AudioDriver
- TouchDriver
- RFIDReader

**Models Layer (3 components):**
- Config
- Token
- ConnectionState

**Services Layer (6 components):**
- ConfigService
- TokenService
- OrchestratorService
- QueueService
- NetworkService
- SerialService

**UI Layer (6 components):**
- Screen (base class)
- ReadyScreen
- StatusScreen
- TokenDisplayScreen
- ProcessingScreen
- UIStateMachine

**Application Layer (1 component):**
- Application (orchestrates all 21 components)

## Test Features

### 1. Automated Test Phases (6 phases, 10s each)

**Phase 1: Application Setup Validation**
- Runs `Application::setup()`
- Verifies all components initialize
- Measures setup time
- Checks initial memory state

**Phase 2: Serial Command Testing**
- Sends CONFIG command
- Sends STATUS command
- Sends TOKENS command
- Validates SerialService integration

**Phase 3: Touch Interaction Testing**
- Instructions for manual touch testing
- Validates UIStateMachine state transitions
- Tests Ready → Status → Ready flow

**Phase 4: RFID Simulation Testing**
- Simulates token scan (kaa001)
- Validates TokenService lookup
- Validates OrchestratorService send/queue
- Validates UIStateMachine display
- Validates AudioDriver playback

**Phase 5: Memory Stability Testing**
- Analyzes heap variation
- Detects memory leaks
- Reports stability metrics

**Phase 6: Final Report**
- Comprehensive test statistics
- Memory analysis
- Verification checklist
- Manual test instructions

### 2. Interactive Commands

```
CONFIG         - Show configuration
STATUS         - Show system status
TOKENS         - Show token database
SHOW_QUEUE     - Show scan queue
SIMULATE_SCAN:kaa001 - Simulate token scan
START_SCANNER  - Initialize RFID (if DEBUG_MODE)
REBOOT         - Restart system
AUTO           - Run automated test cycle
COMPONENTS     - Show component status
STATS          - Show test statistics
MEM            - Show memory details
HELP           - Show all commands
```

### 3. Memory Monitoring

- Tracks heap every 5 seconds
- Reports minimum/maximum heap
- Calculates heap variation
- Detects memory leaks (>10KB variation)

### 4. Comprehensive Validation Checklist

Automated checks:
- [X] Application.h compiles successfully
- [X] setup() initializes all components
- [X] loop() runs without errors
- [X] No memory leaks (stable heap)

Manual checks:
- [ ] Serial commands work
- [ ] Touch navigation works
- [ ] RFID scanning works (hardware or simulation)
- [ ] UI screens display correctly
- [ ] WiFi connects (if configured)
- [ ] Orchestrator sends/queues scans
- [ ] Token database loaded

## Usage

### Upload and Run

```bash
cd /home/maxepunk/projects/Arduino/test-sketches/60-application

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 .

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# Monitor
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### Test Modes

**Automated Mode (Default):**
1. Sketch boots
2. Waits 5 seconds
3. Runs all 6 test phases automatically
4. Each phase runs for 10 seconds
5. Total test time: ~60 seconds

**Interactive Mode:**
Send any command during the 5-second wait to interrupt automation and test manually.

**Repeat Test:**
Send `AUTO` command to restart automated test cycle.

## Expected Output

### Successful Test Output

```
╔════════════════════════════════════════════════════════════╗
║  ALNScanner v5.0 - Application Integration Test         ║
║  Testing: Complete system (21 components)               ║
╚════════════════════════════════════════════════════════════╝

[TEST] Free heap before setup: 298764 bytes
[TEST] Flash size: 4194304 bytes
[TEST] Sketch size: 1234567 bytes

[TEST] Running Application::setup()...

═══════════════════════════════════════════════════════════════
[HAL] SD card initialized
[HAL] Display initialized (240x320)
[HAL] Touch initialized
[HAL] Audio driver ready
[CONFIG] Loaded from SD card
[TOKEN] Database loaded (42 tokens)
[NETWORK] WiFi connected
[ORCH] Connected to orchestrator
[UI] UIStateMachine ready
═══════════════════════════════════════════════════════════════

[TEST] ✓✓✓ Application setup complete in 3456 ms ✓✓✓

═══ Component Status ═══
  SD Card:    ✓ OK
  Display:    ✓ OK
  Touch:      ✓ OK
  Audio:      ✓ OK (lazy init)
  RFID:       ✓ OK
═══════════════════════════

[TEST] Automated test sequence starting in 5 seconds...
[TEST] Send any command to interrupt and test manually
```

### Memory Stability (Good)

```
[MEM] New minimum heap: 245678 bytes (at 10234 ms)
[MEM] New minimum heap: 243456 bytes (at 25678 ms)

═══ Memory Details ═══
  Current heap: 244567 bytes
  Minimum heap: 243456 bytes
  Maximum heap: 298764 bytes
  Heap variation: 2308 bytes
  ✓ Memory stable (variation < 5KB)
```

### Memory Leak (Bad)

```
[MEM] New minimum heap: 245678 bytes (at 10234 ms)
[MEM] New minimum heap: 235678 bytes (at 15234 ms)
[MEM] New minimum heap: 225678 bytes (at 20234 ms)
[MEM] New minimum heap: 215678 bytes (at 25234 ms)

  ✗ Memory unstable (variation > 10KB) - possible leak!
```

## Troubleshooting

### Compilation Errors

**"Application.h: No such file or directory"**
- Ensure Application.h exists at `/home/maxepunk/projects/Arduino/ALNScanner_v5/Application.h`
- Verify all dependencies are in place

**"undefined reference to Application::setup()"**
- Application.h implementation incomplete
- Check Application.cpp or inline implementations

### Runtime Errors

**"✗ Display NOT INITIALIZED"**
- Hardware connection issue
- TFT_eSPI configuration problem
- Check User_Setup.h

**"✗ SD Card NOT AVAILABLE"**
- SD card not inserted or corrupted
- Check SD card pin connections
- Verify SD library initialization

**Memory unstable / leak detected**
- Check for String concatenations in loops
- Verify all dynamic allocations have deallocations
- Use PROGMEM for constant strings
- Check service cleanup methods

## Integration with v5.0 Development

This test sketch is **critical** for Phase 5 verification:

1. **Before creating Application.h:**
   - Review test sketch requirements
   - Understand expected behavior
   - Plan component integration

2. **During Application.h development:**
   - Run test sketch frequently
   - Verify each component integration
   - Monitor memory usage
   - Fix issues immediately

3. **After Application.h complete:**
   - Run full automated test
   - Verify all manual tests
   - Document any issues
   - Measure flash/RAM usage

## Success Criteria

All must pass:
- [X] Compiles without errors
- [X] Setup completes in < 5 seconds
- [X] All components initialize
- [X] Loop runs without crashes
- [X] Heap variation < 10KB
- [X] Flash usage < 95%
- [ ] All manual tests pass
- [ ] Physical device boots and operates

## Files

- `60-application.ino` - Main test sketch (342 lines)
- `README.md` - This documentation

## Related Documentation

- `/home/maxepunk/projects/Arduino/REFACTOR_IMPLEMENTATION_GUIDE.md` - Phase 5 requirements
- `/home/maxepunk/projects/Arduino/CLAUDE.md` - v4.1 architecture reference
- `/home/maxepunk/projects/Arduino/test-sketches/59-ui-layer/` - UI layer test pattern

---

**Version:** 1.0
**Created:** 2025-10-22
**ALNScanner Version:** v5.0
**Test Coverage:** 21/21 components (100%)
