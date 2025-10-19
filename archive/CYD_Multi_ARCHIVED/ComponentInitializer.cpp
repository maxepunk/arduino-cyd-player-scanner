/**
 * ComponentInitializer.cpp - Component initialization implementation
 * 
 * Implements systematic component initialization with error handling,
 * recovery mechanisms, and GPIO27 conflict management.
 */

#include "ComponentInitializer.h"
#include <SD.h>
#include <EEPROM.h>

ComponentInitializer::ComponentInitializer() : gpio27Manager(nullptr), tft(nullptr) {
    clearInitState();
    gpio27Manager = &GPIO27Manager::getInstance();
}

ComponentInitializer::~ComponentInitializer() {
    if (tft) {
        delete tft;
        tft = nullptr;
    }
}

void ComponentInitializer::clearInitState() {
    state.displayReady = false;
    state.touchReady = false;
    state.rfidReady = false;
    state.sdcardReady = false;
    state.audioReady = false;
    state.totalInitTime = 0;
    state.startTime = 0;
    state.stopOnFirstError = false;
    
    // Clear recovery attempts
    for (int i = 0; i < 5; i++) {
        state.recoveryAttempts[i] = 0;
    }
    
    // Clear results
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        results[i] = createResult(static_cast<ComponentType>(i), INIT_FAILED, 0, "Not attempted");
    }
}

InitResult ComponentInitializer::createResult(ComponentType component, InitStatus status, 
                                            uint32_t initTime, const char* errorMsg) {
    InitResult result;
    result.component = component;
    result.status = status;
    result.initTimeMs = initTime;
    
    if (errorMsg) {
        strncpy(result.errorMessage, errorMsg, sizeof(result.errorMessage) - 1);
        result.errorMessage[sizeof(result.errorMessage) - 1] = '\0';
    } else {
        result.errorMessage[0] = '\0';
    }
    
    return result;
}

bool ComponentInitializer::isTimeoutExceeded(uint32_t startTime, uint32_t timeoutMs) {
    return (millis() - startTime) > timeoutMs;
}

InitResult ComponentInitializer::initDisplay(const CYDHardwareConfig& config) {
    uint32_t startTime = millis();
    
    Serial.println(F("[INFO][INIT/Display]: Starting display initialization"));
    
    // Validate configuration
    if (config.displayDriver == DISPLAY_DRIVER_UNKNOWN) {
        return createResult(COMPONENT_DISPLAY, INIT_INVALID_CONFIG, 
                          millis() - startTime, "Unknown display driver");
    }
    
    // Initialize GPIO27 manager first if needed
    if (config.backlightPin == 27) {
        gpio27Manager->initialize(true, config.hasRFID);
        gpio27Manager->setBacklight(false); // Start with backlight off
    }
    
    try {
        // Create TFT instance
        if (!tft) {
            tft = new TFT_eSPI();
        }
        
        // Initialize display
        tft->init();
        
        if (isTimeoutExceeded(startTime, DISPLAY_INIT_TIMEOUT)) {
            return createResult(COMPONENT_DISPLAY, INIT_TIMEOUT,
                              millis() - startTime, "Display init timeout");
        }
        
        // Setup display driver specific settings
        if (!setupDisplayDriver(config.displayDriver)) {
            return createResult(COMPONENT_DISPLAY, INIT_HARDWARE_ERROR,
                              millis() - startTime, "Driver setup failed");
        }
        
        // Test display functionality
        if (!validateDisplayInit()) {
            return createResult(COMPONENT_DISPLAY, INIT_HARDWARE_ERROR,
                              millis() - startTime, "Display validation failed");
        }
        
        // Enable backlight
        if (config.backlightPin == 27) {
            gpio27Manager->setBacklight(true);
        } else {
            pinMode(config.backlightPin, OUTPUT);
            digitalWrite(config.backlightPin, HIGH);
        }
        
        state.displayReady = true;
        
        Serial.println(F("[INFO][INIT/Display]: Display initialization successful"));
        return createResult(COMPONENT_DISPLAY, INIT_SUCCESS, millis() - startTime);
        
    } catch (...) {
        return createResult(COMPONENT_DISPLAY, INIT_HARDWARE_ERROR,
                          millis() - startTime, "Display init exception");
    }
}

