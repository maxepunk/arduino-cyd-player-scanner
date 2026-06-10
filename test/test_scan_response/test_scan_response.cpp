#include <unity.h>
#include <Arduino.h>
#include "services/ScanResponse.h"

// Unity requires setUp/tearDown (even if empty)
void setUp(void) {}
void tearDown(void) {}

using services::ScanOutcome;
using services::classifyScanResponse;

// ─── 2xx: scan recorded ────────────────────────────────────────────────

void test_200_is_accepted() {
    ScanOutcome o = classifyScanResponse(200,
        "{\"status\":\"accepted\",\"videoQueued\":true,\"tokenId\":\"kaa001\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::ACCEPTED), static_cast<int>(o));
}

void test_202_is_accepted() {
    // Backend offline-queue path returns 202 {status:'queued'}
    ScanOutcome o = classifyScanResponse(202, "{\"status\":\"queued\",\"offlineMode\":true}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::ACCEPTED), static_cast<int>(o));
}

void test_2xx_accepted_even_with_empty_body() {
    ScanOutcome o = classifyScanResponse(204, "");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::ACCEPTED), static_cast<int>(o));
}

// ─── 409 SESSION_NOT_FOUND: scan NOT recorded — FINAL failure ──────────
//
// F-PARITY-03 / F-SCAN-03 + Decision A5: the backend answers 409
// {error:'SESSION_NOT_FOUND'} when no session exists. The scan was NOT
// persisted. Treating this as success (old behavior) silently dropped
// every pre-session scan. Per A5 it is FINAL: do not queue — rescan once
// a session exists.

void test_409_session_not_found_is_final_rejection() {
    ScanOutcome o = classifyScanResponse(409,
        "{\"error\":\"SESSION_NOT_FOUND\","
        "\"message\":\"No active session - admin must create session first\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::REJECTED_NO_SESSION), static_cast<int>(o));
}

// ─── 409 video-rejected: scan WAS recorded — final success ─────────────
//
// Backend persists the scan, then rejects only the video trigger
// (video busy or VLC down): 409 {status:'rejected', ...}. Decision A4/A5:
// final success, never requeued (a replayed scan must never start video
// playback later); UI shows a "video unavailable" hint.

void test_409_video_busy_is_accepted_no_video() {
    ScanOutcome o = classifyScanResponse(409,
        "{\"status\":\"rejected\",\"message\":\"Video already playing, please wait\","
        "\"tokenId\":\"kaa001\",\"mediaAssets\":{},\"videoQueued\":false,\"waitTime\":30}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::ACCEPTED_NO_VIDEO), static_cast<int>(o));
}

void test_409_vlc_down_is_accepted_no_video() {
    // vlc_down variant omits waitTime — classification must not depend on it
    ScanOutcome o = classifyScanResponse(409,
        "{\"status\":\"rejected\",\"message\":\"Video playback unavailable\","
        "\"tokenId\":\"kaa001\",\"mediaAssets\":{},\"videoQueued\":false}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::ACCEPTED_NO_VIDEO), static_cast<int>(o));
}

// ─── 409 with malformed/unknown body: retryable (queue) ────────────────
//
// If we cannot prove which 409 sub-case this is, queueing is the safe
// default: the batch path tolerates duplicates (player scans are
// duplicate-allowed), whereas dropping could lose a scan.

void test_409_empty_body_is_retryable() {
    ScanOutcome o = classifyScanResponse(409, "");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_409_non_json_body_is_retryable() {
    ScanOutcome o = classifyScanResponse(409, "<html>409 Conflict</html>");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_409_unknown_shape_is_retryable() {
    ScanOutcome o = classifyScanResponse(409, "{\"foo\":\"bar\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_409_unknown_error_code_is_retryable() {
    // A future/unknown error discriminator must not be misread as no-session
    ScanOutcome o = classifyScanResponse(409,
        "{\"error\":\"SOMETHING_ELSE\",\"message\":\"?\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_409_status_not_rejected_is_retryable() {
    ScanOutcome o = classifyScanResponse(409, "{\"status\":\"accepted\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

// ─── Network / server errors: retryable (queue) ────────────────────────

void test_connection_failure_is_retryable() {
    // HTTPClient returns negative codes for connection-level failures
    ScanOutcome o = classifyScanResponse(-1, "");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_500_is_retryable() {
    ScanOutcome o = classifyScanResponse(500, "{\"error\":\"INTERNAL\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_503_is_retryable() {
    ScanOutcome o = classifyScanResponse(503,
        "{\"error\":\"SERVICE_UNAVAILABLE\",\"message\":\"Server starting up\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

void test_404_is_retryable() {
    // TOKEN_NOT_FOUND is unreachable in practice (local DB gates the send);
    // on local/backend DB skew, queue it — batch replay reports per-item status.
    ScanOutcome o = classifyScanResponse(404,
        "{\"error\":\"TOKEN_NOT_FOUND\",\"message\":\"Token x not recognized\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(ScanOutcome::RETRY_QUEUE), static_cast<int>(o));
}

// ─── Test Runner ───────────────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_200_is_accepted);
    RUN_TEST(test_202_is_accepted);
    RUN_TEST(test_2xx_accepted_even_with_empty_body);
    RUN_TEST(test_409_session_not_found_is_final_rejection);
    RUN_TEST(test_409_video_busy_is_accepted_no_video);
    RUN_TEST(test_409_vlc_down_is_accepted_no_video);
    RUN_TEST(test_409_empty_body_is_retryable);
    RUN_TEST(test_409_non_json_body_is_retryable);
    RUN_TEST(test_409_unknown_shape_is_retryable);
    RUN_TEST(test_409_unknown_error_code_is_retryable);
    RUN_TEST(test_409_status_not_rejected_is_retryable);
    RUN_TEST(test_connection_failure_is_retryable);
    RUN_TEST(test_500_is_retryable);
    RUN_TEST(test_503_is_retryable);
    RUN_TEST(test_404_is_retryable);
    return UNITY_END();
}
