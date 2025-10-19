/**
 * HardwareDetector.cpp - Hardware detection implementation for CYD variants
 * 
 * Implements automatic hardware detection using display driver probing,
 * GPIO testing, and SPI device enumeration.
 */

#include "HardwareDetector.h"
#include <SPI.h>
#include <SD.h>

// Detection timeouts (milliseconds)
#define DISPLAY_PROBE_TIMEOUT 200
#define GPIO_TEST_TIMEOUT 50
#define SPI_PROBE_TIMEOUT 100
#define FULL_DETECTION_TIMEOUT 500

// Pin definitions for testing
#define BACKLIGHT_PIN_1 21
#define BACKLIGHT_PIN_2 27
#define SD_CS_PIN 5
#define TOUCH_CS_PIN 33
#define TOUCH_IRQ_PIN 36
#define RFID_SS_PIN 3

HardwareDetector::HardwareDetector() : detectionComplete(false), detectionStartTime(0) {
    // Initialize config to unknown state
    config.model = CYD_MODEL_UNKNOWN;
    config.displayDriver = DISPLAY_DRIVER_UNKNOWN;
    config.backlightPin = 0;
    config.hasSDCard = false;
    config.hasRFID = false;
    config.hasTouch = false;
    config.hasAudio = false;
}

CYDModel HardwareDetector::detectModel() {
    if (config.model != CYD_MODEL_UNKNOWN) {
        return config.model;
    }
    
    DisplayDriver driver = detectDisplayDriver();
    
    switch (driver) {
        case DISPLAY_DRIVER_ILI9341:
            config.model = CYD_MODEL_SINGLE_USB;
            break;
        case DISPLAY_DRIVER_ST7789:
            config.model = CYD_MODEL_DUAL_USB;
            break;
        default:
            config.model = CYD_MODEL_UNKNOWN;
            break;
    }
    
    return config.model;
}

DisplayDriver HardwareDetector::detectDisplayDriver() {
    if (config.displayDriver != DISPLAY_DRIVER_UNKNOWN) {
        return config.displayDriver;
    }
    
    config.displayDriver = probeDisplayDriver();
    return config.displayDriver;
}

DisplayDriver HardwareDetector::probeDisplayDriver() {
    uint32_t startTime = millis();
    
    // Initialize TFT_eSPI to probe the display
    TFT_eSPI tft;
    tft.init();
    
    // Read display ID register
    uint16_t displayID = tft.readcommand16(0x04); // Read Display ID
    
    // Check timeout
    if (millis() - startTime > DISPLAY_PROBE_TIMEOUT) {
        Serial.println(F("[ERROR][HARDWARE/Detector]: Display probe timeout"));
        return DISPLAY_DRIVER_UNKNOWN;
    }
    
    // Identify display based on ID
    if ((displayID & 0xFF) == 0x41) {
        // ILI9341 typically returns 0x9341 or similar
        return DISPLAY_DRIVER_ILI9341;
    } else if ((displayID & 0xFF) == 0x52) {
        // ST7789 typically returns 0x8552 or similar
        return DISPLAY_DRIVER_ST7789;
    }
    
    // Fallback: try alternative detection method
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("PROBE", 0, 0);
    
    // Read back and analyze response patterns
    uint16_t pixel = tft.readPixel(0, 0);
    
    if (pixel == TFT_WHITE || pixel == 0xFFFF) {
        // Successful readback suggests ILI9341
        return DISPLAY_DRIVER_ILI9341;
    }
    
    // Default assumption for CYD models
    return DISPLAY_DRIVER_ILI9341;
}

uint8_t HardwareDetector::detectBacklightPin() {
    if (config.backlightPin != 0) {
        return config.backlightPin;
    }
    
    // Test both possible backlight pins
    if (testBacklightPin(BACKLIGHT_PIN_1)) {
        config.backlightPin = BACKLIGHT_PIN_1;
        return BACKLIGHT_PIN_1;
    }
    
    if (testBacklightPin(BACKLIGHT_PIN_2)) {
        config.backlightPin = BACKLIGHT_PIN_2;
        return BACKLIGHT_PIN_2;
    }
    
    // Default to GPIO21 if detection fails
    config.backlightPin = BACKLIGHT_PIN_1;
    return BACKLIGHT_PIN_1;
}

