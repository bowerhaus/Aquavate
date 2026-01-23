#ifndef ACTIVITY_STATS_H
#define ACTIVITY_STATS_H

#include <Arduino.h>

// Buffer sizes
#define MOTION_WAKE_MAX_COUNT     100
#define BACKPACK_SESSION_MAX_COUNT 20

// Wake reasons
enum WakeReason : uint8_t {
    WAKE_REASON_MOTION = 0,      // EXT0 interrupt (accelerometer)
    WAKE_REASON_TIMER = 1,       // Timer interrupt (extended sleep)
    WAKE_REASON_POWER_ON = 2,    // Fresh power cycle
    WAKE_REASON_OTHER = 3        // Unknown/other
};

// Sleep types (what sleep mode was entered after this wake)
// Note: Bit 7 (0x80) is used as "drink taken" flag, actual type is in bits 0-6
enum SleepType : uint8_t {
    SLEEP_TYPE_NORMAL = 0,       // Motion-wake deep sleep
    SLEEP_TYPE_EXTENDED = 1,     // Timer-wake deep sleep (backpack mode)
    SLEEP_TYPE_STILL_AWAKE = 0x7F // Placeholder until sleep entered (avoid 0xFF which has flag set)
};

// Flag bit for sleep_type field indicating a drink was taken during wake
#define SLEEP_TYPE_DRINK_TAKEN_FLAG 0x80
#define SLEEP_TYPE_MASK 0x7F  // Mask to extract actual sleep type

// Exit reasons for backpack sessions
enum BackpackExitReason : uint8_t {
    EXIT_MOTION_DETECTED = 0,    // Exited because motion stopped
    EXIT_STILL_ACTIVE = 1,       // Session still in progress
    EXIT_POWER_CYCLE = 2         // Lost due to power cycle
};

// Individual motion wake event (8 bytes)
struct __attribute__((packed)) MotionWakeEvent {
    uint32_t timestamp;        // Unix timestamp when wake occurred
    uint16_t duration_sec;     // How long device stayed awake (seconds)
    uint8_t  wake_reason;      // WakeReason enum
    uint8_t  sleep_type;       // SleepType enum - sleep mode entered after this wake
};

// Aggregated backpack session (12 bytes)
struct __attribute__((packed)) BackpackSession {
    uint32_t start_timestamp;  // Unix timestamp when backpack mode started
    uint32_t duration_sec;     // Total time in backpack mode (seconds)
    uint16_t timer_wake_count; // Number of timer wakes during session
    uint8_t  exit_reason;      // BackpackExitReason enum
    uint8_t  flags;            // Reserved for future use
};

// RTC memory buffer structure (~1,060 bytes)
struct __attribute__((packed)) ActivityBuffer {
    uint32_t magic;            // 0x41435456 ("ACTV") for validation

    // Motion wake circular buffer
    uint8_t  motion_write_index;
    uint8_t  motion_count;
    MotionWakeEvent motion_events[MOTION_WAKE_MAX_COUNT];  // 800 bytes

    // Backpack session circular buffer
    uint8_t  session_write_index;
    uint8_t  session_count;
    BackpackSession sessions[BACKPACK_SESSION_MAX_COUNT];  // 240 bytes

    // Current session tracking (for active backpack mode)
    uint32_t current_session_start;      // 0 if not in backpack mode
    uint16_t current_timer_wake_count;   // Accumulates during backpack mode
    uint16_t _reserved;
};

// Current wake session tracking (RAM only, not in RTC)
struct CurrentWakeSession {
    uint32_t wake_timestamp;     // Unix time when this wake started
    uint32_t wake_millis;        // millis() at wake start
    uint8_t  wake_reason;        // How we woke up
    bool     recorded;           // Has this wake been recorded yet?
    uint16_t drink_count_at_wake; // Drink count when wake started (to detect if drink taken)
};

// Initialize activity stats (call on power cycle)
void activityStatsInit();

// Save buffer to RTC memory before deep sleep
void activityStatsSaveToRTC();

// Restore buffer from RTC memory after wake (returns true if valid)
bool activityStatsRestoreFromRTC();

// Record the start of a wake event (call in setup after determining wake reason)
void activityStatsRecordWakeStart(WakeReason reason);

// Record entering normal sleep - finalizes current wake event
void activityStatsRecordNormalSleep();

// Record entering extended sleep - finalizes wake event and starts/continues backpack session
void activityStatsRecordExtendedSleep();

// Record a timer wake during backpack mode (just increments counter)
void activityStatsRecordTimerWake();

// Finalize backpack session when exiting backpack mode
void activityStatsFinalizeBackpackSession(BackpackExitReason reason);

// Get motion wake events (returns count, fills buffer up to max_count)
uint8_t activityStatsGetMotionEvents(MotionWakeEvent* buffer, uint8_t max_count);

// Get backpack sessions (returns count, fills buffer up to max_count)
uint8_t activityStatsGetBackpackSessions(BackpackSession* buffer, uint8_t max_count);

// Get counts
uint8_t activityStatsGetMotionEventCount();
uint8_t activityStatsGetBackpackSessionCount();

// Check if currently in backpack mode
bool activityStatsIsInBackpackMode();

// Get current backpack session info (if in backpack mode)
uint32_t activityStatsGetCurrentSessionStart();
uint16_t activityStatsGetCurrentTimerWakeCount();

#endif // ACTIVITY_STATS_H
