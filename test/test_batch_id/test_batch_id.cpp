#include <unity.h>
#include <Arduino.h>
#include "services/BatchId.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

// ─── makeBatchId format ────────────────────────────────────────────────
//
// F-PARITY-02 / F-SCAN-02: batchId must never repeat across reboots.
// The backend caches processed batchIds for 1 hour (idempotency); a reboot
// that resets the counter to 0 within that hour would reuse the old id and
// the backend would return the cached result WITHOUT processing — deleting
// the queue entries on-device and silently losing the scans.
//
// Format: {deviceId}_{bootNonce as 8-hex}_{counter}

void test_makeBatchId_format() {
    String id = services::makeBatchId("SCANNER_001", 0xA1B2C3D4u, 0);
    TEST_ASSERT_EQUAL_STRING("SCANNER_001_a1b2c3d4_0", id.c_str());
}

void test_makeBatchId_zero_pads_nonce_to_8_hex() {
    // Fixed-width nonce keeps segments unambiguous regardless of value
    String id = services::makeBatchId("d", 0x12u, 7);
    TEST_ASSERT_EQUAL_STRING("d_00000012_7", id.c_str());
}

// ─── Idempotency semantics ─────────────────────────────────────────────

void test_makeBatchId_stable_for_same_inputs() {
    // Same boot, same counter => identical id. This is what makes HTTP
    // retries of ONE batch idempotent on the backend (must be preserved).
    String a = services::makeBatchId("SCANNER_001", 0xDEADBEEFu, 3);
    String b = services::makeBatchId("SCANNER_001", 0xDEADBEEFu, 3);
    TEST_ASSERT_EQUAL_STRING(a.c_str(), b.c_str());
}

void test_makeBatchId_changes_with_counter() {
    // Successive batches within one boot must get fresh ids
    String a = services::makeBatchId("SCANNER_001", 0xDEADBEEFu, 0);
    String b = services::makeBatchId("SCANNER_001", 0xDEADBEEFu, 1);
    TEST_ASSERT_TRUE(a != b);
}

void test_makeBatchId_changes_with_boot_nonce() {
    // THE FIX: same counter value after a reboot (new nonce) must NOT
    // collide with the pre-reboot id — otherwise the backend idempotency
    // cache swallows the new batch (silent scan loss).
    String beforeReboot = services::makeBatchId("SCANNER_001", 0x11111111u, 0);
    String afterReboot  = services::makeBatchId("SCANNER_001", 0x22222222u, 0);
    TEST_ASSERT_TRUE(beforeReboot != afterReboot);
}

void test_makeBatchId_distinct_devices_never_collide() {
    String a = services::makeBatchId("SCANNER_A", 0x1u, 0);
    String b = services::makeBatchId("SCANNER_B", 0x1u, 0);
    TEST_ASSERT_TRUE(a != b);
}

void test_makeBatchId_large_counter() {
    String id = services::makeBatchId("S", 0xFFFFFFFFu, 4294967295u);
    TEST_ASSERT_EQUAL_STRING("S_ffffffff_4294967295", id.c_str());
}

// ─── Test Runner ───────────────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_makeBatchId_format);
    RUN_TEST(test_makeBatchId_zero_pads_nonce_to_8_hex);
    RUN_TEST(test_makeBatchId_stable_for_same_inputs);
    RUN_TEST(test_makeBatchId_changes_with_counter);
    RUN_TEST(test_makeBatchId_changes_with_boot_nonce);
    RUN_TEST(test_makeBatchId_distinct_devices_never_collide);
    RUN_TEST(test_makeBatchId_large_counter);
    return UNITY_END();
}
