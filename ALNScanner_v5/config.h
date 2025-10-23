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
    constexpr uint32_t ORCHESTRATOR_CHECK_INTERVAL_MS = 10000;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
    constexpr uint32_t DEBUG_OVERRIDE_TIMEOUT_MS = 30000;
}

// PPP RFID CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace rfid_config {
    constexpr uint8_t MAX_RETRIES = 3;
    constexpr uint8_t RETRY_DELAY_MS = 20;
    constexpr uint8_t OPERATION_DELAY_US = 10;
    constexpr uint8_t TIMEOUT_MS = 100;
    constexpr uint8_t CLOCK_DELAY_US = 2;
}

// PPP QUEUE CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace queue_config {
    constexpr int MAX_QUEUE_SIZE = 100;
    constexpr int BATCH_UPLOAD_SIZE = 10;
    constexpr const char* QUEUE_FILE = "/queue.jsonl";
    constexpr const char* QUEUE_TEMP_FILE = "/queue.tmp";
}

// PPP FILE PATHS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace paths {
    constexpr const char* CONFIG_FILE = "/config.txt";
    constexpr const char* TOKEN_DB_FILE = "/tokens.json";
    constexpr const char* DEVICE_ID_FILE = "/device_id.txt";
    constexpr const char* IMAGES_DIR = "/images/";
    constexpr const char* AUDIO_DIR = "/AUDIO/";
}

// PPP SIZE LIMITS PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

namespace limits {
    constexpr int MAX_TOKENS = 50;
    constexpr int MAX_TOKEN_DB_SIZE = 50000; // 50KB
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
    constexpr uint32_t SD_MUTEX_LONG_TIMEOUT_MS = 2000;
}

// PPP DEBUG CONFIGURATION PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP

// Compile-time debug flags (reduces flash in production)
#ifndef DEBUG_MODE
  #define LOG_VERBOSE(...) ((void)0)
  #define LOG_DEBUG(...) ((void)0)
#else
  #define LOG_VERBOSE(...) Serial.printf(__VA_ARGS__)
  #define LOG_DEBUG(...) Serial.printf(__VA_ARGS__)
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
