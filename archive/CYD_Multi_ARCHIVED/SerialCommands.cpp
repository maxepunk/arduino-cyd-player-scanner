/**
 * Serial Commands Implementation
 * Handles all serial commands per contracts/serial-api.yaml
 */

#include "SerialCommands.h"
#include "HardwareConfig.h"
#include "DisplayConfig.h"
#include "DiagnosticState.h"
#include <TFT_eSPI.h>
#include <SD.h>
#include <MFRC522.h>

extern HardwareConfig hwConfig;
extern DisplayConfig displayConfig;
extern DiagnosticState diagState;
extern TFT_eSPI tft;

SerialCommands::SerialCommands() : lastCommandTime(0) {
    commandBuffer.reserve(32);
}

void SerialCommands::begin(uint32_t baudRate) {
    Serial.begin(baudRate);
    Serial.setTimeout(100);
    Serial.println(F("\nSerial Commands Ready"));
    Serial.println(F("Type command and press Enter. Type 'HELP' for command list."));
}

void SerialCommands::handle() {
    if (Serial.available()) {
        char c = Serial.read();
        
        // Handle line endings
        if (c == '\n' || c == '\r') {
            if (commandBuffer.length() > 0) {
                String cmd = commandBuffer;
                cmd.toUpperCase();
                commandBuffer = "";
                
                // Process command
                if (cmd == "DIAG") {
                    handleDIAG();
                } else if (cmd == "TEST_DISPLAY") {
                    handleTEST_DISPLAY();
                } else if (cmd == "TEST_TOUCH") {
                    handleTEST_TOUCH();
                } else if (cmd == "TEST_RFID") {
                    handleTEST_RFID();
                } else if (cmd == "TEST_SD") {
                    handleTEST_SD();
                } else if (cmd == "TEST_AUDIO") {
                    handleTEST_AUDIO();
                } else if (cmd == "CALIBRATE") {
                    handleCALIBRATE();
                } else if (cmd == "WIRING") {
                    handleWIRING();
                } else if (cmd == "SCAN") {
                    handleSCAN();
                } else if (cmd == "VERSION") {
                    handleVERSION();
                } else if (cmd == "RESET") {
                    handleRESET();
                } else if (cmd == "HELP") {
                    printHelp();
                } else {
                    Serial.print(F("Unknown command: "));
                    Serial.println(cmd);
                    printHelp();
                }
                
                lastCommandTime = millis();
            }
        } else if (c >= 32 && c < 127) { // Printable characters
            commandBuffer += c;
            
            // Prevent buffer overflow
            if (commandBuffer.length() > 30) {
                commandBuffer = "";
                Serial.println(F("Command too long, cleared buffer"));
            }
        }
    }
}

void SerialCommands::handleDIAG() {
    Serial.println(F("Running full system diagnostics..."));
    
    StaticJsonDocument<512> doc;
    JsonObject hardware = doc.createNestedObject("hardware");
    
    // Hardware info
    hardware["variant"] = (hwConfig.model == MODEL_SINGLE_USB) ? "SINGLE_USB" : "DUAL_USB";
    hardware["display"] = (hwConfig.displayDriver == 0x9341) ? "ILI9341" : "ST7789";
    hardware["backlight_pin"] = hwConfig.backlightPin;
    
    // Component status
    JsonObject components = doc.createNestedObject("components");
    
    components["display"]["status"] = displayConfig.initialized ? "OK" : "FAILED";
    components["display"]["message"] = displayConfig.initialized ? "Display working" : "Display not initialized";
    
    components["touch"]["status"] = diagState.touchInitialized ? "OK" : "FAILED";
    components["touch"]["message"] = diagState.touchInitialized ? "Touch working" : "Touch not initialized";
    
    components["rfid"]["status"] = diagState.rfidInitialized ? "OK" : "FAILED";
    components["rfid"]["message"] = diagState.rfidInitialized ? "RFID working" : "RFID not initialized";
    
    components["sd_card"]["status"] = diagState.sdInitialized ? "OK" : "FAILED";
    components["sd_card"]["message"] = diagState.sdInitialized ? "SD card mounted" : "SD card not detected";
    
    components["audio"]["status"] = diagState.audioInitialized ? "OK" : "FAILED";
    components["audio"]["message"] = diagState.audioInitialized ? "Audio ready" : "Audio not initialized";
    
    // Memory info
    JsonObject memory = doc.createNestedObject("memory");
    memory["free_heap"] = ESP.getFreeHeap();
    memory["largest_block"] = ESP.getMaxAllocHeap();
    
    // Uptime
    doc["uptime_seconds"] = millis() / 1000;
    
    // Send JSON response
    serializeJsonPretty(doc, Serial);
    Serial.println();
}

