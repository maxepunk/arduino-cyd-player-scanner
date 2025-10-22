/**
 * LED Controller Module
 * 
 * Manages LED blinking patterns with configurable rates.
 * Demonstrates non-blocking timing and state management.
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

class LEDControllerClass {
public:
    bool begin();
    void update();
    void setBlinkRate(unsigned long rateMs);
    void setState(bool on);
    bool getState() const { return _currentState; }
    
private:
    bool _currentState;
    unsigned long _lastToggle;
    unsigned long _blinkRate;
    bool _initialized;
};

extern LEDControllerClass LEDController;

#endif // LED_CONTROLLER_H
