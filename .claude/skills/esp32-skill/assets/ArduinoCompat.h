/**
 * ArduinoCompat.h - Arduino API compatibility layer for native builds
 * 
 * Provides stubs for common Arduino types and functions so ESP32 code
 * can compile and run in native test environments.
 * 
 * Usage:
 *   #ifdef NATIVE_BUILD
 *   #include "ArduinoCompat.h"
 *   #else
 *   #include <Arduino.h>
 *   #endif
 */

#ifndef ARDUINO_COMPAT_H
#define ARDUINO_COMPAT_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

// ============================================================================
// Type Definitions
// ============================================================================

typedef uint8_t byte;
typedef bool boolean;

// ============================================================================
// Constants
// ============================================================================

#define HIGH 1
#define LOW 0

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3

#define LED_BUILTIN 2

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

// ============================================================================
// String Class
// ============================================================================

class String {
public:
    String() : _str("") {}
    String(const char* str) : _str(str ? str : "") {}
    String(const String& other) : _str(other._str) {}
    String(int value, int base = 10) {
        char buf[34];
        switch(base) {
            case 16: snprintf(buf, sizeof(buf), "%X", value); break;
            case 8:  snprintf(buf, sizeof(buf), "%o", value); break;
            case 2:  {
                // Binary conversion
                if (value == 0) { _str = "0"; return; }
                std::string binary;
                unsigned int uval = (unsigned int)value;
                while (uval > 0) {
                    binary = (char)('0' + (uval & 1)) + binary;
                    uval >>= 1;
                }
                _str = binary;
                return;
            }
            default: snprintf(buf, sizeof(buf), "%d", value); break;
        }
        _str = buf;
    }
    String(unsigned int value, int base = 10) : String((int)value, base) {}
    String(long value, int base = 10) : String((int)value, base) {}
    String(unsigned long value, int base = 10) : String((int)value, base) {}
    String(float value, int decimalPlaces = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimalPlaces, value);
        _str = buf;
    }
    String(double value, int decimalPlaces = 2) : String((float)value, decimalPlaces) {}
    
    // Assignment
    String& operator=(const String& rhs) { _str = rhs._str; return *this; }
    String& operator=(const char* rhs) { _str = rhs ? rhs : ""; return *this; }
    
    // Concatenation
    String& operator+=(const String& rhs) { _str += rhs._str; return *this; }
    String& operator+=(const char* rhs) { if (rhs) _str += rhs; return *this; }
    String& operator+=(char c) { _str += c; return *this; }
    String& operator+=(int num) { _str += std::to_string(num); return *this; }
    
    String operator+(const String& rhs) const { return String((_str + rhs._str).c_str()); }
    String operator+(const char* rhs) const { return String((_str + (rhs ? rhs : "")).c_str()); }
    
    friend String operator+(const char* lhs, const String& rhs) {
        return String((std::string(lhs ? lhs : "") + rhs._str).c_str());
    }
    
    // Comparison
    bool operator==(const String& rhs) const { return _str == rhs._str; }
    bool operator==(const char* rhs) const { return _str == (rhs ? rhs : ""); }
    bool operator!=(const String& rhs) const { return !(*this == rhs); }
    bool operator!=(const char* rhs) const { return !(*this == rhs); }
    
    bool equals(const String& s) const { return _str == s._str; }
    bool equals(const char* s) const { return _str == (s ? s : ""); }
    bool equalsIgnoreCase(const String& s) const {
        if (_str.length() != s._str.length()) return false;
        for (size_t i = 0; i < _str.length(); i++) {
            if (tolower(_str[i]) != tolower(s._str[i])) return false;
        }
        return true;
    }
    
    // Access
    char charAt(unsigned int index) const { 
        return index < _str.length() ? _str[index] : 0; 
    }
    char operator[](unsigned int index) const { return charAt(index); }
    
    void setCharAt(unsigned int index, char c) {
        if (index < _str.length()) _str[index] = c;
    }
    
    // Properties
    unsigned int length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }
    
    // Conversion
    const char* c_str() const { return _str.c_str(); }
    
    int toInt() const { return atoi(_str.c_str()); }
    float toFloat() const { return atof(_str.c_str()); }
    double toDouble() const { return atof(_str.c_str()); }
    
    void toUpperCase() {
        for (char& c : _str) c = toupper(c);
    }
    
    void toLowerCase() {
        for (char& c : _str) c = tolower(c);
    }
    
    // Search
    int indexOf(char c, unsigned int fromIndex = 0) const {
        size_t pos = _str.find(c, fromIndex);
        return pos != std::string::npos ? (int)pos : -1;
    }
    
    int indexOf(const String& s, unsigned int fromIndex = 0) const {
        size_t pos = _str.find(s._str, fromIndex);
        return pos != std::string::npos ? (int)pos : -1;
    }
    
    int lastIndexOf(char c) const {
        size_t pos = _str.rfind(c);
        return pos != std::string::npos ? (int)pos : -1;
    }
    
    int lastIndexOf(const String& s) const {
        size_t pos = _str.rfind(s._str);
        return pos != std::string::npos ? (int)pos : -1;
    }
    
    // Substrings
    String substring(unsigned int beginIndex) const {
        return beginIndex < _str.length() ? String(_str.substr(beginIndex).c_str()) : String();
    }
    
    String substring(unsigned int beginIndex, unsigned int endIndex) const {
        if (beginIndex >= _str.length()) return String();
        return String(_str.substr(beginIndex, endIndex - beginIndex).c_str());
    }
    
    // Modification
    void trim() {
        size_t start = _str.find_first_not_of(" \t\n\r");
        size_t end = _str.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) {
            _str = "";
        } else {
            _str = _str.substr(start, end - start + 1);
        }
    }
    
    void remove(unsigned int index, unsigned int count = 1) {
        if (index < _str.length()) {
            _str.erase(index, count);
        }
    }
    
    void replace(const String& find, const String& replace) {
        size_t pos = 0;
        while ((pos = _str.find(find._str, pos)) != std::string::npos) {
            _str.replace(pos, find._str.length(), replace._str);
            pos += replace._str.length();
        }
    }
    
    bool startsWith(const String& prefix) const {
        return _str.compare(0, prefix._str.length(), prefix._str) == 0;
    }
    
    bool endsWith(const String& suffix) const {
        if (suffix._str.length() > _str.length()) return false;
        return _str.compare(_str.length() - suffix._str.length(), 
                           suffix._str.length(), suffix._str) == 0;
    }
    
    // Utility
    void getBytes(unsigned char* buf, unsigned int bufsize, unsigned int index = 0) const {
        if (!buf || bufsize == 0) return;
        size_t len = std::min((size_t)(bufsize - 1), _str.length() - index);
        memcpy(buf, _str.c_str() + index, len);
        buf[len] = 0;
    }
    
    void toCharArray(char* buf, unsigned int bufsize, unsigned int index = 0) const {
        getBytes((unsigned char*)buf, bufsize, index);
    }
    
