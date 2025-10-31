# ESP32 Scanner Quick Reference

## All Backend Endpoints Called

| Method | Endpoint | Called From | Purpose |
|--------|----------|-------------|---------|
| POST | `/api/scan` | OrchestratorService::sendScan() | Send single RFID scan |
| POST | `/api/scan/batch` | OrchestratorService::uploadQueueBatch() | Batch upload (max 10) |
| GET | `/health?deviceId=X` | OrchestratorService::checkHealth() & backgroundTaskLoop() | Health check every 10s |
| GET | `/api/tokens` | TokenService::syncFromOrchestrator() | Token DB sync on boot |

## Request/Response Formats

### POST /api/scan
```json
{
  "tokenId": "kaa001",
  "teamId": "001",
  "deviceId": "SCANNER_001",
  "timestamp": "1970-01-01T00:00:00.000Z"
}
```
Response: HTTP 200/409/error

### POST /api/scan/batch
```json
{
  "transactions": [
    {"tokenId": "...", "teamId": "001", "deviceId": "...", "timestamp": "..."},
    ...
  ]
}
```
Response: HTTP 200 on success

### GET /health?deviceId=SCANNER_001
Response: HTTP 200 if healthy

### GET /api/tokens
Response: HTTP 200 with JSON
```json
{
  "tokens": {
    "kaa001": {"video": "kaa001.mp4"},
    "jaw001": {"video": ""}
  }
}
```

## Configuration File (/config.txt on SD card)

```ini
WIFI_SSID=NetworkName              # Required, 1-32 chars
WIFI_PASSWORD=password             # Optional, 0-63 chars
ORCHESTRATOR_URL=http://10.0.0.177:3000  # Required (must start with http://)
TEAM_ID=001                        # Required (exactly 3 digits)
DEVICE_ID=SCANNER_001              # Optional (auto-generated if empty)
SYNC_TOKENS=true                   # Optional (default: true)
DEBUG_MODE=false                   # Optional (default: false)
```

## HTTP Client Details

- Location: OrchestratorService.h lines 514-617 (HTTPHelper class)
- Supports both HTTP and HTTPS (auto-detects by URL prefix)
- HTTPS uses WiFiClientSecure.setInsecure() (cert validation disabled)
- 5-second timeout on all requests
- Consolidates 4 duplicate HTTP implementations (saves 15KB flash)

## Token Data Flow

1. Boot: ConfigService loads /config.txt
2. If SYNC_TOKENS=true: TokenService.syncFromOrchestrator() 
   - GET /api/tokens
   - Save to /tokens.json
3. TokenService.loadDatabaseFromSD()
   - Parse /tokens.json into std::vector<TokenMetadata>
4. On scan: TokenService.get(tokenId) for metadata
5. Image/audio paths: Always constructed from tokenId
   - Image: /assets/images/{tokenId}.bmp
   - Audio: /assets/audio/{tokenId}.wav

## RFID Scan Processing

1. RFIDReader detects card
2. Extract token ID (NDEF text preferred, UID hex fallback)
3. Route based on connection state:
   - ORCH_CONNECTED: sendScan() → queue on failure
   - ORCH_WIFI_CONNECTED or ORCH_DISCONNECTED: queue immediately
4. Look up token metadata
5. Display token or queue for upload if offline

## Offline Queue

- File: /queue.jsonl (JSONL format, one JSON per line)
- Max size: 100 entries
- Batch upload: Max 10 entries per POST /api/scan/batch
- Background task (Core 0): Checks health every 10s, uploads if connected
- Memory-safe: Stream-based removal, never loads entire queue to RAM

## Device Identification

- teamId: Required, exactly 3 digits (001-999)
- deviceId: Optional in config, auto-generated from MAC address if not provided
- Both sent in every scan via deviceId parameter

## Key Design Patterns

1. **Singleton**: All HAL components & services (getInstance())
2. **Facade**: Application class coordinates subsystems
3. **State Machine**: UI with 4 states (READY, IMAGE, STATUS, PROCESSING)
4. **RAII**: SD card locking (automatic mutex release)
5. **Command Registry**: SerialService replaces if/else chains

## Critical Implementation Notes

1. **VSPI Bus**: Display must initialize BEFORE SD card (or SD.open() fails)
2. **GPIO 3 Conflict**: Shared between Serial RX and RFID_SS
   - DEBUG_MODE=true: Serial active, RFID deferred
   - DEBUG_MODE=false: RFID active, Serial unavailable
3. **Memory Safety**: Queue removal uses streaming, not RAM buffering
4. **Thread Safety**: Spinlock for queue counter, RAII lock for SD operations

## Flash Usage

- Current: 1,207,147 bytes (92%)
- Remaining: ~103KB
- Phase 6 Target: <87% (1,150,000 bytes)

## HTTPS Status

- ✓ Supported via WiFiClientSecure.setInsecure()
- ✓ Auto-detects HTTP vs HTTPS by URL prefix
- ✓ Certificate validation disabled (acceptable for local networks)
- ⚠️ Config validation requires http:// prefix (should accept https://)

## File Paths on SD Card

```
/config.txt             # Configuration
/tokens.json            # Token database
/device_id.txt          # Persisted device ID
/queue.jsonl            # Offline queue
/assets/images/         # Token images (BMP, 240x320)
/assets/audio/          # Token audio (WAV)
```

## Initialization Sequence

1. Serial communication (115200 baud)
2. 30-second DEBUG_MODE override window
3. Display & SD card hardware
4. Config loading
5. Touch, audio, RFID hardware
6. Token sync & database load
7. WiFi connection
8. Background task start
9. UI ready

Total: 15-25 seconds

## Critical File References

| Task | File | Lines |
|------|------|-------|
| Send scan | OrchestratorService.h | 169-224 |
| Queue scan | OrchestratorService.h | 233-291 |
| Batch upload | OrchestratorService.h | 344-429 |
| Health check | OrchestratorService.h | 325-332 |
| HTTP setup | OrchestratorService.h | 554-612 |
| Token sync | TokenService.h | 203-268 |
| Token load | TokenService.h | 93-146 |
| Token lookup | TokenService.h | 160-167 |
| Config | Config.h | 9-85 |
| Scan process | Application.h | 455-560 |

## Known Limitations

1. No HTTPS URL support in config validation (only http://)
2. Timestamp is relative to boot time (no RTC)
3. Flash at 92% capacity (tight constraints)
4. Team ID strictly 3 digits (no flexibility)
