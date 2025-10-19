/**
 * Hardware Detection Contract
 * 
 * This contract defines the interface for detecting CYD hardware variants
 * and initializing the appropriate configuration.
 */

#ifndef HARDWARE_DETECTION_H
#define HARDWARE_DETECTION_H

#include <Arduino.h>

// Hardware model enumeration
enum CYDModel {
    CYD_MODEL_UNKNOWN = 0,
    CYD_MODEL_SINGLE_USB = 1,  // ILI9341 display, micro USB only
    CYD_MODEL_DUAL_USB = 2      // ST7789 display, micro + Type-C
};

// Display driver enumeration  
enum DisplayDriver {
    DISPLAY_DRIVER_UNKNOWN = 0,
    DISPLAY_DRIVER_ILI9341 = 0x9341,
    DISPLAY_DRIVER_ST7789 = 0x8552
};

// Hardware configuration structure
struct CYDHardwareConfig {
    CYDModel model;
    DisplayDriver displayDriver;
    uint8_t backlightPin;
    bool hasSDCard;
    bool hasRFID;
    bool hasTouch;
    bool hasAudio;
};

/**
 * Hardware Detection Interface
 */
class IHardwareDetector {
public:
    virtual ~IHardwareDetector() = default;
    
    /**
     * Detect the CYD hardware model
     * @return Detected model type
     */
    virtual CYDModel detectModel() = 0;
    
    /**
     * Get display driver type
     * @return Display driver ID
     */
    virtual DisplayDriver detectDisplayDriver() = 0;
    
    /**
     * Detect backlight pin configuration
     * @return GPIO pin number for backlight control
     */
    virtual uint8_t detectBacklightPin() = 0;
    
    /**
     * Check if SD card slot is present and accessible
     * @return true if SD card hardware detected
     */
    virtual bool detectSDCard() = 0;
    
    /**
     * Check if RFID reader is connected
     * @return true if RFID hardware responds
     */
    virtual bool detectRFID() = 0;
    
    /**
     * Check if touch controller is present
     * @return true if touch hardware responds
     */
    virtual bool detectTouch() = 0;
    
    /**
     * Check if audio I2S is available
     * @return true if audio can be initialized
     */
    virtual bool detectAudio() = 0;
    
    /**
     * Get complete hardware configuration
     * @return Populated hardware config structure
     */
    virtual CYDHardwareConfig getConfiguration() = 0;
    
    /**
     * Report detection results to serial
     * @param verbose If true, include detailed diagnostics
     */
    virtual void reportDiagnostics(bool verbose = false) = 0;
};

/**
 * Contract Test Requirements:
 * 
 * 1. detectModel() must return a valid model within 500ms
 * 2. detectDisplayDriver() must correctly identify ILI9341 vs ST7789
 * 3. detectBacklightPin() must return 21 or 27
 * 4. All detect*() methods must not crash on missing hardware
 * 5. getConfiguration() must return consistent results across calls
 * 6. reportDiagnostics() must output to Serial at 115200 baud
 */

#endif // HARDWARE_DETECTION_H