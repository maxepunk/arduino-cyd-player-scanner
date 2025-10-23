#pragma once

/**
 * @file TokenDisplayScreen.h
 * @brief Token display screen for regular (non-video) tokens
 *
 * Displays token BMP image and plays audio until dismissed by double-tap.
 * Video tokens are handled by ProcessingScreen instead.
 *
 * Extracted from v4.1 monolithic codebase:
 * - Lines 3511-3559: processTokenScan() regular token display section
 * - Lines 924-1091: drawBmp() BMP rendering
 * - Lines 1094-1154: startAudio() audio playback
 * - Lines 3568-3572: loop() audio processing
 *
 * Architecture:
 * - Singleton HAL access (DisplayDriver, AudioDriver)
 * - Constitution-compliant SPI bus management
 * - Automatic audio cleanup on screen exit
 *
 * Usage:
 *   TokenMetadata token = {...};
 *   auto* screen = new TokenDisplayScreen(token);
 *   screen->enter();  // Displays image + starts audio
 *   screen->update(); // Call in loop() for audio playback
 *   screen->exit();   // Stops audio
 */

#include <Arduino.h>
#include "../../config.h"
#include "../../models/Token.h"
#include "../../hal/DisplayDriver.h"
#include "../../hal/AudioDriver.h"
#include "../Screen.h"

namespace ui {

/**
 * @class TokenDisplayScreen
 * @brief Screen for displaying regular token (BMP image + audio playback)
 *
 * Lifecycle:
 * 1. Constructor: Store token metadata
 * 2. enter(): Display BMP, start audio playback
 * 3. update(): Service audio loop (call in main loop)
 * 4. exit(): Stop audio, clean up resources
 *
 * Key Features:
 * - Automatic BMP image display from SD card
 * - Automatic audio playback (lazy-initialized)
 * - Persistent display until dismissed (vs ProcessingScreen modal)
 * - Graceful error handling for missing files
 * - Constitution-compliant SPI bus usage
 *
 * Error Handling:
 * - Missing image: Shows "Missing: /path/to/file.bmp" on screen
 * - Missing audio: Continues silently (no audio playback)
 * - SD card failure: Shows appropriate error message
 *
 * Thread Safety:
 * - SD card access uses SDCard::Lock RAII pattern
 * - Audio playback is thread-safe (single-threaded access)
 * - Display operations are main-thread only
 */
class TokenDisplayScreen : public Screen {
public:
    /**
     * @brief Construct token display screen
     * @param token Token metadata (must include valid tokenId)
     *
     * Stores token metadata for rendering. Actual display happens in enter().
     */
    TokenDisplayScreen(const models::TokenMetadata& token)
        : _token(token)
        , _audioStarted(false)
    {
        LOG_DEBUG("[TOKEN-DISPLAY] Constructor: tokenId=%s\n", token.tokenId.c_str());
    }

    /**
     * @brief Destructor - ensures audio cleanup
     *
     * Stops any playing audio when screen is destroyed.
     */
    ~TokenDisplayScreen() {
        LOG_DEBUG("[TOKEN-DISPLAY] Destructor\n");
        stopAudio();
    }

protected:
    /**
     * @brief Render screen override for Screen base class
     * @param display DisplayDriver reference (not used - uses singleton internally)
     *
     * This is required to inherit from Screen base class. Simply delegates to enter().
     */
    void onRender(hal::DisplayDriver& display) override {
        enter();
    }

public:
    /**
     * @brief Enter screen - display image and start audio
     *
     * This is the main rendering function, extracted from v4.1 lines 3511-3551.
     *
     * Flow:
     * 1. Display "Token Scanned: {tokenId}" splash (1s)
     * 2. Render BMP image from SD card (Constitution-compliant)
     * 3. Start audio playback (lazy init if needed)
     * 4. Set _audioStarted flag for update() loop
     *
     * Extracted from v4.1:
     * - Lines 3513-3520: Token ID splash screen
     * - Lines 3522-3536: BMP display with SD mutex
     * - Lines 3539-3550: Audio playback with SD mutex
     */
    void enter() {
        LOG_INFO("[TOKEN-DISPLAY] PPP ENTER TOKEN DISPLAY SCREEN PPP\n");
        LOG_INFO("[TOKEN-DISPLAY] Token ID: %s\n", _token.tokenId.c_str());
        LOG_INFO("[TOKEN-DISPLAY] Free heap: %d bytes\n", ESP.getFreeHeap());

        auto& display = hal::DisplayDriver::getInstance();
        auto& tft = display.getTFT();

        // PPP SPLASH SCREEN PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
        // Show "Token Scanned: {tokenId}" for 1 second
        // Extracted from v4.1 lines 3513-3520
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.println("Token Scanned:");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.println(_token.tokenId);
        delay(1000);

        // PPP BMP IMAGE DISPLAY PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
        // Display token image (Constitution-compliant SPI pattern)
        // Extracted from v4.1 lines 3522-3536
        String imagePath = _token.getImagePath();
        LOG_INFO("[TOKEN-DISPLAY] BMP: %s\n", imagePath.c_str());

        // DisplayDriver::drawBMP() handles SD mutex internally
        if (!display.drawBMP(imagePath)) {
            LOG_ERROR("TOKEN-DISPLAY", "Failed to display BMP image");
            // Error message already shown by DisplayDriver
        }

        // PPP AUDIO PLAYBACK PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
        // Start audio playback (lazy init if first use)
        // Extracted from v4.1 lines 3539-3550
        String audioPath = _token.getAudioPath();
        LOG_INFO("[TOKEN-DISPLAY] Audio: %s\n", audioPath.c_str());

        auto& audio = hal::AudioDriver::getInstance();

        // AudioDriver::play() handles SD access internally
        if (audio.play(audioPath)) {
            LOG_INFO("[TOKEN-DISPLAY] Audio playback started\n");
            _audioStarted = true;
        } else {
            LOG_ERROR("TOKEN-DISPLAY", "Failed to start audio playback");
            // Continue without audio (graceful degradation)
            _audioStarted = false;
        }

        LOG_INFO("[TOKEN-DISPLAY] Free heap after enter: %d bytes\n", ESP.getFreeHeap());
        LOG_INFO("[TOKEN-DISPLAY] PPP ENTER COMPLETE PPP\n");
    }

