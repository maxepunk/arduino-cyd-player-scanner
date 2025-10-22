---
name: esp32-arduino
description: Comprehensive ESP32 embedded development using Arduino CLI on Linux/Debian. Use this skill when developing embedded systems projects with ESP32 microcontrollers, compiling sketches, uploading firmware, organizing code into modular structures, refactoring large projects, debugging hardware issues, or working with GPIO, SPI, I2C, UART, displays, sensors, or other peripherals on ESP32 boards. Includes complete multi-file project organization guidance.
---

# ESP32 Arduino CLI Development Skill

This skill provides complete workflows for ESP32 embedded development using Arduino CLI in a Linux environment, from initial setup through debugging and deployment.

## When to Use This Skill

Use this skill for:
- Setting up ESP32 development environment
- Compiling and uploading ESP32 sketches
- **Organizing and refactoring ESP32 projects for better maintainability**
- **Splitting large single-file sketches into modular structure**
- Configuring GPIO, SPI, I2C, UART, and other interfaces
- Debugging hardware issues (boot failures, crashes, memory problems)
- Working with specific ESP32 boards (CYD, DevKit, WROVER, etc.)
- Integrating peripherals (displays, sensors, RFID readers, etc.)
- Troubleshooting compilation, upload, and runtime errors
- **Creating reusable modules for ESP32 applications**

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

Organize code based on project complexity. See `references/code_organization.md` for comprehensive multi-file organization guide.

### Quick Decision Guide

**Single file (< 200 lines):** Simple projects, prototypes
```
MyProject/
└── MyProject.ino
```

**Config split (200-500 lines):** First refactoring step
```
MyProject/
├── MyProject.ino
└── config.h
```

**Modular (500-2000 lines):** Production projects, multiple subsystems
```
MyProject/
├── MyProject.ino          # Main coordination (50-100 lines)
├── config.h               # Configuration
├── display_mgr.h/.cpp     # Display handling
├── wifi_mgr.h/.cpp        # Network operations
└── sensor.h/.cpp          # Sensor reading
```

**Advanced (> 2000 lines):** Complex applications, teams
```
MyProject/
├── MyProject.ino
└── src/
    ├── config/
    ├── hal/               # Hardware abstraction
    ├── drivers/           # Device drivers
    └── managers/          # High-level controllers
```

### Starting a New Project

Use the provided templates based on complexity:

```bash
# Simple single file
cp assets/sketch_template.ino MyProject/MyProject.ino

# With configuration
cp assets/sketch_template.ino MyProject/MyProject.ino
cp assets/config_template.h MyProject/config.h

# Modular project (recommended for > 200 lines)
cp -r assets/modular_template/ MyProject/
cd MyProject/
mv modular_template.ino MyProject.ino
# Edit and add your own modules
```

### Working Examples

