/**
 * CalibrationManager.h - Touch calibration EEPROM management
 * 
 * Manages persistent storage and retrieval of touch calibration data
 * with model-specific profiles and validation.
 */

#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "TouchConfig.h"
#include "HardwareConfig.h"

/**
 * Calibration Data Structure (32 bytes aligned)
 */
struct TouchCalibration {
    // Calibration values
    uint16_t xMin, xMax;         // Raw X coordinate range
    uint16_t yMin, yMax;         // Raw Y coordinate range
    bool swapXY;                 // Swap X/Y axes
    bool invertX, invertY;       // Invert axes
    uint8_t rotation;            // Display rotation (0-3)
    
    // Metadata
    uint8_t modelID;             // Hardware model identifier
    uint8_t hardwareRevision;    // Hardware revision
    uint32_t calibrationDate;    // When calibrated (timestamp)
    uint32_t calibrationFlags;   // Feature flags
    
    // Validation
    uint16_t checksum;           // Data integrity check
    uint8_t version;             // Calibration data version
    uint8_t reserved[3];         // Reserved for future use
    
    // Touch configuration
    uint16_t minPressure;        // Minimum pressure threshold
    uint16_t maxPressure;        // Maximum pressure threshold
    uint16_t debounceMs;         // Debounce time
    uint8_t sampleCount;         // Number of samples for averaging
};

/**
 * Calibration Status
 */
enum CalibrationStatus {
    CAL_STATUS_INVALID = 0,      // No valid calibration
    CAL_STATUS_DEFAULT = 1,      // Using default values
    CAL_STATUS_FACTORY = 2,      // Factory calibration loaded
    CAL_STATUS_USER = 3,         // User calibration loaded
    CAL_STATUS_CORRUPTED = 4     // Data corruption detected
};

/**
 * Default Calibration Values for Known Models
 */
struct DefaultCalibrationSet {
    HardwareConfig::ModelType modelType;
    TouchCalibration calibration;
};

/**
 * Calibration Manager Interface
 */
class ICalibrationManager {
public:
    virtual ~ICalibrationManager() = default;
    
    virtual bool loadCalibration(TouchCalibration& calibration) = 0;
    virtual bool saveCalibration(const TouchCalibration& calibration) = 0;
    virtual bool isCalibrationValid(const TouchCalibration& calibration) = 0;
    virtual TouchCalibration getDefaultCalibration(HardwareConfig::ModelType model) = 0;
    virtual bool resetToDefaults() = 0;
    virtual CalibrationStatus getStatus() = 0;
};

/**
 * EEPROM-based Calibration Manager
 */
class CalibrationManager : public ICalibrationManager {
private:
    static const uint16_t EEPROM_SIZE = 512;
    static const uint16_t CAL_DATA_ADDR = 0x00;
    static const uint16_t CAL_BACKUP_ADDR = 0x40; // Backup copy at offset 64
    static const uint16_t CAL_SIGNATURE = 0xCAFE;
    static const uint8_t CAL_VERSION = 1;
    
    CalibrationStatus currentStatus;
    TouchCalibration activeCalibration;
    bool isInitialized;
    bool hasBackupCopy;
    
    // Default calibration sets for different models
    static const DefaultCalibrationSet defaultCalibrations[];
    static const size_t defaultCalibrationCount;
    
    // Internal methods
    bool initializeEEPROM();
    uint16_t calculateChecksum(const TouchCalibration& cal) const;
    bool validateCalibrationData(const TouchCalibration& cal) const;
    bool readFromEEPROM(TouchCalibration& cal, uint16_t address) const;
    bool writeToEEPROM(const TouchCalibration& cal, uint16_t address);
    void createBackup(const TouchCalibration& cal);
    bool restoreFromBackup(TouchCalibration& cal);
    const DefaultCalibrationSet* findDefaultSet(HardwareConfig::ModelType model) const;
    
    // Validation helpers
    bool isChecksumValid(const TouchCalibration& cal) const;
    bool areRangesValid(const TouchCalibration& cal) const;
    bool isModelSupported(uint8_t modelID) const;
    bool isVersionSupported(uint8_t version) const;
    
    // Migration helpers
    bool migrateFromOldVersion(TouchCalibration& cal);
    void upgradeCalibrationData(TouchCalibration& cal);
    
public:
    CalibrationManager();
    ~CalibrationManager() = default;
    
    // Initialization
    bool initialize();
    bool isReady() const { return isInitialized; }
    
