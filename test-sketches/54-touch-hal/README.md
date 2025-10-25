# TouchDriver HAL Test Sketch

## Purpose
Validates the TouchDriver.h implementation with WiFi EMI filtering.

## Features Tested
1. Touch interrupt fires on GPIO36 FALLING edge
2. Pulse width measurement works correctly
3. WiFi EMI filtering rejects short pulses (<10ms)
4. Real touches accepted (pulse width >=10ms)
5. Multiple touches tracked with statistics

## Hardware Requirements
- CYD ESP32-2432S028R
- Touch controller: XPT2046 on GPIO36

## Setup Instructions
1. Edit the sketch and set your WiFi credentials:
   ```cpp
   const char* TEST_SSID = "YourSSID";
   const char* TEST_PASSWORD = "YourPassword";
   ```

2. Compile and upload:
   ```bash
   cd test-sketches/54-touch-hal
   arduino-cli compile --fqbn esp32:esp32:esp32 .
   arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .
   ```

3. Monitor serial output:
   ```bash
   arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
   ```

## Expected Results

### WiFi EMI Filtering
- WiFi EMI pulses detected but filtered (count increments)
- Pulse width < 10ms → rejected
- No false positives

### Real Touch Detection
- Physical touch detected
- Pulse width typically 70-200ms
- Touch timestamp recorded
- Statistics updated

## Serial Commands
- `STATS` - Show comprehensive touch statistics
- `RESET` - Reset counters

## Example Output

```
═══════════════════════════════════════════════════════════════
═══ TOUCH DRIVER HAL TEST - WiFi EMI Filtering Validation ════
═══════════════════════════════════════════════════════════════

✓ TouchDriver initialized successfully

[WIFI] Connecting to generate EMI...
✓ WiFi connected (EMI generation active)
  SSID: MyNetwork
  IP: 192.168.1.100
  Signal: -65 dBm

─────────────────────────────────────────────────────────────
TEST READY
─────────────────────────────────────────────────────────────
• Touch the screen to test valid touch detection
• WiFi EMI pulses will be filtered automatically
• Commands: STATS, RESET
─────────────────────────────────────────────────────────────

[TOUCH] ✗ WiFi EMI filtered (count: 1)
[TOUCH] ✗ WiFi EMI filtered (count: 2)
[TOUCH] ✗ WiFi EMI filtered (count: 3)

[TOUCH] ✓ Valid touch detected
        Pulse width: 135480 us (135.48 ms)
        Timestamp: 12456789 us
        Total valid: 1, Total interrupts: 15

[TOUCH] ✗ WiFi EMI filtered (count: 14)
[TOUCH] ✗ WiFi EMI filtered (count: 15)
```

## Compilation Results

```
Sketch uses 894307 bytes (68%) of program storage space.
Global variables use 43880 bytes (13%) of dynamic memory.
```

## Technical Notes

### ISR Implementation
The ISR uses global variables (not class static members) to avoid ESP32 linker issues with IRAM placement:

```cpp
static volatile bool g_touchInterruptOccurred = false;
static volatile uint32_t g_touchInterruptTime = 0;

void IRAM_ATTR touchISR() {
    g_touchInterruptOccurred = true;
    g_touchInterruptTime = micros();
}
```

### GPIO36 Constraints
- Input-only pin (no OUTPUT mode, no pull-up resistor)
- Must use `pinMode(pins::TOUCH_IRQ, INPUT)` NOT `INPUT_PULLUP`
- Interrupt on FALLING edge only

### WiFi EMI Characteristics
- EMI pulses: <0.01ms (very brief)
- Real touches: >70ms (sustained)
- Threshold: 10ms (10,000 microseconds)
- Filter efficiency typically >95%

## Reference
- Extracted from: ALNScanner1021_Orchestrator v4.1
- Source lines: 1187-1207 (ISR & pulse measurement), 2867-2871 (initialization)
- Original validation: test-sketches/45-touch-wifi-emi/
- Date: October 22, 2025
