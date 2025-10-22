# Serial Communication Testing and Debugging

This guide provides systematic approaches for testing serial communication, capturing output, and debugging serial command issues on ESP32 projects.

## Serial Testing Workflow

When developing ESP32 projects with serial command interfaces, follow this systematic workflow to ensure reliable communication:

### Phase 1: Verify Basic Serial Connection

**Before testing commands, verify serial communication works at all.**

Use the connection test script:
```bash
./scripts/test_serial_connection.sh
```

This script checks:
1. Port detection and accessibility
2. Port permissions
3. Port configuration
4. Data reception

**Expected result:** Should receive data (boot messages, debug output, etc.)

**If test fails:**
- Check physical USB connection
- Verify baudrate matches code (usually 115200)
- Check cable (must be data cable, not charge-only)
- Try different baudrate: `--baudrate 9600`

### Phase 2: Capture Boot Sequence

**Verify ESP32 boots correctly and Serial.begin() executes.**

```bash
# Capture 10 seconds after manual reset
./scripts/capture_serial.sh --duration 10 --wait 2
```

While script is waiting, press RESET button on ESP32.

**Expected boot messages:**
```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
[Setup] Initializing...
[Setup] Serial ready
```

**If no boot messages:**
- ESP32 not executing code (upload issue)
- Serial.begin() not called or too late
- Code crashes before Serial initialized
- Wrong baudrate

### Phase 3: Monitor Continuous Output

**Watch real-time serial output to understand ESP32 behavior.**

```bash
./scripts/monitor_serial.sh
```

**What to look for:**
- Regular output (background tasks, sensors)
- Error messages
- Watchdog resets
- Memory warnings

Press Ctrl+C to exit monitor.

### Phase 4: Test Single Command

**Send one command and check for response.**

```bash
./scripts/test_serial_command.sh --command "HELP"
```

**Expected:**
- Command sent successfully
- Response received within timeout
- Response content makes sense

**If no response:**
- Command not being processed (check code)
- Command format wrong (check expected format)
- Response goes to different output
- Timeout too short (try `--timeout 10`)

### Phase 5: Interactive Testing

**Test multiple commands in sequence.**

```bash
./scripts/test_serial_command.sh --interactive
```

Type commands and press Enter. Watch for responses.

**Useful for:**
- Testing command sequences
- Debugging state machines
- Verifying command processing timing

## Common Serial Issues and Solutions

### Issue: No Serial Output at All

**Symptoms:** Port exists, but no data received

**Diagnosis workflow:**
```bash
# 1. Test connection
./scripts/test_serial_connection.sh

# 2. Try common baudrates
./scripts/test_serial_connection.sh --baudrate 9600
./scripts/test_serial_connection.sh --baudrate 230400

# 3. Check for boot messages after reset
./scripts/capture_serial.sh --duration 5 --wait 1
# Press RESET while waiting
```

**Common causes:**
1. **Code never calls Serial.begin()** - Add to setup()
2. **Wrong baudrate** - ESP32 code and monitoring must match
3. **Serial output on different UART** - Check Serial vs Serial1/Serial2
4. **Code crashes before setup()** - Check global initializations
5. **Watchdog reset before Serial.begin()** - Add watchdog feed

**Solution pattern:**
```cpp
void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial to initialize
  Serial.println("\n\n=== Device Starting ===");
  // Rest of setup
}
```

### Issue: Boot Messages But No Command Responses

**Symptoms:** See boot output, but commands get no response

**Diagnosis:**

1. **Verify commands are being read:**
```cpp
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    Serial.printf("Received command: [%s]\n", cmd.c_str());
    // Process command
  }
}
```

2. **Check command processing frequency:**
```bash
# Monitor output while sending commands
./scripts/monitor_serial.sh &
MONITOR_PID=$!
sleep 2
echo "TEST" > /dev/ttyUSB0
sleep 1
kill $MONITOR_PID
```

