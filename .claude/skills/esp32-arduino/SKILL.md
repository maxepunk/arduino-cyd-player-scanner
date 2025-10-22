---
name: esp32-arduino
description: Comprehensive ESP32 embedded development using Arduino CLI on Linux/Debian. Use this skill when developing embedded systems projects with ESP32 microcontrollers, compiling sketches, uploading firmware, debugging hardware issues, or working with GPIO, SPI, I2C, UART, displays, sensors, or other peripherals on ESP32 boards.
---

# ESP32 Arduino CLI Development Skill

This skill provides complete workflows for ESP32 embedded development using Arduino CLI in a Linux environment, from initial setup through debugging and deployment.

## When to Use This Skill

Use this skill for:
- Setting up ESP32 development environment
- Compiling and uploading ESP32 sketches
- Configuring GPIO, SPI, I2C, UART, and other interfaces
- Debugging hardware issues (boot failures, crashes, memory problems)
- Working with specific ESP32 boards (CYD, DevKit, WROVER, etc.)
- Integrating peripherals (displays, sensors, RFID readers, etc.)
- Troubleshooting compilation, upload, and runtime errors

## Serial Communication - Critical for Development

Serial communication is essential for ESP32 development. Use the dedicated reference and scripts when:
- Capturing boot sequences
- Sending commands to running devices
- Debugging serial command processing
- Testing device responses
- Diagnosing communication issues

**Reference:** `references/serial_communication.md` - Read for:
- Reliable serial communication patterns
- Decision tree for choosing the right approach
- Common pitfalls and solutions
- Background task conflicts
- Comprehensive troubleshooting

**Critical Scripts:**
- `capture_boot.sh` - Capture complete boot sequence after reset
- `send_serial_command.sh` - Send single command with response capture
- `test_serial_interactive.sh` - Interactive/batch command testing
- `diagnose_serial.sh` - Comprehensive serial diagnostics

**Most Common Issues:**
1. **No serial output** → Run: `./scripts/diagnose_serial.sh`
2. **Commands not processed** → Read: `serial_communication.md` section "Background Task Conflicts"
3. **Missing boot messages** → Use: `./scripts/capture_boot.sh` (starts monitor BEFORE reset)
4. **Port in use** → Scripts automatically clean up, or run: `pkill -f "cat /dev/ttyUSB0"`

## Quick Start Workflow

### 1. Initial Setup (First Time Only)

Run the setup script to install Arduino CLI and ESP32 toolchain:

```bash
./scripts/setup_arduino_cli.sh
```

Verify installation:
```bash
./scripts/verify_setup.sh
```

If verification fails, address errors before proceeding. Common fixes are detailed in the script output.

### 2. Basic Development Cycle

**Compile a sketch:**
```bash
./scripts/compile_esp32.sh path/to/sketch.ino
```

**Upload to board:**
```bash
./scripts/upload_esp32.sh path/to/sketch.ino
```

**Monitor serial output:**
```bash
./scripts/monitor_serial.sh
```

### 3. Advanced Compilation

Use the compile script with options for different configurations:

```bash
# High performance (240MHz CPU)
./scripts/compile_esp32.sh --board fast my_sketch.ino

# Large application (3MB partition)
./scripts/compile_esp32.sh --board huge_app my_sketch.ino

# Custom FQBN with specific options
./scripts/compile_esp32.sh \
  --fqbn esp32:esp32:esp32:CPUFreq=240,FlashMode=qio,PartitionScheme=no_ota \
  my_sketch.ino

# Add debug flags
./scripts/compile_esp32.sh \
  --property compiler.cpp.extra_flags=-DDEBUG_MODE \
  my_sketch.ino
```

### 4. Troubleshooting Uploads

If upload fails, try manual boot mode or slower speed:

```bash
# Slower upload speed (more reliable)
./scripts/upload_esp32.sh --speed 115200 my_sketch.ino

# Specify exact port
./scripts/upload_esp32.sh --port /dev/ttyUSB0 my_sketch.ino
```

See scripts output for detailed instructions on manual boot mode entry.

## Project Structure Best Practices

### Recommended Organization

