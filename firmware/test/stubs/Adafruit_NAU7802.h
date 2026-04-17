#pragma once
#include <stdint.h>

// Minimal stub so weight.h / weight.cpp compile on the host.
// No method bodies needed — weightMeasureStable() is not called in native tests.
class Adafruit_NAU7802 {
public:
    bool begin()      { return false; }
    bool available()  { return false; }
    int32_t read()    { return 0; }
    void setGain(uint8_t) {}
    void setSampleRate(uint8_t) {}
    void calibrate()  {}
};
