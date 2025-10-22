# Feature Specification: Orchestrator Integration for Hardware Scanner

**Feature Branch**: `003-orchestrator-hardware-integration`
**Created**: 2025-10-18
**Status**: Draft
**Input**: User description: "Port the orchestrator integration features from the PWA (aln-memory-scanner) into our dedicated scanner hardware. The PWA in aln-memory-scanner is a development prototype used for playtesting and as a basis to develop our dedicated hardware version. Essential considerations: (1) Respect all hardware limitations while adapting PWA functionality, (2) Carefully identify PWA functionality NOT intended to be ported (QR code scanning, for example - we use RFID instead), (3) Port networked mode features: connection monitoring, offline queue, video playback triggers, (4) Adapt browser-based features to ESP32 capabilities (localStorage to SD card, fetch API to HTTPClient, etc.)"

---

## Overview

The hardware scanner must communicate with a central orchestrator server to log all gameplay activity and trigger synchronized video playback for specific memory tokens. When a player scans any RFID memory token, the scanner sends the scan to the orchestrator (or queues it if offline) to maintain a complete activity log. For tokens with video content, this scan triggers the orchestrator to play the video on a shared display visible to all players, creating synchronized multi-player storytelling experiences.

The scanner operates independently for local content display. Most memory tokens have images and audio stored locally on the SD card, which the scanner displays and plays immediately after scanning. Video memory tokens typically have only a processing image (shown during the brief "Sending..." moment) and no local playback content - the video plays on the orchestrator's shared display instead.

Network operations are invisible and non-blocking. Scans that cannot be sent immediately are queued and synchronized later without player awareness.

---

## Clarifications

### Session 2025-10-19

- Q: Does the orchestrator API require authentication for player scanner endpoints? → A: No authentication - Open API on trusted local network
- Q: Should the scanner use HTTP or HTTPS for orchestrator communication? → A: HTTP only - No encryption on trusted venue network
- Q: What is the exact format of `/config.txt`? → A: Simple key=value pairs - One per line, no sections or comments
- Q: How should the scanner handle WiFi disconnection during gameplay? → A: Auto-reconnect every 10 seconds - Background task monitors and reconnects automatically
- Q: What format should auto-generated device IDs use? → A: Hex with prefix - "SCANNER_A1B2C3D4E5F6" (readable, matches API examples)

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Game Master Pre-Game Setup (Priority: P1)

Before gameplay begins, the Game Master prepares the scanner by configuring network settings and team assignment. The GM edits a simple configuration file on the scanner's SD card using any computer, then inserts the SD card into the scanner. The SD card already contains the token database (tokens.json) and all local media assets (images, audio, processing images) from the latest game build. When powered on, the scanner reads the configuration, connects to the venue WiFi, and optionally synchronizes the latest token database from the orchestrator if available. The scanner is now ready for players to discover and use during gameplay.

**Why this priority**: Without configuration, the scanner cannot connect to the orchestrator or identify which team's scans it's logging. This is the foundational prerequisite for all networked functionality. Must be simple and reliable since GMs are not necessarily technical users.

**Independent Test**: Can be fully tested by: (1) Creating a config file with WiFi and orchestrator settings, (2) Inserting SD card with pre-loaded tokens.json and media, (3) Powering on scanner, (4) Verifying scanner connects to WiFi, (5) Checking display shows "Connected" status. Delivers immediate value by proving network connectivity works.

**Acceptance Scenarios**:

1. **Given** GM creates `/config.txt` with WiFi SSID, password, orchestrator URL, team ID, and device ID, **When** SD card is inserted and scanner is powered on, **Then** scanner connects to configured WiFi within 30 seconds
2. **Given** scanner connected to WiFi, **When** orchestrator is reachable at configured URL, **Then** scanner optionally fetches updated token database via HTTP GET `/api/tokens` within 10 seconds
3. **Given** orchestrator unreachable or sync skipped, **When** scanner boots, **Then** scanner uses cached token database from SD card (`/tokens.json` included in SD card image)
4. **Given** token database loaded (from sync or cache), **When** scanner completes initialization, **Then** display shows "Connected ✓" (if online) or "Ready - Offline" (if offline) and scanner is ready for use
5. **Given** config file has invalid WiFi credentials, **When** scanner attempts connection, **Then** scanner displays "WiFi Error - Check Config" and continues to retry every 30 seconds

