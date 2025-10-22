# TFT_eSPI Configuration - Quick Reference Card

## 🎯 Quick Decision Tree

```
Do you have 1 or 2 USB ports on your CYD?
│
├─ 1 USB (micro only)
│  └─► Use ILI9341 Configuration
│      └─► Backlight = GPIO21
│
└─ 2 USB (micro + Type-C)
   └─► Use ST7789 Configuration
       └─► Backlight = GPIO21 (verified working)
           └─► If dark, try GPIO27
```

## 📁 File Location

Find and edit: `Arduino/libraries/TFT_eSPI/User_Setup.h`

## ⚡ Copy-Paste Configurations

### 🔵 Single USB Port (ILI9341)

```cpp
// DELETE everything in User_Setup.h and paste this:

#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21  // ← Backlight pin

#define TOUCH_CS 33
#define SPI_FREQUENCY  40000000
#define SPI_TOUCH_FREQUENCY  2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
```

### 🔴 Dual USB Ports (ST7789)

```cpp
// DELETE everything in User_Setup.h and paste this:

#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21  // ← Backlight (verified working on dual USB CYD)

#define TOUCH_CS 33
#define SPI_FREQUENCY  40000000
#define SPI_TOUCH_FREQUENCY  2500000

#define TFT_RGB_ORDER TFT_BGR  // ← Important for ST7789

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
```

## ⚠️ Important: No Driver Modifications Required

**The TFT_eSPI library works correctly with the default driver files - NO modifications needed!**

**What you need to configure:**
- ✅ `User_Setup.h` - Copy the configuration above

**What you DON'T need to modify:**
- ❌ `TFT_Drivers/ST7789_Init.h` - Leave this file unchanged
- ❌ Any other driver files

**Note about color inversion:**
- The ST7789 driver includes the `ST7789_INVON` command (line 102 in ST7789_Init.h)
- This is CORRECT and should NOT be commented out
- Current v4.1 ALNScanner sketch displays colors correctly with default driver
- Only use `#define TFT_INVERSION_ON` in User_Setup.h if you observe actual color inversion

## 🔧 Quick Fixes

| Problem | Solution |
|---------|----------|
| **Screen is black** | Wrong backlight pin - swap 21 ↔ 27 |
| **Colors inverted** | Try: `#define TFT_INVERSION_ON` in User_Setup.h (rare) |
| **Red shows as Blue** | Change: `TFT_RGB_ORDER TFT_BGR` → `TFT_RGB` |
| **Display mirrored** | In sketch: try `tft.setRotation(1);` |
| **Compilation error** | Check only ONE driver defined |
| **Display corrupted** | Lower speed: `SPI_FREQUENCY 27000000` |

## 🧪 Test Your Configuration

Upload this test sketch after configuring:

```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
  tft.init();

  // Test 1: Backlight (enable both, one will work)
  pinMode(21, OUTPUT); digitalWrite(21, HIGH);  // Primary for dual USB
  pinMode(27, OUTPUT); digitalWrite(27, HIGH);  // Alternate

  // Test 2: Colors (should see R-G-B bars)
  tft.fillRect(0, 0, 80, 240, TFT_RED);
  tft.fillRect(80, 0, 80, 240, TFT_GREEN);
  tft.fillRect(160, 0, 80, 240, TFT_BLUE);
  
  // Test 3: Text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CYD TEST", 10, 10, 4);
}

void loop() {}
```

**Expected Result:**
- 3 color bars: Red, Green, Blue (left to right)
- White text "CYD TEST" at top
- If colors wrong → adjust TFT_RGB_ORDER
- If all black → wrong backlight pin

## ❌ Common Mistakes

### Mistake 1: Not saving the file
```
✗ Edit User_Setup.h
✗ Forget to save
✗ Compile with old settings
✓ SAVE THE FILE (Ctrl+S)
```

### Mistake 2: Multiple drivers defined
```cpp
✗ #define ILI9341_DRIVER
✗ #define ST7789_DRIVER  // Both defined!

✓ #define ILI9341_DRIVER  // Only one!
```

### Mistake 3: Wrong library folder
```
✗ Arduino/libraries/TFT_eSPI-master/  // Wrong!
✓ Arduino/libraries/TFT_eSPI/         // Correct!
```

### Mistake 4: Using comments incorrectly
```cpp
✗ //#define TFT_BL 21  // This is commented out!
✓ #define TFT_BL 21    // This is active
```

## 📊 Configuration Verification

After uploading any sketch using TFT_eSPI, check Serial Monitor:

```
TFT_eSPI ver = 2.4.72
Processor    = ESP32
Frequency    = 240MHz
Transactions = Yes
Interface    = SPI
Display driver = 9341  ← Should match your display
Display width  = 240
Display height = 320
```

If driver number is wrong, you configured for wrong model!

## 🚀 Step-by-Step Process

1. **Find** `Arduino/libraries/TFT_eSPI/User_Setup.h`
2. **Open** with text editor (not Word!)
3. **Delete** ALL content
4. **Paste** configuration for your model
5. **Save** file (Ctrl+S)
6. **Open** Arduino IDE
7. **Upload** test sketch
8. **Check** display shows colors
9. **Done!** Ready for main sketch

## 💡 Pro Tips

### Tip 1: Make a Backup
```bash
# Before editing, save original:
copy User_Setup.h User_Setup_BACKUP.h
```

### Tip 2: Multiple CYDs?
Create different setups:
- `User_Setup_SingleUSB.h`
- `User_Setup_DualUSB.h`
- Copy appropriate one to `User_Setup.h` when needed

### Tip 3: Finding Library Folder
In Arduino IDE:
- Sketch → Include Library → Manage Libraries
- Search "TFT_eSPI"
- Click "More info" link
- Opens the folder location

### Tip 4: Using PlatformIO?
Add to `platformio.ini`:
```ini
build_flags = 
  -DUSER_SETUP_LOADED=1
  -DILI9341_DRIVER=1
  -DTFT_BL=21
  ; ... other defines
```

## 🆘 Emergency Reset

If everything is broken, reinstall TFT_eSPI:
1. Delete entire `TFT_eSPI` folder
2. In Arduino IDE: Tools → Manage Libraries
3. Search "TFT_eSPI"
4. Click "Install"
5. Start configuration fresh

## ✅ Success Checklist

- [ ] Found User_Setup.h file
- [ ] Deleted old content
- [ ] Pasted correct configuration
- [ ] Saved the file
- [ ] Test sketch compiles
- [ ] Display shows colors
- [ ] Backlight is on
- [ ] Touch responds (optional)

---

**Remember**: This is a ONE-TIME setup. Once configured correctly, you don't need to change it again unless switching to different CYD model!

---

## 📝 Document Update History

**Last Updated:** October 21, 2025

**Changes:**
- ✅ Corrected backlight pin for dual USB CYD (GPIO21, not GPIO27)
- ✅ Removed outdated information about modifying ST7789_Init.h
- ✅ Clarified that default TFT_eSPI driver files work correctly without modification
- ✅ Updated color inversion guidance to reflect current working configuration

**Verified Configuration:**
- Hardware: ESP32-2432S028R (Dual USB CYD with ST7789 display)
- Sketch: ALNScanner1021_Orchestrator v4.1
- Status: ✅ Working correctly with above configuration