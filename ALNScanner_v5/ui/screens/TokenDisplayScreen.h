#pragma once

/**
 * @file TokenDisplayScreen.h
 * @brief Ghost presentation screen — image and/or audio
 *
 * Shows a ghost and plays its audio. Both are optional and independent;
 * the caller has already established which are actually present on the
 * card (see Application::presentGhost) and passes that in, so this screen
 * never has to discover a missing file by failing.
 *
 * That matters: DisplayDriver::drawBMP() paints its own "Missing: <path>"
 * error to the TFT when a file is absent. On a build where most ghosts are
 * audio-only, calling it unconditionally would put a diagnostic on screen
 * for every single scan.
 *
 *   image + audio  -> the BMP, auto-dismiss when audio ends, or tap
 *   audio only     -> the drawn ghost, auto-dismiss when audio ends, or tap
 *   image only     -> the BMP, tap to dismiss
 *
 * CRITICAL: "audio finished" and "audio never started" must stay
 * distinguishable. On main a single _audioStarted flag was false in both
 * cases, so keying auto-dismiss off it would treat a WAV the decoder
 * rejected as a clip that completed instantly — the screen would flash past
 * before a guest saw anything. The states are tracked separately here, and
 * a rejected file falls back to a timed dismissal instead.
 *
 * @namespace ui
 */

#include "../Screen.h"
#include "../../hal/DisplayDriver.h"
#include "../../hal/AudioDriver.h"
#include "../../models/Token.h"
#include "../../config.h"
#include "GhostReadyScreen.h"

namespace ui {

/**
 * @class TokenDisplayScreen
 * @brief Presents one ghost: image if it has one, audio if it has one
 */
class TokenDisplayScreen : public Screen {
public:
    /**
     * @param token     Ghost to present
     * @param hasImage  A BMP for this ghost exists on the card
     * @param hasAudio  A WAV for this ghost exists on the card
     *
     * At least one of hasImage/hasAudio must be true; the caller shows the
     * failure screen otherwise.
     */
    TokenDisplayScreen(const models::TokenMetadata& token, bool hasImage, bool hasAudio)
        : _token(token)
        , _hasImage(hasImage)
        , _expectedAudio(hasAudio)
        , _audioStarted(false)
        , _audioDone(false)
        , _enterTime(0)
    {
    }

    ~TokenDisplayScreen() {
        stopAudio();
    }

protected:
    void onRender(hal::DisplayDriver& display) override {
        enter();
    }

public:
    /**
     * @brief Render the ghost and begin playback
     *
     * No "Token Scanned: <id>" splash and no blocking delay(): on main
     * those cost a full second of dead time on every scan and put a raw
     * token id in front of the guest.
     */
    void enter() {
        LOG_INFO("[GHOST] Presenting %s (image=%s, audio=%s)\n",
                 _token.tokenId.c_str(),
                 _hasImage ? "yes" : "no",
                 _expectedAudio ? "yes" : "no");

        auto& display = hal::DisplayDriver::getInstance();
        _enterTime = millis();

        if (_hasImage) {
            const String imagePath = _token.getImagePath();
            if (!display.drawBMP(imagePath)) {
                // Existence was checked before we got here, so a failure now
                // means a malformed BMP (not 24-bit, RLE, truncated). Fall
                // back to the drawn ghost rather than leaving drawBMP's error
                // text on screen.
                LOG_ERROR("GHOST", "BMP present but unreadable - falling back to drawn ghost");
                Serial.printf("        Check it is 24-bit uncompressed: %s\n", imagePath.c_str());
                drawFallbackGhost(display);
            }
        } else {
            drawFallbackGhost(display);
        }

        if (_expectedAudio) {
            auto& audio = hal::AudioDriver::getInstance();
            if (audio.play(_token.getAudioPath())) {
                _audioStarted = true;
            } else {
                // File exists but the decoder refused it. Overwhelmingly the
                // cause is format: AudioGeneratorWAV takes 8/16-bit PCM WAV
                // only, mono or stereo.
                LOG_ERROR("GHOST", "Audio file present but would not play");
                Serial.printf("        Expected 8/16-bit PCM WAV: %s\n",
                              _token.getAudioPath().c_str());
                Serial.println("        Ghost will clear on the silent-ghost timeout.");
            }
        }
    }

    /**
     * @brief Service audio playback; call every loop iteration
     */
    void update() {
        if (_audioStarted && !_audioDone) {
            auto& audio = hal::AudioDriver::getInstance();
            audio.loop();

            if (!audio.isPlaying()) {
                LOG_INFO("[GHOST] Audio finished\n");
                _audioDone = true;
            }
        }
    }

    /**
     * @brief Whether this presentation has run its course
     *
     * The prop is unattended, so a ghost must clear itself; a guest who
     * walks away mid-clip must not leave a stale image for the next one.
     */
    bool shouldAutoDismiss() const {
        if (_expectedAudio && _audioStarted) {
            return _audioDone;                       // clip ended
        }
        if (_expectedAudio && !_audioStarted) {      // decoder rejected the file
            return (millis() - _enterTime) >= timing::SILENT_GHOST_TIMEOUT_MS;
        }
        return false;                                // image only: tap to dismiss
    }

    void exit() {
        stopAudio();
    }

    void stopAudio() {
        if (_audioStarted && !_audioDone) {
            hal::AudioDriver::getInstance().stop();
        }
        _audioStarted = false;
        _audioDone = true;
    }

    const models::TokenMetadata& getToken() const {
        return _token;
    }

private:
    /**
     * @brief Draw the ghost used when this token has no usable image
     *
     * Reuses the home screen's ghost so the prop reads as one object.
     */
    static void drawFallbackGhost(hal::DisplayDriver& display) {
        auto& tft = display.getTFT();
        tft.fillScreen(TFT_BLACK);
        GhostReadyScreen::drawGhostAt(tft, GhostReadyScreen::SCREEN_W / 2,
                                      GhostReadyScreen::SCREEN_H / 2 - 20);
    }

    models::TokenMetadata _token;
    bool _hasImage;        ///< A BMP was found on the card
    bool _expectedAudio;   ///< A WAV was found on the card
    bool _audioStarted;    ///< play() succeeded
    bool _audioDone;       ///< playback has ended (or was stopped)
    uint32_t _enterTime;   ///< For the silent-ghost fallback
};

} // namespace ui