---

### User Story 2 - Player Scans Memory Token with Local Content (Priority: P1)

During gameplay, a player discovers the scanner and scans an RFID memory token that contains an image and audio file. The scanner immediately sends the scan to the orchestrator (if connected) or queues it (if offline) to log the player's activity. Simultaneously, the scanner displays the token's image and plays its audio from local files on the SD card. The player experiences their memory's visual and audio content without any network dependency. The network operation happens invisibly in the background - the player is unaware whether the scan was sent immediately or queued.

**Why this priority**: This is the primary gameplay interaction - players scanning memory tokens to experience their content. Local playback must work reliably regardless of network status. Orchestrator logging provides game state tracking but doesn't block the core experience.

**Independent Test**: Can be fully tested by: (1) Loading token with image/audio into tokens.json and placing media files on SD card, (2) Scanning the token, (3) Verifying image displays and audio plays within 1 second, (4) Confirming orchestrator receives POST `/api/scan` request (if online) or scan is queued (if offline). Delivers core gameplay functionality.

**Acceptance Scenarios**:

1. **Given** scanner connected to orchestrator and token has image/audio, **When** player scans token, **Then** scanner sends POST request to `/api/scan` with tokenId, deviceId, and teamId within 2 seconds
2. **Given** scanner offline and token has image/audio, **When** player scans token, **Then** scanner queues scan to `/queue.jsonl` on SD card within 500ms
3. **Given** token has image and audio files on SD card, **When** player scans token, **Then** scanner displays image and plays audio within 1 second (regardless of network status)
4. **Given** scan sent to orchestrator, **When** orchestrator responds 200 OK, **Then** scanner completes without blocking content display (fire-and-forget pattern)
5. **Given** token has no local media files, **When** player scans token, **Then** scanner displays token ID as text (fallback display)

---

### User Story 3 - Player Scans Video Memory Token (Priority: P1)

During gameplay, a player scans an RFID memory token that triggers a video cutscene. The scanner sends the scan to the orchestrator (if connected), showing a brief "Sending..." modal with the token's processing image. The orchestrator receives the request and queues the video for playback on the shared display. The player's scanner shows the processing image briefly (since video tokens typically have no local playback content), then returns to ready mode. The video plays on the shared venue display for all players to experience together as a synchronized narrative moment.

**Why this priority**: Video triggers are major game events - cutscenes that advance the shared story. The scanner's job is to reliably notify the orchestrator when these tokens are scanned. If offline, the scan is queued and will trigger the video when connection is restored, maintaining story continuity.

**Independent Test**: Can be fully tested by: (1) Loading token with video field and processingImage into tokens.json, (2) Placing processing image file on SD card, (3) Scanning token, (4) Verifying "Sending..." modal displays with processing image, (5) Confirming orchestrator receives request and queues video, (6) Verifying scanner returns to ready mode after 2.5 seconds. Delivers video trigger functionality.

**Acceptance Scenarios**:

1. **Given** scanner connected to orchestrator and token has video field, **When** player scans token, **Then** scanner sends POST `/api/scan` within 2 seconds and orchestrator queues video for playback
2. **Given** token has processingImage field, **When** scan request is being sent, **Then** scanner displays "Sending..." modal with processing image for maximum 2.5 seconds
3. **Given** scanner offline and token has video, **When** player scans token, **Then** scanner queues scan and displays processing image with "Queued" indicator
4. **Given** token has video but no processingImage, **When** player scans token, **Then** scanner shows "Sending..." modal with token ID text instead of image
5. **Given** video token scan sent successfully, **When** "Sending..." modal completes, **Then** scanner returns to ready mode (no local playback for video tokens)

---

### User Story 4 - Offline Queue and Auto-Sync (Priority: P2)

During gameplay, the venue's WiFi experiences intermittent outages or the orchestrator server becomes temporarily unreachable. Players continue scanning memory tokens (both video and non-video). The scanner queues each scan that cannot be sent immediately, storing them persistently on the SD card. Players see their memory content normally (local images/audio for non-video tokens, processing images for video tokens) - they may notice a "Queued" indicator instead of "Sending..." but the experience is otherwise unchanged. When connectivity is restored, the scanner automatically uploads all queued scans in the background (batches of 10 at a time). Players never notice the synchronization happening. The game continues without interruption, and all scans are eventually logged and video triggers are eventually processed.

