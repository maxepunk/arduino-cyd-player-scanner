# Contract: Configuration File Format

**Feature**: 003-orchestrator-hardware-integration
**File**: `/config.txt` on SD card
**Date**: 2025-10-19
**Status**: Contract Definition

## Purpose

Define the exact format and validation rules for the scanner configuration file that Game Masters edit before gameplay.

---

## Format Specification

### File Location

- **Path**: `/config.txt` (root of SD card FAT32 filesystem)
- **Created by**: Game Master using any text editor (computer/laptop)
- **Read by**: Scanner on boot
- **Modified**: By GM removing SD card, editing on computer, and reinserting (requires scanner reboot)

### File Format

- **Encoding**: UTF-8 or ASCII
- **Line Endings**: CR+LF (Windows) or LF (Unix) both supported
- **Structure**: Key=value pairs, one per line
- **Comments**: NOT supported
- **Sections**: NOT supported
- **Empty Lines**: Ignored (skipped during parsing)
- **Whitespace**: Trimmed from keys and values

### Syntax

```
KEY=VALUE
```

- **KEY**: Alphanumeric + underscore, case-sensitive
- **VALUE**: Any printable characters (no newlines)
- **Separator**: Exactly one `=` character (first occurrence)
- **Multiple `=` in value**: Allowed (e.g., `ORCHESTRATOR_URL=http://example.com?param=value`)

---

## Required Fields

### WIFI_SSID

**Description**: WiFi network name to connect to

**Validation**:
- Length: 1-32 characters (ESP32 WiFi SSID limit)
- Characters: Any printable characters
- Empty value: Invalid (scanner will display error)

**Example**:
```
WIFI_SSID=VenueNetwork
```

---

### WIFI_PASSWORD

**Description**: WiFi network password (WPA/WPA2)

**Validation**:
- Length: 0-63 characters (WPA2 password limit)
- Characters: Any printable characters
- Empty value: Valid (for open WiFi networks without password)

**Example**:
```
WIFI_PASSWORD=secretpassword123
```

**Open Network Example**:
```
WIFI_PASSWORD=
```

---

### ORCHESTRATOR_URL

**Description**: Base URL of orchestrator server

**Validation**:
- Must start with: `http://` (HTTPS NOT supported)
- Format: `http://{IP_OR_HOSTNAME}:{PORT}`
- IP address or hostname allowed
- Port required if not 80
- No trailing slash
- Length: 10-200 characters

**Valid Examples**:
```
ORCHESTRATOR_URL=http://10.0.0.100:3000
ORCHESTRATOR_URL=http://192.168.1.50:3000
ORCHESTRATOR_URL=http://orchestrator.local:3000
```

**Invalid Examples**:
```
ORCHESTRATOR_URL=https://10.0.0.100:3000  # HTTPS not supported
ORCHESTRATOR_URL=http://10.0.0.100:3000/  # Trailing slash
ORCHESTRATOR_URL=10.0.0.100:3000          # Missing http://
```

---

### TEAM_ID

**Description**: Team identifier for scan logging

**Validation**:
- Pattern: `^[0-9]{3}$` (exactly 3 digits)
- Range: 000-999
- Leading zeros required (e.g., `001`, not `1`)

**Valid Examples**:
```
TEAM_ID=001
TEAM_ID=042
TEAM_ID=999
```

**Invalid Examples**:
```
TEAM_ID=1        # Must be 3 digits
TEAM_ID=1234     # Too many digits
TEAM_ID=A01      # Not numeric
```

---

## Optional Fields

### DEVICE_ID

**Description**: Custom device identifier (auto-generated if omitted)

**Validation**:
- Pattern: `^[A-Za-z0-9_]+$` (alphanumeric + underscore only)
- Length: 1-100 characters
- If omitted: Scanner generates ID from MAC address (format: `SCANNER_A1B2C3D4E5F6`)

**Example**:
```
DEVICE_ID=SCANNER_01
DEVICE_ID=GM_SCANNER_ALPHA
DEVICE_ID=PLAYER_SCANNER_BLUE
```

**Auto-Generated Example** (if field omitted):
```
# Generated: SCANNER_A1B2C3D4E5F6
```

---

## Complete Example

```
WIFI_SSID=AboutLastNightVenue
WIFI_PASSWORD=gamenight2025
ORCHESTRATOR_URL=http://10.0.0.100:3000
TEAM_ID=001
DEVICE_ID=SCANNER_ALPHA
```

## Minimal Example (auto-generated device ID)

```
WIFI_SSID=VenueWiFi
WIFI_PASSWORD=password123
ORCHESTRATOR_URL=http://192.168.1.50:3000
TEAM_ID=002
```

---

## Parser Behavior

### Parsing Algorithm

```cpp
void parseConfigFile() {
  File file = SD.open("/config.txt", FILE_READ);
  if (!file) {
    displayError("Config Error: No config.txt");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim(); // Remove whitespace and line endings

    if (line.length() == 0) continue; // Skip empty lines

    int separatorIndex = line.indexOf('=');
    if (separatorIndex == -1) continue; // Skip lines without '='

    String key = line.substring(0, separatorIndex);
    String value = line.substring(separatorIndex + 1);

    key.trim();
    value.trim();

    // Store key-value pair
    config[key] = value;
  }

  file.close();

  // Validate required fields
  validateConfig();
}
```

### Validation Sequence

1. **File Exists**: Check if `/config.txt` exists on SD card
2. **Parse Lines**: Read line by line, extract key=value pairs
3. **Required Fields**: Check all required fields present
4. **Field Validation**: Validate each field against its rules
5. **Error Display**: If validation fails, display specific error message
6. **Fallback Mode**: If config invalid, continue in offline-only mode (no network functionality)

