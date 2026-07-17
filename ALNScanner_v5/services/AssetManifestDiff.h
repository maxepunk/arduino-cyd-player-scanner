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

// Insert or upsert an entry in the local manifest doc. `ext` is optional
// (audio only). Removes any prior entry before creating to avoid the
// duplicate-key trap (ArduinoJson createNestedObject APPENDS, never
// upserts). Repairs a corrupt section (existing key with wrong type)
// by removing and recreating it.
inline void updateEntry(JsonDocument& local,
                        const String& type,
                        const String& tokenId,
                        const String& sha1,
                        size_t size,
                        const char* ext) {
    const char* section = (type == "image") ? "images" : "audio";

    if (!local[section].is<JsonObject>()) {
        local.remove(section);
        local.createNestedObject(section);
    }
    JsonObject sectionObj = local[section].as<JsonObject>();

    sectionObj.remove(tokenId);  // critical: prevents duplicate keys
    JsonObject entry = sectionObj.createNestedObject(tokenId);
    entry["sha1"] = sha1;
    entry["size"] = size;
    if (ext && *ext) entry["ext"] = ext;
}

// ─── Pack identity (Phase 3 A2 staleness visibility) ──────────────────

// Parsed from the manifest's optional top-level `pack` block. All fields
// EMPTY when the block is absent — callers assign unconditionally so a
// re-sync against a pack-less manifest (backend rollback, different
// orchestrator) CLEARS any previously-recorded identity instead of
// letting a stale one masquerade as current.
struct PackIdentity {
    String packId;
    String version;
    String hash;
};

inline PackIdentity extractPackIdentity(const JsonDocument& remoteDoc) {
    PackIdentity id;
    if (remoteDoc.containsKey("pack")) {
        id.packId  = remoteDoc["pack"]["packId"]      | "";
        id.version = remoteDoc["pack"]["version"]     | "";
        id.hash    = remoteDoc["pack"]["contentHash"] | "";
    }
    return id;
}

// Short display form of a content hash for boot-log eyeballing: strips an
// optional "algo:" prefix (e.g. "sha256:"), then takes the first 8 chars.
// Degrades gracefully on plain hex or short strings — never slices into
// the middle of a hash the way a fixed-offset substring would.
inline String shortHash(const String& hash) {
    int colon = hash.indexOf(':');
    String hex = (colon >= 0) ? hash.substring(colon + 1) : hash;
    return hex.length() > 8 ? hex.substring(0, 8) : hex;
}

} // namespace manifest
} // namespace services
