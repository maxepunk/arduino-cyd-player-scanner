#pragma once

#include <Arduino.h>
#include "../config.h"

namespace models {

// Token metadata structure - simplified for v5
// ALWAYS constructs paths from tokenId (ignores orchestrator metadata fields)
struct TokenMetadata {
    String tokenId;  // "ghost01" (NDEF text; UID-hex fallback removed in v5) - ALWAYS used for path construction

    TokenMetadata() = default;

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
        Serial.printf("Image Path: %s\n", getImagePath().c_str());
        Serial.printf("Audio Path: %s\n", getAudioPath().c_str());
        Serial.println("----------------------\n");
    }
};

} // namespace models