---

## Error Handling

### Missing File

**Error**: File `/config.txt` not found on SD card

**Display**: `Config Error: No config.txt`

**Behavior**: Scanner continues in offline-only mode (no WiFi, no orchestrator)

---

### Missing Required Field

**Error**: Required field absent from config file

**Display**: `Config Error: Missing {FIELD_NAME}`

**Behavior**: Scanner continues in offline-only mode

**Example**:
```
Config Error: Missing WIFI_SSID
```

---

### Invalid Field Value

**Error**: Field value doesn't match validation rules

**Display**: `Config Error: Invalid {FIELD_NAME}`

**Behavior**: Scanner continues in offline-only mode

**Examples**:
```
Config Error: Invalid WIFI_SSID (length > 32)
Config Error: Invalid TEAM_ID (must be 3 digits)
Config Error: Invalid ORCHESTRATOR_URL (must start with http://)
```

---

### Parse Error

**Error**: Line format malformed (no `=` separator)

**Behavior**: Skip line silently, log to serial output

**Serial Log**:
```
Config: Skipping invalid line (no separator): WIFI_SSIDVenueNetwork
```

---

## Test Cases

### Test Case 1: Valid Minimal Configuration

**Input**:
```
WIFI_SSID=TestNetwork
WIFI_PASSWORD=pass123
ORCHESTRATOR_URL=http://10.0.0.1:3000
TEAM_ID=001
```

**Expected**: Configuration parsed successfully, all fields valid

---

### Test Case 2: Valid with Optional Device ID

**Input**:
```
WIFI_SSID=TestNetwork
WIFI_PASSWORD=pass123
ORCHESTRATOR_URL=http://10.0.0.1:3000
TEAM_ID=042
DEVICE_ID=SCANNER_TEST_01
```

**Expected**: Configuration parsed successfully, custom device ID used

---

### Test Case 3: Open WiFi Network

**Input**:
```
WIFI_SSID=PublicWiFi
WIFI_PASSWORD=
ORCHESTRATOR_URL=http://192.168.1.100:3000
TEAM_ID=005
```

**Expected**: Configuration parsed successfully, empty password valid

---

### Test Case 4: Missing Required Field

**Input**:
```
WIFI_SSID=TestNetwork
WIFI_PASSWORD=pass123
TEAM_ID=001
```

**Expected**: Error - "Config Error: Missing ORCHESTRATOR_URL"

---

### Test Case 5: Invalid TEAM_ID Format

**Input**:
```
WIFI_SSID=TestNetwork
WIFI_PASSWORD=pass123
ORCHESTRATOR_URL=http://10.0.0.1:3000
TEAM_ID=1
```

**Expected**: Error - "Config Error: Invalid TEAM_ID (must be 3 digits)"

---

### Test Case 6: Invalid URL Protocol

**Input**:
```
WIFI_SSID=TestNetwork
WIFI_PASSWORD=pass123
ORCHESTRATOR_URL=https://10.0.0.1:3000
TEAM_ID=001
```

**Expected**: Error - "Config Error: Invalid ORCHESTRATOR_URL (must start with http://)"

---

### Test Case 7: Extra Whitespace

**Input**:
```
WIFI_SSID = TestNetwork
WIFI_PASSWORD=  pass123
ORCHESTRATOR_URL=http://10.0.0.1:3000
TEAM_ID=001
```

**Expected**: Configuration parsed successfully, whitespace trimmed

---

### Test Case 8: Empty Lines and Comments Attempt

**Input**:
```
WIFI_SSID=TestNetwork

WIFI_PASSWORD=pass123
# This is a comment (will be skipped, not an error)
ORCHESTRATOR_URL=http://10.0.0.1:3000
TEAM_ID=001
```

**Expected**: Configuration parsed successfully, empty lines and comment line skipped

---

## Game Master Instructions (included in quickstart.md)

### How to Edit Configuration

1. **Remove SD card** from scanner (power off first)
2. **Insert SD card** into computer (SD card reader)
3. **Open `/config.txt`** with any text editor:
   - Windows: Notepad, Notepad++, VS Code
   - Mac: TextEdit (plain text mode), VS Code
   - Linux: nano, vi, gedit, VS Code
4. **Edit values** for your venue WiFi and orchestrator server:
   - WIFI_SSID: Your venue's WiFi network name
   - WIFI_PASSWORD: Your venue's WiFi password
   - ORCHESTRATOR_URL: Your orchestrator IP and port (e.g., `http://10.0.0.100:3000`)
   - TEAM_ID: Team number (001, 002, etc.)
   - DEVICE_ID: (optional) Custom scanner name
5. **Save file** (keep file name as `config.txt`)
6. **Eject SD card** safely from computer
7. **Insert SD card** back into scanner
8. **Power on scanner** (configuration read on boot)
9. **Check display** for "Connected ✓" or error message

### Common Mistakes to Avoid

- ❌ Renaming file to `config.txt.txt` (Windows hides extensions by default)
- ❌ Using HTTPS instead of HTTP for orchestrator URL
- ❌ Adding trailing slash to orchestrator URL (`http://10.0.0.1:3000/`)
- ❌ Using 1 or 2 digits for TEAM_ID (must be exactly 3 digits: `001`, not `1`)
- ❌ Including spaces in ORCHESTRATOR_URL (`http:// 10.0.0.1:3000` is invalid)
- ❌ Forgetting to save file after editing

---

**Contract Complete**: 2025-10-19
**Version**: 1.0