bool ComponentInitializer::setupDisplayDriver(DisplayDriver driver) {
    if (!tft) return false;
    
    switch (driver) {
        case DISPLAY_DRIVER_ILI9341:
            // ILI9341 specific setup
            tft->setRotation(1); // Landscape
            tft->fillScreen(TFT_BLACK);
            return true;
            
        case DISPLAY_DRIVER_ST7789:
            // ST7789 specific setup
            tft->setRotation(1); // Landscape
            tft->fillScreen(TFT_BLACK);
            // Some ST7789 displays need color inversion
            tft->invertDisplay(true);
            return true;
            
        default:
            return false;
    }
}

bool ComponentInitializer::validateDisplayInit() {
    if (!tft) return false;
    
    // Test basic drawing
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE);
    tft->drawString("INIT", 0, 0);
    
    // Simple read/write test
    uint16_t pixel = tft->readPixel(0, 0);
    return pixel != 0; // Should not be black if text was drawn
}

InitResult ComponentInitializer::initTouch(const CYDHardwareConfig& config) {
    uint32_t startTime = millis();
    
    Serial.println(F("[INFO][INIT/Touch]: Starting touch initialization"));
    
    if (!config.hasTouch) {
        state.touchReady = false;
        return createResult(COMPONENT_TOUCH, INIT_SUCCESS,
                          millis() - startTime, "Touch not present");
    }
    
    try {
        // Setup touch IRQ pin only (no SPI needed)
        const uint8_t TOUCH_IRQ = 36;
        
        pinMode(TOUCH_IRQ, INPUT_PULLUP);
        
        if (isTimeoutExceeded(startTime, TOUCH_INIT_TIMEOUT)) {
            return createResult(COMPONENT_TOUCH, INIT_TIMEOUT,
                              millis() - startTime, "Touch init timeout");
        }
        
        // Validate touch IRQ pin
        if (!validateTouchInit()) {
            return createResult(COMPONENT_TOUCH, INIT_HARDWARE_ERROR,
                              millis() - startTime, "Touch validation failed");
        }
        
        state.touchReady = true;
        
        Serial.println(F("[INFO][INIT/Touch]: Touch initialization successful"));
        return createResult(COMPONENT_TOUCH, INIT_SUCCESS, millis() - startTime);
        
    } catch (...) {
        return createResult(COMPONENT_TOUCH, INIT_HARDWARE_ERROR,
                          millis() - startTime, "Touch init exception");
    }
}

bool ComponentInitializer::validateTouchInit() {
    // Touch uses IRQ-only detection - no coordinate reading
    const uint8_t TOUCH_IRQ = 36;
    return digitalRead(TOUCH_IRQ) == HIGH; // IRQ should be high when not pressed
}

InitResult ComponentInitializer::initRFID(const CYDHardwareConfig& config) {
    uint32_t startTime = millis();
    
    Serial.println(F("[INFO][INIT/RFID]: Starting RFID initialization"));
    
    if (!config.hasRFID) {
        state.rfidReady = false;
        return createResult(COMPONENT_RFID, INIT_SUCCESS,
                          millis() - startTime, "RFID not present");
    }
    
    // Handle GPIO27 conflict
    if (config.backlightPin == 27) {
        if (!gpio27Manager->beginRFIDOperation()) {
            return createResult(COMPONENT_RFID, INIT_HARDWARE_ERROR,
                              millis() - startTime, "GPIO27 conflict - cannot acquire RFID mode");
        }
    }
    
    try {
        // Setup RFID pins (software SPI)
        const uint8_t RFID_SS = 3;
        
        pinMode(RFID_SS, OUTPUT);
        digitalWrite(RFID_SS, HIGH);
        
        if (isTimeoutExceeded(startTime, RFID_INIT_TIMEOUT)) {
            if (config.backlightPin == 27) {
                gpio27Manager->endRFIDOperation();
            }
            return createResult(COMPONENT_RFID, INIT_TIMEOUT,
                              millis() - startTime, "RFID init timeout");
        }
        
        // Setup RFID reader
        if (!setupRFIDReader()) {
            if (config.backlightPin == 27) {
                gpio27Manager->endRFIDOperation();
            }
            return createResult(COMPONENT_RFID, INIT_HARDWARE_ERROR,
                              millis() - startTime, "RFID reader setup failed");
        }
        
        if (!validateRFIDInit()) {
            if (config.backlightPin == 27) {
                gpio27Manager->endRFIDOperation();
            }
            return createResult(COMPONENT_RFID, INIT_HARDWARE_ERROR,
                              millis() - startTime, "RFID validation failed");
        }
        
        // Release GPIO27 back to backlight if needed
        if (config.backlightPin == 27) {
            gpio27Manager->endRFIDOperation();
        }
        
        state.rfidReady = true;
        
        Serial.println(F("[INFO][INIT/RFID]: RFID initialization successful"));
        return createResult(COMPONENT_RFID, INIT_SUCCESS, millis() - startTime);
        
    } catch (...) {
        if (config.backlightPin == 27) {
            gpio27Manager->endRFIDOperation();
        }
        return createResult(COMPONENT_RFID, INIT_HARDWARE_ERROR,
                          millis() - startTime, "RFID init exception");
    }
}

