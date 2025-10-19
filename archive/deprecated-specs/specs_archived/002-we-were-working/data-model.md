# Data Model: CYD Multi-Hardware Compatibility

**Feature**: CYD Hardware Testing & Verification  
**Date**: 2025-09-19  
**Version**: 1.0.0

## Core Entities

### HardwareVariant
Represents detected CYD hardware configuration.

**Fields**:
- `displayDriver`: enum {ILI9341, ST7789} - Display controller type
- `usbPorts`: enum {SINGLE_MICRO, DUAL_MICRO_TYPEC} - USB configuration  
- `backlightPin`: uint8_t - GPIO pin for backlight control
- `detectedAt`: uint32_t - Millis timestamp of detection

**Validation**:
- displayDriver must be detected within 1000ms of boot
- backlightPin must be 21 or 27
- Detection must complete before component initialization

**State Transitions**:
- UNKNOWN → DETECTING → IDENTIFIED
- UNKNOWN → DETECTING → FAILED

### ComponentStatus
Tracks individual hardware component state.

**Fields**:
- `type`: enum {DISPLAY, TOUCH, RFID, SD_CARD, AUDIO}
- `state`: enum {NOT_TESTED, TESTING, WORKING, FAILED, DEGRADED}
- `errorCode`: uint16_t - Component-specific error code
- `errorMessage`: char[64] - Human-readable error description
- `lastTestTime`: uint32_t - Millis timestamp of last test

**Validation**:
- errorCode = 0 when state = WORKING
- errorMessage required when state = FAILED
- DEGRADED state requires non-zero errorCode

**State Transitions**:
- NOT_TESTED → TESTING → {WORKING, FAILED, DEGRADED}
- WORKING → TESTING → {WORKING, FAILED, DEGRADED}
- FAILED → TESTING → {WORKING, FAILED}

### TouchState
Tracks touch IRQ tap detection state.

**Fields**:
- `irqPin`: uint8_t - Touch IRQ pin (GPIO36)
- `lastTapTime`: uint32_t - Millis timestamp of last tap
- `tapCount`: uint8_t - Number of taps in sequence
- `doubleTapWindow`: uint16_t - Max ms between taps for double-tap
- `debounceMs`: uint16_t - Debounce time in milliseconds

**Validation**:
- irqPin must be 36 (fixed hardware)
- doubleTapWindow typically 300-500ms
- **debounceMs MUST be 200ms** (each physical tap generates 2 interrupts ~80-130ms apart)

**Critical Implementation Notes**:
- Touch uses IRQ-only detection. No coordinate reading supported.
- Each physical tap generates TWO falling edge interrupts:
  1. First interrupt when finger touches (pin goes LOW)
  2. Second interrupt ~80-130ms later (release or noise)
- 200ms debounce filters out the second interrupt
- Attach interrupt on FALLING edge only

### RFIDCard
Represents scanned RFID card data.

**Fields**:
- `uid`: uint8_t[10] - Card unique identifier
- `uidLength`: uint8_t - Actual UID length (4, 7, or 10)
- `type`: enum {MIFARE_MINI, MIFARE_1K, MIFARE_4K, UNKNOWN}
- `imagePath`: char[32] - Associated image filename on SD
- `audioPath`: char[32] - Associated audio filename on SD
- `lastSeen`: uint32_t - Millis timestamp of last scan

**Validation**:
- uidLength must be 4, 7, or 10
- imagePath must end in .bmp or .jpg
- audioPath must end in .mp3 or .wav
- Paths relative to SD card root

### DiagnosticReport
Comprehensive system diagnostic information.

**Fields**:
- `hardwareVariant`: HardwareVariant - Detected hardware
- `components`: ComponentStatus[5] - All component states
- `freeHeap`: uint32_t - Available heap memory
- `uptime`: uint32_t - System uptime in seconds
- `testMode`: bool - Diagnostic mode active flag
- `wiringValid`: bool - Wiring configuration check result

**Validation**:
- All components must have non-NOT_TESTED state
- freeHeap must be > 10000 bytes for stable operation
- wiringValid requires all critical components WORKING

**Generation**:
- Created on demand via serial command
- Auto-generated on component failure
- Saved to SD if available

### GPIO27State
Manages GPIO27 multiplexing between backlight and RFID.

**Fields**:
- `currentMode`: enum {BACKLIGHT_PWM, RFID_MOSI, TRANSITIONING}
- `backlightDuty`: uint8_t - PWM duty cycle (0-255)
- `transitionCount`: uint32_t - Total mode switches
- `lastTransition`: uint32_t - Microseconds timestamp
- `locked`: bool - Interrupt protection flag

**Validation**:
- Mode transitions must be atomic
- Minimum 100ns between transitions
- locked must be true during RFID operations
- backlightDuty ignored when mode = RFID_MOSI

**State Transitions**:
- BACKLIGHT_PWM → TRANSITIONING → RFID_MOSI
- RFID_MOSI → TRANSITIONING → BACKLIGHT_PWM

## Relationships

### HardwareVariant ↔ TouchState
- One-to-one relationship
- All variants use same IRQ pin (GPIO36)
- Touch detection works identically on all variants

### RFIDCard ↔ SD Card Files
- One-to-many relationship  
- Each card UID maps to multiple files
- Files organized in /rfid/{uid}/ directory

### ComponentStatus ↔ DiagnosticReport
- One-to-many aggregation
- Report aggregates all component states
- Component updates trigger report generation

### HardwareVariant ↔ GPIO27State
- One-to-one relationship
- Dual USB variant requires multiplexing
- Single USB variant uses static backlight

## Data Persistence

### EEPROM Layout (512 bytes total)
```
0x00-0x0F: Reserved (formerly touch calibration)
0x10-0x11: HardwareVariant ID
0x12-0x13: Config version
0x14-0x1F: Reserved
0x20-0xFF: User data
```

### SD Card Structure
```
/rfid/
  /{uid}/
    image.bmp
    audio.mp3
/logs/
  diagnostic_{timestamp}.txt
/config/
  variants.json
```

## Constraints

### Memory Constraints
- Total RAM usage < 200KB
- Stack depth < 4KB
- String buffers pre-allocated

### Timing Constraints
- Hardware detection < 1 second
- RFID scan response < 100ms
- Touch IRQ response < 50ms
- Audio start < 200ms

### Concurrency
- No RTOS/threading
- Interrupt-safe RFID operations
- Non-blocking SD card access where possible

---
*Data model optimized for embedded ESP32 constraints*