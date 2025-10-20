# Debugging and Troubleshooting ESP32 Projects

This guide covers common ESP32 issues, systematic debugging approaches, and solutions to frequent problems.

## Common Compilation Errors

### Missing Library Error

**Symptom:**
```
fatal error: SomeLibrary.h: No such file or directory
```

**Solution:**
```bash
# Search for the library
arduino-cli lib search SomeLibrary

# Install it
arduino-cli lib install "SomeLibrary"

# Verify installation
arduino-cli lib list | grep SomeLibrary
```

### ESP32 vs Arduino Library Conflicts

**Symptom:**
```
Multiple libraries found for WiFi.h
Using: /path/to/arduino/libraries/WiFi
Not used: /path/to/esp32/libraries/WiFi
```

**Problem:** Arduino standard libraries conflict with ESP32-specific versions

**Critical libraries that must use ESP32 versions:**
- WiFi.h
- SPI.h
- SD.h
- Wire.h (I2C)

**Solution:**
```bash
# Check library locations
arduino-cli lib list --verbose | grep WiFi

# Uninstall Arduino version if present
arduino-cli lib uninstall WiFi

# The ESP32 version comes with the core and takes precedence
```

### Sketch Too Big Error

**Symptom:**
```
Sketch too big; see http://www.arduino.cc/en/Guide/Troubleshooting#size for tips on reducing it.
```

**Solution:** Use a partition scheme with more app space

```bash
# Option 1: Huge app (3MB, no OTA)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app sketch/

# Option 2: Minimal SPIFFS (1.9MB app)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs sketch/

# Option 3: No OTA (2MB app)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota sketch/
```

### Python Not Found Error

**Symptom:**
```
"python": executable file not found in $PATH
```

**Solution (Debian/Ubuntu):**
```bash
# Install Python 3
sudo apt-get update
sudo apt-get install python3 python-is-python3

# Verify
which python
python --version
```

## Upload Issues

### Timeout Waiting for Packet Header

**Symptom:**
```
A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header
```

**Causes:**
1. ESP32 not in bootloader mode
2. Wrong serial port
3. USB cable is charge-only (no data lines)
4. Driver issues

**Solutions:**

**Manual boot mode entry:**
1. Hold BOOT button on ESP32
2. Press and release EN/RESET button  
3. Release BOOT after "Connecting..." appears in terminal

**Try slower upload speed:**
```bash
arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 \
  --port /dev/ttyUSB0 sketch/
```

**Check cable:** Use a known-good USB data cable

**Auto-reset circuit:** Some boards need hardware modification:
```
DTR/RTS signals from USB → Capacitor → EN pin
```

### Permission Denied on Linux

**Symptom:**
```
cannot open /dev/ttyUSB0: Permission denied
```

**Temporary solution:**
```bash
sudo chmod 666 /dev/ttyUSB0
```

**Permanent solution:**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Verify group membership
groups

# Log out and log back in for changes to take effect
```

### Board Not Detected

**Check connected devices:**
```bash
# List all serial devices
ls -la /dev/tty*

# Monitor USB device connections
dmesg | grep tty

# Check for ESP32 specifically
lsusb | grep -i "CP210\|CH340\|FTDI"
```

**Common USB-to-Serial chips:**
- CP210x (Silicon Labs) - Most common
- CH340/CH341 (Chinese chips)
- FTDI FT232

**Driver installation (if needed):**
```bash
# Linux (usually automatic)
# Drivers included in kernel for most chips

# macOS - may need manual driver
# Download from: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

