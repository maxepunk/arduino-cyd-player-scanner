/**
 * GPIO27Manager.cpp - GPIO27 pin multiplexing implementation
 * 
 * Implements time-division multiplexing for GPIO27 between backlight and RFID.
 * Uses critical sections and timing control to prevent conflicts.
 */

#include "GPIO27Manager.h"

// Static member initialization
GPIO27Manager* GPIO27Manager::instance = nullptr;

GPIO27Manager::GPIO27Manager() : initialized(false), backlightRequired(false), rfidEnabled(false) {
    // Initialize state
    state.currentMode = GPIO27_DISABLED;
    state.requestedMode = GPIO27_DISABLED;
    state.isBacklightOn = false;
    state.lastModeChange = 0;
    state.totalSwitches = 0;
    state.rfidOperations = 0;
    state.conflictDetected = false;
}

GPIO27Manager::~GPIO27Manager() {
    if (initialized) {
        releasePin();
    }
}

GPIO27Manager& GPIO27Manager::getInstance() {
    if (instance == nullptr) {
        instance = new GPIO27Manager();
    }
    return *instance;
}

bool GPIO27Manager::initialize(bool hasBacklight, bool hasRFID) {
    if (initialized) {
        return true; // Already initialized
    }
    
    backlightRequired = hasBacklight;
    rfidEnabled = hasRFID;
    
    // Configure GPIO27 as output initially
    pinMode(GPIO27_PIN, OUTPUT);
    digitalWrite(GPIO27_PIN, LOW);
    
    // Initialize to appropriate mode
    if (backlightRequired && !rfidEnabled) {
        setPhysicalMode(GPIO27_BACKLIGHT);
    } else if (!backlightRequired && rfidEnabled) {
        setPhysicalMode(GPIO27_RFID_MOSI);
    } else if (backlightRequired && rfidEnabled) {
        // Default to backlight mode, RFID will request as needed
        setPhysicalMode(GPIO27_BACKLIGHT);
    } else {
        setPhysicalMode(GPIO27_HIGH_Z);
    }
    
    initialized = true;
    
    Serial.print(F("[INFO][GPIO27/Manager]: Initialized - Backlight: "));
    Serial.print(backlightRequired ? F("Yes") : F("No"));
    Serial.print(F(", RFID: "));
    Serial.println(rfidEnabled ? F("Yes") : F("No"));
    
    return true;
}

void GPIO27Manager::setBacklightRequired(bool required) {
    backlightRequired = required;
    if (!required && state.currentMode == GPIO27_BACKLIGHT) {
        // Switch away from backlight mode if no longer needed
        if (rfidEnabled) {
            setPhysicalMode(GPIO27_RFID_MOSI);
        } else {
            setPhysicalMode(GPIO27_HIGH_Z);
        }
    }
}

void GPIO27Manager::setRFIDEnabled(bool enabled) {
    rfidEnabled = enabled;
    if (!enabled && state.currentMode == GPIO27_RFID_MOSI) {
        // Switch away from RFID mode if no longer needed
        if (backlightRequired) {
            setPhysicalMode(GPIO27_BACKLIGHT);
        } else {
            setPhysicalMode(GPIO27_HIGH_Z);
        }
    }
}

bool GPIO27Manager::requestBacklightMode() {
    if (!initialized || !backlightRequired) {
        return false;
    }
    
    if (state.currentMode == GPIO27_BACKLIGHT) {
        return true; // Already in backlight mode
    }
    
    if (!isModeSwitchSafe()) {
        Serial.println(F("[WARNING][GPIO27/Manager]: Mode switch blocked - unsafe timing"));
        return false;
    }
    
    state.requestedMode = GPIO27_BACKLIGHT;
    setPhysicalMode(GPIO27_BACKLIGHT);
    
    return true;
}

bool GPIO27Manager::requestRFIDMode() {
    if (!initialized || !rfidEnabled) {
        return false;
    }
    
    if (state.currentMode == GPIO27_RFID_MOSI) {
        return true; // Already in RFID mode
    }
    
    if (!isModeSwitchSafe()) {
        Serial.println(F("[WARNING][GPIO27/Manager]: RFID mode request blocked - unsafe timing"));
        return false;
    }
    
    state.requestedMode = GPIO27_RFID_MOSI;
    setPhysicalMode(GPIO27_RFID_MOSI);
    
    return true;
}

