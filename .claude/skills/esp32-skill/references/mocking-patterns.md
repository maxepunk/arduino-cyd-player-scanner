# Mocking Patterns for ESP32 Native Testing

This guide provides patterns for creating mock implementations of hardware interfaces, enabling native unit tests that run without physical hardware.

## Core Principle: Interface-Based Design

To test ESP32 code without hardware, separate "what" from "how":

```
┌─────────────────┐     ┌─────────────────┐
│   Your Code     │────▶│   Interface     │
│   (Testable)    │     │   (Abstract)    │
└─────────────────┘     └────────┬────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
              ┌─────▼─────┐           ┌──────▼──────┐
              │   Real    │           │    Mock     │
              │  Hardware │           │   (Tests)   │
              └───────────┘           └─────────────┘
```

## Pattern 1: RFID Reader Mock

### Interface Definition

```cpp
// include/interfaces/IRFIDReader.h
#ifndef IRFID_READER_H
#define IRFID_READER_H

#include <Arduino.h>

class IRFIDReader {
public:
    virtual ~IRFIDReader() = default;
    
    // Lifecycle
    virtual bool begin() = 0;
    virtual void end() = 0;
    
    // Card detection
    virtual bool isCardPresent() = 0;
    virtual bool readCard() = 0;
    
    // Card data
    virtual String getUID() = 0;
    virtual uint8_t getCardType() = 0;
    
    // Communication
    virtual void haltCard() = 0;
};

#endif // IRFID_READER_H
```

### Real Implementation

```cpp
// lib/RFIDReader/MFRC522Reader.h
#ifndef MFRC522_READER_H
#define MFRC522_READER_H

#include "interfaces/IRFIDReader.h"
#include <MFRC522.h>

class MFRC522Reader : public IRFIDReader {
public:
    MFRC522Reader(uint8_t ssPin, uint8_t rstPin)
        : _rfid(ssPin, rstPin) {}
    
    bool begin() override {
        SPI.begin();
        _rfid.PCD_Init();
        return _rfid.PCD_PerformSelfTest();
    }
    
    void end() override {
        _rfid.PCD_AntennaOff();
    }
    
    bool isCardPresent() override {
        return _rfid.PICC_IsNewCardPresent();
    }
    
    bool readCard() override {
        return _rfid.PICC_ReadCardSerial();
    }
    
    String getUID() override {
        String uid = "";
        for (byte i = 0; i < _rfid.uid.size; i++) {
            if (_rfid.uid.uidByte[i] < 0x10) uid += "0";
            uid += String(_rfid.uid.uidByte[i], HEX);
        }
        uid.toUpperCase();
        return uid;
    }
    
    uint8_t getCardType() override {
        return _rfid.PICC_GetType(_rfid.uid.sak);
    }
    
    void haltCard() override {
        _rfid.PICC_HaltA();
        _rfid.PCD_StopCrypto1();
    }

private:
    MFRC522 _rfid;
};

#endif
```

### Mock Implementation

```cpp
// test/native/mocks/MockRFIDReader.h
#ifndef MOCK_RFID_READER_H
#define MOCK_RFID_READER_H

#include "interfaces/IRFIDReader.h"
#include <vector>

class MockRFIDReader : public IRFIDReader {
public:
    // === Configuration (set before calling methods) ===
    bool beginShouldSucceed = true;
    bool cardPresent = false;
    bool readShouldSucceed = true;
    String uidToReturn = "AABBCCDD";
    uint8_t cardTypeToReturn = 0x08;
    
    // === Call tracking ===
    int beginCallCount = 0;
    int endCallCount = 0;
    int isCardPresentCallCount = 0;
    int readCardCallCount = 0;
    int haltCardCallCount = 0;
    
    // === Sequence support ===
    std::vector<String> uidSequence;
    size_t uidSequenceIndex = 0;
    
    // === Interface Implementation ===
    
    bool begin() override {
        beginCallCount++;
        return beginShouldSucceed;
    }
    
    void end() override {
        endCallCount++;
    }
    
    bool isCardPresent() override {
        isCardPresentCallCount++;
        return cardPresent;
    }
    
    bool readCard() override {
        readCardCallCount++;
        return readShouldSucceed;
    }
    
    String getUID() override {
        if (!uidSequence.empty() && uidSequenceIndex < uidSequence.size()) {
            return uidSequence[uidSequenceIndex++];
        }
        return uidToReturn;
    }
    
    uint8_t getCardType() override {
        return cardTypeToReturn;
    }
    
    void haltCard() override {
        haltCardCallCount++;
    }
    
    // === Test Helpers ===
    
    void reset() {
        beginShouldSucceed = true;
        cardPresent = false;
        readShouldSucceed = true;
        uidToReturn = "AABBCCDD";
        cardTypeToReturn = 0x08;
        
        beginCallCount = 0;
        endCallCount = 0;
        isCardPresentCallCount = 0;
        readCardCallCount = 0;
        haltCardCallCount = 0;
        
        uidSequence.clear();
        uidSequenceIndex = 0;
    }
    
    void simulateCardTap(const String& uid) {
        cardPresent = true;
        readShouldSucceed = true;
        uidToReturn = uid;
    }
    
    void simulateCardSequence(std::initializer_list<String> uids) {
        uidSequence = std::vector<String>(uids);
        uidSequenceIndex = 0;
        cardPresent = true;
        readShouldSucceed = true;
    }
};

#endif // MOCK_RFID_READER_H
```

