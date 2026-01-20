// serial_commands.h
// Serial command interface for USB time setting and configuration
// Part of Aquavate standalone bottle operation

#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include "config.h"

#if ENABLE_SERIAL_COMMANDS

#include <Arduino.h>

// Callback function type for time set events
// Called when SET_TIME command successfully sets the RTC
typedef void (*OnTimeSetCallback)(void);

// Initialize serial command handler
// Must be called once in setup() after Serial.begin()
void serialCommandsInit();

// Update serial command handler (call in loop())
// Checks for incoming serial data and processes commands
void serialCommandsUpdate();

// Register callback for time set events
// callback: Function to call when time is successfully set
void serialCommandsSetTimeCallback(OnTimeSetCallback callback);

#endif // ENABLE_SERIAL_COMMANDS

#endif // SERIAL_COMMANDS_H
