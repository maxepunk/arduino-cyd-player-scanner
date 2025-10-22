/**
 * Example Module Implementation
 * 
 * This file contains the actual code for the module.
 * Always include the module's own header first.
 */

#include "module_example.h"

// ============================================================================
// Global Instance Definition
// ============================================================================
// This creates the actual object (declared extern in .h)
ModuleExample ExampleModule;

// ============================================================================
// Public Methods
// ============================================================================

bool ModuleExample::begin() {
    DEBUG_PRINTLN("ExampleModule: Initializing...");
    
    // Initialize member variables
    _value = 0;
    _lastUpdate = 0;
    _initialized = false;
    
    // Add initialization logic here
    // For example: configure pins, initialize hardware, etc.
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Mark as initialized
    _initialized = true;
    
    DEBUG_PRINTLN("ExampleModule: Ready");
    return true;
}

void ModuleExample::update() {
    // Don't do anything if not initialized
    if (!_initialized) {
        return;
    }
    
    // Non-blocking update pattern using millis()
    unsigned long currentTime = millis();
    
    if (currentTime - _lastUpdate >= UPDATE_INTERVAL_MS) {
        _lastUpdate = currentTime;
        
        // Perform periodic update
        _internalUpdate();
        _checkThreshold();
    }
}

void ModuleExample::setValue(int value) {
    _value = value;
    
    DEBUG_PRINT("ExampleModule: Value set to ");
    DEBUG_PRINTLN(value);
    
    // Could trigger other actions here
    if (_value > THRESHOLD_VALUE) {
        DEBUG_PRINTLN("ExampleModule: Threshold exceeded!");
    }
}

int ModuleExample::getValue() {
    return _value;
}

// ============================================================================
// Private Methods (Implementation Details)
// ============================================================================

void ModuleExample::_internalUpdate() {
    // This is private implementation detail
    // Users of the module don't need to know about this
    
    _value++;
    
    DEBUG_PRINT("ExampleModule: Internal update, value = ");
    DEBUG_PRINTLN(_value);
    
    // Example: blink LED based on value
    if (_value % 2 == 0) {
        digitalWrite(LED_PIN, HIGH);
    } else {
        digitalWrite(LED_PIN, LOW);
    }
    
    // Report heap usage periodically
    if (_value % 10 == 0) {
        DEBUG_PRINT("ExampleModule: Free heap = ");
        DEBUG_PRINT(ESP.getFreeHeap());
        DEBUG_PRINTLN(" bytes");
    }
}

void ModuleExample::_checkThreshold() {
    // Another private helper method
    if (_value > MAX_SAMPLES) {
        DEBUG_PRINTLN("ExampleModule: Resetting value (exceeded max)");
        _value = 0;
    }
}

// ============================================================================
// Notes for Creating Your Own Modules
// ============================================================================
// 
// 1. Replace "ModuleExample" with your module name (e.g., "DisplayManager")
// 2. Replace "EXAMPLE" prefix in header guard
// 3. Add necessary includes for your module's needs
// 4. Implement your module's specific functionality
// 5. Keep public interface minimal and clean
// 6. Hide implementation details in private methods
// 7. Use const for methods that don't modify state
// 8. Document public methods with comments
// 9. Use DEBUG_PRINTLN for optional debug output
// 10. Test your module independently before integrating
//
