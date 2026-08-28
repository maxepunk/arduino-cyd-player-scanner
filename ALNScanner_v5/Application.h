#pragma once

/**
 * @file Application.h
 * @brief Main application orchestrator for the ghost scanner
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
     * If any character is received on Serial within the override window,
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
     * 2. TokenService (load database from SD)
     * 3. SerialService (command processing infrastructure)
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
     * 2. Scan for RFID card (500ms interval to reduce beeping); distinguish
     *    NoCard vs CommFailed via DetectResult enum
     * 3. Extract token ID from NDEF. On failure, show non-blocking
     *    SCAN_FAILED screen and do NOT send to orchestrator.
     * 4. Look up token metadata in local DB BEFORE sending to orchestrator.
     *    Unknown tokens show SCAN_FAILED and are not uploaded — the
     *    orchestrator only ever sees real game tokenIds.
     * 5. Send known tokens to orchestrator or queue offline.
     * 6. Display appropriate screen (video modal or regular token).
     *
     * RATE LIMITING:
     * - 500ms minimum between scans (reduces GPIO 27 beeping)
     * - Blocked when UI not in READY state
     *
     * SOURCE: v4.1 lines 3678-3839
     */
    void processRFIDScan();

    /**
     * @brief Decide what a known ghost can present, and show it
     * @param token Token already confirmed present in the database
     *
     * Shared by processRFIDScan() and the SIMULATE_SCAN serial command so
     * the two cannot drift.
     */
    void presentGhost(const models::TokenMetadata& token);

    /**
     * @brief Draw a full-screen setup-error notice (not in-world)
     * @param display Display driver
     * @param message Short message, kept under ~13 chars to fit at size 3
     */
    static void showFatalError(hal::DisplayDriver& display, const char* message);

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
 * Flow:
 * 1. Guard conditions (RFID init, UI blocking, rate limiting).
 * 2. Detect card via DetectResult enum (NoCard/Detected/CommFailed).
 * 3. Extract tokenId from NDEF. On failure -> showScanFailed(), no
 *    orchestrator send, no UID fallback.
 * 4. Look up token in local DB before sending to orchestrator. Unknown
 *    tokens are reported to the user but NOT uploaded — the old UID-hex
 *    fallback is removed so that the orchestrator only ever sees real
 *    game tokenIds.
 * 5. Display token (video modal or regular) only for known, valid tokens.
 *
 * Failure routing (all via _ui->showScanFailed, which is non-blocking):
 *   CommFailed  -> "COMM FAILED"    (detect retries exhausted)
 *   NDEF empty  -> "READ FAILED"    (NDEF extraction retries exhausted)
 *   Unknown ID  -> "UNKNOWN TOKEN"  (NDEF OK, but tokenId not in DB)
 *
 * The SCAN_FAILED UI state does NOT block RFID scanning, so the player
 * can immediately re-tap a token after a transient failure without
 * waiting for the screen to dismiss.
 */
inline void Application::processRFIDScan() {
    // ═══ GUARD CONDITIONS ═══════════════════════════════════════════
    if (!_rfidInitialized) {
        return;
    }

    if (_ui && _ui->isBlockingRFID()) {
        return;
    }

    if (millis() - _lastRFIDScan < timing::RFID_SCAN_INTERVAL_MS) {
        return;
    }
    _lastRFIDScan = millis();

    // ═══ RFID DETECTION ═════════════════════════════════════════════
    auto& rfid = hal::RFIDReader::getInstance();

    MFRC522::Uid uid;
    hal::DetectResult det = rfid.detectCard(uid);

    if (det == hal::DetectResult::NoCard) {
        return;  // Normal idle — no card in field
    }

    if (det == hal::DetectResult::CommFailed) {
        LOG_INFO("[SCAN-FAIL] Card detect comm failure\n");
        if (_ui) {
            _ui->showScanFailed("COMM FAILED");
        }
        return;
    }

    // det == Detected
    LOG_INFO("[SCAN] Card detected (UID size: %d)\n", uid.size);

    // ═══ NDEF EXTRACTION ════════════════════════════════════════════
    // extractNDEFText() handles its own retries and reSelect recovery,
    // and internally disables the RF field on return (success or failure).
    String tokenId = rfid.extractNDEFText();

    if (tokenId.length() == 0) {
        LOG_INFO("[SCAN-FAIL] NDEF extraction failed after retries\n");
        if (_ui) {
            _ui->showScanFailed("READ FAILED");
        }
        return;
    }

    LOG_INFO("[SCAN] NDEF tokenId: %s\n", tokenId.c_str());

    // ═══ TOKEN DB VALIDATION (gate orchestrator send) ═══════════════
    // Look up the token in the local database BEFORE reporting to the
    // orchestrator. Unknown tokenIds are treated as scan failures —
    // they must not pollute session data with unrecognized entries.
    auto& tokens = services::TokenService::getInstance();
    const models::TokenMetadata* token = tokens.get(tokenId);

    if (!token) {
        LOG_INFO("[SCAN-FAIL] Unknown tokenId '%s' (not in DB)\n", tokenId.c_str());
        if (_ui) {
            _ui->showScanFailed("UNKNOWN TOKEN");
        }
        return;
    }

    // Presentation is shared with SIMULATE_SCAN so the two paths cannot
    // drift — the same reason applyScanOutcome() existed on main.
    presentGhost(*token);
}

