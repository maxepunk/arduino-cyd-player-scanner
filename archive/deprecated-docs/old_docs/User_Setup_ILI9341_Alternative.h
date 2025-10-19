// Alternative configuration for CYD Dual USB - Try this if ST7789 doesn't work
// Copy this content to libraries/TFT_eSPI/User_Setup.h if the display still doesn't work

//                            USER DEFINED SETTINGS
#define USER_SETUP_INFO "User_Setup_ESP32_CYD_DualUSB_ILI9341"

// ##################################################################################
// Section 1. Call up the right driver file and any options for it
// ##################################################################################

// Try ILI9341 driver instead of ST7789
#define ILI9341_DRIVER       // Generic driver for common displays

// For ILI9341, define the colour order
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

// Try with inversion
#define TFT_INVERSION_OFF

// ##################################################################################
// Section 2. Define the pins that are used to interface with the display here
// ##################################################################################

// ESP32-2432S028R CYD Configuration
#define TFT_MISO 12  // Display MISO
#define TFT_MOSI 13  // Display MOSI  
#define TFT_SCLK 14  // Display SCK
#define TFT_CS   15  // Display chip select
#define TFT_DC    2  // Display data/command
#define TFT_RST  -1  // Reset tied to ESP32 RST

// Backlight control - Try both pins
#define TFT_BL   27  // LED back-light control pin (or try 21)
#define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light

// Touch
#define TOUCH_CS 33  // Touch chip select

// ##################################################################################
// Section 3. Define the fonts that are to be used here
// ##################################################################################

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

// ##################################################################################
// Section 4. Other options
// ##################################################################################

// Define the SPI clock frequency
#define SPI_FREQUENCY  40000000  // 40MHz for ILI9341

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  20000000

// The XPT2046 requires a lower SPI clock rate of 2.5MHz so we define that here:
#define SPI_TOUCH_FREQUENCY  2500000