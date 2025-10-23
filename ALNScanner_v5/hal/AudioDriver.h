#pragma once

#include <AudioFileSourceSD.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include "../config.h"

namespace hal {

/**
 * AudioDriver - I2S Audio Playback HAL
 *
 * CRITICAL: Uses LAZY INITIALIZATION to prevent beeping!
 *
 * The Problem:
 * - AudioOutputI2S initialization in setup() causes electrical noise/beeping
 * - This is due to DAC initialization interacting with RFID polling on GPIO 27
 *
 * The Solution:
 * - Defer AudioOutputI2S creation until first play() call
 * - Silence DAC pins (25, 26) at boot to prevent noise
 * - Initialize audio subsystem only when needed
 *
 * Hardware Context (from CLAUDE.md):
 * - GPIO 27 (RFID_MOSI) is electrically coupled to speaker amplifier
 * - Cannot be fixed in hardware without board modification
 * - Software mitigation: lazy init + DAC silencing
 *
 * Usage:
 *   auto& audio = AudioDriver::getInstance();
 *   // NO begin() call in setup() - lazy init!
 *
 *   // First play() triggers initialization
 *   if (audio.play("/AUDIO/token.wav")) {
 *       // Loop must be called frequently
 *       audio.loop();
 *   }
 *
 * Dependencies:
 * - SDCard.h must be initialized before use
 * - ESP8266Audio library (AudioGeneratorWAV, AudioOutputI2S)
 */
class AudioDriver {
public:
    static AudioDriver& getInstance() {
        static AudioDriver instance;
        return instance;
    }

    /**
     * Initialize audio subsystem (LAZY - call not required)
     *
     * This is public for explicit initialization if desired,
     * but normally happens automatically on first play().
     *
     * Returns: true if initialized successfully
     */
    bool begin() {
        if (_initialized) {
            LOG_DEBUG("[AUDIO-HAL] Already initialized (reusing)\n");
            return true;
        }

        LOG_INFO("[AUDIO-HAL] Lazy initialization at %lu ms (deferred from setup)\n", millis());

        // CRITICAL: Create AudioOutputI2S ONLY when needed
        // Creating in setup() causes electrical noise/beeping
        _output = new AudioOutputI2S(0, 1);  // Internal DAC, port 0, mode 1

        if (!_output) {
            LOG_ERROR("AUDIO-HAL", "Failed to create AudioOutputI2S");
            return false;
        }

        LOG_INFO("[AUDIO-HAL] AudioOutputI2S created successfully\n");
        _initialized = true;
        return true;
    }

    /**
     * Silence DAC pins to prevent electrical noise
     *
     * MUST be called in setup() before any RFID operations.
     * Sets DAC pins LOW to minimize electrical coupling with RFID MOSI.
     */
    void silenceDAC() {
        pinMode(pins::DAC_SILENCE_1, OUTPUT);
        pinMode(pins::DAC_SILENCE_2, OUTPUT);
        digitalWrite(pins::DAC_SILENCE_1, LOW);
        digitalWrite(pins::DAC_SILENCE_2, LOW);

        LOG_INFO("[AUDIO-HAL] DAC pins %d/%d set LOW (prevent noise)\n",
                 pins::DAC_SILENCE_1, pins::DAC_SILENCE_2);
    }

    /**
     * Start playback of WAV file
     *
     * Automatically triggers lazy initialization on first use.
     * Stops any existing playback before starting new file.
     *
     * Parameters:
     *   path - Full path to WAV file (e.g., "/AUDIO/token.wav")
     *
     * Returns: true if playback started successfully
     *
     * Note: SD card must be initialized and mounted before calling
     */
    bool play(const String& path) {
        LOG_DEBUG("[AUDIO-HAL] play() called for: %s\n", path.c_str());

        // Lazy initialization on first use
        if (!_initialized) {
            if (!begin()) {
                LOG_ERROR("AUDIO-HAL", "Lazy init failed");
                return false;
            }
        }

        // Stop any existing playback
        if (_generator && _generator->isRunning()) {
            LOG_DEBUG("[AUDIO-HAL] Stopping existing audio\n");
            stop();
        }

        LOG_INFO("[AUDIO-HAL] Playing: %s\n", path.c_str());

        // Open WAV file from SD card
        _source = new AudioFileSourceSD(path.c_str());
        if (!_source || !_source->isOpen()) {
            LOG_ERROR("AUDIO-HAL", "Failed to open audio file");
            delete _source;
            _source = nullptr;
            return false;
        }

        LOG_DEBUG("[AUDIO-HAL] Audio file opened successfully\n");

        // Create WAV generator
        _generator = new AudioGeneratorWAV();
        if (!_generator) {
            LOG_ERROR("AUDIO-HAL", "Failed to create AudioGeneratorWAV");
            delete _source;
            _source = nullptr;
            return false;
        }

        // Start playback
        if (!_generator->begin(_source, _output)) {
            LOG_ERROR("AUDIO-HAL", "WAV generator begin() failed");
            delete _generator;
            delete _source;
            _generator = nullptr;
            _source = nullptr;
            return false;
        }

        LOG_INFO("[AUDIO-HAL] Playback started successfully\n");
        LOG_DEBUG("[AUDIO-HAL] AudioOutputI2S at: %p\n", _output);
        return true;
    }

    /**
     * Stop audio playback
     *
     * Safe to call even if nothing is playing.
     * Cleans up generator and source objects.
     */
    void stop() {
        if (_generator && _generator->isRunning()) {
            LOG_DEBUG("[AUDIO-HAL] Stopping playback\n");
            _generator->stop();
        }

        // Clean up generator
        if (_generator) {
            delete _generator;
            _generator = nullptr;
        }

        // Clean up source
        if (_source) {
            delete _source;
            _source = nullptr;
        }
    }

    /**
     * Check if audio is currently playing
     *
     * Returns: true if audio is actively playing
     */
    bool isPlaying() const {
        return (_generator && _generator->isRunning());
    }

    /**
     * Process audio loop
     *
     * MUST be called frequently in main loop() for smooth playback.
     * If not called regularly, audio will stutter or stop.
     *
     * Automatically stops playback when audio finishes.
     */
    void loop() {
        if (_generator && _generator->isRunning()) {
            if (!_generator->loop()) {
                // Playback finished
                LOG_DEBUG("[AUDIO-HAL] Playback finished naturally\n");
                stop();
            }
        }
    }

private:
    // Singleton pattern
    AudioDriver() = default;
    ~AudioDriver() {
        stop();
        if (_output) {
            delete _output;
            _output = nullptr;
        }
    }
    AudioDriver(const AudioDriver&) = delete;
    AudioDriver& operator=(const AudioDriver&) = delete;

    // Audio subsystem state
    static AudioOutputI2S* _output;
    static AudioGeneratorWAV* _generator;
    static AudioFileSourceSD* _source;
    static bool _initialized;
};

// Static member initialization
AudioOutputI2S* AudioDriver::_output = nullptr;
AudioGeneratorWAV* AudioDriver::_generator = nullptr;
AudioFileSourceSD* AudioDriver::_source = nullptr;
bool AudioDriver::_initialized = false;

} // namespace hal
