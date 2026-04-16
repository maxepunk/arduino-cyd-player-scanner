#pragma once

#include "Screen.h"
#include "screens/ReadyScreen.h"
#include "screens/StatusScreen.h"
#include "screens/TokenDisplayScreen.h"
#include "screens/ProcessingScreen.h"
#include "screens/ScanFailedScreen.h"
#include "../hal/DisplayDriver.h"
#include "../hal/TouchDriver.h"
#include "../hal/AudioDriver.h"
#include "../hal/SDCard.h"
#include "../models/Token.h"
#include "../models/ConnectionState.h"
#include "../config.h"
#include <memory>
#include <functional>

// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
// PPP UI STATE MACHINE - Screen Transition & Touch Event Management PPPP
// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
//
// Manages all screen transitions and touch event routing for ALNScanner.
//
// STATE MODEL:
//   
//    READY (idle)                                                   
//        single-tap > SHOWING_STATUS                            
//        RFID scan   > DISPLAYING_TOKEN or PROCESSING_VIDEO     
//  $
//    SHOWING_STATUS (diagnostics screen)                            
//        any tap     > READY                                    
//  $
//    DISPLAYING_TOKEN (regular token with audio)                    
//        double-tap  > READY                                    
//  $
//    PROCESSING_VIDEO (video token modal)                           
//        2.5s auto   > READY (no touch interaction)             
//   
//
// TOUCH HANDLING:
// - WiFi EMI filtering via TouchDriver (pulse width threshold)
// - Debouncing (50ms)
// - Double-tap detection (500ms window)
// - State-specific touch routing
//
// EXTRACTED FROM: ALNScanner1021_Orchestrator v4.1
// - Touch logic: lines 3577-3664
// - State variables: lines 90, 98-104, 148
// - Screen rendering: lines 2238-2362 (status), 2326-2362 (ready),
//                     2179-2235 (processing), 3511-3559 (token)
//
// PATTERN: State Machine + Strategy (polymorphic screens)
//
// PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace ui {

class UIStateMachine {
public:
    // Callback to get real system status from Application layer
    using StatusProvider = std::function<StatusScreen::SystemStatus()>;

    // UI State enum (from v4.1 implicit states)
    enum class State {
        READY,              // Ready screen (idle, waiting for scan or tap)
        SHOWING_STATUS,     // Status/diagnostics screen
        DISPLAYING_TOKEN,   // Token display with audio (regular token)
        PROCESSING_VIDEO,   // Processing modal (video token, auto-hide)
        SCAN_FAILED         // Transient failure screen (non-blocking, auto-hide)
    };

    // Constructor with HAL dependency injection
    UIStateMachine(hal::DisplayDriver& display,
                   hal::TouchDriver& touch,
                   hal::AudioDriver& audio,
                   hal::SDCard& sd)
        : _display(display)
        , _touch(touch)
        , _audio(audio)
        , _sd(sd)
        , _state(State::READY)
        , _currentScreen(nullptr)
        , _tokenScreenPtr(nullptr)
        , _lastTouchTime(0)
        , _lastTouchWasValid(false)
        , _lastTouchDebounce(0)
        , _processingStartTime(0)
        , _scanFailedStartTime(0)
        , _rfidReady(false)
        , _debugMode(false)
    {
        LOG_INFO("[UI-STATE] UIStateMachine initialized\n");
    }

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP STATE TRANSITIONS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    // Transition to READY state
    // Source: drawReadyScreen() lines 2326-2362
    void showReady(bool rfidReady, bool debugMode) {
        _rfidReady = rfidReady;
        _debugMode = debugMode;

        LOG_INFO("[UI-STATE] Transitioning to READY (RFID: %s, Debug: %s)\n",
                 rfidReady ? "ready" : "disabled",
                 debugMode ? "ON" : "OFF");

        // Create ready screen with current RFID state
        auto screen = std::unique_ptr<ReadyScreen>(
            new ReadyScreen(rfidReady, debugMode)
        );

        // Transition and render
        transitionTo(State::READY, std::move(screen));

        // Reset touch state
        _lastTouchWasValid = false;
        _lastTouchTime = 0;
    }

