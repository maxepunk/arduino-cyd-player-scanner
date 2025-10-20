# ESP32 Hardware Constraints and GPIO Limitations

Understanding ESP32 hardware constraints is essential to avoid boot failures, crashes, and unexplained behavior. This guide covers absolute restrictions that **cannot be violated** without hardware consequences.

## GPIO Overview

ESP32 provides **34 physical GPIO pins** with extensive multiplexing capabilities, but several groups have critical restrictions:

- **Total pins**: GPIO 0-39
- **Usable for general I/O**: ~25 pins
- **Input-only**: GPIO 34-39 (6 pins)
- **Reserved/restricted**: GPIO 6-11, 16-17 (flash/PSRAM)
- **Strapping pins**: GPIO 0, 2, 5, 12, 15

## Input-Only Pins (GPIO 34, 35, 36, 39)

**Absolute restrictions:**
- Cannot be used as outputs
- No internal pull-up or pull-down resistors
- Only support analog input (ADC) or digital input with external pull resistors

**Code that will fail:**
```cpp
pinMode(35, OUTPUT);          // Compiles, fails at runtime
pinMode(36, INPUT_PULLUP);    // Compiles, doesn't work (no pull-up available)
digitalWrite(34, HIGH);       // No effect
```

**Correct usage:**
```cpp
pinMode(35, INPUT);           // Digital input only
analogRead(35);               // Analog input works

// For buttons, use external 10kΩ pull-up or pull-down
pinMode(36, INPUT);
// Hardware: Button between GPIO36 and GND, with 10kΩ resistor to 3.3V
```

**Documentation hint:** These pins are marked as "GPI" (General Purpose Input), not "GPIO."

## Strapping Pins - Boot Mode Control

These pins determine boot mode and **must have specific voltage levels** during power-on and reset:

### GPIO 0 (BOOT Button)
- **Boot mode**: Must be HIGH for normal boot, LOW for flash mode
- **Usage**: Can be used after boot completes
- **Common use**: Boot button on development boards
- **Caution**: External circuit must not pull LOW during power-on

### GPIO 2
- **Boot mode**: Must be floating or LOW
- **Usage**: Can be used for LED or other outputs
- **Caution**: Avoid strong pull-ups during boot
- **Note**: Connected to onboard LED on many boards

### GPIO 12 - CRITICAL!
- **Boot mode**: **MUST BE LOW** or chip refuses to boot
- **Function**: Controls flash voltage (1.8V vs 3.3V)
- **Effect of HIGH**: Boot failure with 3.3V flash chips (most common)
- **Usage**: Can be used after boot, but external circuitry MUST allow LOW during boot
- **Solution**: Use 10kΩ pull-down resistor if external circuit required

```cpp
// GPIO 12 can be used after boot
pinMode(12, OUTPUT);  // OK after boot
// But hardware must not pull it HIGH during power-on!
```

### GPIO 15
- **Boot mode**: Controls boot message output on UART TX
- **Usage**: Can be used as output
- **Default**: Has internal pull-up

### GPIO 5
- **Boot mode**: Affects timing of SDIO slave
- **Usage**: Generally safe for most applications
- **Common use**: SPI CS pin

## SPI Flash Pins - Never Touch!

These pins connect to internal flash chip and **must never be used** for external peripherals:

**Absolutely forbidden:**
- GPIO 6-11 (all ESP32 variants)
- GPIO 16-17 (ESP32-WROVER modules with PSRAM)

**Consequences of using these pins:**
- Immediate crash
- Cannot read program code
- May corrupt flash contents
- Chip appears dead until reflashed

## ADC Limitations

### ADC2 and WiFi Conflict - Critical!

**ADC2 pins (GPIO 0, 2, 4, 12-15, 25-27)** cannot be read while WiFi is active:

```cpp
WiFi.begin(ssid, password);

analogRead(25);  // Returns garbage or 0 while WiFi active!
analogRead(33);  // ADC1 - works fine with WiFi
```

**Rule:** Always use **ADC1 pins (GPIO 32-39)** in WiFi-enabled projects.

### ADC Non-Linearity

ESP32 ADC is highly non-linear:

- **0V - 0.13V**: Reads as 0
- **0.15V - 2.5V**: Most accurate range
- **2.5V - 3.2V**: Decreasing accuracy
- **Above 3.2V**: Reads as 4095 (saturated)

