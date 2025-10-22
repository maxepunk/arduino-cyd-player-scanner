# REVISED Implementation Tasks: CYD Display Compatibility

**⚠️ HISTORICAL DOCUMENT**: This file contains paths and commands from the original WSL2 development environment. The project has since been migrated to Raspberry Pi with native Arduino CLI. For current commands, see CLAUDE.md or CYD_COMPATIBILITY_STATUS.md.

**CRITICAL DISCOVERY**: TFT_eSPI is already configured for ST7789!
- Current config: `/mnt/c/Users/spide/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`
- Uses: `User_Setups/Setup_CYD_ST7789.h` 
- Has: `ST7789_DRIVER` defined, `TFT_INVERSION_ON` enabled, correct pins

**User Context**: ST7789 variant is plugged in on COM8

## Revised Task List

### Phase 1: Test Current State (5 min)

**T001** - Test ALNScanner AS-IS on ST7789
- **Rationale**: Config is already for ST7789, might already work!
- **Commands**:
  ```bash
  cd /home/spide/projects/Arduino
  arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32 ALNScanner0812Working
  sleep 1
  powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1
  ```
- **Expected outcomes**:
  - **Works**: Display shows "NeurAI Memory Scanner" → WE'RE DONE
  - **Doesn't work**: Black screen or wrong display → Continue to T002

### Phase 2: Diagnose Actual Problem (10 min)

**T002** - Test Simple Display Sketch (IF T001 FAILS)
- **Purpose**: Isolate if it's ALNScanner-specific or general display issue
- **Commands**:
  ```bash
  arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32 test-sketches/01-display-hello
  sleep 1
  powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1
  ```
- **Outcomes**:
  - **Works**: Problem is in ALNScanner code → Go to T003
  - **Doesn't work**: Config issue despite looking correct → Go to T004

**T003** - Check ALNScanner Display Init (IF test sketch works)
- **Check**: Line 898-900 in ALNScanner0812Working.ino
- **Current code**:
  ```cpp
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  ```
- **Potential fix**: May need delay or different rotation
- **Test**: Add `delay(100);` after init

**T004** - Verify Config Details (IF test sketch fails)
- **Check Setup_CYD_ST7789.h**:
  ```bash
  grep -E "TFT_|SPI_FREQ" /mnt/c/Users/spide/Documents/Arduino/libraries/TFT_eSPI/User_Setups/Setup_CYD_ST7789.h
  ```
- **Verify**:
  - Pins match hardware (CS=15, DC=2, BL=21)
  - SPI frequency not too high (currently 40MHz)

### Phase 3: Fix for ILI9341 Compatibility (15 min)

**T005** - Save Current ST7789 Config
- **Purpose**: Preserve working ST7789 setup
- **Command**:
  ```bash
  cp /mnt/c/Users/spide/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h \
     /mnt/c/Users/spide/Documents/Arduino/libraries/TFT_eSPI/User_Setup_ST7789_WORKING.h
  ```

**T006** - Find or Create ILI9341 Config
- **Check if exists**:
  ```bash
  ls /mnt/c/Users/spide/Documents/Arduino/libraries/TFT_eSPI/User_Setups/Setup*ILI9341*
  ```
- **If not, create** `User_Setups/Setup_CYD_ILI9341.h`:
  ```cpp
  #define ILI9341_DRIVER
  #define TFT_MISO 12
  #define TFT_MOSI 13  
  #define TFT_SCLK 14
  #define TFT_CS   15
  #define TFT_DC    2
  #define TFT_RST  -1
  #define TFT_BL   21
  // No TFT_INVERSION_ON for ILI9341
  ```

**T007** - Create Config Switching Method
- **Option A: Manual switching**:
  ```bash
  # For ST7789
  echo '#include "User_Setups/Setup_CYD_ST7789.h"' > User_Setup.h
  
  # For ILI9341  
  echo '#include "User_Setups/Setup_CYD_ILI9341.h"' > User_Setup.h
  ```

- **Option B: Build flags** (if supported):
  ```bash
  # Compile for specific variant
  arduino-cli compile --build-property "build.extra_flags=-DUSER_SETUP_LOADED -DILI9341_DRIVER"
  ```

**T008** - Test ILI9341 Config (WITH USER SWAPPING HARDWARE)
- **User action**: Swap to ILI9341 device
- **Update User_Setup.h for ILI9341**
- **Test**: Upload and verify

### Phase 4: Documentation (5 min)

**T009** - Document Working Configuration
- **Update** CYD_COMPATIBILITY_STATUS.md with findings:
  - ST7789: Uses Setup_CYD_ST7789.h (inversion ON)
  - ILI9341: Uses Setup_CYD_ILI9341.h (inversion OFF)
  - Switching procedure

**T010** - Create Quick Switch Scripts
- **File**: `switch-display.sh`
  ```bash
  #!/bin/bash
  case "$1" in
    st7789)
      cp User_Setup_ST7789_WORKING.h User_Setup.h
      echo "Switched to ST7789"
      ;;
    ili9341)
      cp User_Setup_ILI9341_WORKING.h User_Setup.h
      echo "Switched to ILI9341"
      ;;
    *)
      echo "Usage: $0 {st7789|ili9341}"
      ;;
  esac
  ```

## Critical Realizations

1. **Config is already for ST7789** - not ILI9341 as assumed
2. **Inversion is already enabled** via `TFT_INVERSION_ON`
3. **The problem might not exist** - need to test first
4. **If it works**, we just need to create ILI9341 config for other variant

## Execution Flow

```
START → T001 (Test current)
         ├─ Works → T005-T008 (Save and create ILI9341 config) → DONE
         └─ Fails → T002 (Test simple sketch)
                      ├─ Works → T003 (Fix ALNScanner)
                      └─ Fails → T004 (Debug config)
```

## Time Estimate
- If already works: 5 min to test + 15 min for ILI9341 config
- If needs fixing: +10 min debugging
- Total: 20-30 minutes

---
*This revision reflects the ACTUAL state of the system, not assumptions*