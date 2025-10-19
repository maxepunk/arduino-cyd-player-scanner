# Troubleshooting Findings: ST7789 Compatibility Issues

**Date**: 2025-09-19
**Engineer**: Senior Embedded Systems Analysis
**Hardware**: CYD ESP32-2432S028R Dual USB (ST7789)

## Executive Summary

ALNScanner fails on ST7789 hardware with complete serial failure and non-responsive RFID, while display renders correctly. Through systematic testing, we've eliminated common causes and identified the issue is in ALNScanner's specific MFRC522 initialization code, not library incompatibilities. Note: ALNScanner previously worked on ILI9341 hardware with different library configurations - current environment/configs not tested on ILI9341.

## Test Results Summary

| Test # | Description | Libraries Used | Result | Key Finding |
|--------|-------------|----------------|--------|-------------|
| 01 | Basic serial | None | ✅ Works | Serial hardware OK |
| 02 | TFT_eSPI global | TFT_eSPI | ✅ Works | Display library alone OK |
| 03 | GPIO3 usage | None | ✅ Works | RX pin conflict not the issue |
| 04 | With audio | ESP8266Audio | ✅ Works | Audio libs don't break serial |
| 05 | Critical sections | None | ✅ Works | FreeRTOS primitives OK |
| 06 | LED proof of life | None | ✅ Works | Basic GPIO OK |
| 07 | Absolute minimal | None | ✅ Works | Confirmed serial works |
| 08 | MFRC522 include | MFRC522 | ⚠️ Works with delay | Library delays serial init |
| 09 | MFRC522::Uid global | MFRC522 | ⚠️ Works with 3s delay | Global constructor needs time |
| 10 | Full combo | TFT_eSPI + MFRC522 + SPI | ✅ Works | Library combo is NOT the problem |

## Critical Findings

### 1. MFRC522 Library Timing Issue
- **Finding**: Including MFRC522.h delays serial initialization
- **Solution**: 3-second delay after Serial.begin() allows serial to work
- **Impact**: Setup() messages can be missed without proper delay

### 2. Library Combination Works Fine
- **Finding**: TFT_eSPI (ILI9341 config) + MFRC522 + SPIClass globals work on ST7789
- **Evidence**: Test 10 successfully runs with all three libraries
- **Implication**: The problem is NOT library incompatibility

### 3. ALNScanner-Specific Failure
- **Finding**: ALNScanner fails even with fixes that work in test sketches
- **Evidence**: 3-second delay doesn't fix ALNScanner, but fixes test sketches
- **Location**: Problem is in ALNScanner's MFRC522 initialization code (lines 965-1031)

### 4. Display Rendering Proves Code Runs
- **Finding**: Display shows content even when serial fails
- **Implication**: ESP32 boots and executes code, serial is specifically broken
- **Not a crash**: System continues running, just without serial output

## Hypotheses Disproven

1. ❌ **GPIO3/RX Pin Conflict**: Test 03 proved GPIO3 usage alone works fine
2. ❌ **Global Constructor Order**: Test 10 has same globals, works fine
3. ❌ **TFT_eSPI Incompatibility**: Test 02 showed TFT alone works
4. ❌ **Simple Timing Issue**: 3-second delay fixes tests but not ALNScanner
5. ❌ **Driver Mismatch Alone**: ILI9341 config on ST7789 works in test 10

## Current Working Theory

The issue is in ALNScanner's custom software SPI implementation for MFRC522, specifically:
- The bit-banging routines with critical sections
- Register write operations during PCD_Init()
- Possible timing violations when combined with ST7789's different timing

## Next Steps

### Immediate Actions
1. **Uncomment MFRC522 init progressively** in ALNScanner to find exact breaking line
2. **Add debug output** between each register operation
3. **Test with ST7789 driver config** to rule out driver mismatch stress

### Diagnostic Approach
```cpp
// In ALNScanner, between lines 965-1031
Serial.println("Before MFRC522 register write X");
Serial.flush();
// Register operation
Serial.println("After MFRC522 register write X");
Serial.flush();
```

### Potential Solutions
1. **Slower SPI timing** for ST7789 variant
2. **Different initialization sequence** for ST7789
3. **Avoid certain register operations** that conflict
4. **Use hardware SPI** instead of software SPI for RFID

## Configuration Notes

### Current TFT_eSPI User_Setup.h Configuration
- **Defined**: ILI9341_DRIVER (line 10)
- **Hardware**: ST7789 (dual USB CYD variant)
- **Comment in file**: "CYD uses ILI9341 (single USB) or ST7789 (dual USB)"
- **Pin config**: Correct for both variants (lines 17-23)
- **Why display works anyway**: 
  - ST7789 tolerates/ignores ILI9341 init commands (0xEF, 0xCF, etc.)
  - Basic SPI protocol is the same
  - Pixel write commands are similar between chips
  - BUT operating in degraded/undefined state
- **Impact**: This marginal state likely makes system unstable when stressed by MFRC522 operations

### Pin Assignments (Consistent)
```
Display: MISO=12, MOSI=13, SCK=14, CS=15, DC=2, BL=21
RFID: SCK=22, MOSI=27, MISO=35, SS=3 (software SPI)
SD: SCK=18, MOSI=23, MISO=19, CS=5 (hardware VSPI)
Touch: IRQ=36 (interrupt only, no SPI)
```

## Lessons Learned

1. **Systematic testing is crucial** - Our initial assumptions were all wrong
2. **Display rendering != serial working** - Different subsystems can fail independently
3. **Library delays matter** - MFRC522 needs longer serial init delay
4. **Test sketches != production code** - ALNScanner has additional complexity

## Risk Assessment

**Current State**: Non-functional on ST7789 variant with current config
**ILI9341 Status**: Previously worked with different config - untested in current environment
**Impact**: Currently testing on ST7789 variant only
**Severity**: High - Complete loss of diagnostic capability
**Likelihood of Fix**: High - Issue is isolated to specific code section

---
*Last Updated: 2025-09-19 21:45*