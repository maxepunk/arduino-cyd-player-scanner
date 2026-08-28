#pragma once

/**
 * @file ScanFailedScreen.h
 * @brief Non-blocking failure feedback, phrased in-world
 *
 * Shown when a tap cannot produce a ghost. Three distinct causes share this
 * screen, deliberately:
 *   - the tag could not be read (comms or NDEF failure)
 *   - the tag was read but its id is not in the database
 *   - the id is known but the card carries neither audio nor image
 *
 * A guest must not be able to tell these apart — a diagnostic string in
 * front of an audience reads as a broken machine, and the first two are
 * indistinguishable to them anyway ("it didn't work, try again"). The
 * reason is therefore logged to serial and never rendered.
 *
 * Unlike DISPLAYING_TOKEN, this state does NOT block RFID. A guest can
 * re-tap immediately without waiting for the screen to clear.
 *
 * @namespace ui
 */

#include "../Screen.h"
#include "../../hal/DisplayDriver.h"

namespace ui {

/**
 * @class ScanFailedScreen
 * @brief In-world "that didn't work" feedback
 */
class ScanFailedScreen : public Screen {
public:
    /**
     * @param reason Diagnostic string, logged to serial only — never drawn
     */
    explicit ScanFailedScreen(const String& reason)
        : _reason(reason)
    {
        LOG_INFO("[SCAN-FAILED] Showing failure screen (reason: %s)\n", _reason.c_str());
    }

    virtual ~ScanFailedScreen() = default;

    static constexpr int16_t SCREEN_W = 240;

protected:
    void onRender(hal::DisplayDriver& display) override {
        auto& tft = display.getTFT();

        tft.fillScreen(TFT_BLACK);

        printCentred(tft, "THAT'S NO", 3, 110, TFT_WHITE);
        printCentred(tft, "SPIRIT",    3, 145, TFT_WHITE);
        printCentred(tft, "TRY AGAIN", 2, 195, TFT_CYAN);
    }

private:
    static void printCentred(TFT_eSPI& tft, const char* text,
                             uint8_t size, int16_t y, uint16_t colour) {
        const int16_t width = (int16_t)(6 * size * strlen(text));
        tft.setTextSize(size);
        tft.setTextColor(colour, TFT_BLACK);
        tft.setCursor((SCREEN_W - width) / 2, y);
        tft.print(text);
    }

    String _reason;  ///< Serial diagnostics only
};

} // namespace ui
