#pragma once
// Minimal Arduino.h stub for native (host) unit test builds.
// Provides the types, macros, and Serial interface that firmware code expects,
// implemented using standard POSIX/C++14 so tests run without any hardware.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

// ESP32-specific attribute — no-op on host
#define RTC_DATA_ATTR
#define PROGMEM
#define IRAM_ATTR

// Arduino type aliases (stdint already covers uint8_t etc.)
typedef bool boolean;
typedef unsigned char byte;

// Minimal String class (used in some headers but not in tested functions)
class String {
public:
    String() {}
    String(const char*) {}
    String(int) {}
};

// Serial stub — routes to stdout so test output is readable
class SerialClass {
public:
    void begin(int) {}
    void print(const char* s)  { printf("%s", s ? s : "(null)"); }
    void print(int v)          { printf("%d", v); }
    void print(long v)         { printf("%ld", v); }
    void print(float v)        { printf("%f", v); }
    void print(double v)       { printf("%f", v); }
    void print(bool v)         { printf("%s", v ? "true" : "false"); }
    void println(const char* s){ printf("%s\n", s ? s : "(null)"); }
    void println(int v)        { printf("%d\n", v); }
    void println(long v)       { printf("%ld\n", v); }
    void println(float v)      { printf("%f\n", v); }
    void println(double v)     { printf("%f\n", v); }
    void println()             { printf("\n"); }
    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    void flush() {}
};

extern SerialClass Serial;

// Timing stubs — not needed for pure logic tests
inline uint32_t millis() { return 0; }
inline uint32_t micros() { return 0; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}

// Math helpers already in <math.h>; these forward to it
using ::fabs;
using ::sqrt;
using ::abs;
