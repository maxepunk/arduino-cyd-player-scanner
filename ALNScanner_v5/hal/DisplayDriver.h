#pragma once

/**
 * @file DisplayDriver.h
 * @brief TFT display HAL component for ALNScanner v5.0
 *
 * Encapsulates TFT_eSPI operations with Constitution-compliant SPI bus management.
 * CRITICAL: Implements proper SD-then-TFT locking pattern to prevent deadlocks.
 *
 * Key Features:
 * - Singleton pattern for global TFT access
 * - BMP image rendering with SPI deadlock prevention
 * - Bottom-to-top BMP row processing
 * - BGR to RGB565 color conversion
 * - Automatic yield() to prevent watchdog timeouts
 *
 * Extracted from v4.1 monolithic codebase:
 * - Lines 2697-2706: TFT initialization
 * - Lines 924-1091: BMP rendering with SPI bus management
 * - Lines 2179-2362: Screen drawing functions
 */

#include <TFT_eSPI.h>
#include "../config.h"
#include "SDCard.h"

namespace hal {

/**
 * @class DisplayDriver
 * @brief Singleton display manager with Constitution-compliant SPI patterns
 *
 * CRITICAL SPI BUS MANAGEMENT PATTERN:
 *
 * The CYD hardware has SD card and TFT on the SAME VSPI bus.
 * NEVER hold TFT lock while reading from SD - causes system freeze!
 *
 * Constitution-Compliant Pattern (from CLAUDE.md):
 * @code
 * for (each row) {
 *     // STEP 1: Read from SD (SD needs SPI bus)
 *     f.read(rowBuffer, rowBytes);
 *
 *     // STEP 2: Lock TFT and write pixels (TFT needs SPI bus)
 *     tft.startWrite();
 *     tft.setAddrWindow(0, y, width, 1);
 *     // ... push pixels ...
 *     tft.endWrite();
 *
 *     yield();  // Prevent watchdog timeout
 * }
 * @endcode
 *
 * Usage Example:
 * @code
 * auto& display = hal::DisplayDriver::getInstance();
 * display.begin();
 *
 * // Clear screen
 * display.fillScreen(TFT_BLACK);
 *
 * // Draw BMP image (Constitution-compliant)
 * if (display.drawBMP("/images/kaa001.bmp")) {
 *     Serial.println("Image rendered successfully");
 * }
 *
 * // Direct TFT access for text
 * auto& tft = display.getTFT();
 * tft.setCursor(0, 0);
 * tft.println("Hello World!");
 * @endcode
 */
class DisplayDriver {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the global DisplayDriver instance
     */
    static DisplayDriver& getInstance() {
        static DisplayDriver instance;
        return instance;
    }

    /**
     * @brief Initialize TFT display (ST7789, 240x320)
     * @return true if initialization successful, false otherwise
     *
     * Configures display for CYD dual USB variant:
     * - ST7789 driver
     * - 240x320 resolution
     * - Rotation 0 (portrait)
     * - Black background
     */
    inline bool begin() {
        if (_initialized) {
            LOG_INFO("[DISPLAY-HAL] Already initialized\n");
            return true;
        }

        LOG_INFO("[DISPLAY-HAL] Initializing TFT display...\n");

        // Initialize TFT_eSPI (configured in User_Setup.h)
        _tft.init();
        _tft.setRotation(0);  // Portrait mode
        _tft.fillScreen(TFT_BLACK);

        _initialized = true;
        LOG_INFO("[DISPLAY-HAL] TFT initialized successfully (ST7789, 240x320)\n");

        return true;
    }

    /**
     * @brief Get direct access to TFT_eSPI instance
     * @return Reference to the TFT_eSPI object
     *
     * Use for direct TFT operations (text, shapes, colors).
     * For BMP images, use drawBMP() which implements safe SPI management.
     */
    inline TFT_eSPI& getTFT() {
        return _tft;
    }

    /**
     * @brief Check if display is initialized
     * @return true if begin() was called successfully
     */
    inline bool isInitialized() const {
        return _initialized;
    }