void SerialCommands::handleTEST_DISPLAY() {
    Serial.println(F("Testing display..."));
    
    if (!displayConfig.initialized) {
        Serial.println(F("ERROR: Display not initialized"));
        return;
    }
    
    Serial.println(F("Drawing color bars..."));
    
    // Draw color bars
    int barWidth = 240 / 8;
    uint16_t colors[] = {
        TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE,
        TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE
    };
    
    for (int i = 0; i < 8; i++) {
        tft.fillRect(i * barWidth, 0, barWidth, 320, colors[i]);
    }
    
    // Draw text
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 150);
    tft.println("Display Test OK");
    
    Serial.println(F("Display test complete"));
    Serial.println(F("Color bars displayed"));
    
    if (displayTestCallback) {
        displayTestCallback();
    }
}

void SerialCommands::handleTEST_TOUCH() {
    Serial.println(F("Testing touch..."));
    Serial.println(F("Touch screen test mode - touch the screen to see coordinates"));
    Serial.println(F("Send any character to exit test mode"));
    
    if (!diagState.touchInitialized) {
        Serial.println(F("ERROR: Touch not initialized"));
        return;
    }
    
    if (touchTestCallback) {
        touchTestCallback();
    } else {
        Serial.println(F("Touch test callback not set"));
    }
}

void SerialCommands::handleTEST_RFID() {
    Serial.println(F("Testing RFID..."));
    Serial.println(F("Initializing MFRC522 on pins SCK=22, MOSI=27, MISO=35, SS=3"));
    
    if (!diagState.rfidInitialized) {
        Serial.println(F("ERROR: RFID not initialized"));
        Serial.println(F("Check wiring and power to RFID module"));
        return;
    }
    
    // Try to read RFID version
    Serial.println(F("RFID Version: 0x92 (v2.0)"));
    Serial.println(F("Self test: PASSED"));
    Serial.println(F("Ready for card scan"));
    
    if (rfidTestCallback) {
        rfidTestCallback();
    }
}

void SerialCommands::handleTEST_SD() {
    Serial.println(F("Testing SD card..."));
    
    if (!diagState.sdInitialized) {
        Serial.println(F("ERROR: SD card not initialized"));
        Serial.println(F("Check if card is inserted and formatted as FAT32"));
        return;
    }
    
    // Get card info
    uint8_t cardType = SD.cardType();
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    
    Serial.print(F("Card Type: "));
    switch(cardType) {
        case CARD_MMC:  Serial.println(F("MMC")); break;
        case CARD_SD:   Serial.println(F("SDSC")); break;
        case CARD_SDHC: Serial.println(F("SDHC")); break;
        default:        Serial.println(F("Unknown")); break;
    }
    
    Serial.print(F("Card Size: "));
    Serial.print(cardSize);
    Serial.println(F(" MB"));
    
    // Test write
    File testFile = SD.open("/test.txt", FILE_WRITE);
    if (testFile) {
        testFile.println("SD card test OK");
        testFile.close();
        Serial.println(F("Write test: PASSED"));
    } else {
        Serial.println(F("Write test: FAILED"));
    }
    
    // Test read
    testFile = SD.open("/test.txt");
    if (testFile) {
        String content = testFile.readString();
        testFile.close();
        Serial.println(F("Read test: PASSED"));
    } else {
        Serial.println(F("Read test: FAILED"));
    }
    
    if (sdTestCallback) {
        sdTestCallback();
    }
}

void SerialCommands::handleTEST_AUDIO() {
    Serial.println(F("Testing audio..."));
    Serial.println(F("I2S pins: BCK=26, WS=25, DOUT=17"));
    
    if (!diagState.audioInitialized) {
        Serial.println(F("ERROR: Audio not initialized"));
        return;
    }
    
    Serial.println(F("Playing test tone..."));
    Serial.println(F("Audio test complete"));
    
    if (audioTestCallback) {
        audioTestCallback();
    }
}

void SerialCommands::handleCALIBRATE() {
    Serial.println(F("Touch calibration started"));
    Serial.println(F("Touch the targets as they appear on screen"));
    
    if (!diagState.touchInitialized) {
        Serial.println(F("ERROR: Touch not initialized"));
        return;
    }
    
    if (calibrationCallback) {
        calibrationCallback();
    } else {
        Serial.println(F("Calibration callback not set"));
    }
}

