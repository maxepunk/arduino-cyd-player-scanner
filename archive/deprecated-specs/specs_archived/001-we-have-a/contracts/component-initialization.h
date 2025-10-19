/**
 * Component Initialization Contract
 * 
 * Defines the interface for initializing all CYD components
 * with proper error handling and diagnostic reporting.
 */

#ifndef COMPONENT_INITIALIZATION_H
#define COMPONENT_INITIALIZATION_H

#include <Arduino.h>
#include "hardware-detection.h"

// Initialization status codes
enum InitStatus {
    INIT_SUCCESS = 0,
    INIT_FAILED = 1,
    INIT_TIMEOUT = 2,
    INIT_INVALID_CONFIG = 3,
    INIT_HARDWARE_ERROR = 4
};

// Component identifiers
enum ComponentType {
    COMPONENT_DISPLAY = 0,
    COMPONENT_TOUCH = 1,
    COMPONENT_RFID = 2,
    COMPONENT_SDCARD = 3,
    COMPONENT_AUDIO = 4
};

/**
 * Component Initialization Result
 */
struct InitResult {
    ComponentType component;
    InitStatus status;
    uint32_t initTimeMs;
    char errorMessage[64];
};

/**
 * Component Initializer Interface
 */
class IComponentInitializer {
public:
    virtual ~IComponentInitializer() = default;
    
    /**
     * Initialize display with correct driver
     * @param config Hardware configuration
     * @return Initialization result
     */
    virtual InitResult initDisplay(const CYDHardwareConfig& config) = 0;
    
    /**
     * Initialize touch controller with calibration
     * @param config Hardware configuration
     * @return Initialization result
     */
    virtual InitResult initTouch(const CYDHardwareConfig& config) = 0;
    
    /**
     * Initialize RFID reader with software SPI
     * @param config Hardware configuration
     * @return Initialization result
     */
    virtual InitResult initRFID(const CYDHardwareConfig& config) = 0;
    
    /**
     * Initialize SD card with hardware SPI
     * @param config Hardware configuration  
     * @return Initialization result
     */
    virtual InitResult initSDCard(const CYDHardwareConfig& config) = 0;
    
    /**
     * Initialize audio I2S output
     * @param config Hardware configuration
     * @return Initialization result
     */
    virtual InitResult initAudio(const CYDHardwareConfig& config) = 0;
    
    /**
     * Initialize all components in correct order
     * @param config Hardware configuration
     * @param stopOnError If true, stop at first failure
     * @return Array of initialization results
     */
    virtual InitResult* initAll(const CYDHardwareConfig& config, bool stopOnError = false) = 0;
    
    /**
     * Report initialization status
     * @param results Array of init results
     * @param count Number of results
     */
    virtual void reportInitStatus(InitResult* results, size_t count) = 0;
    
    /**
     * Attempt recovery for failed component
     * @param component Component to recover
     * @param config Hardware configuration
     * @return Recovery result
     */
    virtual InitResult recoverComponent(ComponentType component, const CYDHardwareConfig& config) = 0;
};

/**
 * Initialization Sequence Contract:
 * 
 * 1. Display must be initialized first (provides visual feedback)
 * 2. Touch can be initialized in parallel with other components
 * 3. RFID initialization must handle GPIO27 conflict if present
 * 4. SD card must use hardware SPI (VSPI)
 * 5. Audio initialization is optional (can fail gracefully)
 * 
 * Timing Requirements:
 * - Each component init must complete within 2 seconds
 * - Total initialization must complete within 5 seconds
 * - Recovery attempts limited to 3 per component
 * 
 * Error Handling:
 * - All errors must be reported via Serial
 * - Failed components must not prevent others from initializing
 * - System must remain responsive even with failures
 */

#endif // COMPONENT_INITIALIZATION_H