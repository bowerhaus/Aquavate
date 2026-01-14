/**
 * Aquavate - Smart Water Bottle Firmware
 * Main entry point
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include "aquavate.h"
#include "config.h"

// Calibration system includes
#include "gestures.h"
#include "weight.h"
#include "storage.h"
#include "calibration.h"
#include "ui_calibration.h"

// Serial commands for time setting
#include "serial_commands.h"
#include <sys/time.h>
#include <time.h>

// Daily intake tracking
#include "drinks.h"

// FIX Bug #4: Sensor snapshot to prevent multiple reads per loop
struct SensorSnapshot {
    uint32_t timestamp;
    int32_t adc_reading;
    float water_ml;
    GestureType gesture;
};

Adafruit_NAU7802 nau;
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
bool nauReady = false;
bool lisReady = false;

unsigned long wakeTime = 0;

// Water drop icon bitmap (60x60 pixels)
// Created to match SF Symbols "drop.fill" style
const unsigned char PROGMEM water_drop_bitmap[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xFF, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xFF, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00
};

#define WATER_DROP_WIDTH 60
#define WATER_DROP_HEIGHT 60

// Human figure bitmap (50x83 pixels)
const unsigned char PROGMEM human_figure_bitmap[] = {
    0x00, 0x00, 0x03, 0xB0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0E, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3A, 0x0F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x50, 0x03, 0x80, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0x00, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x00, 0xC0, 0x00, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x00, 0x30, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x30, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x30, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x30, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x30, 0x00, 0x00,
    0x00, 0x01, 0x80, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x01, 0x80, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0x01, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x70, 0x03, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x3C, 0x0D, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x17, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0xD0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x91, 0x20, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFA, 0x00, 0x00,
    0x00, 0x1E, 0xFD, 0x55, 0x5E, 0x00, 0x00,
    0x00, 0x70, 0x00, 0x00, 0x05, 0x80, 0x00,
    0x00, 0xD0, 0x00, 0x00, 0x01, 0xC0, 0x00,
    0x00, 0xC0, 0x00, 0x00, 0x00, 0xE0, 0x00,
    0x01, 0x80, 0x00, 0x00, 0x00, 0x50, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00,
    0x0C, 0x06, 0x00, 0x00, 0x18, 0x0C, 0x00,
    0x04, 0x06, 0x00, 0x00, 0x18, 0x0C, 0x00,
    0x0C, 0x06, 0x00, 0x00, 0x18, 0x0C, 0x00,
    0x0C, 0x0D, 0x00, 0x00, 0x1C, 0x04, 0x00,
    0x18, 0x0E, 0x00, 0x00, 0x14, 0x0E, 0x00,
    0x18, 0x1A, 0x00, 0x00, 0x1E, 0x04, 0x00,
    0x10, 0x16, 0x00, 0x00, 0x1A, 0x07, 0x00,
    0x18, 0x1E, 0x00, 0x00, 0x17, 0x02, 0x00,
    0x30, 0x36, 0x00, 0x00, 0x1D, 0x03, 0x00,
    0x30, 0x34, 0x00, 0x00, 0x13, 0x03, 0x00,
    0x20, 0x66, 0x00, 0x00, 0x1B, 0x81, 0x80,
    0x70, 0x66, 0x00, 0x00, 0x19, 0x03, 0x00,
    0x40, 0x66, 0x00, 0x00, 0x09, 0xC1, 0x80,
    0x60, 0xC4, 0x00, 0x00, 0x18, 0x80, 0x80,
    0x60, 0xC6, 0x00, 0x00, 0x18, 0xE1, 0xC0,
    0xC1, 0x86, 0x00, 0x00, 0x08, 0x40, 0x80,
    0x41, 0x86, 0x00, 0x00, 0x1C, 0x60, 0xC0,
    0xE2, 0x86, 0x00, 0x00, 0x08, 0x71, 0x80,
    0x5B, 0x0A, 0x00, 0x00, 0x18, 0x2D, 0x80,
    0x3E, 0x06, 0x00, 0x00, 0x0C, 0x1F, 0x00,
    0x00, 0x06, 0x00, 0x00, 0x18, 0x04, 0x00,
    0x00, 0x0C, 0x00, 0xC0, 0x08, 0x00, 0x00,
    0x00, 0x06, 0x00, 0xC0, 0x1C, 0x00, 0x00,
    0x00, 0x04, 0x00, 0xC0, 0x08, 0x00, 0x00,
    0x00, 0x0E, 0x01, 0xA0, 0x0C, 0x00, 0x00,
    0x00, 0x04, 0x00, 0xE0, 0x18, 0x00, 0x00,
    0x00, 0x0E, 0x01, 0xA0, 0x0C, 0x00, 0x00,
    0x00, 0x04, 0x01, 0xE0, 0x08, 0x00, 0x00,
    0x00, 0x0C, 0x01, 0x60, 0x1C, 0x00, 0x00,
    0x00, 0x06, 0x03, 0x30, 0x08, 0x00, 0x00,
    0x00, 0x0C, 0x01, 0xB0, 0x0C, 0x00, 0x00,
    0x00, 0x04, 0x03, 0x20, 0x18, 0x00, 0x00,
    0x00, 0x0E, 0x03, 0x30, 0x0C, 0x00, 0x00,
    0x00, 0x04, 0x02, 0x18, 0x08, 0x00, 0x00,
    0x00, 0x0C, 0x07, 0x18, 0x0C, 0x00, 0x00,
    0x00, 0x06, 0x04, 0x18, 0x0C, 0x00, 0x00,
    0x00, 0x0C, 0x06, 0x18, 0x0C, 0x00, 0x00,
    0x00, 0x0C, 0x06, 0x08, 0x0C, 0x00, 0x00,
    0x00, 0x04, 0x0A, 0x1C, 0x0C, 0x00, 0x00,
    0x00, 0x0C, 0x04, 0x08, 0x0C, 0x00, 0x00,
    0x00, 0x0C, 0x0C, 0x0C, 0x08, 0x00, 0x00,
    0x00, 0x04, 0x0C, 0x0C, 0x0C, 0x00, 0x00,
    0x00, 0x0C, 0x0C, 0x06, 0x0C, 0x00, 0x00,
    0x00, 0x06, 0x18, 0x0C, 0x18, 0x00, 0x00,
    0x00, 0x07, 0x68, 0x07, 0x58, 0x00, 0x00,
    0x00, 0x03, 0xB0, 0x03, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0x00, 0xA0, 0x00, 0x00,
};

#define HUMAN_FIGURE_WIDTH 50
#define HUMAN_FIGURE_HEIGHT 83

// Human figure FILLED bitmap (50x83 pixels) - flood filled
const unsigned char PROGMEM human_figure_filled_bitmap[] = {
    0x00, 0x00, 0x03, 0xB0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xFC, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5F, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x01, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x3F, 0xFD, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x17, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0xD0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x91, 0x20, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFA, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFE, 0x00, 0x00,
    0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0x00,
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xD0, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x00,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x00,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0x00,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x0F, 0xFD, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x1F, 0xFF, 0xFF, 0xFF, 0xF7, 0xFE, 0x00,
    0x1F, 0xFB, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    0x1F, 0xF7, 0xFF, 0xFF, 0xFB, 0xFF, 0x00,
    0x1F, 0xFF, 0xFF, 0xFF, 0xF7, 0xFE, 0x00,
    0x3F, 0xF7, 0xFF, 0xFF, 0xFD, 0xFF, 0x00,
    0x3F, 0xF7, 0xFF, 0xFF, 0xF3, 0xFF, 0x00,
    0x3F, 0xE7, 0xFF, 0xFF, 0xFB, 0xFF, 0x80,
    0x7F, 0xE7, 0xFF, 0xFF, 0xF9, 0xFF, 0x00,
    0x7F, 0xE7, 0xFF, 0xFF, 0xF9, 0xFF, 0x80,
    0x7F, 0xC7, 0xFF, 0xFF, 0xF8, 0xFF, 0x80,
    0x7F, 0xC7, 0xFF, 0xFF, 0xF8, 0xFF, 0xC0,
    0xFF, 0x87, 0xFF, 0xFF, 0xF8, 0x7F, 0x80,
    0x7F, 0x87, 0xFF, 0xFF, 0xFC, 0x7F, 0xC0,
    0xFE, 0x87, 0xFF, 0xFF, 0xF8, 0x7F, 0x80,
    0x5F, 0x0B, 0xFF, 0xFF, 0xF8, 0x2F, 0x80,
    0x3E, 0x07, 0xFF, 0xFF, 0xFC, 0x1F, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xF8, 0x04, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xBF, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xBF, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0x7F, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0x3F, 0xF8, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0xBF, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFF, 0x3F, 0xF8, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0x3F, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFE, 0x1F, 0xF8, 0x00, 0x00,
    0x00, 0x0F, 0xFF, 0x1F, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFC, 0x1F, 0xFC, 0x00, 0x00,
    0x00, 0x0F, 0xFE, 0x1F, 0xFC, 0x00, 0x00,
    0x00, 0x0F, 0xFE, 0x0F, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xFA, 0x1F, 0xFC, 0x00, 0x00,
    0x00, 0x0F, 0xFC, 0x0F, 0xFC, 0x00, 0x00,
    0x00, 0x0F, 0xFC, 0x0F, 0xF8, 0x00, 0x00,
    0x00, 0x07, 0xFC, 0x0F, 0xFC, 0x00, 0x00,
    0x00, 0x0F, 0xFC, 0x07, 0xFC, 0x00, 0x00,
    0x00, 0x07, 0xF8, 0x0F, 0xF8, 0x00, 0x00,
    0x00, 0x07, 0xE8, 0x07, 0xF8, 0x00, 0x00,
    0x00, 0x03, 0xB0, 0x03, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0x00, 0xA0, 0x00, 0x00,
};

// Calibration state
CalibrationData g_calibration;
bool g_calibrated = false;
CalibrationState g_last_cal_state = CAL_IDLE;

// Display update tracking
float g_last_displayed_water_ml = -1.0f;  // -1 means not yet displayed

// Time state
int8_t g_timezone_offset = 0;  // UTC offset in hours
bool g_time_valid = false;     // Has time been set?

#if defined(BOARD_ADAFRUIT_FEATHER)
#include "Adafruit_ThinkInk.h"

// 2.13" Mono E-Paper display (GDEY0213B74 variant - no 8-pixel shift)
ThinkInk_213_Mono_GDEY0213B74 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

float getBatteryVoltage() {
    float voltage = analogReadMilliVolts(VBAT_PIN);
    voltage *= 2;     // Voltage divider, multiply back
    voltage /= 1000;  // Convert to volts
    return voltage;
}

int getBatteryPercent(float voltage) {
    // LiPo: BATTERY_VOLTAGE_FULL = 100%, BATTERY_VOLTAGE_EMPTY = 0%
    int percent = (voltage - BATTERY_VOLTAGE_EMPTY) / (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100;
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;
    return percent;
}

void drawBatteryIcon(int x, int y, int percent) {
    // Battery outline (20x12 pixels)
    display.drawRect(x, y, 20, 12, EPD_BLACK);
    display.fillRect(x + 20, y + 3, 3, 6, EPD_BLACK);  // Positive terminal

    // Fill level (max 16px wide inside)
    int fillWidth = (percent * 16) / 100;
    if (fillWidth > 0) {
        display.fillRect(x + 2, y + 2, fillWidth, 8, EPD_BLACK);
    }
}

void drawBottleGraphic(int x, int y, float fill_percent) {
    // Bottle dimensions
    int bottle_width = 40;
    int bottle_height = 90;
    int bottle_body_height = 70;  // Main body height
    int neck_height = 10;
    int cap_height = 10;

    // Calculate fill level (0-100%)
    int fill_height = (int)(bottle_body_height * fill_percent);

    // Draw bottle body (vertical cylinder)
    display.fillRoundRect(x, y + cap_height + neck_height,
                         bottle_width, bottle_body_height, 8, EPD_BLACK);
    display.fillRoundRect(x + 2, y + cap_height + neck_height + 2,
                         bottle_width - 4, bottle_body_height - 4, 6, EPD_WHITE);

    // Draw neck (narrower at top)
    int neck_width = bottle_width - 12;
    int neck_x = x + 6;
    display.fillRect(neck_x, y + cap_height, neck_width, neck_height, EPD_BLACK);
    display.fillRect(neck_x + 2, y + cap_height + 2, neck_width - 4, neck_height - 4, EPD_WHITE);

    // Draw cap
    int cap_width = neck_width - 4;
    int cap_x = neck_x + 2;
    display.fillRect(cap_x, y, cap_width, cap_height, EPD_BLACK);

    // Draw water level inside bottle (fill from bottom)
    if (fill_height > 0) {
        int water_y = y + cap_height + neck_height + bottle_body_height - fill_height;
        display.fillRoundRect(x + 4, water_y,
                             bottle_width - 8, fill_height - 2, 4, EPD_BLACK);
    }
}
#endif

// Get day name from weekday number (0=Sunday, 1=Monday, etc.)
const char* getDayName(int weekday) {
    switch (weekday) {
        case 0: return "Sun";
        case 1: return "Mon";
        case 2: return "Tue";
        case 3: return "Wed";
        case 4: return "Thu";
        case 5: return "Fri";
        case 6: return "Sat";
        default: return "---";
    }
}

// Format time for display as "Wed 2pm" (day + 12-hour format)
// Returns "--- --" if time not valid
void formatTimeForDisplay(char* buffer, size_t buffer_size) {
    if (!g_time_valid) {
        snprintf(buffer, buffer_size, "--- --");
        return;
    }

    // Get current time from RTC
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;

    // Convert to local time using timezone offset
    now += g_timezone_offset * 3600;  // Add timezone offset in seconds
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // Get day name
    const char* day = getDayName(timeinfo.tm_wday);

    // Convert to 12-hour format
    int hour_12 = timeinfo.tm_hour % 12;
    if (hour_12 == 0) hour_12 = 12;  // 0 or 12 should display as 12
    const char* am_pm = (timeinfo.tm_hour < 12) ? "am" : "pm";

    // Format as "Wed 2pm"
    snprintf(buffer, buffer_size, "%s %d%s", day, hour_12, am_pm);
}

// Callback when time is set via serial command
void onTimeSet() {
    g_time_valid = true;
    Serial.println("Main: Time set callback - time is now valid");
}

void writeAccelReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(0x18);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readAccelReg(uint8_t reg) {
    Wire.beginTransmission(0x18);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)0x18, (uint8_t)1);
    return Wire.read();
}

void configureLIS3DHInterrupt() {
    // Configure LIS3DH to wake when Z-axis < threshold (tilted >80° from vertical)

    // Configure interrupt pin with pulldown (INT is push-pull active high)
    pinMode(LIS3DH_INT_PIN, INPUT_PULLDOWN);

    // CTRL_REG1: 10Hz ODR, normal mode, all axes enabled
    writeAccelReg(LIS3DH_REG_CTRL1, 0x27);

    // CTRL_REG2: No filters
    writeAccelReg(LIS3DH_REG_CTRL2, 0x00);

    // CTRL_REG3: AOI1 interrupt on INT1 pin
    writeAccelReg(LIS3DH_REG_CTRL3, 0x40);

    // CTRL_REG4: ±2g full scale
    writeAccelReg(LIS3DH_REG_CTRL4, 0x00);

    // CTRL_REG5: Latch interrupt on INT1
    writeAccelReg(LIS3DH_REG_CTRL5, 0x08);

    // CTRL_REG6: INT1 active high
    writeAccelReg(LIS3DH_REG_CTRL6, 0x00);

    // INT1_THS: Threshold for tilt detection
    // See config.h for calibration details
    writeAccelReg(LIS3DH_REG_INT1THS, LIS3DH_TILT_WAKE_THRESHOLD);

    // INT1_DURATION: 0 (immediate)
    writeAccelReg(LIS3DH_REG_INT1DUR, 0x00);

    // INT1_CFG: Trigger on Z-low event only (bit 1)
    // 0x02 = ZLIE (Z Low Interrupt Enable)
    writeAccelReg(LIS3DH_REG_INT1CFG, 0x02);

    delay(50);

    // Clear any pending interrupt
    readAccelReg(LIS3DH_REG_INT1SRC);

    Serial.print("INT pin state: ");
    Serial.println(digitalRead(LIS3DH_INT_PIN));
    Serial.println("LIS3DH configured: wake when Z < ~4000 (tilt >75°)");
}

void enterDeepSleep() {
    Serial.println("Entering deep sleep...");
    Serial.flush();

    // Configure wake-up interrupt from LIS3DH INT1 pin
    esp_sleep_enable_ext0_wakeup((gpio_num_t)LIS3DH_INT_PIN, 1);  // Wake on HIGH

    // Enter deep sleep
    esp_deep_sleep_start();
}

void blinkLED(int durationSeconds) {
    Serial.print("Blinking LED for ");
    Serial.print(durationSeconds);
    Serial.println(" seconds...");

    unsigned long endTime = millis() + (durationSeconds * 1000);
    while (millis() < endTime) {
        digitalWrite(PIN_LED, HIGH);
        delay(500);
        digitalWrite(PIN_LED, LOW);
        delay(500);
    }
    Serial.println("LED blink complete!");
}

#if defined(BOARD_ADAFRUIT_FEATHER)
/**
 * Draw human figure with fill from bottom to top
 * Uses two bitmaps: outline (top) and filled (bottom)
 * @param x X position (top-left corner)
 * @param y Y position (top-left corner)
 * @param fill_percent Fill percentage (0.0 to 1.0)
 */
