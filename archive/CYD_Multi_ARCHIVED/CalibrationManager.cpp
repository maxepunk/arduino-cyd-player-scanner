/**
 * CalibrationManager.cpp - Touch calibration EEPROM management implementation
 * 
 * Provides persistent touch calibration storage with model-specific defaults,
 * data validation, backup/recovery, and integrity checking.
 */

#include "CalibrationManager.h"

// Default calibration data for different CYD models
namespace DefaultCalibrations {
    const TouchCalibration CYD_SINGLE_USB_DEFAULT = {
        .xMin = 300, .xMax = 3700,      // Typical ILI9341 touch range
        .yMin = 200, .yMax = 3800,
        .swapXY = false,
        .invertX = false, .invertY = false,
        .rotation = 1,                   // Landscape
        .modelID = static_cast<uint8_t>(HardwareConfig::MODEL_CYD_SINGLE_USB),
        .hardwareRevision = 1,
        .calibrationDate = 0,
        .calibrationFlags = 0,
        .checksum = 0,                   // Will be calculated
        .version = CalibrationConfig::DATA_OFFSET,
        .reserved = {0, 0, 0},
        .minPressure = 200,
        .maxPressure = 1000,
        .debounceMs = 50,
        .sampleCount = 4
    };
    
    const TouchCalibration CYD_DUAL_USB_DEFAULT = {
        .xMin = 250, .xMax = 3750,      // Typical ST7789 touch range
        .yMin = 180, .yMax = 3850,
        .swapXY = false,
        .invertX = false, .invertY = false,
        .rotation = 1,                   // Landscape
        .modelID = static_cast<uint8_t>(HardwareConfig::MODEL_CYD_DUAL_USB),
        .hardwareRevision = 1,
        .calibrationDate = 0,
        .calibrationFlags = 0,
        .checksum = 0,                   // Will be calculated
        .version = CalibrationConfig::DATA_OFFSET,
        .reserved = {0, 0, 0},
        .minPressure = 250,
        .maxPressure = 1000,
        .debounceMs = 50,
        .sampleCount = 4
    };
    
    const TouchCalibration GENERIC_DEFAULT = {
        .xMin = 200, .xMax = 3800,      // Safe generic range
        .yMin = 200, .yMax = 3800,
        .swapXY = false,
        .invertX = false, .invertY = false,
        .rotation = 1,
        .modelID = 0,
        .hardwareRevision = 1,
        .calibrationDate = 0,
        .calibrationFlags = 0,
        .checksum = 0,
        .version = CalibrationConfig::DATA_OFFSET,
        .reserved = {0, 0, 0},
        .minPressure = 300,
        .maxPressure = 1000,
        .debounceMs = 100,
        .sampleCount = 4
    };
}

// Static member definitions
const DefaultCalibrationSet CalibrationManager::defaultCalibrations[] = {
    {HardwareConfig::MODEL_CYD_SINGLE_USB, DefaultCalibrations::CYD_SINGLE_USB_DEFAULT},
    {HardwareConfig::MODEL_CYD_DUAL_USB, DefaultCalibrations::CYD_DUAL_USB_DEFAULT},
    {HardwareConfig::MODEL_UNKNOWN, DefaultCalibrations::GENERIC_DEFAULT}
};

const size_t CalibrationManager::defaultCalibrationCount = 
    sizeof(CalibrationManager::defaultCalibrations) / sizeof(DefaultCalibrationSet);

CalibrationManager::CalibrationManager() : currentStatus(CAL_STATUS_INVALID), 
                                          isInitialized(false), hasBackupCopy(false) {
    memset(&activeCalibration, 0, sizeof(activeCalibration));
}

