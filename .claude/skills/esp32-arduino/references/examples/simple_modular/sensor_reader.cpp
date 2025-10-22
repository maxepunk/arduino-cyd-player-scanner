/**
 * Sensor Reader Implementation
 */

#include "sensor_reader.h"

// Global instance
SensorReaderClass SensorReader;

bool SensorReaderClass::begin() {
    DEBUG_PRINTLN("Sensor Reader: Initializing...");
    
    // Initialize analog pin (input is default for analog)
    pinMode(SENSOR_PIN, INPUT);
    
    // Initialize sample buffer
    for (int i = 0; i < SENSOR_SAMPLES; i++) {
        _samples[i] = 0;
    }
    
    _rawValue = 0;
    _voltage = 0.0;
    _newReading = false;
    _lastRead = 0;
    _sampleIndex = 0;
    _initialized = true;
    
    // Take initial reading
    _readSensor();
    
    DEBUG_PRINTLN("Sensor Reader: Ready");
    return true;
}

void SensorReaderClass::update() {
    if (!_initialized) {
        return;
    }
    
    unsigned long now = millis();
    
    if (now - _lastRead >= SENSOR_INTERVAL_MS) {
        _lastRead = now;
        _readSensor();
        _newReading = true;
    }
}

bool SensorReaderClass::hasNewReading() {
    // Returns true once per reading, then clears flag
    if (_newReading) {
        _newReading = false;
        return true;
    }
    return false;
}

int SensorReaderClass::getRawValue() {
    return _rawValue;
}

float SensorReaderClass::getVoltage() {
    return _voltage;
}

// Private methods

void SensorReaderClass::_readSensor() {
    // Read raw ADC value
    int reading = analogRead(SENSOR_PIN);
    
    // Store in circular buffer
    _samples[_sampleIndex] = reading;
    _sampleIndex = (_sampleIndex + 1) % SENSOR_SAMPLES;
    
    // Calculate average
    _rawValue = _getAverageValue();
    
    // Convert to voltage
    _voltage = (_rawValue * ADC_VOLTAGE_REF) / ADC_MAX_VALUE;
    
    DEBUG_PRINT("Sensor Reader: Raw=");
    DEBUG_PRINT(_rawValue);
    DEBUG_PRINT(", Voltage=");
    DEBUG_PRINT(_voltage, 3);
    DEBUG_PRINTLN("V");
}

int SensorReaderClass::_getAverageValue() {
    // Calculate average of all samples
    long sum = 0;
    for (int i = 0; i < SENSOR_SAMPLES; i++) {
        sum += _samples[i];
    }
    return sum / SENSOR_SAMPLES;
}