### Usage in Tests

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

void test_scanner_detects_card_tap() {
    mockReader.simulateCardTap("12345678");
    scanner->begin();
    
    bool detected = scanner->checkForCard();
    
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_STRING("12345678", scanner->getLastUID().c_str());
}

void test_scanner_handles_multiple_cards() {
    mockReader.simulateCardSequence({"AAAA", "BBBB", "CCCC"});
    scanner->begin();
    
    scanner->checkForCard();
    TEST_ASSERT_EQUAL_STRING("AAAA", scanner->getLastUID().c_str());
    
    scanner->checkForCard();
    TEST_ASSERT_EQUAL_STRING("BBBB", scanner->getLastUID().c_str());
}

void test_scanner_tracks_halt_calls() {
    mockReader.simulateCardTap("DEADBEEF");
    scanner->begin();
    scanner->checkForCard();
    scanner->releaseCard();
    
    TEST_ASSERT_EQUAL(1, mockReader.haltCardCallCount);
}
```

## Pattern 2: SD Card Mock

### Interface

```cpp
// include/interfaces/ISDCard.h
#ifndef ISD_CARD_H
#define ISD_CARD_H

#include <Arduino.h>

class ISDCard {
public:
    virtual ~ISDCard() = default;
    
    virtual bool begin(uint8_t csPin) = 0;
    virtual void end() = 0;
    
    virtual bool exists(const char* path) = 0;
    virtual bool mkdir(const char* path) = 0;
    virtual bool remove(const char* path) = 0;
    
    virtual bool writeFile(const char* path, const char* content) = 0;
    virtual bool appendFile(const char* path, const char* content) = 0;
    virtual String readFile(const char* path) = 0;
    
    virtual size_t totalBytes() = 0;
    virtual size_t usedBytes() = 0;
};

#endif
```

### Mock Implementation

```cpp
// test/native/mocks/MockSDCard.h
#ifndef MOCK_SD_CARD_H
#define MOCK_SD_CARD_H

#include "interfaces/ISDCard.h"
#include <map>

class MockSDCard : public ISDCard {
public:
    // === In-memory filesystem ===
    std::map<String, String> files;
    std::map<String, bool> directories;
    
    // === Configuration ===
    bool beginShouldSucceed = true;
    size_t totalBytesValue = 4 * 1024 * 1024;  // 4MB
    
    // === Call tracking ===
    int beginCallCount = 0;
    String lastWritePath;
    String lastWriteContent;
    
    bool begin(uint8_t csPin) override {
        beginCallCount++;
        return beginShouldSucceed;
    }
    
    void end() override {}
    
    bool exists(const char* path) override {
        String p(path);
        return files.count(p) > 0 || directories.count(p) > 0;
    }
    
    bool mkdir(const char* path) override {
        directories[String(path)] = true;
        return true;
    }
    
    bool remove(const char* path) override {
        files.erase(String(path));
        return true;
    }
    
    bool writeFile(const char* path, const char* content) override {
        lastWritePath = String(path);
        lastWriteContent = String(content);
        files[String(path)] = String(content);
        return true;
    }
    
    bool appendFile(const char* path, const char* content) override {
        String p(path);
        if (files.count(p) == 0) files[p] = "";
        files[p] += String(content);
        return true;
    }
    
    String readFile(const char* path) override {
        String p(path);
        if (files.count(p) > 0) {
            return files[p];
        }
        return "";
    }
    
    size_t totalBytes() override { return totalBytesValue; }
    
    size_t usedBytes() override {
        size_t used = 0;
        for (const auto& f : files) {
            used += f.second.length();
        }
        return used;
    }
    
    void reset() {
        files.clear();
        directories.clear();
        beginShouldSucceed = true;
        beginCallCount = 0;
        lastWritePath = "";
        lastWriteContent = "";
    }
    
    // Test helpers
    void preloadFile(const char* path, const char* content) {
        files[String(path)] = String(content);
    }
};

