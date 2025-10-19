/**
 * Serial Commands Handler
 * Implements all serial commands per contracts/serial-api.yaml
 */

#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include <Arduino.h>
#include <ArduinoJson.h>

class SerialCommands {
public:
    SerialCommands();
    void begin(uint32_t baudRate = 115200);
    void handle();
    
    // Command handlers
    void handleDIAG();
    void handleTEST_DISPLAY();
    void handleTEST_TOUCH();
    void handleTEST_RFID();
    void handleTEST_SD();
    void handleTEST_AUDIO();
    void handleCALIBRATE();
    void handleWIRING();
    void handleSCAN();
    void handleVERSION();
    void handleRESET();
    
    // Set callbacks for component access
    void setDisplayCallback(void (*callback)());
    void setTouchCallback(void (*callback)());
    void setRFIDCallback(void (*callback)());
    void setSDCallback(void (*callback)());
    void setAudioCallback(void (*callback)());
    void setCalibrationCallback(void (*callback)());
    
private:
    String readCommand();
    void printHelp();
    void sendJSONResponse(const JsonDocument& doc);
    
    // Callbacks
    void (*displayTestCallback)() = nullptr;
    void (*touchTestCallback)() = nullptr;
    void (*rfidTestCallback)() = nullptr;
    void (*sdTestCallback)() = nullptr;
    void (*audioTestCallback)() = nullptr;
    void (*calibrationCallback)() = nullptr;
    
    String commandBuffer;
    uint32_t lastCommandTime;
    static const uint32_t COMMAND_TIMEOUT = 5000;
};

#endif // SERIAL_COMMANDS_H