    // Transition to SHOWING_STATUS state
    // Source: displayStatusScreen() lines 2238-2315
    void showStatus(const StatusScreen::SystemStatus& status) {
        LOG_INFO("[UI-STATE] Transitioning to SHOWING_STATUS\n");

        // Create status screen with current system state
        auto screen = std::unique_ptr<StatusScreen>(
            new StatusScreen(status)
        );

        // Transition and render
        transitionTo(State::SHOWING_STATUS, std::move(screen));
    }

    // Transition to DISPLAYING_TOKEN state
    // Source: processTokenScan() lines 3511-3559
    void showToken(const models::TokenMetadata& token) {
        LOG_INFO("[UI-STATE] Transitioning to DISPLAYING_TOKEN (token: %s)\n",
                 token.tokenId.c_str());

        // Create token display screen
        auto* tokenScreen = new TokenDisplayScreen(token);
        auto screen = std::unique_ptr<TokenDisplayScreen>(tokenScreen);

        // Store raw pointer for audio updates (ownership stays with unique_ptr)
        _tokenScreenPtr = tokenScreen;

        // Transition and render
        transitionTo(State::DISPLAYING_TOKEN, std::move(screen));

        // Reset touch state for double-tap detection
        _lastTouchWasValid = false;
        _lastTouchTime = 0;
    }

    // Transition to PROCESSING_VIDEO state
    // Source: displayProcessingImage() lines 2179-2235
    void showProcessing(const models::TokenMetadata& token) {
        LOG_INFO("[UI-STATE] Transitioning to PROCESSING_VIDEO (token: %s)\n",
                 token.tokenId.c_str());

        // Create processing screen with token metadata
        auto screen = std::unique_ptr<ProcessingScreen>(
            new ProcessingScreen(token)
        );

        // Transition and render
        transitionTo(State::PROCESSING_VIDEO, std::move(screen));

        // Start auto-hide timer
        _processingStartTime = millis();
    }

    // Transition to SCAN_FAILED state (non-blocking)
    //
    // Shows a brief failure message and auto-dismisses after
    // timing::SCAN_FAILED_TIMEOUT_MS (or any tap). Unlike other non-READY
    // states, SCAN_FAILED does NOT block RFID scanning — see isBlockingRFID().
    // This means a failed scan does not prevent the player from immediately
    // re-tapping the same or another token.
    //
    // @param reason Short label shown on the screen. Keep under ~16 chars.
    //               Typical values: "COMM FAILED", "READ FAILED", "UNKNOWN TOKEN".
    void showScanFailed(const String& reason = "READ FAILED") {
        LOG_INFO("[UI-STATE] Transitioning to SCAN_FAILED (%s)\n", reason.c_str());

        auto screen = std::unique_ptr<ScanFailedScreen>(
            new ScanFailedScreen(reason)
        );

        transitionTo(State::SCAN_FAILED, std::move(screen));

        // Start auto-dismiss timer
        _scanFailedStartTime = millis();

        // Reset touch state so a quick tap-to-dismiss is recognized cleanly
        _lastTouchWasValid = false;
        _lastTouchTime = 0;
    }

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP EVENT HANDLING PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    // Handle touch events with WiFi EMI filtering and state routing
    // Source: Touch handling logic lines 3577-3664
    void handleTouch() {
        // Check for touch interrupt
        if (!_touch.isTouched()) {
            // No interrupt - check for expired single-tap timeout
            if (_lastTouchWasValid &&
                (millis() - _lastTouchTime) >= timing::DOUBLE_TAP_TIMEOUT_MS) {
                _lastTouchWasValid = false;  // Clear single-tap flag
            }
            return;
        }

        // Touch interrupt detected - apply WiFi EMI filter
        if (!_touch.isValidTouch()) {
            // EMI rejected - pulse width too brief
            _touch.clearTouch();
            return;
        }

        LOG_INFO("[UI-STATE] Valid touch detected (passed EMI filter)\n");

        // Clear interrupt flag
        _touch.clearTouch();

        // Apply debouncing
        uint32_t now = millis();
        if (now - _lastTouchDebounce < timing::TOUCH_DEBOUNCE_MS) {
            LOG_INFO("[UI-STATE] Touch debounced\n");
            return;
        }
        _lastTouchDebounce = now;

        // Route to state-specific handler
        handleTouchInState(_state, now);
    }