void drawHumanFigure(int x, int y, float fill_percent) {
    // Calculate where to transition from outline to filled
    int fill_start_row = (int)(HUMAN_FIGURE_HEIGHT * (1.0f - fill_percent));

    // Draw row by row, switching between outline and filled bitmaps
    int bytes_per_row = (HUMAN_FIGURE_WIDTH + 7) / 8;  // 7 bytes per row

    for (int row = 0; row < HUMAN_FIGURE_HEIGHT; row++) {
        int byte_offset = row * bytes_per_row;

        // Choose bitmap based on position
        const unsigned char* bitmap_to_use;
        if (row >= fill_start_row) {
            // Bottom part - use filled bitmap
            bitmap_to_use = human_figure_filled_bitmap;
        } else {
            // Top part - use outline bitmap
            bitmap_to_use = human_figure_bitmap;
        }

        // Draw this row pixel by pixel
        for (int col = 0; col < HUMAN_FIGURE_WIDTH; col++) {
            int byte_index = byte_offset + (col / 8);
            int bit_index = 7 - (col % 8);

            // Read bit from PROGMEM
            uint8_t byte_val = pgm_read_byte(&bitmap_to_use[byte_index]);
            bool pixel_set = (byte_val & (1 << bit_index)) != 0;

            if (pixel_set) {
                display.drawPixel(x + col, y + row, EPD_BLACK);
            }
        }
    }
}

