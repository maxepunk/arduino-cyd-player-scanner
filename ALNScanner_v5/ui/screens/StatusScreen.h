#pragma once

/**
 * @file StatusScreen.h
 * @brief Hidden diagnostics screen for the ghost scanner
 *
 * Reached only by a deliberate 5-second long-press on the home screen —
 * there is no on-screen hint for it. A single tap was too easy for a guest
 * to trigger by accident, but the prop is shipped to a venue with no serial
 * access, so some way to read its state without a laptop is essential.
 *
 * Deliberately terse. Everything the orchestrator build showed here (WiFi
 * SSID, local IP, orchestrator state, queue depth, team ID) is meaningless
 * on this build and has been removed.
 *
 * Design: stateless rendering; all state arrives via SystemStatus.
 *
 * @namespace ui
 */

#include "../Screen.h"
#include "../../hal/DisplayDriver.h"

namespace ui {

/**
 * @class StatusScreen
 * @brief Hidden diagnostics readout
 */
class StatusScreen : public Screen {
public:
    /**
     * @struct SystemStatus
     * @brief Status snapshot, captured at construction for consistent render
     */
    struct SystemStatus {
        String deviceID;    ///< Which prop this is (e.g. "SCANNER_004")
        int    ghostCount;  ///< Ghosts loaded from /tokens.json
        bool   rfidReady;   ///< RFID reader initialized
        bool   sdPresent;   ///< SD card mounted
        float  volume;      ///< Configured playback gain
        int    freeHeap;    ///< Free heap in bytes
    };

    explicit StatusScreen(const SystemStatus& status)
        : _status(status)
    {
    }

    virtual ~StatusScreen() = default;

protected:
    void onRender(hal::DisplayDriver& display) override {
        auto& tft = display.getTFT();

        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 10);
        tft.setTextSize(2);

        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(" STATUS");
        tft.println("");

        tft.setTextSize(1);

        // Device identity — the reason DEVICE_ID still exists on this build
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" Device:  ");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(_status.deviceID.length() ? _status.deviceID : "(unset)");

        // SD card — if this is wrong, nothing else works
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" SD card: ");
        if (_status.sdPresent) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.println("present");
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("ABSENT");
        }

        // Ghost count — zero means tokens.json is missing or malformed
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" Ghosts:  ");
        if (_status.ghostCount > 0) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.printf("%d loaded\n", _status.ghostCount);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("NONE LOADED");
        }

        // RFID — in debug mode this is deliberately deferred, not broken
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" RFID:    ");
        if (_status.rfidReady) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.println("ready");
        } else {
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
            tft.println("not started");
        }

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" Volume:  ");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.printf("%.2f\n", _status.volume);

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" Heap:    ");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.printf("%d bytes\n", _status.freeHeap);

        tft.println("");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(" Tap to dismiss");
    }

private:
    SystemStatus _status;
};

} // namespace ui
