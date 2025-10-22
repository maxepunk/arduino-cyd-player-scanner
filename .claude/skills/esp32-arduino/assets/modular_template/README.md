# Modular ESP32 Template

This template demonstrates proper multi-file organization for ESP32 Arduino projects compiled with Arduino CLI.

## Structure

```
modular_template/
├── modular_template.ino    # Main file (setup/loop coordination)
├── config.h                # Configuration constants
├── module_example.h        # Module header (interface)
├── module_example.cpp      # Module implementation
└── README.md               # This file
```

## Key Concepts

### 1. Separation of Concerns
- **Main .ino**: High-level coordination only (setup/loop)
- **config.h**: All constants and configuration
- **Modules**: Specific functionality encapsulated

### 2. Header/Implementation Split
- **.h files**: Declarations (what the module does)
- **.cpp files**: Definitions (how it does it)
- This separation enables better organization and compilation

### 3. Include Guards
- Every .h file has `#ifndef`/`#define`/`#endif`
- Prevents multiple inclusion errors
- Alternative: `#pragma once`

### 4. Global Instances
- Declared with `extern` in .h file
- Defined in .cpp file
- Accessible throughout the project

## How to Use This Template

### 1. Copy Template to Your Project
```bash
cp -r assets/modular_template/ MyProject/
cd MyProject/
mv modular_template.ino MyProject.ino
```

### 2. Customize Configuration
Edit `config.h`:
- Set pin definitions for your hardware
- Configure feature flags
- Set timing constants
- Add your project-specific constants

### 3. Create Your Modules

Follow the `module_example` pattern:

**For a Display Module:**
```bash
# Create new module files
touch display_manager.h
touch display_manager.cpp
```

In `display_manager.h`:
```cpp
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

class DisplayManager {
public:
    bool begin();
    void clear();
    void drawText(const char* text, int x, int y);
    // ... more methods
    
private:
    TFT_eSPI _tft;
    bool _initialized;
};

extern DisplayManager Display;

#endif
```

In `display_manager.cpp`:
```cpp
#include "display_manager.h"

DisplayManager Display;  // Create global instance

bool DisplayManager::begin() {
    _tft.init();
    _initialized = true;
    return true;
}

void DisplayManager::clear() {
    if (!_initialized) return;
    _tft.fillScreen(TFT_BLACK);
}

void DisplayManager::drawText(const char* text, int x, int y) {
    if (!_initialized) return;
    _tft.setCursor(x, y);
    _tft.print(text);
}
```

### 4. Update Main File

In `MyProject.ino`, add includes and use your modules:

```cpp
#include "config.h"
#include "display_manager.h"
#include "wifi_manager.h"

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    
    Display.begin();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Display.drawText("Ready!", 10, 10);
}

void loop() {
    Display.update();
    WiFi.update();
    // Your coordination logic
}
```

## Adding New Modules

**Step-by-step process:**

1. **Identify module purpose**
   - What does this module handle?
   - What's its public interface?

2. **Create header file** (`modulename.h`)
   - Add include guard
   - Include necessary headers
   - Declare public methods
   - Declare extern instance

3. **Create implementation** (`modulename.cpp`)
   - Include module header first
   - Create global instance
   - Implement all declared methods

4. **Update main file**
   - Add `#include "modulename.h"`
   - Call `Module.begin()` in setup()
   - Call `Module.update()` in loop()

5. **Test incrementally**
   - Compile after creating header
   - Compile after implementation
   - Test module functionality

## Common Module Patterns

### Display Manager
```cpp
class DisplayManager {
    bool begin();
    void clear();
    void drawText(...);
    void update();
};
```

### WiFi Manager
```cpp
class WiFiManager {
    bool begin(const char* ssid, const char* pass);
    bool isConnected();
    void update();
    bool sendData(const char* data);
};
```

### Sensor Controller
```cpp
class SensorController {
    bool begin();
    void update();
    float getValue();
    bool hasNewData();
};
```

### State Machine
```cpp
enum class State { INIT, RUNNING, ERROR };

class StateMachine {
    void setState(State s);
    State getState();
    void update();
};
```

## Compilation

### Compile
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 modular_template/
```

### Upload
```bash
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 modular_template/
```

### Monitor Serial Output
```bash
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

## When to Use This Template

**Use modular template when:**
- ✅ Project > 200 lines
- ✅ Multiple distinct subsystems
- ✅ Code reuse desired
- ✅ Team collaboration
- ✅ Testing individual components needed

**Use simple template when:**
- ❌ Project < 200 lines
- ❌ Single simple purpose
- ❌ Quick prototype or demo

## Troubleshooting

### "multiple definition" error
**Cause:** Implementation in header file

**Fix:** Move implementations to .cpp file

### "undefined reference" error
**Cause:** Declaration without implementation

**Fix:** Verify .cpp file exists and is in sketch directory

### "has not been declared" error
**Cause:** Missing include or forward declaration

**Fix:** Add necessary `#include` or forward declaration

## Best Practices

1. **One module = one responsibility**
2. **Keep public interfaces minimal**
3. **Hide implementation details (private)**
4. **Document public methods**
5. **Test modules independently**
6. **Use DEBUG_PRINTLN for debug output**
7. **Check initialization status**
8. **Handle errors gracefully**
9. **Update modules in loop()**
10. **Keep main file simple (< 100 lines)**

## References

For more detailed information:
- See `references/code_organization.md` for comprehensive guide
- See `references/getting_started.md` for Arduino CLI basics
- See `references/debugging_troubleshooting.md` for common issues

## Examples

See `references/examples/` for working multi-file examples:
- `simple_modular/` - Basic 3-module project
- `cyd_display_project/` - CYD board with display, touch, WiFi

## License

This template is part of the ESP32 Arduino CLI skill and follows the same license terms.
