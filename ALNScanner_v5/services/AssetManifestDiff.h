#pragma once

/**
 * @file AssetManifestDiff.h
 * @brief Pure-logic manifest diffing used by AssetService.
 *
 * Kept in its own header so PlatformIO native tests can exercise the diff
 * and path-construction behaviour without pulling in SD/WiFi/mbedtls.
 * AssetService.h includes this header.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "../config.h"

namespace services {
namespace manifest {

// One file pending download. `ext` is empty for images (always ".bmp").
struct Pending {
    String type;       // "image" or "audio"
    String tokenId;
    String sha1;
    size_t size;
    String ext;
};

// Append every (tokenId -> {sha1, size, ext?}) entry from `remoteSection`
// whose hash doesn't match the matching entry in `localSection` (or which
// is missing locally entirely). Skips entries missing required fields.
inline void diffSection(JsonObjectConst remoteSection,
                        JsonObjectConst localSection,
                        const char* type,
                        std::vector<Pending>& out) {
    for (JsonPairConst kv : remoteSection) {
        const char* tokenId = kv.key().c_str();
        const char* remoteSha = kv.value()["sha1"] | "";
        size_t remoteSize = kv.value()["size"] | 0;
        const char* remoteExt = kv.value()["ext"] | "";
        if (!remoteSha[0] || remoteSize == 0) continue;

        const char* localSha = "";
        if (!localSection.isNull() && localSection.containsKey(tokenId)) {
            localSha = localSection[tokenId]["sha1"] | "";
        }
        if (strcmp(localSha, remoteSha) == 0) continue;

        Pending p;
        p.type = type;
        p.tokenId = tokenId;
        p.sha1 = remoteSha;
        p.size = remoteSize;
        p.ext = remoteExt;
        out.push_back(p);
    }
}

// Convenience: diff both sections in canonical (images-first) order so
// the pending list index is stable for progress UI.
inline std::vector<Pending> diff(const JsonDocument& remote,
                                 const JsonDocument& local) {
    std::vector<Pending> out;
    diffSection(remote["images"].as<JsonObjectConst>(),
                local["images"].as<JsonObjectConst>(),
                "image", out);
    diffSection(remote["audio"].as<JsonObjectConst>(),
                local["audio"].as<JsonObjectConst>(),
                "audio", out);
    return out;
}

// Collect local entries whose tokenId is not in the remote manifest.
inline void collectOrphans(JsonObject localSection,
                           JsonObjectConst remoteSection,
                           std::vector<String>& outIds) {
    for (JsonPair kv : localSection) {
        if (remoteSection.isNull() || !remoteSection.containsKey(kv.key())) {
            outIds.emplace_back(kv.key().c_str());
        }
    }
}

// SD path for a given asset entry. Matches AssetService::_buildPath.
inline String buildPath(const String& type, const String& tokenId, const String& ext) {
    if (type == "image") {
        return String(paths::IMAGES_DIR) + tokenId + ".bmp";
    }
    return String(paths::AUDIO_DIR) + tokenId + "." + (ext.length() ? ext : String("wav"));
}

} // namespace manifest
} // namespace services
