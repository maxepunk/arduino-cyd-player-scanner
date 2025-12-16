---
name: esp32-arduino
description: ESP32 embedded development with PlatformIO, native unit testing, and Raspberry Pi support. Use this skill for ESP32 projects using Arduino framework - compiling, uploading, organizing code, native testing with Unity/mocks, refactoring, debugging, and working with GPIO/SPI/I2C/UART peripherals. Includes Raspberry Pi Bookworm setup and ARM64 toolchain guidance.
---

# ESP32 PlatformIO Development Skill

This skill provides complete workflows for ESP32 embedded development using PlatformIO, including **native unit testing** that runs on your development machine without hardware.

## When to Use This Skill

Use this skill for:
- Setting up ESP32 development environment (PlatformIO)
- **Writing and running native unit tests** (no hardware needed)
- Compiling and uploading ESP32 sketches
- Organizing and refactoring ESP32 projects
- **Creating testable code with dependency injection**
- Configuring GPIO, SPI, I2C, UART interfaces
- Debugging hardware issues
- Working on Raspberry Pi (Bookworm) via SSH

## Quick Start: Two Workflows

### Workflow A: Test-First Development (Recommended)
```bash
# 1. Run native tests (no hardware needed)
pio test -e native

# 2. Build for ESP32
pio run -e esp32dev

# 3. Upload to hardware
pio run -e esp32dev -t upload

# 4. Monitor serial output
pio device monitor
```

### Workflow B: Direct Hardware Development
```bash
pio run           # Build
pio run -t upload # Upload  
pio device monitor # Monitor
```

## Environment Setup

### Standard Linux/Mac Setup

```bash
# Install PlatformIO CLI
pip install platformio
# Or with pipx (recommended)
pipx install platformio

# Verify installation
pio --version

# Serial port access
sudo usermod -a -G dialout $USER
# Then logout/login
```

### Raspberry Pi Bookworm Setup (Critical!)

Raspberry Pi OS Bookworm enforces PEP 668 - direct `pip install` fails. Use one of these methods:

**Option A: pipx (Recommended for CLI tools)**
```bash
sudo apt install pipx
pipx install platformio
pipx ensurepath
# Restart shell or: source ~/.bashrc
```

**Option B: Virtual Environment**
```bash
python3 -m venv ~/.pio-venv
source ~/.pio-venv/bin/activate
pip install platformio
# Add to ~/.bashrc: source ~/.pio-venv/bin/activate
```

**Option C: Break System Packages (Not Recommended)**
```bash
pip install platformio --break-system-packages
```

**Serial Port Access (All Platforms)**
```bash
sudo usermod -a -G dialout $USER
# Logout and login again (or: newgrp dialout)
```

### ARM64 Toolchain Notes (Raspberry Pi 4/5)

**Good news:** Native tests work perfectly on ARM64 - they use your system's native GCC, not the ESP32 cross-compiler.

**For ESP32 builds on ARM64:**
- PlatformIO + Espressif now provide ARM64 toolchains (since late 2022)
- Use latest platform version: `platform = espressif32 @ 6.6.0` or higher
- If toolchain errors occur, see "ARM64 Troubleshooting" section below

## Project Structure

### Recommended Layout (Testable)
```
MyProject/
├── platformio.ini          # Build configuration
├── src/
│   └── main.cpp            # Main application
├── include/
│   ├── config.h            # Configuration
│   ├── interfaces/         # Abstract interfaces for mocking
│   │   └── IRFIDReader.h
│   └── modules/
│       └── scanner.h
├── lib/
│   └── Scanner/            # Reusable library modules
│       ├── Scanner.cpp
│       └── Scanner.h
└── test/
    ├── native/             # Native tests (run on dev machine)
    │   ├── test_scanner.cpp
    │   └── mocks/
    │       └── MockRFIDReader.h
    └── embedded/           # On-device tests (optional)
        └── test_hardware.cpp
```

### platformio.ini Template

```ini
[platformio]
default_envs = esp32dev

; === ESP32 Hardware Build ===
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps = 
    ; Add your libraries here

; === Native Testing (No Hardware!) ===
[env:native]
platform = native
build_flags = 
    -std=c++11
    -DUNIT_TEST
    -DNATIVE_BUILD
test_framework = unity
lib_deps =
    ; Test-only dependencies
```

## Native Testing Workflow

### Why Native Testing?

| Traditional (Hardware) | Native Testing |
|------------------------|----------------|
| Upload every change (30-60s) | Sub-second test runs |
| Need physical device | Test on any computer |
| Hard to test edge cases | Easy mock injection |
| Flaky due to hardware | Deterministic results |

### Writing Testable Code

**Key Principle:** Depend on interfaces, not implementations.

**Before (Not Testable):**
```cpp
// Tightly coupled to hardware
class Scanner {
    MFRC522 rfid;  // Concrete dependency
public:
    void begin() { rfid.PCD_Init(); }
    bool scan() { return rfid.PICC_IsNewCardPresent(); }
};
```

