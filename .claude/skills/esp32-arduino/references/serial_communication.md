# Serial Communication Patterns for ESP32

This guide covers reliable patterns for ESP32 serial communication, including monitoring, logging, command interaction, and boot capture.

## Core Principles

### Serial Port Exclusivity
**Critical:** Serial ports can only be opened by ONE process at a time. Multiple processes attempting to access the same port will cause conflicts.

**Before any serial operation:**
```bash
# Check if port is in use
lsof /dev/ttyUSB0

# Kill any existing serial monitors
pkill -f "cat /dev/ttyUSB0"
pkill -f "arduino-cli monitor"
pkill screen
```

### Baud Rate Consistency
ESP32 default: **115200 baud**
- Must match in code: `Serial.begin(115200)`
- Must match in monitor: all tools must use 115200
- Mismatch causes garbled output or no output

### Boot Sequence Timing
- Boot messages start immediately on reset
- Setup() logging appears within 1-2 seconds
- Missing boot messages indicates:
  - Serial monitor started too late
  - Baud rate mismatch
  - Device not actually resetting
  - Code crashed before Serial.begin()

## Decision Tree: Choose the Right Approach

```
Need to...
├─ Capture boot sequence?
│  └─> Use: Persistent monitor with hardware reset
│      Script: capture_boot.sh
│
├─ Monitor continuous output?
│  └─> Use: Simple cat or arduino-cli monitor
│      Script: monitor_serial.sh
│
├─ Send commands and read responses?
│  └─> Use: Interactive script with delays
│      Script: send_serial_command.sh
│
├─ Test serial command processing?
│  └─> Use: Automated test script
│      Script: test_serial_commands.sh
│
└─ Debug why no output?
   └─> Use: Serial diagnostic workflow
       See: "Diagnosing Serial Issues" below
```

## Pattern 1: Capturing Boot Sequence

**Challenge:** Boot messages start immediately on reset, easy to miss.

**Solution:** Start monitoring BEFORE hardware reset.

```bash
# Method 1: Using cat (simplest)
cat /dev/ttyUSB0 > boot.log &
MONITOR_PID=$!
echo "Press RESET button NOW"
sleep 10
kill $MONITOR_PID
cat boot.log
```

**Critical timing:**
1. Start monitor first (background process)
2. Press hardware RESET button
3. Wait for complete boot (10 seconds safe)
4. Stop monitor
5. Examine log

**Common mistake:** Trying to monitor after reset - boot messages already gone.

## Pattern 2: Continuous Monitoring

**Use case:** Watch ongoing device output.

**Simple approach:**
```bash
# Clean any existing monitors
pkill -f "cat /dev/ttyUSB0" 2>/dev/null

# Start fresh monitor
cat /dev/ttyUSB0
```

**With timestamps:**
```bash
cat /dev/ttyUSB0 | while IFS= read -r line; do
  echo "[$(date +%H:%M:%S)] $line"
done
```

**To file and screen:**
```bash
cat /dev/ttyUSB0 | tee serial_output.log
```

## Pattern 3: Sending Commands

**Challenge:** ESP32 processes Serial input asynchronously. Need delays between commands.

**Reliable command sending:**
```bash
# Function to send command and wait for response
send_command() {
  local cmd="$1"
  local wait_time="${2:-2}"  # Default 2 seconds
  
  echo "Sending: $cmd"
  echo "$cmd" > /dev/ttyUSB0
  sleep "$wait_time"
}

# Example usage
send_command "STATUS" 3
send_command "HELP" 2
```

**Critical delays:**
- After sending command: Wait 1-3 seconds for processing
- Between commands: Wait 1-2 seconds minimum
- After device operations: Wait longer (5-10 seconds for WiFi, file operations)

## Pattern 4: Interactive Command Testing

**Complete workflow for command-response testing:**

