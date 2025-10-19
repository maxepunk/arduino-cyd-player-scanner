/**
 * ComponentInitializer.h - Component initialization manager
 * 
 * Implements the component initialization contract with proper error handling,
 * timing constraints, and GPIO27 conflict resolution.
 */

#ifndef COMPONENT_INITIALIZER_H
#define COMPONENT_INITIALIZER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "HardwareDetector.h"
#include "GPIO27Manager.h"

// Import contract types
enum InitStatus {
    INIT_SUCCESS = 0,
    INIT_FAILED = 1,
    INIT_TIMEOUT = 2,
    INIT_INVALID_CONFIG = 3,
    INIT_HARDWARE_ERROR = 4
};

enum ComponentType {
    COMPONENT_DISPLAY = 0,
    COMPONENT_TOUCH = 1,
    COMPONENT_RFID = 2,
    COMPONENT_SDCARD = 3,
    COMPONENT_AUDIO = 4
};

struct InitResult {
    ComponentType component;
    InitStatus status;
    uint32_t initTimeMs;
    char errorMessage[64];
};

/**
 * Component Initializer Interface (from contract)
 */
class IComponentInitializer {
public:
    virtual ~IComponentInitializer() = default;
    virtual InitResult initDisplay(const CYDHardwareConfig& config) = 0;
    virtual InitResult initTouch(const CYDHardwareConfig& config) = 0;
    virtual InitResult initRFID(const CYDHardwareConfig& config) = 0;
    virtual InitResult initSDCard(const CYDHardwareConfig& config) = 0;
    virtual InitResult initAudio(const CYDHardwareConfig& config) = 0;
    virtual InitResult* initAll(const CYDHardwareConfig& config, bool stopOnError = false) = 0;
    virtual void reportInitStatus(InitResult* results, size_t count) = 0;
    virtual InitResult recoverComponent(ComponentType component, const CYDHardwareConfig& config) = 0;
};

/**
 * Component Initialization State Tracking
 */
struct InitializationState {
    bool displayReady;
    bool touchReady;
    bool rfidReady;
    bool sdcardReady;
    bool audioReady;
    uint32_t totalInitTime;
    uint8_t recoveryAttempts[5]; // Per component
    uint32_t startTime;
    bool stopOnFirstError;
};

/**
 * CYD Component Initializer Implementation
 */
class ComponentInitializer : public IComponentInitializer {
private:
    InitializationState state;
    GPIO27Manager* gpio27Manager;
    TFT_eSPI* tft;
    static const size_t MAX_COMPONENTS = 5;
    InitResult results[MAX_COMPONENTS];
    
    // Timing constants (milliseconds)
    static const uint32_t DISPLAY_INIT_TIMEOUT = 2000;
    static const uint32_t TOUCH_INIT_TIMEOUT = 1000;
    static const uint32_t RFID_INIT_TIMEOUT = 1500;
    static const uint32_t SDCARD_INIT_TIMEOUT = 2000;
    static const uint32_t AUDIO_INIT_TIMEOUT = 1000;
    static const uint32_t TOTAL_INIT_TIMEOUT = 5000;
    static const uint8_t MAX_RECOVERY_ATTEMPTS = 3;
    
    // Helper methods
    InitResult createResult(ComponentType component, InitStatus status, 
                          uint32_t initTime, const char* errorMsg = nullptr);
    bool isTimeoutExceeded(uint32_t startTime, uint32_t timeoutMs);
    void clearInitState();
    
    // Component-specific initialization helpers
    bool setupDisplayDriver(DisplayDriver driver);
    bool setupRFIDReader();
    bool setupSDCardInterface();
    bool setupAudioI2S();
    
    // Recovery helpers
    bool attemptDisplayRecovery(const CYDHardwareConfig& config);
    bool attemptTouchRecovery(const CYDHardwareConfig& config);
    bool attemptRFIDRecovery(const CYDHardwareConfig& config);
    bool attemptSDCardRecovery(const CYDHardwareConfig& config);
    bool attemptAudioRecovery(const CYDHardwareConfig& config);
    
    // Validation helpers
    bool validateDisplayInit();
    bool validateTouchInit();
    bool validateRFIDInit();
    bool validateSDCardInit();
    bool validateAudioInit();
    
public:
    ComponentInitializer();
    ~ComponentInitializer();
    
    // IComponentInitializer interface implementation
    InitResult initDisplay(const CYDHardwareConfig& config) override;
    InitResult initTouch(const CYDHardwareConfig& config) override;
    InitResult initRFID(const CYDHardwareConfig& config) override;
    InitResult initSDCard(const CYDHardwareConfig& config) override;
    InitResult initAudio(const CYDHardwareConfig& config) override;
    InitResult* initAll(const CYDHardwareConfig& config, bool stopOnError = false) override;
    void reportInitStatus(InitResult* results, size_t count) override;
    InitResult recoverComponent(ComponentType component, const CYDHardwareConfig& config) override;
    
    // Additional utility methods
    bool isComponentReady(ComponentType component) const;
    InitializationState getState() const { return state; }
    uint32_t getTotalInitTime() const { return state.totalInitTime; }
    
    // Component access (for use after initialization)
    TFT_eSPI* getTFT() { return tft; }
    
    // Diagnostics and troubleshooting
    void printInitializationReport() const;
    bool validateAllComponents() const;
    void resetRecoveryCounters();
};

/**
 * Initialization Helper Functions
 */
namespace InitHelper {
    const char* statusToString(InitStatus status);
    const char* componentToString(ComponentType component);
    bool isStatusError(InitStatus status);
    void printInitResult(const InitResult& result);
}

/**
 * Initialization Configuration
 */
namespace InitConfig {
    // Component initialization order (dependencies)
    static const ComponentType INIT_ORDER[] = {
        COMPONENT_DISPLAY,  // Must be first (provides visual feedback)
        COMPONENT_TOUCH,    // Can be parallel with others
        COMPONENT_SDCARD,   // Independent
        COMPONENT_RFID,     // Depends on GPIO27 management
        COMPONENT_AUDIO     // Optional, last
    };
    static const size_t INIT_ORDER_COUNT = 5;
    
    // Retry policies
    enum RetryPolicy {
        NO_RETRY = 0,
        RETRY_ONCE = 1,
        RETRY_AGGRESSIVE = 3
    };
    
    static const RetryPolicy COMPONENT_RETRY_POLICY[] = {
        RETRY_AGGRESSIVE,  // Display
        RETRY_ONCE,        // Touch
        RETRY_ONCE,        // RFID
        RETRY_ONCE,        // SD Card
        NO_RETRY           // Audio (optional)
    };
}

#endif // COMPONENT_INITIALIZER_H