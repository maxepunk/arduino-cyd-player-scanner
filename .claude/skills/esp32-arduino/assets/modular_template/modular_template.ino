/**
 * Modular ESP32 Project Template
 * 
 * This template demonstrates proper multi-file organization
 * for ESP32 Arduino projects compiled with Arduino CLI.
 * 
 * Structure:
 * - config.h: Configuration constants
 * - module_example.h/cpp: Example module implementation
 * - This file: High-level coordination only
 */

#include "config.h"
#include "module_example.h"

void setup() {
    // Initialize serial
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("\n=================================");
    Serial.println("  Modular ESP32 Project");
    Serial.println("=================================\n");
    
    // Initialize modules in order
    DEBUG_PRINTLN("Initializing modules...");
    
    if (!ExampleModule.begin()) {
        Serial.println("ERROR: Module initialization failed!");
        while(1) delay(1000);  // Halt on critical error
    }
    
    DEBUG_PRINTLN("All modules initialized");
    Serial.println("Setup complete!\n");
}

void loop() {
    // Update modules
    ExampleModule.update();
    
    // Add your main application logic here
    // This should remain high-level coordination only
    // Move complex logic into modules
    
    delay(LOOP_DELAY_MS);
}