**Why this priority**: Gameplay sessions must not be disrupted by technical issues. Players should never need to re-scan tokens or worry about whether their scans "counted." This resilience is critical for production deployment in real-world venues where network reliability varies.

**Independent Test**: Can be fully tested by: (1) Starting scanner with orchestrator disconnected, (2) Scanning 5 tokens (mix of video and non-video), (3) Verifying all scans are queued to `/queue.jsonl` on SD card, (4) Confirming queue size shown in status display, (5) Restoring orchestrator connection, (6) Verifying all 5 scans uploaded via batch endpoint within 30 seconds, (7) Confirming queue file cleared. Delivers offline resilience independently of other features.

**Acceptance Scenarios**:

1. **Given** orchestrator is unreachable, **When** player scans any token, **Then** scan is appended to `/queue.jsonl` file on SD card within 500ms
2. **Given** scanner has 3 queued scans, **When** player views status screen, **Then** display shows "Queue: 3 scans" indicator
3. **Given** scanner has queued scans and connection is restored, **When** background monitor detects orchestrator available, **Then** scanner uploads queued scans via POST `/api/scan/batch` within 10 seconds
4. **Given** queue has 15 scans, **When** upload begins, **Then** scanner sends first 10 scans in batch, then sends remaining 5 in second batch with 1-second delay between batches
5. **Given** batch upload successful (200 OK), **When** orchestrator confirms receipt, **Then** scanner removes uploaded scans from queue file and updates queue count
6. **Given** batch upload fails (timeout or error), **When** scanner detects failure, **Then** scans remain in queue and scanner retries on next connection check (10 seconds later)
7. **Given** scanner loses power while scan is being queued, **When** scanner restarts, **Then** previously queued scans (written before power loss) remain intact in queue file

---

### User Story 5 - Queue Overflow Protection (Priority: P2)

During an extended network outage, players scan many memory tokens. The offline queue grows to its maximum size of 100 scans. When a player scans the 101st token, the scanner removes the oldest queued scan (first-in-first-out) to make room for the new scan. The player sees a brief "Queue Full!" warning flash on the display but the scan still succeeds - they still see the local content (or processing image) immediately. When connection is restored, the scanner uploads the 100 most recent scans. The oldest scan (the one discarded) is lost, but this is acceptable given the extreme circumstances.

**Why this priority**: Prevents unbounded queue growth that could exhaust SD card storage or cause performance issues. Provides graceful degradation under extreme conditions. FIFO ensures most recent scans (likely most important to current gameplay) are preserved.

**Independent Test**: Can be fully tested by: (1) Disconnecting orchestrator, (2) Scanning 100 tokens to fill queue, (3) Verifying queue file has 100 entries, (4) Scanning 101st token, (5) Confirming oldest scan removed and new scan appended, (6) Verifying "Queue Full!" warning displayed briefly, (7) Confirming player still sees local content. Delivers overflow protection independently.

**Acceptance Scenarios**:

1. **Given** queue contains 100 scans, **When** player scans any token, **Then** scanner removes first (oldest) line from `/queue.jsonl` and appends new scan
2. **Given** queue overflow occurs, **When** oldest scan is discarded, **Then** scanner displays "Queue Full!" warning for 500ms in yellow text
3. **Given** queue is at 100 scans, **When** player views status screen, **Then** display shows "Queue: 100 scans (FULL)" in yellow/red text
4. **Given** queue full and new scan queued, **When** overflow occurs, **Then** player still sees local content for newly scanned token (display unaffected by queue overflow)

---

### User Story 6 - Connection Status Visibility (Priority: P3)

Players and game masters need to understand whether the scanner is successfully communicating with the orchestrator. During idle time (not actively scanning), a player taps the scanner display once. The scanner shows a status screen displaying WiFi connection status, orchestrator connection status, number of queued scans, team assignment, and device identifier. The player taps again to dismiss the status screen and return to ready mode. This allows players to self-diagnose issues ("Am I connected?") and GMs to verify scanner health during setup.

**Why this priority**: Provides transparency and troubleshooting capability. Players feel confident their scans are being synchronized. GMs can quickly verify scanner configuration and connectivity during setup. Not critical for core gameplay (scans work regardless of connection visibility) but important for user confidence and support.