bool GPIO27Manager::requestHighZMode() {
    if (!initialized) {
        return false;
    }
    
    if (state.currentMode == GPIO27_HIGH_Z) {
        return true; // Already in high-Z mode
    }
    
    state.requestedMode = GPIO27_HIGH_Z;
    setPhysicalMode(GPIO27_HIGH_Z);
    
    return true;
}

bool GPIO27Manager::releasePin() {
    if (!initialized) {
        return false;
    }
    
    // Set to high impedance and mark as disabled
    pinMode(GPIO27_PIN, INPUT);
    state.currentMode = GPIO27_DISABLED;
    state.requestedMode = GPIO27_DISABLED;
    
    return true;
}

void GPIO27Manager::setPhysicalMode(GPIO27Mode mode) {
    GPIO27Mode previousMode = state.currentMode;
    
    // Enforce setup time if switching modes
    if (previousMode != mode && previousMode != GPIO27_DISABLED) {
        enforceSetupTime(mode);
    }
    
    switch (mode) {
        case GPIO27_BACKLIGHT:
            pinMode(GPIO27_PIN, OUTPUT);
            // Backlight state will be set separately via setBacklight()
            break;
            
        case GPIO27_RFID_MOSI:
            pinMode(GPIO27_PIN, OUTPUT);
            digitalWrite(GPIO27_PIN, LOW); // MOSI idle state
            break;
            
        case GPIO27_HIGH_Z:
            pinMode(GPIO27_PIN, INPUT);
            break;
            
        case GPIO27_DISABLED:
        default:
            pinMode(GPIO27_PIN, INPUT);
            break;
    }
    
    // Update state
    state.currentMode = mode;
    state.lastModeChange = micros();
    
    if (previousMode != mode) {
        state.totalSwitches++;
        
        // Check for potential conflicts
        detectConflicts();
    }
}

void GPIO27Manager::enforceSetupTime(GPIO27Mode newMode) {
    uint32_t requiredSetupTime;
    
    switch (newMode) {
        case GPIO27_BACKLIGHT:
            requiredSetupTime = BACKLIGHT_SETUP_TIME_US;
            break;
        case GPIO27_RFID_MOSI:
            requiredSetupTime = RFID_SETUP_TIME_US;
            break;
        default:
            requiredSetupTime = MODE_SWITCH_DELAY_US;
            break;
    }
    
    uint32_t elapsed = micros() - state.lastModeChange;
    if (elapsed < requiredSetupTime) {
        delayMicroseconds(requiredSetupTime - elapsed);
    }
}

bool GPIO27Manager::isModeSwitchSafe() {
    uint32_t elapsed = micros() - state.lastModeChange;
    return elapsed >= MODE_SWITCH_DELAY_US;
}

void GPIO27Manager::detectConflicts() {
    // Check if switching too frequently
    static uint32_t lastConflictCheck = 0;
    static uint32_t switchesAtLastCheck = 0;
    
    uint32_t now = millis();
    if (now - lastConflictCheck >= 1000) { // Check every second
        uint32_t switchesPerSecond = state.totalSwitches - switchesAtLastCheck;
        
        if (switchesPerSecond > GPIO27Config::MAX_SWITCHES_PER_SECOND) {
            state.conflictDetected = true;
            Serial.print(F("[ERROR][GPIO27/Manager]: Excessive switching detected: "));
            Serial.print(switchesPerSecond);
            Serial.println(F(" switches/sec"));
        }
        
        lastConflictCheck = now;
        switchesAtLastCheck = state.totalSwitches;
    }
}

bool GPIO27Manager::setBacklight(bool on) {
    if (!initialized || !backlightRequired) {
        return false;
    }
    
    // Request backlight mode if not already active
    if (state.currentMode != GPIO27_BACKLIGHT) {
        if (!requestBacklightMode()) {
            return false;
        }
    }
    
    // Set backlight state
    digitalWrite(GPIO27_PIN, on ? HIGH : LOW);
    state.isBacklightOn = on;
    
    return true;
}

