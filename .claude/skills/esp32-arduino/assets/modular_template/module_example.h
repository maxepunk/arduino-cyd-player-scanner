/**
 * Example Module Header
 * 
 * This demonstrates proper module structure:
 * - Header contains public interface (declarations)
 * - Implementation goes in .cpp file (definitions)
 * - Use extern for global instances
 * - Include guards prevent multiple inclusion
 */

#ifndef MODULE_EXAMPLE_H
#define MODULE_EXAMPLE_H

#include <Arduino.h>
#include "config.h"

/**
 * ModuleExample Class
 * 
 * This is an example of a well-structured module for ESP32.
 * Replace this with your actual module (DisplayManager, WiFiHandler, etc.)
 * 
 * Public methods define the module's interface.
 * Private members and methods are hidden in the implementation.
 */
class ModuleExample {
public:
    /**
     * Initialize the module
     * Call this once in setup()
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * Update the module
     * Call this in loop() for periodic updates
     */
    void update();
    
    /**
     * Set a value
     * @param value The value to set
     */
    void setValue(int value);
    
    /**
     * Get current value
     * @return The current value
     */
    int getValue();
    
    /**
     * Check if module is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return _initialized; }
    
private:
    // Private member variables
    int _value;
    unsigned long _lastUpdate;
    bool _initialized;
    
    // Private methods (implementation details)
    void _internalUpdate();
    void _checkThreshold();
};

/**
 * Global instance of the module
 * Declared with 'extern' here, defined in .cpp
 * This allows using 'ExampleModule' anywhere after including this header
 */
extern ModuleExample ExampleModule;

#endif // MODULE_EXAMPLE_H