private:
    std::string _str;
};

// ============================================================================
// Serial Class
// ============================================================================

class SerialClass {
public:
    void begin(unsigned long baud) { (void)baud; }
    void end() {}
    
    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() { fflush(stdout); }
    
    size_t print(const char* s) { return printf("%s", s); }
    size_t print(const String& s) { return printf("%s", s.c_str()); }
    size_t print(char c) { return printf("%c", c); }
    size_t print(int n, int base = DEC) {
        switch(base) {
            case HEX: return printf("%X", n);
            case OCT: return printf("%o", n);
            case BIN: return printf("%s", String(n, 2).c_str());
            default:  return printf("%d", n);
        }
    }
    size_t print(unsigned int n, int base = DEC) { return print((int)n, base); }
    size_t print(long n, int base = DEC) { return print((int)n, base); }
    size_t print(unsigned long n, int base = DEC) { return print((int)n, base); }
    size_t print(double n, int digits = 2) { return printf("%.*f", digits, n); }
    
    size_t println() { return printf("\n"); }
    size_t println(const char* s) { return printf("%s\n", s); }
    size_t println(const String& s) { return printf("%s\n", s.c_str()); }
    size_t println(char c) { return printf("%c\n", c); }
    size_t println(int n, int base = DEC) { print(n, base); return println(); }
    size_t println(unsigned int n, int base = DEC) { return println((int)n, base); }
    size_t println(long n, int base = DEC) { return println((int)n, base); }
    size_t println(unsigned long n, int base = DEC) { return println((int)n, base); }
    size_t println(double n, int digits = 2) { print(n, digits); return println(); }
    