    /**
     * @brief Update screen - service audio playback
     *
     * MUST be called frequently in main loop() for smooth audio playback.
     * If not called, audio will stutter or stop.
     *
     * Extracted from v4.1 lines 3568-3572:
     * @code
     * if (wav && wav->isRunning()) {
     *     if (!wav->loop()) {
     *         stopAudio();
     *     }
     * }
     * @endcode
     */
    void update() {
        if (_audioStarted) {
            auto& audio = hal::AudioDriver::getInstance();
            audio.loop();

            // Check if audio finished naturally
            if (!audio.isPlaying()) {
                LOG_DEBUG("[TOKEN-DISPLAY] Audio finished naturally\n");
                _audioStarted = false;
            }
        }
    }

    /**
     * @brief Exit screen - stop audio and clean up
     *
     * Called when transitioning to another screen (e.g., after double-tap).
     * Ensures audio resources are freed.
     */
    void exit() {
        LOG_DEBUG("[TOKEN-DISPLAY] Exiting token display screen\n");
        stopAudio();
    }

    /**
     * @brief Stop audio playback
     *
     * Safe to call multiple times or when audio is not playing.
     * Cleans up audio resources and sets _audioStarted flag to false.
     *
     * Extracted from v4.1 lines 1156-1170 (stopAudio function)
     */
    void stopAudio() {
        if (_audioStarted) {
            LOG_DEBUG("[TOKEN-DISPLAY] Stopping audio playback\n");
            auto& audio = hal::AudioDriver::getInstance();
            audio.stop();
            _audioStarted = false;
        }
    }

    /**
     * @brief Check if audio is currently playing
     * @return true if audio is actively playing
     */
    bool isAudioPlaying() const {
        return _audioStarted;
    }

    /**
     * @brief Get token metadata
     * @return Reference to stored token metadata
     */
    const models::TokenMetadata& getToken() const {
        return _token;
    }

private:
    models::TokenMetadata _token;  // Token metadata (image/audio paths)
    bool _audioStarted;            // Flag: audio playback active
};

} // namespace ui

