# Code Organization for ESP32 Arduino Projects

This guide provides comprehensive guidance on organizing ESP32 Arduino projects, from single files to complex multi-module architectures. Use this when projects outgrow single files or need better maintainability.

## When to Refactor: Decision Tree

### Single File (< 200 lines)
**Indicators:**
- Total project under 200 lines
- Single clear purpose
- Few external dependencies
- Can be understood in one reading

**Structure:**
```
MyProject/
└── MyProject.ino
```

**Best for:** Simple examples, proof-of-concepts, single-sensor projects

### Config File Split (200-500 lines)
**Indicators:**
- Hard-coded values scattered throughout
- Pin definitions mixed with logic
- Multiple magic numbers
- Need to change settings frequently

**Structure:**
```
MyProject/
├── MyProject.ino
└── config.h
```

**When to use:** First refactoring step, minimal complexity increase

### Modular Organization (500-2000 lines)
**Indicators:**
- Multiple distinct subsystems
- Reusable components desired
- Team collaboration needed
- Want to test individual components

**Structure:**
```
MyProject/
├── MyProject.ino          # Main coordination (50-100 lines)
├── config.h               # Configuration
├── display_mgr.h/cpp      # Display handling
├── wifi_mgr.h/cpp         # Network operations
├── sensor.h/cpp           # Sensor reading
└── utils.h/cpp            # Utilities
```

**Best for:** Production projects, team development, reusable components

### Advanced Architecture (> 2000 lines)
**Indicators:**
- Production-grade application
- Multiple developers
- Extensive testing required
- Complex state management

**Structure:**
```
MyProject/
├── MyProject.ino
├── src/
│   ├── config/
│   │   ├── config.h
│   │   └── pins.h
│   ├── hal/               # Hardware abstraction
│   │   ├── display_hal.h/cpp
│   │   └── sensor_hal.h/cpp
│   ├── drivers/           # Device drivers
│   │   ├── tft_driver.h/cpp
│   │   └── touch_driver.h/cpp
│   └── managers/          # High-level controllers
│       ├── ui_manager.h/cpp
│       └── data_manager.h/cpp
└── test/
    ├── test_display.ino
    └── test_sensor.ino
```

**Best for:** Large applications, products, multiple board variants

## Arduino CLI Compilation Model

### How Arduino CLI Handles Multiple Files

Understanding Arduino CLI's compilation behavior is critical for multi-file projects:

**Automatic compilation:**
1. Main .ino file converts to .cpp internally
2. ALL .cpp files in sketch directory are compiled automatically
3. ALL .cpp files in subdirectories are compiled
4. Object files are linked together
5. No explicit build configuration needed

**Include path behavior:**
- Sketch directory is automatically in include path
- Use `#include "local_file.h"` for sketch files
- Use `#include <Library.h>` for library files
- Subdirectories also added to include path

**Critical differences from Arduino IDE:**
- No "tab" system (just files in directory)
- More explicit about include paths
- Better error messages for missing includes
- Compilation order not guaranteed

**Common pitfalls:**
- Forgetting include guards → multiple definition errors
- Circular includes → compilation failures
- Missing forward declarations → ordering issues
- Implementation in headers → link-time errors

### Proper Header File Structure

Every .h file must follow this pattern:

```cpp
#ifndef MODULE_NAME_H  // Include guard start
#define MODULE_NAME_H

// 1. System includes
#include <Arduino.h>

// 2. Library includes
#include <Wire.h>
#include <SPI.h>

// 3. Local includes (if needed)
#include "config.h"

// 4. Forward declarations (avoid circular dependencies)
class SomeOtherClass;

// 5. Constants
#define MODULE_CONSTANT 42

// 6. Type definitions
typedef struct {
    int value;
    bool flag;
} ModuleData;

// 7. Class declaration (interface only)
class ModuleClass {
public:
    ModuleClass();              // Constructor
    void begin();               // Public methods
    void update();
    int getValue();
    
private:
    int _privateValue;          // Private members
    void _privateMethod();      // Private methods
};

// OR function declarations (for non-class modules)
void moduleInit();
void moduleUpdate();
int moduleGetValue();

// 8. Global instance declaration (if needed)
extern ModuleClass Module;

#endif // MODULE_NAME_H  // Include guard end
```

