# Getting Started with ESP32 and Arduino CLI

This guide covers the essential workflow for ESP32 development using Arduino CLI on Linux systems.

## Arduino CLI Basics

### Five Essential Setup Commands

1. **Initialize configuration**: `arduino-cli config init`
2. **Add ESP32 board manager URL**: `arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Update package index**: `arduino-cli core update-index`
4. **Install ESP32 core**: `arduino-cli core install esp32:esp32`
5. **Verify installation**: `arduino-cli board listall esp32`

### Understanding FQBN (Fully Qualified Board Name)

Every ESP32 board requires an FQBN for compilation. Common examples:

- Generic ESP32: `esp32:esp32:esp32`
- ESP32-WROVER: `esp32:esp32:esp32wrover`
- ESP32-S2: `esp32:esp32:esp32s2`
- ESP32-S3: `esp32:esp32:esp32s3`
- ESP32-C3: `esp32:esp32:esp32c3`

### FQBN Configuration Options

Configuration options append to the FQBN with colon separators:

```bash
esp32:esp32:esp32:CPUFreq=240,FlashMode=qio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600
```

**Common options:**
- `CPUFreq`: 240, 160, 80, 40 (MHz)
- `FlashMode`: qio, dio, qout, dout
- `FlashFreq`: 80, 40 (MHz)
- `FlashSize`: 4M, 8M, 16M
- `UploadSpeed`: 921600, 460800, 256000, 115200 (baud)
- `PartitionScheme`: default, huge_app, min_spiffs, no_ota
- `DebugLevel`: none, error, warn, info, debug, verbose

## Compilation Workflow

### Basic Compilation

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 MySketch/
```

### Compilation with Options

```bash
# Verbose output (helpful for debugging)
arduino-cli compile --fqbn esp32:esp32:esp32 --verbose MySketch/

# Export compiled binaries
arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries MySketch/

# Custom build properties
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-property compiler.cpp.extra_flags=-DDEBUG_MODE \
  MySketch/

# Optimized for performance
arduino-cli compile --fqbn esp32:esp32:esp32:CPUFreq=240,FlashMode=qio \
  MySketch/

# Large application (3MB partition)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  MySketch/
```

## Upload Workflow

### Basic Upload

```bash
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 MySketch/
```

### Common Ports

- **Linux**: `/dev/ttyUSB0`, `/dev/ttyUSB1`, `/dev/ttyACM0`
- **macOS**: `/dev/cu.usbserial-XXXX`, `/dev/cu.SLAB_USBtoUART`
- **Windows**: `COM3`, `COM4`, etc.

### Manual Boot Mode Entry

If uploads fail with timeout errors, manually enter boot mode:

1. Hold BOOT button on ESP32
2. Press and release EN/RESET button
3. Release BOOT button after "Connecting..." appears

### Troubleshooting Upload Issues

**Timeout errors:**
- Try slower upload speed: Add `:UploadSpeed=115200` to FQBN
- Ensure correct port specified
- Check cable quality (data cable, not charge-only)

**Permission denied:**
```bash
# Temporary fix
sudo chmod 666 /dev/ttyUSB0

# Permanent fix
sudo usermod -a -G dialout $USER
# Then log out and log back in
```

## Serial Monitoring

### Arduino CLI Monitor

```bash
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

**Common baudrates:** 9600, 115200, 230400, 460800, 921600

**Important:** The monitor blocks the serial port exclusively. Close it before uploading.

### Alternative Tools

```bash
# screen (Ctrl+A then K to exit)
screen /dev/ttyUSB0 115200

# minicom
minicom -D /dev/ttyUSB0 -b 115200

# picocom (Ctrl+A then Ctrl+X to exit)
picocom -b 115200 /dev/ttyUSB0
```

## Library Management

### Installing Libraries

```bash
# Install by exact name
arduino-cli lib install "Adafruit GFX Library"

# Install specific version
arduino-cli lib install "ArduinoJson@6.21.0"

# Install from GitHub (for development versions)
arduino-cli lib install --git-url https://github.com/username/repo.git
```

### Essential ESP32 Libraries

These must be installed separately (not included in ESP32 core):

```bash
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "TFT_eSPI"
arduino-cli lib install "PubSubClient"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "ESP32Servo"
arduino-cli lib install "AsyncTCP"
arduino-cli lib install "ESPAsyncWebServer"
```

### Library Conflicts - Critical!

**Always use ESP32 versions** of these libraries (bundled with the core):
- WiFi.h (not Arduino WiFi)
- SPI.h (ESP32 version)
- SD.h (ESP32 version)
- Wire.h (ESP32 I2C)

The Arduino standard versions will compile but cause runtime failures.

### Checking Library Locations

```bash
arduino-cli lib list --verbose
```

Verify ESP32 libraries come from `~/.arduino15/packages/esp32/` not Arduino standard library paths.

## Sketch Structure Best Practices

### Minimal ESP32 Sketch

```cpp
void setup() {
  Serial.begin(115200);
  // Initialization code
}

void loop() {
  // Main code
}
```

### Modular Project Structure

```
MyProject/
├── MyProject.ino          # Main sketch
├── config.h               # Configuration constants
├── sensor_handler.h/.cpp  # Sensor module
├── wifi_manager.h/.cpp    # Network module
└── utils.h/.cpp           # Utility functions
```

### Include Guards (Header Files)

```cpp
#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

// Header content

#endif // SENSOR_HANDLER_H
```

## Quick Reference Commands

```bash
# List all ESP32 boards
arduino-cli board listall esp32

# Search for libraries
arduino-cli lib search sensor

# List installed libraries
arduino-cli lib list

# Get board details
arduino-cli board details -b esp32:esp32:esp32

# List connected boards
arduino-cli board list

# Create new sketch
arduino-cli sketch new MyNewProject
```

## Next Steps

- See **hardware_constraints.md** for GPIO limitations and pin assignments
- See **communication_interfaces.md** for SPI, I2C, UART configuration
- See **debugging_troubleshooting.md** for common issues and solutions
