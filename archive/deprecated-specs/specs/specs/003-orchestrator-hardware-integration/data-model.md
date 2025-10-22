# Data Model: Orchestrator Integration

**Feature**: 003-orchestrator-hardware-integration
**Date**: 2025-10-19
**Status**: Phase 1 Design

## Overview

This document defines the data entities, state machines, and validation rules for orchestrator integration on the ESP32 hardware scanner.

---

## Entity Definitions

### 1. Configuration

**Purpose**: Persistent settings for WiFi and orchestrator connection

**Storage**: SD card `/config.txt`

**Format**: Key=value pairs (one per line, no sections, no comments)

**Fields**:

| Field | Type | Required | Validation | Default | Description |
|-------|------|----------|------------|---------|-------------|
| `WIFI_SSID` | String | Yes | 1-32 characters | N/A | WiFi network name |
| `WIFI_PASSWORD` | String | Yes | 0-63 characters | N/A | WiFi password (empty string for open networks) |
| `ORCHESTRATOR_URL` | String | Yes | Valid HTTP URL | N/A | Orchestrator server base URL (e.g., `http://10.0.0.100:3000`) |
| `TEAM_ID` | String | Yes | Exactly 3 digits | N/A | Team identifier (e.g., `001`, `002`) |
| `DEVICE_ID` | String | No | 1-100 characters, alphanumeric+underscore | Auto-generated | Device identifier (e.g., `SCANNER_01`) |

**Lifecycle**:
- **Created**: By Game Master editing file on computer before inserting SD card
- **Read**: Once on scanner boot
- **Updated**: By GM removing SD card, editing, and reinserting (requires reboot)
- **Validated**: On read - scanner displays error if required fields missing or invalid

**Validation Rules**:
- `WIFI_SSID`: Length 1-32 (ESP32 WiFi SSID limit)
- `WIFI_PASSWORD`: Length 0-63 (WPA2 password limit, 0 for open networks)
- `ORCHESTRATOR_URL`: Must start with `http://` (HTTPS not supported per spec)
- `TEAM_ID`: Must match pattern `^[0-9]{3}$`
- `DEVICE_ID`: Must match pattern `^[A-Za-z0-9_]+$`, length 1-100

**Example**:
```
WIFI_SSID=VenueNetwork
WIFI_PASSWORD=secretpassword123
ORCHESTRATOR_URL=http://10.0.0.100:3000
TEAM_ID=001
DEVICE_ID=SCANNER_01
```

**Error Handling**:
- Missing required field → Display "Config Error: Missing [field]" + continue offline-only mode
- Invalid format → Display "Config Error: Invalid [field]" + continue offline-only mode
- File not found → Display "Config Error: No config.txt" + continue offline-only mode

---

### 2. Device Identifier

**Purpose**: Unique identifier for scanner device (used in all API requests)

**Storage**:
- Primary: Configuration file `DEVICE_ID` field (if GM configured)
- Secondary: SD card `/device_id.txt` (auto-generated if not configured)

**Generation Algorithm** (if not configured):
```cpp
String generateDeviceId() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char deviceId[20];
  sprintf(deviceId, "SCANNER_%02X%02X%02X%02X%02X%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(deviceId);
}
```

**Format**: "SCANNER_" + 12-character uppercase hex MAC address without separators

**Example**: `SCANNER_A1B2C3D4E5F6`

**Lifecycle**:
1. **Boot**: Check configuration file for `DEVICE_ID` field
2. **If absent**: Check SD card for `/device_id.txt`
3. **If absent**: Generate from MAC address
4. **Persist**: Save generated ID to `/device_id.txt`
5. **Use**: Include in all `/api/scan` and `/api/scan/batch` requests

**Validation**: 1-100 characters, alphanumeric + underscore only

---

### 3. Scan Request

**Purpose**: Represents a token scan sent to orchestrator (both video and non-video tokens)

**Network Transmission**: HTTP POST `/api/scan`

**Fields**:

| Field | Type | Required | Validation | Source | Description |
|-------|------|----------|------------|--------|-------------|
| `tokenId` | String | Yes | 1-100 chars, alphanumeric+underscore | RFID scan | Token identifier from card UID |
| `teamId` | String | No | Exactly 3 digits | Configuration | Team assignment from config.txt |
| `deviceId` | String | Yes | 1-100 chars, alphanumeric+underscore | Device ID entity | Unique scanner identifier |
| `timestamp` | String | Yes | ISO 8601 datetime | ESP32 RTC or uptime | Scan timestamp |