### Proper Implementation File Structure

Every .cpp file must follow this pattern:

```cpp
// 1. Include own header FIRST
#include "module_name.h"

// 2. Other includes needed for implementation
#include <SPI.h>
#include "other_module.h"

// 3. File-local constants/helpers (use static)
static const int INTERNAL_BUFFER_SIZE = 64;

static void internalHelper() {
    // File-local function
}

// 4. Class implementation
ModuleClass::ModuleClass() {
    _privateValue = 0;
}

void ModuleClass::begin() {
    // Initialization logic
}

void ModuleClass::update() {
    // Update logic
}

int ModuleClass::getValue() {
    return _privateValue;
}

void ModuleClass::_privateMethod() {
    // Private implementation
}

// 5. Global instance definition (if declared extern in header)
ModuleClass Module;

// OR function implementations
void moduleInit() {
    // Implementation
}

void moduleUpdate() {
    // Implementation
}
```

## Step-by-Step Refactoring Workflow

### Phase 1: Extract Configuration (30 minutes)

**Goal:** Move all constants to config.h

**Steps:**
1. Create config.h with include guard
2. Identify all #define, const declarations
3. Move to config.h, grouped by category
4. Add #include "config.h" to .ino
5. Compile and test

**Before (in .ino):**
```cpp
#define LED_PIN 2
#define BUTTON_PIN 0
#define BAUD_RATE 115200
const int SENSOR_PIN = 34;

void setup() {
    Serial.begin(BAUD_RATE);
    pinMode(LED_PIN, OUTPUT);
}
```

**After:**

config.h:
```cpp
#ifndef CONFIG_H
#define CONFIG_H

// Pin Definitions
#define LED_PIN 2
#define BUTTON_PIN 0
#define SENSOR_PIN 34

// Serial Configuration
#define BAUD_RATE 115200

// Timing
#define LOOP_DELAY 1000
#define SENSOR_INTERVAL 5000

#endif // CONFIG_H
```

MyProject.ino:
```cpp
#include "config.h"

void setup() {
    Serial.begin(BAUD_RATE);
    pinMode(LED_PIN, OUTPUT);
}
```

### Phase 2: Identify Module Boundaries (1 hour)

**Goal:** Map functional subsystems

**Analysis questions:**
1. What are the major subsystems? (Display, WiFi, Sensors)
2. Which functions always change together?
3. What has natural interface boundaries?
4. What might be reused in other projects?

**Mapping technique:**
1. List all functions in your .ino
2. Group related functions
3. Identify dependencies between groups
4. Draw dependency diagram

**Common module patterns:**
- **Display Manager**: TFT initialization, drawing, updates
- **WiFi Manager**: Connection, requests, reconnection logic
- **Sensor Controller**: Reading, filtering, calibration
- **Data Logger**: Storage, retrieval, formatting
- **UI Handler**: Button input, touch processing
- **State Machine**: Application state transitions

**Example mapping:**
```
setup() calls:
  - initDisplay()      → Display module
  - connectWiFi()      → WiFi module
  - setupSensors()     → Sensor module

loop() calls:
  - updateDisplay()    → Display module
  - checkWiFi()        → WiFi module
  - readSensors()      → Sensor module
  - handleButtons()    → UI module
  - updateStateMachine() → State module
```

### Phase 3: Create Module Headers (2 hours)

**Goal:** Define public interfaces

**For each module:**
1. Create modulename.h
2. Add include guard
3. Include necessary headers
4. Declare public interface only
5. Add documentation comments
6. Do NOT implement yet

**Example: Display Manager**

