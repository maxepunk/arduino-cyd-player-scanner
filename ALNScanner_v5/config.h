#pragma once

// PPP HARDWARE CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

// Pin Definitions (CYD ESP32-2432S028R)
namespace pins {
    // SD Card (VSPI - Hardware SPI Bus 2)
    constexpr uint8_t SD_SCK   = 18;
    constexpr uint8_t SD_MISO  = 19;
    constexpr uint8_t SD_MOSI  = 23;
    constexpr uint8_t SD_CS    = 5;

    // Touch Controller (XPT2046)
    constexpr uint8_t TOUCH_CS  = 33;
    constexpr uint8_t TOUCH_IRQ = 36;

    // RFID Reader (Software SPI - MFRC522)
    constexpr uint8_t RFID_SCK  = 22;
    constexpr uint8_t RFID_MOSI = 27;
    constexpr uint8_t RFID_MISO = 35;
    constexpr uint8_t RFID_SS   = 3;

    // Audio (I2S DAC)
    constexpr uint8_t AUDIO_BCLK = 26;
    constexpr uint8_t AUDIO_LRC  = 25;
    constexpr uint8_t AUDIO_DIN  = 22;

    // DAC Silence Pins (prevent beeping)
    constexpr uint8_t DAC_SILENCE_1 = 25;
    constexpr uint8_t DAC_SILENCE_2 = 26;
}

// PPP TIMING CONSTANTS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace timing {
    constexpr uint32_t RFID_SCAN_INTERVAL_MS = 500;
    constexpr uint32_t TOUCH_DEBOUNCE_MS = 50;
    constexpr uint32_t DOUBLE_TAP_TIMEOUT_MS = 500;
    constexpr uint32_t TOUCH_PULSE_WIDTH_THRESHOLD_US = 10000;
    constexpr uint32_t PROCESSING_MODAL_TIMEOUT_MS = 2500;
    constexpr uint32_t SCAN_FAILED_TIMEOUT_MS = 1500;  // Non-blocking failure screen auto-dismiss
    constexpr uint32_t ORCHESTRATOR_CHECK_INTERVAL_MS = 10000;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
    constexpr uint32_t DEBUG_OVERRIDE_TIMEOUT_MS = 30000;
}

// PPP RFID CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace rfid_config {
    // Retry parameters (used by detectCard() and extractNDEFText() retry loops)
    constexpr uint8_t MAX_RETRIES = 3;
    constexpr uint8_t RETRY_DELAY_MS = 100;      // Community-standard for NTAG state recovery
    constexpr uint8_t ANTENNA_SETTLE_MS = 5;     // Settling time after RF field enable

    // Low-level SPI/operation timing (unchanged)
    constexpr uint8_t OPERATION_DELAY_US = 10;
    constexpr uint8_t TIMEOUT_MS = 100;
    constexpr uint8_t CLOCK_DELAY_US = 2;
}

// PPP QUEUE CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace queue_config {
    constexpr int MAX_QUEUE_SIZE = 100;
    constexpr int BATCH_UPLOAD_SIZE = 10;
    constexpr unsigned long MAX_QUEUE_FILE_SIZE = 102400;  // 100KB - corruption detection threshold
    constexpr const char* QUEUE_FILE = "/queue.jsonl";
    constexpr const char* QUEUE_TEMP_FILE = "/queue.tmp";
}

// PPP FILE PATHS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace paths {
    constexpr const char* CONFIG_FILE = "/config.txt";
    constexpr const char* TOKEN_DB_FILE = "/tokens.json";
    constexpr const char* DEVICE_ID_FILE = "/device_id.txt";
    constexpr const char* IMAGES_DIR = "/assets/images/";
    constexpr const char* AUDIO_DIR = "/assets/audio/";
    // Asset manifest tracks sha1/size per synced file; updated atomically
    // after each successful download (stream to .part, rename on success).
    constexpr const char* MANIFEST_FILE = "/assets/manifest.json";
    constexpr const char* MANIFEST_TEMP_FILE = "/assets/manifest.tmp";
    constexpr const char* PART_SUFFIX = ".part";
}