/**
 * Draw a 5 high x 2 wide grid of glass icons that fill as daily intake increases
 * @param x Top-left X position
 * @param y Top-left Y position
 * @param fill_percent Fill percentage (0.0 to 1.0)
 */
void drawGlassGrid(int x, int y, float fill_percent) {
    const int GLASS_WIDTH = 18;
    const int GLASS_HEIGHT = 16;
    const int GLASS_SPACING_X = 4;
    const int GLASS_SPACING_Y = 2;
    const int COLS = 2;
    const int ROWS = 5;
    const int TOTAL_GLASSES = COLS * ROWS;  // 10 glasses

    // Calculate total fill amount (can be fractional)
    float total_fill = fill_percent * TOTAL_GLASSES;
    if (total_fill > TOTAL_GLASSES) total_fill = TOTAL_GLASSES;

    int glass_index = 0;

    // Draw glasses from bottom to top, left to right within each row
    for (int row = ROWS - 1; row >= 0; row--) {
        for (int col = 0; col < COLS; col++) {
            int glass_x = x + col * (GLASS_WIDTH + GLASS_SPACING_X);
            int glass_y = y + row * (GLASS_HEIGHT + GLASS_SPACING_Y);

            // Calculate fill level for this glass (0.0 = empty, 1.0 = full)
            float glass_fill = 0.0f;
            if (total_fill >= glass_index + 1) {
                glass_fill = 1.0f;  // Fully filled
            } else if (total_fill > glass_index) {
                glass_fill = total_fill - glass_index;  // Partially filled
            }

            // Draw tumbler shape (wider at top/rim, narrower at bottom)
            // Left edge - slopes inward from top to bottom
            display.drawLine(glass_x, glass_y, glass_x + 3, glass_y + GLASS_HEIGHT - 1, EPD_BLACK);
            // Right edge - slopes inward from top to bottom
            display.drawLine(glass_x + GLASS_WIDTH - 1, glass_y, glass_x + GLASS_WIDTH - 4, glass_y + GLASS_HEIGHT - 1, EPD_BLACK);
            // Top edge (wide rim)
            display.drawLine(glass_x, glass_y, glass_x + GLASS_WIDTH - 1, glass_y, EPD_BLACK);
            // Bottom edge (narrow)
            display.drawLine(glass_x + 3, glass_y + GLASS_HEIGHT - 1, glass_x + GLASS_WIDTH - 4, glass_y + GLASS_HEIGHT - 1, EPD_BLACK);

            // Fill if this glass has any fill
            if (glass_fill > 0.0f) {
                // Calculate fill height (fills from bottom up)
                int fill_height = (int)((GLASS_HEIGHT - 2) * glass_fill);
                int fill_start_row = GLASS_HEIGHT - 1 - fill_height;

                // Fill the interior with tapered shape matching the tumbler
                for (int i = fill_start_row; i < GLASS_HEIGHT - 1; i++) {
                    // Calculate width at this row (wider at top, narrower at bottom)
                    float ratio = (float)i / (float)(GLASS_HEIGHT - 1);
                    int top_inset = 1;  // Inset at top (wider)
                    int bottom_inset = 4;  // Inset at bottom (narrower)
                    int current_inset = top_inset + (int)(ratio * (bottom_inset - top_inset));

                    int line_start = glass_x + current_inset;
                    int line_end = glass_x + GLASS_WIDTH - 1 - current_inset;
                    display.drawLine(line_start, glass_y + i, line_end, glass_y + i, EPD_BLACK);
                }
            }

            glass_index++;
        }
    }
}