/**
 * IMPLEMENTATION NOTES (from v4.1 extraction)
 *
 * 1. SOURCE LINE MAPPINGS
 *    v4.1 Lines � TokenDisplayScreen Implementation:
 *    - 3513-3520 � enter() splash screen
 *    - 3522-3536 � enter() BMP display
 *    - 3539-3550 � enter() audio playback
 *    - 3568-3572 � update() audio loop
 *    - 1156-1170 � stopAudio()
 *
 * 2. REFACTORING CHANGES
 *    a) SD Mutex Handling:
 *       - v4.1: Manual sdTakeMutex()/sdGiveMutex() calls
 *       - v5.0: Encapsulated in DisplayDriver::drawBMP() and AudioDriver::play()
 *       - Rationale: Prevent mutex leaks, enforce RAII pattern
 *
 *    b) Global State Elimination:
 *       - v4.1: Global variables (imageIsDisplayed, wav, file, out)
 *       - v5.0: Instance variables (_audioStarted), HAL singleton state
 *       - Rationale: Better encapsulation, testability, thread safety
 *
 *    c) Error Handling:
 *       - v4.1: Prints errors but continues
 *       - v5.0: Returns error codes, HAL shows error messages
 *       - Rationale: Separation of concerns (display vs logging)
 *
 *    d) File Path Construction:
 *       - v4.1: ndefToFilename(), ndefToAudioFilename()
 *       - v5.0: TokenMetadata::getImagePath(), getAudioPath()
 *       - Rationale: Path logic belongs in model, not display
 *
 * 3. CONSTITUTION COMPLIANCE
 *    SPI Bus Management:
 *    - v4.1: Manual mutex + TFT lock coordination in drawBmp()
 *    - v5.0: DisplayDriver::drawBMP() handles entire pattern
 *    - Result: Screen code doesn't need to know about SPI deadlock
 *
 *    Pattern Preserved (in DisplayDriver.h):
 *    @code
 *    for (each row) {
 *        f.read(rowBuffer, rowBytes);  // SD read FIRST
 *        tft.startWrite();              // TFT lock SECOND
 *        tft.setAddrWindow(...);
 *        tft.pushColor(...);
 *        tft.endWrite();
 *        yield();
 *    }
 *    @endcode
 *
 * 4. AUDIO PLAYBACK INTEGRATION
 *    Lazy Initialization:
 *    - v4.1: AudioOutputI2S created on first startAudio() call
 *    - v5.0: AudioDriver::play() triggers lazy init
 *    - Reason: Prevent electrical beeping at boot (CLAUDE.md lines 1100-1106)
 *
 *    Loop Processing:
 *    - v4.1: Checked in main loop() with global wav pointer
 *    - v5.0: TokenDisplayScreen::update() � AudioDriver::loop()
 *    - Rationale: Screen owns audio lifecycle, HAL provides service
 *
 * 5. VIDEO TOKEN HANDLING
 *    This screen is ONLY for regular tokens (persistent display + audio).
 *    Video tokens are handled by ProcessingScreen (2.5s modal).
 *
 *    Detection Logic (in caller):
 *    @code
 *    if (token.isVideoToken()) {
 *        screen = new ProcessingScreen(token);  // Modal with timeout
 *    } else {
 *        screen = new TokenDisplayScreen(token);  // Persistent display
 *    }
 *    @endcode
 *
 * 6. SCREEN LIFECYCLE
 *    State Machine Integration:
 *    @code
 *    // UIStateMachine.h (pseudocode)
 *    void transitionToTokenDisplay(TokenMetadata token) {
 *        exitCurrentScreen();
 *        _currentScreen = new TokenDisplayScreen(token);
 *        _currentScreen->enter();
 *        _state = STATE_TOKEN_DISPLAY;
 *    }
 *
 *    void loop() {
 *        if (_state == STATE_TOKEN_DISPLAY) {
 *            _currentScreen->update();  // Service audio
 *
 *            if (touch.wasDoubleTap()) {
 *                transitionToReady();
 *            }
 *        }
 *    }
 *    @endcode
 *
 * 7. MEMORY FOOTPRINT
 *    Stack Usage:
 *    - TokenMetadata: ~100 bytes (4 Strings)
 *    - _audioStarted: 1 byte
 *    - Total: ~100 bytes per instance
 *
 *    Heap Usage:
 *    - BMP row buffer: 720 bytes (240 pixels * 3 bytes, freed after render)
 *    - Audio resources: ~2KB (AudioGeneratorWAV + AudioFileSourceSD)
 *    - Total peak: ~3KB during enter(), ~2KB during playback
 *
 * 8. ERROR RECOVERY
 *    Missing Image:
 *    - DisplayDriver::drawBMP() returns false
 *    - Error message shown on screen: "Missing: /images/kaa001.bmp"
 *    - Audio still plays (graceful degradation)
 *
 *    Missing Audio:
 *    - AudioDriver::play() returns false
 *    - _audioStarted stays false, update() does nothing
 *    - Image still displayed (graceful degradation)
 *
 *    SD Card Failure:
 *    - DisplayDriver::drawBMP() shows "SD Busy" or "No SD Card"
 *    - AudioDriver::play() fails silently (SD check inside)
 *    - Screen remains functional (shows error message)
 *
 * 9. FUTURE ENHANCEMENTS (NOT IMPLEMENTED)
 *    - Progress bar for audio playback (requires AudioGeneratorWAV extension)
 *    - Volume control (requires AudioOutputI2S configuration)
 *    - Pause/resume audio (requires state management)
 *    - Background music (requires audio mixing)
 *    - Animated token images (requires GIF/APNG support)
 *
 * 10. TESTING CHECKLIST
 *     [ ] Regular token with valid image + audio
 *     [ ] Regular token with missing image (shows error)
 *     [ ] Regular token with missing audio (silent)
 *     [ ] Regular token with both missing (shows error, silent)
 *     [ ] Video token (should NOT use this screen)
 *     [ ] SD card removed during display (error handling)
 *     [ ] Double-tap dismiss (audio stops cleanly)
 *     [ ] Audio finishes naturally (cleanup happens)
 *     [ ] Multiple sequential tokens (no resource leak)
 *     [ ] Memory stability (heap usage stable)
 */