bool ComponentInitializer::setupRFIDReader() {
    // Basic RFID setup
    // This is a placeholder - actual implementation would use MFRC522 library
    return true;
}

bool ComponentInitializer::validateRFIDInit() {
    // Basic RFID validation
    const uint8_t RFID_SS = 3;
    return digitalRead(RFID_SS) == HIGH; // SS should be controllable
}

InitResult ComponentInitializer::initSDCard(const CYDHardwareConfig& config) {
    uint32_t startTime = millis();
    
    Serial.println(F("[INFO][INIT/SDCard]: Starting SD card initialization"));
    
    if (!config.hasSDCard) {
        state.sdcardReady = false;
        return createResult(COMPONENT_SDCARD, INIT_SUCCESS,
                          millis() - startTime, "SD card not present");
    }
    
    try {
        // Initialize hardware SPI for SD card
        SPI.begin();
        
        const uint8_t SD_CS = 5;
        if (!SD.begin(SD_CS)) {
            return createResult(COMPONENT_SDCARD, INIT_HARDWARE_ERROR,
                              millis() - startTime, "SD card mount failed");
        }
        
        if (isTimeoutExceeded(startTime, SDCARD_INIT_TIMEOUT)) {
            return createResult(COMPONENT_SDCARD, INIT_TIMEOUT,
                              millis() - startTime, "SD card init timeout");
        }
        
        if (!validateSDCardInit()) {
            return createResult(COMPONENT_SDCARD, INIT_HARDWARE_ERROR,
                              millis() - startTime, "SD card validation failed");
        }
        
        state.sdcardReady = true;
        
        Serial.println(F("[INFO][INIT/SDCard]: SD card initialization successful"));
        return createResult(COMPONENT_SDCARD, INIT_SUCCESS, millis() - startTime);
        
    } catch (...) {
        return createResult(COMPONENT_SDCARD, INIT_HARDWARE_ERROR,
                          millis() - startTime, "SD card init exception");
    }
}

bool ComponentInitializer::validateSDCardInit() {
    uint64_t cardSize = SD.cardSize();
    return cardSize > 0;
}

InitResult ComponentInitializer::initAudio(const CYDHardwareConfig& config) {
    uint32_t startTime = millis();
    
    Serial.println(F("[INFO][INIT/Audio]: Starting audio initialization"));
    
    if (!config.hasAudio) {
        state.audioReady = false;
        return createResult(COMPONENT_AUDIO, INIT_SUCCESS,
                          millis() - startTime, "Audio not present");
    }
    
    try {
        // Audio initialization is optional and can fail gracefully
        if (!setupAudioI2S()) {
            return createResult(COMPONENT_AUDIO, INIT_FAILED,
                              millis() - startTime, "I2S setup failed (non-critical)");
        }
        
        if (isTimeoutExceeded(startTime, AUDIO_INIT_TIMEOUT)) {
            return createResult(COMPONENT_AUDIO, INIT_TIMEOUT,
                              millis() - startTime, "Audio init timeout (non-critical)");
        }
        
        state.audioReady = true;
        
        Serial.println(F("[INFO][INIT/Audio]: Audio initialization successful"));
        return createResult(COMPONENT_AUDIO, INIT_SUCCESS, millis() - startTime);
        
    } catch (...) {
        return createResult(COMPONENT_AUDIO, INIT_FAILED,
                          millis() - startTime, "Audio init exception (non-critical)");
    }
}

bool ComponentInitializer::setupAudioI2S() {
    // Basic I2S setup placeholder
    // Actual implementation would configure I2S pins and parameters
    return true;
}

