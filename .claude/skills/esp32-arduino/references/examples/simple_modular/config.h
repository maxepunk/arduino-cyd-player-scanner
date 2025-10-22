/**
 * Configuration for Simple Modular Example
 */

#ifndef CONFIG_H
#define CONFIG_H

// Serial
#define SERIAL_BAUD         115200

// Pin Definitions
#define LED_PIN             2       // Onboard LED
#define BUTTON_PIN          0       // Boot button
#define SENSOR_PIN          34      // ADC1 pin (GPIO 34)

// Timing Constants
#define DEBOUNCE_TIME_MS    50      // Button debounce
#define SENSOR_INTERVAL_MS  1000    // Sensor reading interval

// Sensor Configuration
#define SENSOR_SAMPLES      10      // Number of samples for averaging
#define ADC_MAX_VALUE       4095    // 12-bit ADC
#define ADC_VOLTAGE_REF     3.3     // Reference voltage

// Debug Mode
#define DEBUG_MODE          1

#if DEBUG_MODE
    #define DEBUG_PRINT(x)   Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#endif

#endif // CONFIG_H
