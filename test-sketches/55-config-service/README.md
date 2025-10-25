# Test 55: ConfigService HAL Component

## Purpose

Test the ConfigService singleton component extracted from ALNScanner v4.1 monolithic code. Validates all configuration management functionality including loading, saving, validation, and runtime editing.

## Extracted Code

**Source:** `/home/maxepunk/projects/Arduino/ALNScanner1021_Orchestrator/ALNScanner1021_Orchestrator.ino`

**Lines Extracted:**
- Lines 1295-1320: `generateDeviceId()` → `ConfigService::generateDeviceId()`
- Lines 1323-1440: `parseConfigFile()` → `ConfigService::loadFromSD()`
- Lines 1443-1542: `validateConfig()` → `ConfigService::validate()`
- Lines 3157-3234: `SET_CONFIG` command → `ConfigService::set()`
- Lines 3236-3288: `SAVE_CONFIG` command → `ConfigService::saveToSD()`

**Target:** `/home/maxepunk/projects/Arduino/ALNScanner_v5/services/ConfigService.h`

## Features Tested

1. **Load Configuration** (`loadFromSD()`)
   - Opens `/config.txt` from SD card
   - Parses KEY=VALUE pairs
   - Skips comments and empty lines
   - Auto-generates device ID if not set
   - Validates required fields

2. **Save Configuration** (`saveToSD()`)
   - Writes all config fields to `/config.txt`
   - Overwrites existing file
   - Adds header comments
   - Thread-safe via hal::SDCard mutex