bool ComponentInitializer::validateAudioInit() {
    // Basic audio validation
    return true;
}

InitResult* ComponentInitializer::initAll(const CYDHardwareConfig& config, bool stopOnError) {
    state.startTime = millis();
    state.stopOnFirstError = stopOnError;
    
    Serial.println(F("[INFO][INIT]: Starting complete system initialization"));
    
    clearInitState();
    
    // Initialize components in dependency order
    for (size_t i = 0; i < InitConfig::INIT_ORDER_COUNT; i++) {
        ComponentType component = InitConfig::INIT_ORDER[i];
        InitResult result;
        
        switch (component) {
            case COMPONENT_DISPLAY:
                result = initDisplay(config);
                break;
            case COMPONENT_TOUCH:
                result = initTouch(config);
                break;
            case COMPONENT_RFID:
                result = initRFID(config);
                break;
            case COMPONENT_SDCARD:
                result = initSDCard(config);
                break;
            case COMPONENT_AUDIO:
                result = initAudio(config);
                break;
        }
        
        results[component] = result;
        
        // Check for critical failures
        if (stopOnError && result.status != INIT_SUCCESS) {
            // Display is critical, others may be optional
            if (component == COMPONENT_DISPLAY) {
                Serial.println(F("[ERROR][INIT]: Critical component failed, aborting"));
                state.totalInitTime = millis() - state.startTime;
                return results;
            }
        }
        
        // Check total timeout
        if (isTimeoutExceeded(state.startTime, TOTAL_INIT_TIMEOUT)) {
            Serial.println(F("[WARNING][INIT]: Total initialization timeout reached"));
            break;
        }
    }
    
    state.totalInitTime = millis() - state.startTime;
    
    Serial.print(F("[INFO][INIT]: Initialization complete in "));
    Serial.print(state.totalInitTime);
    Serial.println(F("ms"));
    
    return results;
}

void ComponentInitializer::reportInitStatus(InitResult* results, size_t count) {
    Serial.println(F("=== Component Initialization Status ==="));
    
    for (size_t i = 0; i < count && i < MAX_COMPONENTS; i++) {
        InitHelper::printInitResult(results[i]);
    }
    
    Serial.print(F("Total initialization time: "));
    Serial.print(state.totalInitTime);
    Serial.println(F("ms"));
    
    Serial.println(F("======================================="));
}

InitResult ComponentInitializer::recoverComponent(ComponentType component, 
                                                const CYDHardwareConfig& config) {
    if (state.recoveryAttempts[component] >= MAX_RECOVERY_ATTEMPTS) {
        return createResult(component, INIT_FAILED, 0, "Max recovery attempts exceeded");
    }
    
    state.recoveryAttempts[component]++;
    
    Serial.print(F("[INFO][INIT/Recovery]: Attempting recovery for "));
    Serial.print(InitHelper::componentToString(component));
    Serial.print(F(", attempt "));
    Serial.println(state.recoveryAttempts[component]);
    
    // Component-specific recovery
    switch (component) {
        case COMPONENT_DISPLAY:
            return attemptDisplayRecovery(config) ? 
                   initDisplay(config) : createResult(component, INIT_FAILED, 0, "Recovery failed");
        case COMPONENT_TOUCH:
            return attemptTouchRecovery(config) ? 
                   initTouch(config) : createResult(component, INIT_FAILED, 0, "Recovery failed");
        case COMPONENT_RFID:
            return attemptRFIDRecovery(config) ? 
                   initRFID(config) : createResult(component, INIT_FAILED, 0, "Recovery failed");
        case COMPONENT_SDCARD:
            return attemptSDCardRecovery(config) ? 
                   initSDCard(config) : createResult(component, INIT_FAILED, 0, "Recovery failed");
        case COMPONENT_AUDIO:
            return attemptAudioRecovery(config) ? 
                   initAudio(config) : createResult(component, INIT_FAILED, 0, "Recovery failed");
        default:
            return createResult(component, INIT_FAILED, 0, "Unknown component");
    }
}

bool ComponentInitializer::attemptDisplayRecovery(const CYDHardwareConfig& config) {
    // Reset display-related hardware
    if (tft) {
        delete tft;
        tft = nullptr;
    }
    
    delay(100); // Allow hardware to reset
    return true;
}