void drawWelcomeScreen() {
    Serial.println("Drawing welcome screen...");
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);

    // Show Aquavate branding (left side)
    display.setTextSize(3);
    display.setCursor(20, 50);
    display.print("Aquavate");

    // Draw water drop bitmap to the right (50x60 pixels)
    // Position: right side of screen, vertically centered
    int drop_x = 180;
    int drop_y = 30;
    display.drawBitmap(drop_x, drop_y, water_drop_bitmap,
                      WATER_DROP_WIDTH, WATER_DROP_HEIGHT, EPD_BLACK);

    display.display();
}

void drawMainScreen() {
    Serial.println("Drawing main screen...");
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);

    // Calculate current water level in ml
    float water_ml = 0.0f;
    if (g_calibrated && nauReady) {
        // Wait for NAU7802 to have data available (may take a moment)
        int retry_count = 0;
        while (!nau.available() && retry_count < 10) {
            delay(10);
            retry_count++;
        }

        if (nau.available()) {
            int32_t current_adc = nau.read();
            water_ml = calibrationGetWaterWeight(current_adc, g_calibration);

            Serial.print("Main Screen: ADC=");
            Serial.print(current_adc);
            Serial.print(" Water=");
            Serial.print(water_ml, 1);
            Serial.print("ml (Scale factor=");
            Serial.print(g_calibration.scale_factor, 2);
            Serial.println(")");

            // Clamp to 0-830ml range
            if (water_ml < 0) water_ml = 0;
            if (water_ml > 830) water_ml = 830;
        } else {
            // NAU7802 not ready - show error
            Serial.println("Main Screen: ERROR - NAU7802 not available!");
            water_ml = 0.0f;
        }
    } else {
        // Not calibrated - show 830ml as placeholder
        water_ml = 830.0f;
        Serial.println("Main Screen: Not calibrated - showing 830ml");
    }

    // Draw vertical bottle graphic on left side
    int bottle_x = 10;  // Left side of screen
    int bottle_y = 20;   // Top margin
    float fill_percent = water_ml / 830.0f;
    drawBottleGraphic(bottle_x, bottle_y, fill_percent);

    // Draw large text showing daily intake (right side)
    display.setTextSize(3);

    // Get daily intake total
    uint16_t daily_total_ml = 0;
    if (g_time_valid) {
        DailyState daily_state;
        drinksGetState(daily_state);
        daily_total_ml = daily_state.daily_total_ml;
    }

    char intake_text[16];
    snprintf(intake_text, sizeof(intake_text), "%dml", daily_total_ml);

    // Calculate text width for centering (size 3: 18 pixels per char)
    int intake_text_width = strlen(intake_text) * 18;
    // Center between bottle (ends at ~60px) and visualization (starts at ~185px)
    int available_width = 185 - 60;  // ~125 pixels available
    int intake_x = 60 + (available_width - intake_text_width) / 2;

    // Position text centered between bottle and visualization
    display.setCursor(intake_x, 50);
    display.print(intake_text);

    // Draw "today" label below in size 2 (12 pixels per char)
    display.setTextSize(2);
    int today_width = 5 * 12;  // "today" = 5 chars
    int today_x = 60 + (available_width - today_width) / 2;
    display.setCursor(today_x, 75);
    display.print("today");

    // Draw battery status in top-right corner
    float batteryV = getBatteryVoltage();
    int batteryPct = getBatteryPercent(batteryV);
    drawBatteryIcon(220, 5, batteryPct);

    // Draw time centered at top
    char time_text[16];
    formatTimeForDisplay(time_text, sizeof(time_text));
    display.setTextSize(1);

    // Calculate width of text for centering (size 1 = 6 pixels per char)
    int text_width = strlen(time_text) * 6;  // Approximate width
    int center_x = (250 - text_width) / 2;  // Display width is 250 pixels

    display.setCursor(center_x, 5);
    display.print(time_text);

    // Draw daily intake visualization (if time is valid)
    float daily_fill = 0.0f;
    if (g_time_valid) {
        // Get current daily state
        DailyState daily_state;
        drinksGetState(daily_state);

        // Calculate fill percentage based on daily goal
        daily_fill = (float)daily_state.daily_total_ml / (float)DRINK_DAILY_GOAL_ML;
        if (daily_fill > 1.0f) daily_fill = 1.0f;  // Cap at 100%
    }

    #if DAILY_INTAKE_DISPLAY_MODE == 0
        // Human figure mode: 50x83px figure on right side
        int figure_x = 185;  // Right side (250 - 50 - 15 margin)
        int figure_y = 20;   // Top margin
        drawHumanFigure(figure_x, figure_y, daily_fill);
    #else
        // Tumbler grid mode: 44x86px grid (2 cols x 5 rows) on right side
        int grid_x = 195;    // Right side (250 - 44 - 11 margin)
        int grid_y = 20;     // Top margin
        drawGlassGrid(grid_x, grid_y, daily_fill);
    #endif

    display.display();
}
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);

    wakeTime = millis();

    // Initialize LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Print wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.println("=================================");
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Woke up from tilt/motion!");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Woke up from timer");
            break;
        default:
            Serial.println("Normal boot (power on/reset)");
            break;
    }
    Serial.println("=================================");

    Serial.print("Aquavate v");
    Serial.print(AQUAVATE_VERSION_MAJOR);
    Serial.print(".");
    Serial.print(AQUAVATE_VERSION_MINOR);
    Serial.print(".");
    Serial.println(AQUAVATE_VERSION_PATCH);
    Serial.println("=================================");