**JSON Payload Example**:
```json
{
  "tokenId": "534e2b03",
  "teamId": "001",
  "deviceId": "SCANNER_A1B2C3D4E5F6",
  "timestamp": "2025-10-19T14:30:00.000Z"
}
```

**Lifecycle**:
1. **Created**: When RFID card scanned
2. **Sent**: Immediately if orchestrator connected (fire-and-forget)
3. **Queued**: If orchestrator offline or send fails (timeout/error)
4. **Response**: Check HTTP status code only (200-299 = success, else queue)

**Validation**:
- All required fields present
- `tokenId`: Not empty, matches pattern `^[A-Za-z0-9_]+$`
- `teamId`: If present, matches pattern `^[0-9]{3}$`
- `deviceId`: Matches pattern `^[A-Za-z0-9_]+$`
- `timestamp`: Valid ISO 8601 format

**HTTP Response Codes**:
- `200 OK`: Scan accepted
- `409 Conflict`: Video already playing (acceptable, queue processed)
- `400 Bad Request`: Validation error (unexpected, queue for retry)
- `503 Service Unavailable`: Orchestrator offline queue full (rare, queue locally anyway)
- Timeout/network error: Queue immediately

**State Transitions**:
```
[Token Scanned]
    ↓
[Scan Request Created]
    ↓
[Connection Check]
    ↓
    ├─ [Connected] → [Send HTTP POST /api/scan]
    │                       ↓
    │                 [Check Status Code]
    │                       ↓
    │                       ├─ [200-299] → [Success] → [Done]
    │                       └─ [Other/Timeout] → [Queue Entry]
    │
    └─ [Offline] → [Queue Entry]
```

---

### 4. Queue Entry

**Purpose**: Represents a scan that couldn't be sent immediately (persisted for later upload)

**Storage**: SD card `/queue.jsonl` (JSON Lines format, one entry per line)

**Fields**: Same as Scan Request (tokenId, teamId, deviceId, timestamp)

**JSON Line Example**:
```json
{"tokenId":"534e2b03","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:30:00.000Z"}
```

**Lifecycle**:
1. **Created**: When scan request fails to send (offline or timeout)
2. **Appended**: To `/queue.jsonl` file on SD card (atomic line append)
3. **Persisted**: Through power cycles (survives reboot)
4. **Uploaded**: When connection restored, batch of up to 10 entries via `/api/scan/batch`
5. **Removed**: From queue file after successful batch upload (200 OK response)

**Queue Management**:
- **FIFO Order**: First in, first out
- **Maximum Size**: 100 entries
- **Overflow Handling**: Remove oldest entry (first line) when appending 101st entry

**File Operations**:
```cpp
// Append (when scan fails to send)
File file = SD.open("/queue.jsonl", FILE_APPEND);
file.println(jsonString); // Adds newline automatically
file.flush(); // Force write to SD card
file.close();

// Read (when uploading queue)
File file = SD.open("/queue.jsonl", FILE_READ);
while (file.available()) {
  String line = file.readStringUntil('\n');
  // Parse JSON line into Queue Entry
}
file.close();

// Remove uploaded entries (after successful batch)
// Read all lines, skip first N (uploaded), rewrite file with remaining
```

**Overflow Algorithm**:
```cpp
if (countLines("/queue.jsonl") >= 100) {
  // Read all lines into array
  // Remove first line (oldest)
  // Append new line
  // Rewrite entire file
}
```

**Validation**:
- Each line must be valid JSON
- Each JSON object must have required fields (tokenId, deviceId, timestamp)
- Invalid lines skipped during boot recovery (corruption tolerance)

**State Transitions**:
```
[Scan Request Failed]
    ↓
[Queue Entry Created]
    ↓
[Append to /queue.jsonl]
    ↓
[Persist to SD Card]
    ↓
[Wait for Connection]
    ↓
[Connection Restored]
    ↓
[Background Task] → [Batch Upload (up to 10 entries)]
    ↓
[HTTP POST /api/scan/batch]
    ↓
    ├─ [200 OK] → [Remove from Queue]
    └─ [Error/Timeout] → [Keep in Queue, Retry in 10s]
```

---

### 5. Connection State

**Purpose**: Runtime status of WiFi and orchestrator connectivity

**Storage**: RAM only (not persisted)

**States**:

