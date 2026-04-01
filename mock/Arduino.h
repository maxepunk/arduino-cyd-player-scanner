#pragma once
/**
 * Arduino Mock for PlatformIO Native Testing
 *
 * Provides the subset of Arduino API used by ALNScanner_v5 model headers:
 * - String class (length, startsWith, replace, trim, toLowerCase, charAt, c_str, operators)
 * - SerialMock (print, println, printf — all no-ops)
 * - isDigit() function
 * - F() macro and __FlashStringHelper type
 * - Arduino type aliases (byte, uint8_t, etc.)
 *
 * NOTE: This does NOT mock hardware APIs (WiFi, SD, SPI, I2S, FreeRTOS).
 * Only models/ headers are testable with this mock. Testing services/ or hal/
 * requires additional mocks (future phase).
 */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>

// ─── Arduino type aliases ─────────────────────────────────────────────

typedef uint8_t byte;

// ─── Flash string helper (no-op on native) ────────────────────────────

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(string_literal))

// ─── Arduino String class ─────────────────────────────────────────────
// Wraps std::string. Implements the subset used by models/Config.h and
// models/Token.h. Method signatures match Arduino's String exactly:
// replace() is void (mutates in place), toLowerCase() is void, etc.

class String {
    std::string _buf;
public:
    String() {}
    String(const char* s) : _buf(s ? s : "") {}
    String(const String& s) = default;
    String(String&& s) = default;
    String(const __FlashStringHelper* f) : _buf(reinterpret_cast<const char*>(f)) {}
    // Numeric constructors (used by OrchestratorService: String(_nextBatchId), Application: String(i))
    String(int val) : _buf(std::to_string(val)) {}
    String(unsigned int val) : _buf(std::to_string(val)) {}
    String(long val) : _buf(std::to_string(val)) {}
    String(unsigned long val) : _buf(std::to_string(val)) {}

    // Assignment
    String& operator=(const char* s) { _buf = s ? s : ""; return *this; }
    String& operator=(const String&) = default;
    String& operator=(String&&) = default;

    // Length
    unsigned int length() const { return static_cast<unsigned int>(_buf.length()); }

    // Access
    const char* c_str() const { return _buf.c_str(); }
    char charAt(unsigned int i) const { return i < _buf.length() ? _buf[i] : 0; }
    char operator[](unsigned int i) const { return charAt(i); }

    // Search
    bool startsWith(const char* prefix) const {
        return _buf.compare(0, std::strlen(prefix), prefix) == 0;
    }
    bool startsWith(const String& prefix) const { return startsWith(prefix.c_str()); }

    // Mutation (Arduino String mutates in place, returns void)
    void replace(const char* from, const char* to) {
        std::string f(from), t(to);
        size_t pos = 0;
        while ((pos = _buf.find(f, pos)) != std::string::npos) {
            _buf.replace(pos, f.length(), t);
            pos += t.length();
        }
    }
    void replace(const String& from, const String& to) { replace(from.c_str(), to.c_str()); }

    void trim() {
        size_t start = _buf.find_first_not_of(" \t\r\n");
        size_t end = _buf.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) { _buf.clear(); return; }
        _buf = _buf.substr(start, end - start + 1);
    }

    void toLowerCase() {
        for (auto& c : _buf) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }

    // Concatenation
    String operator+(const char* s) const { String r(*this); r._buf += (s ? s : ""); return r; }
    String operator+(const String& s) const { String r(*this); r._buf += s._buf; return r; }
    String& operator+=(const char* s) { _buf += (s ? s : ""); return *this; }
    String& operator+=(const String& s) { _buf += s._buf; return *this; }

    // Comparison
    bool operator==(const char* s) const { return _buf == (s ? s : ""); }
    bool operator==(const String& s) const { return _buf == s._buf; }
    bool operator!=(const char* s) const { return !(*this == s); }
    bool operator!=(const String& s) const { return !(*this == s); }

    // Friend: "literal" + String
    friend String operator+(const char* lhs, const String& rhs) {
        String r(lhs); r._buf += rhs._buf; return r;
    }
};

// ─── Serial mock (all no-ops) ─────────────────────────────────────────

class SerialMock {
public:
    void begin(unsigned long) {}
    void print(const char*) {}
    void print(const __FlashStringHelper*) {}
    void print(int) {}
    void println() {}
    void println(const char*) {}
    void println(const __FlashStringHelper*) {}
    void println(int) {}
    // printf is variadic — use va_list to accept any args
    void printf(const char*, ...) {}
};

// C++17 inline variable — avoids multiple-definition errors across translation units
inline SerialMock Serial;

// ─── Arduino functions ────────────────────────────────────────────────

inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
