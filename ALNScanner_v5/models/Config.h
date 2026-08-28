#pragma once

#include <Arduino.h>
#include "../config.h"

namespace models {

/**
 * @brief Device configuration for the standalone ghost scanner
 *
 * Everything networking-related is gone. With no orchestrator there is no
 * WiFi, no URL, no team and no sync flags — what remains is exactly what a
 * person editing config.txt on the SD card can usefully change.
 */
struct DeviceConfig {
    String deviceID;                          ///< Which prop this is; blank derives from MAC
    bool   debugMode = false;                 ///< Defer RFID init so serial commands work
    float  volume = limits::DEFAULT_VOLUME;   ///< Playback gain (see limits::MIN/MAX_VOLUME)

    DeviceConfig() = default;

    /**
     * @brief Validate and normalize the configuration
     * @return false only if a value is unusable
     *
     * Deliberately permissive. This prop is assembled by someone who is not
     * the developer, from a hand-edited text file, and it must boot. Only an
     * over-long deviceID fails outright; an out-of-range volume is clamped
     * rather than rejected, because refusing to boot over a stray digit is a
     * far worse outcome than playing too quietly.
     */
    bool validate() {
        if (deviceID.length() > limits::MAX_DEVICE_ID_LENGTH) {
            return false;
        }

        if (volume < limits::MIN_VOLUME) {
            Serial.printf("[CONFIG] VOLUME %.2f below %.2f - clamping\n",
                          volume, limits::MIN_VOLUME);
            volume = limits::MIN_VOLUME;
        } else if (volume > limits::MAX_VOLUME) {
            Serial.printf("[CONFIG] VOLUME %.2f above %.2f - clamping\n",
                          volume, limits::MAX_VOLUME);
            volume = limits::MAX_VOLUME;
        }

        return true;
    }

    /**
     * @brief Whether the config is usable as-is
     *
     * Always true on this build: every field has a working default, so a
     * missing or empty config.txt still yields a functioning prop.
     */
    bool isComplete() const {
        return true;
    }

    void print() const {
        Serial.println("\n=== Device Configuration ===");
        Serial.printf("Device ID:  %s\n",
                      deviceID.length() > 0 ? deviceID.c_str() : "(auto-generate)");
        Serial.printf("Debug Mode: %s\n", debugMode ? "true" : "false");
        Serial.printf("Volume:     %.2f\n", volume);
        Serial.println("============================\n");
    }
};

} // namespace models