# Windows - usually automatic via Windows Update
# Manual install: silabs.com or manufacturer website
```

### ModemManager Interference (Linux)

**Symptom:** Upload starts but fails, or port disappears during upload

**Solution:** Disable ModemManager
```bash
sudo systemctl stop ModemManager
sudo systemctl disable ModemManager
```

## Runtime Crashes and Resets

### Decoding Stack Traces

**Symptom:**
```
Guru Meditation Error: Core  0 panic'ed (LoadProhibited)
PC: 0x400d1234
...
Stack trace...
```

**Solution:** Use ESP Exception Decoder

1. Install decoder: https://github.com/me-no-dev/EspExceptionDecoder
2. Copy complete stack trace
3. Paste into decoder with your compiled .elf file
4. Identifies exact function and line number

### Common Reset Reasons

Check reset reason in Serial output:
```cpp
void setup() {
  Serial.begin(115200);
  
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("Reset reason: ");
  Serial.println(reason);
  
  // Reasons:
  // ESP_RST_UNKNOWN    = 0,
  // ESP_RST_POWERON    = 1,  // Power on
  // ESP_RST_EXT        = 2,  // External pin
  // ESP_RST_SW         = 3,  // Software reset
  // ESP_RST_PANIC      = 4,  // Exception/panic
  // ESP_RST_INT_WDT    = 5,  // Interrupt watchdog
  // ESP_RST_TASK_WDT   = 6,  // Task watchdog
  // ESP_RST_WDT        = 7,  // Other watchdog
  // ESP_RST_DEEPSLEEP  = 8,  // Deep sleep reset
  // ESP_RST_BROWNOUT   = 9,  // Brownout
  // ESP_RST_SDIO       = 10  // SDIO
}
```

### Brownout Detector Reset

**Symptom:**
```
Brownout detector was triggered
```

**Causes:**
- Insufficient power supply
- Voltage drops during WiFi transmission (high current draw)
- Poor quality USB cable
- Underpowered USB port

**Solutions:**
1. Use external 5V power supply (500mA+ minimum, 1A recommended)
2. Add bulk capacitor (100-1000µF) near ESP32 power pins
3. Use shorter, thicker USB cables
4. Disable brownout detector (not recommended):
   ```cpp
   #include "soc/soc.h"
   #include "soc/rtc_cntl_reg.h"
   
   void setup() {
     WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout
   }
   ```

### Watchdog Timer Resets

**Task Watchdog Symptom:**
```
Task watchdog got triggered. The following tasks did not reset the watchdog in time:
- IDLE (CPU 0)
```

**Cause:** Code not yielding control to RTOS

**Solution:** Add delays or yield calls
```cpp
void loop() {
  // Bad: blocking loop
  while (someCondition) {
    // intensive work
  }
  
  // Good: add delay or yield
  while (someCondition) {
    // intensive work
    delay(1);  // or vTaskDelay(1)
  }
}
```

**Disable watchdog (use cautiously):**
```cpp
void setup() {
  disableCore0WDT();
  disableCore1WDT();
}
```

### Interrupt Watchdog Reset

**Symptom:**
```
Interrupt wdt timeout on CPU0
```

**Cause:** ISR took too long or accessed flash during execution

**Solution:** Keep ISRs short and in IRAM
```cpp
void IRAM_ATTR myISR() {
  // Keep this SHORT
  // No Serial.print(), no delays
  // Just set flags or read/write variables
  volatile bool flag = true;
}
```

## Memory Issues

### Heap Memory Monitoring

```cpp
void printMemoryInfo() {
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
}

void loop() {
  printMemoryInfo();
  delay(5000);
}
```

### Memory Leak Detection

**Symptom:** Free heap gradually decreases over time

**Solution:**
```cpp
// Enable heap tracing
#include "esp_heap_trace.h"

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS];

void setup() {
  heap_trace_init_standalone(trace_record, NUM_RECORDS);
  heap_trace_start(HEAP_TRACE_LEAKS);
}

void loop() {
  // Your code
  
  // Periodically dump leaks
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 60000) {
    heap_trace_dump();
    lastCheck = millis();
  }
}
```

### Stack Overflow

**Symptom:** Random crashes, corrupted variables

**Causes:**
- Large local variables (arrays, structs)
- Deep recursion
- Insufficient task stack size

**Solutions:**
```cpp
// Bad: large array on stack
void badFunction() {
  byte buffer[8192];  // Too big!
}

// Good: dynamic allocation or global
byte* buffer;
void goodFunction() {
  buffer = (byte*)malloc(8192);
  // Use buffer
  free(buffer);
}

// Or use global/static
static byte buffer[8192];

// Increase task stack size (for FreeRTOS tasks)
xTaskCreate(
  myTask,
  "MyTask",
  8192,  // Stack size in bytes (default often 2048-4096)
  NULL,
  1,
  NULL
);
```

## WiFi Issues

### WiFi Connection Failures

**Diagnostic code:**
```cpp
#include <WiFi.h>

