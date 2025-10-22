/**
 * Simple Modular ESP32 Example
 * 
 * Demonstrates proper multi-file organization with 3 modules:
 * - LED Controller: Manages LED blinking patterns
 * - Button Handler: Debounced button input
 * - Sensor Reader: Reads analog sensor with filtering
 * 
 * This project shows how to organize ~300 lines of code
 * across multiple files for better maintainability.
 */

#include "config.h"
#include "led_controller.h"
#include "button_handler.h"
#include "sensor_reader.h"

// Application state
enum AppMode {
    MODE_SLOW_BLINK,
    MODE_FAST_BLINK,
    MODE_SENSOR_DISPLAY
};

AppMode currentMode = MODE_SLOW_BLINK;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  Simple Modular ESP32 Example");
    Serial.println("========================================\n");
    
    // Initialize all modules
    Serial.println("Initializing modules...");
    
    if (!LEDController.begin()) {
        Serial.println("ERROR: LED init failed!");
    }
    
    if (!ButtonHandler.begin()) {
        Serial.println("ERROR: Button init failed!");
    }
    
    if (!SensorReader.begin()) {
        Serial.println("ERROR: Sensor init failed!");
    }
    
    Serial.println("All modules initialized\n");
    Serial.println("Press button to cycle modes:");
    Serial.println("  1. Slow blink");
    Serial.println("  2. Fast blink");
    Serial.println("  3. Sensor display\n");
}

void loop() {
    // Update all modules
    LEDController.update();
    ButtonHandler.update();
    SensorReader.update();
    
    // Check for button press
    if (ButtonHandler.wasPressed()) {
        // Cycle through modes
        currentMode = (AppMode)((currentMode + 1) % 3);
        
        // Update LED pattern based on mode
        switch (currentMode) {
            case MODE_SLOW_BLINK:
                Serial.println("Mode: Slow Blink");
                LEDController.setBlinkRate(1000);
                break;
                
            case MODE_FAST_BLINK:
                Serial.println("Mode: Fast Blink");
                LEDController.setBlinkRate(200);
                break;
                
            case MODE_SENSOR_DISPLAY:
                Serial.println("Mode: Sensor Display");
                LEDController.setBlinkRate(0);  // Solid on
                LEDController.setState(true);
                break;
        }
    }
    
    // In sensor display mode, show readings
    if (currentMode == MODE_SENSOR_DISPLAY) {
        if (SensorReader.hasNewReading()) {
            float voltage = SensorReader.getVoltage();
            int raw = SensorReader.getRawValue();
            
            Serial.print("Sensor: ");
            Serial.print(voltage, 3);
            Serial.print("V (raw: ");
            Serial.print(raw);
            Serial.println(")");
        }
    }
    
    // Small delay for stability
    delay(10);
}
