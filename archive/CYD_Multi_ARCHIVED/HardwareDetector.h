/**
 * HardwareDetector.h - Hardware detection implementation for CYD variants
 * 
 * Implements the hardware detection contract to identify CYD model,
 * display driver, and available components automatically.
 */

#ifndef HARDWARE_DETECTOR_H
#define HARDWARE_DETECTOR_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "HardwareConfig.h"

// Import contract interfaces
enum CYDModel {
    CYD_MODEL_UNKNOWN = 0,
    CYD_MODEL_SINGLE_USB = 1,  // ILI9341 display, micro USB only
    CYD_MODEL_DUAL_USB = 2     // ST7789 display, micro + Type-C
};

enum DisplayDriver {
    DISPLAY_DRIVER_UNKNOWN = 0,
    DISPLAY_DRIVER_ILI9341 = 0x9341,
    DISPLAY_DRIVER_ST7789 = 0x8552
};

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
    virtual CYDModel detectModel() = 0;
    virtual DisplayDriver detectDisplayDriver() = 0;
    virtual uint8_t detectBacklightPin() = 0;
    virtual bool detectSDCard() = 0;
    virtual bool detectRFID() = 0;
    virtual bool detectTouch() = 0;
    virtual bool detectAudio() = 0;
    virtual CYDHardwareConfig getConfiguration() = 0;
    virtual void reportDiagnostics(bool verbose = false) = 0;
};

/**
 * CYD Hardware Detector Implementation
 */
class HardwareDetector : public IHardwareDetector {
private:
    CYDHardwareConfig config;
    bool detectionComplete;
    uint32_t detectionStartTime;
    
    // Internal detection methods
    DisplayDriver probeDisplayDriver();
    bool testBacklightPin(uint8_t pin);
    bool testSDCardPresence();
    bool testRFIDConnection();
    bool testTouchController();
    bool testAudioI2S();
    
    // GPIO test helpers
    bool testGPIOOutput(uint8_t pin, uint32_t testDurationMs = 50);
    bool testGPIOInput(uint8_t pin, bool pullup = true);
    
    // SPI probe helpers
    bool probeSPIDevice(uint8_t cs_pin, uint8_t expected_response = 0);
    
public:
    HardwareDetector();
    
    // IHardwareDetector interface implementation
    CYDModel detectModel() override;
    DisplayDriver detectDisplayDriver() override;
    uint8_t detectBacklightPin() override;
    bool detectSDCard() override;
    bool detectRFID() override;
    bool detectTouch() override;
    bool detectAudio() override;
    CYDHardwareConfig getConfiguration() override;
    void reportDiagnostics(bool verbose = false) override;
    
    // Additional utility methods
    bool runFullDetection();
    bool isDetectionComplete() const { return detectionComplete; }
    uint32_t getDetectionTime() const;
    
    // Convert between config formats
    HardwareConfig toHardwareConfig() const;
    static CYDHardwareConfig fromHardwareConfig(const HardwareConfig& hwConfig);
};

#endif // HARDWARE_DETECTOR_H