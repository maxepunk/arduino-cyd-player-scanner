# Contract: Orchestrator API Client

**Feature**: 003-orchestrator-hardware-integration
**Client**: ESP32 Hardware Scanner (HTTPClient library)
**Server**: ALN Orchestrator Backend
**Date**: 2025-10-19
**Status**: Contract Definition

## Purpose

Define the HTTP client behavior, request formats, response handling, and error handling for all orchestrator API endpoints used by the hardware scanner.

---

## API Client Configuration

### HTTP Client Settings

| Setting | Value | Rationale |
|---------|-------|-----------|
| **Protocol** | HTTP only (not HTTPS) | Trusted local network, no TLS overhead on ESP32 |
| **Timeout** | 5000ms (5 seconds) | Balance between network reliability and acceptable latency |
| **Retry Logic** | None for immediate scans | Queue-based resilience instead of blocking retries |
| **Keep-Alive** | Disabled | Fire-and-forget pattern, short-lived connections |
| **User-Agent** | `ESP32-Scanner/1.0` | Identify client in server logs |

### Base URL Configuration

**Source**: Configuration file `/config.txt` → `ORCHESTRATOR_URL` field

**Format**: `http://{IP_OR_HOSTNAME}:{PORT}`

**Example**: `http://10.0.0.100:3000`

**Validation**:
- Must start with `http://`
- No trailing slash
- Port required if not 80

---

## Endpoint Definitions

### 1. GET /health

**Purpose**: Connection health check (verify orchestrator is reachable)

**When Called**:
- Every 10 seconds (background task, connection monitoring)
- After WiFi reconnection (verify orchestrator availability)

**Request**:
```http
GET /health HTTP/1.1
Host: {orchestrator-ip}:3000
Cache-Control: no-cache
```

**Expected Response** (200 OK):
```json
{
  "status": "online",
  "version": "1.0.0",
  "uptime": 3600.5,
  "timestamp": "2025-10-19T12:00:00.000Z"
}
```

**Client Behavior**:
- **200 OK**: Orchestrator is reachable → Set connection state to CONNECTED
- **Timeout (5s)**: Orchestrator unreachable → Set connection state to OFFLINE
- **4xx/5xx Error**: Treat as unreachable → Set connection state to OFFLINE

**Implementation**:
```cpp
bool checkOrchestratorHealth() {
  HTTPClient http;
  http.begin(orchestratorUrl + "/health");
  http.setTimeout(5000); // 5-second timeout

  int httpCode = http.GET();
  http.end();

  return (httpCode == 200);
}
```

**Parse Response**: NOT required (only check status code)

---

### 2. GET /api/tokens

**Purpose**: Fetch token database for local caching (optional sync on boot)

**When Called**:
- Once on boot (if orchestrator is reachable)
- On-demand (if user requests token database refresh via serial command)

**Request**:
```http
GET /api/tokens HTTP/1.1
Host: {orchestrator-ip}:3000
Accept: application/json
```

**Expected Response** (200 OK):
```json
{
  "tokens": {
    "534e2b02": {
      "image": "assets/images/534e2b02.jpg",
      "audio": "assets/audio/534e2b02.mp3",
      "video": null,
      "processingImage": null,
      "SF_RFID": "534e2b02",
      "SF_ValueRating": 3,
      "SF_MemoryType": "Technical",
      "SF_Group": ""
    },
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
  },
  "count": 2,
  "lastUpdate": "2025-10-19T12:00:00.000Z"
}
```

**Client Behavior**:
- **200 OK**: Parse response, save full JSON to `/tokens.json` on SD card (overwrite cache)
- **Timeout/Error**: Use existing `/tokens.json` cache from SD card (fallback mode)

**Implementation**:
```cpp
bool syncTokenDatabase() {
  HTTPClient http;
  http.begin(orchestratorUrl + "/api/tokens");
  http.setTimeout(5000);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    http.end();

    // Save to SD card
    File file = SD.open("/tokens.json", FILE_WRITE);
    if (file) {
      file.print(payload);
      file.close();
      Serial.println("Token database synced successfully");
      return true;
    }
  }

  http.end();
  Serial.println("Token database sync failed, using cache");
  return false;
}
```