    size_t write(uint8_t c) { return putchar(c) != EOF ? 1 : 0; }
    size_t write(const uint8_t* buf, size_t size) { return fwrite(buf, 1, size, stdout); }
    size_t write(const char* buf) { return printf("%s", buf); }
    
    size_t printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        size_t result = vprintf(format, args);
        va_end(args);
        return result;
    }
    
    operator bool() { return true; }
};

extern SerialClass Serial;

#ifndef ARDUINO_COMPAT_IMPL
SerialClass Serial;
#endif

// ============================================================================
// Time Functions
// ============================================================================

// These return 0 by default - for real timing tests, use IClock interface
inline unsigned long millis() { return 0; }
inline unsigned long micros() { return 0; }
inline void delay(unsigned long ms) { (void)ms; }
inline void delayMicroseconds(unsigned int us) { (void)us; }

// ============================================================================
// GPIO Functions (No-ops for native builds)
// ============================================================================

inline void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
inline void digitalWrite(uint8_t pin, uint8_t val) { (void)pin; (void)val; }
inline int digitalRead(uint8_t pin) { (void)pin; return LOW; }
inline int analogRead(uint8_t pin) { (void)pin; return 0; }
inline void analogWrite(uint8_t pin, int val) { (void)pin; (void)val; }

// ============================================================================
// Math Functions
// ============================================================================

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef abs
#define abs(x) ((x)>0?(x):-(x))
#endif

#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

#ifndef round
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#endif

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ============================================================================
// Random Functions
// ============================================================================

inline void randomSeed(unsigned long seed) { srand(seed); }
inline long random(long howbig) { return rand() % howbig; }
inline long random(long howsmall, long howbig) { 
    return howsmall + rand() % (howbig - howsmall); 
}

// ============================================================================
// Bit Manipulation
// ============================================================================

#ifndef bit
#define bit(b) (1UL << (b))
#endif

#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#endif

#ifndef bitSet
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#endif

#ifndef bitClear
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#endif

#ifndef bitWrite
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#endif

#ifndef lowByte
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#endif

#ifndef highByte
#define highByte(w) ((uint8_t) ((w) >> 8))
#endif

// ============================================================================
// SPI Stub
// ============================================================================

class SPIClass {
public:
    void begin() {}
    void end() {}
    void beginTransaction(uint32_t settings) { (void)settings; }
    void endTransaction() {}
    uint8_t transfer(uint8_t data) { return data; }
    void transfer(void* buf, size_t count) { (void)buf; (void)count; }
};

extern SPIClass SPI;

#ifndef ARDUINO_COMPAT_IMPL
SPIClass SPI;
#endif

// ============================================================================
// Wire (I2C) Stub  
// ============================================================================

class TwoWire {
public:
    void begin() {}
    void begin(uint8_t addr) { (void)addr; }
    void end() {}
    void beginTransmission(uint8_t addr) { (void)addr; }
    uint8_t endTransmission(bool stop = true) { (void)stop; return 0; }
    uint8_t requestFrom(uint8_t addr, uint8_t qty, bool stop = true) { 
        (void)addr; (void)qty; (void)stop; return 0; 
    }
    size_t write(uint8_t data) { (void)data; return 1; }
    size_t write(const uint8_t* data, size_t qty) { (void)data; return qty; }
    int available() { return 0; }
    int read() { return -1; }
    void setClock(uint32_t freq) { (void)freq; }
};

extern TwoWire Wire;

#ifndef ARDUINO_COMPAT_IMPL
TwoWire Wire;
#endif

// ============================================================================
// ESP32-specific Stubs
// ============================================================================

class EspClass {
public:
    uint32_t getFreeHeap() { return 300000; }
    uint32_t getHeapSize() { return 320000; }
    uint32_t getMinFreeHeap() { return 200000; }
    uint32_t getMaxAllocHeap() { return 100000; }
    uint32_t getChipId() { return 0x12345678; }
    const char* getChipModel() { return "ESP32-NATIVE-TEST"; }
    uint8_t getChipRevision() { return 1; }
    uint32_t getCpuFreqMHz() { return 240; }
    uint32_t getFlashChipSize() { return 4 * 1024 * 1024; }
    void restart() {}
    void deepSleep(uint64_t time_us) { (void)time_us; }
};

extern EspClass ESP;

#ifndef ARDUINO_COMPAT_IMPL
EspClass ESP;
#endif

// ============================================================================
// Yield
// ============================================================================

inline void yield() {}

#endif // ARDUINO_COMPAT_H
