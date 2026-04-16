#pragma once

/**
 * @file ScanFailedScreen.h
 * @brief Non-blocking scan failure screen for ALNScanner v5.0
 *
 * Displays a short, non-blocking "SCAN FAILED" message when an RFID scan
 * cannot be processed. Unlike DISPLAYING_TOKEN / SHOWING_STATUS / PROCESSING_VIDEO,
 * the SCAN_FAILED state does NOT block RFID scanning — the next tap works
 * immediately. Paired with UIStateMachine::isBlockingRFID() returning false
 * for SCAN_FAILED, this means a failed scan does not prevent the player
 * from re-tapping the same or another token right away.
 *
 * Failure reasons this screen is used for:
 * - "COMM FAILED"    : detectCard returned DetectResult::CommFailed
 * - "READ FAILED"    : NDEF extraction exhausted retries
 * - "UNKNOWN TOKEN"  : NDEF extracted cleanly but tokenId not in database
 *
 * Visual Layout:
 *
 *   SCAN FAILED      (large, red)
 *
 *   <reason>         (orange)
 *
 *   Try again...     (small, cyan)
 *
 * Auto-dismisses after timing::SCAN_FAILED_TIMEOUT_MS (~1.5s). Also
 * dismisses on any tap. Either dismissal returns to READY state.
 */

#include "../Screen.h"
#include "../../hal/DisplayDriver.h"

namespace ui {

/**
 * @class ScanFailedScreen
 * @brief Transient failure feedback screen (non-blocking)
 *
 * Rendered when an RFID scan cannot produce a displayable token. The
 * `reason` string is caller-provided so the screen can differentiate
 * between distinct failure modes without this class needing to know
 * about the RFID internals.
 *
 * Design pattern: Stateless rendering (reason passed to constructor).
 */
class ScanFailedScreen : public Screen {
public:
    /**
     * @brief Construct failure screen with a short reason string
     * @param reason Short human-readable label (e.g. "READ FAILED",
     *               "COMM FAILED", "UNKNOWN TOKEN"). Keep under ~16
     *               characters so it fits on a single line at text size 2.
     */
    explicit ScanFailedScreen(const String& reason)
        : _reason(reason)
    {
    }

    virtual ~ScanFailedScreen() = default;

protected:
    void onRender(hal::DisplayDriver& display) override {
        auto& tft = display.getTFT();

        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 80);

        // Large red header
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(3);
        tft.println(" SCAN FAILED");
        tft.println("");

        // Orange reason line
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.setTextSize(2);
        tft.println(" " + _reason);
        tft.println("");

        // Small cyan retry hint
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(1);
        tft.println(" Try again...");
    }

private:
    String _reason;
};

} // namespace ui