**Independent Test**: Can be fully tested by: (1) Configuring scanner and connecting to orchestrator, (2) Tapping display once during idle mode, (3) Verifying status screen shows WiFi SSID, orchestrator status (Connected/Offline), queue count, team ID, device ID, (4) Tapping again to dismiss, (5) Confirming return to ready screen. Delivers diagnostic capability independently of other features.

**Acceptance Scenarios**:

1. **Given** scanner in idle/ready mode, **When** player taps display once, **Then** status screen displays within 200ms showing WiFi, orchestrator, queue, team, and device info
2. **Given** status screen displayed, **When** player taps display again, **Then** scanner returns to ready mode within 200ms
3. **Given** scanner connected to orchestrator, **When** status screen shown, **Then** orchestrator status shows "Connected" with green indicator
4. **Given** scanner offline, **When** status screen shown, **Then** orchestrator status shows "Offline" with yellow indicator and queue count
5. **Given** no scans queued, **When** status screen shown, **Then** queue line shows "Queue: 0 scans"

---

### User Story 7 - Token Database Synchronization (Priority: P3)

When the scanner boots up, it connects to WiFi and optionally contacts the orchestrator to fetch the latest token database. This database contains metadata for all memory tokens in the game, including which tokens have video content, which have local media, and processing image paths. The scanner downloads the database, saves it to SD card (overwriting previous cache), and loads it into memory. This ensures the scanner has the most current token information. If the orchestrator is unreachable, the scanner uses the cached database from the SD card (which was included in the initial SD card image or from a previous sync).

**Why this priority**: Ensures scanner has current token metadata for displaying processing images and understanding token content. Not critical for MVP (scanner can work with pre-loaded cache indefinitely) but useful for keeping scanners synchronized with game updates. Cache fallback ensures scanner remains functional during orchestrator downtime.

**Independent Test**: Can be fully tested by: (1) Starting scanner with orchestrator online, (2) Verifying HTTP GET `/api/tokens` request sent on boot, (3) Confirming `/tokens.json` saved to SD card, (4) Checking tokens loaded into memory, (5) Scanning token with processingImage and confirming image displays correctly. Delivers sync capability independently.

**Acceptance Scenarios**:

1. **Given** scanner boots with orchestrator reachable, **When** WiFi connection established, **Then** scanner optionally sends HTTP GET request to `/api/tokens` within 5 seconds
2. **Given** `/api/tokens` response received (200 OK), **When** JSON parsed successfully, **Then** scanner saves full JSON to `/tokens.json` on SD card (overwriting previous cache)
3. **Given** tokens saved to SD card, **When** scanner loads token database, **Then** scanner can look up video presence, media paths, and processing images for any token
4. **Given** orchestrator unreachable on boot, **When** sync fails, **Then** scanner loads `/tokens.json` from SD card cache (pre-loaded in SD card image)
5. **Given** token has `"processingImage": "assets/images/token_id.jpg"`, **When** player scans token, **Then** scanner displays processing image from SD card during "Sending..." modal
6. **Given** no cache exists and sync fails, **When** scanner boots, **Then** scanner displays warning "No Token Database - Limited Functionality" but still allows scanning

---

### Edge Cases

**What happens when WiFi credentials in config file are correct but orchestrator URL is wrong?**
- Scanner successfully connects to WiFi
- Token sync fails (HTTP request to wrong URL times out)
- Scanner uses cached `/tokens.json` from SD card
- Status screen shows "Connected to WiFi" but "Orchestrator: Offline"
- Scans are queued (orchestrator unreachable)
- GM can identify issue via status screen and update config file

**What happens when scanner loses WiFi connection mid-gameplay?**
- Background connection monitor detects WiFi loss on next check (within 10 seconds)
- Background task attempts WiFi reconnection automatically every 10 seconds
- New scans are queued to SD card during disconnection
- Players see "Queued" indicator instead of "Sending..."
- Local image/audio playback unaffected
- When WiFi restored, scanner auto-reconnects and processes queue without user intervention

