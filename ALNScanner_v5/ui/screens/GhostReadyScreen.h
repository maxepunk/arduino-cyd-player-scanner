#pragma once

/**
 * @file GhostReadyScreen.h
 * @brief Idle "place ghost here" screen for the ghost scanner prop
 *
 * Replaces the ALN ReadyScreen. Everything a guest should never see is
 * gone: no branding, no version, no "Tap for Status" hint, no explanation
 * of the GPIO 3 debug-mode conflict.
 *
 * Layout is fixed to the CYD's portrait orientation (240x320,
 * setRotation(0)):
 *
 *     PLACE GHOST          <- instruction, top ("WAKING UP" until
 *        HERE                    RFID is live)
 *
 *        (ghost)           <- drawn, centre
 *
 *          |               <- arrow pointing at the bottom edge,
 *          V                  where the RFID antenna sits
 *
 * The arrow points down because the MFRC522 is mounted BELOW the screen in
 * this enclosure. If the antenna ever moves, this screen must change with
 * it — an arrow pointing at nothing is worse than no arrow.
 *
 * Rendering is static by design. The main loop must stay responsive to the
 * 500ms RFID poll, and an animated idle screen would couple frame timing
 * to scan latency for no real benefit.
 *
 * @namespace ui
 */

#include "../Screen.h"
#include "../../hal/DisplayDriver.h"

namespace ui {

/**
 * @class GhostReadyScreen
 * @brief Idle screen instructing the guest where to place a ghost
 */
class GhostReadyScreen : public Screen {
public:
    /**
     * @param rfidReady True once the RFID reader is initialized
     *
     * When false the screen says WAKING UP and shows no arrow, because the
     * scanner genuinely cannot read a tag yet — RFID is initialised after
     * the boot-override window, and in DEBUG_MODE not at all until
     * START_SCANNER. Showing "PLACE GHOST HERE" in that state invites a
     * guest to try something that cannot work.
     */
    explicit GhostReadyScreen(bool rfidReady)
        : _rfidReady(rfidReady)
    {
    }

    virtual ~GhostReadyScreen() = default;

    /// Screen geometry (portrait, setRotation(0))
    static constexpr int16_t SCREEN_W = 240;
    static constexpr int16_t SCREEN_H = 320;

    /**
     * @brief Draw a classic sheet ghost centred on (cx, cy)
     *
     * Public and static because TokenDisplayScreen draws the same ghost for
     * audio-only tokens. Sharing one primitive keeps the prop looking like
     * a single object rather than two screens that happen to co-exist.
     *
     * Built from primitives rather than text art: the built-in font's glyph
     * spacing makes ASCII ghosts look cramped and misaligned at any size
     * this screen can show.
     */
    static void drawGhostAt(TFT_eSPI& tft, int16_t cx, int16_t cy) {
        constexpr int16_t R = 34;       // head radius / half body width
        constexpr int16_t BODY = 46;    // straight body section height

        // Domed head and rectangular body form one continuous sheet
        tft.fillCircle(cx, cy, R, TFT_WHITE);
        tft.fillRect(cx - R, cy, 2 * R + 1, BODY, TFT_WHITE);

        // Scalloped hem: three bumps along the bottom edge
        const int16_t hemY = cy + BODY;
        const int16_t bump = R / 3;
        for (int8_t i = -1; i <= 1; i++) {
            tft.fillCircle(cx + i * (2 * bump), hemY, bump, TFT_WHITE);
        }
        // Notches between the bumps, cut back to the background
        tft.fillCircle(cx - bump, hemY, bump / 2, TFT_BLACK);
        tft.fillCircle(cx + bump, hemY, bump / 2, TFT_BLACK);

        // Face
        tft.fillCircle(cx - 12, cy - 6, 6, TFT_BLACK);
        tft.fillCircle(cx + 12, cy - 6, 6, TFT_BLACK);
        tft.fillCircle(cx, cy + 12, 5, TFT_BLACK);
    }

protected:
    void onRender(hal::DisplayDriver& display) override {
        auto& tft = display.getTFT();

        tft.fillScreen(TFT_BLACK);

        if (_rfidReady) {
            printCentred(tft, "PLACE GHOST", 3, 30, TFT_WHITE);
            printCentred(tft, "HERE",        3, 62, TFT_WHITE);
        } else {
            // The scanner physically cannot read a tag yet: RFID is
            // initialised AFTER the boot-override window. Telling a guest
            // to place a ghost here would be a lie for ~10 seconds, and
            // they would get no response and conclude the prop is broken.
            printCentred(tft, "WAKING UP", 3, 30, TFT_CYAN);
        }

        drawGhostAt(tft, SCREEN_W / 2, 150);

        // The arrow points at the antenna, so it only appears once the
        // antenna is actually listening.
        if (_rfidReady) {
            drawArrow(tft, SCREEN_W / 2, 240);
        }
    }

private:
    /**
     * @brief Centre a line of built-in-font text at a given text size
     *
     * The built-in GLCD font is 6px per character before scaling, so a
     * string's rendered width is 6 * size * length.
     */
    static void printCentred(TFT_eSPI& tft, const char* text,
                             uint8_t size, int16_t y, uint16_t colour) {
        const int16_t width = (int16_t)(6 * size * strlen(text));
        tft.setTextSize(size);
        tft.setTextColor(colour, TFT_BLACK);
        tft.setCursor((SCREEN_W - width) / 2, y);
        tft.print(text);
    }

    /**
     * @brief Draw a downward arrow whose tip sits near the bottom edge
     */
    static void drawArrow(TFT_eSPI& tft, int16_t cx, int16_t topY) {
        constexpr int16_t STEM_W = 14;
        constexpr int16_t STEM_H = 34;
        constexpr int16_t HEAD_W = 46;
        constexpr int16_t HEAD_H = 30;

        tft.fillRect(cx - STEM_W / 2, topY, STEM_W, STEM_H, TFT_CYAN);

        const int16_t headTop = topY + STEM_H;
        tft.fillTriangle(cx - HEAD_W / 2, headTop,
                         cx + HEAD_W / 2, headTop,
                         cx,              headTop + HEAD_H,
                         TFT_CYAN);
    }

    bool _rfidReady;
};

} // namespace ui
