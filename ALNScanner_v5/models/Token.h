#pragma once

#include <Arduino.h>
#include "../config.h"

namespace models {

// Token metadata structure (from v4.1 lines 182-189)
struct TokenMetadata {
    String tokenId;           // "kaa001" or UID hex
    String video;             // "kaa001.mp4" or "" for non-video
    String image;             // "images/kaa001.bmp"
    String audio;             // "audio/kaa001.wav"
    String processingImage;   // "images/kaa001_processing.jpg" (orchestrator path)

    TokenMetadata() = default;

    // Check if this is a video token
    bool isVideoToken() const {
        return (video.length() > 0 && video != "null");
    }

    // Get image path for display
    String getImagePath() const {
        if (image.length() > 0 && image != "null") {
            // Ensure path starts with /
            if (image.startsWith("/")) {
                return image;
            } else {
                return "/" + image;
            }
        }
        // Fallback: construct from tokenId
        return String(paths::IMAGES_DIR) + tokenId + ".bmp";
    }

    // Get audio path for playback
    String getAudioPath() const {
        if (audio.length() > 0 && audio != "null") {
            // Ensure path starts with /
            if (audio.startsWith("/")) {
                return audio;
            } else {
                return "/" + audio;
            }
        }
        // Fallback: construct from tokenId
        return String(paths::AUDIO_DIR) + tokenId + ".wav";
    }

    // Get processing image path for video tokens
    String getProcessingImagePath() const {
        if (processingImage.length() > 0 && processingImage != "null") {
            // Extract file extension from processingImage field
            String extension = ".bmp";  // Default
            int lastDot = processingImage.lastIndexOf('.');
            if (lastDot >= 0) {
                extension = processingImage.substring(lastDot);
            }

            // Construct SD card path from tokenId
            return String(paths::IMAGES_DIR) + tokenId + extension;
        }
        // Fallback: use regular image
        return getImagePath();
    }

    // Print metadata (for debugging)
    void print() const {
        Serial.println("\n--- Token Metadata ---");
        Serial.printf("Token ID: %s\n", tokenId.c_str());
        Serial.printf("Video: %s\n", video.length() > 0 ? video.c_str() : "(none)");
        Serial.printf("Image: %s\n", image.c_str());
        Serial.printf("Audio: %s\n", audio.c_str());
        Serial.printf("Processing Image: %s\n", processingImage.c_str());
        Serial.printf("Is Video Token: %s\n", isVideoToken() ? "YES" : "NO");
        Serial.println("----------------------\n");
    }
};

// Scan data structure for queue/orchestrator
struct ScanData {
    String tokenId;     // Required
    String teamId;      // Optional
    String deviceId;    // Required
    String timestamp;   // Required (ISO 8601-ish format)

    ScanData() = default;

    ScanData(const String& token, const String& team, const String& device, const String& ts)
        : tokenId(token), teamId(team), deviceId(device), timestamp(ts) {}

    // Validate required fields
    bool isValid() const {
        return (tokenId.length() > 0 &&
                deviceId.length() > 0 &&
                timestamp.length() > 0);
    }

    // Print scan data (for debugging)
    void print() const {
        Serial.println("\n--- Scan Data ---");
        Serial.printf("Token ID: %s\n", tokenId.c_str());
        Serial.printf("Team ID: %s\n", teamId.length() > 0 ? teamId.c_str() : "(none)");
        Serial.printf("Device ID: %s\n", deviceId.c_str());
        Serial.printf("Timestamp: %s\n", timestamp.c_str());
        Serial.println("-----------------\n");
    }
};

} // namespace models