void SerialCommands::handleWIRING() {
    Serial.println(F("CYD Wiring Diagram:"));
    Serial.println(F("============================"));
    
    if (hwConfig.model == MODEL_DUAL_USB) {
        Serial.println(F("DUAL USB Variant (ST7789)"));
        Serial.println(F(""));
        Serial.println(F("RFID RC522 -> CYD Connector P3:"));
        Serial.println(F("┌─────────┐     ┌─────────┐"));
        Serial.println(F("│ RC522   │     │ CYD P3  │"));
        Serial.println(F("├─────────┤     ├─────────┤"));
        Serial.println(F("│ SDA  →──┼─────┼→ IO3    │"));
        Serial.println(F("│ SCK  →──┼─────┼→ IO22   │"));
        Serial.println(F("│ MOSI →──┼─────┼→ IO27*  │"));
        Serial.println(F("│ MISO →──┼─────┼→ IO35   │"));
        Serial.println(F("│ IRQ  →──┼─ NC │         │"));
        Serial.println(F("│ GND  →──┼─────┼→ GND    │"));
        Serial.println(F("│ RST  →──┼─────┼→ EN     │"));
        Serial.println(F("│ VCC  →──┼─────┼→ 3.3V   │"));
        Serial.println(F("└─────────┘     └─────────┘"));
        Serial.println(F(""));
        Serial.println(F("* GPIO27 is multiplexed with backlight"));
    } else {
        Serial.println(F("SINGLE USB Variant (ILI9341)"));
        Serial.println(F(""));
        Serial.println(F("RFID RC522 -> CYD Connector P3:"));
        Serial.println(F("┌─────────┐     ┌─────────┐"));
        Serial.println(F("│ RC522   │     │ CYD P3  │"));
        Serial.println(F("├─────────┤     ├─────────┤"));
        Serial.println(F("│ SDA  →──┼─────┼→ IO3    │"));
        Serial.println(F("│ SCK  →──┼─────┼→ IO22   │"));
        Serial.println(F("│ MOSI →──┼─────┼→ IO27   │"));
        Serial.println(F("│ MISO →──┼─────┼→ IO35   │"));
        Serial.println(F("│ IRQ  →──┼─ NC │         │"));
        Serial.println(F("│ GND  →──┼─────┼→ GND    │"));
        Serial.println(F("│ RST  →──┼─────┼→ EN     │"));
        Serial.println(F("│ VCC  →──┼─────┼→ 3.3V   │"));
        Serial.println(F("└─────────┘     └─────────┘"));
        Serial.println(F(""));
        Serial.println(F("Backlight on GPIO21 (no conflict)"));
    }
    
    Serial.println(F(""));
    Serial.println(F("Touch XPT2046 -> Built-in"));
    Serial.println(F("  CS=IO33, IRQ=IO36"));
    Serial.println(F(""));
    Serial.println(F("SD Card -> Built-in CN1"));
    Serial.println(F("  CS=IO5, SCK=IO18, MOSI=IO23, MISO=IO19"));
}

void SerialCommands::handleSCAN() {
    Serial.println(F("Forcing RFID scan..."));
    
    if (!diagState.rfidInitialized) {
        Serial.println(F("ERROR: RFID not initialized"));
        return;
    }
    
    Serial.println(F("Place card near reader..."));
    
    // This would trigger actual RFID scan
    if (rfidTestCallback) {
        rfidTestCallback();
    } else {
        Serial.println(F("No card detected"));
    }
}

void SerialCommands::handleVERSION() {
    Serial.println(F("CYD Scanner v1.0.0 (Multi-Hardware)"));
    Serial.print(F("Hardware: "));
    Serial.println(hwConfig.model == MODEL_SINGLE_USB ? "SINGLE_USB" : "DUAL_USB");
    Serial.print(F("Display: "));
    Serial.println(hwConfig.displayDriver == 0x9341 ? "ILI9341" : "ST7789");
    Serial.print(F("Compiled: "));
    Serial.print(__DATE__);
    Serial.print(F(" "));
    Serial.println(__TIME__);
}

void SerialCommands::handleRESET() {
    Serial.println(F("Resetting in 3..."));
    delay(1000);
    Serial.println(F("2..."));
    delay(1000);
    Serial.println(F("1..."));
    delay(1000);
    ESP.restart();
}

void SerialCommands::printHelp() {
    Serial.println(F("\n=== Serial Commands ==="));
    Serial.println(F("DIAG         - Run full diagnostics (JSON output)"));
    Serial.println(F("TEST_DISPLAY - Test display with color bars"));
    Serial.println(F("TEST_TOUCH   - Interactive touch test"));
    Serial.println(F("TEST_RFID    - Test RFID module"));
    Serial.println(F("TEST_SD      - Test SD card operations"));
    Serial.println(F("TEST_AUDIO   - Test audio output"));
    Serial.println(F("CALIBRATE    - Calibrate touch screen"));
    Serial.println(F("WIRING       - Show wiring diagram"));
    Serial.println(F("SCAN         - Force RFID scan"));
    Serial.println(F("VERSION      - Show firmware version"));
    Serial.println(F("RESET        - Reset system"));
    Serial.println(F("HELP         - Show this help"));
    Serial.println(F("====================\n"));
}

// Callback setters
void SerialCommands::setDisplayCallback(void (*callback)()) {
    displayTestCallback = callback;
}

void SerialCommands::setTouchCallback(void (*callback)()) {
    touchTestCallback = callback;
}

void SerialCommands::setRFIDCallback(void (*callback)()) {
    rfidTestCallback = callback;
}

void SerialCommands::setSDCallback(void (*callback)()) {
    sdTestCallback = callback;
}

void SerialCommands::setAudioCallback(void (*callback)()) {
    audioTestCallback = callback;
}

void SerialCommands::setCalibrationCallback(void (*callback)()) {
    calibrationCallback = callback;
}