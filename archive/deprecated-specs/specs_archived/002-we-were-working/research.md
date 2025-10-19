# Phase 0: Research & Technical Clarifications

**Feature**: CYD Multi-Hardware Compatibility Testing and Verification  
**Date**: 2025-09-19  
**Status**: Complete  
**Last Updated**: 2025-09-19

## Executive Summary
Research focused on understanding the hardware differences between CYD variants and developing strategies for zero-wiring-change compatibility. Critical findings include: WSL2→Windows development environment constraints, touch controller using IRQ-only detection (no SPI communication), and GPIO27 serving dual purposes (backlight on dual USB, RFID MOSI on our wiring), requiring time-division multiplexing approach.

## Development Environment Research

### Finding: WSL2→Windows Arduino CLI Bridge
**Discovery**: Arduino CLI must run on Windows host, accessed from WSL2 Ubuntu
**Critical Constraints**:
- Serial ports are Windows COM ports (COM3-COM8), NOT Linux /dev/tty*
- File paths auto-translate: WSL2 `/home/spide/` → Windows `\\wsl.localhost\Ubuntu-22.04\home\spide\`
- Sequential compilation only - Windows file locking prevents parallel builds
- arduino-cli monitor does NOT work from WSL2
**Verified Solution**: PowerShell serial monitor script (reliable-serial-monitor.ps1)

### Decision: Standardized Workflow
**Compile + Upload**: `arduino-cli compile --upload -p COM8 --fqbn esp32:esp32:esp32 [sketch]`
**Monitor**: `sleep 1 && powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1`
**Combined**: Always chain compile+upload+monitor for debugging

## Hardware Variant Research

### Decision: Runtime Hardware Detection
**Rationale**: Display driver chip ID can differentiate variants (ILI9341 vs ST7789)
**Alternatives Considered**:
- Pin state detection (rejected: unreliable)
- User configuration (rejected: requires manual setup)
- Compile-time flags (rejected: requires multiple binaries)

### Decision: GPIO27 Multiplexing Strategy  
**Rationale**: Time-division multiplexing allows both backlight and RFID without rewiring
**Implementation**: 
- During RFID operations: GPIO27 as MOSI
- Between operations: GPIO27 as PWM backlight
- Interrupt protection ensures timing integrity
**Alternatives Considered**:
- Hardware rewiring (rejected: violates zero-change requirement)
- Disable backlight on dual USB (rejected: poor user experience)
- Alternative RFID pins (rejected: would require wiring changes)

## Software Architecture Research

### Decision: Software SPI for RFID
**Rationale**: Hardware SPI conflicts with SD card; software SPI provides pin flexibility
**Performance**: Adequate for MFRC522 (up to 10MHz clock achievable)
**Alternatives Considered**:
- Second hardware SPI (rejected: ESP32 limitation)
- I2C RFID modules (rejected: different hardware required)

### Decision: EEPROM-based Configuration Storage
**Rationale**: Store variant ID and configuration data
**Implementation**: Reserved EEPROM space for future use
**Note**: Touch calibration not needed (IRQ-only detection)

## Testing Strategy Research

### Decision: Component Isolation Testing
**Rationale**: Identifies specific failure points before integration
**Test Order**:
1. Display (no dependencies)
2. SD card (hardware SPI verification)
3. Touch (requires display working)
4. RFID (complex due to GPIO27 multiplexing)
5. Audio (requires SD for files)

### Decision: Diagnostic Categorization
**Rationale**: Differentiates fixable issues from hardware defects
**Categories**:
- No Response: Hardware disconnected/dead
- Wrong Response: Wiring issue or config error
- Intermittent: Power/connection quality issue
- Timeout: Incorrect pin or timing configuration

## Wiring Compatibility Research

### Finding: Identical Wiring IS Possible
**Key Insight**: All CYD variants expose same physical pins at connectors
**Verification Method**:
- P1 connector: Consistent across variants
- P3 connector: Same pinout both variants
- CN1 connector: Identical SD card interface

### Decision: Standard Pin Mapping
```
RFID: SCK=22, MOSI=27, MISO=35, SS=3
SD: SCK=18, MOSI=23, MISO=19, CS=5  
Touch: IRQ=36 only (CS=33 not used - no SPI communication)
Display: Standard TFT_eSPI pins
```

## Performance Optimizations

### Decision: Burst Mode RFID Communication
**Rationale**: Minimizes GPIO27 switching overhead
**Implementation**: Complete RFID transactions in single burst
**Benefit**: Reduces backlight flicker on dual USB variant

### Decision: Display Buffer Management
**Rationale**: Both ILI9341 and ST7789 support similar buffering
**Implementation**: Use partial screen updates where possible
**Benefit**: Consistent performance across variants

## Error Recovery Research

### Decision: Graceful Degradation
**Rationale**: System remains useful even with component failures
**Priority Order**:
1. Display (required - fail if not working)
2. RFID (core function - alert but continue)
3. SD card (try to continue with reduced functionality)
4. Touch (fallback to serial commands)
5. Audio (silent operation if unavailable)

### Decision: Diagnostic Mode Entry
**Trigger**: Serial command only (touch is IRQ-only, no coordinate reading)
**Output**: Comprehensive hardware report via serial
**Benefit**: Field debugging without code changes

## Touch Controller Research

### Critical Finding: IRQ-Only Touch Detection
**Discovery Date**: 2025-09-19
**Testing Results**:
- Touch IRQ (GPIO36) works perfectly - goes LOW on touch, HIGH on release
- SPI communication completely fails on all tested buses
- Hardware SPI (pins 18,23,19): Returns all 8191s (0x1FFF) - no device responding
- RFID SPI (pins 22,27,35): Returns all 0s - no device responding
**Root Cause**: XPT2046 SPI pins either not connected or non-standard on CYD boards
**Working Implementation**: ALNScanner0812Working uses IRQ-only for double-tap detection

### Decision: Abandon Touch Coordinate Reading
**Rationale**: Touch controller SPI interface non-functional on CYD boards
**Implementation**: Use GPIO36 interrupt for tap/double-tap detection only
**Benefits**: Simpler, more reliable, works on all variants
**Alternatives Rejected**:
- XPT2046_Touchscreen library (requires working SPI)
- TFT_eSPI touch functions (also expects SPI communication)
- Raw SPI communication (hardware limitation, not software issue)

### Critical Finding: Touch Interrupt Behavior
**Discovery Date**: 2025-09-19 (during testing)
**Behavior**: Each physical tap generates TWO falling edge interrupts
- First interrupt: Finger touches screen (pin goes LOW)
- Second interrupt: ~80-130ms later (release event or noise)
**Solution**: 200ms debounce period (NOT 50ms as originally assumed)
**Verification**: Tested with touch-irq-test sketch - confirmed reliable tap/double-tap detection
**Impact**: All touch implementations MUST use 200ms debounce to avoid false double-taps

## Key Technical Clarifications

### TFT_eSPI Configuration
- Both variants use same SPI pins for display
- Only backlight GPIO differs (21 vs 27)
- Library auto-detects controller type

### MFRC522 Timing Requirements  
- Minimum 100ns between GPIO state changes
- Software SPI easily meets requirements
- Interrupt protection critical during transactions

### Audio Output Configuration
- I2S pins same on both variants
- No conflicts with other peripherals
- Volume control via software only

## Resolved Questions

1. **Can we avoid wiring changes?** YES - through GPIO27 multiplexing
2. **How to detect variant?** Display driver ID read
3. **Will software SPI work?** YES - tested up to 10MHz
4. **Can touch work without SPI?** YES - IRQ-only tap detection sufficient
5. **Audio compatibility?** Full compatibility confirmed

## Implementation Readiness

All technical unknowns have been resolved:
- Development environment constraints documented and solved
- Touch simplified to IRQ-only detection (no coordinate reading)
- GPIO27 multiplexing enables zero-wiring-change compatibility
- Serial monitoring via PowerShell script verified working
- Component test sketches will validate each subsystem before integration

---
*Research complete - Ready for Phase 1: Design & Contracts*