```
MyESP32Project/
├── MyESP32Project.ino     # Main sketch
├── config.h                # Configuration (use template)
├── module1.h/.cpp          # Modular code
├── module2.h/.cpp
└── README.md
```

### Starting a New Project

Use the provided templates:

```bash
# Create sketch structure
mkdir MyProject && cd MyProject

# Copy template files
cp assets/sketch_template.ino MyProject.ino
cp assets/config_template.h config.h

# Edit as needed, then compile
./scripts/compile_esp32.sh MyProject.ino
```

## Essential Reference Documentation

Read the appropriate reference file(s) based on your needs:

### Serial Communication (NEW - Critical for Development)
**File:** `references/serial_communication.md`
**Read when:** Any serial communication task, debugging command processing, or communication failures

**Covers:**
- Reliable patterns for boot capture, monitoring, command sending
- Decision tree for choosing the right approach
- Common pitfalls (port exclusivity, timing, baud rate)
- Background task conflicts affecting command processing
- Comprehensive troubleshooting workflows

**Critical concepts:**
- Serial ports are exclusive (only one process at a time)
- Boot messages need monitoring BEFORE reset
- Commands need 2-3 second delays between sends
- processSerialCommands() must be called frequently in loop()

### Getting Started
**File:** `references/getting_started.md`
**Read when:** First time using ESP32 or Arduino CLI, or need quick command reference

**Covers:**
- Arduino CLI basics and setup
- FQBN configuration options
- Compilation and upload workflows
- Library management
- Serial monitoring

### Hardware Constraints
**File:** `references/hardware_constraints.md`
**Read when:** Designing circuits, assigning pins, or encountering boot failures

**Critical topics:**
- GPIO restrictions (input-only pins, strapping pins, flash pins)
- ADC2 and WiFi conflicts
- Current limits and voltage levels
- Safe pin recommendations

**CRITICAL:** GPIO 12 must be LOW at boot or ESP32 won't start. GPIO 34-39 are input-only with no pull resistors.

### Communication Interfaces
**File:** `references/communication_interfaces.md`
**Read when:** Using SPI, I2C, UART, or PWM peripherals

**Covers:**
- SPI (HSPI vs VSPI), default pins and configuration
- I2C setup, multiple buses, pull-up requirements
- UART pin assignments and remapping
- PWM/servo control

**CRITICAL:** I2C requires external 4.7kΩ pull-up resistors. Internal pulls are insufficient.

### Debugging & Troubleshooting
**File:** `references/debugging_troubleshooting.md`
**Read when:** Encountering compilation errors, upload failures, crashes, or unexpected behavior

**Covers:**
- Common compilation errors and library conflicts
- Upload issues (timeouts, permissions, boot mode)
- Runtime crashes and reset debugging
- Memory issues and leak detection
- WiFi problems and display issues

### Serial Communication Testing
**File:** `references/serial_testing.md`
**Read when:** Serial commands not working, need to test command interface, or debug serial communication

**Covers:**
- Systematic serial testing workflow (4 phases)
- Common serial issues and solutions
- Testing serial commands programmatically
- Best practices for command interfaces

**CRITICAL:** Process serial commands frequently in loop(). Long delays block command processing.

### Board-Specific Documentation
**Directory:** `references/hardware_specs/`
**Read when:** Working with a specific board (e.g., CYD, custom boards)

**Example:** `references/hardware_specs/CYD_ESP32-2432S028R.md` - Complete CYD pinout

## Common Development Scenarios

### Scenario: No Serial Output After Upload

**Symptoms:** Upload succeeds but no serial output visible

**Actions:**
1. Run diagnostic: `./scripts/diagnose_serial.sh`
2. If port accessible but no data: capture boot with `./scripts/capture_boot.sh`
3. Check if device is running: look for LED activity
4. Verify Serial.begin(115200) in code setup()

### Scenario: Commands Not Being Processed

**Symptoms:** Send commands via serial, no response from device