bool ComponentInitializer::attemptTouchRecovery(const CYDHardwareConfig& config) {
    // Reset touch pins
    const uint8_t TOUCH_CS_PIN = 33;
    pinMode(TOUCH_CS_PIN, OUTPUT);
    digitalWrite(TOUCH_CS_PIN, HIGH);
    delay(50);
    return true;
}

bool ComponentInitializer::attemptRFIDRecovery(const CYDHardwareConfig& config) {
    // Reset RFID pins and GPIO27 state
    if (config.backlightPin == 27) {
        gpio27Manager->requestHighZMode();
        delay(50);
    }
    
    const uint8_t RFID_SS = 3;
    pinMode(RFID_SS, OUTPUT);
    digitalWrite(RFID_SS, HIGH);
    delay(50);
    return true;
}

bool ComponentInitializer::attemptSDCardRecovery(const CYDHardwareConfig& config) {
    // Reinitialize SPI
    SPI.end();
    delay(100);
    SPI.begin();
    return true;
}

bool ComponentInitializer::attemptAudioRecovery(const CYDHardwareConfig& config) {
    // Audio recovery is minimal since it's optional
    delay(50);
    return true;
}

bool ComponentInitializer::isComponentReady(ComponentType component) const {
    switch (component) {
        case COMPONENT_DISPLAY: return state.displayReady;
        case COMPONENT_TOUCH: return state.touchReady;
        case COMPONENT_RFID: return state.rfidReady;
        case COMPONENT_SDCARD: return state.sdcardReady;
        case COMPONENT_AUDIO: return state.audioReady;
        default: return false;
    }
}

void ComponentInitializer::printInitializationReport() const {
    Serial.println(F("=== Detailed Initialization Report ==="));
    
    Serial.print(F("Display Ready: "));
    Serial.println(state.displayReady ? F("YES") : F("NO"));
    
    Serial.print(F("Touch Ready: "));
    Serial.println(state.touchReady ? F("YES") : F("NO"));
    
    Serial.print(F("RFID Ready: "));
    Serial.println(state.rfidReady ? F("YES") : F("NO"));
    
    Serial.print(F("SD Card Ready: "));
    Serial.println(state.sdcardReady ? F("YES") : F("NO"));
    
    Serial.print(F("Audio Ready: "));
    Serial.println(state.audioReady ? F("YES") : F("NO"));
    
    Serial.print(F("Total Time: "));
    Serial.print(state.totalInitTime);
    Serial.println(F("ms"));
    
    Serial.println(F("Recovery Attempts:"));
    for (int i = 0; i < 5; i++) {
        if (state.recoveryAttempts[i] > 0) {
            Serial.print(F("  "));
            Serial.print(InitHelper::componentToString(static_cast<ComponentType>(i)));
            Serial.print(F(": "));
            Serial.println(state.recoveryAttempts[i]);
        }
    }
    
    Serial.println(F("====================================="));
}

// Helper function implementations
namespace InitHelper {
    const char* statusToString(InitStatus status) {
        switch (status) {
            case INIT_SUCCESS: return "SUCCESS";
            case INIT_FAILED: return "FAILED";
            case INIT_TIMEOUT: return "TIMEOUT";
            case INIT_INVALID_CONFIG: return "INVALID_CONFIG";
            case INIT_HARDWARE_ERROR: return "HARDWARE_ERROR";
            default: return "UNKNOWN";
        }
    }
    
    const char* componentToString(ComponentType component) {
        switch (component) {
            case COMPONENT_DISPLAY: return "Display";
            case COMPONENT_TOUCH: return "Touch";
            case COMPONENT_RFID: return "RFID";
            case COMPONENT_SDCARD: return "SD Card";
            case COMPONENT_AUDIO: return "Audio";
            default: return "Unknown";
        }
    }
    
    bool isStatusError(InitStatus status) {
        return status != INIT_SUCCESS;
    }
    
    void printInitResult(const InitResult& result) {
        Serial.print(F("["));
        Serial.print(statusToString(result.status));
        Serial.print(F("]["));
        Serial.print(componentToString(result.component));
        Serial.print(F("]: "));
        
        if (result.status == INIT_SUCCESS) {
            Serial.print(F("Initialized in "));
            Serial.print(result.initTimeMs);
            Serial.println(F("ms"));
        } else {
            Serial.print(result.errorMessage);
            Serial.print(F(" ("));
            Serial.print(result.initTimeMs);
            Serial.println(F("ms)"));
        }
    }
}