void connectWiFi() {
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nConnection failed!");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
    // Status codes:
    // WL_IDLE_STATUS     = 0
    // WL_NO_SSID_AVAIL   = 1  (SSID not found)
    // WL_CONNECTED       = 3
    // WL_CONNECT_FAILED  = 4
    // WL_CONNECTION_LOST = 5
    // WL_DISCONNECTED    = 6
  }
}
```

### WiFi Stability Issues

**Solutions:**
```cpp
// Disable power save mode
WiFi.setSleep(false);

// Set hostname (helps some routers)
WiFi.setHostname("esp32-device");

// Increase WiFi TX power
WiFi.setTxPower(WIFI_POWER_19_5dBm);

// Use static IP (faster, more reliable)
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
WiFi.config(local_IP, gateway, subnet);
```

## Serial Monitor Issues

### Garbled Output

**Causes:**
- Wrong baud rate
- Serial port conflict

**Solutions:**
```cpp
// Ensure Serial.begin() matches monitor baudrate
Serial.begin(115200);  // Most common for ESP32

// Add delay at start for monitor to connect
delay(1000);
Serial.println("Starting...");
```

### No Output

**Checks:**
```cpp
void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial to initialize
  
  Serial.println("If you see this, serial works!");
  
  // Check if Serial is ready
  while (!Serial) {
    delay(10);
  }
}
```

## Display Issues (TFT, OLED)

### Blank Screen

**Checks:**
1. Power supply adequate?
2. Correct driver selected? (ILI9341, ST7789, etc.)
3. SPI pins correct?
4. Initialization code complete?

**Diagnostic:**
```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  
  Serial.println("Initializing display...");
  tft.init();
  Serial.println("Display initialized");
  
  tft.fillScreen(TFT_RED);
  Serial.println("Screen should be red");
  delay(1000);
  
  tft.fillScreen(TFT_GREEN);
  Serial.println("Screen should be green");
  delay(1000);
  
  tft.fillScreen(TFT_BLUE);
  Serial.println("Screen should be blue");
}
```

### Inverted Colors

**Solution (TFT_eSPI):**
Edit `User_Setup.h`:
```cpp
#define TFT_RGB_ORDER TFT_BGR  // or TFT_RGB
#define TFT_INVERSION_ON       // or TFT_INVERSION_OFF
```

## Systematic Debugging Workflow

### 1. Reproduce the Issue
- Document exact steps to trigger
- Note conditions (WiFi on/off, specific inputs, timing)

### 2. Isolate the Problem
```cpp
// Binary search debugging - disable code sections
void loop() {
  // Disable sections one at a time
  // section1();  // Disabled
  section2();
  section3();
  // section4();  // Disabled
}
```

### 3. Add Diagnostic Logging
```cpp
#define DEBUG 1

#if DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

void someFunction() {
  DEBUG_PRINTLN("Entering someFunction");
  // code
  DEBUG_PRINT("Variable value: ");
  DEBUG_PRINTLN(myVariable);
}
```

### 4. Check Assumptions
- Verify pin numbers match hardware
- Confirm voltage levels (3.3V, not 5V)
- Check power supply adequacy
- Verify library versions

### 5. Simplify
- Create minimal sketch that reproduces issue
- Remove unnecessary libraries
- Test with known-good hardware

## Quick Troubleshooting Checklist

**Won't compile:**
- [ ] Libraries installed?
- [ ] Correct board selected?
- [ ] Syntax errors?

**Won't upload:**
- [ ] Correct port selected?
- [ ] Board connected?
- [ ] Boot mode entered (if needed)?
- [ ] Permissions correct (Linux)?

**Crashes/resets:**
- [ ] Power supply adequate?
- [ ] Stack trace decoded?
- [ ] Memory leaks checked?
- [ ] Watchdog issue?

**Not working as expected:**
- [ ] Pin assignments correct?
- [ ] Pulled pin states checked (ADC2 with WiFi)?
- [ ] Timing issues (delays, millis())?
- [ ] External components tested separately?

## Next Steps

- See **hardware_constraints.md** for GPIO restrictions
- See **communication_interfaces.md** for peripheral configuration
- See **getting_started.md** for basic workflows