    /**
     * @brief Draw BMP image from SD card (Constitution-compliant)
     * @param path Full path to BMP file (e.g., "/images/kaa001.bmp")
     * @return true if image rendered successfully, false on error
     *
     * Requirements:
     * - 24-bit BMP format (no compression)
     * - Bottom-to-top row order (standard BMP format)
     * - File must exist on SD card
     *
     * This function implements the Constitution-compliant SPI pattern:
     * 1. Read from SD FIRST (requires SPI bus)
     * 2. Lock TFT SECOND (requires same SPI bus)
     * 3. Release TFT lock
     * 4. Repeat for each row
     *
     * CRITICAL: NEVER change this pattern - causes system deadlock!
     *
     * Extracted from v4.1 lines 924-1091 (drawBmp function)
     */
    inline bool drawBMP(const String& path) {
        LOG_INFO("[DISPLAY-HAL] Drawing BMP: %s\n", path.c_str());

        // Check if SD card is available
        auto& sd = SDCard::getInstance();
        if (!sd.isPresent()) {
            LOG_ERROR("DISPLAY-HAL", "SD card not present");
            displayError("No SD Card");
            return false;
        }

        // Acquire SD mutex for the entire operation
        SDCard::Lock lock("drawBMP", freertos_config::SD_MUTEX_LONG_TIMEOUT_MS);
        if (!lock.acquired()) {
            LOG_ERROR("DISPLAY-HAL", "Could not acquire SD mutex");
            displayError("SD Busy");
            return false;
        }

        // Open BMP file
        File f = SD.open(path.c_str(), FILE_READ);
        if (!f) {
            LOG_ERROR("DISPLAY-HAL", "File not found");
            displayError("Missing:", path);
            return false;
        }

        LOG_INFO("[DISPLAY-HAL] File opened, size: %d bytes\n", f.size());

        // Parse BMP header
        int32_t width = 0, height = 0;
        uint16_t bpp = 0;
        if (!parseBMPHeader(f, width, height, bpp)) {
            f.close();
            displayError("Bad BMP");
            return false;
        }

        LOG_INFO("[DISPLAY-HAL] BMP: %dx%d, %d bpp\n", width, height, bpp);

        // Validate format (only 24-bit uncompressed supported)
        if (bpp != 24) {
            LOG_ERROR("DISPLAY-HAL", "Unsupported BMP format (not 24-bit)");
            f.close();
            displayError("Unsupported BMP");
            return false;
        }

        // Allocate row buffer (stack allocation for small sizes, heap for large)
        uint16_t rowBytes = width * 3;  // 3 bytes per pixel (BGR)
        LOG_INFO("[DISPLAY-HAL] Allocating %d bytes for row buffer...\n", rowBytes);

        uint8_t* rowBuffer = (uint8_t*)malloc(rowBytes);
        if (!rowBuffer) {
            LOG_ERROR("DISPLAY-HAL", "Failed to allocate row buffer");
            f.close();
            displayError("Out of Memory");
            return false;
        }

        LOG_INFO("[DISPLAY-HAL] Row buffer allocated at %p\n", rowBuffer);
        LOG_INFO("[DISPLAY-HAL] Starting row-by-row rendering (Constitution-compliant)...\n");

        // CONSTITUTION-COMPLIANT RENDERING LOOP
        // Process each row from bottom to top (BMP format)
        for (int y = height - 1; y >= 0; y--) {
            // STEP 1: READ FROM SD (SD needs SPI bus)
            // This MUST happen BEFORE tft.startWrite()!
            size_t bytesRead = f.read(rowBuffer, rowBytes);
            if (bytesRead != rowBytes) {
                LOG_ERROR("DISPLAY-HAL", "Failed to read row");
                Serial.printf("        Expected: %d, Got: %d, Row: %d\n", rowBytes, bytesRead, y);
                free(rowBuffer);
                f.close();
                return false;
            }

            // STEP 2: LOCK TFT AND WRITE PIXELS (TFT needs SPI bus)
            // Now we can safely use the SPI bus for TFT
            _tft.startWrite();  // Lock SPI for TFT

            // Set address window for THIS specific row at correct Y position
            _tft.setAddrWindow(0, y, width, 1);  // Draw one row at position y

            // Convert BGR to RGB565 and push pixels
            uint8_t* p = rowBuffer;
            for (int32_t x = 0; x < width; x++) {
                uint8_t b = *p++;  // Blue
                uint8_t g = *p++;  // Green
                uint8_t r = *p++;  // Red
                _tft.pushColor(_tft.color565(r, g, b));
            }

            _tft.endWrite();  // Release SPI

            // STEP 3: YIELD TO PREVENT WATCHDOG TIMEOUT
            // Let FreeRTOS schedule other tasks
            yield();
        }

        // Cleanup
        free(rowBuffer);
        f.close();

        LOG_INFO("[DISPLAY-HAL] BMP rendering complete\n");
        return true;
    }