**Actions:**
1. Read **serial_communication.md** section "Background Task Conflicts"
2. Verify device is outputting anything: `timeout 5 cat /dev/ttyUSB0`
3. Check if processSerialCommands() or Serial.available() is called in loop()
4. Test with simple command: `./scripts/send_serial_command.sh /dev/ttyUSB0 "HELP" 5 10`
5. Ensure loop() is not blocked by delays or long operations

### Scenario: Missing Boot Messages

**Symptoms:** Device boots but no boot sequence visible

**Actions:**
1. Use capture script: `./scripts/capture_boot.sh /dev/ttyUSB0`
2. Start monitor BEFORE pressing reset button (script does this)
3. Check baud rate: `stty -F /dev/ttyUSB0 115200`
4. If still empty, check if Serial.begin() is early in setup()

### Scenario: Intermittent Serial Communication

**Symptoms:** Sometimes works, sometimes doesn't

**Actions:**
1. Check for port conflicts: `lsof /dev/ttyUSB0`
2. Kill competing processes: `pkill -f "cat /dev/ttyUSB0"`
3. Verify USB cable quality (try different cable)
4. Check for brownout resets: see boot capture for reset reasons
5. Add delays between commands (minimum 2 seconds)

### Scenario: Boot Failure or No Serial Output

**Symptoms:** ESP32 won't boot, no serial output, or continuous reset

**Actions:**
1. Read **hardware_constraints.md** section on strapping pins
2. Check GPIO 12 is not pulled HIGH (must be LOW at boot)
3. Verify power supply is adequate (>500mA)
4. Check serial monitor baudrate (usually 115200)

### Scenario: I2C Device Not Responding

**Actions:**
1. Read **communication_interfaces.md** I2C section
2. Verify external pull-up resistors (4.7kΩ) on SDA and SCL
3. Run I2C scanner code from reference
4. Check power to I2C device
5. Verify I2C address (scan output shows connected devices)

### Scenario: Analog Reading Returns 0 with WiFi Enabled

**Actions:**
1. Read **hardware_constraints.md** ADC section
2. Check if pin is on ADC2 (GPIO 0, 2, 4, 12-15, 25-27)
3. Move analog reading to ADC1 pin (GPIO 32-39)

### Scenario: Upload Keeps Timing Out

**Actions:**
1. Read **debugging_troubleshooting.md** upload issues section
2. Try manual boot mode (hold BOOT, press/release RESET)
3. Use slower upload speed: `--speed 115200`
4. Check permissions: `sudo usermod -a -G dialout $USER`
5. Try different USB cable (must be data cable, not charge-only)

### Scenario: Sketch Too Big for Partition

**Actions:**
1. Compile with larger partition: `--board huge_app`
2. Or use custom partition scheme in FQBN: `:PartitionScheme=no_ota`
3. Optimize code: remove unused libraries, move strings to PROGMEM

### Scenario: Working with CYD Board

**Actions:**
1. Read **hardware_specs/CYD_ESP32-2432S028R.md** for complete pinout
2. Note: Only 3 easily accessible GPIOs (IO22, IO27, IO35)
3. For more pins, sacrifice RGB LED or SD card
4. Display uses HSPI, external SPI devices should use VSPI

### Scenario: Serial Commands Not Working

**Symptoms:** Commands sent to ESP32 get no response, or serial output is missing

**Actions:**
1. Read **serial_testing.md** for complete serial testing workflow
2. Test basic connection: `./scripts/test_serial_connection.sh`
3. Capture boot sequence: `./scripts/capture_serial.sh --duration 10`
4. Test single command: `./scripts/test_serial_command.sh --command "HELP"`
5. Check if Serial processing happens frequently enough in code

**Common causes:**
- Serial.begin() not called or wrong baudrate
- Serial commands blocked by long delays in loop()
- Commands not being read (Serial.available() not checked)
- Wrong line endings (need \n character)

## Development Tips

### Pin Selection Strategy
1. Check board-specific documentation first (if available in hardware_specs/)
2. Consult **hardware_constraints.md** for ESP32 limitations
3. Use ADC1 pins (32-39) for analog readings if WiFi enabled
4. Use standard pins (21/22 for I2C, 18/19/23/5 for VSPI)
5. Avoid strapping pins (0, 2, 5, 12, 15) unless necessary

