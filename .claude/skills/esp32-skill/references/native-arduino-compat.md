# Arduino Compatibility Layer for Native Builds

This document explains the `ArduinoCompat.h` header that provides Arduino API stubs for native unit testing.

## Purpose

When running native tests with `pio test -e native`, Arduino libraries aren't available. The compatibility layer provides minimal implementations of common Arduino types and functions so your code compiles and tests can run.

## What's Provided

### Types
- `String` class with basic operations
- `byte`, `uint8_t`, `uint16_t`, etc.

### Functions
- `millis()`, `micros()` - Time functions (return 0 or can be mocked)
- `delay()`, `delayMicroseconds()` - No-ops in native mode
- `pinMode()`, `digitalWrite()`, `digitalRead()` - No-ops / return 0
- `analogRead()`, `analogWrite()` - No-ops / return 0

### Objects
- `Serial` - Stub that wraps stdout/stderr
- `SPI` - Empty stub
- `Wire` - Empty stub

### Constants
- `HIGH`, `LOW`
- `INPUT`, `OUTPUT`, `INPUT_PULLUP`
- `LED_BUILTIN`

## Usage

### In Your Code

```cpp
// At the top of files that use Arduino APIs
#ifdef NATIVE_BUILD
#include "ArduinoCompat.h"
#else
#include <Arduino.h>
#endif
```

### In platformio.ini

```ini
[env:native]
platform = native
build_flags = 
    -std=c++11
    -DUNIT_TEST
    -DNATIVE_BUILD
test_framework = unity
```

## Important Limitations

The compatibility layer provides **stubs**, not full implementations:

1. **millis()/micros()** return 0 by default
   - For timing tests, inject an `IClock` interface instead
   
2. **GPIO functions** are no-ops
   - For pin logic tests, inject an `IGPIO` interface instead
   
3. **String** is a simplified implementation
   - Most operations work, but edge cases may differ from Arduino
   
4. **Serial** goes to stdout
   - `Serial.println("test")` prints to console

## Better Approach: Dependency Injection

Instead of relying on the compatibility layer for complex testing, use interfaces:

```cpp
// BAD: Hard to test
class Blinker {
    void blink() {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
    }
};

// GOOD: Easy to test
class Blinker {
    IGPIO* _gpio;
    IClock* _clock;
public:
    Blinker(IGPIO* gpio, IClock* clock) 
        : _gpio(gpio), _clock(clock) {}
    
    void blink() {
        _gpio->digitalWrite(LED_PIN, HIGH);
        _clock->delay(500);
        _gpio->digitalWrite(LED_PIN, LOW);
    }
};
```

Now you can inject mocks and verify the exact sequence of GPIO writes and delays.

## When the Compatibility Layer is Useful

1. **String manipulation** - Testing string parsing/formatting
2. **State machines** - Testing logic that doesn't directly touch hardware
3. **Data processing** - Testing calculations, conversions
4. **Protocol encoding** - Testing message formatting

## When to Use Interfaces Instead

1. **Timing-dependent code** - Use `IClock` mock
2. **GPIO logic** - Use `IGPIO` mock
3. **Hardware communication** - Use device-specific interfaces
4. **File operations** - Use `ISDCard` or `IFileSystem` mock

## Full Header Reference

See `assets/ArduinoCompat.h` for the complete implementation.