See `references/examples/` for complete working projects:
- **simple_modular/** - 3-module example (~365 lines) demonstrating LED, button, sensor
- **cyd_display_project/** - CYD-specific organization with display, touch, WiFi

These examples show proper file organization, module patterns, and non-blocking code.

## Essential Reference Documentation

Read the appropriate reference file(s) based on your needs:

### Code Organization (NEW - Essential for Projects > 200 Lines)
**File:** `references/code_organization.md`
**Read when:** Project outgrowing single file, refactoring needed, or want to split into modules

**Covers:**
- When to refactor (decision criteria by project size)
- Arduino CLI compilation model for multi-file projects
- Proper header/implementation file structure
- Step-by-step refactoring workflow (6 phases)
- Common module patterns (managers, callbacks, state machines, HAL)
- Avoiding pitfalls (circular dependencies, include issues)
- CYD-specific organization challenges
- Compilation troubleshooting after refactoring

**Critical for:**
- Projects > 200 lines that need better organization
- Converting single-file sketches to modular structure
- Creating reusable modules
- Team collaboration on ESP32 projects

**Working examples:** See `references/examples/simple_modular/` and study the patterns

### Serial Communication (Critical for Development)
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

## Refactoring Large Projects

When projects outgrow single files, proper refactoring maintains code quality and enables growth.

### When to Refactor

**Refactor when you experience:**
- Difficulty finding specific code sections (excessive scrolling)
- Fear of breaking unrelated code when making changes
- Multiple people need to work on the same file
- Want to reuse components in other projects
- Need to test components independently
- File exceeds 200-300 lines

**Typical triggers:**
- Single file > 200 lines → Extract config.h
- Single file > 500 lines → Split into modules
- Multiple related projects → Create reusable modules
- Team collaboration → Modular organization required

### Quick Refactoring Overview

**Read first:** `references/code_organization.md` provides complete step-by-step workflow

**Summary of process:**
1. **Extract configuration** (30 min) - Move constants to config.h
2. **Identify modules** (1 hour) - Map functional boundaries
3. **Create headers** (2 hours) - Define public interfaces
4. **Implement modules** (3-5 hours) - Move code to .cpp files
5. **Update main** (1 hour) - Simplify to coordination only
6. **Test thoroughly** (2 hours) - Verify each module works

**Total time:** 8-11 hours for typical 800-line project

### Example: CYD Project Refactoring

**Before:** Single 800-line file
- Mixed display, touch, WiFi, SD card code
- Hard to modify one subsystem without risk to others
- Difficult to test display independently
- Tight coupling everywhere

**After:** Modular organization
```
CYDProject/
├── CYDProject.ino        # 50 lines: setup/loop coordination
├── config.h              # Pin definitions, constants
├── cyd_pins.h            # CYD-specific pin mapping
├── display_mgr.h/cpp     # All TFT operations
├── touch_mgr.h/cpp       # Touch handling
├── wifi_mgr.h/cpp        # Network operations
└── sd_mgr.h/cpp          # SD card operations
```

**Benefits:**
- Display changes don't affect WiFi code
- Can test each module independently
- Multiple developers can work simultaneously
- Modules reusable in other CYD projects
- Main file shows high-level logic clearly

### Key Refactoring Patterns

**Pattern 1: Manager Classes**
```cpp
// display_manager.h
class DisplayManager {
public:
    bool begin();
    void update();
    void showText(const char* text);
private:
    TFT_eSPI _tft;
    bool _initialized;
};
extern DisplayManager Display;
```

**Pattern 2: Config First**
Always start by extracting config.h before splitting code:
```cpp
// config.h
#define LED_PIN 2
#define SENSOR_PIN 34
#define BAUD_RATE 115200
```

**Pattern 3: Global Instances**
Make modules easily accessible:
```cpp
// In .h: extern DisplayManager Display;
// In .cpp: DisplayManager Display;
// Usage: Display.showText("Hello");
```

### Arduino CLI Compilation Notes

**Important:** Arduino CLI automatically compiles:
- All .cpp files in sketch directory
- All .cpp files in subdirectories
- Main .ino file (converted to .cpp internally)

**You must:**
- Use include guards in all .h files (`#ifndef`/`#define`/`#endif`)
- Include module header first in each .cpp
- Put declarations in .h, implementations in .cpp
- Keep .cpp files in sketch directory

**Common compilation errors after refactoring:**
- "multiple definition" → Implementation in .h file
- "undefined reference" → Missing .cpp or wrong signature
- "not declared" → Missing #include or forward declaration

See `references/code_organization.md` for detailed troubleshooting.

### Working Examples

Study these complete examples in `references/examples/`:

**simple_modular/** - Basic 3-module project
- LED controller, button handler, sensor reader
- ~365 lines across 7 files
- Shows fundamental patterns
- Good starting point for learning

**cyd_display_project/** - Complex CYD organization
- Display, touch, WiFi, SD card modules
- SPI bus coordination
- CYD-specific pin management
- Production-ready structure

### Common Refactoring Pitfalls

❌ **Don't** put everything in headers
✅ **Do** use .h for declarations, .cpp for implementations

❌ **Don't** create circular includes (A includes B, B includes A)
✅ **Do** use forward declarations to break circular dependencies

❌ **Don't** refactor everything at once
✅ **Do** refactor one module at a time, testing each

❌ **Don't** forget include guards
✅ **Do** add include guards to every .h file

❌ **Don't** access hardware directly from main
✅ **Do** access through clean module interfaces

### Testing After Refactoring

**Checklist:**
- [ ] Compiles without errors or warnings
- [ ] All original functionality works
- [ ] No memory leaks (check `ESP.getFreeHeap()`)
- [ ] Each module can be tested independently
- [ ] Code is more readable and maintainable
- [ ] Modules follow consistent patterns

**Test each module independently:**
```cpp
// test/test_display.ino
#include "display_manager.h"

void setup() {
    Serial.begin(115200);
    if (Display.begin()) {
        Display.showText("Test OK");
    }
}

void loop() {}
```

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
1. **Use config.h for all constants and pin definitions**
   - Centralize all #define statements
   - Group by category (pins, timing, features)
   - Makes board adaptation easy

2. **Separate concerns into modular .h/.cpp files**
   - One module = one responsibility
   - Public interface in .h (declarations)
   - Implementation in .cpp (definitions)
   - See `references/code_organization.md` for patterns
   - Study `references/examples/simple_modular/` for working example

3. **Use `#ifdef DEBUG` for removable debug code**
   - Define in config.h: `#define DEBUG_MODE 1`
   - Wrap debug output: `#ifdef DEBUG_MODE Serial.println(...); #endif`
   - Or use macros: `DEBUG_PRINTLN(x)`
   - Eliminates performance overhead in production

4. **Add include guards to all headers**
   - Use `#ifndef MODULE_H` / `#define MODULE_H` / `#endif`
   - Or use `#pragma once` (simpler, widely supported)
   - Prevents multiple inclusion errors

5. **Keep ISRs short and in IRAM**
   - Use `IRAM_ATTR` attribute for ISR functions
   - Set flags in ISR, process in loop()
   - Avoids crashes during flash operations
   - Example: `void IRAM_ATTR onTimer() { flag = true; }`

6. **Follow consistent naming conventions**
   - Classes: PascalCase (DisplayManager)
   - Functions/methods: camelCase (begin, getValue)
   - Private members: _leadingUnderscore (_initialized)
   - Constants: UPPER_CASE (LED_PIN, BAUD_RATE)
   - Global instances: Capital (Display, WiFi)

**For detailed organization patterns, architecture guidance, and refactoring workflows:**
- Read `references/code_organization.md` (comprehensive guide)
- Study working examples in `references/examples/`
- Use `assets/modular_template/` as starting point

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
