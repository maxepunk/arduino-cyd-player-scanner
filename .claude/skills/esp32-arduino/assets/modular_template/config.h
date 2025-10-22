/**
 * Configuration Header for Modular Project
 * 
 * Centralized configuration for all modules.
 * All constants, pin definitions, and settings go here.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PROJECT INFORMATION
// ============================================================================
#define PROJECT_NAME    "Modular ESP32 Project"
#define VERSION         "1.0.0"
#define AUTHOR          "Your Name"

// ============================================================================
// DEBUG SETTINGS
// ============================================================================
// Set to 1 to enable debug output, 0 to disable
#define DEBUG_MODE      1

#if DEBUG_MODE
    #define DEBUG_PRINT(x)    Serial.print(x)
    #define DEBUG_PRINTLN(x)  Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

// ============================================================================
// SERIAL CONFIGURATION
// ============================================================================
#define SERIAL_BAUD_RATE    115200

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
// LED Pins
#define LED_PIN             2       // Onboard LED

// Button Pins
#define BUTTON_PIN          0       // Boot button

// Sensor Pins (use ADC1 if WiFi enabled)
#define SENSOR_PIN          34      // ADC1 pin

// I2C Pins (default ESP32)
#define I2C_SDA             21
#define I2C_SCL             22

// SPI Pins (VSPI - recommended for external devices)
#define SPI_MISO            19
#define SPI_MOSI            23
#define SPI_CLK             18
#define SPI_CS              5

// ============================================================================
// TIMING CONSTANTS
// ============================================================================
#define LOOP_DELAY_MS       1000    // Main loop delay
#define DEBOUNCE_DELAY_MS   50      // Button debounce time
#define SENSOR_INTERVAL_MS  5000    // Sensor reading interval
#define UPDATE_INTERVAL_MS  1000    // Display update interval

// ============================================================================
// FEATURE FLAGS
// ============================================================================
// Enable/disable features (1 = enabled, 0 = disabled)
#define FEATURE_WIFI        0       // WiFi connectivity
#define FEATURE_DISPLAY     0       // TFT display
#define FEATURE_SD_CARD     0       // SD card logging
#define FEATURE_BLUETOOTH   0       // Bluetooth

// ============================================================================
// WIFI CONFIGURATION (if enabled)
// ============================================================================
#if FEATURE_WIFI
    #define WIFI_SSID           "YourSSID"
    #define WIFI_PASSWORD       "YourPassword"
    #define WIFI_TIMEOUT_MS     10000
    #define WIFI_RETRY_DELAY    500
#endif

// ============================================================================
// DISPLAY CONFIGURATION (if enabled)
// ============================================================================
#if FEATURE_DISPLAY
    #define TFT_WIDTH           240
    #define TFT_HEIGHT          320
    #define TFT_ROTATION        1       // 0-3 for orientations
#endif

// ============================================================================
// MEMORY SETTINGS
// ============================================================================
#define HEAP_WARNING_THRESHOLD  20000   // Warn if heap drops below

// ============================================================================
// APPLICATION-SPECIFIC SETTINGS
// ============================================================================
// Add your custom configuration here
#define THRESHOLD_VALUE     512
#define MAX_SAMPLES         100

#endif // CONFIG_H