display_manager.h:
```cpp
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

/**
 * Display Manager
 * Handles all TFT display operations
 */
class DisplayManager {
public:
    // Initialization
    bool begin();
    
    // Basic drawing
    void clear();
    void drawText(const char* text, int x, int y, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    
    // High-level UI
    void showStatus(const char* message);
    void showError(const char* error);
    void drawProgressBar(int x, int y, int w, int h, int percent);
    
    // Update
    void update();
    
private:
    TFT_eSPI _tft;
    bool _initialized;
    void _initializeTFT();
    void _clearArea(int x, int y, int w, int h);
};

// Global instance
extern DisplayManager Display;

#endif // DISPLAY_MANAGER_H
```

### Phase 4: Implement Modules (3-5 hours)

**Goal:** Move implementation to .cpp files

**For each module:**
1. Create modulename.cpp
2. Include module header first
3. Move relevant code from .ino
4. Implement all declared methods
5. Create global instance if needed
6. Test compilation after each

**Example: Display Manager**

display_manager.cpp:
```cpp
#include "display_manager.h"

// Create global instance
DisplayManager Display;

bool DisplayManager::begin() {
    _initialized = false;
    
    _tft.init();
    _tft.setRotation(TFT_ROTATION);
    _tft.fillScreen(TFT_BLACK);
    _tft.setTextSize(2);
    _tft.setTextColor(TFT_WHITE);
    
    _initialized = true;
    
    #ifdef DEBUG_MODE
    Serial.println("DisplayManager: Initialized");
    #endif
    
    return true;
}

void DisplayManager::clear() {
    if (!_initialized) return;
    _tft.fillScreen(TFT_BLACK);
}

void DisplayManager::drawText(const char* text, int x, int y, uint16_t color) {
    if (!_initialized) return;
    _tft.setCursor(x, y);
    _tft.setTextColor(color);
    _tft.print(text);
}

void DisplayManager::drawRect(int x, int y, int w, int h, uint16_t color) {
    if (!_initialized) return;
    _tft.drawRect(x, y, w, h, color);
}

void DisplayManager::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (!_initialized) return;
    _tft.fillRect(x, y, w, h, color);
}

void DisplayManager::showStatus(const char* message) {
    clear();
    drawText(message, 10, 10, TFT_WHITE);
}

void DisplayManager::showError(const char* error) {
    clear();
    drawText("ERROR:", 10, 10, TFT_RED);
    drawText(error, 10, 40, TFT_RED);
}

void DisplayManager::drawProgressBar(int x, int y, int w, int h, int percent) {
    if (!_initialized) return;
    
    // Clamp percent to 0-100
    percent = constrain(percent, 0, 100);
    
    // Draw border
    drawRect(x, y, w, h, TFT_WHITE);
    
    // Clear interior
    _clearArea(x + 1, y + 1, w - 2, h - 2);
    
    // Draw fill
    int fillWidth = ((w - 2) * percent) / 100;
    if (fillWidth > 0) {
        fillRect(x + 1, y + 1, fillWidth, h - 2, TFT_GREEN);
    }
}

void DisplayManager::update() {
    // Could add periodic refresh logic here
}

void DisplayManager::_initializeTFT() {
    // Private helper if needed
}

void DisplayManager::_clearArea(int x, int y, int w, int h) {
    fillRect(x, y, w, h, TFT_BLACK);
}
```

### Phase 5: Update Main File (1 hour)

**Goal:** Simplify .ino to coordination only

**Main file should contain:**
1. Module includes
2. Minimal global state
3. setup() - initialize modules
4. loop() - coordinate modules

**Example transformed main:**