#endif
```

## Pattern 3: Clock/Time Mock

Essential for testing timing-dependent code without real delays.

### Interface

```cpp
// include/interfaces/IClock.h
#ifndef ICLOCK_H
#define ICLOCK_H

#include <cstdint>

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint32_t millis() = 0;
    virtual uint32_t micros() = 0;
    virtual void delay(uint32_t ms) = 0;
    virtual void delayMicroseconds(uint32_t us) = 0;
};

#endif
```

### Real Implementation

```cpp
// lib/Clock/ArduinoClock.h
#ifndef ARDUINO_CLOCK_H
#define ARDUINO_CLOCK_H

#include "interfaces/IClock.h"
#include <Arduino.h>

class ArduinoClock : public IClock {
public:
    uint32_t millis() override { return ::millis(); }
    uint32_t micros() override { return ::micros(); }
    void delay(uint32_t ms) override { ::delay(ms); }
    void delayMicroseconds(uint32_t us) override { ::delayMicroseconds(us); }
};

#endif
```

### Mock (Fake) Implementation

```cpp
// test/native/mocks/FakeClock.h
#ifndef FAKE_CLOCK_H
#define FAKE_CLOCK_H

#include "interfaces/IClock.h"

class FakeClock : public IClock {
public:
    uint32_t currentMillis = 0;
    uint32_t currentMicros = 0;
    
    int delayCallCount = 0;
    uint32_t lastDelayMs = 0;
    
    uint32_t millis() override { return currentMillis; }
    uint32_t micros() override { return currentMicros; }
    
    void delay(uint32_t ms) override {
        delayCallCount++;
        lastDelayMs = ms;
        currentMillis += ms;
        currentMicros += ms * 1000;
    }
    
    void delayMicroseconds(uint32_t us) override {
        currentMicros += us;
        currentMillis += us / 1000;
    }
    
    // === Test Helpers ===
    
    void advance(uint32_t ms) {
        currentMillis += ms;
        currentMicros += ms * 1000;
    }
    
    void advanceMicros(uint32_t us) {
        currentMicros += us;
        currentMillis += us / 1000;
    }
    
    void reset() {
        currentMillis = 0;
        currentMicros = 0;
        delayCallCount = 0;
        lastDelayMs = 0;
    }
};

#endif
```

### Usage Example

```cpp
// Test timeout behavior without real delays
void test_sensor_times_out_after_5_seconds() {
    FakeClock clock;
    Sensor sensor(&clock);
    
    sensor.startReading();
    
    // Simulate 4 seconds - should not timeout
    clock.advance(4000);
    TEST_ASSERT_FALSE(sensor.hasTimedOut());
    
    // Simulate 2 more seconds (total 6s) - should timeout
    clock.advance(2000);
    TEST_ASSERT_TRUE(sensor.hasTimedOut());
}
```

## Pattern 4: GPIO Mock

For testing pin-dependent logic.

### Interface

```cpp
// include/interfaces/IGPIO.h
#ifndef IGPIO_H
#define IGPIO_H

#include <cstdint>

class IGPIO {
public:
    virtual ~IGPIO() = default;
    virtual void pinMode(uint8_t pin, uint8_t mode) = 0;
    virtual void digitalWrite(uint8_t pin, uint8_t value) = 0;
    virtual int digitalRead(uint8_t pin) = 0;
    virtual int analogRead(uint8_t pin) = 0;
    virtual void analogWrite(uint8_t pin, int value) = 0;
};

#endif
```

### Mock Implementation

```cpp
// test/native/mocks/MockGPIO.h
#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include "interfaces/IGPIO.h"
#include <map>
#include <vector>

class MockGPIO : public IGPIO {
public:
    std::map<uint8_t, uint8_t> pinModes;
    std::map<uint8_t, uint8_t> digitalValues;
    std::map<uint8_t, int> analogValues;
    
    // Track write history for verification
    std::vector<std::pair<uint8_t, uint8_t>> digitalWriteHistory;
    
    void pinMode(uint8_t pin, uint8_t mode) override {
        pinModes[pin] = mode;
    }
    
    void digitalWrite(uint8_t pin, uint8_t value) override {
        digitalValues[pin] = value;
        digitalWriteHistory.push_back({pin, value});
    }
    
    int digitalRead(uint8_t pin) override {
        return digitalValues.count(pin) ? digitalValues[pin] : 0;
    }
    
    int analogRead(uint8_t pin) override {
        return analogValues.count(pin) ? analogValues[pin] : 0;
    }
    
    void analogWrite(uint8_t pin, int value) override {
        analogValues[pin] = value;
    }
    
