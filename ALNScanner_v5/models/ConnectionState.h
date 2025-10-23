#pragma once

#include <freertos/semphr.h>
#include "../config.h"

namespace models {

// Connection state enum (from v4.1 lines 141-148)
enum ConnectionState {
    ORCH_DISCONNECTED = 0,      // WiFi down
    ORCH_WIFI_CONNECTED = 1,    // WiFi up, orchestrator unknown/down
    ORCH_CONNECTED = 2          // WiFi + orchestrator healthy
};

// Thread-safe connection state holder
class ConnectionStateHolder {
public:
    ConnectionStateHolder() {
        _mutex = xSemaphoreCreateMutex();
        _state = ORCH_DISCONNECTED;
    }

    ~ConnectionStateHolder() {
        if (_mutex) {
            vSemaphoreDelete(_mutex);
        }
    }

    // Get current state (thread-safe)
    ConnectionState get() const {
        if (!_mutex) return _state;

        xSemaphoreTake(_mutex, portMAX_DELAY);
        ConnectionState state = _state;
        xSemaphoreGive(_mutex);

        return state;
    }

    // Set state (thread-safe)
    void set(ConnectionState newState) {
        if (!_mutex) {
            _state = newState;
            return;
        }

        xSemaphoreTake(_mutex, portMAX_DELAY);
        _state = newState;
        xSemaphoreGive(_mutex);
    }

    // Check if connected to orchestrator
    bool isConnected() const {
        return get() == ORCH_CONNECTED;
    }

    // Check if WiFi connected
    bool hasWiFi() const {
        ConnectionState state = get();
        return (state == ORCH_WIFI_CONNECTED || state == ORCH_CONNECTED);
    }

private:
    SemaphoreHandle_t _mutex;
    ConnectionState _state;

    // Prevent copying
    ConnectionStateHolder(const ConnectionStateHolder&) = delete;
    ConnectionStateHolder& operator=(const ConnectionStateHolder&) = delete;
};

// Helper function to convert state to string
inline const char* connectionStateToString(ConnectionState state) {
    switch (state) {
        case ORCH_DISCONNECTED:     return "DISCONNECTED";
        case ORCH_WIFI_CONNECTED:   return "WIFI_CONNECTED";
        case ORCH_CONNECTED:        return "CONNECTED";
        default:                    return "UNKNOWN";
    }
}

} // namespace models