// ───────────────────────────────────────────────────────────────────────────
// Ghost Presentation - shared by the RFID path and SIMULATE_SCAN
// ───────────────────────────────────────────────────────────────────────────

/**
 * presentGhost() - decide what a known ghost can present, and show it.
 *
 * Works out what is actually on the card BEFORE entering the display
 * screen: an image if one exists, audio if one exists. Checking up front
 * matters because DisplayDriver::drawBMP() paints its own
 * "Missing: <path>" error to the TFT when a file is absent, which a guest
 * must never see.
 *
 * A known ghost with neither image nor audio is a card SETUP ERROR, not an
 * unrecognised tag. The guest sees the same in-world copy either way; the
 * serial lines below are what tell whoever built the card which it was.
 */
inline void Application::presentGhost(const models::TokenMetadata& token) {
    auto& sd = hal::SDCard::getInstance();
    const bool hasImage = sd.exists(token.getImagePath());
    const bool hasAudio = sd.exists(token.getAudioPath());

    if (!hasImage && !hasAudio) {
        LOG_ERROR("SCAN-FAIL", "Known token has neither image nor audio on SD");
        Serial.printf("        Expected image: %s\n", token.getImagePath().c_str());
        Serial.printf("        Expected audio: %s\n", token.getAudioPath().c_str());
        if (_ui) {
            _ui->showScanFailed("NO ASSETS");
        }
        return;
    }

    LOG_INFO("[SCAN] Presenting ghost %s (image=%s, audio=%s)\n",
             token.tokenId.c_str(), hasImage ? "yes" : "no", hasAudio ? "yes" : "no");
    if (_ui) {
        _ui->showToken(token, hasImage, hasAudio);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PPP IMPLEMENTATION SECTION - APPLICATION SETUP LOGIC PPPPPPPPPPPPPPPPPPPPPPP
// ═══════════════════════════════════════════════════════════════════════════

#include <esp_system.h>

// ───────────────────────────────────────────────────────────────────────────
// Boot Override Handler - DEBUG_MODE Override Window
// ───────────────────────────────────────────────────────────────────────────

inline void Application::handleBootOverride() {
    // Paint the home screen FIRST, so the prop looks alive immediately and a
    // guest never sees a blank or branded boot screen. Safe here: the
    // display is fully initialized by initializeEarlyHardware(), which runs
    // before this. RFID is not up yet, hence rfidReady=false.
    {
        ui::GhostReadyScreen bootScreen(false);
        bootScreen.render(hal::DisplayDriver::getInstance());
    }

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
    Serial.printf("Send ANY character within %lu seconds to force DEBUG_MODE\n",
                  (unsigned long)(timing::DEBUG_OVERRIDE_TIMEOUT_MS / 1000));
    Serial.println("(Allows serial commands even if config.txt has DEBUG_MODE=false)");
    Serial.println("");
    Serial.printf("Waiting (%lus): ",
                  (unsigned long)(timing::DEBUG_OVERRIDE_TIMEOUT_MS / 1000));

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
        // NOT fatal. On main this returned false, setup() bailed, and the
        // screen stayed blank — so the "No SD Card" message in
        // initializeServices() was unreachable, and the one failure an
        // operator most needs to see was the one that showed nothing.
        // The display works; carry on far enough to say so.
        LOG_ERROR("INIT", "SD card not available - continuing to report it on screen");
    }
    LOG_INFO("[INIT] ✓ SD card initialized\n");

    // No boot splash. handleBootOverride() paints the home screen a moment
    // from now; anything drawn here would only be overwritten, and on main
    // this spot showed ALN branding for the whole override window.
    display.fillScreen(0x0000);  // Black

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

    // Audio: real init is deferred until first playback (prevents boot
    // beeping), but the configured gain is handed over now so it is applied
    // the moment the output is created.
    auto& audio = hal::AudioDriver::getInstance();
    audio.setVolume(services::ConfigService::getInstance().getConfig().volume);
    LOG_INFO("[INIT] ✓ Audio driver ready (deferred init)\n");

    // RFID Initialization (conditional on DEBUG_MODE - now correctly set from config!)
    if (!_debugMode) {
        LOG_INFO("[INIT] Initializing RFID (production mode)...\n");

        auto& rfid = hal::RFIDReader::getInstance();
        if (rfid.begin()) {
            _rfidInitialized = true;
            LOG_INFO("[INIT] ✓ RFID initialized\n");
        } else {
            LOG_ERROR("INIT", "RFID initialization failed");
        }
    } else {
        LOG_INFO("[INIT] RFID deferred (DEBUG_MODE - use START_SCANNER)\n");
    }

    LOG_INFO("[INIT] Late hardware initialization complete\n");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Service Initialization - Config, Tokens, Orchestrator
// ───────────────────────────────────────────────────────────────────────────

/**
 * showFatalError() - full-screen, legible failure notice
 *
 * For conditions where the prop cannot function at all. Unlike the in-world
 * SCAN_FAILED screen this is aimed at whoever is setting the prop up, so it
 * says plainly what is wrong rather than staying in character.
 */
inline void Application::showFatalError(hal::DisplayDriver& display, const char* message) {
    auto& tft = display.getTFT();
    const int16_t screenW = 240;

    tft.fillScreen(TFT_BLACK);

    tft.setTextSize(3);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    int16_t w = (int16_t)(6 * 3 * strlen(message));
    tft.setCursor((screenW - w) / 2, 130);
    tft.print(message);

    tft.setTextSize(1);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    const char* hint = "Insert a prepared card and restart";
    w = (int16_t)(6 * strlen(hint));
    tft.setCursor((screenW - w) / 2, 180);
    tft.print(hint);
}

inline bool Application::initializeServices() {
    LOG_INFO("[INIT] Initializing services...\n");

    auto& sd = hal::SDCard::getInstance();
    auto& display = hal::DisplayDriver::getInstance();

    if (!sd.isPresent()) {
        // A prop with no card cannot present anything. This is the one
        // boot error a guest might legitimately see, and it is correct
        // that they do: the device is broken until the card is fixed.
        LOG_ERROR("INIT", "Services require SD card - degraded mode");
        showFatalError(display, "NO SD CARD");
        return false;
    }

    // Token database. Config is already loaded by setup(). There is no
    // orchestrator on this build, so the card is the sole source of
    // truth — no sync, no queue, no network.
    auto& tokens = services::TokenService::getInstance();
    tokens.loadDatabaseFromSD();
    LOG_INFO("[INIT] ✓ Token service initialized (%d ghosts)\n", tokens.getCount());

    // Deliberately no TFT output here. The home screen is painted before
    // the boot-override window, and boot status text would scribble over
    // it in front of a guest.
    LOG_INFO("[INIT] ✓ Serial service ready\n");
    LOG_INFO("[INIT] Service initialization complete\n");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Stub Methods - Implemented by Other Agents
// ───────────────────────────────────────────────────────────────────────────

inline void Application::registerSerialCommands() {
    auto& serial = services::SerialService::getInstance();
    auto& config = services::ConfigService::getInstance();
    auto& tokens = services::TokenService::getInstance();
    auto& rfid = hal::RFIDReader::getInstance();

    // Built-ins: HELP, REBOOT, MEM
    serial.registerBuiltinCommands();

    serial.registerCommand("CONFIG", [&config](const String& args) {
        config.getConfig().print();
    }, "Show current device configuration");

    serial.registerCommand("STATUS", [this, &tokens, &config](const String& args) {
        Serial.println("\n=== Ghost Scanner Status ===");
        Serial.printf("Device ID:  %s\n", config.getConfig().deviceID.c_str());
        Serial.printf("Ghosts:     %d in database\n", tokens.getCount());
        Serial.printf("RFID:       %s\n", _rfidInitialized ? "ready" : "not initialized");
        Serial.printf("Debug mode: %s\n", _debugMode ? "yes" : "no");
        Serial.printf("SD card:    %s\n",
                      hal::SDCard::getInstance().isPresent() ? "present" : "ABSENT");
        Serial.printf("Free heap:  %d bytes\n", ESP.getFreeHeap());
        Serial.println("============================\n");
    }, "Show device status");

    serial.registerCommand("TOKENS", [&tokens](const String& args) {
        tokens.printDatabase();
    }, "Show first 10 ghosts from database");

    serial.registerCommand("SET_CONFIG", [&config](const String& args) {
        if (args.length() == 0) {
            Serial.println("Usage: SET_CONFIG:KEY=VALUE");
            Serial.println("Example: SET_CONFIG:VOLUME=1.5");
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

    serial.registerCommand("SAVE_CONFIG", [&config](const String& args) {
        if (config.saveToSD()) {
            Serial.println("✓ Configuration saved to SD card");
            Serial.println("Changes will persist after reboot");
        } else {
            Serial.println("✗ Failed to save configuration");
        }
    }, "Save current config to /config.txt");

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

        if (rfid.begin()) {
            _rfidInitialized = true;
            Serial.println("✓ RFID initialized successfully");
            Serial.println("⚡ Serial RX now disabled (GPIO 3 conflict)");
        } else {
            Serial.println("✗ RFID initialization failed");
        }
    }, "Initialize RFID (DEBUG_MODE only, kills serial RX)");

    // SIMULATE_SCAN - exercise the full presentation path without a tag.
    // Unlike main, this DOES drive the real display: with no orchestrator
    // there is no network step to stub out, so what you see here is what a
    // real tap produces.
    serial.registerCommand("SIMULATE_SCAN", [this, &tokens](const String& args) {
        if (args.length() == 0) {
            Serial.println("Usage: SIMULATE_SCAN:tokenId");
            Serial.println("Example: SIMULATE_SCAN:ghost01");
            return;
        }

        String tokenId = args;
        tokenId.trim();

        const models::TokenMetadata* token = tokens.get(tokenId);
        if (!token) {
            Serial.printf("Unknown ghost '%s' - not in database\n", tokenId.c_str());
            Serial.println("A real tap would show the failure screen.");
            if (_ui) {
                _ui->showScanFailed("UNKNOWN TOKEN");
            }
            return;
        }

        Serial.printf("Simulating scan of '%s'\n", tokenId.c_str());
        presentGhost(*token);
    }, "Present a ghost by id, exactly as a real tap would");

    serial.registerCommand("SIMULATE_FAIL", [this](const String& args) {
        String reason = args;
        reason.trim();
        if (reason.length() == 0) {
            reason = "READ FAILED";
        }

        Serial.printf("Showing failure screen (reason logged as '%s')\n", reason.c_str());
        if (_ui) {
            _ui->showScanFailed(reason);
        }
    }, "Show the failure screen (UI verification)");

    LOG_INFO("[INIT] ✓ Serial commands registered (%d commands)\n", 9);
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
    Serial.println("\n━━━ Ghost Scanner (standalone) ━━━");
    Serial.println("Refactored: Full HAL + Service Layer");
    Serial.printf("[BOOT] Free heap at start: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] Chip model: ESP32, %d cores, %d MHz\n",
                  ESP.getChipCores(), ESP.getCpuFreqMHz());
}


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

    // 4. Boot override handling (10-second window, can force DEBUG_MODE=true).
    //    Paints the home screen first so a guest never sees a blank or
    //    branded boot screen while the window runs.
    handleBootOverride();

    // 5. Initialize remaining hardware (RFID decision based on correct DEBUG_MODE)
    if (!initializeLateHardware()) {
        LOG_ERROR("SETUP", "Late hardware initialization failed");
        return;
    }

    // 6. Initialize services (token database)
    if (!initializeServices()) {
        LOG_ERROR("SETUP", "Service initialization failed");
        // Continue in degraded mode
    }

    // 5. Register serial commands
    registerSerialCommands();

    // 7. Create UI state machine with HAL references
    auto& display = hal::DisplayDriver::getInstance();
    auto& touch = hal::TouchDriver::getInstance();
    auto& audio = hal::AudioDriver::getInstance();
    auto& sd = hal::SDCard::getInstance();

    _ui = new ui::UIStateMachine(display, touch, audio, sd);

    // Wire status provider so the hidden long-press screen shows real data
    _ui->setStatusProvider([this]() -> ui::StatusScreen::SystemStatus {
        auto& config = services::ConfigService::getInstance();
        auto& tokens = services::TokenService::getInstance();

        ui::StatusScreen::SystemStatus status;
        status.deviceID   = config.getConfig().deviceID;
        status.ghostCount = tokens.getCount();
        status.rfidReady  = _rfidInitialized;
        status.sdPresent  = hal::SDCard::getInstance().isPresent();
        status.volume     = config.getConfig().volume;
        status.freeHeap   = ESP.getFreeHeap();
        return status;
    });

    if (hal::SDCard::getInstance().isPresent()) {
        _ui->showReady(_rfidInitialized, _debugMode);
    } else {
        // Leave the NO SD CARD notice up. A "place ghost here" screen would
        // claim the prop is working when it cannot read a single ghost.
        LOG_ERROR("SETUP", "No SD card - leaving error notice on screen");
    }

    Serial.println("\n━━━ Setup Complete ━━━");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[SETUP] ✓✓✓ Boot complete ✓✓✓\n");
}
