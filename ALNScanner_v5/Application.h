#pragma once

/**
 * @file Application.h
 * @brief Main application orchestrator for ALNScanner v5.0
 *
 * This class integrates all HAL components, services, and the UI state machine
 * to provide the complete scanning application functionality.
 *
 * ARCHITECTURE PATTERN: Facade + Dependency Injection
 * - HAL components: Singleton pattern with getInstance()
 * - Services: Singleton pattern with getInstance()
 * - UI: Instance member, needs HAL singleton references
 *
 * INITIALIZATION ORDER (CRITICAL):
 * 1. HAL components (via getInstance() calls)
 * 2. Services (via getInstance() calls)
 * 3. UI state machine (constructed with HAL references)
 * 4. State variables (primitives)
 *
 * EXTRACTED FROM: ALNScanner1021_Orchestrator v4.1
 * - setup() logic: lines 2615-2925
 * - loop() logic: lines 3563-3840
 * - RFID scan processing: lines 3678-3839
 * - Serial command registration: lines 2938-3392
 *
 * Phase 5: Application Integration
 * Status: Skeleton created, implementations pending
 */

#include "config.h"
#include "hal/RFIDReader.h"
#include "hal/DisplayDriver.h"
#include "hal/AudioDriver.h"
#include "hal/TouchDriver.h"
#include "hal/SDCard.h"
#include "services/ConfigService.h"
#include "services/TokenService.h"
#include "services/OrchestratorService.h"
#include "services/SerialService.h"
#include "ui/UIStateMachine.h"

/**
 * @class Application
 * @brief Main application coordinator - integrates all subsystems
 *
 * USAGE PATTERN:
 * @code
 * Application app;
 *
 * void setup() {
 *     app.setup();  // Initialize all subsystems
 * }
 *
 * void loop() {
 *     app.loop();   // Process events and coordinate components
 * }
 * @endcode
 *
 * RESPONSIBILITIES:
 * - Initialize all HAL components and services
 * - Coordinate RFID scanning → Orchestrator → UI flow
 * - Manage DEBUG_MODE and boot override logic
 * - Register and route serial commands
 * - Handle touch events via UI state machine
 * - Start FreeRTOS background task for queue management
 *
 * CRITICAL GPIO 3 CONFLICT:
 * GPIO 3 is shared between Serial RX and RFID_SS.
 * - DEBUG_MODE=true: Serial commands active, RFID deferred (send START_SCANNER)
 * - DEBUG_MODE=false: RFID initializes at boot, Serial RX unavailable
 * - Boot override: Send any character within 30s to force DEBUG_MODE
 */
class Application {
public:
    /**
     * @brief Constructor - initializes state variables
     *
     * NOTE: HAL and services are accessed via singletons (getInstance()).
     * UI state machine is created in setup() after HAL initialization.
     *
     * Initialization order matches member declaration order:
     * 1. State primitives (bool, uint32_t)
     * 2. UI pointer (nullptr until setup())
     */
    Application()
        : _debugMode(false)
        , _rfidInitialized(false)
        , _lastRFIDScan(0)
        , _bootOverrideReceived(false)
        , _ui(nullptr)
    {
        LOG_INFO("[APP] Application instance created\n");
    }

    /**
     * @brief Destructor - cleanup UI state machine
     */
    ~Application() {
        if (_ui) {
            delete _ui;
            _ui = nullptr;
        }
    }

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP PUBLIC API - Main Entry Points PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    /**
     * @brief Initialize all subsystems and prepare for operation
     *
     * INITIALIZATION SEQUENCE:
     * 1. Serial communication (115200 baud)
     * 2. Boot override check (30s window for DEBUG_MODE override)
     * 3. Hardware initialization (HAL layer)
     * 4. Service initialization (Config, Token, Orchestrator)
     * 5. Serial command registration
     * 6. FreeRTOS background task startup
     * 7. Initial UI screen (Ready)
     *
     * SOURCE: ALNScanner1021_Orchestrator.ino lines 2615-2925
     *
     * @note This method MUST complete successfully before calling loop()
     */
    void setup();

    /**
     * @brief Main event loop - coordinate all subsystem operations
     *
     * EXECUTION FLOW:
     * 1. Process serial commands (if DEBUG_MODE active)
     * 2. Update audio playback state
     * 3. Update UI state machine (timeouts, screen updates)
     * 4. Handle touch events
     * 5. Process RFID scans (if not blocked by UI)
     * 6. Process serial commands again (responsiveness)
     *
     * SOURCE: ALNScanner1021_Orchestrator.ino lines 3563-3840
     *
     * @note This method should be called repeatedly in Arduino loop()
     */
    void loop();

private:
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP STATE VARIABLES PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    /**
     * Debug mode flag (from config or boot override)
     * - true: Serial commands active, RFID deferred until START_SCANNER
     * - false: RFID initializes at boot, Serial RX unavailable
     * SOURCE: v4.1 line 132, boot override lines 2627-2677
     */
    bool _debugMode;

    /**
     * RFID initialization state
     * - true: RFID reader initialized and ready to scan
     * - false: RFID not initialized (GPIO 3 conflict with Serial RX)
     * SOURCE: v4.1 line 88
     */
    bool _rfidInitialized;

    /**
     * Last RFID scan timestamp (for 500ms rate limiting)
     * Prevents excessive scanning and reduces GPIO 27 beeping
     * SOURCE: v4.1 line 106
     */
    uint32_t _lastRFIDScan;

