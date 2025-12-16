# ESP32 Skill Update Instructions

This package updates your `esp32-arduino` skill from Arduino CLI to PlatformIO with native testing support.

## What Changed

### Removed (Arduino CLI specific)
- `scripts/setup_arduino_cli.sh`
- `scripts/verify_setup.sh`
- `scripts/compile_esp32.sh`
- `scripts/upload_esp32.sh`
- `scripts/monitor_serial.sh`

### Kept (Platform-agnostic serial debugging)
- `scripts/capture_serial.sh`
- `scripts/test_serial_command.sh`
- `scripts/test_serial_connection.sh`

### Added (Testing infrastructure)
- `assets/ArduinoCompat.h` - Arduino API stubs for native builds
- `assets/test_template.cpp` - Unity test starter file
- `assets/platformio.ini.template` - Dual-environment config template
- `references/mocking-patterns.md` - Interface patterns for testing
- `references/native-arduino-compat.md` - Documentation for compatibility layer

### Updated
- `SKILL.md` - Complete rewrite for PlatformIO + native testing

## Installation Steps

### Option A: Manual Copy (Recommended)

1. **Navigate to your skill directory:**
   ```bash
   cd /path/to/arduino-cyd-player-scanner/.claude/skills/esp32-arduino
   ```

2. **Backup current skill (optional):**
   ```bash
   cp -r . ../esp32-arduino-backup
   ```

3. **Delete obsolete scripts:**
   ```bash
   rm scripts/setup_arduino_cli.sh
   rm scripts/verify_setup.sh
   rm scripts/compile_esp32.sh
   rm scripts/upload_esp32.sh
   rm scripts/monitor_serial.sh
   ```

4. **Copy new files from this package:**
   ```bash
   # Replace SKILL.md
   cp /path/to/update/SKILL.md .
   
   # Add new reference files
   cp /path/to/update/references/*.md references/
   
   # Add new asset files
   cp /path/to/update/assets/* assets/
   ```

### Option B: Full Replacement

If you want to start fresh (but keep your serial debugging scripts):

1. **Backup serial scripts:**
   ```bash
   mkdir ~/skill-backup
   cp scripts/capture_serial.sh ~/skill-backup/
   cp scripts/test_serial_command.sh ~/skill-backup/
   cp scripts/test_serial_connection.sh ~/skill-backup/
   ```

2. **Replace skill contents with this package**

3. **Restore serial scripts:**
   ```bash
   cp ~/skill-backup/*.sh scripts/
   ```

## Verification

After installation, your skill directory should look like:

```
esp32-arduino/
├── SKILL.md                    # Updated
├── assets/
│   ├── ArduinoCompat.h         # NEW
│   ├── config_template.h       # Existing
│   ├── modular_template/       # Existing
│   ├── platformio.ini.template # NEW
│   ├── sketch_template.ino     # Existing
│   └── test_template.cpp       # NEW
├── references/
│   ├── code_organization.md    # Existing
│   ├── communication_interfaces.md  # Existing
│   ├── debugging_troubleshooting.md # Existing
│   ├── examples/               # Existing
│   ├── getting_started.md      # Existing (may be outdated now)
│   ├── hardware_constraints.md # Existing
│   ├── hardware_specs/         # Existing
│   ├── mocking-patterns.md     # NEW
│   ├── native-arduino-compat.md # NEW
│   ├── serial_communication.md # Existing
│   └── serial_testing.md       # Existing
└── scripts/
    ├── capture_serial.sh       # Keep
    ├── test_serial_command.sh  # Keep
    └── test_serial_connection.sh # Keep
```

## PlatformIO Setup on Raspberry Pi

After updating the skill, install PlatformIO:

```bash
# On Raspberry Pi Bookworm
sudo apt install pipx
pipx install platformio
pipx ensurepath
source ~/.bashrc

# Serial access
sudo usermod -a -G dialout $USER
# Logout and login
```

## Quick Test

```bash
cd your-project
pio test -e native    # Run native tests (no hardware)
pio run               # Build for ESP32
pio run -t upload     # Upload to board
pio device monitor    # Serial monitor
```
