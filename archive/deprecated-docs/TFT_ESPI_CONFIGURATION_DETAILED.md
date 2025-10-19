# TFT_eSPI Library Configuration - Detailed Guide

## Why Configuration is Required

The TFT_eSPI library is a high-performance display driver that achieves its speed by using **compile-time configuration** rather than runtime detection. This means you must manually tell it:
- Which display controller chip you have (ILI9341 or ST7789)
- Which pins connect to the display
- What SPI speed to use
- Display dimensions and color order

**IMPORTANT**: Our auto-detection code can identify your hardware, but TFT_eSPI still needs manual configuration before compilation.

## Step-by-Step Configuration Process

### Step 1: Locate the TFT_eSPI Library Folder

After installing TFT_eSPI through Arduino Library Manager, find it at:

**Windows paths:**
```
C:\Users\[YourUsername]\Documents\Arduino\libraries\TFT_eSPI\
```
or
```
C:\Program Files (x86)\Arduino\libraries\TFT_eSPI\
```

**Mac paths:**
```
~/Documents/Arduino/libraries/TFT_eSPI/
```

**Linux paths:**
```
~/Arduino/libraries/TFT_eSPI/
```

### Step 2: Understanding the Configuration Files

Inside the TFT_eSPI folder, you'll find:

```
TFT_eSPI/
├── User_Setup.h                 # Default configuration file
├── User_Setup_Select.h          # Selects which setup to use
├── User_Setups/                 # Folder with pre-made configurations
│   ├── Setup1_ILI9341.h        # Example ILI9341 setup
│   ├── Setup2_ST7735.h         # Example ST7735 setup
│   └── ... (many more)
└── TFT_eSPI.h                   # Main library header
```

### Step 3: Determine Your CYD Model

**Option A: Visual Identification**
- **Single USB (micro only)** = ILI9341 driver
- **Dual USB (micro + Type-C)** = ST7789 driver

**Option B: Run Detection Sketch First**
1. Temporarily use a generic setup
2. Upload `test_sketches/CYD_Model_Detector/CYD_Model_Detector.ino`
3. Serial Monitor will report:
   ```
   Model Detected: Single USB (ILI9341)
   Driver ID: 0x9341
   Backlight Pin: GPIO21
   ```
   or
   ```
   Model Detected: Dual USB (ST7789)
   Driver ID: 0x8552
   Backlight Pin: GPIO27
   ```

### Step 4: Create Your Configuration

**METHOD 1: Edit User_Setup.h Directly (Simplest)**

1. Open `TFT_eSPI/User_Setup.h` in a text editor (Notepad++, VS Code, etc.)
2. Delete ALL existing content
3. Copy and paste the appropriate configuration below:

**For Single USB CYD (ILI9341):**
```cpp
// User_Setup.h for CYD Single USB Model
// ESP32-2432S028 with ILI9341 display

#define USER_SETUP_INFO "CYD_Single_USB_ILI9341"

// Display driver
#define ILI9341_DRIVER

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ESP32 pins used for display
#define TFT_MISO 12  // Master In Slave Out
#define TFT_MOSI 13  // Master Out Slave In
#define TFT_SCLK 14  // Serial Clock
#define TFT_CS   15  // Chip Select
#define TFT_DC    2  // Data/Command
#define TFT_RST  -1  // Reset pin not used (tied to ESP32 reset)
#define TFT_BL   21  // Backlight control pin

// Touch controller pins (XPT2046)
#define TOUCH_CS 33  // Touch controller chip select

// SPI frequency for display
#define SPI_FREQUENCY  40000000  // 40 MHz

// SPI frequency for touch controller
#define SPI_TOUCH_FREQUENCY  2500000  // 2.5 MHz

// Optional: Uncomment if colors are inverted
// #define TFT_INVERSION_ON

// Optional: Uncomment if colors are wrong (red shows as blue, etc)
// #define TFT_RGB_ORDER TFT_BGR

// Font support
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel high font
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel high font
#define LOAD_FONT8  // Font 8. Large 75 pixel high font
#define LOAD_GFXFF  // FreeFonts. Include access to custom fonts

// Enable smooth fonts
#define SMOOTH_FONT
```

**For Dual USB CYD (ST7789):**
```cpp
// User_Setup.h for CYD Dual USB Model
// ESP32-2432S028R with ST7789 display

#define USER_SETUP_INFO "CYD_Dual_USB_ST7789"

// Display driver
#define ST7789_DRIVER

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ESP32 pins used for display
#define TFT_MISO 12  // Master In Slave Out
#define TFT_MOSI 13  // Master Out Slave In
#define TFT_SCLK 14  // Serial Clock
#define TFT_CS   15  // Chip Select
#define TFT_DC    2  // Data/Command
#define TFT_RST  -1  // Reset pin not used (tied to ESP32 reset)

// IMPORTANT: Backlight pin varies on dual USB models!
// Try GPIO27 first, if display stays dark, change to GPIO21
#define TFT_BL   27  // Backlight control pin (or 21 on some units)

// Touch controller pins (XPT2046)
#define TOUCH_CS 33  // Touch controller chip select

// SPI frequency for display
#define SPI_FREQUENCY  40000000  // 40 MHz

// SPI frequency for touch controller  
#define SPI_TOUCH_FREQUENCY  2500000  // 2.5 MHz

// ST7789 specific settings
#define TFT_RGB_ORDER TFT_BGR  // Blue-Green-Red color order

// Some ST7789 displays need inversion
// Uncomment if colors look inverted (white appears black)
// #define TFT_INVERSION_ON
// #define TFT_INVERSION_OFF

// Font support
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel high font
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel high font
#define LOAD_FONT8  // Font 8. Large 75 pixel high font
#define LOAD_GFXFF  // FreeFonts. Include access to custom fonts

// Enable smooth fonts
#define SMOOTH_FONT
```