    /**
     * @brief Fill entire screen with solid color
     * @param color 16-bit RGB565 color (use TFT_BLACK, TFT_WHITE, etc.)
     */
    inline void fillScreen(uint16_t color) {
        _tft.fillScreen(color);
    }

    /**
     * @brief Clear screen (fill with black)
     */
    inline void clear() {
        fillScreen(TFT_BLACK);
    }

private:
    // Singleton pattern - private constructors
    DisplayDriver() = default;
    ~DisplayDriver() = default;
    DisplayDriver(const DisplayDriver&) = delete;
    DisplayDriver& operator=(const DisplayDriver&) = delete;

    /**
     * @brief Parse BMP file header
     * @param f Open file handle (must be at start of file)
     * @param width Output: image width in pixels
     * @param height Output: image height in pixels
     * @param bpp Output: bits per pixel
     * @return true if header valid, false otherwise
     *
     * Validates:
     * - BM signature
     * - 54-byte header read
     * - No compression (compression = 0)
     *
     * Extracted from v4.1 lines 977-1022
     */
    inline bool parseBMPHeader(File& f, int32_t& width, int32_t& height, uint16_t& bpp) {
        // Read 54-byte BMP header
        uint8_t header[54];
        size_t bytesRead = f.read(header, 54);

        if (bytesRead != 54) {
            LOG_ERROR("DISPLAY-HAL", "Failed to read BMP header");
            return false;
        }

        // Validate BMP signature
        if (header[0] != 'B' || header[1] != 'M') {
            LOG_ERROR("DISPLAY-HAL", "Invalid BMP signature");
            return false;
        }

        // Extract header fields (little-endian)
        width       = *(int32_t*)&header[18];
        height      = *(int32_t*)&header[22];
        bpp         = *(uint16_t*)&header[28];
        uint32_t compression = *(uint32_t*)&header[30];
        uint32_t dataOffset  = *(uint32_t*)&header[10];

        // Validate format (only uncompressed 24-bit BMPs supported)
        if (compression != 0) {
            LOG_ERROR("DISPLAY-HAL", "Compressed BMPs not supported");
            return false;
        }

        // Seek to pixel data
        if (!f.seek(dataOffset)) {
            LOG_ERROR("DISPLAY-HAL", "Failed to seek to pixel data");
            return false;
        }

        LOG_DEBUG("[DISPLAY-HAL] BMP header parsed: %dx%d, %d bpp, offset: %d\n",
                  width, height, bpp, dataOffset);

        return true;
    }

    /**
     * @brief Display error message on screen
     * @param message Error message to display
     * @param detail Optional detail line (default: empty)
     *
     * Fills screen with black and shows error in white text.
     */
    inline void displayError(const String& message, const String& detail = "") {
        _tft.fillScreen(TFT_BLACK);
        _tft.setCursor(0, 0);
        _tft.setTextColor(TFT_WHITE, TFT_BLACK);
        _tft.setTextSize(2);
        _tft.println(message);
        if (detail.length() > 0) {
            _tft.println(detail);
        }
    }

