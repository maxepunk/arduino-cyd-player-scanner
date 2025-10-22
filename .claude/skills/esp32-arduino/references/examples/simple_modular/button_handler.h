/**
 * Button Handler Module
 * 
 * Handles button input with debouncing.
 * Demonstrates edge detection and timing.
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

class ButtonHandlerClass {
public:
    bool begin();
    void update();
    bool wasPressed();      // Returns true once per press
    bool isPressed();       // Returns current state
    
private:
    bool _lastState;
    bool _currentState;
    bool _pressDetected;
    unsigned long _lastDebounceTime;
    bool _initialized;
};

extern ButtonHandlerClass ButtonHandler;

#endif // BUTTON_HANDLER_H
