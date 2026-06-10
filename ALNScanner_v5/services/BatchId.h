#pragma once

/**
 * @file BatchId.h
 * @brief Pure batch-id construction for /api/scan/batch idempotency.
 *
 * F-PARITY-02 / F-SCAN-02: the backend caches processed batchIds for one
 * hour. The old scheme ({deviceID}_{counter}, counter reset to 0 each boot)
 * meant a reboot within that hour reused an already-processed id — the
 * backend returned the cached result without processing, the device deleted
 * its queue entries, and the scans were silently lost.
 *
 * Fix: include a per-boot nonce so ids never repeat across reboots:
 *
 *     {deviceId}_{bootNonce as zero-padded 8-hex}_{counter}
 *
 * Idempotency semantics (both must hold):
 *  - SAME id across HTTP retries of one batch: the caller passes the same
 *    (nonce, counter) pair until the upload succeeds — counter is only
 *    advanced AFTER a successful upload.
 *  - DIFFERENT id for every distinct batch, including the first batch after
 *    a reboot (fresh nonce).
 *
 * Pure function — no I/O, no hardware. Tested in test/test_batch_id/.
 * The nonce itself is seeded by the caller (esp_random() on device).
 */

#include <Arduino.h>
#include <cstdio>

namespace services {

inline String makeBatchId(const String& deviceId, uint32_t bootNonce, uint32_t counter) {
    // Fixed-width hex nonce keeps the three segments unambiguous.
    char suffix[24];  // "_xxxxxxxx_4294967295" = 20 chars + NUL
    snprintf(suffix, sizeof(suffix), "_%08lx_%lu",
             static_cast<unsigned long>(bootNonce),
             static_cast<unsigned long>(counter));
    return deviceId + suffix;
}

} // namespace services
