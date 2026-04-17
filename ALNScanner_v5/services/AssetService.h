#pragma once

/**
 * @file AssetService.h
 * @brief Wireless BMP/audio asset sync for the CYD scanner.
 *
 * At boot (after token sync) the scanner fetches a small manifest describing
 * every image and audio file the backend currently ships, diffs it against
 * a locally-cached copy, and streams only the changed files to SD via
 * `OrchestratorService::httpGETStreamToSD`. Orphaned local files (tokens
 * removed from Notion) are pruned.
 *
 * Single source of truth: `aln-memory-scanner/assets/`. Nothing else needs
 * to be pre-copied to the SD card beyond a minimal bootstrap set — see
 * 'arduino-cyd-player-scanner/CLAUDE.md'.
 *
 * Resumability is per-file. The local manifest is only updated after a
 * byte-perfect write (see `httpGETStreamToSD`), so a power loss or WiFi
 * drop mid-sync always leaves the device in a recoverable state: the next
 * boot re-diffs and retries whatever wasn't committed.
 *
 * Heap budget: manifest JSON ≤ 128 KB (plus ArduinoJson doc overhead ~2x).
 * Streaming download buffer 4 KB. SHA-1 context ~100 bytes. All downloads
 * run under the SD mutex — safe at boot because the queue-upload task has
 * nothing to process yet.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <functional>
#include <vector>
#include "../hal/SDCard.h"
#include "../config.h"
#include "AssetManifestDiff.h"
#include "OrchestratorService.h"

namespace services {

class AssetService {
public:
    struct ProgressInfo {
        String tokenId;
        String type;           // "image" or "audio"
        int fileIndex;         // 1-based
        int fileCount;         // total files in this sync
        size_t bytesDone;      // current file
        size_t bytesTotal;     // current file
    };

    // Invoked synchronously on the main task whenever sync progresses. Used
    // to drive the boot-screen progress UI. Never invoked from a background
    // task so no locking is required on the callback side.
    using ProgressCallback = std::function<void(const ProgressInfo&)>;

    static AssetService& getInstance() {
        static AssetService instance;
        return instance;
    }

    AssetService(const AssetService&) = delete;
    AssetService& operator=(const AssetService&) = delete;

    void setProgressCallback(ProgressCallback cb) { _onProgress = cb; }

    /**
     * @brief Sync all BMP/audio assets from the orchestrator.
     *
     * Steps:
     *   1. GET /api/assets/manifest → small JSON describing the canonical
     *      asset set.
     *   2. Load our local manifest (if present).
     *   3. Queue every file whose sha1 differs (or is missing locally).
     *   4. For each queued file, stream to SD with hash/size verification;
     *      update local manifest on success.
     *   5. Delete local files whose tokenId is no longer in the remote
     *      manifest.
     *
     * Any individual-file failure (HTTP error, SHA mismatch, SD write
     * failure) is logged and skipped; the rest of the queue continues.
     * Returns true if every queued file succeeded.
     */
    bool syncFromOrchestrator(const String& orchestratorURL, OrchestratorService& orch) {
        Serial.println("\n[ASSET-SVC] >>> ASSET SYNC START <<<");
        Serial.printf("[ASSET-SVC] Free heap: %d bytes\n", ESP.getFreeHeap());

        if (orchestratorURL.length() == 0) {
            Serial.println("[ASSET-SVC] Orchestrator URL not set, skipping.");
            return false;
        }

        // Step 1: fetch the remote manifest (a small JSON payload, safe to
        // buffer via the existing httpGETWithRetry path).
        String body;
        int code = orch.httpGETWithRetry(
            orchestratorURL + "/api/assets/manifest", 15000, "asset manifest fetch", body);
        if (code != 200) {
            Serial.printf("[ASSET-SVC] Manifest fetch failed (HTTP %d). Aborting.\n", code);
            return false;
        }
        if ((int)body.length() > limits::MAX_MANIFEST_SIZE) {
            Serial.printf("[ASSET-SVC] Manifest too large (%u > %d)\n",
                          body.length(), limits::MAX_MANIFEST_SIZE);
            return false;
        }

        // ArduinoJson DynamicJsonDocument sized for the manifest payload
        // plus the copy overhead (roughly 2x for duplicated keys + tree
        // nodes). If this ever pressures heap, move to stream-parsing.
        DynamicJsonDocument remoteDoc(limits::MAX_MANIFEST_SIZE * 2);
        DeserializationError err = deserializeJson(remoteDoc, body);
        if (err) {
            Serial.printf("[ASSET-SVC] Manifest parse failed: %s\n", err.c_str());
            return false;
        }
        body = String(); // free the buffered copy ASAP

        // Step 2: read whatever local manifest already exists. Missing or
        // corrupt = empty, which forces a full re-sync.
        DynamicJsonDocument localDoc(limits::MAX_MANIFEST_SIZE * 2);
        _loadLocalManifest(localDoc);

        // Step 3: build the download queue. We flatten both asset types
        // into a single list so the progress counter is meaningful to the
        // operator ("12 / 147" rather than "12 / 130 images + 0 / 3
        // audio"). Logic lives in AssetManifestDiff.h so native tests can
        // exercise it without SD/WiFi deps.
        std::vector<manifest::Pending> pending = manifest::diff(remoteDoc, localDoc);

        Serial.printf("[ASSET-SVC] Queue: %u file(s) to download\n",
                      (unsigned)pending.size());

        // Step 4: download each queued file; commit local manifest on
        // each success so the next boot resumes from wherever we stopped.
        int successCount = 0;
        int failCount = 0;
        const int total = (int)pending.size();
        for (int i = 0; i < total; i++) {
            const auto& p = pending[i];
            String destPath = _buildPath(p.type, p.tokenId, p.ext);
            String url = orchestratorURL + "/api/assets/" +
                         (p.type == "image" ? "images" : "audio") + "/" +
                         p.tokenId + "." + (p.type == "image" ? "bmp" : p.ext);

            Serial.printf("[ASSET-SVC] (%d/%d) %s %s\n",
                          i + 1, total, p.type.c_str(), p.tokenId.c_str());

            ProgressInfo info{p.tokenId, p.type, i + 1, total, 0, p.size};
            if (_onProgress) _onProgress(info);

            auto streamProgress = [&](size_t done, size_t totalBytes) {
                if (!_onProgress) return;
                info.bytesDone = done;
                info.bytesTotal = totalBytes;
                _onProgress(info);
            };

            bool ok = orch.httpGETStreamToSD(
                url, destPath, p.size, p.sha1, 60000, streamProgress);
            if (!ok) {
                failCount++;
                continue;
            }

            // Commit this entry to the local manifest immediately. Cheap
            // per-file writes are acceptable here — manifest is small and
            // this is a boot-time operation.
            _updateLocalEntry(localDoc, p.type, p.tokenId, p.sha1, p.size,
                              p.type == "audio" ? p.ext.c_str() : nullptr);
            _writeLocalManifestAtomic(localDoc);
            successCount++;
        }

        // Step 5: orphan pruning. Anything in localDoc that isn't in
        // remoteDoc refers to a token no longer in Notion — delete both
        // the SD file and the local manifest entry.
        int prunedCount = _pruneOrphans(remoteDoc, localDoc);

        Serial.printf("[ASSET-SVC] <<< ASSET SYNC END: %d ok, %d fail, %d pruned >>>\n\n",
                      successCount, failCount, prunedCount);
        return failCount == 0;
    }