**Common causes:**
1. **Serial check blocked by long delays** - Use non-blocking patterns
2. **Serial buffer overflow** - Process more frequently
3. **Commands consumed but not processed** - Check command parsing logic
4. **Wrong line ending** - Commands need newline (\n)

**Solution patterns:**

**❌ Bad: Blocking delays prevent serial processing**
```cpp
void loop() {
  doWork();
  delay(5000);  // Serial not checked for 5 seconds!
}
```

**✅ Good: Non-blocking pattern**
```cpp
unsigned long lastWork = 0;

void loop() {
  // Process serial FIRST
  if (Serial.available()) {
    processCommand();
  }
  
  // Non-blocking work
  if (millis() - lastWork >= 5000) {
    lastWork = millis();
    doWork();
  }
}
```

**✅ Good: Dedicated serial check function**
```cpp
void loop() {
  processSerialCommands();  // Call multiple times per loop
  
  // Other work
  if (shouldScan()) {
    doScan();
  }
  
  processSerialCommands();  // Call again
  
  updateDisplay();
  
  processSerialCommands();  // And again
}
```

### Issue: Intermittent Command Responses

**Symptoms:** Sometimes works, sometimes doesn't

**Diagnosis:**
```bash
# Send 10 commands and count responses
for i in {1..10}; do
  echo "Command $i:"
  ./scripts/test_serial_command.sh --command "STATUS" --timeout 2
  sleep 1
done | tee test_results.log

# Count successes
grep "Received" test_results.log | wc -l
```

**Common causes:**
1. **Race conditions** - Serial checked at wrong time
2. **Task priorities** - Other tasks starving serial processing
3. **Interrupt conflicts** - ISRs interfering with serial
4. **Buffer issues** - Serial buffer too small or not cleared

**Solution: Increase serial processing frequency**
```cpp
// Call processSerial() more often
void loop() {
  processSerial();
  doTask1();
  processSerial();
  doTask2();
  processSerial();
}

// Or use hardware serial events
void serialEvent() {
  while (Serial.available()) {
    processCommand();
  }
}
```

### Issue: Garbled Serial Output

**Symptoms:** Received data is corrupted or unreadable

**Diagnosis:**
```bash
# Capture raw output
./scripts/capture_serial.sh --duration 10 --output raw.log

# Check for patterns
cat raw.log | head -50
```

**Common causes:**
1. **Baudrate mismatch** - Most common cause
2. **Multiple serial outputs** - Different code sections using different rates
3. **Buffer overflow** - Data loss causing sync issues
4. **Hardware issue** - Cable, connector, or chip problem

**Solution:**
```bash
# Try standard baudrates
for baud in 9600 115200 230400; do
  echo "Testing $baud baud..."
  ./scripts/test_serial_connection.sh --baudrate $baud --duration 3
done
```

## Testing Serial Commands Programmatically

For automated testing or CI/CD, use Python or bash scripts.

### Python Test Script Example

```python
#!/usr/bin/env python3
import serial
import time

def test_serial_commands(port='/dev/ttyUSB0', baudrate=115200):
    """Test serial commands systematically"""
    
    # Open serial connection
    ser = serial.Serial(port, baudrate, timeout=2)
    time.sleep(2)  # Let port stabilize
    
    # Clear any pending data
    ser.reset_input_buffer()
    
    # Test commands
    commands = ['HELP', 'STATUS', 'VERSION']
    results = {}
    
    for cmd in commands:
        print(f"Testing: {cmd}")
        
        # Send command
        ser.write(f"{cmd}\n".encode())
        
        # Collect response
        response = []
        start_time = time.time()
        
        while time.time() - start_time < 2:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    response.append(line)
        
        results[cmd] = response
        print(f"  Response: {len(response)} lines")
        time.sleep(0.5)
    
    ser.close()
    return results

# Usage
if __name__ == "__main__":
    results = test_serial_commands()
    
    # Verify results
    for cmd, response in results.items():
        if response:
            print(f"✅ {cmd}: OK")
        else:
            print(f"❌ {cmd}: NO RESPONSE")
```

