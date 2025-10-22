# Contract: Offline Queue File Format

**Feature**: 003-orchestrator-hardware-integration
**File**: `/queue.jsonl` on SD card
**Date**: 2025-10-19
**Status**: Contract Definition

## Purpose

Define the exact format, operations, and validation rules for the offline queue that persists scans when the orchestrator is unreachable.

---

## Format Specification

### File Location

- **Path**: `/queue.jsonl` (root of SD card FAT32 filesystem)
- **Created by**: Scanner (automatically when first scan fails to send)
- **Managed by**: Scanner (append, read, delete operations)
- **Persists**: Through power cycles (survives scanner reboot)

### File Format

- **Encoding**: UTF-8
- **Line Endings**: LF (`\n`) - Unix style
- **Structure**: JSON Lines (JSONL) - one JSON object per line, newline-delimited
- **Compression**: None (uncompressed for simplicity)
- **Maximum Size**: 100 lines (FIFO overflow when exceeding)

### JSON Lines (JSONL) Standard

**Spec**: https://jsonlines.org/

**Key Requirements**:
- Each line is a complete, valid JSON object
- Lines separated by newline character (`\n`)
- NO comma separators between objects (unlike JSON arrays)
- NO square brackets wrapping the file
- Each object can be parsed independently

---

## Queue Entry Schema

### JSON Object Fields

| Field | Type | Required | Description | Example |
|-------|------|----------|-------------|---------|
| `tokenId` | String | Yes | Token identifier from RFID scan | `"534e2b03"` |
| `teamId` | String | No | Team identifier from config (optional) | `"001"` |
| `deviceId` | String | Yes | Scanner device identifier | `"SCANNER_A1B2C3D4E5F6"` |
| `timestamp` | String | Yes | Scan timestamp in ISO 8601 format | `"2025-10-19T14:30:00.000Z"` |

### JSON Object Example

```json
{"tokenId":"534e2b03","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:30:00.000Z"}
```

**Formatting Rules**:
- NO pretty-printing (no indentation or extra whitespace)
- Compact format (minimizes file size)
- Field order not guaranteed (JSON objects are unordered)
- String values enclosed in double quotes
- NO internal newlines within JSON object (all on one line)

---

## Complete File Example

```
{"tokenId":"534e2b02","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:25:00.000Z"}
{"tokenId":"534e2b03","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:30:00.000Z"}
{"tokenId":"tac001","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:35:00.000Z"}
```

**Note**: Each line ends with newline (`\n`), including the last line.

---

## File Operations

### 1. Append (Queue a Scan)

**Trigger**: Scan request fails to send to orchestrator (offline or timeout)

**Operation**:
```cpp
void queueScan(String tokenId, String teamId, String deviceId, String timestamp) {
  // Check queue size first (FIFO overflow if at limit)
  if (countLines("/queue.jsonl") >= 100) {
    removeOldestEntry(); // FIFO: remove first line
  }

  // Create JSON object
  JsonDocument doc;
  doc["tokenId"] = tokenId;
  if (teamId.length() > 0) {
    doc["teamId"] = teamId;
  }
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;

  // Serialize to string
  String jsonLine;
  serializeJson(doc, jsonLine);

  // Append to file
  File file = SD.open("/queue.jsonl", FILE_APPEND);
  if (file) {
    file.println(jsonLine); // Adds '\n' automatically
    file.flush();           // Force write to SD card
    file.close();
  }
}
```

**Important**: `file.flush()` ensures data written to SD card before close (minimizes data loss on power failure).

---

### 2. Read (Load Queue for Upload)

**Trigger**: Background task detects orchestrator is reachable and queue is not empty

**Operation**:
```cpp
void readQueue(std::vector<QueueEntry>& entries, int maxEntries = 10) {
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (!file) return;

  int count = 0;
  while (file.available() && count < maxEntries) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue; // Skip empty lines

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, line);

    if (error) {
      // Corruption: skip invalid line
      Serial.println("Queue: Skipping invalid JSON line");
      continue;
    }

    // Validate required fields
    if (!doc.containsKey("tokenId") ||
        !doc.containsKey("deviceId") ||
        !doc.containsKey("timestamp")) {
      Serial.println("Queue: Skipping entry missing required fields");
      continue;
    }

    // Add to batch
    QueueEntry entry;
    entry.tokenId = doc["tokenId"].as<String>();
    entry.teamId = doc["teamId"].as<String>(); // May be empty
    entry.deviceId = doc["deviceId"].as<String>();
    entry.timestamp = doc["timestamp"].as<String>();
    entries.push_back(entry);

    count++;
  }

  file.close();
}
```

**Batch Size**: Read up to 10 entries at a time (matches batch upload limit).

**Corruption Handling**: Skip invalid JSON lines (graceful recovery from power-loss corruption).

---

### 3. Remove (Delete Uploaded Entries)