    /**
     * Boot override flag (30-second window)
     * If any character received during boot, force DEBUG_MODE=true
     * SOURCE: v4.1 lines 2627-2677
     */
    bool _bootOverrideReceived;

    /**
     * UI state machine instance
     * Created in setup() after HAL initialization (needs HAL singleton refs)
     * Manages screen transitions and touch event routing
     */
    ui::UIStateMachine* _ui;

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP INITIALIZATION HELPERS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    /**
     * @brief Handle boot override logic (30-second DEBUG_MODE override window)
     *
     * If any character is received on Serial within 30 seconds of boot,
     * force DEBUG_MODE=true to allow serial commands.
     *
     * This provides emergency access to serial commands even when
     * DEBUG_MODE=false in config.txt.
     *
     * SOURCE: v4.1 lines 2627-2677
     */
    void handleBootOverride();

    /**
     * @brief Initialize early hardware (Display + SD) before config loading
     *
     * INITIALIZATION ORDER:
     * 1. Display Driver (VSPI setup)
     * 2. SD Card (needs Display's VSPI config)
     *
     * This must run BEFORE config loading so we can read config.txt.
     *
     * @return true if successful, false on failure
     */
    bool initializeEarlyHardware();

    /**
     * @brief Initialize late hardware (Touch, Audio, RFID) after config loaded
     *
     * INITIALIZATION ORDER:
     * 3. Touch Driver (interrupt-based, no dependencies)
     * 4. Audio Driver (lazy-initialized)
     * 5. RFID Reader (only if !_debugMode, GPIO 3 conflict)
     *
     * This runs AFTER config is loaded and DEBUG_MODE is correctly set.
     *
     * @return true if successful, false on failure
     */
    bool initializeLateHardware();

    /**
     * @brief Initialize all service layer components
     *
     * INITIALIZATION ORDER:
     * 1. ConfigService (load from SD)
     * 2. TokenService (load database or sync from orchestrator)
     * 3. OrchestratorService (WiFi + orchestrator connection)
     * 4. SerialService (command processing infrastructure)
     *
     * SOURCE: v4.1 lines 2703-2850, 2863-2886
     *
     * @return true if services initialized, false on failure
     */
    bool initializeServices();

    /**
     * @brief Register all serial command handlers
     *
     * Registers handlers for commands like:
     * - CONFIG, STATUS, TOKENS
     * - SET_CONFIG, SAVE_CONFIG
     * - START_SCANNER, SIMULATE_SCAN
     * - QUEUE_TEST, FORCE_UPLOAD, SHOW_QUEUE
     * - REBOOT, HELP
     *
     * Uses SerialService command registry pattern to replace
     * the 468-line if/else chain from v4.1.
     *
     * SOURCE: v4.1 lines 2938-3392
     */
    void registerSerialCommands();

    /**
     * @brief Start FreeRTOS background task on Core 0
     *
     * Starts the background synchronization task that:
     * - Checks orchestrator health every 10 seconds
     * - Uploads queued scans when connection available
     * - Updates connection state
     *
     * SOURCE: v4.1 lines 2679-2683, 2893-2900
     */
    void startBackgroundTasks();

    /**
     * @brief Print ESP32 reset reason for diagnostics
     *
     * Maps all ESP32 reset reason codes to human-readable strings.
     * Critical for debugging crashes and unexpected reboots.
     *
     * SOURCE: v4.1 lines 1275-1293
     */
    void printResetReason();

    /**
     * @brief Print boot banner with version and memory info
     *
     * Displays application version, chip info, and free heap.
     *
     * SOURCE: v4.1 lines 2680-2688
     */
    void printBootBanner();

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP EVENT PROCESSORS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    /**
     * @brief Process RFID card scan events
     *
     * EXECUTION FLOW:
     * 1. Check preconditions (initialized, UI not blocked, rate limit)
     * 2. Scan for RFID card (500ms interval to reduce beeping)
     * 3. Extract token ID (NDEF text or UID hex fallback)
     * 4. Send to orchestrator or queue offline
     * 5. Look up token metadata
     * 6. Display appropriate screen (video modal or regular token)
     *
     * RATE LIMITING:
     * - 500ms minimum between scans (reduces GPIO 27 beeping)
     * - Blocked when UI not in READY state
     *
     * SOURCE: v4.1 lines 3678-3839
     */
    void processRFIDScan();

    /**
     * @brief Process touch events via UI state machine
     *
     * Delegates to UIStateMachine::handleTouch() which:
     * - Applies WiFi EMI filtering
     * - Applies debouncing (50ms)
     * - Routes to state-specific handlers
     * - Manages screen transitions
     *
     * SOURCE: v4.1 lines 3577-3664 (extracted to UIStateMachine)
     */
    void processTouch();

    /**
     * @brief Generate ISO 8601 timestamp for scan records
     *
     * Format: YYYY-MM-DDTHH:MM:SSZ
     * Uses ESP32 time() function (requires NTP or manual time set)
     *
     * SOURCE: v4.1 lines 1649-1669
     *
     * @return ISO 8601 formatted timestamp string
     */
    String generateTimestamp();

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP LIFECYCLE MANAGEMENT PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    // Prevent copying
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
};

// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
// PPP IMPLEMENTATION SECTION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

// ═══════════════════════════════════════════════════════════════════════
// MAIN LOOP - Event Coordination
// ═══════════════════════════════════════════════════════════════════════