MyProject.ino:
```cpp
#include "config.h"
#include "display_manager.h"
#include "wifi_manager.h"
#include "sensor_controller.h"

// Global application state (minimal)
unsigned long lastUpdate = 0;

void setup() {
    // Serial for debugging
    Serial.begin(BAUD_RATE);
    delay(1000);
    Serial.println("\n=== Starting MyProject ===\n");
    
    // Initialize modules in order
    if (!Display.begin()) {
        Serial.println("ERROR: Display init failed!");
        while(1) delay(1000);  // Halt on critical error
    }
    
    if (!WiFi.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Display.showError("WiFi failed");
        // Continue anyway (may retry)
    }
    
    if (!Sensor.begin()) {
        Display.showError("Sensor failed");
        // Continue anyway
    }
    
    Display.showStatus("System Ready");
    Serial.println("Setup complete\n");
}

void loop() {
    unsigned long now = millis();
    
    // Update all modules
    WiFi.update();
    Sensor.update();
    Display.update();
    
    // Application logic
    if (now - lastUpdate >= UPDATE_INTERVAL) {
        lastUpdate = now;
        
        // Get sensor data
        if (Sensor.hasNewData()) {
            float value = Sensor.getValue();
            
            // Update display
            char msg[32];
            snprintf(msg, sizeof(msg), "Sensor: %.2f", value);
            Display.showStatus(msg);
            
            // Send to WiFi if connected
            if (WiFi.isConnected()) {
                WiFi.sendData(value);
            }
        }
    }
    
    delay(10);  // Small delay for stability
}
```

### Phase 6: Test and Iterate (2 hours)

**Testing checklist:**
- [ ] Compile without errors or warnings
- [ ] Upload to board successfully
- [ ] Verify serial output shows initialization
- [ ] Test each module's basic functionality
- [ ] Test module interactions
- [ ] Check heap usage: `ESP.getFreeHeap()`
- [ ] Run for extended period (memory leaks)
- [ ] Test error conditions

**Common issues after refactoring:**
1. **Missing includes** - Add needed headers to .cpp files
2. **Undefined references** - Verify .cpp files in sketch directory
3. **Multiple definitions** - Check include guards, move implementations to .cpp
4. **Wrong behavior** - Verify initialization order in setup()

## Common Patterns for ESP32

### Pattern 1: Singleton Manager

**Use when:** One instance of hardware resource

```cpp
// In header
class WiFiManager {
public:
    static WiFiManager& getInstance() {
        static WiFiManager instance;
        return instance;
    }
    
    bool begin(const char* ssid, const char* pass);
    bool isConnected();
    
private:
    WiFiManager() {}  // Private constructor
    WiFiManager(const WiFiManager&) = delete;  // No copying
    WiFiManager& operator=(const WiFiManager&) = delete;
};

// Usage
WiFiManager::getInstance().begin("SSID", "pass");
```

### Pattern 2: Callback-Based Async

**Use when:** Non-blocking operations needed

```cpp
// In header
typedef void (*SensorCallback)(float value);

class SensorController {
public:
    void setCallback(SensorCallback cb);
    void update();  // Call in loop()
    
private:
    SensorCallback _callback;
    unsigned long _lastRead;
};

// In implementation
void SensorController::update() {
    if (millis() - _lastRead > READ_INTERVAL) {
        float value = analogRead(SENSOR_PIN) * 3.3 / 4095.0;
        if (_callback) {
            _callback(value);
        }
        _lastRead = millis();
    }
}

// Usage
void handleData(float v) {
    Serial.println(v);
}

Sensor.setCallback(handleData);
```

### Pattern 3: State Machine Module

**Use when:** Complex state-dependent behavior

```cpp
// state_machine.h
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

enum class AppState {
    INIT,
    CONNECTING,
    RUNNING,
    ERROR,
    SLEEP
};

class StateMachine {
public:
    void begin();
    void update();
    void setState(AppState newState);
    AppState getState() const { return _state; }
    
private:
    AppState _state;
    unsigned long _stateStartTime;
    
    void _handleInit();
    void _handleConnecting();
    void _handleRunning();
    void _handleError();
    void _handleSleep();
};

extern StateMachine State;

#endif
```

### Pattern 4: Hardware Abstraction Layer

**Use when:** Supporting multiple hardware variants