bool CalibrationManager::initialize() {
    if (isInitialized) {
        return true;
    }
    
    Serial.println(F("[INFO][CAL]: Initializing calibration manager"));
    
    if (!initializeEEPROM()) {
        Serial.println(F("[ERROR][CAL]: EEPROM initialization failed"));
        return false;
    }
    
    // Try to load existing calibration
    TouchCalibration cal;
    if (readFromEEPROM(cal, CAL_DATA_ADDR)) {
        if (validateCalibrationData(cal)) {
            activeCalibration = cal;
            currentStatus = CAL_STATUS_USER;
            Serial.println(F("[INFO][CAL]: User calibration loaded"));
        } else {
            Serial.println(F("[WARNING][CAL]: User calibration corrupted, checking backup"));
            currentStatus = CAL_STATUS_CORRUPTED;
            
            // Try backup
            if (restoreFromBackup(cal)) {
                activeCalibration = cal;
                currentStatus = CAL_STATUS_USER;
                Serial.println(F("[INFO][CAL]: Backup calibration restored"));
            } else {
                Serial.println(F("[WARNING][CAL]: No valid backup, using defaults"));
                // Will load defaults later
            }
        }
    }
    
    // If no valid user calibration, load defaults
    if (currentStatus == CAL_STATUS_INVALID || currentStatus == CAL_STATUS_CORRUPTED) {
        activeCalibration = DefaultCalibrations::GENERIC_DEFAULT;
        currentStatus = CAL_STATUS_DEFAULT;
        Serial.println(F("[INFO][CAL]: Default calibration loaded"));
    }
    
    // Check for backup copy
    TouchCalibration backupCal;
    hasBackupCopy = readFromEEPROM(backupCal, CAL_BACKUP_ADDR) && 
                   validateCalibrationData(backupCal);
    
    isInitialized = true;
    
    // Calculate checksum for active calibration if needed
    if (activeCalibration.checksum == 0) {
        activeCalibration.checksum = calculateChecksum(activeCalibration);
    }
    
    Serial.print(F("[INFO][CAL]: Initialization complete, status: "));
    Serial.println(CalibrationHelper::statusToString(currentStatus));
    
    return true;
}

bool CalibrationManager::initializeEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Test EEPROM access
    uint8_t testValue = 0xAA;
    EEPROM.write(EEPROM_SIZE - 1, testValue);
    EEPROM.commit();
    
    if (EEPROM.read(EEPROM_SIZE - 1) != testValue) {
        Serial.println(F("[ERROR][CAL]: EEPROM write test failed"));
        return false;
    }
    
    return true;
}

uint16_t CalibrationManager::calculateChecksum(const TouchCalibration& cal) const {
    uint16_t checksum = CAL_SIGNATURE;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&cal);
    
    // Calculate checksum excluding the checksum field itself
    for (size_t i = 0; i < sizeof(TouchCalibration) - sizeof(cal.checksum); i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 15); // Rotate left
    }
    
    return checksum;
}

bool CalibrationManager::validateCalibrationData(const TouchCalibration& cal) const {
    // Check version
    if (!isVersionSupported(cal.version)) {
        Serial.print(F("[ERROR][CAL]: Unsupported version: "));
        Serial.println(cal.version);
        return false;
    }
    
    // Check checksum
    if (!isChecksumValid(cal)) {
        Serial.println(F("[ERROR][CAL]: Checksum validation failed"));
        return false;
    }
    
    // Check coordinate ranges
    if (!areRangesValid(cal)) {
        Serial.println(F("[ERROR][CAL]: Invalid coordinate ranges"));
        return false;
    }
    
    // Check pressure values
    if (cal.minPressure > cal.maxPressure ||
        cal.minPressure < CalibrationConfig::MIN_PRESSURE ||
        cal.maxPressure > CalibrationConfig::MAX_PRESSURE) {
        Serial.println(F("[ERROR][CAL]: Invalid pressure ranges"));
        return false;
    }
    
    // Check debounce time
    if (cal.debounceMs > CalibrationConfig::MAX_DEBOUNCE_MS) {
        Serial.println(F("[ERROR][CAL]: Invalid debounce time"));
        return false;
    }
    
    return true;
}

bool CalibrationManager::isChecksumValid(const TouchCalibration& cal) const {
    TouchCalibration temp = cal;
    temp.checksum = 0; // Zero out checksum for calculation
    uint16_t calculatedChecksum = calculateChecksum(temp);
    return calculatedChecksum == cal.checksum;
}

bool CalibrationManager::areRangesValid(const TouchCalibration& cal) const {
    return (cal.xMax > cal.xMin + CalibrationConfig::MIN_COORDINATE_RANGE) &&
           (cal.yMax > cal.yMin + CalibrationConfig::MIN_COORDINATE_RANGE) &&
           (cal.xMax <= CalibrationConfig::MAX_COORDINATE_VALUE) &&
           (cal.yMax <= CalibrationConfig::MAX_COORDINATE_VALUE);
}

bool CalibrationManager::isVersionSupported(uint8_t version) const {
    return version <= CAL_VERSION;
}