**Parse Response**: Full JSON saved to SD card, then loaded and parsed with filtered fields (see data-model.md).

---

### 3. POST /api/scan

**Purpose**: Send single token scan to orchestrator (fire-and-forget)

**When Called**:
- Immediately when token scanned and connection state is CONNECTED
- Fire-and-forget pattern (don't block on response parsing)

**Request**:
```http
POST /api/scan HTTP/1.1
Host: {orchestrator-ip}:3000
Content-Type: application/json
User-Agent: ESP32-Scanner/1.0

{
  "tokenId": "534e2b03",
  "teamId": "001",
  "deviceId": "SCANNER_A1B2C3D4E5F6",
  "timestamp": "2025-10-19T14:30:00.000Z"
}
```

**Request Body Fields**:

| Field | Type | Required | Source | Example |
|-------|------|----------|--------|---------|
| `tokenId` | String | Yes | RFID card UID | `"534e2b03"` |
| `teamId` | String | No | Configuration `TEAM_ID` field | `"001"` |
| `deviceId` | String | Yes | Device Identifier entity | `"SCANNER_A1B2C3D4E5F6"` |
| `timestamp` | String | Yes | ISO 8601 formatted current time | `"2025-10-19T14:30:00.000Z"` |

**Expected Response** (200 OK):
```json
{
  "status": "accepted",
  "message": "Video queued for playback",
  "tokenId": "534e2b03",
  "mediaAssets": {
    "video": "test_30sec.mp4",
    "image": null,
    "audio": null
  },
  "videoQueued": true
}
```

**Other Possible Responses**:
- **202 Accepted**: Scan queued (orchestrator offline mode) - treat as success
- **409 Conflict**: Video already playing - acceptable, orchestrator handles queue
- **400 Bad Request**: Validation error - unexpected, queue for retry
- **503 Service Unavailable**: Orchestrator offline queue full - rare, queue locally anyway
- **Timeout (5s)**: Network issue - queue immediately

**Client Behavior** (Fire-and-Forget):
- **200-299 Status**: Success → Done, do not parse response body
- **Other/Timeout**: Failure → Queue scan to `/queue.jsonl`
- **NO retries**: Immediately move to next operation (display local content)

**Implementation**:
```cpp
bool sendScan(String tokenId, String teamId, String deviceId, String timestamp) {
  HTTPClient http;
  http.begin(orchestratorUrl + "/api/scan");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  // Create JSON request body
  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) {
    doc["teamId"] = teamId;
  }
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  String requestBody;
  serializeJson(doc, requestBody);

  // Send POST request
  int httpCode = http.POST(requestBody);
  http.end();

  // Fire-and-forget: only check status code range
  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("Scan sent successfully");
    return true;
  } else {
    Serial.printf("Scan failed (HTTP %d), queueing\n", httpCode);
    return false;
  }
}
```

**Parse Response**: NOT required (fire-and-forget pattern)

---

### 4. POST /api/scan/batch

**Purpose**: Upload queued scans in batch when connection restored

**When Called**:
- Background task detects connection restored and queue is not empty
- Recursively called for subsequent batches if queue >10 entries (1-second delay between batches)

**Request**:
```http
POST /api/scan/batch HTTP/1.1
Host: {orchestrator-ip}:3000
Content-Type: application/json
User-Agent: ESP32-Scanner/1.0

{
  "transactions": [
    {"tokenId":"534e2b02","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:25:00.000Z"},
    {"tokenId":"534e2b03","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:30:00.000Z"},
    {"tokenId":"tac001","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:35:00.000Z"}
  ]
}
```

**Request Body Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `transactions` | Array | Array of scan objects (up to 10 items) |
| `transactions[].tokenId` | String | Token identifier |
| `transactions[].teamId` | String | Team identifier (optional) |
| `transactions[].deviceId` | String | Scanner device identifier |
| `transactions[].timestamp` | String | ISO 8601 scan timestamp |

**Batch Size**: Up to 10 entries per request

**Expected Response** (200 OK):
```json
{
  "status": "accepted",
  "processed": 3,
  "message": "Batch processed successfully"
}
```

**Client Behavior**:
- **200 OK**: Success → Remove uploaded entries from `/queue.jsonl`
- **Other/Timeout**: Failure → Keep entries in queue, retry on next connection check (10s)
- **Recursive**: If queue still has entries, wait 1 second and upload next batch

**Implementation**:
```cpp
bool uploadQueueBatch(int maxBatchSize = 10) {
  std::vector<QueueEntry> batch;
  readQueue(batch, maxBatchSize);

  if (batch.empty()) return true; // No entries to upload

  // Create JSON request
  JsonDocument doc;
  JsonArray transactions = doc["transactions"].to<JsonArray>();

  for (QueueEntry& entry : batch) {
    JsonObject transaction = transactions.add<JsonObject>();
    transaction["tokenId"] = entry.tokenId;
    if (entry.teamId.length() > 0) {
      transaction["teamId"] = entry.teamId;
    }
    transaction["deviceId"] = entry.deviceId;
    transaction["timestamp"] = entry.timestamp;
  }

  String requestBody;
  serializeJson(doc, requestBody);

  // Send POST request
  HTTPClient http;
  http.begin(orchestratorUrl + "/api/scan/batch");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST(requestBody);
  http.end();

  if (httpCode == 200) {
    // Success: remove uploaded entries
    removeUploadedEntries(batch.size());
    Serial.printf("Batch uploaded: %d entries\n", batch.size());

    // Upload next batch if queue not empty
    if (countQueueEntries() > 0) {
      delay(1000); // 1-second delay between batches
      return uploadQueueBatch(maxBatchSize); // Recursive call
    }

    return true;
  } else {
    Serial.printf("Batch upload failed (HTTP %d), retrying later\n", httpCode);
    return false; // Keep in queue, retry on next check
  }
}
```

**Parse Response**: NOT required (only check status code)

---

## Error Handling

### Network Errors

| Error Type | Detection | Behavior |
|------------|-----------|----------|
| **Timeout** | `httpCode == -11` or 5s elapsed | Queue scan, set connection state to OFFLINE |
| **Connection Refused** | `httpCode == -1` | Queue scan, set connection state to OFFLINE |
| **DNS Failure** | `httpCode == -2` | Queue scan, display "Config Error: Invalid URL" |
| **Read Error** | `httpCode < 0` | Queue scan, set connection state to OFFLINE |

**Implementation**:
```cpp
if (httpCode < 0) {
  Serial.printf("HTTP error: %s\n", http.errorToString(httpCode).c_str());
  queueScan(tokenId, teamId, deviceId, timestamp);
  return false;
}
```

---

### HTTP Status Codes

#### Success Codes (2xx)

| Code | Meaning | Client Action |
|------|---------|---------------|
| **200 OK** | Request successful | Done (scan logged or batch uploaded) |
| **202 Accepted** | Request queued on orchestrator | Treat as success (orchestrator handles queueing) |

#### Client Error Codes (4xx)

| Code | Meaning | Client Action |
|------|---------|---------------|
| **400 Bad Request** | Validation error | Queue for retry (orchestrator may be updated) |
| **409 Conflict** | Video already playing | Acceptable (orchestrator queues duplicate) |

#### Server Error Codes (5xx)

| Code | Meaning | Client Action |
|------|---------|---------------|
| **500 Internal Server Error** | Orchestrator crash | Queue for retry (may recover) |
| **503 Service Unavailable** | Orchestrator offline queue full | Queue locally (rare edge case) |

**Implementation**:
```cpp
if (httpCode >= 200 && httpCode < 300) {
  return true; // Success
} else {
  // Any non-2xx response: queue for retry
  queueScan(tokenId, teamId, deviceId, timestamp);
  return false;
}
```

---

### JSON Serialization Errors

| Error | Detection | Behavior |
|-------|-----------|----------|
| **ArduinoJson overflow** | `serializeJson()` returns incomplete | Log error, abort request, queue scan |
| **Invalid characters** | JSON contains unescaped special chars | ArduinoJson handles escaping automatically |

**Implementation**:
```cpp
String requestBody;
size_t bytesWritten = serializeJson(doc, requestBody);

if (bytesWritten == 0) {
  Serial.println("JSON serialization failed");
  queueScan(tokenId, teamId, deviceId, timestamp);
  return false;
}
```

---

## Timestamp Generation

### ISO 8601 Format

**Required Format**: `YYYY-MM-DDTHH:MM:SS.sssZ`

**Example**: `2025-10-19T14:30:00.000Z`

**Generation** (using ESP32 uptime, NOT real-time clock):

```cpp
String generateTimestamp() {
  unsigned long millisSinceBoot = millis();

  // Convert to ISO 8601 format (approximation using uptime)
  unsigned long seconds = millisSinceBoot / 1000;
  unsigned long millis = millisSinceBoot % 1000;

  unsigned long minutes = seconds / 60;
  seconds = seconds % 60;

  unsigned long hours = minutes / 60;
  minutes = minutes % 60;

  char timestamp[30];
  snprintf(timestamp, sizeof(timestamp),
    "1970-01-01T%02lu:%02lu:%02lu.%03luZ",
    hours % 24, minutes, seconds, millis);

  return String(timestamp);
}
```

**Alternative** (if ESP32 has RTC configured via NTP):

```cpp
String generateTimestamp() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  gmtime_r(&now, &timeinfo);

  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);

  return String(timestamp);
}
```

**Recommendation**: Use uptime-based approach for simplicity (orchestrator doesn't rely on accurate timestamps, only relative ordering).

---

## Connection State Management

### State Transitions via API Calls

```
[Connection State: DISCONNECTED]
    ↓
[Background Task: Attempt WiFi Connection]
    ↓
[WiFi Connected]
    ↓
[Connection State: WIFI_CONNECTED]
    ↓
[Call GET /health]
    ↓
    ├─ [200 OK] → [Connection State: CONNECTED]
    │                 ↓
    │            [Start Queue Upload]
    │
    └─ [Timeout/Error] → [Connection State: OFFLINE]
        ↓
        [Background Task: Check /health every 10s]
        ↓
        [Call GET /health]
        ↓
        ├─ [200 OK] → [Connection State: CONNECTED] (reconnected)
        └─ [Timeout/Error] → [Connection State: OFFLINE] (stay offline)
```

### API Call Decision Logic

```cpp
void handleTokenScan(String tokenId) {
  // Create scan request
  ScanRequest scan(tokenId, teamId, deviceId, generateTimestamp());

  // Check connection state
  if (connectionState == CONNECTED) {
    // Attempt to send scan
    bool success = sendScan(scan.tokenId, scan.teamId,
                             scan.deviceId, scan.timestamp);
    if (!success) {
      // Send failed, queue for later
      queueScan(scan.tokenId, scan.teamId, scan.deviceId, scan.timestamp);
    }
  } else {
    // Offline or disconnected, queue immediately
    queueScan(scan.tokenId, scan.teamId, scan.deviceId, scan.timestamp);
  }

  // Display local content (fire-and-forget, don't wait for network)
  displayTokenContent(tokenId);
}
```

---

## Security Considerations

### No Authentication

**Decision**: Player scanner endpoints (`/api/tokens`, `/api/scan`, `/api/scan/batch`, `/health`) require NO authentication (open API on trusted local network).

**Rationale**:
- Trusted venue network (closed WiFi, no internet access)
- ESP32 HTTP client complexity (no JWT support in standard library)
- Admin endpoints use JWT authentication (player scanner doesn't need admin access)

**Risk**: Anyone on venue network can send scans. Acceptable for closed event environment.

---

### No TLS/HTTPS

**Decision**: HTTP only (no HTTPS/TLS encryption).

**Rationale**:
- Trusted local network (no internet exposure)
- ESP32 TLS overhead (memory, performance impact)
- Certificate management complexity (no CA infrastructure in venue)

**Risk**: Network traffic is unencrypted. Acceptable for closed event environment.

---

### Input Validation

**Scanner Validates**:
- Configuration file fields (WIFI_SSID, ORCHESTRATOR_URL, etc.)
- Token ID not empty
- Team ID matches 3-digit pattern

**Orchestrator Validates** (server-side):
- Request body JSON structure
- Required fields present
- Field formats (tokenId, teamId patterns)
- SQL injection prevention (parameterized queries)

**Defense in Depth**: Scanner performs basic validation, orchestrator performs comprehensive validation.

---

## Performance Benchmarks

### Request Latency Targets

| Operation | Target | Max Acceptable | Notes |
|-----------|--------|----------------|-------|
| **POST /api/scan** | <500ms | <2000ms | Fire-and-forget, timeout at 5s |
| **GET /health** | <100ms | <5000ms | Simple endpoint, timeout at 5s |
| **POST /api/scan/batch** | <1s | <5000ms | 10 entries, timeout at 5s |
| **GET /api/tokens** | <2s | <5000ms | 50KB JSON, timeout at 5s |

### Timeout Strategy

**All Requests**: 5-second timeout

**Rationale**:
- Balances network reliability with acceptable latency
- Prevents indefinite blocking of scanner operation
- Matches PWA reference implementation pattern

---

## Test Cases

### Test Case 1: Successful Scan Send

**Preconditions**: Connection state = CONNECTED

**Operation**: Send scan request

**Expected**:
- HTTP POST `/api/scan` sent with JSON payload
- Response: 200 OK
- Scan not queued
- Serial log: "Scan sent successfully"

---

### Test Case 2: Scan Send Timeout

**Preconditions**: Orchestrator unreachable

**Operation**: Send scan request

**Expected**:
- HTTP POST `/api/scan` times out after 5 seconds
- Scan queued to `/queue.jsonl`
- Connection state changed to OFFLINE
- Serial log: "Scan failed (timeout), queueing"

---

### Test Case 3: Batch Upload Success

**Preconditions**: Queue has 15 entries, connection state = CONNECTED

**Operation**: Upload queue batch

**Expected**:
- HTTP POST `/api/scan/batch` with 10 entries
- Response: 200 OK
- First 10 entries removed from queue
- Wait 1 second
- HTTP POST `/api/scan/batch` with 5 remaining entries
- Response: 200 OK
- All entries removed from queue
- Serial log: "Batch uploaded: 10 entries", "Batch uploaded: 5 entries"

---

### Test Case 4: Health Check Failure

**Preconditions**: Connection state = CONNECTED

**Operation**: Background task calls GET /health, orchestrator offline

**Expected**:
- HTTP GET `/health` times out after 5 seconds
- Connection state changed to OFFLINE
- New scans queued instead of sent
- Serial log: "Connection lost!"

---

### Test Case 5: Token Database Sync Success

**Preconditions**: Scanner boots, orchestrator reachable

**Operation**: Sync token database

**Expected**:
- HTTP GET `/api/tokens`
- Response: 200 OK with JSON payload
- JSON saved to `/tokens.json` on SD card (overwrite cache)
- Serial log: "Token database synced successfully"

---

### Test Case 6: Token Database Sync Failure

**Preconditions**: Scanner boots, orchestrator unreachable

**Operation**: Attempt token database sync

**Expected**:
- HTTP GET `/api/tokens` times out after 5 seconds
- Load existing `/tokens.json` from SD card cache
- Serial log: "Token database sync failed, using cache"

---

**Contract Complete**: 2025-10-19
**Version**: 1.0
