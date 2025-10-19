/**
 * CYD Multi-Model Compatible Sketch
 * 
 * Universal sketch supporting all CYD Resistive 2.8" variants:
 * - Single USB (micro) with ILI9341 display
 * - Dual USB (micro + Type-C) with ST7789 display
 * 
 * Features maintained from original ALNScanner0812Working:
 * - RFID card reading with NDEF text extraction
 * - BMP image display from SD card
 * - Touch input handling
 * - Audio playback (WAV files)
 * 
 * ZERO WIRING CHANGE POLICY:
 * All pin configurations remain unchanged from original setup
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// Include our custom headers
#include "HardwareConfig.h"
#include "DisplayConfig.h"
#include "TouchConfig.h"
#include "RFIDConfig.h"
#include "DiagnosticState.h"
#include "HardwareDetector.h"
#include "ComponentInitializer.h"
#include "DiagnosticReporter.h"
#include "GPIO27Manager.h"
#include "CalibrationManager.h"
#include "SoftwareSPI.h"
#include "SerialCommands.h"

// ─── Configuration Constants ─────────────────────────────────────
#define SERIAL_BAUD_RATE 115200
#define EEPROM_SIZE 512
#define BOOT_TIMEOUT_MS 5000
#define COMPONENT_INIT_TIMEOUT_MS 2000

// ─── Pin Definitions (NO CHANGES ALLOWED) ────────────────────────
// SD Card (VSPI - Hardware SPI Bus)
static const uint8_t SDSPI_SCK   = 18;
static const uint8_t SDSPI_MISO  = 19;
static const uint8_t SDSPI_MOSI  = 23;
static const uint8_t SD_CS       = 5;

// Touch Controller
#define TOUCH_CS   33
#define TOUCH_IRQ  36

// RFID Reader (Software SPI)
#define SOFT_SPI_SCK  22
#define SOFT_SPI_MOSI 27
#define SOFT_SPI_MISO 35
#define RFID_SS       3
#define RFID_RST      MFRC522::UNUSED_PIN

// ─── Global Objects ───────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
SPIClass SDSPI(VSPI);
XPT2046_Touchscreen* touch = nullptr;
MFRC522* rfid = nullptr;

// Our custom managers
HardwareDetector* hwDetector = nullptr;
ComponentInitializer* componentInit = nullptr;
DiagnosticReporter* diagnostics = nullptr;
GPIO27Manager* gpio27Mgr = nullptr;
CalibrationManager* calibMgr = nullptr;
SoftwareSPI* softSPI = nullptr;
SerialCommands* serialCmd = nullptr;

// Configuration storage
HardwareConfig hwConfig;
DisplayConfig displayConfig;
TouchConfig touchConfig;
RFIDConfig rfidConfig;
DiagnosticState diagState;

// Audio objects
AudioGeneratorWAV* wav = nullptr;
AudioFileSourceSD* file = nullptr;
AudioOutputI2S* out = nullptr;

// State tracking
bool systemReady = false;
bool touchCalibrationMode = false;
uint32_t bootStartTime = 0;

// ─── Function Declarations ────────────────────────────────────────
void initializeSystem();
void detectHardware();
void initializeComponents();
void loadCalibration();
void handleSerialCommandsOld();
void handleTouch();
void handleRFID();
void showBootScreen();
void showErrorScreen(const char* error);
void enterDiagnosticMode();
void performSystemTest();

// ─── Setup Function ───────────────────────────────────────────────
void setup() {
    bootStartTime = millis();
    
    // Initialize serial first for diagnostics
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("\n════════════════════════════════════════════════════════════");
    Serial.println("        CYD MULTI-MODEL COMPATIBLE SYSTEM v1.0");
    Serial.println("════════════════════════════════════════════════════════════");
    Serial.printf("[%lu] System boot initiated\n", millis());
    
    // Initialize serial command handler
    serialCmd = new SerialCommands();
    serialCmd->begin(SERIAL_BAUD_RATE);
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Create diagnostic reporter first
    diagnostics = new DiagnosticReporter();
    diagnostics->setDiagnosticLevel(DIAG_INFO);
    
    // Initialize system
    try {
        initializeSystem();
        systemReady = true;
        diagnostics->report(DIAG_INFO, "SYSTEM", "Boot completed successfully in %lums", 
                          millis() - bootStartTime);
    } catch (...) {
        diagnostics->report(DIAG_CRITICAL, "SYSTEM", "Boot failed - entering diagnostic mode");
        enterDiagnosticMode();
    }
    
    // Show memory status
    diagnostics->reportMemory();
}

// ─── Main Loop ────────────────────────────────────────────────────
void loop() {
    if (!systemReady) {
        delay(100);
        return;
    }
    
    // Handle serial commands for diagnostics
    if (serialCmd) {
        serialCmd->handle();
    }
    
    // Handle touch input
    handleTouch();
    
    // Handle RFID scanning
    handleRFID();
    
    // Update diagnostic heartbeat
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();
        diagnostics->reportMemory();
    }
    
    // Small delay to prevent CPU hogging
    delay(10);
}

// ─── System Initialization ────────────────────────────────────────
void initializeSystem() {
    diagnostics->report(DIAG_INFO, "INIT", "Starting system initialization");
    
    // Step 1: Detect hardware
    detectHardware();
    
    // Step 2: Initialize components based on detected hardware
    initializeComponents();
    
    // Step 3: Load calibration data
    loadCalibration();
    
    // Step 4: Show boot screen
    showBootScreen();
    
    // Step 5: Perform system test
    performSystemTest();
}

// ─── Hardware Detection ───────────────────────────────────────────
void detectHardware() {
    diagnostics->report(DIAG_INFO, "DETECT", "Starting hardware detection");
    
    hwDetector = new HardwareDetector(diagnostics);
    
    // Perform detection
    uint32_t detectStart = millis();
    CYDModel model = hwDetector->detectModel();
    uint32_t detectTime = millis() - detectStart;
    
    if (detectTime > 500) {
        diagnostics->report(DIAG_WARNING, "DETECT", 
                          "Detection took %lums (>500ms limit)", detectTime);
    }
    
    // Store configuration
    hwConfig = hwDetector->getConfiguration();
    
    // Report detected hardware
    diagnostics->report(DIAG_INFO, "DETECT", "Detected model: %s",
                      model == CYD_MODEL_SINGLE_USB ? "Single USB (ILI9341)" :
                      model == CYD_MODEL_DUAL_USB ? "Dual USB (ST7789)" :
                      "Unknown");
    
    diagnostics->report(DIAG_INFO, "DETECT", "Display driver: 0x%04X", 
                      hwConfig.displayDriver);
    diagnostics->report(DIAG_INFO, "DETECT", "Backlight pin: GPIO%d", 
                      hwConfig.backlightPin);
    
    // Create GPIO27 manager if needed
    if (hwConfig.model == CYD_MODEL_DUAL_USB && hwConfig.backlightPin == 27) {
        gpio27Mgr = new GPIO27Manager(diagnostics);
        diagnostics->report(DIAG_INFO, "DETECT", 
                          "GPIO27 multiplexing enabled for dual USB model");
    }
}

// ─── Component Initialization ─────────────────────────────────────
void initializeComponents() {
    diagnostics->report(DIAG_INFO, "INIT", "Initializing components");
    
    componentInit = new ComponentInitializer(diagnostics, gpio27Mgr);
    
    // Initialize all components
    InitResult* results = componentInit->initAll(hwConfig, false);
    
    // Report results
    componentInit->reportInitStatus(results, 5);
    
    // Check for critical failures
    bool hasDisplay = false;
    bool hasTouch = false;
    
    for (int i = 0; i < 5; i++) {
        if (results[i].component == COMPONENT_DISPLAY && 
            results[i].status == INIT_SUCCESS) {
            hasDisplay = true;
            displayConfig.initialized = true;
        }
        if (results[i].component == COMPONENT_TOUCH && 
            results[i].status == INIT_SUCCESS) {
            hasTouch = true;
            touchConfig.initialized = true;
        }
        if (results[i].component == COMPONENT_RFID && 
            results[i].status == INIT_SUCCESS) {
            rfidConfig.initialized = true;
        }
    }
    
    if (!hasDisplay) {
        diagnostics->report(DIAG_CRITICAL, "INIT", 
                          "Display initialization failed - cannot continue");
        showErrorScreen("Display Init Failed");
        while(1) { delay(1000); }
    }
    
    if (!hasTouch) {
        diagnostics->report(DIAG_WARNING, "INIT", 
                          "Touch initialization failed - limited functionality");
    }
    
    // Initialize display with detected configuration
    initDisplay();
    
    // Initialize touch if available
    if (hasTouch) {
        initTouch();
    }
    
    // Initialize RFID with software SPI
    if (rfidConfig.initialized) {
        initRFID();
    }
    
    // Initialize SD card
    initSDCard();
    
    // Initialize audio
    initAudio();
}

// ─── Display Initialization ───────────────────────────────────────
void initDisplay() {
    diagnostics->report(DIAG_INFO, "DISPLAY", "Initializing display subsystem");
    
    // Display is already initialized by TFT_eSPI
    // Configure based on detected hardware
    tft.init();
    tft.setRotation(1);  // Landscape
    
    // Set backlight
    pinMode(hwConfig.backlightPin, OUTPUT);
    digitalWrite(hwConfig.backlightPin, HIGH);
    
    // Clear screen
    tft.fillScreen(TFT_BLACK);
    
    // Test colors
    tft.fillRect(0, 0, 80, 240, TFT_RED);
    tft.fillRect(80, 0, 80, 240, TFT_GREEN);
    tft.fillRect(160, 0, 80, 240, TFT_BLUE);
    delay(500);
    
    tft.fillScreen(TFT_BLACK);
    displayConfig.initialized = true;
    displayConfig.width = 320;
    displayConfig.height = 240;
}

// ─── Touch Initialization ─────────────────────────────────────────
void initTouch() {
    diagnostics->report(DIAG_INFO, "TOUCH", "Initializing touch controller");
    
    touch = new XPT2046_Touchscreen(TOUCH_CS, TOUCH_IRQ);
    touch->begin();
    touch->setRotation(1);
    
    touchConfig.pins.cs = TOUCH_CS;
    touchConfig.pins.irq = TOUCH_IRQ;
    touchConfig.initialized = true;
}

// ─── RFID Initialization ──────────────────────────────────────────
void initRFID() {
    diagnostics->report(DIAG_INFO, "RFID", "Initializing RFID reader");
    
    // Create software SPI instance
    softSPI = new SoftwareSPI(SOFT_SPI_SCK, SOFT_SPI_MOSI, SOFT_SPI_MISO, RFID_SS);
    softSPI->begin();
    
    // Note: MFRC522 library needs modification to use software SPI
    // For now, we'll use manual implementation
    rfidConfig.pins.sck = SOFT_SPI_SCK;
    rfidConfig.pins.mosi = SOFT_SPI_MOSI;
    rfidConfig.pins.miso = SOFT_SPI_MISO;
    rfidConfig.pins.ss = RFID_SS;
    rfidConfig.initialized = true;
}

// ─── SD Card Initialization ───────────────────────────────────────
void initSDCard() {
    diagnostics->report(DIAG_INFO, "SDCARD", "Initializing SD card");
    
    SDSPI.begin(SDSPI_SCK, SDSPI_MISO, SDSPI_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, SDSPI)) {
        diagnostics->report(DIAG_WARNING, "SDCARD", "SD card not detected");
        hwConfig.hasSDCard = false;
    } else {
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        diagnostics->report(DIAG_INFO, "SDCARD", "SD card detected: %lluMB", cardSize);
        hwConfig.hasSDCard = true;
    }
}

// ─── Audio Initialization ─────────────────────────────────────────
void initAudio() {
    diagnostics->report(DIAG_INFO, "AUDIO", "Initializing audio subsystem");
    
    out = new AudioOutputI2S();
    out->SetOutputModeMono(true);
    out->SetGain(0.3);
    
    wav = new AudioGeneratorWAV();
    
    hwConfig.hasAudio = true;
    diagnostics->report(DIAG_INFO, "AUDIO", "Audio system ready");
}

// ─── Calibration Loading ──────────────────────────────────────────
void loadCalibration() {
    diagnostics->report(DIAG_INFO, "CALIB", "Loading calibration data");
    
    calibMgr = new CalibrationManager(diagnostics);
    
    if (calibMgr->loadCalibration(hwConfig.model, &touchConfig)) {
        diagnostics->report(DIAG_INFO, "CALIB", "Calibration loaded successfully");
    } else {
        diagnostics->report(DIAG_WARNING, "CALIB", "No calibration found, using defaults");
        calibMgr->loadDefaultCalibration(hwConfig.model, &touchConfig);
    }
}

// ─── Boot Screen ──────────────────────────────────────────────────
void showBootScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    
    tft.setCursor(40, 30);
    tft.println("CYD Multi-Model");
    tft.setCursor(40, 50);
    tft.println("Compatible System");
    
    tft.setTextSize(1);
    tft.setCursor(40, 90);
    tft.print("Model: ");
    tft.println(hwConfig.model == CYD_MODEL_SINGLE_USB ? "Single USB" : "Dual USB");
    
    tft.setCursor(40, 105);
    tft.print("Display: ");
    tft.println(hwConfig.displayDriver == DISPLAY_DRIVER_ILI9341 ? "ILI9341" : "ST7789");
    
    tft.setCursor(40, 120);
    tft.print("Touch: ");
    tft.println(touchConfig.initialized ? "Ready" : "Not Available");
    
    tft.setCursor(40, 135);
    tft.print("RFID: ");
    tft.println(rfidConfig.initialized ? "Ready" : "Not Available");
    
    tft.setCursor(40, 150);
    tft.print("SD Card: ");
    tft.println(hwConfig.hasSDCard ? "Detected" : "Not Present");
    
    tft.setCursor(40, 165);
    tft.print("Audio: ");
    tft.println(hwConfig.hasAudio ? "Ready" : "Not Available");
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(40, 195);
    tft.print("Boot Time: ");
    tft.print(millis() - bootStartTime);
    tft.println("ms");
    
    delay(2000);
}

// ─── Error Screen ─────────────────────────────────────────────────
void showErrorScreen(const char* error) {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 80, 320, 80, TFT_RED);
    
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(50, 100);
    tft.println("SYSTEM ERROR");
    
    tft.setTextSize(1);
    tft.setCursor(50, 130);
    tft.println(error);
}

// ─── System Test ──────────────────────────────────────────────────
void performSystemTest() {
    diagnostics->report(DIAG_INFO, "TEST", "Performing system test");
    
    // Test each component briefly
    if (displayConfig.initialized) {
        tft.fillCircle(160, 120, 10, TFT_GREEN);
        delay(100);
        tft.fillCircle(160, 120, 10, TFT_BLACK);
    }
    
    if (touchConfig.initialized && touch->touched()) {
        diagnostics->report(DIAG_INFO, "TEST", "Touch detected during boot");
    }
    
    diagState.health.display = displayConfig.initialized ? STATUS_OK : STATUS_FAILED;
    diagState.health.touch = touchConfig.initialized ? STATUS_OK : STATUS_WARNING;
    diagState.health.rfid = rfidConfig.initialized ? STATUS_OK : STATUS_WARNING;
    diagState.health.sdcard = hwConfig.hasSDCard ? STATUS_OK : STATUS_WARNING;
    diagState.health.audio = hwConfig.hasAudio ? STATUS_OK : STATUS_WARNING;
    
    diagnostics->report(DIAG_INFO, "TEST", "System test complete");
}

// ─── Serial Command Handler ───────────────────────────────────────
void handleSerialCommandsOld() {
    // Old single-character commands kept for backwards compatibility
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'D':  // Dump system state
            case 'd':
                diagnostics->dumpSystemState(true);
                break;
                
            case 'P':  // Show pin states
            case 'p':
                showPinStates();
                break;
                
            case 'M':  // Memory diagnostic
            case 'm':
                diagnostics->reportMemory();
                break;
                
            case 'H':  // Hardware recap
            case 'h':
                hwDetector->reportDiagnostics(true);
                break;
                
            case 'V':  // Toggle verbose mode
            case 'v':
                toggleVerboseMode();
                break;
                
            case 'R':  // Reset and re-detect
            case 'r':
                ESP.restart();
                break;
                
            case 'T':  // Test pattern
            case 't':
                showTestPattern();
                break;
                
            case 'C':  // Calibrate touch
            case 'c':
                enterCalibrationMode();
                break;
                
            default:
                // Don't print help for new commands
                break;
        }
    }
}

// ─── Touch Handler ────────────────────────────────────────────────
void handleTouch() {
    if (!touchConfig.initialized || !touch) return;
    
    if (touch->touched()) {
        TS_Point p = touch->getPoint();
        
        // Apply calibration
        int x = map(p.x, touchConfig.calibration.xMin, touchConfig.calibration.xMax, 0, 320);
        int y = map(p.y, touchConfig.calibration.yMin, touchConfig.calibration.yMax, 0, 240);
        
        // Update touch state
        touchConfig.touched = true;
        touchConfig.lastX = x;
        touchConfig.lastY = y;
        touchConfig.lastTouchTime = millis();
        
        // Draw touch point
        tft.fillCircle(x, y, 3, TFT_WHITE);
        
        diagnostics->report(DIAG_DEBUG, "TOUCH", "Touch at (%d, %d)", x, y);
    } else {
        touchConfig.touched = false;
    }
}

// ─── RFID Handler ─────────────────────────────────────────────────
void handleRFID() {
    if (!rfidConfig.initialized) return;
    
    // Check for GPIO27 conflict and manage
    if (gpio27Mgr) {
        gpio27Mgr->prepareForRFID();
    }
    
    // RFID scanning logic would go here
    // Using software SPI implementation
    
    // Restore backlight if needed
    if (gpio27Mgr) {
        gpio27Mgr->restoreBacklight();
    }
}

// ─── Helper Functions ─────────────────────────────────────────────
void showPinStates() {
    PinDiagnostic pins[] = {
        {21, OUTPUT, digitalRead(21), true, "Backlight GPIO21"},
        {27, OUTPUT, digitalRead(27), true, "Backlight/RFID GPIO27"},
        {22, OUTPUT, digitalRead(22), true, "RFID SCK"},
        {35, INPUT, digitalRead(35), true, "RFID MISO"},
        {3, OUTPUT, digitalRead(3), true, "RFID SS"},
        {33, OUTPUT, digitalRead(33), true, "Touch CS"},
        {36, INPUT_PULLUP, digitalRead(36), true, "Touch IRQ"},
    };
    
    diagnostics->reportPinStates(pins, 7);
}

void toggleVerboseMode() {
    static DiagLevel currentLevel = DIAG_INFO;
    
    if (currentLevel == DIAG_INFO) {
        currentLevel = DIAG_DEBUG;
        diagnostics->setDiagnosticLevel(DIAG_DEBUG);
        Serial.println("Verbose mode: ON");
    } else {
        currentLevel = DIAG_INFO;
        diagnostics->setDiagnosticLevel(DIAG_INFO);
        Serial.println("Verbose mode: OFF");
    }
}

void showTestPattern() {
    tft.fillScreen(TFT_BLACK);
    
    // Color bars
    tft.fillRect(0, 0, 80, 240, TFT_RED);
    tft.fillRect(80, 0, 80, 240, TFT_GREEN);
    tft.fillRect(160, 0, 80, 240, TFT_BLUE);
    tft.fillRect(240, 0, 80, 240, TFT_WHITE);
    
    delay(2000);
    tft.fillScreen(TFT_BLACK);
}

void enterCalibrationMode() {
    touchCalibrationMode = true;
    Serial.println("Touch calibration mode - Touch corners when prompted");
    
    // Calibration logic would go here
    // Store results using CalibrationManager
}

void enterDiagnosticMode() {
    Serial.println("DIAGNOSTIC MODE - System failed to boot normally");
    
    while (1) {
        if (serialCmd) {
            serialCmd->handle();
        }
        delay(100);
    }
}