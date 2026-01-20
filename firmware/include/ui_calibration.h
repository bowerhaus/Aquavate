/**
 * Aquavate - Calibration UI Module
 * E-paper display screens for calibration wizard
 *
 * Note: SSD1680 controller supports full refresh only (~1-2s per update)
 * No partial updates or fast refresh modes available.
 */

#ifndef UI_CALIBRATION_H
#define UI_CALIBRATION_H

#include <Arduino.h>
#include "calibration.h"
#include "config.h"

// Include display header for function declarations (needed even when calibration disabled)
#if defined(BOARD_ADAFRUIT_FEATHER)
    #include "Adafruit_ThinkInk.h"
#endif

#if ENABLE_STANDALONE_CALIBRATION

// Initialize calibration UI module
// Pass reference to e-paper display object
#if defined(BOARD_ADAFRUIT_FEATHER)
    void uiCalibrationInit(ThinkInk_213_Mono_GDEY0213B74& display);
#endif

// Show calibration start screen
// "Calibration Mode" + "Empty bottle completely"
void uiCalibrationShowStart();

// Show calibration started acknowledgment screen
// "Calibration / Started" (3x size)
void uiCalibrationShowStarted();

// Show empty bottle prompt with "?" graphic
// Empty bottle (0.0 fill) + "?" + "Empty bottle"
void uiCalibrationShowEmptyPrompt();

// Show full bottle prompt with "?" graphic
// Full bottle (1.0 fill) + "?" + "Fill to 830ml"
void uiCalibrationShowFullPrompt();

// Show measuring empty screen
// "Measuring empty..." (static during 10s measurement)
void uiCalibrationShowMeasuringEmpty();

// Show empty confirmation screen
// "Empty: XXXXX ADC" + "Tilt sideways to confirm"
void uiCalibrationShowEmptyConfirm(int32_t adc);

// Show fill bottle screen
// "Fill to 830ml line" + "Hold still when ready"
void uiCalibrationShowFillBottle();

// Show measuring full screen
// "Measuring full..." (static during 10s measurement)
void uiCalibrationShowMeasuringFull();

// Show full confirmation screen
// "Full: XXXXX ADC" + "Tilt sideways to confirm"
void uiCalibrationShowFullConfirm(int32_t adc);

// Show calibration complete screen
// "Complete!" + "Scale: XX.X ct/g"
void uiCalibrationShowComplete(float scale_factor);

// Show calibration error screen
// "Calibration / Error" (3x size, matches Started screen)
void uiCalibrationShowError(const char* message);

// Show calibration aborted screen
// "Calibration / Aborted" (3x size, matches Started screen)
void uiCalibrationShowAborted();

// Update display for current calibration state
// Automatically selects appropriate screen based on state
void uiCalibrationUpdateForState(CalibrationState state, int32_t adc_value, float scale_factor);

#endif // ENABLE_STANDALONE_CALIBRATION

// Bottle emptied confirmation screen (always available - not tied to calibration mode)
// Shows "Bottle / Emptied" in calibration screen style
#if defined(BOARD_ADAFRUIT_FEATHER)
    void uiShowBottleEmptied(ThinkInk_213_Mono_GDEY0213B74& display);
#endif

#endif // UI_CALIBRATION_H