    // Static members
    static TFT_eSPI _tft;
    static bool _initialized;
};

// Static member initialization
TFT_eSPI DisplayDriver::_tft = TFT_eSPI();
bool DisplayDriver::_initialized = false;

} // namespace hal

/**
 * IMPLEMENTATION NOTES (from v4.1 extraction)
 *
 * 1. SPI BUS DEADLOCK PREVENTION (CRITICAL!)
 *    Problem: SD card and TFT share VSPI hardware SPI bus on CYD
 *    Solution: NEVER hold tft.startWrite() lock while calling SD.read()
 *
 *    Constitution-Compliant Pattern:
 *    for (each row) {
 *        f.read(rowBuffer, rowBytes);   // SD read FIRST (needs SPI)
 *        tft.startWrite();               // TFT lock SECOND (needs SPI)
 *        tft.setAddrWindow(...);
 *        tft.pushColor(...);
 *        tft.endWrite();                 // Release TFT lock
 *        yield();                        // Let other tasks run
 *    }
 *
 *    This pattern was discovered through hardware debugging (Sept 20, 2025)
 *    and documented in CLAUDE.md lines 951-975.
 *
 * 2. BMP FORMAT DETAILS
 *    - Bottom-to-top row order (y = height-1 down to 0)
 *    - BGR color order (swap to RGB for display)
 *    - 3 bytes per pixel (no alpha channel)
 *    - Row padding to 4-byte boundary (not implemented - assumes 240px width)
 *    - 54-byte header (14-byte file header + 40-byte DIB header)
 *
 * 3. MEMORY MANAGEMENT
 *    - Row buffer allocated on heap (too large for stack on ESP32)
 *    - Typical size: 240 pixels * 3 bytes = 720 bytes
 *    - Freed immediately after rendering
 *    - Free heap monitored during allocation
 *
 * 4. WATCHDOG PREVENTION
 *    - yield() called after each row to prevent watchdog timeout
 *    - Important for large images (320 rows * 10ms = 3.2s rendering time)
 *    - Without yield(), watchdog resets ESP32 at ~1 second
 *
 * 5. COLOR CONVERSION
 *    - BMP: BGR 24-bit (8 bits red, 8 bits green, 8 bits blue)
 *    - TFT: RGB565 16-bit (5 bits red, 6 bits green, 5 bits blue)
 *    - Conversion: tft.color565(r, g, b) handles bit packing
 *
 * 6. TFT_eSPI CONFIGURATION
 *    - Configured in libraries/TFT_eSPI/User_Setup.h
 *    - ST7789 driver (not ST7735!)
 *    - 240x320 resolution
 *    - TFT_RGB_ORDER = TFT_BGR (for dual USB CYD variant)
 *    - ST7789_INVON command is ACTIVE (line 102 in ST7789_Init.h)
 *
 * 7. EXTRACTED FROM v4.1 LINES
 *    - Lines 2697-2706: TFT initialization in setup()
 *    - Lines 924-1091: drawBmp() function with SPI deadlock fix
 *    - Lines 2179-2362: Screen drawing functions (ready screen, status screen)
 *    - Lines 945-970: Error handling for missing files
 *
 * 8. SINGLETON PATTERN RATIONALE
 *    - Only one physical TFT display on hardware
 *    - Global access needed from multiple modules (RFID, WiFi, Queue)
 *    - Thread-safe static initialization (C++11 magic statics)
 *    - Prevents multiple TFT_eSPI instances (causes bus conflicts)
 *
 * 9. INTEGRATION WITH OTHER HAL COMPONENTS
 *    - Depends on: SDCard.h (for image loading)
 *    - Used by: UIManager.h (for screen composition)
 *    - Independent of: WiFiManager.h, RFIDReader.h, AudioPlayer.h
 *
 * 10. FUTURE ENHANCEMENTS (NOT IMPLEMENTED YET)
 *     - PNG support (requires additional library)
 *     - JPEG support (ESP32 has hardware decoder)
 *     - Double buffering (ESP32 RAM too tight)
 *     - DMA transfers (TFT_eSPI supports, not tested)
 *     - Partial screen updates (currently full-screen only)
 */