// PPP SIZE LIMITS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace limits {
    constexpr int MAX_TOKENS = 50;
    constexpr int MAX_TOKEN_DB_SIZE = 50000; // 50KB
    // Wire-format ceiling for the manifest payload. Device refuses anything
    // larger to avoid pathological-input OOM. Tied to the parent CLAUDE.md
    // "ESP32 Asset Sync Issues" debug step #5.
    constexpr int MAX_MANIFEST_SIZE = 131072; // 128KB

    // Heap budget for parsed JsonDocument. 64KB covers ~600 entries at the
    // observed ~80 bytes/entry; if a future manifest grows past this we abort
    // with a diagnostic rather than risk OOM. Two docs (remote+local) are
    // live during diff, so total live heap = 2 * MANIFEST_DOC_SIZE.
    constexpr int MANIFEST_DOC_SIZE = 65536; // 64KB
    // Streaming download buffer sized to balance TCP window utilization
    // against heap pressure (TLS session ~22 KB, file I/O overhead, SHA
    // context). 4 KB chunks are the standard Espressif streaming example.
    constexpr int ASSET_DOWNLOAD_CHUNK_SIZE = 4096;
    // Per-file asset download timeout passed to httpGETStreamToSD. The
    // SD_MUTEX_LONG_TIMEOUT_MS static_assert below ensures the mutex wait
    // timeout outlasts this so mid-session re-sync (Task 14) doesn't
    // spuriously fail on lock acquisition.
    constexpr int ASSET_DOWNLOAD_TIMEOUT_MS = 60000;
    // Per-file streaming abort threshold used by httpGETStreamToSD;
    // separate from the manifest-parse pre-flight in AssetService.
    constexpr int ASSET_MIN_FREE_HEAP = 40960; // 40KB
    constexpr int MAX_DEVICE_ID_LENGTH = 100;
    constexpr int TEAM_ID_LENGTH = 3;
    constexpr int MAX_SSID_LENGTH = 32;
    constexpr int MAX_PASSWORD_LENGTH = 63;
}

// PPP FREERTOS CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace freertos_config {
    constexpr uint32_t BACKGROUND_TASK_STACK_SIZE = 16384; // 16KB
    constexpr uint8_t BACKGROUND_TASK_PRIORITY = 1;
    constexpr uint8_t BACKGROUND_TASK_CORE = 0;
    constexpr uint32_t BACKGROUND_TASK_DELAY_MS = 100;
    constexpr uint32_t SD_MUTEX_TIMEOUT_MS = 500;
    // Long-form mutex acquire used by operations that hold the SD card for
    // extended periods (asset streaming downloads up to 60 s per file).
    // Must outlast the longest holder — see static_assert below.
    constexpr uint32_t SD_MUTEX_LONG_TIMEOUT_MS = 60000;
}

// AssetService::httpGETStreamToSD holds the SD mutex for an entire
// download (up to 60s per file in the default config). The long-timeout
// mutex must outlast that, otherwise every download aborts on lock
// acquisition. Encoded as a compile-time constraint so a future tweak
// to either constant can't silently break asset sync.
static_assert(freertos_config::SD_MUTEX_LONG_TIMEOUT_MS >= limits::ASSET_DOWNLOAD_TIMEOUT_MS,
              "SD_MUTEX_LONG_TIMEOUT_MS must outlast ASSET_DOWNLOAD_TIMEOUT_MS "
              "so mid-session asset sync doesn't fail on mutex acquisition");

// PPP DEBUG CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

// Compile-time debug flags (reduces flash in production)
#ifndef DEBUG_MODE
  #define LOG_VERBOSE(...) ((void)0)
  #define LOG_DEBUG(...) ((void)0)
#else
  #define LOG_VERBOSE(...) Serial.printf(__VA_ARGS__)
  #define LOG_DEBUG(...) Serial.printf(__VA_ARGS__)
#endif

// NDEF diagnostic logging — enable even in production builds
// Serial TX still works when RFID is active (only RX is killed by GPIO 3)
// Uncomment to capture byte-level NDEF data during game sessions:
// #define NDEF_DEBUG

#ifdef NDEF_DEBUG
  #define LOG_NDEF(...) Serial.printf(__VA_ARGS__)
#else
  #define LOG_NDEF(...) ((void)0)
#endif

// Always compile info/error logs
#define LOG_INFO(...) Serial.printf(__VA_ARGS__)
#define LOG_ERROR(tag, msg) logError(F(tag), F(msg))

// Error logging helper (inline to avoid code duplication)
inline void logError(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
    Serial.print(F("[ERROR] "));
    Serial.print(tag);
    Serial.print(F(": "));
    Serial.println(msg);
}