**After (Testable):**
```cpp
// interfaces/IRFIDReader.h
class IRFIDReader {
public:
    virtual ~IRFIDReader() = default;
    virtual bool begin() = 0;
    virtual bool isCardPresent() = 0;
    virtual String getUID() = 0;
};

// scanner.h - depends on interface
class Scanner {
    IRFIDReader* _reader;  // Interface pointer
public:
    Scanner(IRFIDReader* reader) : _reader(reader) {}
    bool begin() { return _reader->begin(); }
    bool scan() { return _reader->isCardPresent(); }
};
```

### Creating Mocks

```cpp
// test/native/mocks/MockRFIDReader.h
class MockRFIDReader : public IRFIDReader {
public:
    bool beginCalled = false;
    bool beginReturnValue = true;
    bool cardPresent = false;
    String uidToReturn = "";
    
    bool begin() override { 
        beginCalled = true;
        return beginReturnValue; 
    }
    
    bool isCardPresent() override { return cardPresent; }
    String getUID() override { return uidToReturn; }
    
    void reset() {
        beginCalled = false;
        beginReturnValue = true;
        cardPresent = false;
        uidToReturn = "";
    }
};
```

### Writing Tests

```cpp
// test/native/test_scanner.cpp
#include <unity.h>
#include "scanner.h"
#include "mocks/MockRFIDReader.h"

MockRFIDReader mockReader;
Scanner* scanner;

void setUp() {
    mockReader.reset();
    scanner = new Scanner(&mockReader);
}

void tearDown() {
    delete scanner;
}

void test_begin_initializes_reader() {
    mockReader.beginReturnValue = true;
    
    bool result = scanner->begin();
    
    TEST_ASSERT_TRUE(mockReader.beginCalled);
    TEST_ASSERT_TRUE(result);
}

void test_scan_returns_false_when_no_card() {
    mockReader.cardPresent = false;
    
    bool result = scanner->scan();
    
    TEST_ASSERT_FALSE(result);
}

void test_scan_returns_true_when_card_present() {
    mockReader.cardPresent = true;
    
    bool result = scanner->scan();
    
    TEST_ASSERT_TRUE(result);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_initializes_reader);
    RUN_TEST(test_scan_returns_false_when_no_card);
    RUN_TEST(test_scan_returns_true_when_card_present);
    return UNITY_END();
}
```

### Arduino Compatibility for Native Builds

Native builds don't have Arduino libraries. Use the provided compatibility header:

```cpp
// include/ArduinoCompat.h - see assets/ArduinoCompat.h for full version
#ifdef NATIVE_BUILD
// Provides: String, Serial, millis(), delay(), pinMode(), etc.
#include "ArduinoCompat.h"
#endif
```

### Running Tests

```bash
# Run all native tests
pio test -e native

# Run specific test file
pio test -e native -f test_scanner

# Verbose output
pio test -e native -v

# Run with filter
pio test -e native --filter "test_scan*"
```

## PlatformIO Command Reference

### Build Commands
```bash
pio run                      # Build default environment
pio run -e esp32dev          # Build specific environment
pio run -e esp32dev -v       # Verbose build
pio run --target clean       # Clean build files
```

### Upload Commands
```bash
pio run -t upload            # Upload to connected board
pio run -t upload --upload-port /dev/ttyUSB0  # Specific port
```

### Monitor Commands
```bash
pio device monitor           # Serial monitor (default baud)
pio device monitor -b 115200 # Specific baud rate
pio device monitor -f esp32_exception_decoder  # With crash decoder
```

### Testing Commands
```bash
pio test                     # Run all tests
pio test -e native           # Native tests only
pio test -e esp32dev         # Embedded tests only
pio test -v                  # Verbose output
```

### Other Useful Commands
```bash
pio device list              # List connected devices
pio lib search <name>        # Search for libraries
pio lib install <name>       # Install library
pio system info              # System information
pio upgrade                  # Upgrade PlatformIO
```

## Serial Debugging

Serial communication remains essential for debugging. Use the platform-agnostic scripts in `scripts/`:

### Quick Diagnostics
```bash
# Test if serial is working
./scripts/test_serial_connection.sh

# Capture output (with time for reset)
./scripts/capture_serial.sh --duration 10 --wait 2

# Send command and capture response  
./scripts/test_serial_command.sh --command "STATUS"
```

### Common Serial Issues

**No output after upload:**
1. Verify `Serial.begin(115200)` in setup()
2. Check monitor baud rate matches code
3. Try: `pio device monitor -b 115200`

**Commands not processed:**
1. Ensure `Serial.available()` checked in loop()
2. Avoid long `delay()` calls blocking serial
3. See `references/serial_communication.md`

## ARM64 Troubleshooting (Raspberry Pi)

### If ESP32 Build Fails on ARM64

**Error:** `Could not find the package with '...' requirements for your system 'linux_aarch64'`

**Solution 1: Update Platform Version**
```ini
[env:esp32dev]
platform = espressif32 @ 6.6.0  ; Or latest
```