```bash
#!/bin/bash
# Interactive serial command tester

PORT="/dev/ttyUSB0"
LOGFILE="serial_test_$(date +%Y%m%d_%H%M%S).log"

# Stop any existing monitors
pkill -f "cat $PORT" 2>/dev/null
sleep 1

# Start background monitor
cat "$PORT" | tee "$LOGFILE" &
MONITOR_PID=$!
echo "Monitor started (PID: $MONITOR_PID)"
sleep 2

# Function to send command
send_cmd() {
  echo ""
  echo ">>> Sending: $1"
  echo "$1" > "$PORT"
  sleep 3  # Wait for response
}

# Send test commands
send_cmd "HELP"
send_cmd "STATUS"
send_cmd "VERSION"

# Wait and cleanup
echo ""
echo "Waiting 5 seconds for final output..."
sleep 5

kill $MONITOR_PID 2>/dev/null
echo ""
echo "Test complete. Log saved to: $LOGFILE"
```

## Pattern 5: Automated Testing

**For CI/CD or repeated testing:**

```bash
#!/bin/bash
# Automated serial command test with validation

test_command() {
  local cmd="$1"
  local expected_pattern="$2"
  local timeout="${3:-5}"
  
  echo "Testing command: $cmd"
  
  # Clear buffer
  timeout 0.5 cat /dev/ttyUSB0 > /dev/null 2>&1
  
  # Send command and capture response
  (echo "$cmd" > /dev/ttyUSB0) &
  sleep 1
  
  response=$(timeout "$timeout" cat /dev/ttyUSB0)
  
  # Validate response
  if echo "$response" | grep -q "$expected_pattern"; then
    echo "  ✓ PASS: Found '$expected_pattern'"
    return 0
  else
    echo "  ✗ FAIL: Expected '$expected_pattern' not found"
    echo "  Response: $response"
    return 1
  fi
}

# Example tests
test_command "STATUS" "Firmware Version" 5
test_command "HELP" "Available commands" 5
test_command "UPTIME" "seconds" 3
```

## Diagnosing Serial Issues

### Symptom: No output at all

**Step-by-step diagnosis:**

```bash
# 1. Verify port exists
ls -la /dev/ttyUSB0
# If not found: USB not connected or driver issue

# 2. Check permissions
ls -la /dev/ttyUSB0
# Should show: crw-rw---- 1 root dialout
groups
# Should include: dialout

# 3. Check if port is in use
lsof /dev/ttyUSB0
# If shows process: kill that process first

# 4. Verify baud rate
stty -F /dev/ttyUSB0
# Should show: speed 115200 baud

# 5. Try simple read
timeout 5 cat /dev/ttyUSB0
# Should see output within 5 seconds if device is running
```

### Symptom: Garbled output

**Cause:** Baud rate mismatch

**Solution:**
```bash
# Set correct baud rate
stty -F /dev/ttyUSB0 115200

# Or try common alternatives
stty -F /dev/ttyUSB0 9600
stty -F /dev/ttyUSB0 230400
```

### Symptom: Output stops after a while

**Causes:**
1. Buffer overflow (device side)
2. Monitor process died
3. USB disconnect

**Check:**
```bash
# Is monitor still running?
ps aux | grep "cat /dev/ttyUSB0"

# Is USB connected?
lsusb | grep -i "CP210\|CH340\|FTDI"

# Check kernel messages
dmesg | tail -20 | grep tty
```

### Symptom: Commands not processed

**Diagnostic steps:**

```bash
# 1. Verify device is running (should see periodic output)
timeout 10 cat /dev/ttyUSB0

# 2. Check if Serial.available() is being called in loop()
# Code must have: while (Serial.available()) { ... }

# 3. Test with very simple command
echo "TEST" > /dev/ttyUSB0
sleep 2
timeout 3 cat /dev/ttyUSB0

# 4. Verify line endings
echo -e "HELP\n" > /dev/ttyUSB0  # With newline
echo -e "HELP\r\n" > /dev/ttyUSB0  # With CR+LF

# 5. Check if commands are echoed back
# Some devices echo commands: HELP -> "HELP" in output
```

## Background Task Conflicts