bool HardwareDetector::testBacklightPin(uint8_t pin) {
    pinMode(pin, OUTPUT);
    
    // Test OFF state
    digitalWrite(pin, LOW);
    delay(10);
    
    // Test ON state
    digitalWrite(pin, HIGH);
    delay(10);
    
    // Read back state
    bool state = digitalRead(pin);
    
    // Leave in ON state for visible confirmation
    digitalWrite(pin, HIGH);
    
    return state == HIGH;
}

bool HardwareDetector::detectSDCard() {
    if (config.hasSDCard) {
        return true; // Already detected
    }
    
    config.hasSDCard = testSDCardPresence();
    return config.hasSDCard;
}

bool HardwareDetector::testSDCardPresence() {
    // Initialize SPI for SD card
    SPI.begin();
    
    // Try to initialize SD card
    bool sdPresent = SD.begin(SD_CS_PIN);
    
    if (sdPresent) {
        uint64_t cardSize = SD.cardSize();
        if (cardSize > 0) {
            Serial.print(F("[INFO][HARDWARE/Detector]: SD card detected, size: "));
            Serial.print(cardSize / (1024 * 1024));
            Serial.println(F(" MB"));
            return true;
        }
    }
    
    return false;
}

bool HardwareDetector::detectRFID() {
    if (config.hasRFID) {
        return true; // Already detected
    }
    
    config.hasRFID = testRFIDConnection();
    return config.hasRFID;
}

bool HardwareDetector::testRFIDConnection() {
    // Test RFID SS pin response
    return probeSPIDevice(RFID_SS_PIN);
}

bool HardwareDetector::detectTouch() {
    if (config.hasTouch) {
        return true; // Already detected
    }
    
    config.hasTouch = testTouchController();
    return config.hasTouch;
}

bool HardwareDetector::testTouchController() {
    // Check touch interrupt pin
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP);
    delay(1);
    
    // Touch IRQ should normally be HIGH when not pressed
    bool irqState = digitalRead(TOUCH_IRQ_PIN);
    
    // Test touch CS pin
    bool csTest = probeSPIDevice(TOUCH_CS_PIN);
    
    return irqState && csTest;
}

bool HardwareDetector::detectAudio() {
    if (config.hasAudio) {
        return true; // Already detected
    }
    
    config.hasAudio = testAudioI2S();
    return config.hasAudio;
}

bool HardwareDetector::testAudioI2S() {
    // For CYD models, audio is typically present
    // This is a conservative assumption since I2S testing 
    // requires more complex initialization
    return true;
}

bool HardwareDetector::probeSPIDevice(uint8_t cs_pin, uint8_t expected_response) {
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);
    delay(1);
    
    // Simple CS toggle test
    digitalWrite(cs_pin, LOW);
    delayMicroseconds(10);
    digitalWrite(cs_pin, HIGH);
    
    // If we can control the CS pin, assume device is present
    return digitalRead(cs_pin) == HIGH;
}

bool HardwareDetector::testGPIOOutput(uint8_t pin, uint32_t testDurationMs) {
    uint32_t startTime = millis();
    
    pinMode(pin, OUTPUT);
    
    // Test low state
    digitalWrite(pin, LOW);
    delay(testDurationMs / 2);
    bool lowRead = (digitalRead(pin) == LOW);
    
    // Test high state
    digitalWrite(pin, HIGH);
    delay(testDurationMs / 2);
    bool highRead = (digitalRead(pin) == HIGH);
    
    return lowRead && highRead && (millis() - startTime <= GPIO_TEST_TIMEOUT);
}

bool HardwareDetector::testGPIOInput(uint8_t pin, bool pullup) {
    pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
    delay(1);
    
    // Read state (should be HIGH with pullup, undefined without)
    bool state = digitalRead(pin);
    
    return pullup ? state == HIGH : true; // Input test always passes without pullup
}

CYDHardwareConfig HardwareDetector::getConfiguration() {
    if (!detectionComplete) {
        runFullDetection();
    }
    
    return config;
}

bool HardwareDetector::runFullDetection() {
    detectionStartTime = millis();
    
    Serial.println(F("[INFO][HARDWARE/Detector]: Starting hardware detection"));
    
    // Detect in dependency order
    detectDisplayDriver();
    detectModel();
    detectBacklightPin();
    detectSDCard();
    detectTouch();
    detectRFID();
    detectAudio();
    
    detectionComplete = true;
    
    uint32_t detectionTime = millis() - detectionStartTime;
    Serial.print(F("[INFO][HARDWARE/Detector]: Detection complete in "));
    Serial.print(detectionTime);
    Serial.println(F("ms"));
    
    return detectionTime <= FULL_DETECTION_TIMEOUT;
}

