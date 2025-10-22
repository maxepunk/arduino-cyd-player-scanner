/**
 * LED Controller Implementation
 */

#include "led_controller.h"

// Global instance
LEDControllerClass LEDController;

bool LEDControllerClass::begin() {
    DEBUG_PRINTLN("LED Controller: Initializing...");
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    _currentState = false;
    _lastToggle = 0;
    _blinkRate = 500;  // Default 500ms
    _initialized = true;
    
    DEBUG_PRINTLN("LED Controller: Ready");
    return true;
}

void LEDControllerClass::update() {
    if (!_initialized || _blinkRate == 0) {
        return;  // Don't blink if rate is 0 (solid state)
    }
    
    unsigned long now = millis();
    
    if (now - _lastToggle >= _blinkRate) {
        _lastToggle = now;
        _currentState = !_currentState;
        digitalWrite(LED_PIN, _currentState ? HIGH : LOW);
    }
}

void LEDControllerClass::setBlinkRate(unsigned long rateMs) {
    _blinkRate = rateMs;
    _lastToggle = millis();  // Reset timing
    
    DEBUG_PRINT("LED Controller: Blink rate set to ");
    DEBUG_PRINT(rateMs);
    DEBUG_PRINTLN(" ms");
}

void LEDControllerClass::setState(bool on) {
    _currentState = on;
    digitalWrite(LED_PIN, on ? HIGH : LOW);
    
    DEBUG_PRINT("LED Controller: State set to ");
    DEBUG_PRINTLN(on ? "ON" : "OFF");
}
