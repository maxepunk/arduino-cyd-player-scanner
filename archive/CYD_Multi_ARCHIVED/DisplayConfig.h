#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <Arduino.h>

/**
 * DisplayConfig - Display driver settings and state
 * 
 * Validation Rules:
 * - width * height must equal 76800 (240*320)
 * - rotation must be 0-3
 * - spiFrequency must be between 10MHz and 80MHz
 * - Must be initialized before use
 */
struct DisplayConfig {
  // Driver settings
  enum DriverType {
    DRIVER_ILI9341,
    DRIVER_ST7789,
    DRIVER_UNKNOWN
  };
  DriverType driver;
  
  // Display parameters
  uint16_t width;                // 240 or 320 depending on rotation
  uint16_t height;               // 320 or 240 depending on rotation
  uint8_t rotation;              // 0-3 for orientation
  bool colorInverted;            // Some ST7789 need inversion
  bool useBGR;                   // Color order (RGB vs BGR)
  
  // SPI configuration
  uint32_t spiFrequency;         // Display SPI speed
  uint8_t spiMode;               // SPI mode (usually 0)
  
  // Status
  bool initialized;
  uint32_t lastError;
  uint32_t framesRendered;
};

#endif // DISPLAY_CONFIG_H