bool CalibrationManager::readFromEEPROM(TouchCalibration& cal, uint16_t address) const {
    if (address + sizeof(TouchCalibration) > EEPROM_SIZE) {
        return false;
    }
    
    uint8_t* data = reinterpret_cast<uint8_t*>(&cal);
    for (size_t i = 0; i < sizeof(TouchCalibration); i++) {
        data[i] = EEPROM.read(address + i);
    }
    
    return true;
}

bool CalibrationManager::writeToEEPROM(const TouchCalibration& cal, uint16_t address) {
    if (address + sizeof(TouchCalibration) > EEPROM_SIZE) {
        return false;
    }
    
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&cal);
    for (size_t i = 0; i < sizeof(TouchCalibration); i++) {
        EEPROM.write(address + i, data[i]);
    }
    
    bool success = EEPROM.commit();
    if (success) {
        delay(CalibrationConfig::EEPROM_WRITE_DELAY_MS);
    }
    
    return success;
}

bool CalibrationManager::loadCalibration(TouchCalibration& calibration) {
    if (!isInitialized) {
        if (!initialize()) {
            return false;
        }
    }
    
    calibration = activeCalibration;
    return currentStatus != CAL_STATUS_INVALID;
}

bool CalibrationManager::saveCalibration(const TouchCalibration& calibration) {
    if (!isInitialized) {
        if (!initialize()) {
            return false;
        }
    }
    
    // Validate calibration data
    TouchCalibration cal = calibration;
    cal.calibrationDate = millis();
    cal.version = CAL_VERSION;
    cal.checksum = calculateChecksum(cal);
    
    if (!validateCalibrationData(cal)) {
        Serial.println(F("[ERROR][CAL]: Cannot save invalid calibration data"));
        return false;
    }
    
    // Create backup of current calibration
    if (currentStatus != CAL_STATUS_INVALID && currentStatus != CAL_STATUS_CORRUPTED) {
        createBackup(activeCalibration);
    }
    
    // Write new calibration
    if (!writeToEEPROM(cal, CAL_DATA_ADDR)) {
        Serial.println(F("[ERROR][CAL]: Failed to write calibration to EEPROM"));
        return false;
    }
    
    // Verify write
    TouchCalibration verifyCal;
    if (!readFromEEPROM(verifyCal, CAL_DATA_ADDR) || 
        !compareCalibrations(cal, verifyCal)) {
        Serial.println(F("[ERROR][CAL]: Calibration write verification failed"));
        return false;
    }
    
    // Update active calibration
    activeCalibration = cal;
    currentStatus = CAL_STATUS_USER;
    
    Serial.println(F("[INFO][CAL]: Calibration saved successfully"));
    return true;
}

bool CalibrationManager::compareCalibrations(const TouchCalibration& cal1, 
                                            const TouchCalibration& cal2) const {
    return memcmp(&cal1, &cal2, sizeof(TouchCalibration)) == 0;
}

void CalibrationManager::createBackup(const TouchCalibration& cal) {
    if (writeToEEPROM(cal, CAL_BACKUP_ADDR)) {
        hasBackupCopy = true;
        Serial.println(F("[INFO][CAL]: Backup calibration created"));
    } else {
        Serial.println(F("[WARNING][CAL]: Failed to create backup calibration"));
    }
}

bool CalibrationManager::restoreFromBackup(TouchCalibration& cal) {
    if (!hasBackupCopy) {
        return false;
    }
    
    if (readFromEEPROM(cal, CAL_BACKUP_ADDR) && validateCalibrationData(cal)) {
        Serial.println(F("[INFO][CAL]: Calibration restored from backup"));
        return true;
    }
    
    hasBackupCopy = false;
    return false;
}

bool CalibrationManager::isCalibrationValid(const TouchCalibration& calibration) {
    return validateCalibrationData(calibration);
}

TouchCalibration CalibrationManager::getDefaultCalibration(HardwareConfig::ModelType model) {
    const DefaultCalibrationSet* defaultSet = findDefaultSet(model);
    
    if (defaultSet) {
        TouchCalibration cal = defaultSet->calibration;
        cal.calibrationDate = millis();
        cal.checksum = calculateChecksum(cal);
        return cal;
    }
    
    // Fallback to generic default
    TouchCalibration cal = DefaultCalibrations::GENERIC_DEFAULT;
    cal.calibrationDate = millis();
    cal.checksum = calculateChecksum(cal);
    return cal;
}