    // ICalibrationManager interface
    bool loadCalibration(TouchCalibration& calibration) override;
    bool saveCalibration(const TouchCalibration& calibration) override;
    bool isCalibrationValid(const TouchCalibration& calibration) override;
    TouchCalibration getDefaultCalibration(HardwareConfig::ModelType model) override;
    bool resetToDefaults() override;
    CalibrationStatus getStatus() override { return currentStatus; }
    
    // Extended functionality
    bool hasValidCalibration() const;
    TouchCalibration getCurrentCalibration() const { return activeCalibration; }
    
    // Backup and recovery
    bool createBackupCalibration();
    bool restoreBackupCalibration();
    bool hasBackup() const { return hasBackupCopy; }
    
    // Calibration utilities
    TouchCalibration createCalibrationFromTouchConfig(const TouchConfig& touchConfig) const;
    TouchConfig createTouchConfigFromCalibration(const TouchCalibration& calibration) const;
    
    // Statistics and diagnostics
    void printCalibrationInfo() const;
    bool testEEPROMIntegrity();
    void dumpEEPROMData() const;
    uint32_t getLastCalibrationDate() const { return activeCalibration.calibrationDate; }
    
    // Factory reset and maintenance
    bool performFactoryReset();
    bool clearAllCalibrationData();
    void defragmentEEPROM();
    
    // Calibration comparison and merging
    bool compareCalibrations(const TouchCalibration& cal1, const TouchCalibration& cal2) const;
    TouchCalibration mergeCalibrations(const TouchCalibration& user, const TouchCalibration& factory) const;
    
    // Advanced features
    bool exportCalibration(char* jsonBuffer, size_t bufferSize) const;
    bool importCalibration(const char* jsonData);
    bool autoDetectModel(TouchCalibration& calibration);
};

/**
 * Calibration Helper Functions
 */
namespace CalibrationHelper {
    // Coordinate transformation
    uint16_t mapTouchX(uint16_t rawX, const TouchCalibration& cal, uint16_t displayWidth);
    uint16_t mapTouchY(uint16_t rawY, const TouchCalibration& cal, uint16_t displayHeight);
    
    // Validation
    bool isPointInBounds(uint16_t x, uint16_t y, const TouchCalibration& cal);
    bool isPressureValid(uint16_t pressure, const TouchCalibration& cal);
    
    // Calibration quality assessment
    float calculateCalibrationAccuracy(const TouchCalibration& cal);
    bool isCalibrationHighQuality(const TouchCalibration& cal);
    
    // String conversion
    const char* statusToString(CalibrationStatus status);
    const char* modelToString(uint8_t modelID);
    
    // Debugging
    void printCalibrationDetails(const TouchCalibration& cal);
    void validateCalibrationRanges(const TouchCalibration& cal);
}

/**
 * Calibration Configuration Constants
 */
namespace CalibrationConfig {
    // Validation thresholds
    static const uint16_t MIN_COORDINATE_RANGE = 100;    // Minimum X/Y range
    static const uint16_t MAX_COORDINATE_VALUE = 4095;   // Maximum raw coordinate
    static const uint16_t MIN_PRESSURE = 10;             // Minimum pressure threshold
    static const uint16_t MAX_PRESSURE = 2000;           // Maximum pressure threshold
    static const uint16_t MAX_DEBOUNCE_MS = 500;         // Maximum debounce time
    static const uint8_t MAX_SAMPLE_COUNT = 16;          // Maximum sample averaging
    
    // Quality thresholds
    static const float MIN_ACCURACY_PERCENT = 85.0f;    // Minimum calibration accuracy
    static const uint16_t MIN_USABLE_RANGE = 200;       // Minimum usable touch range
    
    // EEPROM layout
    static const uint16_t SIGNATURE_OFFSET = 0;
    static const uint16_t VERSION_OFFSET = 2;
    static const uint16_t DATA_OFFSET = 4;
    static const uint16_t CHECKSUM_OFFSET = 30;
    
    // Timing
    static const uint32_t EEPROM_WRITE_DELAY_MS = 5;    // Delay between EEPROM writes
    static const uint32_t BACKUP_INTERVAL_MS = 3600000; // Backup every hour
}

/**
 * Model-Specific Default Calibrations
 */
namespace DefaultCalibrations {
    extern const TouchCalibration CYD_SINGLE_USB_DEFAULT;
    extern const TouchCalibration CYD_DUAL_USB_DEFAULT;
    extern const TouchCalibration GENERIC_DEFAULT;
}

#endif // CALIBRATION_MANAGER_H