#if defined(BOARD_ADAFRUIT_FEATHER)
    Serial.println("Board: Adafruit ESP32 Feather V2");
    Serial.println("Display: 2.13\" E-Paper FeatherWing");
#elif defined(BOARD_SPARKFUN_QWIIC)
    Serial.println("Board: SparkFun ESP32-C6 Qwiic Pocket");
    Serial.println("Display: Waveshare 1.54\" E-Paper");
#endif

    Serial.println();

    // Initialize E-Paper display
#if defined(BOARD_ADAFRUIT_FEATHER)
    Serial.println("Initializing E-Paper display...");
    display.begin();
    display.setRotation(2);  // Rotate 180 degrees

    // Draw welcome screen on first boot
    drawWelcomeScreen();

    // Print battery info
    float batteryV = getBatteryVoltage();
    int batteryPct = getBatteryPercent(batteryV);
    Serial.print("Battery: ");
    Serial.print(batteryV);
    Serial.print("V (");
    Serial.print(batteryPct);
    Serial.println("%)");

    Serial.println("E-Paper display initialized!");
#endif

    Serial.println("Initializing...");

    // Initialize I2C and NAU7802
    Wire.begin();
    Serial.println("Initializing NAU7802...");
    if (nau.begin()) {
        Serial.println("NAU7802 found!");
        nau.setLDO(NAU7802_3V3);
        nau.setGain(NAU7802_GAIN_128);
        nau.setRate(NAU7802_RATE_10SPS);
        nauReady = true;
    } else {
        Serial.println("NAU7802 not found!");
    }

    // Initialize LIS3DH accelerometer
    Serial.println("Initializing LIS3DH...");
    if (lis.begin(0x18)) {  // Default I2C address
        Serial.println("LIS3DH found!");
        lis.setRange(LIS3DH_RANGE_2_G);
        lis.setDataRate(LIS3DH_DATARATE_10_HZ);
        lisReady = true;

        // Configure interrupt for wake-on-tilt
        configureLIS3DHInterrupt();
    } else {
        Serial.println("LIS3DH not found!");
    }

    // Initialize calibration system
    Serial.println("Initializing calibration system...");

    // Initialize storage (NVS)
    if (storageInit()) {
        Serial.println("Storage initialized");

        // Load calibration from NVS
        g_calibrated = storageLoadCalibration(g_calibration);

        if (g_calibrated) {
            Serial.println("Calibration loaded from NVS:");
            Serial.print("  Scale factor: ");
            Serial.print(g_calibration.scale_factor);
            Serial.println(" ADC/g");
            Serial.print("  Empty ADC: ");
            Serial.println(g_calibration.empty_bottle_adc);
            Serial.print("  Full ADC: ");
            Serial.println(g_calibration.full_bottle_adc);
        } else {
            Serial.println("No valid calibration found - calibration required");
        }

        // Load timezone and time_valid from NVS
        g_timezone_offset = storageLoadTimezone();
        g_time_valid = storageLoadTimeValid();

        if (g_time_valid) {
            Serial.println("Time configuration loaded:");
            Serial.print("  Timezone offset: ");
            Serial.println(g_timezone_offset);

            // Show current time
            char time_str[64];
            formatTimeForDisplay(time_str, sizeof(time_str));
            Serial.print("  Current time: ");
            Serial.println(time_str);
        } else {
            Serial.println("WARNING: Time not set!");
            Serial.println("Use SET_TIME command to set time");
            Serial.println("Example: SET_TIME 2026-01-13 14:30:00 -5");
        }
    } else {
        Serial.println("Storage initialization failed");
    }

    // Initialize serial command handler
    serialCommandsInit();
    serialCommandsSetTimeCallback(onTimeSet);
    Serial.println("Serial command handler initialized");

    // Initialize gesture detection
    if (lisReady) {
        gesturesInit(lis);
        Serial.println("Gesture detection initialized");
    }

    // Initialize weight measurement
    if (nauReady) {
        weightInit(nau);
        Serial.println("Weight measurement initialized");
    }

    // Initialize calibration state machine
    calibrationInit();
    Serial.println("Calibration state machine initialized");

    // Initialize calibration UI