private:
    AssetService() = default;
    ProgressCallback _onProgress;

    // ─── Helpers ───────────────────────────────────────────────────────

    // Read /assets/manifest.json into the caller-owned JsonDocument; zero
    // the doc on any failure so downstream logic sees an empty local state.
    void _loadLocalManifest(DynamicJsonDocument& doc) {
        doc.clear();
        hal::SDCard::Lock lock("AssetService::loadManifest",
                               freertos_config::SD_MUTEX_TIMEOUT_MS);
        if (!lock.acquired()) return;

        if (!SD.exists(paths::MANIFEST_FILE)) {
            Serial.println("[ASSET-SVC] No local manifest yet (first sync).");
            return;
        }
        File f = SD.open(paths::MANIFEST_FILE, FILE_READ);
        if (!f) return;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (err) {
            Serial.printf("[ASSET-SVC] Local manifest corrupt (%s), treating as empty.\n",
                          err.c_str());
            doc.clear();
        }
    }

    // Write the doc to a temp file then rename, so a power loss between
    // write and rename leaves the previous manifest intact.
    void _writeLocalManifestAtomic(const DynamicJsonDocument& doc) {
        hal::SDCard::Lock lock("AssetService::writeManifest",
                               freertos_config::SD_MUTEX_TIMEOUT_MS);
        if (!lock.acquired()) return;

        SD.remove(paths::MANIFEST_TEMP_FILE); // no-op if absent
        File f = SD.open(paths::MANIFEST_TEMP_FILE, FILE_WRITE);
        if (!f) {
            Serial.println("[ASSET-SVC] Could not open manifest tmp for write");
            return;
        }
        serializeJson(doc, f);
        f.flush();
        f.close();

        SD.remove(paths::MANIFEST_FILE);
        if (!SD.rename(paths::MANIFEST_TEMP_FILE, paths::MANIFEST_FILE)) {
            Serial.println("[ASSET-SVC] Manifest rename failed");
        }
    }

    // Merge a freshly-downloaded entry into the in-memory local doc. Keys
    // are auto-created; existing entries are overwritten.
    void _updateLocalEntry(DynamicJsonDocument& local, const String& type,
                           const String& tokenId, const String& sha1,
                           size_t size, const char* ext) {
        const char* section = (type == "image") ? "images" : "audio";
        JsonObject sectionObj = local[section].is<JsonObject>()
            ? local[section].as<JsonObject>()
            : local.createNestedObject(section);
        JsonObject entry = sectionObj.createNestedObject(tokenId);
        entry["sha1"] = sha1;
        entry["size"] = size;
        if (ext && *ext) entry["ext"] = ext;
    }

    // Remove SD files and local manifest entries whose tokenId no longer
    // exists remotely. Returns count removed.
    int _pruneOrphans(const JsonDocument& remote, DynamicJsonDocument& local) {
        int pruned = 0;
        pruned += _pruneSection(remote, local, "images", "image");
        pruned += _pruneSection(remote, local, "audio", "audio");
        if (pruned > 0) _writeLocalManifestAtomic(local);
        return pruned;
    }

    int _pruneSection(const JsonDocument& remote, DynamicJsonDocument& local,
                      const char* sectionKey, const char* type) {
        if (!local[sectionKey].is<JsonObject>()) return 0;
        JsonObject section = local[sectionKey].as<JsonObject>();
        JsonObjectConst remoteSection = remote[sectionKey].as<JsonObjectConst>();

        std::vector<String> toRemove;
        manifest::collectOrphans(section, remoteSection, toRemove);
        if (toRemove.empty()) return 0;

        hal::SDCard::Lock lock("AssetService::prune",
                               freertos_config::SD_MUTEX_LONG_TIMEOUT_MS);
        if (!lock.acquired()) return 0;

        for (const auto& tokenId : toRemove) {
            const char* extStr = section[tokenId]["ext"] | "";
            String destPath = _buildPath(type, tokenId, extStr);
            SD.remove(destPath.c_str()); // no-op if absent
            section.remove(tokenId);
            Serial.printf("[ASSET-SVC] Pruned orphan %s %s\n", type, tokenId.c_str());
        }
        return (int)toRemove.size();
    }

    String _buildPath(const String& type, const String& tokenId, const String& ext) const {
        return manifest::buildPath(type, tokenId, ext);
    }
};

} // namespace services