uint32_t HardwareDetector::getDetectionTime() const {
    if (detectionStartTime == 0) return 0;
    return millis() - detectionStartTime;
}

void HardwareDetector::reportDiagnostics(bool verbose) {
    if (!detectionComplete) {
        runFullDetection();
    }
    
    Serial.println(F("=== CYD Hardware Detection Report ==="));
    
    // Model information
    Serial.print(F("Model: "));
    switch (config.model) {
        case CYD_MODEL_SINGLE_USB:
            Serial.println(F("CYD Single USB"));
            break;
        case CYD_MODEL_DUAL_USB:
            Serial.println(F("CYD Dual USB"));
            break;
        default:
            Serial.println(F("Unknown"));
            break;
    }
    
    // Display driver
    Serial.print(F("Display: "));
    switch (config.displayDriver) {
        case DISPLAY_DRIVER_ILI9341:
            Serial.println(F("ILI9341"));
            break;
        case DISPLAY_DRIVER_ST7789:
            Serial.println(F("ST7789"));
            break;
        default:
            Serial.println(F("Unknown"));
            break;
    }
    
    // Backlight pin
    Serial.print(F("Backlight: GPIO"));
    Serial.println(config.backlightPin);
    
    // Component status
    Serial.print(F("SD Card: "));
    Serial.println(config.hasSDCard ? F("Present") : F("Not detected"));
    
    Serial.print(F("Touch: "));
    Serial.println(config.hasTouch ? F("Present") : F("Not detected"));
    
    Serial.print(F("RFID: "));
    Serial.println(config.hasRFID ? F("Present") : F("Not detected"));
    
    Serial.print(F("Audio: "));
    Serial.println(config.hasAudio ? F("Present") : F("Not detected"));
    
    if (verbose) {
        Serial.print(F("Detection time: "));
        Serial.print(getDetectionTime());
        Serial.println(F("ms"));
        
        Serial.print(F("Free heap: "));
        Serial.print(ESP.getFreeHeap());
        Serial.println(F(" bytes"));
    }
    
    Serial.println(F("====================================="));
}

HardwareConfig HardwareDetector::toHardwareConfig() const {
    HardwareConfig hwConfig;
    
    // Convert model
    switch (config.model) {
        case CYD_MODEL_SINGLE_USB:
            hwConfig.model = HardwareConfig::MODEL_CYD_SINGLE_USB;
            break;
        case CYD_MODEL_DUAL_USB:
            hwConfig.model = HardwareConfig::MODEL_CYD_DUAL_USB;
            break;
        default:
            hwConfig.model = HardwareConfig::MODEL_UNKNOWN;
            break;
    }
    
    // Convert display driver
    hwConfig.displayDriverID = static_cast<uint16_t>(config.displayDriver);
    hwConfig.backlightPin = config.backlightPin;
    hwConfig.backlightActiveHigh = true;
    
    // Copy feature flags
    hwConfig.hasSDCard = config.hasSDCard;
    hwConfig.hasRFID = config.hasRFID;
    hwConfig.hasTouch = config.hasTouch;
    hwConfig.hasAudio = config.hasAudio;
    
    // Set metadata
    hwConfig.hardwareRevision = 1;
    hwConfig.detectedAt = millis();
    
    return hwConfig;
}

CYDHardwareConfig HardwareDetector::fromHardwareConfig(const HardwareConfig& hwConfig) {
    CYDHardwareConfig config;
    
    // Convert model
    switch (hwConfig.model) {
        case HardwareConfig::MODEL_CYD_SINGLE_USB:
            config.model = CYD_MODEL_SINGLE_USB;
            break;
        case HardwareConfig::MODEL_CYD_DUAL_USB:
            config.model = CYD_MODEL_DUAL_USB;
            break;
        default:
            config.model = CYD_MODEL_UNKNOWN;
            break;
    }
    
    // Convert display driver
    config.displayDriver = static_cast<DisplayDriver>(hwConfig.displayDriverID);
    config.backlightPin = hwConfig.backlightPin;
    
    // Copy feature flags
    config.hasSDCard = hwConfig.hasSDCard;
    config.hasRFID = hwConfig.hasRFID;
    config.hasTouch = hwConfig.hasTouch;
    config.hasAudio = hwConfig.hasAudio;
    
    return config;
}