### Bash Test Script Example

```bash
#!/bin/bash
# test_commands.sh - Test multiple commands

PORT="${1:-/dev/ttyUSB0}"
COMMANDS=("HELP" "STATUS" "VERSION")

echo "Testing serial commands on $PORT"

for cmd in "${COMMANDS[@]}"; do
    echo ""
    echo "Testing: $cmd"
    echo "─────────────────"
    
    # Send command and capture response
    ./scripts/test_serial_command.sh --command "$cmd" --timeout 3
    
    sleep 1
done

echo ""
echo "Test complete"
```

## Best Practices for Serial Command Interfaces

### 1. Always Echo Commands

Help with debugging by echoing received commands:

```cpp
void processCommand(String cmd) {
  Serial.printf("[CMD] Received: %s\n", cmd.c_str());
  
  if (cmd == "HELP") {
    showHelp();
  } else if (cmd == "STATUS") {
    showStatus();
  } else {
    Serial.println("[ERROR] Unknown command");
  }
}
```

### 2. Use Distinct Message Prefixes

Make parsing and debugging easier:

```cpp
Serial.println("[INFO] Device initialized");
Serial.println("[ERROR] SD card not found");
Serial.println("[DEBUG] Free heap: 180000");
Serial.println("[CMD-RESPONSE] Status: OK");
```

### 3. Provide Comprehensive Help

```cpp
void showHelp() {
  Serial.println("Available commands:");
  Serial.println("  HELP    - Show this message");
  Serial.println("  STATUS  - Show device status");
  Serial.println("  RESET   - Reset device");
  Serial.println("  SCAN    - Trigger manual scan");
}
```

### 4. Include Version/Build Info

```cpp
void setup() {
  Serial.begin(115200);
  Serial.printf("\n\n%s v%s\n", PROJECT_NAME, VERSION);
  Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("Chip: ESP32 Rev %d\n", ESP.getChipRevision());
}
```

### 5. Process Serial Commands Frequently

```cpp
void loop() {
  // Call at least 3x per loop iteration
  processSerialCommands();
  
  if (shouldDoWork()) {
    doWork();
    processSerialCommands();  // Between operations
  }
  
  updateDisplay();
  processSerialCommands();  // Before delays
  
  delay(10);
}
```

### 6. Handle Line Endings Properly

```cpp
void processSerialCommands() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();  // Remove whitespace and \r
    
    if (cmd.length() > 0) {
      processCommand(cmd);
    }
  }
}
```

## Troubleshooting Checklist

When serial commands don't work, check in order:

- [ ] **Physical connection:** USB cable connected?
- [ ] **Port exists:** `/dev/ttyUSB0` present?
- [ ] **Permissions:** Can read/write port?
- [ ] **Baudrate:** Matches code (usually 115200)?
- [ ] **Serial initialized:** `Serial.begin()` called in setup()?
- [ ] **Boot messages:** ESP32 actually booting?
- [ ] **Command format:** Includes newline character?
- [ ] **Processing frequency:** Serial checked often enough?
- [ ] **Not blocked:** No long delays preventing checks?
- [ ] **Echo working:** Commands being received at all?

Run scripts in order:
1. `test_serial_connection.sh` - Basic connection
2. `capture_serial.sh` - Boot messages
3. `test_serial_command.sh` - Command response
4. `monitor_serial.sh` - Real-time observation

## Summary

Serial communication testing requires systematic verification:

1. **Connection works** (test_serial_connection.sh)
2. **Boot completes** (capture_serial.sh)
3. **Commands processed** (test_serial_command.sh)
4. **Responses correct** (interactive testing)

Always start with Phase 1 (basic connection) before debugging command processing. Most issues are either baudrate mismatches or infrequent serial checking in code.

## Next Steps

- See **getting_started.md** for Arduino CLI basics
- See **debugging_troubleshooting.md** for crash analysis
- See **communication_interfaces.md** for UART configuration
