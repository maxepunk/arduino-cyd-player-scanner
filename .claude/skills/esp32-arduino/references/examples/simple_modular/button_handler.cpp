/**
 * Button Handler Implementation
 */

#include "button_handler.h"

// Global instance
ButtonHandlerClass ButtonHandler;

bool ButtonHandlerClass::begin() {
    DEBUG_PRINTLN("Button Handler: Initializing...");
    
    pinMode(BUTTON_PIN, INPUT);  // Boot button has external pull-up
    
    _lastState = HIGH;
    _currentState = HIGH;
    _pressDetected = false;
    _lastDebounceTime = 0;
    _initialized = true;
    
    DEBUG_PRINTLN("Button Handler: Ready");
    return true;
}

void ButtonHandlerClass::update() {
    if (!_initialized) {
        return;
    }
    
    // Read current button state (LOW = pressed for boot button)
    bool reading = digitalRead(BUTTON_PIN);
    
    // Debounce logic
    if (reading != _lastState) {
        _lastDebounceTime = millis();
    }
    
    if ((millis() - _lastDebounceTime) > DEBOUNCE_TIME_MS) {
        // State has been stable for debounce time
        if (reading != _currentState) {
            _currentState = reading;
            
            // Detect falling edge (button pressed)
            if (_currentState == LOW) {
                _pressDetected = true;
                DEBUG_PRINTLN("Button Handler: Button pressed");
            }
        }
    }
    
    _lastState = reading;
}

bool ButtonHandlerClass::wasPressed() {
    // Returns true once per press, then clears flag
    if (_pressDetected) {
        _pressDetected = false;
        return true;
    }
    return false;
}

bool ButtonHandlerClass::isPressed() {
    // Returns current debounced state
    return (_currentState == LOW);
}