    // Update loop - handles audio playback and auto-timeouts
    // Source: loop() audio updates and processing modal timeout
    void update() {
        // Update audio for TokenDisplayScreen (state-based, no RTTI needed)
        if (_state == State::DISPLAYING_TOKEN && _tokenScreenPtr) {
            _tokenScreenPtr->update();
        }

        // Check processing modal timeout (2.5s auto-hide)
        // Source: displayProcessingImage() lines 2227-2234
        if (_state == State::PROCESSING_VIDEO) {
            uint32_t elapsed = millis() - _processingStartTime;
            if (elapsed >= timing::PROCESSING_MODAL_TIMEOUT_MS) {
                LOG_INFO("[UI-STATE] Processing modal timeout - returning to ready\n");
                showReady(_rfidReady, _debugMode);
            }
        }

        // Check scan-failed auto-dismiss timeout
        if (_state == State::SCAN_FAILED) {
            uint32_t elapsed = millis() - _scanFailedStartTime;
            if (elapsed >= timing::SCAN_FAILED_TIMEOUT_MS) {
                LOG_INFO("[UI-STATE] SCAN_FAILED timeout - returning to ready\n");
                showReady(_rfidReady, _debugMode);
            }
        }
    }

    // Get current state
    State getState() const {
        return _state;
    }

    // Set callback for providing real system status data
    void setStatusProvider(StatusProvider provider) {
        _statusProvider = std::move(provider);
    }

    // Check if UI is blocking RFID scanning
    // Source: lines 3661-3664
    bool isBlockingRFID() const {
        return (_state == State::DISPLAYING_TOKEN ||
                _state == State::SHOWING_STATUS ||
                _state == State::PROCESSING_VIDEO);
    }

private:
    // HAL dependencies
    hal::DisplayDriver& _display;
    hal::TouchDriver& _touch;
    hal::AudioDriver& _audio;
    hal::SDCard& _sd;

    // State machine state
    State _state;
    std::unique_ptr<Screen> _currentScreen;
    TokenDisplayScreen* _tokenScreenPtr;  // Raw pointer for audio updates (no ownership)

    // Touch handling state (from v4.1 lines 98-104)
    uint32_t _lastTouchTime;
    bool _lastTouchWasValid;
    uint32_t _lastTouchDebounce;

    // Processing modal timing
    uint32_t _processingStartTime;

    // Scan-failed auto-dismiss timing
    uint32_t _scanFailedStartTime;

    // Cached application state for internal transitions
    bool _rfidReady;
    bool _debugMode;

    StatusProvider _statusProvider;  // Callback to get real status from Application

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP INTERNAL HELPERS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    // Atomic state transition with screen replacement
    void transitionTo(State newState, std::unique_ptr<Screen> screen) {
        // Stop audio if leaving DISPLAYING_TOKEN state
        if (_state == State::DISPLAYING_TOKEN && newState != State::DISPLAYING_TOKEN) {
            _audio.stop();
            _tokenScreenPtr = nullptr;  // Clear pointer when leaving token display
        }

        // Update state
        _state = newState;

        // Replace screen (old screen auto-destroyed by unique_ptr)
        _currentScreen = std::move(screen);

        // Render new screen
        if (_currentScreen) {
            _currentScreen->render(_display);
        }
    }