#if defined(BOARD_ADAFRUIT_FEATHER)
    uiCalibrationInit(display);
    Serial.println("Calibration UI initialized");

    // Show calibration status on display if not calibrated
    if (!g_calibrated) {
        display.clearBuffer();
        display.setTextSize(2);
        display.setTextColor(EPD_BLACK);
        display.setCursor(20, 30);
        display.print("Calibration");
        display.setCursor(40, 55);
        display.print("Required");
        display.setTextSize(1);
        display.setCursor(10, 85);
        display.print("Hold bottle inverted");
        display.setCursor(10, 100);
        display.print("for 5 seconds");
        display.display();
    }
#endif

    // Initialize drink tracking system (only if time is valid)
    if (g_time_valid) {
        drinksInit();
        Serial.println("Drink tracking system initialized");
    } else {
        Serial.println("WARNING: Drink tracking not initialized - time not set");
    }

    Serial.println("Setup complete!");
    Serial.print("Will sleep in ");
    Serial.print(AWAKE_DURATION_MS / 1000);
    Serial.println(" seconds...");
}

void loop() {
    // Check for serial commands
    serialCommandsUpdate();

    // FIX Bug #4: READ SENSORS ONCE - create snapshot for this loop iteration
    SensorSnapshot sensors;
    sensors.timestamp = millis();
    sensors.adc_reading = 0;
    sensors.water_ml = 0.0f;
    sensors.gesture = GESTURE_NONE;

    // Read load cell
    if (nauReady && nau.available()) {
        sensors.adc_reading = nau.read();
        if (g_calibrated) {
            sensors.water_ml = calibrationGetWaterWeight(sensors.adc_reading, g_calibration);
        }
    }

    // Read accelerometer and get gesture (ONCE)
    if (lisReady) {
        sensors.gesture = gesturesUpdate(sensors.water_ml);
    }

    // NOW use snapshot for all logic below
    int32_t current_adc = sensors.adc_reading;
    float current_water_ml = sensors.water_ml;
    GestureType gesture = sensors.gesture;

    // Check calibration state
    CalibrationState cal_state = calibrationGetState();

    // If not in calibration mode, check for calibration trigger
    if (cal_state == CAL_IDLE) {
        // Check for calibration trigger gesture (hold inverted for 5s)
        if (gesture == GESTURE_INVERTED_HOLD) {
            Serial.println("Main: Calibration triggered!");
            calibrationStart();
            cal_state = calibrationGetState();
        }

        // If calibrated, show water level
        #if DEBUG_WATER_LEVEL
        if (g_calibrated && nauReady) {
            float water_ml = calibrationGetWaterWeight(current_adc, g_calibration);
            Serial.print("Water level: ");
            Serial.print(water_ml);
            Serial.println(" ml");
        }
        #endif
    }

    // If in calibration mode, update state machine
    if (calibrationIsActive()) {
        CalibrationState new_state = calibrationUpdate(gesture, current_adc);

        // Update UI when state changes
        if (new_state != g_last_cal_state) {
            Serial.print("Main: Calibration state changed: ");
            Serial.print(calibrationGetStateName(g_last_cal_state));
            Serial.print(" -> ");
            Serial.println(calibrationGetStateName(new_state));

            // Update display for new state
#if defined(BOARD_ADAFRUIT_FEATHER)
            int32_t display_adc = 0;
            if (new_state == CAL_CONFIRM_EMPTY) {
                CalibrationResult result = calibrationGetResult();
                display_adc = result.data.empty_bottle_adc;
            } else if (new_state == CAL_CONFIRM_FULL) {
                CalibrationResult result = calibrationGetResult();
                display_adc = result.data.full_bottle_adc;
            }

            CalibrationResult result = calibrationGetResult();
            uiCalibrationUpdateForState(new_state, display_adc, result.data.scale_factor);
#endif

            g_last_cal_state = new_state;
        }

        // Check if calibration completed
        if (new_state == CAL_COMPLETE && g_last_cal_state != CAL_COMPLETE) {
            CalibrationResult result = calibrationGetResult();
            if (result.success) {
                Serial.println("Main: Calibration completed successfully!");
                g_calibration = result.data;
                g_calibrated = true;

                // Show complete screen for 5 seconds
                delay(5000);

                // Return to IDLE and redraw main screen
                calibrationCancel();
                g_last_cal_state = CAL_IDLE;
                g_last_displayed_water_ml = -1.0f;  // Reset so first check will update display
                Serial.println("Main: Returning to main screen");
                drawMainScreen();
            }
        }

        // Check for calibration error
        if (new_state == CAL_ERROR && g_last_cal_state != CAL_ERROR) {
            CalibrationResult result = calibrationGetResult();
            Serial.print("Main: Calibration error: ");
            Serial.println(result.error_message);

            // Show error screen for 5 seconds
            delay(5000);

            // Return to IDLE and redraw main screen
            calibrationCancel();
            g_last_cal_state = CAL_IDLE;
            Serial.println("Main: Returning to main screen after error");
            drawMainScreen();
        }
    }

    // Periodically check water level and update display if changed significantly
    // Only refresh e-paper display if water level changed by >5ml to minimize flashing
    static unsigned long last_level_check = 0;

    // Check water level ONLY when bottle is upright stable (accurate readings)
    if (cal_state == CAL_IDLE && g_calibrated && gesture == GESTURE_UPRIGHT_STABLE) {
        // Check water level every DISPLAY_UPDATE_INTERVAL_MS when bottle is upright stable
        if (millis() - last_level_check >= DISPLAY_UPDATE_INTERVAL_MS) {
            last_level_check = millis();

#if defined(BOARD_ADAFRUIT_FEATHER)
            if (nauReady) {
                // FIX Bug #4: Use snapshot data instead of reading sensors again
                float display_water_ml = current_water_ml;

                // Clamp to valid range
                if (display_water_ml < 0) display_water_ml = 0;
                if (display_water_ml > 830) display_water_ml = 830;

                // Process drink tracking (we're already UPRIGHT_STABLE)
                if (g_time_valid) {
                    drinksUpdate(current_adc, g_calibration);
                }

                // Only refresh display if water level changed by more than threshold
                if (g_last_displayed_water_ml < 0 ||
                    fabs(display_water_ml - g_last_displayed_water_ml) >= DISPLAY_UPDATE_THRESHOLD_ML) {

                    #if DEBUG_DISPLAY_UPDATES
                    Serial.print("Main: Water level changed from ");
                    Serial.print(g_last_displayed_water_ml, 1);
                    Serial.print("ml to ");
                    Serial.print(display_water_ml, 1);
                    Serial.println("ml - refreshing display");
                    #endif

                    drawMainScreen();
                    g_last_displayed_water_ml = display_water_ml;
                } else {
                    #if DEBUG_DISPLAY_UPDATES
                    Serial.print("Main: Water level unchanged (");
                    Serial.print(display_water_ml, 1);
                    Serial.println("ml) - no display refresh");
                    #endif
                }
            }
#endif
        }
    }

    // Debug output (only print periodically to reduce serial spam)
    #if DEBUG_ACCELEROMETER
    static unsigned long last_debug_print = 0;
    if (lisReady && (millis() - last_debug_print >= 1000)) {
        last_debug_print = millis();

        float x, y, z;
        gesturesGetAccel(x, y, z);
        Serial.print("Accel X: ");
        Serial.print(x, 2);
        Serial.print("g  Y: ");
        Serial.print(y, 2);
        Serial.print("g  Z: ");
        Serial.print(z, 2);
        Serial.print("g  Gesture: ");

        switch (gesture) {
            case GESTURE_INVERTED_HOLD: Serial.print("INVERTED_HOLD"); break;
            case GESTURE_UPRIGHT_STABLE: Serial.print("UPRIGHT_STABLE"); break;
            case GESTURE_SIDEWAYS_TILT: Serial.print("SIDEWAYS_TILT"); break;
            default: Serial.print("NONE"); break;
        }

        Serial.print("  Cal State: ");
        Serial.print(calibrationGetStateName(cal_state));

        unsigned long remaining = (AWAKE_DURATION_MS - (millis() - wakeTime)) / 1000;
        Serial.print("  (sleep in ");
        Serial.print(remaining);
        Serial.println("s)");
    }
    #endif

    // Check if it's time to sleep
    // DISABLED FOR CALIBRATION TESTING
    /*
    if (millis() - wakeTime >= AWAKE_DURATION_MS) {
        if (lisReady) {
            // Clear any pending interrupt before sleep
            readAccelReg(LIS3DH_REG_INT1SRC);

            Serial.print("INT pin before sleep: ");
            Serial.println(digitalRead(LIS3DH_INT_PIN));
        }
        enterDeepSleep();
    }
    */

    // Reduced delay for more responsive gesture detection
    delay(200);
}
