/**
 * GPIO27Manager.h - GPIO27 pin multiplexing manager
 * 
 * Manages the GPIO27 pin conflict between backlight control and RFID MOSI
 * on CYD Dual USB models using time-division multiplexing.
 */

#ifndef GPIO27_MANAGER_H
#define GPIO27_MANAGER_H

#include <Arduino.h>

/**
 * GPIO27 Usage Modes
 */
enum GPIO27Mode {
    GPIO27_DISABLED = 0,        // Pin not in use
    GPIO27_BACKLIGHT = 1,       // Backlight control mode
    GPIO27_RFID_MOSI = 2,       // RFID MOSI mode
    GPIO27_HIGH_Z = 3           // High impedance (input)
};

/**
 * Pin State Information for diagnostics
 */
struct GPIO27State {
    GPIO27Mode currentMode;
    GPIO27Mode requestedMode;
    bool isBacklightOn;
    uint32_t lastModeChange;
    uint32_t totalSwitches;
    uint32_t rfidOperations;
    bool conflictDetected;
};

/**
 * GPIO27 Manager Class
 * 
 * Provides safe multiplexing of GPIO27 between backlight and RFID functions
 * with automatic conflict detection and resolution.
 */
class GPIO27Manager {
private:
    static GPIO27Manager* instance;
    static const uint8_t GPIO27_PIN = 27;
    
    GPIO27State state;
    bool initialized;
    bool backlightRequired;
    bool rfidEnabled;
    
    // Timing parameters
    static const uint32_t MODE_SWITCH_DELAY_US = 100;
    static const uint32_t BACKLIGHT_SETUP_TIME_US = 50;
    static const uint32_t RFID_SETUP_TIME_US = 20;
    
    // Internal methods
    void setPhysicalMode(GPIO27Mode mode);
    void enforceSetupTime(GPIO27Mode newMode);
    
    // Safety checks
    bool isModeSwitchSafe();
    void detectConflicts();
    
public:
    GPIO27Manager();
    ~GPIO27Manager();
    
    // Singleton access
    static GPIO27Manager& getInstance();
    
    // Initialization and configuration
    bool initialize(bool hasBacklight, bool hasRFID);
    void setBacklightRequired(bool required);
    void setRFIDEnabled(bool enabled);
    
    // Mode management
    bool requestBacklightMode();
    bool requestRFIDMode();
    bool requestHighZMode();
    bool releasePin();
    
    // Backlight operations
    bool setBacklight(bool on);
    bool isBacklightOn() const { return state.isBacklightOn; }
    
    // RFID operation helpers
    bool beginRFIDOperation();
    void endRFIDOperation();
    bool isRFIDModeActive() const { return state.currentMode == GPIO27_RFID_MOSI; }
    
    // State queries
    GPIO27Mode getCurrentMode() const { return state.currentMode; }
    GPIO27State getState() const { return state; }
    bool hasConflict() const { return state.conflictDetected; }
    
    // Diagnostics and debugging
    void printDiagnostics() const;
    void resetStatistics();
    uint32_t getSwitchCount() const { return state.totalSwitches; }
    uint32_t getRFIDOperationCount() const { return state.rfidOperations; }
    
    // Configuration validation
    bool validateConfiguration() const;
    
    // Critical section helpers for RFID timing
    class RFIDCriticalSection {
    private:
        GPIO27Manager* manager;
        bool wasSuccessful;
        
    public:
        RFIDCriticalSection(GPIO27Manager* mgr);
        ~RFIDCriticalSection();
        bool isActive() const { return wasSuccessful; }
    };
};

/**
 * RAII helper macros for common operations
 */
#define GPIO27_BACKLIGHT_MODE() \
    GPIO27Manager::getInstance().requestBacklightMode()

#define GPIO27_RFID_MODE() \
    GPIO27Manager::RFIDCriticalSection _rfid_cs(&GPIO27Manager::getInstance())

#define GPIO27_SET_BACKLIGHT(state) \
    GPIO27Manager::getInstance().setBacklight(state)

/**
 * Configuration Constants
 */
namespace GPIO27Config {
    // Pin conflict resolution policy
    enum ConflictPolicy {
        BACKLIGHT_PRIORITY = 0,     // Backlight takes precedence
        RFID_PRIORITY = 1,          // RFID takes precedence
        TIME_DIVISION = 2           // Time-division multiplexing (default)
    };
    
    // Default timing parameters (microseconds)
    static const uint32_t DEFAULT_SETUP_TIME = 50;
    static const uint32_t RFID_HOLD_TIME = 20;
    static const uint32_t BACKLIGHT_HOLD_TIME = 100;
    
    // Safety limits
    static const uint32_t MAX_SWITCHES_PER_SECOND = 1000;
    static const uint32_t CONFLICT_DETECTION_THRESHOLD = 10;
}

#endif // GPIO27_MANAGER_H