```cpp
// hal_display.h
class DisplayHAL {
public:
    virtual bool begin() = 0;
    virtual void clear() = 0;
    virtual void drawPixel(int x, int y, uint16_t c) = 0;
};

class TFT_DisplayHAL : public DisplayHAL {
public:
    bool begin() override;
    void clear() override;
    void drawPixel(int x, int y, uint16_t c) override;
private:
    TFT_eSPI _tft;
};

// Usage - easy to swap implementations
DisplayHAL* display = new TFT_DisplayHAL();
```

## Avoiding Common Pitfalls

### Pitfall 1: Circular Dependencies

**Problem:**
```cpp
// display.h includes sensor.h
// sensor.h includes display.h
// = Compilation error!
```

**Solution - Forward Declarations:**
```cpp
// display.h
class SensorController;  // Forward declaration

class DisplayManager {
    void setSensor(SensorController* s);
private:
    SensorController* _sensor;  // Pointer only
};
```

### Pitfall 2: Missing Include Guards

**Problem:** Multiple inclusion causes redefinition errors

**Solution:**
```cpp
#ifndef MY_MODULE_H
#define MY_MODULE_H
// content
#endif
```

Or use `#pragma once` (simpler, widely supported):
```cpp
#pragma once
// content
```

### Pitfall 3: Implementation in Header

**Wrong:**
```cpp
// module.h
void myFunction() {  // Definition in header!
    Serial.println("Bad");
}
```

**Correct:**
```cpp
// module.h
void myFunction();  // Declaration only

// module.cpp
void myFunction() {
    Serial.println("Good");
}
```

**Exception:** Templates and inline must be in headers:
```cpp
// OK in header
template<typename T>
T max(T a, T b) { return (a > b) ? a : b; }

inline int fast() { return 42; }
```

### Pitfall 4: Global State Chaos

**Problem:**
```cpp
// Multiple files modifying globals
int sensorValue;  // In many files
```

**Solution - Encapsulate:**
```cpp
// sensor.h
class Sensor {
public:
    static int getValue();
private:
    static int _value;
};

// sensor.cpp
int Sensor::_value = 0;
```

## CYD-Specific Organization

### Challenge: Shared SPI Bus (Display + Touch + SD)

**Problem:** All peripherals need SPI coordination

**Solution - SPI Coordinator:**

spi_coordinator.h/cpp:
```cpp
class SPICoordinator {
public:
    void begin();
    void beginDisplayTransaction();
    void endDisplayTransaction();
    void beginTouchTransaction();
    void endTouchTransaction();
    void beginSDTransaction();
    void endSDTransaction();
    
private:
    SPISettings _displaySettings;
    SPISettings _touchSettings;
    SPISettings _sdSettings;
};
```

### Challenge: Limited GPIO on CYD

**Solution - Centralized Pin Management:**

cyd_pins.h:
```cpp
#ifndef CYD_PINS_H
#define CYD_PINS_H

// Display (HSPI - occupied)
#define TFT_CS 15
#define TFT_DC 2
#define TFT_MOSI 13
#define TFT_CLK 14

// Touch
#define TOUCH_CS 33

// SD Card
#define SD_CS 5

// RGB LED (can sacrifice for 3 GPIOs)
#define LED_R 4
#define LED_G 16
#define LED_B 17

// Available GPIOs
#define GPIO_1 22  // Easy access
#define GPIO_2 27  // Easy access
#define GPIO_3 35  // Input only

// Pin allocation tracker
class CYD_PinManager {
public:
    static void configureForDisplay();
    static void configureForExternal();
    static void disableRGBLED();  // Frees 3 pins
    static void printPinout();
};

#endif
```

### Challenge: Soft SPI Workarounds

**Solution - Abstraction Layer:**

spi_config.h:
```cpp
#ifndef SPI_CONFIG_H
#define SPI_CONFIG_H

#ifdef CYD_NEEDS_SOFT_SPI
  #define USE_SOFT_SPI 1
  #define SOFT_MOSI 23
  #define SOFT_MISO 19
  #define SOFT_CLK 18
#else
  #define USE_SOFT_SPI 0
#endif

void configureSPI();
void transferSPI(uint8_t data);

#endif
```

