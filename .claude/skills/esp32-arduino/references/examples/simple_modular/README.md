# Simple Modular ESP32 Example

This is a complete working example demonstrating proper multi-file organization for ESP32 Arduino projects.

## What This Example Demonstrates

- **Modular code organization** with 3 distinct modules
- **Proper header/implementation split** (.h and .cpp files)
- **Non-blocking timing patterns** using millis()
- **Clean public interfaces** with private implementation
- **Global module instances** for easy access
- **Configuration centralization** in config.h

## Project Structure

```
simple_modular/
├── simple_modular.ino      # Main coordination (90 lines)
├── config.h                # Configuration (35 lines)
├── led_controller.h/cpp    # LED blinking (60 lines)
├── button_handler.h/cpp    # Button input (80 lines)
├── sensor_reader.h/cpp     # Analog reading (100 lines)
└── README.md               # This file

Total: ~365 lines across 7 files
```

## The Three Modules

### 1. LED Controller
**Purpose:** Manages LED blinking patterns

**Public interface:**
- `begin()` - Initialize LED
- `update()` - Call in loop() for timing
- `setBlinkRate(ms)` - Set blink speed
- `setState(on/off)` - Set solid on/off

**Private implementation:**
- Non-blocking timing with millis()
- State tracking
- Pin control

### 2. Button Handler
**Purpose:** Debounced button input

**Public interface:**
- `begin()` - Initialize button
- `update()` - Call in loop() for reading
- `wasPressed()` - Returns true once per press
- `isPressed()` - Returns current state

**Private implementation:**
- Debounce logic
- Edge detection
- State tracking

### 3. Sensor Reader
**Purpose:** Analog sensor reading with averaging

**Public interface:**
- `begin()` - Initialize sensor
- `update()` - Call in loop() for periodic reading
- `hasNewReading()` - Returns true when new data
- `getRawValue()` - Get raw ADC value
- `getVoltage()` - Get converted voltage

**Private implementation:**
- Periodic reading
- Moving average filter
- ADC to voltage conversion

## How It Works

### Main File Responsibilities
The `simple_modular.ino` file is kept simple (90 lines):
- Includes module headers
- Manages application state (modes)
- Coordinates modules in loop()
- No direct hardware access

### Module Responsibilities
Each module:
- Handles specific hardware/functionality
- Provides clean public interface
- Hides implementation details
- Can be tested independently
- Can be reused in other projects

### Interaction Example
```
User presses button
    ↓
ButtonHandler detects press in update()
    ↓
Main loop checks wasPressed()
    ↓
Main changes mode
    ↓
Main tells LEDController new blink rate
    ↓
LEDController updates blinking in its update()
```

## Build and Upload

### Compile
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 simple_modular/
```

### Upload
```bash
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 simple_modular/
```

### Monitor
```bash
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

## Expected Behavior

1. **On startup:**
   - Serial output shows initialization
   - LED blinks slowly (1000ms)

2. **First button press:**
   - Mode changes to fast blink
   - LED blinks quickly (200ms)

3. **Second button press:**
   - Mode changes to sensor display
   - LED turns solid on
   - Sensor readings print every second

4. **Third button press:**
   - Cycles back to slow blink

## Hardware Required

- ESP32 development board
- Built-in LED (GPIO 2)
- Built-in BOOT button (GPIO 0)
- Optional: Sensor on GPIO 34 (or just leave floating to see noise)

## Key Learning Points

### 1. Include Order Matters
Each .cpp file includes its own .h file first:
```cpp
#include "module_name.h"  // Own header first
#include "other_stuff.h"  // Then others
```

### 2. Global Instances Pattern
Header declares:
```cpp
extern ModuleClass Module;
```

Implementation defines:
```cpp
ModuleClass Module;  // Create actual object
```

### 3. Non-Blocking Updates
All modules use this pattern:
```cpp
void update() {
    unsigned long now = millis();
    if (now - _lastAction >= interval) {
        _lastAction = now;
        // Do periodic action
    }
}
```

### 4. Clean Interfaces
Public methods are minimal:
```cpp
class Module {
public:
    bool begin();      // One-time init
    void update();     // Periodic update
    Type getValue();   // Getters
private:
    // Hide all details
};
```

## Adapting This Example

### To Add a New Module

1. **Create header** (e.g., `display.h`):
```cpp
#ifndef DISPLAY_H
#define DISPLAY_H
#include <Arduino.h>
#include "config.h"

class DisplayClass {
public:
    bool begin();
    void update();
    void showText(const char* text);
private:
    bool _initialized;
};

extern DisplayClass Display;
#endif
```

2. **Create implementation** (`display.cpp`):
```cpp
#include "display.h"

DisplayClass Display;

bool DisplayClass::begin() {
    _initialized = true;
    return true;
}

void DisplayClass::update() {
    // Update logic
}

void DisplayClass::showText(const char* text) {
    Serial.println(text);
}
```

3. **Use in main file**:
```cpp
#include "display.h"

void setup() {
    Display.begin();
}

void loop() {
    Display.update();
}
```

### To Modify for Your Hardware

1. Edit `config.h` to set your pins
2. Modify module implementations for your sensors/actuators
3. Keep the same structure and patterns
4. Test each module independently

## Common Mistakes to Avoid

❌ **Don't** put implementation in .h files
✅ **Do** put only declarations in .h

❌ **Don't** use blocking delays in modules
✅ **Do** use non-blocking millis() timing

❌ **Don't** access hardware directly in main
✅ **Do** access through module interfaces

❌ **Don't** forget to call update() in loop()
✅ **Do** call update() for each module

## Troubleshooting

### "multiple definition" error
Check that implementations are in .cpp, not .h

### "undefined reference" error
Verify .cpp file is in the sketch directory

### Nothing happens
Verify you're calling update() in loop()

### Button not responsive
Check BUTTON_PIN is correct for your board

## Next Steps

After understanding this example:
1. See `references/code_organization.md` for detailed patterns
2. See CYD example for more complex organization
3. Try creating your own module following these patterns

## License

Part of the ESP32 Arduino CLI skill, same license terms apply.
