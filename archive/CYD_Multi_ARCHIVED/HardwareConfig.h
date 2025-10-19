#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>

/**
 * HardwareConfig - Represents detected CYD hardware variant and capabilities
 * 
 * Validation Rules:
 * - model must be valid enum value
 * - displayDriverID must match known drivers
 * - backlightPin must be 21 or 27
 * - At least one feature flag must be true
 * 
 * State Transitions:
 * - UNKNOWN → SINGLE_USB or DUAL_USB (on detection)
 * - No reverse transitions allowed
 */
struct HardwareConfig {
  // Model identification
  enum ModelType {
    MODEL_UNKNOWN = 0,
    MODEL_CYD_SINGLE_USB = 1,  // ILI9341, micro USB only
    MODEL_CYD_DUAL_USB = 2     // ST7789, micro + Type-C
  };
  ModelType model;
  
  // Display configuration
  uint16_t displayDriverID;      // 0x9341 or 0x8552
  uint8_t backlightPin;          // GPIO21 or GPIO27
  bool backlightActiveHigh;      // true for most CYDs
  
  // Feature flags
  bool hasSDCard;
  bool hasRFID;
  bool hasTouch;
  bool hasAudio;
  
  // Version info
  uint8_t hardwareRevision;      // For future variants
  uint32_t detectedAt;           // Timestamp of detection
};

// Default configurations
const HardwareConfig DEFAULT_SINGLE_USB = {
  .model = HardwareConfig::MODEL_CYD_SINGLE_USB,
  .displayDriverID = 0x9341,
  .backlightPin = 21,
  .backlightActiveHigh = true,
  .hasSDCard = true,
  .hasRFID = true,
  .hasTouch = true,
  .hasAudio = true,
  .hardwareRevision = 1,
  .detectedAt = 0
};

const HardwareConfig DEFAULT_DUAL_USB = {
  .model = HardwareConfig::MODEL_CYD_DUAL_USB,
  .displayDriverID = 0x8552,
  .backlightPin = 27,  // or 21 for some units
  .backlightActiveHigh = true,
  .hasSDCard = true,
  .hasRFID = true,
  .hasTouch = true,
  .hasAudio = true,
  .hardwareRevision = 1,
  .detectedAt = 0
};

#endif // HARDWARE_CONFIG_H