**Solution 2: Manual Toolchain Install**
```bash
# Download from Espressif releases
curl -L -o xtensa.tar.xz \
  "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20241119/xtensa-esp-elf-14.2.0_20241119-aarch64-linux-gnu.tar.xz"
  
# Extract to PlatformIO packages  
mkdir -p ~/.platformio/packages/toolchain-xtensa-esp32
tar -xf xtensa.tar.xz -C ~/.platformio/packages/toolchain-xtensa-esp32 --strip-components=1
```

**Solution 3: Use 32-bit Mode (Fallback)**
```bash
sudo dpkg --add-architecture armhf
sudo apt update
sudo apt install libc6:armhf libstdc++6:armhf
linux32 pio run
```

### Remember: Native Tests Always Work!
Native tests use your system's native GCC, not the ESP32 cross-compiler. So even if ESP32 builds fail on ARM64, you can still:
```bash
pio test -e native  # Always works on any architecture
```

## Code Organization

For projects over 200 lines, proper organization improves maintainability. See `references/code_organization.md` for comprehensive patterns including:

- When to refactor (decision criteria)
- Module patterns (managers, HAL, state machines)
- Arduino compilation model
- Step-by-step refactoring workflow

### Quick Module Pattern

```cpp
// display_manager.h
class DisplayManager {
public:
    bool begin();
    void update();
    void showText(const char* text);
private:
    bool _initialized = false;
};
extern DisplayManager Display;

// display_manager.cpp
#include "display_manager.h"
DisplayManager Display;

bool DisplayManager::begin() { /* ... */ }
void DisplayManager::update() { /* ... */ }
```

## Hardware References

### Essential Documentation

| Reference | When to Read |
|-----------|--------------|
| `references/hardware_constraints.md` | Pin selection, boot issues |
| `references/communication_interfaces.md` | SPI, I2C, UART setup |
| `references/hardware_specs/CYD_ESP32-2432S028R.md` | CYD-specific pinout |
| `references/debugging_troubleshooting.md` | Crashes, memory issues |

### Critical Hardware Notes

- **GPIO 12 must be LOW at boot** (strapping pin)
- **GPIO 34-39 are input-only** (no internal pullups)
- **ADC2 doesn't work with WiFi enabled** - use ADC1 (GPIO 32-39)
- **I2C needs external 4.7kΩ pullups** (internal too weak)

## Common Scenarios

### Scenario: Setting Up New Testable Project

```bash
mkdir MyProject && cd MyProject
pio project init --board esp32dev

# Create structure
mkdir -p include/interfaces lib test/native/mocks

# Copy templates from skill assets
cp assets/platformio.ini.template platformio.ini
cp assets/ArduinoCompat.h include/
cp assets/test_template.cpp test/native/test_main.cpp
```

### Scenario: Adding Tests to Existing Project

1. Add native environment to `platformio.ini`
2. Identify tightly-coupled code
3. Extract interfaces for hardware dependencies
4. Create mock implementations
5. Write tests using mocks
6. Run with `pio test -e native`

### Scenario: Working Over SSH on Raspberry Pi

```bash
# Use tmux for persistent sessions
tmux new -s esp32

# Split panes: one for editing, one for testing
# Ctrl+b % (vertical split)
# Ctrl+b " (horizontal split)

# Quick test cycle
pio test -e native && pio run
```

## Scripts Reference

Platform-agnostic serial debugging scripts (work with any build system):

| Script | Purpose |
|--------|---------|
| `test_serial_connection.sh` | Diagnose serial connectivity |
| `test_serial_command.sh` | Send commands, capture responses |
| `capture_serial.sh` | Capture boot sequences, logs |

All scripts accept `--help` for usage information.

## Migrating from Arduino CLI

If you have existing Arduino CLI projects:

| Arduino CLI | PlatformIO |
|-------------|------------|
| `arduino-cli compile` | `pio run` |
| `arduino-cli upload` | `pio run -t upload` |
| `arduino-cli monitor` | `pio device monitor` |
| `arduino-cli lib install` | `pio lib install` |
| `.ino` file | `src/main.cpp` (or keep .ino) |

**Key differences:**
- PlatformIO uses `platformio.ini` for all configuration
- Libraries specified in `lib_deps`, not installed globally
- Project-based isolation (no global library conflicts)
- Built-in test framework support

## Reference Documentation

### Core References
- `references/code_organization.md` - Multi-file project structure
- `references/mocking-patterns.md` - Interface patterns for testing
- `references/native-arduino-compat.md` - Arduino API stubs for native builds

### Hardware References
- `references/hardware_constraints.md` - GPIO restrictions, boot requirements
- `references/communication_interfaces.md` - SPI, I2C, UART configuration
- `references/debugging_troubleshooting.md` - Crash analysis, memory issues

### Serial Communication
- `references/serial_communication.md` - Reliable serial patterns
- `references/serial_testing.md` - Testing serial interfaces

### Board-Specific
- `references/hardware_specs/CYD_ESP32-2432S028R.md` - CYD pinout

## Getting Help

1. **PlatformIO Docs:** https://docs.platformio.org/
2. **ESP32 Arduino Core:** https://github.com/espressif/arduino-esp32
3. **Unity Test Framework:** https://github.com/ThrowTheSwitch/Unity
4. **PlatformIO Community:** https://community.platformio.org/