    // === Test Helpers ===
    
    void reset() {
        pinModes.clear();
        digitalValues.clear();
        analogValues.clear();
        digitalWriteHistory.clear();
    }
    
    void simulateButtonPress(uint8_t pin) {
        digitalValues[pin] = 0;  // Active LOW typical
    }
    
    void simulateButtonRelease(uint8_t pin) {
        digitalValues[pin] = 1;
    }
    
    bool wasWritten(uint8_t pin, uint8_t value) {
        for (const auto& w : digitalWriteHistory) {
            if (w.first == pin && w.second == value) return true;
        }
        return false;
    }
};

#endif
```

## Pattern 5: HTTP Client Mock

For testing network-dependent code.

```cpp
// include/interfaces/IHTTPClient.h
#ifndef IHTTP_CLIENT_H
#define IHTTP_CLIENT_H

#include <Arduino.h>

class IHTTPClient {
public:
    virtual ~IHTTPClient() = default;
    virtual bool begin(const String& url) = 0;
    virtual int GET() = 0;
    virtual int POST(const String& payload) = 0;
    virtual String getString() = 0;
    virtual void end() = 0;
    virtual void addHeader(const String& name, const String& value) = 0;
};

// test/native/mocks/MockHTTPClient.h
class MockHTTPClient : public IHTTPClient {
public:
    String lastURL;
    String lastPayload;
    std::map<String, String> headers;
    
    int responseCode = 200;
    String responseBody = "{}";
    
    bool begin(const String& url) override {
        lastURL = url;
        return true;
    }
    
    int GET() override { return responseCode; }
    
    int POST(const String& payload) override {
        lastPayload = payload;
        return responseCode;
    }
    
    String getString() override { return responseBody; }
    
    void end() override {}
    
    void addHeader(const String& name, const String& value) override {
        headers[name] = value;
    }
    
    // Test helpers
    void simulateResponse(int code, const String& body) {
        responseCode = code;
        responseBody = body;
    }
    
    void simulateNetworkError() {
        responseCode = -1;
    }
};

#endif
```

## Verification Patterns

### Verifying Method Calls

```cpp
void test_initialization_sequence() {
    MockRFIDReader reader;
    Scanner scanner(&reader);
    
    scanner.begin();
    
    // Verify begin was called exactly once
    TEST_ASSERT_EQUAL(1, reader.beginCallCount);
}
```

### Verifying Call Order

```cpp
class OrderTrackingMock {
public:
    std::vector<String> callOrder;
    
    void methodA() { callOrder.push_back("A"); }
    void methodB() { callOrder.push_back("B"); }
    void methodC() { callOrder.push_back("C"); }
};

void test_correct_initialization_order() {
    OrderTrackingMock mock;
    
    // ... use mock ...
    
    TEST_ASSERT_EQUAL(3, mock.callOrder.size());
    TEST_ASSERT_EQUAL_STRING("A", mock.callOrder[0].c_str());
    TEST_ASSERT_EQUAL_STRING("B", mock.callOrder[1].c_str());
    TEST_ASSERT_EQUAL_STRING("C", mock.callOrder[2].c_str());
}
```

### Verifying Arguments

```cpp
void test_file_written_with_correct_content() {
    MockSDCard sd;
    Logger logger(&sd);
    
    logger.log("Test message");
    
    TEST_ASSERT_TRUE(sd.lastWritePath.endsWith(".log"));
    TEST_ASSERT_TRUE(sd.lastWriteContent.indexOf("Test message") >= 0);
}
```

## Tips for Effective Mocking

### 1. Keep Mocks Simple
Mocks should be simpler than real implementations. If your mock is complex, your interface might need refactoring.

### 2. Reset State Between Tests
Always call `mock.reset()` in `setUp()` to ensure test isolation.

### 3. Use Descriptive Configuration Names
```cpp
// Good
mockReader.cardPresent = true;
mockReader.simulateCardTap("12345678");

// Less clear
mockReader.state1 = true;
mockReader.data = "12345678";
```

### 4. Test Both Success and Failure Paths
```cpp
void test_handles_reader_initialization_failure() {
    mockReader.beginShouldSucceed = false;
    
    bool result = scanner.begin();
    
    TEST_ASSERT_FALSE(result);
}
```

### 5. Mock at the Right Level
- **Too low:** Mocking individual register reads
- **Just right:** Mocking device interfaces (RFID, SD, Network)
- **Too high:** Mocking entire subsystems

## When NOT to Mock

Don't mock:
- Pure functions (no side effects)
- Simple data structures
- Standard library functions
- Things that are faster to use real implementations

Do mock:
- Hardware I/O
- Network calls
- File system operations
- Time/delays
- External services