const DefaultCalibrationSet* CalibrationManager::findDefaultSet(HardwareConfig::ModelType model) const {
    for (size_t i = 0; i < defaultCalibrationCount; i++) {
        if (defaultCalibrations[i].modelType == model) {
            return &defaultCalibrations[i];
        }
    }
    return nullptr;
}

bool CalibrationManager::resetToDefaults() {
    if (!isInitialized) {
        if (!initialize()) {
            return false;
        }
    }
    
    // Load generic defaults
    TouchCalibration defaultCal = DefaultCalibrations::GENERIC_DEFAULT;
    defaultCal.calibrationDate = millis();
    defaultCal.checksum = calculateChecksum(defaultCal);
    
    // Save to EEPROM
    if (saveCalibration(defaultCal)) {
        currentStatus = CAL_STATUS_DEFAULT;
        Serial.println(F("[INFO][CAL]: Reset to default calibration"));
        return true;
    }
    
    return false;
}

TouchCalibration CalibrationManager::createCalibrationFromTouchConfig(const TouchConfig& touchConfig) const {
    TouchCalibration cal = DefaultCalibrations::GENERIC_DEFAULT;
    
    // Copy calibration values
    cal.xMin = touchConfig.calibration.xMin;
    cal.xMax = touchConfig.calibration.xMax;
    cal.yMin = touchConfig.calibration.yMin;
    cal.yMax = touchConfig.calibration.yMax;
    cal.swapXY = touchConfig.calibration.swapXY;
    cal.invertX = touchConfig.calibration.invertX;
    cal.invertY = touchConfig.calibration.invertY;
    cal.rotation = touchConfig.calibration.rotation;
    
    // Copy configuration values
    cal.minPressure = touchConfig.minPressure;
    cal.debounceMs = touchConfig.debounceMs;
    
    // Set metadata
    cal.calibrationDate = millis();
    cal.version = CAL_VERSION;
    cal.checksum = calculateChecksum(cal);
    
    return cal;
}

TouchConfig CalibrationManager::createTouchConfigFromCalibration(const TouchCalibration& calibration) const {
    TouchConfig touchConfig;
    
    // Copy calibration values
    touchConfig.calibration.xMin = calibration.xMin;
    touchConfig.calibration.xMax = calibration.xMax;
    touchConfig.calibration.yMin = calibration.yMin;
    touchConfig.calibration.yMax = calibration.yMax;
    touchConfig.calibration.swapXY = calibration.swapXY;
    touchConfig.calibration.invertX = calibration.invertX;
    touchConfig.calibration.invertY = calibration.invertY;
    touchConfig.calibration.rotation = calibration.rotation;
    
    // Copy configuration values
    touchConfig.minPressure = calibration.minPressure;
    touchConfig.debounceMs = calibration.debounceMs;
    
    // Set default pin configuration
    touchConfig.pins.cs = 33;
    touchConfig.pins.irq = 36;
    touchConfig.pins.mosi = 32;
    touchConfig.pins.miso = 39;
    touchConfig.pins.clk = 25;
    
    // Initialize runtime state
    touchConfig.touched = false;
    touchConfig.lastX = 0;
    touchConfig.lastY = 0;
    touchConfig.lastZ = 0;
    touchConfig.lastTouchTime = 0;
    
    return touchConfig;
}

void CalibrationManager::printCalibrationInfo() const {
    Serial.println(F("=== Touch Calibration Info ==="));
    
    Serial.print(F("Status: "));
    Serial.println(CalibrationHelper::statusToString(currentStatus));
    
    Serial.print(F("Model ID: "));
    Serial.println(CalibrationHelper::modelToString(activeCalibration.modelID));
    
    Serial.print(F("Version: "));
    Serial.println(activeCalibration.version);
    
    Serial.print(F("X Range: "));
    Serial.print(activeCalibration.xMin);
    Serial.print(F(" - "));
    Serial.println(activeCalibration.xMax);
    
    Serial.print(F("Y Range: "));
    Serial.print(activeCalibration.yMin);
    Serial.print(F(" - "));
    Serial.println(activeCalibration.yMax);
    
    Serial.print(F("Pressure: "));
    Serial.print(activeCalibration.minPressure);
    Serial.print(F(" - "));
    Serial.println(activeCalibration.maxPressure);
    
    Serial.print(F("Rotation: "));
    Serial.println(activeCalibration.rotation);
    
    Serial.print(F("Flags: SwapXY="));
    Serial.print(activeCalibration.swapXY ? F("Yes") : F("No"));
    Serial.print(F(", InvertX="));
    Serial.print(activeCalibration.invertX ? F("Yes") : F("No"));
    Serial.print(F(", InvertY="));
    Serial.println(activeCalibration.invertY ? F("Yes") : F("No"));
    
    Serial.print(F("Calibration Date: "));
    Serial.println(activeCalibration.calibrationDate);
    
    Serial.print(F("Has Backup: "));
    Serial.println(hasBackupCopy ? F("Yes") : F("No"));
    
    Serial.println(F("=============================="));
}