**Trigger**: Batch upload succeeds (HTTP 200 OK from `/api/scan/batch`)

**Operation** (remove first N lines):
```cpp
void removeUploadedEntries(int numEntries) {
  // Read entire file into memory
  std::vector<String> allLines;
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        allLines.push_back(line);
      }
    }
    file.close();
  }

  // Remove first N entries (uploaded batch)
  if (numEntries >= allLines.size()) {
    // All entries uploaded, delete file
    SD.remove("/queue.jsonl");
    return;
  }

  // Remove first N lines
  allLines.erase(allLines.begin(), allLines.begin() + numEntries);

  // Rewrite file with remaining entries
  SD.remove("/queue.jsonl"); // Delete old file
  file = SD.open("/queue.jsonl", FILE_WRITE);
  if (file) {
    for (String& line : allLines) {
      file.println(line);
    }
    file.flush();
    file.close();
  }
}
```

**Alternative**: If all entries uploaded, delete file entirely (`SD.remove("/queue.jsonl")`).

---

### 4. Count (Get Queue Size)

**Trigger**: Display status screen, check FIFO overflow

**Operation**:
```cpp
int countQueueEntries() {
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) count++;
  }

  file.close();
  return count;
}
```

**Optimization**: Cache queue size in RAM variable, update on append/remove (avoids frequent SD card reads).

---

## FIFO Overflow Behavior

### Capacity Limit

**Maximum Queue Size**: 100 entries

**Rationale**:
- Prevents unbounded SD card usage
- Balances data loss risk with storage capacity
- Matches PWA reference implementation

### Overflow Algorithm

**Trigger**: Attempting to queue 101st entry

**Behavior**:
1. Remove oldest entry (first line in file)
2. Append new entry (last line in file)
3. Display warning: "Queue Full!" (500ms yellow text)
4. Player still sees local content (scan not lost, just orchestrator logging delayed)

**Implementation**:
```cpp
void handleQueueOverflow() {
  // Remove first line (oldest entry)
  std::vector<String> allLines;
  File file = SD.open("/queue.jsonl", FILE_READ);
  if (file) {
    bool skipFirst = true;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        if (skipFirst) {
          skipFirst = false; // Skip oldest entry
        } else {
          allLines.push_back(line);
        }
      }
    }
    file.close();
  }

  // Rewrite file without oldest entry
  SD.remove("/queue.jsonl");
  file = SD.open("/queue.jsonl", FILE_WRITE);
  if (file) {
    for (String& line : allLines) {
      file.println(line);
    }
    file.flush();
    file.close();
  }

  // Now queue is 99 entries, can append new entry
}
```

---

## Validation Rules

### File-Level Validation

| Rule | Validation | Error Handling |
|------|------------|----------------|
| File exists | Check on boot, create if missing | Create empty file if needed |
| Line count ≤100 | Count on append | FIFO overflow (remove oldest) |
| UTF-8 encoding | Assume UTF-8, handle parse errors | Skip invalid lines |

### Entry-Level Validation

| Rule | Validation | Error Handling |
|------|------------|----------------|
| Valid JSON | `deserializeJson()` | Skip line, log error to serial |
| Required fields | Check `tokenId`, `deviceId`, `timestamp` present | Skip entry missing fields |
| `tokenId` not empty | String length > 0 | Skip entry if empty |
| `timestamp` valid ISO 8601 | Pattern check (optional) | Accept as-is (orchestrator validates) |

---

## Corruption Recovery

### Causes of Corruption

- **Power loss during write**: SD card sector write interrupted
- **SD card removal during write**: File write incomplete
- **Filesystem corruption**: FAT32 table corruption

### Recovery Strategy

**On Boot**:
1. Attempt to open `/queue.jsonl`
2. If open fails → File corrupted, delete and recreate empty file
3. Read file line by line
4. For each line:
   - Parse JSON
   - If parse fails → Skip line (log to serial)
   - If required fields missing → Skip entry
   - If valid → Add to in-memory queue
5. Rewrite file with only valid entries (self-healing)

**Implementation**:
```cpp
void recoverQueue() {
  std::vector<String> validLines;
  File file = SD.open("/queue.jsonl", FILE_READ);

  if (!file) {
    // File corrupted or missing, recreate
    Serial.println("Queue: File corrupted, recreating");
    SD.remove("/queue.jsonl");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    // Validate JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, line);

    if (error || !doc.containsKey("tokenId") ||
        !doc.containsKey("deviceId") || !doc.containsKey("timestamp")) {
      Serial.println("Queue: Skipping corrupt entry");
      continue; // Skip corrupt entry
    }

    validLines.push_back(line);
  }

  file.close();

  // Rewrite file with only valid entries
  SD.remove("/queue.jsonl");
  file = SD.open("/queue.jsonl", FILE_WRITE);
  if (file) {
    for (String& line : validLines) {
      file.println(line);
    }
    file.flush();
    file.close();
  }

  Serial.printf("Queue: Recovered %d valid entries\n", validLines.size());
}
```