| State | WiFi | Orchestrator | Behavior | Display Indicator |
|-------|------|--------------|----------|-------------------|
| `DISCONNECTED` | Not connected | N/A | Queue all scans, attempt WiFi connection every 30s | "WiFi Error - Check Config" |
| `WIFI_CONNECTED` | Connected | Not checked | Check orchestrator health on next interval | "Connecting..." |
| `CONNECTED` | Connected | Reachable | Send scans directly, upload queue | "Connected ✓" |
| `OFFLINE` | Connected | Unreachable | Queue all scans, check orchestrator every 10s | "Ready - Offline" |

**Transitions**:

```
[Boot]
    ↓
[DISCONNECTED]
    ↓
[WiFi.begin(ssid, password)]
    ↓
    ├─ [Success within 30s] → [WIFI_CONNECTED]
    │                              ↓
    │                         [Check /health endpoint]
    │                              ↓
    │                              ├─ [200 OK] → [CONNECTED]
    │                              └─ [Timeout/Error] → [OFFLINE]
    │
    └─ [Timeout] → [DISCONNECTED] → [Retry every 30s]

[CONNECTED]
    ↓
[Background Task: Check /health every 10s]
    ↓
    ├─ [200 OK] → [CONNECTED] (stay connected)
    └─ [Timeout/Error] → [OFFLINE]
        ↓
        [Background Task: Check /health every 10s]
        ↓
        ├─ [200 OK] → [CONNECTED] (reconnected)
        └─ [Timeout/Error] → [OFFLINE] (stay offline)

[WiFi Disconnection Event]
    ↓
[Any State] → [DISCONNECTED]
    ↓
[WiFi.reconnect()]
    ↓
[Retry every 10s via background task]
```

**Monitoring**:
- **WiFi**: Event-driven (`ARDUINO_EVENT_WIFI_STA_DISCONNECTED`) + 10s polling
- **Orchestrator**: 10-second interval health checks (HTTP GET `/health` with 5s timeout)

**RAM Representation**:
```cpp
enum ConnectionState {
  DISCONNECTED,     // WiFi not connected
  WIFI_CONNECTED,   // WiFi connected, orchestrator status unknown
  CONNECTED,        // WiFi + orchestrator both reachable
  OFFLINE           // WiFi connected, orchestrator unreachable
};

ConnectionState currentState = DISCONNECTED;
bool wifiConnected = false;
bool orchestratorReachable = false;
```

---

### 6. Token Metadata

**Purpose**: Information about memory tokens loaded from tokens.json database

**Source**:
- Primary: HTTP GET `/api/tokens` (optional sync on boot)
- Secondary: SD card `/tokens.json` (cached database, pre-loaded in SD card image)

**Storage**:
- **SD Card**: `/tokens.json` (full database, 50KB max)
- **RAM**: Parsed into memory on boot (filtered fields only to save RAM)

**Fields** (parsed from tokens.json):

| Field | Type | Required | Description | Used For |
|-------|------|----------|-------------|----------|
| `tokenId` | String | Yes | Token identifier (SF_RFID) | Lookup key |
| `video` | String or null | No | Video filename | Determine if token triggers video |
| `image` | String or null | No | Image path | Local content display |
| `audio` | String or null | No | Audio path | Local content playback |
| `processingImage` | String or null | No | Processing image path | "Sending..." modal display |

**Example Entry in tokens.json**:
```json
{
  "534e2b03": {
    "image": null,
    "audio": null,
    "video": "test_30sec.mp4",
    "processingImage": "534e2b03.jpg",
    "SF_RFID": "534e2b03",
    "SF_ValueRating": 3,
    "SF_MemoryType": "Technical",
    "SF_Group": ""
  }
}
```

**Filtered Fields** (loaded into RAM):
- Only parse: `tokenId`, `video`, `image`, `audio`, `processingImage`
- Ignore: `SF_*` fields (game metadata not needed by hardware scanner)

**Lifecycle**:
1. **Boot**: Check if orchestrator reachable
2. **Sync** (optional): HTTP GET `/api/tokens` → Save to `/tokens.json` (overwrite cache)
3. **Fallback**: If sync fails, load from existing `/tokens.json` cache
4. **Parse**: Load JSON into RAM with filtered fields (ArduinoJson deserialize with filter)
5. **Lookup**: When token scanned, check `tokens[tokenId]` for video/image/audio/processingImage

**Sync Behavior**:
```
[Boot]
    ↓
[Connection State == CONNECTED]
    ↓
    ├─ [Yes] → [HTTP GET /api/tokens]
    │              ↓
    │              ├─ [200 OK] → [Save to /tokens.json] → [Parse into RAM]
    │              └─ [Timeout/Error] → [Load from /tokens.json cache]
    │
    └─ [No] → [Load from /tokens.json cache]
        ↓
        ├─ [File exists] → [Parse into RAM]
        └─ [File missing] → [Display "No Token Database - Limited Functionality"]
```