## Compilation Troubleshooting

### Error: "multiple definition of..."

**Cause:** Implementation in header, included multiple times

**Solutions:**
1. Move implementation to .cpp
2. Use `inline` keyword for small functions
3. Use `static` for file-local functions
4. Check include guards

### Error: "undefined reference to..."

**Cause:** Declaration exists, no implementation

**Solutions:**
1. Verify .cpp file is in sketch directory
2. Check function signature matches exactly
3. Ensure .cpp includes its header
4. Check for typos in function names

### Error: "... has not been declared"

**Cause:** Missing include or forward declaration

**Solutions:**
1. Add necessary #include
2. Add forward declaration if circular dependency
3. Check include order
4. Verify header has include guard

### Error: "circular dependency detected"

**Cause:** A.h includes B.h AND B.h includes A.h

**Solutions:**
1. Use forward declarations
2. Redesign to break circular dependency
3. Create common types header
4. Move shared types to config.h

## Testing Modular Code

### Unit Test Template

test/test_display.ino:
```cpp
#include "display_manager.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Display Module Test ===\n");
    
    // Test initialization
    if (Display.begin()) {
        Serial.println("[PASS] Display init");
    } else {
        Serial.println("[FAIL] Display init");
        return;
    }
    
    // Test clear
    Display.clear();
    delay(500);
    Serial.println("[PASS] Display clear");
    
    // Test text
    Display.drawText("Test", 10, 10, 0xFFFF);
    delay(500);
    Serial.println("[PASS] Display text");
    
    // Test shapes
    Display.drawRect(50, 50, 100, 50, 0xF800);
    delay(500);
    Serial.println("[PASS] Display rect");
    
    Serial.println("\n=== All tests passed ===\n");
}

void loop() {}
```

### Integration Test Template

test/test_integration.ino:
```cpp
#include "display_manager.h"
#include "sensor_controller.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Integration Test ===\n");
    
    // Initialize modules
    Display.begin();
    Sensor.begin();
    
    // Test interaction
    Display.showStatus("Reading...");
    delay(1000);
    
    float value = Sensor.getValue();
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Value: %.2f", value);
    Display.showStatus(msg);
    
    Serial.printf("Sensor value: %.2f\n", value);
    Serial.println("\n=== Test complete ===\n");
}

void loop() {}
```

## Performance Considerations

### Module Call Overhead
- Function calls: ~1-2 µs (negligible)
- Virtual functions: ~5-10 µs (small)
- Use `inline` for hot paths if needed

### Memory Impact
- Each class instance: size of member variables
- Virtual functions: +4 bytes per object (vtable pointer)
- Multiple .cpp files: no significant overhead

### Build Time
- More files = longer initial compilation
- Arduino CLI caches unchanged files
- Subsequent builds much faster
- Typical: 5-10s for initial, 1-2s for incremental

## Summary Checklist

**Before refactoring:**
- [ ] Project > 200 lines?
- [ ] Clear module boundaries identified?
- [ ] Backup original code (git commit)
- [ ] Read this guide completely
- [ ] Understand Arduino CLI compilation model

**During refactoring:**
- [ ] Extract config.h first
- [ ] One module at a time
- [ ] Test compilation after each module
- [ ] Verify functionality after each module
- [ ] Commit to version control frequently

**After refactoring:**
- [ ] All modules compile without warnings
- [ ] Original functionality preserved
- [ ] No memory leaks (check heap)
- [ ] Document module interactions
- [ ] Update project README
- [ ] Create module usage examples

**Quality markers:**
- [ ] No code duplication
- [ ] Clear, minimal interfaces
- [ ] Low coupling between modules
- [ ] High cohesion within modules
- [ ] Each file < 500 lines
- [ ] Comprehensive error handling
- [ ] Consistent naming conventions
