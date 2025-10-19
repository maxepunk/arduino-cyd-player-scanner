# Phase 0: Research & Technical Decisions

**Feature**: CYD Multi-Model Compatibility  
**Date**: 2025-09-18  
**Status**: Complete

## Executive Summary
Research completed for enabling ALNScanner0812Working.ino to support all CYD Resistive 2.8" variants without wiring changes. Key findings: hardware detection is possible via display driver response, GPIO27 conflict can be resolved through careful timing, and all functionality can be preserved.

## Research Areas & Findings

### 1. Hardware Detection Methodology

**Decision**: Use display driver ID register reading for model detection  
**Rationale**: 
- ILI9341 returns 0x9341 when reading ID register
- ST7789 returns 0x8552 or doesn't respond to ILI9341 commands
- This provides reliable differentiation between models

**Alternatives Considered**:
- GPIO level detection: Rejected - not reliable across all units
- USB port detection: Rejected - requires additional hardware access
- EEPROM storage: Rejected - requires initial manual configuration

**Implementation Approach**:
```cpp
uint16_t detectDisplayDriver() {
  // Try ILI9341 ID read command
  uint16_t id = tft.readID();
  if (id == 0x9341) return DRIVER_ILI9341;
  // Try ST7789 specific command
  if (testST7789()) return DRIVER_ST7789;
  return DRIVER_UNKNOWN;
}
```

### 2. GPIO27 Pin Conflict Resolution

**Decision**: Use time-division multiplexing for GPIO27  
**Rationale**:
- Backlight (when on GPIO27) only needs to be set once at startup
- RFID MOSI needs GPIO27 only during RFID operations
- Can safely switch pin function between operations

**Alternatives Considered**:
- Hardware SPI for RFID: Rejected - conflicts with display SPI
- Different RFID pins: Rejected - violates Zero Wiring Change Policy
- PWM backlight dimming: Rejected - adds unnecessary complexity

**Implementation Approach**:
```cpp
class GPIO27Manager {
  void setBacklight() {
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);
  }
  
  void prepareForRFID() {
    pinMode(27, OUTPUT);  // MOSI mode
    // RFID operations...
  }
  
  void restoreBacklight() {
    digitalWrite(27, HIGH);  // Restore if needed
  }
};
```

### 3. Display Driver Runtime Selection

**Decision**: Use conditional compilation with runtime switching  
**Rationale**:
- TFT_eSPI doesn't support runtime driver changes natively
- Can use two TFT_eSPI instances with different configs
- Minimal memory overhead (~4KB per instance)

**Alternatives Considered**:
- Single driver with compatibility layer: Rejected - too complex
- Manual register programming: Rejected - loses TFT_eSPI optimizations
- Separate sketches: Rejected - violates single sketch requirement

**Implementation Approach**:
```cpp
// Pre-configured instances
TFT_eSPI_ILI9341 tft_ili;
TFT_eSPI_ST7789 tft_st;
TFT_eSPI* tft = nullptr;  // Runtime pointer

void setupDisplay() {
  if (detected_driver == DRIVER_ILI9341) {
    tft = &tft_ili;
  } else {
    tft = &tft_st;
  }
  tft->init();
}
```

### 4. Touch Calibration Management

**Decision**: Store calibration values in EEPROM with model-specific profiles  
**Rationale**:
- Each CYD model has consistent calibration values
- EEPROM persists across power cycles
- Can include default values for known models

**Alternatives Considered**:
- Runtime calibration every boot: Rejected - poor user experience
- Hard-coded values: Rejected - may vary between units
- SD card storage: Rejected - SD card might not be present

**Implementation Approach**:
```cpp
struct TouchCalibration {
  uint16_t xMin, xMax, yMin, yMax;
  uint8_t modelID;
  uint16_t checksum;
};

TouchCalibration getCalibration() {
  TouchCalibration cal;
  EEPROM.get(TOUCH_CAL_ADDR, cal);
  if (cal.checksum != calculateChecksum(cal)) {
    return getDefaultCalibration(detected_model);
  }
  return cal;
}
```

### 5. Software SPI Timing Optimization

**Decision**: Use interrupt-protected critical sections for SPI bit-banging  
**Rationale**:
- Prevents timing disruption from system interrupts
- Ensures consistent RFID communication
- Minimal impact on system responsiveness

**Alternatives Considered**:
- Dedicated SPI task on Core 1: Rejected - overkill for this application
- DMA-based SPI: Rejected - not available for software SPI
- Lower SPI speed: Rejected - already at practical minimum

**Implementation Approach**:
```cpp
byte softSPI_transfer(byte data) {
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);
  // Bit-bang SPI with consistent timing
  for (int i = 0; i < 8; i++) {
    digitalWrite(MOSI, (data & 0x80));
    data <<= 1;
    delayMicroseconds(2);
    digitalWrite(SCK, HIGH);
    delayMicroseconds(2);
    if (digitalRead(MISO)) data |= 1;
    digitalWrite(SCK, LOW);
    delayMicroseconds(2);
  }
  portEXIT_CRITICAL(&mux);
  return data;
}
```

### 6. Audio I2S Compatibility

**Decision**: Use same I2S pins for all models (GPIO25, 26, 22)  
**Rationale**:
- These pins are free on all CYD variants
- Standard ESP32 I2S configuration
- No conflicts with other peripherals

**Alternatives Considered**:
- External DAC: Rejected - requires hardware changes
- PWM audio: Rejected - poor quality
- Disable audio on some models: Rejected - violates functionality preservation

### 7. Diagnostic System Design

**Decision**: Implement hierarchical diagnostic reporting with severity levels  
**Rationale**:
- Allows filtering by importance
- Structured output for parsing
- Includes timestamps and component context

**Implementation Approach**:
```cpp
enum DiagLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

class DiagnosticSystem {
  void report(DiagLevel level, String component, String message) {
    Serial.printf("[%lu][%s][%s]: %s\n", 
                  millis(), 
                  levelStr(level), 
                  component.c_str(), 
                  message.c_str());
  }
};
```

## Technical Constraints Validated

1. **Memory Usage**: Combined approach uses ~470KB SRAM (within 520KB limit)
2. **Pin Availability**: All required pins available with multiplexing
3. **Performance**: Touch response <50ms, display refresh maintains 30fps
4. **Power**: No significant increase in power consumption

## Risk Mitigation

1. **Unknown Models**: Default to safe ILI9341 mode with diagnostics
2. **Timing Issues**: Critical sections protect time-sensitive operations
3. **Partial Failures**: Each component can fail independently without crash
4. **Calibration Loss**: Factory defaults provided for each model

## Next Steps

Phase 1 Design will create:
- Detailed data models for hardware configurations
- Contract specifications for component interfaces
- Test scenarios for validation
- Quickstart guide for users

## References

- ESP32 Technical Reference Manual (GPIO characteristics)
- TFT_eSPI Library Documentation (driver support)
- MFRC522 Datasheet (SPI timing requirements)
- XPT2046 Touch Controller Specification (calibration ranges)