**Validation**:
- File size < 50KB (fits in ESP32 RAM for parsing)
- Valid JSON structure with `tokens` object
- Each token has at least `tokenId` field
- Invalid tokens skipped during parsing (partial database acceptable)

---

### 7. Processing Image

**Purpose**: Visual content shown during "Sending..." modal for video tokens

**Source**:
- Token metadata `processingImage` field (path relative to SD card root)
- SD card `/images/` folder (same location as regular token images)

**Storage**: SD card only (not loaded into RAM, displayed on-demand)

**Lifecycle**:
1. **Token Scanned**: Lookup token in metadata
2. **Check Video**: If token has `video` field (non-null)
3. **Check Processing Image**: If token has `processingImage` field
4. **Display**: Load BMP from SD card path, display during "Sending..." modal (max 2.5 seconds)
5. **Fallback**: If file missing or load fails, display "Sending..." text with token ID

**Display Behavior**:
```
[Token Scanned]
    ↓
[Lookup Token in Metadata]
    ↓
    ├─ [Has video field] → [Video Token]
    │       ↓
    │       [Check processingImage field]
    │       ↓
    │       ├─ [Present] → [Display "Sending..." modal with processing image]
    │       │                 ↓
    │       │            [Send scan request in background]
    │       │                 ↓
    │       │            [Auto-hide after 2.5s or send complete]
    │       │
    │       └─ [Absent] → [Display "Sending..." modal with token ID text]
    │
    └─ [No video field] → [Non-Video Token]
        ↓
        [Display local image/audio, no processing image needed]
```

**File Path Resolution**:
- Token metadata: `"processingImage": "534e2b03.jpg"`
- SD card path: `/images/534e2b03.jpg`
- BMP display: Same renderer as regular token images (avoid SPI deadlock per constitution)

---

## Data Relationships

### Entity Relationship Diagram

```
Configuration (SD: /config.txt)
    ↓ (provides WiFi credentials, orchestrator URL, team ID)
Connection State (RAM)
    ↓ (determines routing for scan requests)
Scan Request (Network/Queue)
    ↓ (fails to send)
Queue Entry (SD: /queue.jsonl)
    ↓ (batched for upload)
    ↓ (HTTP POST /api/scan/batch)
[Orchestrator API]

Token Metadata (SD: /tokens.json, synced from /api/tokens)
    ↓ (looked up by tokenId)
    ↓ (provides video/image/audio/processingImage)
Processing Image (SD: /images/*)
    ↓ (displayed during "Sending..." modal)
```

### Data Flow for Token Scan

```
1. [RFID Scan] → tokenId
2. [Lookup tokenId] → Token Metadata (video/image/audio/processingImage)
3. [Create Scan Request] → tokenId + teamId (from Configuration) + deviceId (from Device Identifier) + timestamp
4. [Check Connection State]
    ├─ [CONNECTED] → [Send HTTP POST /api/scan]
    │                    ↓
    │                [Check HTTP status]
    │                    ↓
    │                    ├─ [200-299] → [Success, done]
    │                    └─ [Other/Timeout] → [Create Queue Entry]
    │
    └─ [OFFLINE/DISCONNECTED] → [Create Queue Entry]
5. [Display Content]
    ├─ [Video Token] → [Processing Image during "Sending..."] → [Return to ready mode]
    └─ [Non-Video Token] → [Local image/audio from SD card]
```

### Data Flow for Queue Sync

```
1. [Background Task: 10s interval]
2. [Check Connection State]
    ↓
    [CONNECTED] + [Queue not empty]
    ↓
3. [Read /queue.jsonl] → Load up to 10 Queue Entries
4. [HTTP POST /api/scan/batch] → transactions array
5. [Check HTTP status]
    ├─ [200 OK] → [Remove uploaded entries from /queue.jsonl]
    │                ↓
    │            [Queue has more?]
    │                ↓
    │                ├─ [Yes] → [Wait 1 second] → [Upload next batch]
    │                └─ [No] → [Done]
    │
    └─ [Error/Timeout] → [Keep in queue, retry in 10s]
```

---

## State Machines

### Scanner Operational State Machine

