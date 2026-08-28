#pragma once

#include "Screen.h"
#include "screens/GhostReadyScreen.h"
#include "screens/StatusScreen.h"
#include "screens/TokenDisplayScreen.h"
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
//    READY (the "place ghost here" home screen)
//        tap            -> ignored
//        5s hold        -> SHOWING_STATUS
//        RFID scan      -> DISPLAYING_TOKEN or SCAN_FAILED
//
//    SHOWING_STATUS (hidden diagnostics)
//        any tap        -> READY
//
//    DISPLAYING_TOKEN (ghost on screen, audio playing)
//        audio ends     -> READY   (auto; the prop is unattended)
//        tap            -> READY
//
//    SCAN_FAILED (in-world "that's no spirit", non-blocking)
//        1.5s auto      -> READY
//        tap            -> READY
//
// RFID is blocked in DISPLAYING_TOKEN and SHOWING_STATUS, but NOT in
// SCAN_FAILED — a guest must be able to re-tap immediately after a miss.
//
// TOUCH HANDLING:
// - WiFi EMI filtering via TouchDriver (pulse width threshold)
// - Debouncing (50ms)
// - Sustained-hold detection polled in updateLongPress(), because
//   measurePulseWidth() caps at 500ms and blocks while measuring
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
        DISPLAYING_TOKEN,   // Ghost on screen, audio playing
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
        , _pressStart(0)
        , _scanFailedStartTime(0)
        , _rfidReady(false)
        , _debugMode(false)
    {
        LOG_INFO("[UI-STATE] UIStateMachine initialized\n");
    }

    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPP STATE TRANSITIONS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
    // PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

    // Transition to READY state (the "place ghost here" home screen)
    void showReady(bool rfidReady, bool debugMode) {
        _rfidReady = rfidReady;
        _debugMode = debugMode;

        LOG_INFO("[UI-STATE] Transitioning to READY (RFID: %s, Debug: %s)\n",
                 rfidReady ? "ready" : "disabled",
                 debugMode ? "ON" : "OFF");

        auto screen = std::unique_ptr<GhostReadyScreen>(
            new GhostReadyScreen(rfidReady)
        );

        transitionTo(State::READY, std::move(screen));

        _lastTouchWasValid = false;
        _lastTouchTime = 0;
        _pressStart = 0;
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
    //
    // @param hasImage A BMP for this ghost exists on the card
    // @param hasAudio A WAV for this ghost exists on the card
    //
    // The caller establishes both before calling; at least one must be true.
    void showToken(const models::TokenMetadata& token, bool hasImage, bool hasAudio) {
        LOG_INFO("[UI-STATE] Transitioning to DISPLAYING_TOKEN (%s)\n",
                 token.tokenId.c_str());

        auto* tokenScreen = new TokenDisplayScreen(token, hasImage, hasAudio);
        auto screen = std::unique_ptr<TokenDisplayScreen>(tokenScreen);

        // Raw pointer for update()/auto-dismiss polling; unique_ptr keeps ownership
        _tokenScreenPtr = tokenScreen;

        transitionTo(State::DISPLAYING_TOKEN, std::move(screen));

        _lastTouchWasValid = false;
        _lastTouchTime = 0;
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

    // Update loop - services audio, auto-dismissal and the long-press
    void update() {
        if (_state == State::DISPLAYING_TOKEN && _tokenScreenPtr) {
            _tokenScreenPtr->update();

            // The prop is unattended: a ghost must clear itself so a guest
            // who walks away mid-clip does not leave a stale screen.
            if (_tokenScreenPtr->shouldAutoDismiss()) {
                LOG_INFO("[UI-STATE] Ghost finished - returning to ready\n");
                showReady(_rfidReady, _debugMode);
            }
        }

        // Scan-failed auto-dismiss
        if (_state == State::SCAN_FAILED) {
            if ((millis() - _scanFailedStartTime) >= timing::SCAN_FAILED_TIMEOUT_MS) {
                LOG_INFO("[UI-STATE] SCAN_FAILED timeout - returning to ready\n");
                showReady(_rfidReady, _debugMode);
            }
        }

        updateLongPress();
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
                _state == State::SHOWING_STATUS);
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

    // Long-press (hidden status screen) timing
    uint32_t _pressStart;

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
                // Single tap dismisses. Double-tap was safe on main only
                // because the home screen carried a "Double-Tap to Escape"
                // hint; that hint is gone, and an unhinted double-tap is
                // undiscoverable. Single tap is safe here now that the
                // status screen requires a deliberate long-press.
                LOG_INFO("[UI-STATE] DISPLAYING_TOKEN: Tap - dismissing\n");
                _audio.stop();
                showReady(_rfidReady, _debugMode);
                break;

            case State::READY:
                // Deliberately inert. Guests will touch the screen; the
                // status screen is reachable only by a sustained hold,
                // handled in updateLongPress().
                LOG_INFO("[UI-STATE] READY: Tap ignored (hold %lums for status)\n",
                         (unsigned long)timing::LONG_PRESS_MS);
                break;

            case State::SCAN_FAILED:
                // Tap dismisses failure screen early (also auto-dismisses in update())
                LOG_INFO("[UI-STATE] SCAN_FAILED: Tap dismiss - returning to ready\n");
                showReady(_rfidReady, _debugMode);
                break;
        }
    }

    /**
     * @brief Reveal the hidden status screen on a sustained hold
     *
     * Polled rather than measured inside handleTouch(): TouchDriver's
     * measurePulseWidth() caps at 500ms and blocks while measuring, so it
     * can neither observe a multi-second hold nor be called from a loop
     * that must keep polling RFID and servicing audio.
     *
     * Only armed on the home screen. A hold that begins elsewhere, or that
     * lifts early, resets cleanly.
     */
    void updateLongPress() {
        if (_state != State::READY) {
            _pressStart = 0;
            return;
        }

        if (!_touch.isPressed()) {
            _pressStart = 0;
            return;
        }

        if (_pressStart == 0) {
            _pressStart = millis();
            return;
        }

        if ((millis() - _pressStart) >= timing::LONG_PRESS_MS) {
            _pressStart = 0;
            LOG_INFO("[UI-STATE] Long-press - showing status\n");

            if (_statusProvider) {
                showStatus(_statusProvider());
            } else {
                LOG_ERROR("UI-STATE", "No status provider set");
                StatusScreen::SystemStatus status;
                status.deviceID   = "NO PROVIDER";
                status.ghostCount = 0;
                status.rfidReady  = _rfidReady;
                status.sdPresent  = false;
                status.volume     = 0.0f;
                status.freeHeap   = 0;
                showStatus(status);
            }
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
        case UIStateMachine::State::SCAN_FAILED:       return "SCAN_FAILED";
        default:                                        return "UNKNOWN";
    }
}

} // namespace ui
