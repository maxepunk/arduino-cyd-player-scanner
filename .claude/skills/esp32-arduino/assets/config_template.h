/*
 * Configuration Header
 * 
 * Centralized configuration for ESP32 project.
 * Modify pin assignments, constants, and feature flags here.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PROJECT INFORMATION
// ============================================================================
#define PROJECT_NAME    "ESP32 Project"
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

// --- LED Pins ---
#define LED_BUILTIN         2     // Onboard LED (most ESP32 boards)
#define LED_RED             4     // Example RGB LED pins (CYD board)
#define LED_GREEN           16
#define LED_BLUE            17

// --- Button Pins ---
#define BUTTON_BOOT         0     // Boot button (GPIO 0)
#define BUTTON_USER         32    // Example user button

// --- Sensor Pins ---
#define SENSOR_PIN          33    // ADC1 pin (safe with WiFi)
#define LDR_PIN             34    // Light sensor (input only)

// --- Communication Pins ---

// I2C (default ESP32 pins)
#define I2C_SDA             21
#define I2C_SCL             22

// SPI (VSPI - recommended for external devices)
#define SPI_MISO            19
#define SPI_MOSI            23
#define SPI_CLK             18
#define SPI_CS              5

// UART2 (for external serial devices)
#define UART2_RX            16
#define UART2_TX            17

// ============================================================================
// TIMING CONSTANTS
// ============================================================================
#define LOOP_DELAY_MS       1000   // Main loop delay
#define DEBOUNCE_DELAY_MS   50     // Button debounce time
#define SENSOR_READ_INTERVAL 5000  // Sensor reading interval

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================
#define WIFI_ENABLED        1      // Set to 0 to disable WiFi

#if WIFI_ENABLED
  #define WIFI_SSID         "YourSSID"
  #define WIFI_PASSWORD     "YourPassword"
  #define WIFI_TIMEOUT_MS   10000  // Connection timeout
  #define WIFI_RETRY_DELAY  500    // Delay between retries
#endif

// ============================================================================
// HARDWARE FEATURES
// ============================================================================
// Enable/disable hardware features (1 = enabled, 0 = disabled)
#define FEATURE_DISPLAY     0      // TFT display
#define FEATURE_SD_CARD     0      // SD card
#define FEATURE_BLUETOOTH   0      // Bluetooth
#define FEATURE_WEBSERVER   0      // Web server

// ============================================================================
// DISPLAY CONFIGURATION (if enabled)
// ============================================================================
#if FEATURE_DISPLAY
  #define TFT_WIDTH         240
  #define TFT_HEIGHT        320
  #define TFT_ROTATION      1      // 0-3 for different orientations
#endif

// ============================================================================
// MEMORY SETTINGS
// ============================================================================
#define HEAP_WARNING_THRESHOLD  20000  // Warn if free heap drops below this

// ============================================================================
// APPLICATION-SPECIFIC SETTINGS
// ============================================================================
// Add your custom configuration constants here
#define THRESHOLD_VALUE     512
#define MAX_SAMPLES         100
#define SAMPLE_RATE_HZ      10

// ============================================================================
// ADVANCED SETTINGS
// ============================================================================
// CPU Frequency (set in Arduino IDE or via FQBN)
// Options: 240, 160, 80, 40 MHz
// #define CPU_FREQ_MHZ     240

// Watchdog timer
#define WATCHDOG_TIMEOUT_SECONDS  10

// ============================================================================
// SANITY CHECKS
// ============================================================================
// Compile-time checks for invalid configurations

#if WIFI_ENABLED && (SENSOR_PIN >= 0 && SENSOR_PIN <= 15 && SENSOR_PIN != 11)
  // Check if sensor is on ADC2 (conflicts with WiFi)
  #if (SENSOR_PIN == 0 || SENSOR_PIN == 2 || SENSOR_PIN == 4 || \
       SENSOR_PIN == 12 || SENSOR_PIN == 13 || SENSOR_PIN == 14 || \
       SENSOR_PIN == 15 || SENSOR_PIN == 25 || SENSOR_PIN == 26 || \
       SENSOR_PIN == 27)
    #warning "SENSOR_PIN is on ADC2 and will not work with WiFi enabled! Use ADC1 (GPIO 32-39)"
  #endif
#endif

#endif // CONFIG_H
