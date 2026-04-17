#pragma once
#include <stdint.h>

// Sensor event stub (normally from Adafruit_Sensor.h)
struct sensors_vec_t { float x, y, z; };
struct sensors_event_t { sensors_vec_t acceleration; };

// Minimal stub so gestures.h / calibration.h compile on the host.
// gesturesInit() is never called from native tests.
class Adafruit_ADXL343 {
public:
    Adafruit_ADXL343(uint8_t) {}
    bool begin(uint8_t = 0x53)           { return false; }
    bool getEvent(sensors_event_t*)      { return false; }
    void setRange(uint8_t)               {}
    void setDataRate(uint8_t)            {}
    void writeRegister(uint8_t, uint8_t) {}
    uint8_t readRegister(uint8_t)        { return 0; }
};