3. **Validate Configuration** (`validate()`)
   - Checks WIFI_SSID (1-32 characters, required)
   - Checks WIFI_PASSWORD (0-63 characters, optional)
   - Checks ORCHESTRATOR_URL (http://, 10-200 characters, required)
   - Checks TEAM_ID (exactly 3 digits, required)
   - Checks DEVICE_ID (1-100 characters, alphanumeric + underscore, optional)
   - Validates boolean flags (SYNC_TOKENS, DEBUG_MODE)

4. **Runtime Editing** (`set()`)
   - Updates configuration in memory
   - Supports all config keys
   - Boolean parsing (true/false, 1/0, case-insensitive)
   - Does not persist until `saveToSD()` called

5. **Device ID Generation** (`generateDeviceId()`)
   - Reads ESP32 MAC address from eFuse
   - Uses `esp_read_mac()` (works before WiFi init)
   - Formats as "SCANNER_XXXXXXXXXXXX" (12 hex digits)
   - Returns "SCANNER_ERROR" on failure

## Hardware Requirements

- **ESP32-2432S028R (CYD)** - Target device
- **SD Card** - Must be inserted with valid `config.txt` file

## Setup Instructions

### 1. Prepare SD Card

Copy `sample_config.txt` to SD card root as `config.txt`:

```bash
# On Raspberry Pi
cp sample_config.txt /media/user/SD_CARD/config.txt

# Or manually edit config.txt on SD card
```

Edit values as needed:
```ini
WIFI_SSID=YourNetwork
WIFI_PASSWORD=your_password
ORCHESTRATOR_URL=http://10.0.0.177:3000
TEAM_ID=001
```

### 2. Compile Sketch

```bash
cd /home/maxepunk/projects/Arduino/test-sketches/55-config-service

arduino-cli compile --fqbn esp32:esp32:esp32 .
```

**Expected Flash Usage:** < 400KB (target for modular HAL component)

### 3. Upload to Device

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .
```

### 4. Open Serial Monitor

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Serial Commands

| Command | Description | Example |
|---------|-------------|---------|
| `LOAD` | Load configuration from SD card | `LOAD` |
| `SAVE` | Save current configuration to SD card | `SAVE` |
| `SHOW` | Display current configuration | `SHOW` |
| `VALIDATE` | Validate current configuration | `VALIDATE` |
| `GENID` | Generate device ID from MAC | `GENID` |
| `SET:KEY=VAL` | Set configuration value (in-memory) | `SET:WIFI_SSID=MyNetwork` |
| `MEM` | Show memory usage | `MEM` |
| `HELP` | Show all commands | `HELP` |

### SET Command Keys

- `WIFI_SSID` - Network name (1-32 characters)
- `WIFI_PASSWORD` - Network password (0-63 characters)
- `ORCHESTRATOR_URL` - Server URL (must start with http://)
- `TEAM_ID` - Team identifier (exactly 3 digits)
- `DEVICE_ID` - Custom device identifier (1-100 characters)
- `SYNC_TOKENS` - Enable/disable token sync (true/false)
- `DEBUG_MODE` - Enable/disable debug features (true/false)

## Test Procedure

### Basic Load/Save Test

1. **Load config from SD card:**
   ```
   LOAD
   ```
   - Should show parsing progress
   - Should display all loaded values
   - Should auto-generate device ID if not set
   - Should validate required fields

2. **Display current config:**
   ```
   SHOW
   ```
   - Should show all configuration fields
   - Passwords should be masked as "***"

3. **Validate configuration:**
   ```
   VALIDATE
   ```
   - Should report validation results for each field
   - Should pass if all required fields present and valid

### Runtime Editing Test

4. **Update TEAM_ID:**
   ```
   SET:TEAM_ID=999
   ```
   - Should confirm update in memory
   - Should warn that changes not persisted

5. **Display updated config:**
   ```
   SHOW
   ```
   - Should show TEAM_ID=999

6. **Save to SD card:**
   ```
   SAVE
   ```
   - Should write config.txt to SD card
   - Should confirm success

7. **Reload to verify persistence:**
   ```
   LOAD
   SHOW
   ```
   - Should show TEAM_ID=999 (persisted change)

### Device ID Generation Test

8. **Generate device ID:**
   ```
   GENID
   ```
   - Should display device ID in format "SCANNER_XXXXXXXXXXXX"
   - Should show MAC address extraction logs
   - Should be consistent across multiple calls (same MAC)

9. **Set custom device ID:**
   ```
   SET:DEVICE_ID=SCANNER_FLOOR1_001
   SHOW
   ```
   - Should show custom device ID

### Validation Test

10. **Test invalid TEAM_ID:**
    ```
    SET:TEAM_ID=12
    VALIDATE
    ```
    - Should fail validation (TEAM_ID must be 3 digits)

11. **Test invalid ORCHESTRATOR_URL:**
    ```
    SET:ORCHESTRATOR_URL=https://example.com
    VALIDATE
    ```
    - Should fail validation (must start with http://, not https://)

12. **Fix configuration:**
    ```
    SET:TEAM_ID=001
    SET:ORCHESTRATOR_URL=http://192.168.1.100:3000
    VALIDATE
    ```
    - Should pass validation

### Memory Test

13. **Check memory usage:**
    ```
    MEM
    ```
    - Should show free heap, heap size, free PSRAM
    - Free heap should be > 200KB (plenty of headroom)

## Expected Results

### Compilation
- ✅ Compiles without errors
- ✅ Flash usage < 400KB
- ✅ No warnings about undefined references

### Runtime
- ✅ SD card initializes successfully
- ✅ Config loads from `/config.txt`
- ✅ All fields parse correctly
- ✅ Comments and empty lines ignored
- ✅ Device ID auto-generated if not set
- ✅ Validation catches invalid fields
- ✅ Runtime editing updates in-memory config
- ✅ Save persists changes to SD card
- ✅ Device ID generation produces correct format
- ✅ Memory usage is reasonable (free heap > 200KB)

### Error Handling
- ✅ Graceful failure if SD card missing
- ✅ Clear error messages for validation failures
- ✅ Unknown keys ignored during load
- ✅ Unknown keys rejected during set
- ✅ Thread-safe SD card access (no mutex timeout errors)

## Success Criteria

1. **Functionality:** All serial commands execute without errors
2. **Flash:** Sketch uses < 400KB program storage
3. **Memory:** Free heap > 200KB after initialization
4. **Validation:** Catches all invalid field formats
5. **Persistence:** Save/load cycle preserves all config values
6. **Thread Safety:** No SD mutex timeout errors
7. **Device ID:** Generation produces correct SCANNER_XXXXXXXXXXXX format

## Implementation Details

### Singleton Pattern
```cpp
auto& config = services::ConfigService::getInstance();
```

### RAII SD Card Locking
```cpp
hal::SDCard::Lock lock("ConfigService::loadFromSD");
if (lock.acquired()) {
    // SD operations here
}  // Lock automatically released
```

### Boolean Parsing
```cpp
// Accepts: "true", "1", "TRUE" → true
// Accepts: "false", "0", "FALSE" → false
bool value = !(str.equalsIgnoreCase("false") || str == "0");
```

### Device ID Generation
```cpp
uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Works before WiFi init
sprintf(deviceId, "SCANNER_%02X%02X%02X%02X%02X%02X", mac[0], ...);
```

## Integration with v5.0 Architecture

### Dependencies
- **hal::SDCard** - Thread-safe SD card access with mutex
- **models::DeviceConfig** - Configuration data structure
- **config.h** - Constants (limits, paths, logging macros)

### Used By
- **Application** - Main application class loads config on startup
- **SerialService** - Provides runtime config editing commands
- **OrchestratorService** - Uses config for WiFi and API connection
- **TokenService** - Uses config for sync enablement flag

### File Structure
```
ALNScanner_v5/
├── services/
│   └── ConfigService.h         # This component (200 lines)
├── models/
│   └── Config.h                # DeviceConfig struct
├── hal/
│   └── SDCard.h                # SD card HAL with mutex
└── config.h                    # Constants and limits
```

## Known Issues / Notes

1. **HTTPS Not Supported** - ORCHESTRATOR_URL must use `http://` (ESP32 HTTP client limitation)
2. **No MQTT/WebSocket** - Only HTTP/REST API supported in v4.1
3. **Buffer Overflow Fix** - v4.1 fixed device ID buffer overflow (20 → 32 bytes)
4. **WiFi Password Masking** - Passwords always displayed as "***" in logs
5. **Reboot Required** - Config changes require ESP32 restart to take effect in v4.1

## Version History

- **v1.0** (2025-10-22) - Initial extraction from v4.1, test sketch created

## Related Tests

- **Test 50: SDCard HAL** - Tests underlying SD card mutex and RAII lock
- **Test 56: SerialService** - Tests serial command interface (uses ConfigService)
- **Test 60: Application** - Tests full integration with all services

## References

- **CLAUDE.md** - Project documentation and architecture
- **REFACTOR_IMPLEMENTATION_GUIDE.md** - v5.0 refactor plan
- **ALNScanner1021_Orchestrator.ino** - v4.1 source code
- **sample_config.txt** - Example configuration file