### Debugging Approach
1. Enable verbose compilation: `--verbose` flag
2. Add debug Serial.print() statements
3. Monitor free heap: `ESP.getFreeHeap()`
4. Use exception decoder for crash stack traces
5. Start simple, add complexity incrementally

### Code Organization
1. Use config.h for all constants and pin definitions
2. Separate concerns into modular .h/.cpp files
3. Use `#ifdef DEBUG` for removable debug code
4. Add include guards to all headers
5. Keep ISRs short and in IRAM (`IRAM_ATTR`)

## Scripts Reference

All scripts accept `--help` for detailed usage information.

### Development Setup

#### setup_arduino_cli.sh
Installs Arduino CLI, ESP32 core, and common libraries. Run once per system.

#### verify_setup.sh
Validates complete development environment. Run after setup or when encountering issues.

### Compilation and Upload

#### compile_esp32.sh
Compiles sketches with configurable FQBN options. Supports verbose output, binary export, and custom build properties.

#### upload_esp32.sh
Uploads compiled sketches to ESP32 boards. Auto-detects port or accepts explicit port specification. Configurable upload speed.

### Serial Communication

#### monitor_serial.sh
**Use when:** Need to watch real-time serial output

Basic serial monitoring with configurable baudrate. Close before uploading.

```bash
./scripts/monitor_serial.sh
./scripts/monitor_serial.sh --baudrate 9600
```

#### test_serial_connection.sh
**Use when:** Serial not working and need to diagnose basic connectivity

Comprehensive 4-phase test: port detection, permissions, configuration, data reception. First diagnostic step when serial issues occur.

```bash
./scripts/test_serial_connection.sh
./scripts/test_serial_connection.sh --duration 10
```

#### capture_serial.sh
**Use when:** Need to capture serial output to a file (boot sequences, logs)

Captures serial output to file with proper cleanup. Allows time for manual reset. Automatically analyzes results.

```bash
# Capture 10 seconds after pressing reset
./scripts/capture_serial.sh --duration 10 --wait 2

# Immediate capture (no wait)
./scripts/capture_serial.sh --duration 5 --wait 0

# Save to specific file
./scripts/capture_serial.sh --output boot_log.txt --duration 15
```

#### test_serial_command.sh
**Use when:** Testing serial command interface (single or multiple commands)

Sends commands and captures responses. Supports single command or interactive mode with proper timing and cleanup.

```bash
# Test single command
./scripts/test_serial_command.sh --command "STATUS"

# Interactive mode
./scripts/test_serial_command.sh --interactive

# Custom timeout and save to file
./scripts/test_serial_command.sh --command "HELP" --timeout 5 --output responses.log
```

### Basic Monitoring

#### monitor_serial.sh
Monitors serial output from ESP32. Configurable baudrate. Close before uploading.

```bash
./scripts/monitor_serial.sh /dev/ttyUSB0
```

## Advanced Topics

### Custom Partition Tables
For precise control over flash layout, create custom partition CSV and reference in compilation.

### Multi-Core Programming
ESP32 has dual cores. Use FreeRTOS tasks (`xTaskCreatePinnedToCore`) for core-specific execution.

### Low Power Modes
Configure WiFi sleep, CPU frequency, and deep sleep for battery-powered applications.

### OTA Updates
Over-the-air firmware updates require specific partition schemes and additional code.

For advanced topics, consult ESP32 Arduino core documentation and the references provided in this skill.

## Skill Maintenance

When adding new board-specific documentation:
1. Create file in `references/hardware_specs/[BOARD_NAME].md`
2. Include: pinout, connectors, onboard peripherals, common issues
3. Follow CYD example format for consistency

When encountering new common issues:
1. Document solution in appropriate reference file
2. Add to troubleshooting scenarios in this SKILL.md

## Getting Help

If issues persist after consulting references:
1. Check Arduino CLI issues: https://github.com/arduino/arduino-cli/issues
2. Check ESP32 Arduino core issues: https://github.com/espressif/arduino-esp32/issues
3. Verify hardware with known-good example sketches
4. Test with minimal code to isolate problems