**METHOD 2: Using User_Setup_Select.h (Advanced)**

1. Create a new file: `TFT_eSPI/User_Setups/Setup_CYD.h`
2. Put your configuration in that file
3. Edit `TFT_eSPI/User_Setup_Select.h`:
```cpp
// Comment out the default:
// #include <User_Setup.h>

// Add your custom setup:
#include <User_Setups/Setup_CYD.h>
```

### Step 5: Verify Configuration

After saving your configuration, test it with a simple sketch:

```cpp
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("TFT_eSPI Configuration Test");
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  
  // Should see color bars
  tft.fillRect(0, 0, 80, 240, TFT_RED);
  tft.fillRect(80, 0, 80, 240, TFT_GREEN);
  tft.fillRect(160, 0, 80, 240, TFT_BLUE);
  
  Serial.println("If you see RGB bars, config is correct!");
}

void loop() {}
```

### Step 6: Troubleshooting Configuration Issues

**Problem: Display stays black**
```cpp
// Wrong backlight pin - try the other one:
#define TFT_BL   21  // Change to 27 or vice versa
```

**Problem: Colors are inverted (white shows as black)**
```cpp
// Add or remove this line:
#define TFT_INVERSION_ON
```

**Problem: Colors are wrong (red shows as blue)**
```cpp
// Toggle between RGB and BGR:
#define TFT_RGB_ORDER TFT_BGR  // or TFT_RGB
```

**Problem: Display is mirrored or rotated wrong**
```cpp
// In your sketch, try different rotations:
tft.setRotation(0);  // Try 0, 1, 2, or 3
```

**Problem: Compilation errors about TFT_eSPI**
- Make sure you saved User_Setup.h
- Check that you're not mixing configurations
- Verify only ONE driver is defined (ILI9341_DRIVER OR ST7789_DRIVER, not both)

## Configuration for Our Multi-Model Sketch

### Important Notes:

1. **The sketch auto-detects your model** but TFT_eSPI still needs manual config
2. **You must configure TFT_eSPI** for your specific CYD model before compiling
3. **The configuration is compile-time** - you can't change it at runtime
4. **If testing both models**, you'll need to reconfigure and recompile when switching

### Recommended Approach:

1. Start with the detection sketch to identify your model
2. Configure TFT_eSPI based on detection results
3. Compile and upload the main CYD_Multi_Compatible sketch
4. The sketch will handle all other hardware differences automatically

## Pin Configuration Reference

Both CYD models use the same pins for most connections:

| Function | Pin | Notes |
|----------|-----|-------|
| TFT_MISO | 12 | Display data out |
| TFT_MOSI | 13 | Display data in |
| TFT_SCLK | 14 | Display clock |
| TFT_CS | 15 | Display chip select |
| TFT_DC | 2 | Display data/command |
| TFT_RST | -1 | Not used (tied to system) |
| TFT_BL | 21 or 27 | Backlight (model dependent) |
| TOUCH_CS | 33 | Touch chip select |
| TOUCH_IRQ | 36 | Touch interrupt |

## Advanced Configuration Options

### Optimizing SPI Speed

If display works but seems slow:
```cpp
// Try different frequencies (in Hz):
#define SPI_FREQUENCY  27000000  // 27 MHz (safer)
#define SPI_FREQUENCY  40000000  // 40 MHz (default)
#define SPI_FREQUENCY  55000000  // 55 MHz (faster but may glitch)
#define SPI_FREQUENCY  80000000  // 80 MHz (maximum, often unstable)
```

### Enabling DMA (Direct Memory Access)

For smoother animations (ESP32 only):
```cpp
// Add to User_Setup.h:
#define USE_DMA_TO_TFT
```

### Reducing Memory Usage

If running low on RAM:
```cpp
// Comment out unused fonts:
// #define LOAD_FONT6
// #define LOAD_FONT7
// #define LOAD_FONT8
// #define LOAD_GFXFF
```

## Creating a Universal Configuration

Since TFT_eSPI requires compile-time configuration, true runtime switching isn't possible. However, you can create a "safe" configuration that works (suboptimally) on both:

```cpp
// Universal (but not optimal) configuration
#define ILI9341_DRIVER  // ILI9341 commands work on some ST7789
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Pins common to both
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Use both backlight pins (one will work)
#define TFT_BL   21  

// Conservative SPI speed
#define SPI_FREQUENCY  27000000

// In your sketch, manually control both backlight pins:
// pinMode(21, OUTPUT); digitalWrite(21, HIGH);
// pinMode(27, OUTPUT); digitalWrite(27, HIGH);
```

## Verification Checklist

After configuration, verify:

- [ ] Display shows something (not black)
- [ ] Colors appear correct (red is red, not blue)
- [ ] Touch responds (if testing touch)
- [ ] Backlight is on
- [ ] No compilation errors
- [ ] Serial monitor shows initialization success

## Common Mistakes to Avoid

1. **DON'T** define both ILI9341_DRIVER and ST7789_DRIVER
2. **DON'T** forget to save User_Setup.h after editing
3. **DON'T** use Notepad (it may add unwanted formatting) - use Notepad++ or VS Code
4. **DON'T** mix pin definitions from different boards
5. **DON'T** assume all dual USB models use GPIO27 for backlight (some use GPIO21)

## Need Help?

If configuration isn't working:

1. Run the detection sketch first
2. Copy the EXACT output from Serial Monitor
3. Try the test sketch with color bars
4. Note which colors appear wrong
5. Document whether backlight is on/off
6. Include your User_Setup.h content when asking for help

Remember: The hardest part is getting TFT_eSPI configured correctly. Once that's done, our auto-detection handles everything else!