// Helper function implementations
namespace CalibrationHelper {
    uint16_t mapTouchX(uint16_t rawX, const TouchCalibration& cal, uint16_t displayWidth) {
        if (rawX < cal.xMin) rawX = cal.xMin;
        if (rawX > cal.xMax) rawX = cal.xMax;
        
        uint16_t mappedX = map(rawX, cal.xMin, cal.xMax, 0, displayWidth - 1);
        
        if (cal.invertX) {
            mappedX = displayWidth - 1 - mappedX;
        }
        
        return mappedX;
    }
    
    uint16_t mapTouchY(uint16_t rawY, const TouchCalibration& cal, uint16_t displayHeight) {
        if (rawY < cal.yMin) rawY = cal.yMin;
        if (rawY > cal.yMax) rawY = cal.yMax;
        
        uint16_t mappedY = map(rawY, cal.yMin, cal.yMax, 0, displayHeight - 1);
        
        if (cal.invertY) {
            mappedY = displayHeight - 1 - mappedY;
        }
        
        return mappedY;
    }
    
    bool isPointInBounds(uint16_t x, uint16_t y, const TouchCalibration& cal) {
        return (x >= cal.xMin && x <= cal.xMax) &&
               (y >= cal.yMin && y <= cal.yMax);
    }
    
    bool isPressureValid(uint16_t pressure, const TouchCalibration& cal) {
        return pressure >= cal.minPressure && pressure <= cal.maxPressure;
    }
    
    const char* statusToString(CalibrationStatus status) {
        switch (status) {
            case CAL_STATUS_INVALID: return "Invalid";
            case CAL_STATUS_DEFAULT: return "Default";
            case CAL_STATUS_FACTORY: return "Factory";
            case CAL_STATUS_USER: return "User";
            case CAL_STATUS_CORRUPTED: return "Corrupted";
            default: return "Unknown";
        }
    }
    
    const char* modelToString(uint8_t modelID) {
        switch (modelID) {
            case static_cast<uint8_t>(HardwareConfig::MODEL_CYD_SINGLE_USB):
                return "CYD Single USB";
            case static_cast<uint8_t>(HardwareConfig::MODEL_CYD_DUAL_USB):
                return "CYD Dual USB";
            default:
                return "Generic";
        }
    }
    
    void printCalibrationDetails(const TouchCalibration& cal) {
        Serial.println(F("--- Calibration Details ---"));
        Serial.print(F("xMin="));
        Serial.print(cal.xMin);
        Serial.print(F(", xMax="));
        Serial.print(cal.xMax);
        Serial.print(F(", yMin="));
        Serial.print(cal.yMin);
        Serial.print(F(", yMax="));
        Serial.println(cal.yMax);
        
        Serial.print(F("swapXY="));
        Serial.print(cal.swapXY);
        Serial.print(F(", invertX="));
        Serial.print(cal.invertX);
        Serial.print(F(", invertY="));
        Serial.print(cal.invertY);
        Serial.print(F(", rotation="));
        Serial.println(cal.rotation);
        
        Serial.print(F("minPressure="));
        Serial.print(cal.minPressure);
        Serial.print(F(", maxPressure="));
        Serial.print(cal.maxPressure);
        Serial.print(F(", debounce="));
        Serial.print(cal.debounceMs);
        Serial.println(F("ms"));
        
        Serial.print(F("modelID="));
        Serial.print(cal.modelID);
        Serial.print(F(", version="));
        Serial.print(cal.version);
        Serial.print(F(", checksum=0x"));
        Serial.println(cal.checksum, HEX);
        Serial.println(F("--------------------------"));
    }
}