bool GPIO27Manager::beginRFIDOperation() {
    if (!initialized || !rfidEnabled) {
        return false;
    }
    
    bool success = requestRFIDMode();
    if (success) {
        state.rfidOperations++;
    }
    
    return success;
}

void GPIO27Manager::endRFIDOperation() {
    if (!initialized) {
        return;
    }
    
    // If backlight is required, switch back to backlight mode
    if (backlightRequired && state.isBacklightOn) {
        requestBacklightMode();
        digitalWrite(GPIO27_PIN, HIGH); // Restore backlight state
    }
}

void GPIO27Manager::printDiagnostics() const {
    Serial.println(F("=== GPIO27 Manager Diagnostics ==="));
    
    Serial.print(F("Current mode: "));
    switch (state.currentMode) {
        case GPIO27_DISABLED:
            Serial.println(F("Disabled"));
            break;
        case GPIO27_BACKLIGHT:
            Serial.println(F("Backlight"));
            break;
        case GPIO27_RFID_MOSI:
            Serial.println(F("RFID MOSI"));
            break;
        case GPIO27_HIGH_Z:
            Serial.println(F("High-Z"));
            break;
    }
    
    Serial.print(F("Backlight required: "));
    Serial.println(backlightRequired ? F("Yes") : F("No"));
    
    Serial.print(F("RFID enabled: "));
    Serial.println(rfidEnabled ? F("Yes") : F("No"));
    
    Serial.print(F("Backlight state: "));
    Serial.println(state.isBacklightOn ? F("ON") : F("OFF"));
    
    Serial.print(F("Total mode switches: "));
    Serial.println(state.totalSwitches);
    
    Serial.print(F("RFID operations: "));
    Serial.println(state.rfidOperations);
    
    Serial.print(F("Conflicts detected: "));
    Serial.println(state.conflictDetected ? F("Yes") : F("No"));
    
    Serial.print(F("Time since last mode change: "));
    Serial.print(micros() - state.lastModeChange);
    Serial.println(F(" µs"));
    
    Serial.println(F("=================================="));
}

void GPIO27Manager::resetStatistics() {
    state.totalSwitches = 0;
    state.rfidOperations = 0;
    state.conflictDetected = false;
}

bool GPIO27Manager::validateConfiguration() const {
    if (!initialized) {
        return false;
    }
    
    // Check that we have at least one function enabled
    if (!backlightRequired && !rfidEnabled) {
        Serial.println(F("[WARNING][GPIO27/Manager]: No functions enabled for GPIO27"));
        return false;
    }
    
    // Check pin can be controlled
    int previousMode = digitalRead(GPIO27_PIN);
    pinMode(GPIO27_PIN, OUTPUT);
    digitalWrite(GPIO27_PIN, HIGH);
    delay(1);
    bool highTest = digitalRead(GPIO27_PIN) == HIGH;
    
    digitalWrite(GPIO27_PIN, LOW);
    delay(1);
    bool lowTest = digitalRead(GPIO27_PIN) == LOW;
    
    // Restore previous state
    digitalWrite(GPIO27_PIN, previousMode);
    
    if (!highTest || !lowTest) {
        Serial.println(F("[ERROR][GPIO27/Manager]: GPIO27 pin test failed"));
        return false;
    }
    
    return true;
}

// RAII Critical Section Implementation
GPIO27Manager::RFIDCriticalSection::RFIDCriticalSection(GPIO27Manager* mgr) 
    : manager(mgr), wasSuccessful(false) {
    if (manager) {
        wasSuccessful = manager->beginRFIDOperation();
        if (!wasSuccessful) {
            Serial.println(F("[ERROR][GPIO27/RFID]: Failed to acquire RFID mode"));
        }
    }
}

GPIO27Manager::RFIDCriticalSection::~RFIDCriticalSection() {
    if (manager && wasSuccessful) {
        manager->endRFIDOperation();
    }
}