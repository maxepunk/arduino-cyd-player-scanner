#pragma once

#include <Arduino.h>
#include "../config.h"

namespace models {

// Token metadata structure - simplified for v5
// ALWAYS constructs paths from tokenId (ignores orchestrator metadata fields)
struct TokenMetadata {
    String tokenId;  // "kaa001" or UID hex - ALWAYS used for path construction
    String video;    // "kaa001.mp4" or "" - only used for behavior (modal vs persistent)

    TokenMetadata() = default;

    // Check if this is a video token (determines UI behavior only)
    bool isVideoToken() const {
        return (video.length() > 0 && video != "null");
    }

    // Clean tokenId for filesystem (remove special chars, convert to lowercase)
    // Matches v4.1 behavior: remove ":" and " ", then lowercase
    static String cleanTokenId(const String& rawTokenId) {
        String clean = rawTokenId;
        clean.replace(":", "");   // Remove colons (e.g., "04:A1:B2" → "04A1B2")
        clean.replace(" ", "");   // Remove spaces
        clean.trim();             // Remove leading/trailing whitespace
        clean.toLowerCase();      // Convert to lowercase for filesystem
        return clean;
    }

    // Get image path for display
    // ALWAYS constructs from tokenId: /assets/images/{cleanTokenId}.bmp
    String getImagePath() const {
        return String(paths::IMAGES_DIR) + cleanTokenId(tokenId) + ".bmp";
    }

    // Get audio path for playback
    // ALWAYS constructs from tokenId: /assets/audio/{cleanTokenId}.wav
    String getAudioPath() const {
        return String(paths::AUDIO_DIR) + cleanTokenId(tokenId) + ".wav";
    }

    // Print metadata (for debugging)
    void print() const {
        Serial.println("\n--- Token Metadata ---");
        Serial.printf("Token ID: %s\n", tokenId.c_str());
        Serial.printf("Video: %s\n", video.length() > 0 ? video.c_str() : "(none)");
        Serial.printf("Image Path: %s\n", getImagePath().c_str());
        Serial.printf("Audio Path: %s\n", getAudioPath().c_str());
        Serial.printf("Is Video Token: %s\n", isVideoToken() ? "YES" : "NO");
        Serial.println("----------------------\n");
    }
};

// Scan data structure for queue/orchestrator
struct ScanData {
    String tokenId;     // Required
    String teamId;      // Optional
    String deviceId;    // Required
    String deviceType;  // Required (P2.3: "esp32" for hardware scanners)
    String timestamp;   // Required (ISO 8601-ish format)

    ScanData() : deviceType("esp32") {}  // P2.3: Default to "esp32" for hardware scanners

    ScanData(const String& token, const String& team, const String& device, const String& ts)
        : tokenId(token), teamId(team), deviceId(device), deviceType("esp32"), timestamp(ts) {}

    // Validate required fields
    bool isValid() const {
        return (tokenId.length() > 0 &&
                deviceId.length() > 0 &&
                deviceType.length() > 0 &&
                timestamp.length() > 0);
    }

    // Print scan data (for debugging)
    void print() const {
        Serial.println("\n--- Scan Data ---");
        Serial.printf("Token ID: %s\n", tokenId.c_str());
        Serial.printf("Team ID: %s\n", teamId.length() > 0 ? teamId.c_str() : "(none)");
        Serial.printf("Device ID: %s\n", deviceId.c_str());
        Serial.printf("Device Type: %s\n", deviceType.c_str());  // P2.3
        Serial.printf("Timestamp: %s\n", timestamp.c_str());
        Serial.println("-----------------\n");
    }
};

} // namespace models
