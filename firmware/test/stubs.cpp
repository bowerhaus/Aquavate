// stubs.cpp — link-time stub implementations for the native test build.
// Provides: Serial global, all extern globals from main.cpp, all storage/display
// function stubs, and an in-memory circular buffer that drinks tests can seed.
//
// The three source files under test are #included at the bottom of this file.
// PlatformIO native test mode only compiles test/ files, so the source files
// must be pulled in here rather than via build_src_filter.

#include "Arduino.h"
#include "storage.h"
#include "storage_drinks.h"
#include "display.h"

// ---------------------------------------------------------------------------
// Serial global (declared in Arduino.h stub)
// ---------------------------------------------------------------------------
SerialClass Serial;

// ---------------------------------------------------------------------------
// Globals normally defined in main.cpp
// ---------------------------------------------------------------------------
bool g_debug_enabled          = false;
bool g_debug_water_level      = false;
bool g_debug_accelerometer    = false;
bool g_debug_display          = false;
bool g_debug_drink_tracking   = false;
bool g_debug_calibration      = false;
bool g_debug_ble              = false;
uint8_t g_daily_intake_display_mode = 0;

// Accessed by drinks.cpp as extern
int8_t g_timezone_offset      = 0;
bool   g_time_valid            = false;
bool   g_rtc_ds3231_present    = false;

// display.h declares this as extern; the real implementation is in display.cpp
// (not compiled in the native build) so we provide a placeholder here.
const unsigned char water_drop_bitmap[] = {0};

// ---------------------------------------------------------------------------
// Display stubs
// ---------------------------------------------------------------------------
void displayNVSWarning() {}

// ---------------------------------------------------------------------------
// In-memory storage for drinks tests
// ---------------------------------------------------------------------------
static CircularBufferMetadata g_stub_meta;
static DrinkRecord             g_stub_records[30];
static DailyState              g_stub_daily_state;
static bool                    g_stub_daily_state_valid = false;

void testClearStorage() {
    memset(&g_stub_meta, 0, sizeof(g_stub_meta));
    g_stub_meta.next_record_id = 1;
    memset(g_stub_records, 0, sizeof(g_stub_records));
    memset(&g_stub_daily_state, 0, sizeof(g_stub_daily_state));
    g_stub_daily_state_valid = false;
}

void testAddDrinkRecord(uint32_t timestamp, int16_t amount_ml, uint8_t flags) {
    if (g_stub_meta.record_count >= 30) return;
    int i = g_stub_meta.record_count;
    g_stub_records[i].record_id      = g_stub_meta.next_record_id++;
    g_stub_records[i].timestamp      = timestamp;
    g_stub_records[i].amount_ml      = amount_ml;
    g_stub_records[i].bottle_level_ml = 0;
    g_stub_records[i].flags          = flags;
    g_stub_records[i].type           = (amount_ml >= 100) ? 1 : 0;
    g_stub_meta.record_count++;
}

// ---------------------------------------------------------------------------
// storage_drinks.h stubs
// ---------------------------------------------------------------------------
bool storageInitDrinkFS() { return true; }

bool storageLoadBufferMetadata(CircularBufferMetadata& meta) {
    meta = g_stub_meta;
    return true;
}

bool storageSaveBufferMetadata(const CircularBufferMetadata& meta) {
    g_stub_meta = meta;
    return true;
}

bool storageGetDrinkRecord(uint16_t index, DrinkRecord& record) {
    if (index >= g_stub_meta.record_count || index >= 30) return false;
    record = g_stub_records[index];
    return true;
}

bool storageSaveDrinkRecord(const DrinkRecord& record) {
    if (g_stub_meta.record_count >= 30) return false;
    DrinkRecord r = record;
    r.record_id = g_stub_meta.next_record_id++;
    g_stub_records[g_stub_meta.write_index % 30] = r;
    g_stub_meta.write_index++;
    g_stub_meta.record_count++;
    g_stub_meta.total_writes++;
    return true;
}

bool storageLoadLastDrinkRecord(DrinkRecord& record) {
    if (g_stub_meta.record_count == 0) return false;
    record = g_stub_records[g_stub_meta.record_count - 1];
    return true;
}

bool storageLoadDailyState(DailyState& state) {
    if (!g_stub_daily_state_valid) return false;
    state = g_stub_daily_state;
    return true;
}

bool storageSaveDailyState(const DailyState& state) {
    g_stub_daily_state       = state;
    g_stub_daily_state_valid = true;
    return true;
}

bool storageMarkSynced(uint16_t, uint16_t) { return true; }
uint16_t storageGetUnsyncedCount()          { return 0; }
bool storageGetUnsyncedRecords(DrinkRecord*, uint16_t, uint16_t& out_count) {
    out_count = 0;
    return true;
}

bool storageMarkDeleted(uint32_t record_id) {
    for (uint16_t i = 0; i < g_stub_meta.record_count; i++) {
        if (g_stub_records[i].record_id == record_id) {
            g_stub_records[i].flags |= 0x04;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// storage.h stubs (NVS-based settings — not exercised in drinks/weight tests)
// ---------------------------------------------------------------------------
bool storageInit()                                  { return true; }
bool storageSaveCalibration(const CalibrationData&) { return true; }
bool storageLoadCalibration(CalibrationData&)       { return false; }
bool storageResetCalibration()                      { return true; }
bool storageHasValidCalibration()                   { return false; }

CalibrationData storageGetEmptyCalibration() {
    CalibrationData c;
    memset(&c, 0, sizeof(c));
    return c;
}

bool storageSaveTimezone(int8_t)    { return true; }
int8_t storageLoadTimezone()        { return 0; }
bool storageSaveTimeValid(bool)     { return true; }
bool storageLoadTimeValid()         { return false; }
bool storageSaveLastBootTime(uint32_t) { return true; }
uint32_t storageLoadLastBootTime()  { return 0; }
bool storageSaveDisplayMode(uint8_t) { return true; }
uint8_t storageLoadDisplayMode()    { return 0; }
bool storageSaveSleepTimeout(uint32_t) { return true; }
uint32_t storageLoadSleepTimeout()  { return 30; }
bool storageSaveExtendedSleepTimer(uint32_t) { return true; }
uint32_t storageLoadExtendedSleepTimer() { return 60; }
bool storageSaveExtendedSleepThreshold(uint32_t) { return true; }
uint32_t storageLoadExtendedSleepThreshold() { return 120; }
bool storageSaveShakeToEmptyEnabled(bool) { return true; }
bool storageLoadShakeToEmptyEnabled() { return true; }
bool storageSaveDailyGoal(uint16_t) { return true; }
uint16_t storageLoadDailyGoal()     { return 2500; }
bool storageSaveLowBatteryThreshold(uint8_t) { return true; }
uint8_t storageLoadLowBatteryThreshold() { return 20; }

// ---------------------------------------------------------------------------
// Source files under test — included here because PlatformIO native test mode
// only compiles files in test/, not src/.
// ---------------------------------------------------------------------------
#include "../src/calibration.cpp"
#include "../src/weight.cpp"
#include "../src/drinks.cpp"