/**
 * loop() - Main event loop
 *
 * Extracted from v4.1 lines 3563-3664
 *
 * Flow:
 * 1. Process serial commands (called multiple times for responsiveness)
 * 2. Update audio playback
 * 3. Update UI (timeouts, screen transitions)
 * 4. Process touch events (delegated to UIStateMachine)
 * 5. Process RFID scanning (guarded by state checks)
 *
 * Design notes:
 * - Serial commands are processed multiple times per loop for responsiveness
 * - All touch logic is in UIStateMachine (no duplication here)
 * - RFID scanning is guarded by _rfidInitialized and UI state
 */
inline void Application::loop() {
    // Get singleton references (efficient - static local in getInstance())
    auto& serial = services::SerialService::getInstance();
    auto& audio = hal::AudioDriver::getInstance();

    // Process serial commands (responsive - called multiple times per loop)
    serial.processCommands();

    // Audio playback service
    audio.loop();

    // UI updates (timeouts, screen transitions)
    if (_ui) {
        _ui->update();
    }

    // Process serial commands again (responsiveness)
    serial.processCommands();

    // Touch handling (delegated to UIStateMachine)
    processTouch();

    // RFID scanning (guarded by state checks)
    processRFIDScan();

    // Process serial commands one more time
    serial.processCommands();
}

// ═══════════════════════════════════════════════════════════════════════
// TOUCH HANDLING - Delegation to UI State Machine
// ═══════════════════════════════════════════════════════════════════════

/**
 * processTouch() - Touch event handler
 *
 * Touch events are fully handled by UIStateMachine:
 * - WiFi EMI filtering
 * - Debouncing
 * - Double-tap detection
 * - State-based routing (READY → STATUS, IMAGE → dismiss, STATUS → dismiss)
 *
 * This is a simple delegation method.
 */
inline void Application::processTouch() {
    if (_ui) {
        _ui->handleTouch();
    }
}

// ═══════════════════════════════════════════════════════════════════════
// RFID SCANNING - Card Detection and Token Processing
// ═══════════════════════════════════════════════════════════════════════

/**
 * processRFIDScan() - RFID scanning and token processing
 *
 * Extracted from v4.1 lines 3678-3839
 *
 * Flow:
 * 1. Guard conditions (RFID init, UI blocking, rate limiting)
 * 2. Scan for card (using detectCard + extractNDEFText)
 * 3. Send to orchestrator (or queue if offline)
 * 4. Display appropriate screen (video modal or regular token)
 *
 * Guard conditions:
 * - Skip if RFID not initialized (DEBUG_MODE deferred init)
 * - Skip if UI is blocking (image/status screen displayed)
 * - Rate limit to 500ms between scans (reduces GPIO 27 beeping)
 *
 * Orchestrator routing:
 * - ORCH_CONNECTED: sendScan() → queue on failure
 * - ORCH_WIFI_CONNECTED or ORCH_DISCONNECTED: queue immediately
 *
 * Token display:
 * - Video tokens: showProcessing() with 2.5s auto-dismiss
 * - Regular tokens: showToken() with double-tap dismiss
 * - Unknown tokens: fallback to UID-based paths
 */