### Problem: Loop() blocked, commands delayed

When ESP32 code has:
- Long `delay()` in loop()
- Blocking operations (WiFi.begin(), RFID scanning)
- Heavy processing without yield

**Symptoms:**
- Commands processed slowly
- Inconsistent response times
- Some commands ignored

**Solutions:**

**Code pattern 1: Non-blocking loop**
```cpp
unsigned long lastCommandCheck = 0;

void loop() {
  // Check commands frequently
  if (millis() - lastCommandCheck >= 10) {
    processSerialCommands();
    lastCommandCheck = millis();
  }
  
  // Other non-blocking operations
  doNonBlockingWork();
  
  yield();  // Let system tasks run
}
```

**Code pattern 2: Dedicated command task (FreeRTOS)**
```cpp
void serialCommandTask(void *parameter) {
  while (true) {
    if (Serial.available()) {
      processSerialCommands();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreatePinnedToCore(
    serialCommandTask,
    "SerialCmd",
    4096,
    NULL,
    2,  // Higher priority
    NULL,
    1   // Core 1
  );
}
```

**Code pattern 3: Multiple check points**
```cpp
void loop() {
  processSerialCommands();  // Check 1
  
  // Do some work
  doWork1();
  
  processSerialCommands();  // Check 2
  
  // More work
  doWork2();
  
  processSerialCommands();  // Check 3
}
```

## Best Practices Summary

### For Monitoring
1. **Always stop existing monitors first**
2. Use simple `cat` for basic monitoring
3. Use `tee` to see and log simultaneously
4. Add timestamps for debugging timing issues

### For Boot Capture
1. **Start monitor BEFORE reset**
2. Use hardware RESET button (not software)
3. Wait 10 seconds minimum for complete boot
4. Check log file size (0 bytes = capture failed)

### For Command Sending
1. **Wait between commands** (2-3 seconds minimum)
2. Use newline termination (`\n`)
3. Flush outputs after sending
4. Verify device is ready before sending next command

### For Testing
1. **Isolate tests** - one command per test run
2. Clear serial buffer before reading response
3. Use timeouts to prevent hanging
4. Validate responses programmatically

### For Debugging
1. **Simplify first** - test with echo command
2. Check permissions and port status
3. Verify baud rate matches code
4. Test with minimal code (just Serial.print in loop)
5. Use logic analyzer if software methods fail

## Common Pitfalls to Avoid

❌ **Don't:** Open multiple monitors simultaneously
✅ **Do:** Kill existing monitors before starting new one

❌ **Don't:** Send commands too quickly
✅ **Do:** Wait 2-3 seconds between commands

❌ **Don't:** Assume device reset on upload
✅ **Do:** Press hardware RESET and verify boot messages

❌ **Don't:** Use background processes without proper cleanup
✅ **Do:** Track PIDs and kill on exit

❌ **Don't:** Ignore empty log files
✅ **Do:** Check file size, indicates monitoring failed

❌ **Don't:** Test during device blocking operations
✅ **Do:** Wait for device to be idle (check periodic output)

## Troubleshooting Decision Matrix

| Symptom | Check | Solution |
|---------|-------|----------|
| No output at all | Port exists? Permissions? | Add to dialout group |
| Garbled output | Baud rate | Set to 115200 |
| Missing boot messages | Monitor timing | Start before reset |
| Commands ignored | Loop blocking | Add processSerial() calls |
| Intermittent responses | Timing | Increase delays |
| "Permission denied" | Group membership | Add to dialout, logout/login |
| Port in use | lsof check | Kill conflicting process |
| Buffer overflow | Output rate | Reduce Serial.print() calls |

## Next Steps

When serial communication fails:
1. Start with simplest test: `timeout 5 cat /dev/ttyUSB0`
2. If that works, issue is with command processing (code side)
3. If that fails, issue is with serial connection (hardware/driver side)
4. Use diagnostic workflow above to isolate
5. Consult hardware_constraints.md for GPIO TX/RX pin issues