**What happens when orchestrator is slow to respond (>5 seconds)?**
- HTTP client timeout triggers after 5 seconds
- Scanner treats as failure and queues the scan
- "Sending..." modal auto-hides after 2.5 seconds regardless
- Player sees local content immediately (network timeout doesn't block)
- Queued scan will be uploaded when connection stabilizes

**What happens when player scans same token twice rapidly?**
- Each scan is treated independently
- Both scans sent to orchestrator (or queued)
- Orchestrator decides how to handle duplicates (may queue both videos or ignore duplicate)
- Hardware scanner's job is just to report scans faithfully
- Player sees local content twice (or processing image twice for video tokens)

**What happens when SD card is removed during gameplay?**
- Scanner detects SD card missing on next access attempt
- Cannot queue scans (queue writes fail)
- Cannot load images/audio/processing images (display shows token ID as fallback)
- Scanner displays "SD Card Error - Insert Card" message
- Network operations may continue if already connected (send directly if online, but cannot queue if offline)

**What happens when orchestrator API changes or returns unexpected errors?**
- Scanner receives unexpected HTTP status code (not 200/202/409)
- Treats as failure and queues scan
- Error logged to serial for debugging: "Unexpected response: [code]"
- Player sees normal operation (local content displayed, scan queued)
- GM can diagnose via serial monitor if persistent

**What happens when queue has 50 scans and scanner is powered off?**
- Queue file `/queue.jsonl` persists on SD card (FAT32 filesystem)
- On next boot, scanner loads queue from file
- Connection monitor triggers queue processing when orchestrator available
- All 50 scans uploaded in 5 batches of 10

**What happens when multiple scanners assigned to same team scan simultaneously?**
- Each scanner sends requests with same `teamId` but different `deviceId`
- Orchestrator receives multiple requests and handles queueing/sequencing
- Videos may queue in order received by orchestrator
- Hardware scanners operate independently - no coordination required

**What happens during network congestion causing many timeout failures?**
- Scans queue up on SD card
- Background monitor continues checking connection every 10 seconds
- When network stabilizes, bulk upload via batch endpoint
- Player experience unchanged (local display always works)

**What happens when token has both video and local image/audio (unusual case)?**
- Scanner sends scan to orchestrator (video will be queued for playback)
- Scanner displays "Sending..." with processing image (if available) or local image
- After "Sending..." completes, scanner displays local image and plays local audio
- Both video (on shared display) and local content (on scanner) are experienced

**What happens when tokens.json has token entry but media files are missing from SD card?**
- Scanner attempts to load image/audio/processing image from configured paths
- File open fails (file not found)
- Scanner falls back to displaying token ID as text
- Scan is still sent to orchestrator (or queued) normally
- Serial log shows "File not found: /images/token_id.jpg" for debugging

---

## Requirements *(mandatory)*

### Functional Requirements

#### Network Configuration

- **FR-001**: Scanner MUST read WiFi SSID and password from SD card configuration file `/config.txt` on boot
- **FR-002**: Scanner MUST read orchestrator server URL from SD card configuration file `/config.txt` on boot
- **FR-003**: Scanner MUST read team ID and device ID from SD card configuration file `/config.txt` on boot
- **FR-004**: Scanner MUST connect to configured WiFi network within 30 seconds of boot
- **FR-005**: Scanner MUST retry WiFi connection every 30 seconds if initial connection fails during boot
- **FR-005a**: Scanner MUST automatically attempt WiFi reconnection every 10 seconds if connection drops during gameplay (handled by background connection monitor)
- **FR-006**: Scanner MUST display connection status on screen (Connected/Offline/Error)

#### Token Database Synchronization

- **FR-007**: Scanner SHOULD attempt to fetch token database via HTTP GET `/api/tokens` on boot when orchestrator is reachable (optional sync)
- **FR-008**: Scanner MUST save fetched token database to SD card file `/tokens.json` if sync succeeds (overwriting previous version)
- **FR-009**: Scanner MUST load token database from SD card file `/tokens.json` on boot (from sync or pre-loaded cache)
- **FR-010**: Scanner MUST be able to look up token metadata from loaded database (video presence, media paths, processing image paths)
- **FR-011**: Scanner MUST include pre-loaded `/tokens.json` cache in SD card image for offline-first operation

#### Scan Requests (All Tokens)

- **FR-012**: Scanner MUST send HTTP POST request to `/api/scan` when ANY token is scanned and orchestrator is connected
- **FR-013**: Scanner MUST include token ID, device ID, and team ID in scan request JSON payload
- **FR-014**: Scanner MUST include ISO 8601 timestamp in scan request
- **FR-015**: Scanner MUST timeout scan requests after 5 seconds if no response received
- **FR-016**: Scanner MUST display "Sending..." modal during scan request transmission
- **FR-017**: Scanner MUST display processing image (if available) in "Sending..." modal for tokens with processingImage field
- **FR-018**: Scanner MUST NOT block local content display while waiting for network response (fire-and-forget pattern)

#### Offline Queue Management

- **FR-019**: Scanner MUST append scans to queue file `/queue.jsonl` on SD card when orchestrator is unreachable
- **FR-020**: Scanner MUST store each queued scan as single-line JSON object (JSON Lines format)
- **FR-021**: Scanner MUST preserve scan order in queue (FIFO - first in, first out)
- **FR-022**: Scanner MUST limit queue size to 100 scans maximum
- **FR-023**: Scanner MUST remove oldest queued scan when appending 101st scan (FIFO overflow)
- **FR-024**: Scanner MUST display "Queue Full!" warning when overflow occurs
- **FR-025**: Scanner MUST persist queue through power cycles (queue survives reboot)

#### Queue Synchronization

- **FR-026**: Scanner MUST monitor orchestrator connectivity in background (check every 10 seconds)
- **FR-027**: Scanner MUST automatically upload queued scans when connection is restored
- **FR-028**: Scanner MUST send queued scans via HTTP POST `/api/scan/batch` endpoint in batches of up to 10 scans
- **FR-029**: Scanner MUST remove successfully uploaded scans from queue file after receiving 200 OK response
- **FR-030**: Scanner MUST retain scans in queue if batch upload fails (for retry on next connection check)
- **FR-031**: Scanner MUST delay 1 second between batch uploads when queue has more than 10 scans
- **FR-032**: Scanner MUST process queue in background without blocking new scans or content display

#### Local Content Display

- **FR-033**: Scanner MUST display token image from SD card when token has image field and file exists
- **FR-034**: Scanner MUST play token audio from SD card when token has audio field and file exists
- **FR-035**: Scanner MUST display token ID as text when token has no local media files (fallback display)
- **FR-036**: Scanner MUST allow player to dismiss content via double-tap gesture and return to ready mode
- **FR-037**: Scanner MUST display processing image from SD card when token has processingImage field during network operations

#### Status Display

- **FR-038**: Scanner MUST show status screen when player taps display once during idle mode
- **FR-039**: Status screen MUST display WiFi SSID and connection status
- **FR-040**: Status screen MUST display orchestrator connection status (Connected/Offline)
- **FR-041**: Status screen MUST display current queue size (number of scans awaiting upload)
- **FR-042**: Status screen MUST display configured team ID
- **FR-043**: Status screen MUST display device ID
- **FR-044**: Scanner MUST dismiss status screen and return to ready mode when player taps again

#### Device Identification

- **FR-045**: Scanner MUST generate unique device ID from MAC address on first boot if not configured in `/config.txt` (format: "SCANNER_" prefix + 12-character uppercase hex MAC without separators, e.g., "SCANNER_A1B2C3D4E5F6")
- **FR-046**: Scanner MUST persist auto-generated device ID to SD card file `/device_id.txt`
- **FR-047**: Scanner MUST use device ID from configuration file `/config.txt` if present (override auto-generated)
- **FR-048**: Scanner MUST include device ID in all scan requests sent to orchestrator

#### Error Handling

- **FR-049**: Scanner MUST display error message when WiFi connection fails after 30 seconds
- **FR-050**: Scanner MUST display error message when SD card is missing or unreadable
- **FR-051**: Scanner MUST continue functioning for local content display even when all network operations fail
- **FR-052**: Scanner MUST log network errors to serial output for debugging
- **FR-053**: Scanner MUST display token ID as text when configured media files are missing from SD card

### Key Entities *(include if feature involves data)*

- **Scan Request**: Represents any token scan sent to orchestrator (both video and non-video tokens). Contains token ID, device ID, team ID, and timestamp. Sent immediately when online, queued when offline. Purpose: Game activity logging and video trigger.

- **Queue Entry**: Represents a scan that could not be sent immediately. Stored as single-line JSON in `/queue.jsonl` file. Contains same fields as Scan Request. Persists through power cycles.

- **Token Metadata**: Information about a memory token loaded from tokens.json. Contains video filename (if video token), local media paths (image/audio for non-video tokens), processing image path, and game metadata (SF_RFID, SF_ValueRating, etc.). Used to determine display behavior and network requirements.

- **Configuration**: Persistent settings stored in `/config.txt` on SD card. Contains WiFi credentials, orchestrator URL, team ID, and optionally device ID. Edited by GM before gameplay. Format: simple key=value pairs (one per line, no sections, no comments). Example: `WIFI_SSID=VenueNetwork`, `ORCHESTRATOR_URL=http://10.0.0.100:3000`, `TEAM_ID=001`, `DEVICE_ID=SCANNER_01`.

- **Device ID**: Unique identifier for physical scanner device. Either auto-generated from MAC address (format: "SCANNER_" + 12-char hex MAC without separators, e.g., "SCANNER_A1B2C3D4E5F6") or manually configured by GM. Persisted to SD card. Included in all orchestrator requests for tracking purposes.

- **Connection State**: Runtime status of network connectivity. Three states: Connected (online, sending directly), Offline (queueing scans), WiFi Error (cannot connect). Drives visual indicators on display.

- **Processing Image**: Visual content shown during "Sending..." modal for video tokens. Stored locally on SD card in `/images/` folder. Guides player attention to shared display where video will play.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Scanner connects to configured WiFi and loads token database within 60 seconds of boot when network is operational
- **SC-002**: Scanner sends scan request and receives confirmation from orchestrator in under 2 seconds when connection is available
- **SC-003**: Scanner displays token local content (image/audio or processing image) within 1 second of scan, regardless of network status
- **SC-004**: Scanner queues scans successfully when offline and uploads all queued scans within 60 seconds of connection restoration (for queues up to 30 scans)
- **SC-005**: Scanner processes queue in background without introducing perceivable delay to new scans (new scan displays content within 1 second even during queue upload)
- **SC-006**: Queue file survives power cycle with zero data loss for scans written before power loss
- **SC-007**: Scanner handles 100 queued scans without performance degradation (queue processing, display responsiveness, scan latency all unaffected)
- **SC-008**: GM can configure scanner WiFi and orchestrator settings in under 5 minutes using SD card config file and computer text editor
- **SC-009**: Status screen displays accurate connection status and queue count within 500ms of tap gesture
- **SC-010**: Scanner operates continuously for 2+ hour gameplay session without requiring GM intervention (automatic reconnection, queue management, error recovery)
- **SC-011**: Scanner displays processing images for video tokens within 200ms of scan when processingImage files exist on SD card

---

## Assumptions

- Orchestrator server API conforms to OpenAPI specification at `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/backend/contracts/openapi.yaml`
- Orchestrator uses HTTP protocol (not HTTPS) - no TLS encryption on trusted venue network
- Player scanner endpoints (`/api/tokens`, `/api/scan`, `/api/scan/batch`, `/health`) require NO authentication - open API on trusted local network (admin endpoints use JWT)
- Orchestrator provides `/api/tokens` endpoint returning JSON with tokens object
- Orchestrator provides `/api/scan` endpoint accepting POST with tokenId, deviceId, teamId, and timestamp
- Orchestrator provides `/api/scan/batch` endpoint accepting POST with transactions array (for queue upload)
- Orchestrator provides `/health` endpoint for connection checks
- WiFi network uses WPA/WPA2 encryption (ESP32 native support)
- SD card filesystem is FAT32 (existing scanner configuration)
- SD card contains pre-loaded `/tokens.json`, `/images/` folder with all token images and processing images, and `/audio/` folder with all token audio files
- Venue network latency is typically under 500ms (WiFi environment)
- Orchestrator can handle 10+ simultaneous scanner connections without significant slowdown
- Power failures are infrequent enough that 100-scan queue limit is sufficient buffer
- Token database size remains under 50KB (fits comfortably in ESP32 RAM for parsing)
- Video tokens typically have processingImage only (no image/audio for local playback)
- Non-video tokens typically have image and/or audio for local playback (no video field)
- GM has access to computer with text editor and SD card reader for configuration
- Physical scanner case does not provide access to RGB LEDs or physical buttons during gameplay
- Players are familiar with double-tap gesture from existing scanner operation

---

## Out of Scope (PWA Features NOT Ported)

The following features exist in the PWA reference implementation but are **intentionally excluded** from hardware scanner:

- **QR Code Scanning**: Hardware uses RFID exclusively. QR scanning is PWA-only for testing without RFID readers.
- **NFC Scanning**: Hardware uses MFRC522 RFID. NFC is PWA browser feature for compatible phones.
- **Browser Service Workers**: PWA offline caching mechanism not applicable to embedded system.
- **PWA Installation Prompts**: Hardware is native embedded system, not installed web app.
- **Path-Based Mode Detection**: PWA detects standalone vs. networked mode by URL path (`/player-scanner/`). Hardware is always networked mode when orchestrator configured.
- **LocalStorage API**: Hardware uses SD card filesystem instead of browser storage.
- **Web Fetch API**: Hardware uses ESP32 HTTPClient library instead of browser networking.
- **Manual Token Entry**: Hardware scanner does not provide keyboard-based token ID entry (RFID-only workflow).
- **Browser-Based UI Components**: Hardware has physical TFT display with embedded graphics, not HTML/CSS.
- **Player Team Selection UI**: PWA hardcodes team ID or uses sessionStorage. Hardware receives team assignment from GM configuration (not player-selectable).
- **Collection Progress Tracking**: PWA tracks "collected memories" in browser localStorage. Hardware sends scans to orchestrator; tracking is server-side responsibility.
- **Character Entry for Configuration**: PWA allows text input via browser keyboard. Hardware configuration uses SD card files edited on computer (no on-device typing).
- **Selective Scan Sending (only video tokens)**: PWA only sends scans for video tokens. Hardware sends ALL scans to orchestrator for complete activity logging.

These exclusions are intentional design decisions based on hardware capabilities and production deployment requirements.

---

## Dependencies

- **Existing Hardware Scanner**: ALNScanner0812Working v3.4 (BMP display, I2S audio, RFID reading)
- **PWA Reference Implementation**: `/home/maxepunk/projects/AboutLastNight/aln-memory-scanner` (orchestrator integration patterns)
- **Orchestrator Backend**: `/home/maxepunk/projects/AboutLastNight/ALN-Ecosystem/backend` (API endpoints)
- **Token Database**: `/home/maxepunk/projects/AboutLastNight/ALN-TokenData/tokens.json` (shared data source)
- **ESP32 Arduino Libraries**: HTTPClient (networking), ArduinoJson (JSON parsing), SD (file I/O), WiFi (connectivity)
- **SD Card Image**: Pre-loaded with `/tokens.json`, `/images/` folder (token images and processing images), `/audio/` folder (token audio files)

---

## Notes for Implementation

- **All scans sent to orchestrator** (not just video tokens) - provides complete game activity log
- **Video tokens typically have processingImage only** - no local image/audio playback content
- **Non-video tokens have image/audio** - displayed/played from SD card immediately after scan
- **Processing images stored locally** on SD card in `/images/` folder alongside regular token images
- Token database sync optional on boot (not critical) - scanner works with pre-loaded cache indefinitely
- Connection monitoring runs in background FreeRTOS task to avoid blocking RFID scanning (checks every 10 seconds)
- Background task handles both orchestrator connectivity checks AND WiFi reconnection if connection drops
- All network I/O has 5-second timeout to prevent hanging
- Queue processing uses batch endpoint to minimize HTTP overhead (10 scans per request)
- Device ID auto-generated from MAC address ensures uniqueness without manual configuration (format: "SCANNER_A1B2C3D4E5F6")
- Configuration file uses simple key=value format (one per line, no sections or comments) for easy editing by non-technical GMs on any text editor
- Required config keys: WIFI_SSID, WIFI_PASSWORD, ORCHESTRATOR_URL, TEAM_ID (DEVICE_ID optional, auto-generated if missing)
- Status display provides diagnostic capability for GMs during setup and players during gameplay
- Fire-and-forget pattern for scan requests (don't block on response parsing) maintains responsive UX
- Local content display never waits for network operations - content always displays immediately
- Offline queue uses JSON Lines format (newline-delimited) for atomic append operations and corruption resistance