---

## Batch Upload Format

### HTTP Request to /api/scan/batch

**Endpoint**: `POST /api/scan/batch`

**Request Body**:
```json
{
  "transactions": [
    {"tokenId":"534e2b02","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:25:00.000Z"},
    {"tokenId":"534e2b03","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:30:00.000Z"},
    {"tokenId":"tac001","teamId":"001","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:35:00.000Z"}
  ]
}
```

**Batch Size**: Up to 10 entries per request

**Delay Between Batches**: 1 second (if queue has >10 entries)

**Generation**:
```cpp
void uploadQueueBatch() {
  std::vector<QueueEntry> batch;
  readQueue(batch, 10); // Load up to 10 entries

  if (batch.empty()) return;

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

  // Serialize to JSON string
  String requestBody;
  serializeJson(doc, requestBody);

  // Send HTTP POST
  HTTPClient http;
  http.begin(orchestratorUrl + "/api/scan/batch");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000); // 5-second timeout

  int httpCode = http.POST(requestBody);

  if (httpCode == 200) {
    // Success: remove uploaded entries from queue
    removeUploadedEntries(batch.size());
    Serial.println("Queue: Batch uploaded successfully");

    // Upload next batch if queue not empty (with 1s delay)
    if (countQueueEntries() > 0) {
      delay(1000);
      uploadQueueBatch(); // Recursive call for next batch
    }
  } else {
    // Failure: keep in queue, retry on next connection check (10s)
    Serial.printf("Queue: Batch upload failed (HTTP %d)\n", httpCode);
  }

  http.end();
}
```

---

## Test Cases

### Test Case 1: Append Single Entry

**Initial State**: Queue file does not exist

**Operation**: Queue one scan entry

**Expected**: File created with 1 line

**File Contents**:
```
{"tokenId":"534e2b02","deviceId":"SCANNER_A1B2C3D4E5F6","timestamp":"2025-10-19T14:25:00.000Z"}
```

---

### Test Case 2: Append Multiple Entries

**Initial State**: Queue file with 2 entries

**Operation**: Queue 3 more scans

**Expected**: File contains 5 lines total

---

### Test Case 3: FIFO Overflow

**Initial State**: Queue file with 100 entries

**Operation**: Queue 1 more scan

**Expected**:
- Oldest entry (first line) removed
- New entry appended (last line)
- File contains 100 lines total
- Display shows "Queue Full!" warning

---

### Test Case 4: Batch Upload Success

**Initial State**: Queue file with 15 entries

**Operation**: Upload first batch (10 entries), receive HTTP 200 OK

**Expected**:
- First 10 entries removed from queue
- File contains 5 entries remaining
- Second batch automatically uploaded after 1-second delay

---

### Test Case 5: Batch Upload Failure

**Initial State**: Queue file with 10 entries

**Operation**: Upload batch, receive HTTP timeout

**Expected**:
- All 10 entries remain in queue
- Background task retries in 10 seconds

---

### Test Case 6: Corrupted Queue Recovery

**Initial State**: Queue file with 5 valid entries + 2 corrupt lines (invalid JSON)

**Operation**: Scanner boots, reads queue

**Expected**:
- 5 valid entries loaded into memory
- 2 corrupt lines skipped
- File rewritten with only 5 valid entries
- Serial log shows "Queue: Skipping corrupt entry" (2 times)

---

### Test Case 7: Empty Queue

**Initial State**: Queue file empty (0 lines)

**Operation**: Read queue for upload

**Expected**: No entries returned, no upload attempt

---

### Test Case 8: Queue Full During Extended Outage

**Initial State**: Queue file with 98 entries

**Operation**: Queue 10 more scans during network outage

**Expected**:
- First 2 scans → Queue grows to 99, then 100
- Next 8 scans → Each removes oldest entry (FIFO)
- Final queue: 100 entries (entries 9-108, entries 1-8 lost)
- Display shows "Queue Full!" 8 times

---

## Performance Considerations

### Write Performance

- **Append**: O(1) - fast, atomic operation (file opened, line written, closed immediately)
- **FIFO Overflow**: O(n) - requires reading entire file, removing first line, rewriting

**Optimization**: Keep queue size below 100 to avoid expensive overflow operations.

### Read Performance

- **Read Batch (10 entries)**: O(n) where n = 10 - fast, sequential read
- **Count Entries**: O(n) where n = queue size - can be optimized with RAM cache

**Optimization**: Cache queue size in RAM variable, update on append/remove.

### Storage Usage

- **Per Entry**: ~100-150 bytes (JSON object + newline)
- **Maximum Queue**: 100 entries × 150 bytes = 15KB max
- **SD Card Space**: Negligible impact on typical 8GB+ SD cards

---

**Contract Complete**: 2025-10-19
**Version**: 1.0
