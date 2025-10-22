/**
 * Sensor Reader Module
 * 
 * Reads analog sensor with averaging filter.
 * Demonstrates periodic reading and data smoothing.
 */

#ifndef SENSOR_READER_H
#define SENSOR_READER_H

#include <Arduino.h>
#include "config.h"

class SensorReaderClass {
public:
    bool begin();
    void update();
    bool hasNewReading();
    int getRawValue();
    float getVoltage();
    
private:
    int _rawValue;
    float _voltage;
    bool _newReading;
    unsigned long _lastRead;
    int _samples[SENSOR_SAMPLES];
    int _sampleIndex;
    bool _initialized;
    
    void _readSensor();
    int _getAverageValue();
};

extern SensorReaderClass SensorReader;

#endif // SENSOR_READER_H