    // State-specific touch event routing
    // Source: Touch routing logic lines 3602-3656
    void handleTouchInState(State state, uint32_t now) {
        switch (state) {
            case State::SHOWING_STATUS:
                // Any tap dismisses status screen (FR-044)
                // Source: lines 3605-3611
                LOG_INFO("[UI-STATE] SHOWING_STATUS: Tap detected - dismissing\n");
                showReady(_rfidReady, _debugMode);
                _lastTouchWasValid = false;  // Reset double-tap logic
                break;

            case State::DISPLAYING_TOKEN:
                // Double-tap dismisses token display (FR-036)
                // Source: lines 3614-3638
                if (_lastTouchWasValid &&
                    (now - _lastTouchTime) < timing::DOUBLE_TAP_TIMEOUT_MS) {
                    // Double-tap detected
                    LOG_INFO("[UI-STATE] DISPLAYING_TOKEN: Double-tap - dismissing\n");
                    _audio.stop();
                    showReady(_rfidReady, _debugMode);
                    _lastTouchWasValid = false;
                } else {
                    // First tap - start double-tap timer
                    LOG_INFO("[UI-STATE] DISPLAYING_TOKEN: First tap registered\n");
                    _lastTouchTime = now;
                    _lastTouchWasValid = true;
                }
                break;

            case State::READY:
                // Single-tap shows status screen (FR-038)
                // Source: lines 3641-3655
                if (_lastTouchWasValid &&
                    (now - _lastTouchTime) < timing::DOUBLE_TAP_TIMEOUT_MS) {
                    // This is a double-tap in idle state - ignore
                    LOG_INFO("[UI-STATE] READY: Double-tap ignored (idle)\n");
                    _lastTouchWasValid = false;
                } else {
                    // Single tap - show status
                    LOG_INFO("[UI-STATE] READY: Single-tap - showing status\n");

                    if (_statusProvider) {
                        showStatus(_statusProvider());
                    } else {
                        LOG_ERROR("UI-STATE", "No status provider set - using defaults");
                        StatusScreen::SystemStatus status;
                        status.connState = models::ORCH_DISCONNECTED;
                        status.wifiSSID = "N/A";
                        status.localIP = "0.0.0.0";
                        status.queueSize = 0;
                        status.maxQueueSize = queue_config::MAX_QUEUE_SIZE;
                        status.teamID = "---";
                        status.deviceID = "NO PROVIDER";
                        showStatus(status);
                    }

                    _lastTouchTime = now;
                    _lastTouchWasValid = true;
                }
                break;

            case State::PROCESSING_VIDEO:
                // Processing modal ignores touch (auto-timeout only)
                LOG_INFO("[UI-STATE] PROCESSING_VIDEO: Touch ignored (auto-timeout)\n");
                break;

            case State::SCAN_FAILED:
                // Tap dismisses failure screen early (also auto-dismisses in update())
                LOG_INFO("[UI-STATE] SCAN_FAILED: Tap dismiss - returning to ready\n");
                showReady(_rfidReady, _debugMode);
                break;
        }
    }

    // Prevent copying
    UIStateMachine(const UIStateMachine&) = delete;
    UIStateMachine& operator=(const UIStateMachine&) = delete;
};

// Helper function to convert state to string (for debugging)
inline const char* stateToString(UIStateMachine::State state) {
    switch (state) {
        case UIStateMachine::State::READY:             return "READY";
        case UIStateMachine::State::SHOWING_STATUS:    return "SHOWING_STATUS";
        case UIStateMachine::State::DISPLAYING_TOKEN:  return "DISPLAYING_TOKEN";
        case UIStateMachine::State::PROCESSING_VIDEO:  return "PROCESSING_VIDEO";
        case UIStateMachine::State::SCAN_FAILED:       return "SCAN_FAILED";
        default:                                        return "UNKNOWN";
    }
}

} // namespace ui
