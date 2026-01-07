# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Aquavate is a smart water bottle project that measures daily water intake using a weight-based sensor system. The device communicates with an iOS app via Bluetooth Low Energy (BLE) and displays status on an e-paper display.

**Current Status:** Hardware planning phase - no firmware or app code yet.

## Hardware Options

Two prototype configurations are being evaluated:

### Option A: Adafruit Feather Ecosystem (Recommended for simplicity)
- Adafruit ESP32 Feather V2 (STEMMA QT)
- 2.13" Mono E-Paper FeatherWing (stacks directly - no wiring)
- NAU7802 24-bit ADC for load cell
- LIS3DH accelerometer for wake-on-tilt
- UK BOM: Plans/002-bom-adafruit-feather.md

### Option B: SparkFun Qwiic Ecosystem
- SparkFun ESP32-C6 Qwiic Pocket (smaller, WiFi 6)
- Waveshare 1.54" e-paper (requires SPI wiring)
- SparkFun Qwiic Scale NAU7802
- UK BOM: Plans/003-bom-sparkfun-qwiic.md

## Planned Technology Stack

### Firmware (ESP32)
- **Framework:** PlatformIO + Arduino
- **Language:** C++
- **Key Libraries:** NimBLE-Arduino, Adafruit EPD/GxEPD2, NAU7802, LIS3DH

### iOS App
- **UI:** SwiftUI
- **BLE:** CoreBluetooth (native)
- **Storage:** CoreData
- **Architecture:** MVVM + ObservableObject

## Key Design Decisions

- **BLE over WiFi:** Lower power, excellent iOS support, industry standard for smart bottles
- **Weight-based tracking:** Load cell measures water removed rather than flow sensors
- **Wake-on-tilt:** Accelerometer interrupt wakes MCU from deep sleep when bottle is picked up
- **E-paper display:** Near-zero power when static, readable in sunlight

## User Requirements

- Battery life: 1-2 weeks between charges
- Accuracy: ±15ml
- Display: Current level + daily total
- Deep sleep with wake-on-tilt power strategy