inline void Application::processRFIDScan() {
    // ═══ GUARD CONDITIONS ═══════════════════════════════════════════
    // 1. Skip if RFID not initialized (DEBUG_MODE)
    if (!_rfidInitialized) {
        return;
    }

    // 2. Skip if UI is blocking (image/status screen displayed)
    if (_ui && _ui->isBlockingRFID()) {
        return;
    }

    // 3. Rate limiting (500ms between scans)
    if (millis() - _lastRFIDScan < timing::RFID_SCAN_INTERVAL_MS) {
        return;
    }
    _lastRFIDScan = millis();

    // ═══ RFID SCANNING ══════════════════════════════════════════════
    // Get singleton reference
    auto& rfid = hal::RFIDReader::getInstance();

    // Detect card (stores UID in internal state)
    MFRC522::Uid uid;
    if (!rfid.detectCard(uid)) {
        return;  // No card detected
    }

    LOG_INFO("[SCAN] Card detected (UID size: %d)\n", uid.size);

    // Extract token ID (NDEF text or UID hex fallback)
    String tokenId = rfid.extractNDEFText();

    if (tokenId.length() == 0) {
        // No NDEF text - use UID as token ID
        tokenId = "";
        for (byte i = 0; i < uid.size; i++) {
            char hex[3];
            sprintf(hex, "%02x", uid.uidByte[i]);
            tokenId += String(hex);
        }
        LOG_INFO("[SCAN] Using UID as tokenId: %s\n", tokenId.c_str());
    } else {
        LOG_INFO("[SCAN] Using NDEF text as tokenId: %s\n", tokenId.c_str());
    }

    // Halt card (prepare for next scan)
    // Note: v4.1 used SoftSPI_PICC_HaltA() - need to check if RFIDReader has halt()
    rfid.disableRFField();  // Disable RF field as mitigation

    // ═══ ORCHESTRATOR SEND/QUEUE ════════════════════════════════════
    // Get singleton references
    auto& config = services::ConfigService::getInstance();
    auto& orchestrator = services::OrchestratorService::getInstance();

    models::ScanData scan(
        tokenId,
        config.getConfig().teamID,
        config.getConfig().deviceID,
        generateTimestamp()
    );

    // Send to orchestrator or queue if offline
    auto connState = orchestrator.getState();
    if (connState == models::ORCH_CONNECTED) {
        LOG_INFO("[SCAN] Attempting to send to orchestrator...\n");
        if (!orchestrator.sendScan(scan, config.getConfig())) {
            // Failed to send - queue for later
            LOG_INFO("[SCAN] Send failed, queueing\n");
            orchestrator.queueScan(scan);
        } else {
            LOG_INFO("[SCAN] ✓ Sent to orchestrator\n");
        }
    } else {
        // Offline - queue immediately
        LOG_INFO("[SCAN] Offline, queueing immediately\n");
        orchestrator.queueScan(scan);
    }

    // ═══ TOKEN LOOKUP AND DISPLAY ═══════════════════════════════════
    auto& tokens = services::TokenService::getInstance();
    const models::TokenMetadata* token = tokens.get(tokenId);

    if (token) {
        // Known token - check if video or regular
        if (token->isVideoToken()) {
            // Video token - show processing modal (2.5s auto-dismiss)
            LOG_INFO("[SCAN] Video token detected\n");
            _ui->showProcessing(*token);
        } else {
            // Regular token - show image + audio (double-tap to dismiss)
            LOG_INFO("[SCAN] Regular token detected\n");
            _ui->showToken(*token);
        }
    } else {
        // Unknown token - construct fallback metadata from UID
        LOG_INFO("[SCAN] Unknown token - using UID-based fallback\n");

        models::TokenMetadata fallback;
        fallback.tokenId = tokenId;
        fallback.video = "";  // Not a video token
        // Note: image/audio paths auto-constructed from tokenId via getImagePath()/getAudioPath()

        _ui->showToken(fallback);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// TIMESTAMP GENERATION - ISO 8601 Format
// ═══════════════════════════════════════════════════════════════════════

/**
 * generateTimestamp() - Create ISO 8601-ish timestamp
 *
 * Extracted from v4.1 lines 1260-1272
 *
 * Uses millis() since device doesn't have RTC.
 * Format: "1970-01-01THH:MM:SS.mmmZ"
 *
 * Note: This is a placeholder timestamp. The orchestrator should use
 * server-side time for actual event logging.
 */
inline String Application::generateTimestamp() {
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    char timestamp[30];
    snprintf(timestamp, sizeof(timestamp),
        "1970-01-01T%02lu:%02lu:%02lu.%03luZ",
        hours % 24, minutes % 60, seconds % 60, ms % 1000);

    return String(timestamp);
}

// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
// PPP END OF IMPLEMENTATION SECTION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
//
// NOTE: Other method implementations (setup, initializeHardware, etc.)
// will be added by other agents in Phase 5.
//
// Completed implementations:
// ✅ loop() - Main event loop coordination
// ✅ processTouch() - Touch event delegation
// ✅ processRFIDScan() - RFID scan processing flow
// ✅ generateTimestamp() - ISO 8601 timestamp generation
//
// Pending implementations:
// ⏳ setup() - Boot sequence orchestration
// ⏳ initializeHardware() - HAL component initialization
// ⏳ initializeServices() - Service layer initialization
// ⏳ registerSerialCommands() - Command handler registration
// ⏳ startBackgroundTasks() - FreeRTOS task creation
// ⏳ handleBootOverride() - Boot-time DEBUG_MODE override
//
// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

// ═══════════════════════════════════════════════════════════════════════════
// PPP IMPLEMENTATION SECTION - APPLICATION SETUP LOGIC PPPPPPPPPPPPPPPPPPPPPPP
// ═══════════════════════════════════════════════════════════════════════════

#include <esp_system.h>

// ───────────────────────────────────────────────────────────────────────────
// Boot Override Handler - 30-Second DEBUG_MODE Override Window
// ───────────────────────────────────────────────────────────────────────────

inline void Application::handleBootOverride() {
    // If DEBUG_MODE is already true from config, skip the override countdown
    if (_debugMode) {
        Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("     DEBUG MODE ENABLED FROM CONFIG");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("DEBUG_MODE: true (from config.txt)");
        Serial.println("Serial commands are available!");
        Serial.println("RFID initialization deferred - send START_SCANNER to enable");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        delay(1000);
        return;  // Skip the countdown
    }

    // DEBUG_MODE is false - show 30-second override window
    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("     BOOT-TIME DEBUG MODE OVERRIDE");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("Send ANY character within 30 seconds to force DEBUG_MODE");
    Serial.println("(Allows serial commands even if config.txt has DEBUG_MODE=false)");
    Serial.println("");
    Serial.print("Waiting (30s): ");

    unsigned long overrideStart = millis();
    int lastSecond = -1;

    while (millis() - overrideStart < timing::DEBUG_OVERRIDE_TIMEOUT_MS) {
        if (Serial.available()) {
            char received = Serial.read();
            Serial.printf("\n\n✓ Override character received: '%c'\n", received);
            _bootOverrideReceived = true;
            _debugMode = true;
            break;
        }

        // Print countdown every second
        int currentSecond = (millis() - overrideStart) / 1000;
        if (currentSecond != lastSecond) {
            lastSecond = currentSecond;
            Serial.printf("%d ", 30 - currentSecond);
            if ((30 - currentSecond) % 10 == 0 && currentSecond > 0) {
                Serial.println();
                Serial.print("          ");
            }
        }
        delay(100);
    }

    Serial.println("");
    if (_bootOverrideReceived) {
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("   ✓✓✓ DEBUG MODE OVERRIDE ACTIVE ✓✓✓");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("DEBUG_MODE forced to TRUE (ignores config.txt)");
        Serial.println("Serial commands will work!");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    } else {
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("  No override received");
        Serial.println("  Using config.txt DEBUG_MODE setting");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }

    delay(1000); // Brief pause before continuing boot
}

// ───────────────────────────────────────────────────────────────────────────
// Hardware Initialization - Split into Early/Late for Config Loading
// ───────────────────────────────────────────────────────────────────────────

/**
 * Initialize early hardware (Display + SD) needed for config loading
 */
inline bool Application::initializeEarlyHardware() {
    LOG_INFO("[INIT] Initializing early hardware (Display + SD)...\n");

    // Silence DAC pins (prevent beeping from RFID polling)
    pinMode(pins::DAC_SILENCE_1, OUTPUT);
    pinMode(pins::DAC_SILENCE_2, OUTPUT);
    digitalWrite(pins::DAC_SILENCE_1, LOW);
    digitalWrite(pins::DAC_SILENCE_2, LOW);
    LOG_INFO("[INIT] DAC pins silenced\n");

    // Initialize Display FIRST (REQUIRED)
    // CRITICAL: Display must init before SD to prevent VSPI bus conflicts
    auto& display = hal::DisplayDriver::getInstance();
    if (!display.begin()) {
        LOG_ERROR("INIT", "Display initialization failed!");
        return false;
    }
    LOG_INFO("[INIT] ✓ Display initialized\n");

    // Initialize SD Card SECOND (needed for config)
    // CRITICAL: Must come after Display to get final VSPI configuration
    auto& sd = hal::SDCard::getInstance();
    if (!sd.begin()) {
        LOG_ERROR("INIT", "SD card not available - config cannot be loaded!");
        return false;
    }
    LOG_INFO("[INIT] ✓ SD card initialized\n");

    // Show early boot message on display
    display.fillScreen(0x0000);  // Black
    display.getTFT().setTextColor(0xFFE0);  // Yellow
    display.getTFT().setTextSize(2);
    display.getTFT().setCursor(0, 0);
    display.getTFT().println("NeurAI");
    display.getTFT().println("Memory Scanner");
    display.getTFT().println("v5.0 Booting...");

    LOG_INFO("[INIT] Early hardware initialization complete\n");
    return true;
}

/**
 * Initialize late hardware (Touch, Audio, RFID) after config is loaded
 */
inline bool Application::initializeLateHardware() {
    LOG_INFO("[INIT] Initializing late hardware (Touch, Audio, RFID)...\n");

    auto& display = hal::DisplayDriver::getInstance();

    // Initialize Touch Controller (REQUIRED)
    auto& touch = hal::TouchDriver::getInstance();
    if (!touch.begin()) {
        LOG_ERROR("INIT", "Touch initialization failed!");
        return false;
    }
    LOG_INFO("[INIT] ✓ Touch initialized\n");

    // Initialize Audio Driver (lazy-init to prevent boot beeping)
    auto& audio = hal::AudioDriver::getInstance();
    // Audio initialization is deferred until first use
    LOG_INFO("[INIT] ✓ Audio driver ready (deferred init)\n");

    // RFID Initialization (conditional on DEBUG_MODE - now correctly set from config!)
    if (!_debugMode) {
        LOG_INFO("[INIT] Initializing RFID (production mode)...\n");
        display.getTFT().println("RFID: Initializing...");

        auto& rfid = hal::RFIDReader::getInstance();
        if (rfid.begin()) {
            _rfidInitialized = true;
            LOG_INFO("[INIT] ✓ RFID initialized\n");
            display.getTFT().setTextColor(0x07E0);  // Green
            display.getTFT().println("RFID: Ready");
        } else {
            LOG_ERROR("INIT", "RFID initialization failed");
            display.getTFT().setTextColor(0xF800);  // Red
            display.getTFT().println("RFID: FAILED");
        }
    } else {
        LOG_INFO("[INIT] RFID deferred (DEBUG_MODE - use START_SCANNER)\n");
        display.getTFT().setTextColor(0xFD20);  // Orange
        display.getTFT().println("RFID: Debug Mode");
    }

    LOG_INFO("[INIT] Late hardware initialization complete\n");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Service Initialization - Config, Tokens, Orchestrator
// ───────────────────────────────────────────────────────────────────────────

inline bool Application::initializeServices() {
    LOG_INFO("[INIT] Initializing services...\n");

    auto& sd = hal::SDCard::getInstance();
    auto& display = hal::DisplayDriver::getInstance();
    auto& config = services::ConfigService::getInstance();

    if (!sd.isPresent()) {
        LOG_ERROR("INIT", "Services require SD card - degraded mode");
        display.getTFT().setTextColor(0xF800);  // Red
        display.getTFT().println("No SD Card!");
        return false;
    }

    // Config already loaded in setup(), just use it

    // Initialize orchestrator service (WiFi + connection)
    auto& orchestrator = services::OrchestratorService::getInstance();
    orchestrator.initializeWiFi(config.getConfig());

    // Display WiFi status
    if (orchestrator.getState() == models::ORCH_CONNECTED) {
        display.getTFT().setTextColor(0x07E0);  // Green
        display.getTFT().println("Connected!");
    } else {
        display.getTFT().setTextColor(0xFD20);  // Orange
        display.getTFT().println("WiFi: Offline");
    }

    // Initialize token service
    auto& tokens = services::TokenService::getInstance();

    if (config.getConfig().syncTokens && orchestrator.getState() == models::ORCH_CONNECTED) {
        LOG_INFO("[INIT] Syncing tokens from orchestrator...\n");
        display.getTFT().println("Syncing tokens...");

        if (tokens.syncFromOrchestrator(config.getConfig().orchestratorURL)) {
            LOG_INFO("[INIT] ✓ Token sync successful\n");
            display.getTFT().setTextColor(0x07E0);  // Green
            display.getTFT().println("Tokens: Synced");
        } else {
            LOG_ERROR("INIT", "Token sync failed - using cached data");
            display.getTFT().setTextColor(0xFD20);  // Orange
            display.getTFT().println("Tokens: Cached");
        }
    } else {
        LOG_INFO("[INIT] Skipping token sync (disabled or offline)\n");
        display.getTFT().setTextColor(0xFD20);  // Orange
        display.getTFT().println("Tokens: Cached");
    }

    tokens.loadDatabaseFromSD();
    LOG_INFO("[INIT] ✓ Token service initialized (%d tokens)\n", tokens.getCount());

    display.getTFT().setTextColor(0x07E0);  // Green
    display.getTFT().printf("Loaded: %d tokens\n", tokens.getCount());

    // Serial service ready (command registration done separately)
    LOG_INFO("[INIT] ✓ Serial service ready\n");

    LOG_INFO("[INIT] Service initialization complete\n");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Stub Methods - Implemented by Other Agents
// ───────────────────────────────────────────────────────────────────────────

inline void Application::registerSerialCommands() {
    // Get singleton service references
    auto& serial = services::SerialService::getInstance();
    auto& config = services::ConfigService::getInstance();
    auto& tokens = services::TokenService::getInstance();
    auto& orch = services::OrchestratorService::getInstance();
    auto& rfid = hal::RFIDReader::getInstance();

    // Register built-in commands (HELP, REBOOT, MEM)
    serial.registerBuiltinCommands();

    // CONFIG - Show current configuration
    serial.registerCommand("CONFIG", [&config](const String& args) {
        config.getConfig().print();  // Config model has print() method
    }, "Show current device configuration");

    // STATUS / DIAG_NETWORK - Show orchestrator connection status
    serial.registerCommand("STATUS", [&orch, &tokens](const String& args) {
        models::ConnectionState state = orch.getState();
        Serial.println("\n=== Orchestrator Status ===");
        Serial.print("Connection: ");

        switch (state) {
            case models::ConnectionState::ORCH_DISCONNECTED:
                Serial.println("DISCONNECTED");
                break;
            case models::ConnectionState::ORCH_WIFI_CONNECTED:
                Serial.printf("WIFI_CONNECTED (%s)\n", WiFi.localIP().toString().c_str());
                break;
            case models::ConnectionState::ORCH_CONNECTED:
                Serial.printf("CONNECTED (%s + orchestrator)\n", WiFi.localIP().toString().c_str());
                break;
        }

        Serial.printf("Queue size: %d entries\n", orch.getQueueSize());
        Serial.printf("Token database: %d tokens loaded\n", tokens.getCount());
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.println("===========================\n");
    }, "Show orchestrator connection and queue status");

    serial.registerCommand("DIAG_NETWORK", [&orch, &tokens](const String& args) {
        // Alias for STATUS
        models::ConnectionState state = orch.getState();
        Serial.println("\n=== Network Diagnostics ===");
        Serial.print("Connection: ");

        switch (state) {
            case models::ConnectionState::ORCH_DISCONNECTED:
                Serial.println("DISCONNECTED");
                break;
            case models::ConnectionState::ORCH_WIFI_CONNECTED:
                Serial.printf("WIFI_CONNECTED (%s)\n", WiFi.localIP().toString().c_str());
                break;
            case models::ConnectionState::ORCH_CONNECTED:
                Serial.printf("CONNECTED (%s + orchestrator)\n", WiFi.localIP().toString().c_str());
                break;
        }

        Serial.printf("Queue size: %d entries\n", orch.getQueueSize());
        Serial.printf("Token database: %d tokens\n", tokens.getCount());
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.println("===========================\n");
    }, "Alias for STATUS command");

    // TOKENS - Show token database
    serial.registerCommand("TOKENS", [&tokens](const String& args) {
        tokens.printDatabase();
    }, "Show first 10 tokens from database");

    // SET_CONFIG - Update configuration value
    serial.registerCommand("SET_CONFIG", [&config](const String& args) {
        if (args.length() == 0) {
            Serial.println("Usage: SET_CONFIG:KEY=VALUE");
            Serial.println("Example: SET_CONFIG:TEAM_ID=999");
            return;
        }

        String keyValue = args;
        int equalsPos = keyValue.indexOf('=');

        if (equalsPos == -1) {
            Serial.println("Error: Missing '=' in KEY=VALUE");
            return;
        }

        String key = keyValue.substring(0, equalsPos);
        String value = keyValue.substring(equalsPos + 1);

        key.trim();
        value.trim();

        if (config.set(key, value)) {
            Serial.printf("✓ Set %s = %s\n", key.c_str(), value.c_str());
            Serial.println("Use SAVE_CONFIG to persist changes");
        } else {
            Serial.printf("✗ Failed to set %s\n", key.c_str());
        }
    }, "Update config value (use SAVE_CONFIG to persist)");

    // SAVE_CONFIG - Persist configuration to SD card
    serial.registerCommand("SAVE_CONFIG", [&config](const String& args) {
        if (config.saveToSD()) {
            Serial.println("✓ Configuration saved to SD card");
            Serial.println("Changes will persist after reboot");
        } else {
            Serial.println("✗ Failed to save configuration");
        }
    }, "Save current config to /config.txt");

    // START_SCANNER - Initialize RFID in DEBUG_MODE
    serial.registerCommand("START_SCANNER", [this, &rfid](const String& args) {
        if (!_debugMode) {
            Serial.println("Error: START_SCANNER only works in DEBUG_MODE");
            return;
        }

        if (_rfidInitialized) {
            Serial.println("Warning: RFID already initialized");
            return;
        }

        Serial.println("\n⚠ WARNING: Initializing RFID will disable Serial RX!");
        Serial.println("You will NOT be able to send commands after this.");
        Serial.flush();
        delay(100);

        if (rfid.begin()) {  // RFIDReader uses begin() not initialize()
            _rfidInitialized = true;
            Serial.println("✓ RFID initialized successfully");
            Serial.println("⚡ Serial RX now disabled (GPIO 3 conflict)");
        } else {
            Serial.println("✗ RFID initialization failed");
        }
    }, "Initialize RFID (DEBUG_MODE only, kills serial RX)");

    // SIMULATE_SCAN - Simulate token processing without hardware
    serial.registerCommand("SIMULATE_SCAN", [this, &tokens, &orch, &config](const String& args) {
        if (args.length() == 0) {
            Serial.println("\n✗ ERROR: No tokenId provided");
            Serial.println("Usage: SIMULATE_SCAN:tokenId");
            Serial.println("Example: SIMULATE_SCAN:kaa001");
            return;
        }

        String tokenId = args;
        tokenId.trim();

        Serial.println("\n═══════════════════════════════════════════════");
        Serial.println("     SIMULATE TOKEN SCAN (NO RFID HARDWARE)");
        Serial.println("═══════════════════════════════════════════════");
        Serial.printf("Token ID: %s\n", tokenId.c_str());

        // Create scan data (use same timestamp format as real scans)
        models::ScanData scan;
        scan.tokenId = tokenId;
        scan.teamId = config.getConfig().teamID;
        scan.deviceId = config.getConfig().deviceID;
        scan.timestamp = generateTimestamp();

        // Send or queue (same as real scan)
        if (orch.getState() == models::ConnectionState::ORCH_CONNECTED) {
            Serial.println("Sending to orchestrator...");
            if (orch.sendScan(scan, config.getConfig())) {
                Serial.println("✓ Sent successfully");
            } else {
                Serial.println("Send failed, queuing...");
                orch.queueScan(scan);
            }
        } else {
            Serial.println("Offline, queuing...");
            orch.queueScan(scan);
        }

        // Display token info (UI would show this)
        const auto* token = tokens.get(tokenId);
        if (token) {
            Serial.printf("Token found: %s\n", token->isVideoToken() ? "VIDEO" : "REGULAR");
            Serial.printf("  Image: %s\n", token->getImagePath().c_str());
            if (!token->isVideoToken()) {
                Serial.printf("  Audio: %s\n", token->getAudioPath().c_str());
            }
        } else {
            Serial.println("Token not in database (would use UID fallback)");
        }

        Serial.println("✓ Simulation complete");
        Serial.println("═══════════════════════════════════════════════\n");
    }, "Simulate token scan for testing (no hardware)");

    // QUEUE_TEST - Add mock scans to queue
    serial.registerCommand("QUEUE_TEST", [&orch, &config](const String& args) {
        Serial.println("\n=== Queue Test ===");
        Serial.println("Adding 20 mock scans to queue...");
        Serial.printf("Queue size before: %d\n", orch.getQueueSize());

        for (int i = 1; i <= 20; i++) {
            models::ScanData mockScan;
            mockScan.tokenId = String("TEST") + String(i, 16);
            mockScan.teamId = config.getConfig().teamID;
            mockScan.deviceId = config.getConfig().deviceID;
            mockScan.timestamp = String(millis());

            orch.queueScan(mockScan);  // queueScan takes only ScanData
        }

        Serial.printf("Queue size after: %d\n", orch.getQueueSize());
        Serial.println("✓ 20 mock scans added");
        Serial.println("==================\n");
    }, "Add 20 mock scans to queue for testing");

    // FORCE_UPLOAD - Manually trigger queue batch upload
    serial.registerCommand("FORCE_UPLOAD", [&orch, &config](const String& args) {
        Serial.println("\n=== Manual Queue Upload ===");
        Serial.printf("Queue size: %d\n", orch.getQueueSize());

        if (orch.getQueueSize() == 0) {
            Serial.println("Queue is empty, nothing to upload");
            Serial.println("===========================\n");
            return;
        }

        Serial.println("Attempting batch upload...");

        if (orch.uploadQueueBatch(config.getConfig())) {
            Serial.printf("✓ Upload successful, queue size now: %d\n", orch.getQueueSize());
        } else {
            Serial.println("✗ Upload failed (check connection)");
        }

        Serial.println("===========================\n");
    }, "Force immediate queue batch upload");

    // SHOW_QUEUE - Display queue contents
    serial.registerCommand("SHOW_QUEUE", [&orch](const String& args) {
        orch.printQueue();
    }, "Show first 10 queued scans");

    // FORCE_OVERFLOW - Test FIFO overflow protection
    serial.registerCommand("FORCE_OVERFLOW", [&orch, &config](const String& args) {
        Serial.println("\n=== FIFO Overflow Test ===");
        Serial.println("Adding 105 mock scans (max=100)...");
        Serial.printf("Queue size before: %d\n", orch.getQueueSize());

        for (int i = 1; i <= 105; i++) {
            models::ScanData mockScan;
            mockScan.tokenId = String("OVERFLOW") + String(i);
            mockScan.teamId = config.getConfig().teamID;
            mockScan.deviceId = config.getConfig().deviceID;
            mockScan.timestamp = String(millis());

            orch.queueScan(mockScan);  // queueScan takes only ScanData
        }

        Serial.printf("Queue size after: %d (should be capped at 100)\n", orch.getQueueSize());

        if (orch.getQueueSize() <= queue_config::MAX_QUEUE_SIZE) {
            Serial.println("✓ FIFO overflow protection working");
        } else {
            Serial.println("✗ Warning: Queue exceeded maximum size!");
        }

        Serial.println("===========================\n");
    }, "Test FIFO overflow (adds 105 entries)");

    LOG_INFO("[INIT] ✓ Serial commands registered (%d commands)\n", 14);
}

inline void Application::startBackgroundTasks() {
    // Get singleton service references
    auto& orch = services::OrchestratorService::getInstance();
    auto& config = services::ConfigService::getInstance();

    // Start FreeRTOS background sync task on Core 0
    // (main loop runs on Core 1)
    orch.startBackgroundTask(config.getConfig());

    LOG_INFO("[INIT] ✓ Background queue sync task started on Core 0\n");
}

// NOTE: processRFIDScan(), processTouch(), and loop() are already implemented above
// (lines 346-570). See earlier in this file.

// ───────────────────────────────────────────────────────────────────────────
// Utility Helpers
// ───────────────────────────────────────────────────────────────────────────

inline void Application::printResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.print("[BOOT] Reset reason: ");

    switch (reason) {
        case ESP_RST_UNKNOWN:     Serial.println("ESP_RST_UNKNOWN (indeterminate)"); break;
        case ESP_RST_POWERON:     Serial.println("ESP_RST_POWERON (normal power-on)"); break;
        case ESP_RST_EXT:         Serial.println("ESP_RST_EXT (external pin reset)"); break;
        case ESP_RST_SW:          Serial.println("ESP_RST_SW (software reset via esp_restart)"); break;
        case ESP_RST_PANIC:       Serial.println("ESP_RST_PANIC (exception/panic - CRASH!)"); break;
        case ESP_RST_INT_WDT:     Serial.println("ESP_RST_INT_WDT (interrupt watchdog - CODE HUNG!)"); break;
        case ESP_RST_TASK_WDT:    Serial.println("ESP_RST_TASK_WDT (task watchdog - TASK HUNG!)"); break;
        case ESP_RST_WDT:         Serial.println("ESP_RST_WDT (other watchdog - CODE HUNG!)"); break;
        case ESP_RST_DEEPSLEEP:   Serial.println("ESP_RST_DEEPSLEEP (wake from deep sleep)"); break;
        case ESP_RST_BROWNOUT:    Serial.println("ESP_RST_BROWNOUT (brownout reset - POWER ISSUE!)"); break;
        case ESP_RST_SDIO:        Serial.println("ESP_RST_SDIO (SDIO reset)"); break;
        default:                  Serial.printf("UNKNOWN (%d)\n", reason); break;
    }
}

inline void Application::printBootBanner() {
    Serial.println("\n━━━ ALNScanner v5.0 (OOP Architecture) ━━━");
    Serial.println("Refactored: Full HAL + Service Layer");
    Serial.printf("[BOOT] Free heap at start: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] Chip model: ESP32, %d cores, %d MHz\n",
                  ESP.getChipCores(), ESP.getCpuFreqMHz());
}

// NOTE: generateTimestamp() is already implemented above (line 549)

// ═══════════════════════════════════════════════════════════════════════════
// PPP MAIN SETUP METHOD PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
// ═══════════════════════════════════════════════════════════════════════════

inline void Application::setup() {
    // LED diagnostic BEFORE serial - prove we're running
    pinMode(2, OUTPUT);
    for(int i = 0; i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(50);
        digitalWrite(2, LOW);
        delay(50);
    }

    Serial.begin(115200);

    // Initialize SerialService (FIX: Enable command processing)
    auto& serial = services::SerialService::getInstance();
    serial.begin();

    // 1. Print boot banner and reset reason
    printBootBanner();
    printResetReason();

    // 2. Initialize Display and SD card FIRST (needed for config)
    if (!initializeEarlyHardware()) {
        LOG_ERROR("SETUP", "Early hardware initialization failed");
        return;
    }

    // 3. Load configuration from SD card
    auto& config = services::ConfigService::getInstance();
    if (!config.loadFromSD()) {
        LOG_ERROR("SETUP", "Failed to load configuration - using defaults");
    } else {
        LOG_INFO("[INIT] ✓ Configuration loaded\n");
        // Set DEBUG_MODE from config (can be overridden by boot sequence)
        _debugMode = config.getConfig().debugMode;
    }

    // 4. Boot override handling (30-second window, can force DEBUG_MODE=true)
    handleBootOverride();

    // 5. Initialize remaining hardware (RFID decision based on correct DEBUG_MODE)
    if (!initializeLateHardware()) {
        LOG_ERROR("SETUP", "Late hardware initialization failed");
        return;
    }

    // 6. Initialize services (tokens, orchestrator)
    if (!initializeServices()) {
        LOG_ERROR("SETUP", "Service initialization failed");
        // Continue in degraded mode
    }

    // 5. Register serial commands
    registerSerialCommands();

    // 6. Start background tasks (Core 0)
    startBackgroundTasks();

    // 7. Create UI state machine with HAL references
    auto& display = hal::DisplayDriver::getInstance();
    auto& touch = hal::TouchDriver::getInstance();
    auto& audio = hal::AudioDriver::getInstance();
    auto& sd = hal::SDCard::getInstance();

    _ui = new ui::UIStateMachine(display, touch, audio, sd);
    _ui->showReady(_rfidInitialized, _debugMode);

    Serial.println("\n━━━ Setup Complete ━━━");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[SETUP] ✓✓✓ Boot complete ✓✓✓\n");
}