```
[BOOT]
    ↓
[Initialize Hardware] (TFT, SD, RFID, Touch)
    ↓
[Read Configuration] (/config.txt)
    ↓
    ├─ [Success] → [Initialize WiFi]
    │                  ↓
    │              [WiFi Connection Attempt]
    │                  ↓
    │                  ├─ [Success] → [CONNECTED/OFFLINE] (check orchestrator)
    │                  └─ [Timeout] → [DISCONNECTED] (retry every 30s)
    │
    └─ [Failure] → [OFFLINE_ONLY Mode] (no network)
        ↓
[READY State] (idle, waiting for scan)
    ↓
[Token Scanned]
    ↓
[PROCESSING State]
    ↓
    ├─ [Video Token] → [Display Processing Image + "Sending..."] → [Send/Queue]
    └─ [Non-Video Token] → [Display Local Image/Audio] → [Send/Queue]
    ↓
[READY State] (return to idle)
```

### Connection Recovery State Machine

```
[WiFi Disconnection Event]
    ↓
[DISCONNECTED State]
    ↓
[Background Task: Attempt Reconnection]
    ↓
[WiFi.reconnect()]
    ↓
    ├─ [Success] → [WIFI_CONNECTED]
    │                  ↓
    │              [Check Orchestrator /health]
    │                  ↓
    │                  ├─ [200 OK] → [CONNECTED] → [Upload Queue]
    │                  └─ [Timeout] → [OFFLINE] → [Wait 10s, retry]
    │
    └─ [Timeout] → [DISCONNECTED] → [Wait 10s, retry]
```

---

## Validation Rules Summary

### Configuration Validation

| Field | Rule | Error Message |
|-------|------|---------------|
| `WIFI_SSID` | 1-32 characters | "Config Error: Invalid WiFi SSID" |
| `WIFI_PASSWORD` | 0-63 characters | "Config Error: Invalid WiFi Password" |
| `ORCHESTRATOR_URL` | Starts with `http://` | "Config Error: Invalid Orchestrator URL" |
| `TEAM_ID` | Pattern: `^[0-9]{3}$` | "Config Error: Invalid Team ID (must be 3 digits)" |
| `DEVICE_ID` | Pattern: `^[A-Za-z0-9_]+$`, 1-100 chars | "Config Error: Invalid Device ID" |

### Scan Request Validation

| Field | Rule | Error Handling |
|-------|------|----------------|
| `tokenId` | Not empty, pattern: `^[A-Za-z0-9_]+$` | Log error, skip scan |
| `teamId` | If present, pattern: `^[0-9]{3}$` | Omit field if invalid, log warning |
| `deviceId` | Pattern: `^[A-Za-z0-9_]+$`, 1-100 chars | System error (should never fail) |
| `timestamp` | Valid ISO 8601 format | System error (should never fail) |

### Queue Entry Validation

| Aspect | Rule | Error Handling |
|--------|------|----------------|
| Line format | Valid JSON object | Skip invalid line (corruption recovery) |
| Required fields | tokenId, deviceId, timestamp present | Skip entry missing required fields |
| File size | ≤100 lines | FIFO overflow (remove oldest) |

### Token Metadata Validation

| Aspect | Rule | Error Handling |
|--------|------|----------------|
| File size | <50KB | Display "Token Database Too Large" |
| JSON structure | Has `tokens` object | Display "Invalid Token Database" |
| Token entries | At least `tokenId` field | Skip invalid tokens, load partial database |

---

## Memory Considerations

### RAM Usage Estimates

| Entity | Storage Location | Estimated RAM |
|--------|------------------|---------------|
| Configuration | Parsed into RAM on boot | ~300 bytes (5 strings) |
| Device ID | RAM string | ~20 bytes |
| Connection State | Enum + booleans | ~4 bytes |
| Token Metadata | RAM (filtered fields) | ~10KB (50 tokens × 5 fields × 40 bytes avg) |
| Scan Request (transient) | RAM during send/queue | ~300 bytes |
| Queue Metadata (cached) | RAM (queue size only) | ~4 bytes (int) |

**Total Estimated**: ~11KB RAM for orchestrator integration data structures

**Available**: ~300KB total ESP32 RAM (plenty of headroom)

---

## SD Card File Structure

```
/
├── config.txt              # Configuration (created by GM)
├── device_id.txt           # Auto-generated device ID (if not in config)
├── tokens.json             # Token database (synced from orchestrator or pre-loaded)
├── queue.jsonl             # Offline queue (FIFO, max 100 lines)
├── images/                 # Token images and processing images
│   ├── 534e2b02.jpg
│   ├── 534e2b03.jpg        # Processing image for video token
│   └── ...
└── audio/                  # Token audio files
    ├── 534e2b02.mp3
    └── ...
```

---

**Data Model Complete**: 2025-10-19
**Phase 1 Status**: ✅ Entities defined, ready for contracts generation
