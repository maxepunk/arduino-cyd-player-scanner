#pragma once

/**
 * @file ScanResponse.h
 * @brief Pure classification of POST /api/scan responses.
 *
 * F-PARITY-03 / F-SCAN-03: the backend NEVER returns 409 for player-scan
 * duplicates (player duplicates are allowed by design). A 409 from
 * /api/scan is one of two very different things, distinguished by body:
 *
 *   {error:"SESSION_NOT_FOUND", message}   — no session exists; the scan
 *       was NOT persisted. Decision A5: FINAL failure — do NOT queue
 *       (the offline queue exists only for network-unreachable failures);
 *       the player rescans once a session exists.
 *
 *   {status:"rejected", message, tokenId, ...} — the scan WAS persisted;
 *       only the realtime video trigger was rejected (video busy or VLC
 *       down). Decisions A4/A5: final success — never requeue (a replayed
 *       scan must never start playback later); UI shows a brief
 *       "video unavailable" hint instead of the normal video treatment.
 *
 * A 409 with a malformed or unrecognized body is classified RETRY_QUEUE:
 * queueing is the safe default because player scans are duplicate-tolerant
 * server-side, while dropping could permanently lose a scan.
 *
 * Pure function — no I/O, no hardware. Tested in test/test_scan_response/.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

namespace services {

/**
 * Outcome of a single POST /api/scan attempt.
 */
enum class ScanOutcome {
    ACCEPTED,             ///< 2xx — scan recorded by the orchestrator
    REJECTED_NO_SESSION,  ///< 409 SESSION_NOT_FOUND — NOT recorded; FINAL (no queue, A5)
    ACCEPTED_NO_VIDEO,    ///< 409 status:rejected — recorded; video unavailable (A4)
    RETRY_QUEUE           ///< network failure / 5xx / unrecognized — queue for batch replay
};

/**
 * Classify an /api/scan HTTP response.
 *
 * @param code HTTP response code (negative for connection-level failures)
 * @param body Raw response body (may be empty or non-JSON)
 */
inline ScanOutcome classifyScanResponse(int code, const String& body) {
    if (code >= 200 && code < 300) {
        return ScanOutcome::ACCEPTED;
    }

    if (code == 409) {
        JsonDocument doc;
        if (deserializeJson(doc, body.c_str()) == DeserializationError::Ok) {
            const char* err = doc["error"] | "";
            if (strcmp(err, "SESSION_NOT_FOUND") == 0) {
                return ScanOutcome::REJECTED_NO_SESSION;
            }
            const char* status = doc["status"] | "";
            if (strcmp(status, "rejected") == 0) {
                return ScanOutcome::ACCEPTED_NO_VIDEO;
            }
        }
        // Malformed or unrecognized 409 body — cannot prove the scan was
        // persisted, so queue it (duplicates are allowed for player scans).
        return ScanOutcome::RETRY_QUEUE;
    }

    // Connection failures (<0), 4xx we don't recognize, 5xx, timeouts.
    return ScanOutcome::RETRY_QUEUE;
}

} // namespace services
