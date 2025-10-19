# Simple Workflow: Getting Both CYD Variants Working

## The Situation
- ALNScanner works on ILI9341 (single USB)
- Doesn't work on ST7789 (dual USB)
- User can only test one device at a time

## Step 1: User Identifies Hardware
**User**: "I have the dual USB variant plugged in"
**Us**: "That's the ST7789. Let's fix it."

## Step 2: Quick Test Current State
```bash
# User uploads current ALNScanner
arduino-cli upload -p COM3 ALNScanner0812Working

# User runs serial monitor
powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1
```

**User reports**: "Display is black but serial works"
**Us**: "Good, that confirms wrong display driver"

## Step 3: Find the Fix

### Fastest Path: Edit User_Setup.h
**Us**: "Open this file in notepad:"
```
C:\Users\[username]\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
```

**Us**: "Find this line (probably around line 40):"
```cpp
#define ILI9341_DRIVER
```

**Us**: "Change it to:"
```cpp
// #define ILI9341_DRIVER
#define ST7789_DRIVER
```

**Us**: "Save and close"

## Step 4: Test the Fix
```bash
# Compile with new driver
arduino-cli compile ALNScanner0812Working

# Upload
arduino-cli upload -p COM3 ALNScanner0812Working

# Monitor
powershell.exe -ExecutionPolicy Bypass -File reliable-serial-monitor.ps1
```

**User**: "Display works but colors look wrong!"
**Us**: "Let's fix that..."

## Step 5: Fix Color Inversion (if needed)

**Us**: "Edit ALNScanner0812Working.ino, find tft.init() around line 898"
**Us**: "Add this right after it:"
```cpp
tft.init();
tft.invertDisplay(true);  // Add this line
```

**User**: Compile, upload, test
**User**: "Perfect! It works!"

## Step 6: Document for Switching

**Us**: "Save your User_Setup.h as User_Setup_ST7789.h"
**User**: "How do I switch back to ILI9341?"
**Us**: "Just change the driver back to ILI9341_DRIVER"

## Total Time
- 5 minutes to diagnose
- 2 minutes to fix
- 3 minutes to test

## What We Actually Changed
1. One line in User_Setup.h (the driver)
2. Maybe one line in the sketch (invertDisplay)

That's it. No architecture changes, no refactoring, just the minimal fix.