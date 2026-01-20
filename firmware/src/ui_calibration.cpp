/**
 * Aquavate - Calibration UI Module
 * Implementation for E-Paper display
 */

#include "ui_calibration.h"
#include "display.h"
#include "config.h"

#if ENABLE_STANDALONE_CALIBRATION

#if defined(BOARD_ADAFRUIT_FEATHER)

// Static variables
static ThinkInk_213_Mono_GDEY0213B74* g_display = nullptr;
static bool g_initialized = false;

void uiCalibrationInit(ThinkInk_213_Mono_GDEY0213B74& display) {
    g_display = &display;
    g_initialized = true;
}

// Helper function to center text
static void printCentered(const char* text, int y, int textSize) {
    if (!g_display) return;

    g_display->setTextSize(textSize);
    int charWidth = textSize * 6; // Approximate character width
    int textWidth = strlen(text) * charWidth;
    int x = (250 - textWidth) / 2; // Display width is 250px

    if (x < 0) x = 5; // Left margin if text too long

    g_display->setCursor(x, y);
    g_display->print(text);
}

// Helper function to print left-aligned text
static void printLeft(const char* text, int x, int y, int textSize) {
    if (!g_display) return;

    g_display->setTextSize(textSize);
    g_display->setCursor(x, y);
    g_display->print(text);
}

void uiCalibrationShowStart() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing calibration start screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Title
    printCentered("Calibration", 20, 2);

    // Instructions
    printLeft("Empty bottle", 10, 60, 2);
    printLeft("completely", 10, 80, 2);

    g_display->display(); // Full refresh
}

void uiCalibrationShowStarted() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing calibration started screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Large "Calibration / Started" text
    printCentered("Calibration", 35, 3);
    printCentered("Started", 70, 3);

    g_display->display(); // Full refresh
}

void uiCalibrationShowEmptyPrompt() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing empty bottle prompt");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Draw empty bottle graphic with "?" (40x90px bottle)
    // Center horizontally: (250 - 40) / 2 = 105
    // Center vertically: (122 - 90) / 2 = 16
    drawBottleGraphic(105, 16, 0.0f, true);

    g_display->display(); // Full refresh
}

void uiCalibrationShowFullPrompt() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing full bottle prompt");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Draw full bottle graphic with "?" (40x90px bottle)
    // Center horizontally: (250 - 40) / 2 = 105
    // Center vertically: (122 - 90) / 2 = 16
    drawBottleGraphic(105, 16, 1.0f, true);

    g_display->display(); // Full refresh
}

void uiCalibrationShowMeasuringEmpty() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing measuring empty screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Title
    printCentered("Measuring", 30, 2);
    printCentered("Empty...", 55, 2);

    // Instructions
    printLeft("Hold still", 10, 95, 1);

    g_display->display(); // Full refresh
}

void uiCalibrationShowEmptyConfirm(int32_t adc) {
    // DEPRECATED - No longer used in calibration flow
    // Empty measurement goes directly to fill screen
    // Kept for backward compatibility
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Empty confirm deprecated - showing fill bottle instead");
    uiCalibrationShowFillBottle();
}

void uiCalibrationShowFillBottle() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing fill bottle screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Title
    printCentered("Fill Bottle", 20, 2);

    // Instructions - updated for new flow
    printLeft("Fill to 830ml", 10, 60, 2);
    printLeft("Then place", 10, 80, 2);
    printLeft("upright", 10, 100, 2);

    g_display->display(); // Full refresh
}

void uiCalibrationShowMeasuringFull() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing measuring full screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Title
    printCentered("Measuring", 30, 2);
    printCentered("Full...", 55, 2);

    // Instructions
    printLeft("Hold still", 10, 95, 1);

    g_display->display(); // Full refresh
}

void uiCalibrationShowFullConfirm(int32_t adc) {
    // DEPRECATED - No longer used in calibration flow
    // Full measurement goes directly to complete screen
    // Kept for backward compatibility
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Full confirm deprecated - showing complete instead");
    uiCalibrationShowComplete(0.0f); // Scale factor will be shown in complete state
}

void uiCalibrationShowComplete(float scale_factor) {
    (void)scale_factor;  // No longer displayed
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing calibration complete screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Large "Calibration / Complete" text (matches Started screen)
    printCentered("Calibration", 35, 3);
    printCentered("Complete", 70, 3);

    g_display->display(); // Full refresh
}

void uiCalibrationShowError(const char* message) {
    (void)message;  // No longer displayed
    if (!g_initialized || !g_display) return;

    Serial.print("UI: Showing error screen: ");
    Serial.println(message);

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Large "Calibration / Error" text (matches Started screen)
    printCentered("Calibration", 35, 3);
    printCentered("Error", 70, 3);

    g_display->display(); // Full refresh
}

void uiCalibrationShowAborted() {
    if (!g_initialized || !g_display) return;

    Serial.println("UI: Showing calibration aborted screen");

    g_display->clearBuffer();
    g_display->setTextColor(EPD_BLACK);

    // Large "Calibration / Aborted" text (matches Started screen)
    printCentered("Calibration", 35, 3);
    printCentered("Aborted", 70, 3);

    g_display->display(); // Full refresh
}

void uiCalibrationUpdateForState(CalibrationState state, int32_t adc_value, float scale_factor) {
    // Track last displayed state to avoid unnecessary refreshes
    static CalibrationState last_displayed_state = CAL_IDLE;

    // Group states that show the same screen
    CalibrationState display_state = state;
    if (state == CAL_MEASURE_EMPTY) {
        display_state = CAL_WAIT_EMPTY;  // Same screen as WAIT_EMPTY
    } else if (state == CAL_MEASURE_FULL) {
        display_state = CAL_WAIT_FULL;   // Same screen as WAIT_FULL
    }

    // Only update if the display state actually changed
    if (display_state == last_displayed_state) {
        return;  // No change, skip refresh
    }

    last_displayed_state = display_state;

    switch (state) {
        case CAL_TRIGGERED:
            // Old flow - no longer used (goes straight to CAL_STARTED)
            uiCalibrationShowStart();
            break;

        case CAL_STARTED:
            uiCalibrationShowStarted();
            break;

        case CAL_WAIT_EMPTY:
        case CAL_MEASURE_EMPTY:
            // Stay on empty bottle prompt during measurement
            uiCalibrationShowEmptyPrompt();
            break;

        case CAL_CONFIRM_EMPTY:
            // DEPRECATED - skip to fill bottle screen
            uiCalibrationShowFillBottle();
            break;

        case CAL_WAIT_FULL:
        case CAL_MEASURE_FULL:
            // Stay on full bottle prompt during measurement
            uiCalibrationShowFullPrompt();
            break;

        case CAL_CONFIRM_FULL:
            // DEPRECATED - skip to complete screen
            uiCalibrationShowComplete(scale_factor);
            break;

        case CAL_COMPLETE:
            uiCalibrationShowComplete(scale_factor);
            break;

        case CAL_ERROR:
            uiCalibrationShowError("Measurement failed");
            break;

        default:
            // No UI update for IDLE or unknown states
            break;
    }
}

#endif // BOARD_ADAFRUIT_FEATHER

#endif // ENABLE_STANDALONE_CALIBRATION