**Recommendations:**
- Use voltage dividers to keep readings in 0.5V - 2.4V range
- Apply calibration curves for precision measurements
- Consider external ADC (ADS1115, MCP3008) for accuracy

## Internal Pull Resistors

**Available on most GPIOs** (except 34-39):
- Resistance: ~45kΩ
- Configured via `pinMode(pin, INPUT_PULLUP)` or `pinMode(pin, INPUT_PULLDOWN)`

**Not available:**
- GPIO 34-39 (input-only pins)
- Require external 10kΩ pull resistors for reliable operation

## Pin Usage Recommendations

### Safe General-Purpose Pins

These pins have no major restrictions and are ideal for most applications:

**Best choices:**
- GPIO 13, 14, 25, 26, 27, 32, 33

**Good choices (with minor notes):**
- GPIO 4 (ADC2 - avoid with WiFi)
- GPIO 16, 17 (not on WROVER with PSRAM)
- GPIO 18, 19, 21, 22, 23 (often used for peripherals)

### Pins to Avoid for General I/O

- GPIO 0 (strapping, boot button)
- GPIO 2 (strapping, often onboard LED)
- GPIO 12 (strapping - boot failure if pulled HIGH)
- GPIO 15 (strapping, internal pull-up)
- GPIO 34-39 (input-only)
- GPIO 6-11 (flash pins)

### Example Pin Assignment Strategy

```cpp
// Configuration for a typical ESP32 project
#define SENSOR_PIN    33    // ADC1, safe with WiFi
#define LED_PIN       13    // No restrictions
#define BUTTON_PIN    32    // Has internal pull-up
#define SDA_PIN       21    // I2C SDA (standard)
#define SCL_PIN       22    // I2C SCL (standard)
#define SPI_MISO      19    // VSPI default
#define SPI_MOSI      23    // VSPI default
#define SPI_CLK       18    // VSPI default
#define SPI_CS        5     // VSPI default
```

## Power Considerations

### GPIO Current Limits

- **Maximum per pin**: 40mA (absolute maximum)
- **Recommended per pin**: 20mA
- **Total for all pins**: 200mA recommended maximum

**Never drive high-current devices directly:**
- Use MOSFETs or transistors for motors, solenoids, high-power LEDs
- Add current-limiting resistors for standard LEDs (330Ω typical)

### Voltage Levels

- **Input voltage range**: 0V - 3.3V
- **5V tolerant**: NO! Applying 5V damages GPIO pins
- **Level shifting**: Required for 5V peripherals (use 74LVC245 or similar)

## Quick Reference Table

| GPIO | Input | Output | Pull-Up | Pull-Down | ADC | Notes |
|------|-------|--------|---------|-----------|-----|-------|
| 0 | ✓ | ✓ | ✓ | ✓ | ADC2 | Strapping - boot mode |
| 2 | ✓ | ✓ | ✓ | ✓ | ADC2 | Strapping - onboard LED |
| 4 | ✓ | ✓ | ✓ | ✓ | ADC2 | Avoid with WiFi |
| 5 | ✓ | ✓ | ✓ | ✓ | - | Strapping, SPI CS |
| 12 | ✓ | ✓ | ✓ | ✓ | ADC2 | **MUST BE LOW AT BOOT** |
| 13 | ✓ | ✓ | ✓ | ✓ | ADC2 | Safe for general use |
| 14 | ✓ | ✓ | ✓ | ✓ | ADC2 | Safe for general use |
| 15 | ✓ | ✓ | ✓ | ✓ | ADC2 | Strapping, has pull-up |
| 32 | ✓ | ✓ | ✓ | ✓ | ADC1 | Safe with WiFi |
| 33 | ✓ | ✓ | ✓ | ✓ | ADC1 | Safe with WiFi |
| 34 | ✓ | ✗ | ✗ | ✗ | ADC1 | Input-only |
| 35 | ✓ | ✗ | ✗ | ✗ | ADC1 | Input-only |
| 36 | ✓ | ✗ | ✗ | ✗ | ADC1 | Input-only (VP) |
| 39 | ✓ | ✗ | ✗ | ✗ | ADC1 | Input-only (VN) |

## Next Steps

- See **communication_interfaces.md** for SPI, I2C, UART pin assignments
- See **hardware_specs/your_board.md** for board-specific pinouts
