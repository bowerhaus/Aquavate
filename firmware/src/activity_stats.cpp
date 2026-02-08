// activity_stats.cpp - Activity and sleep mode tracking implementation
// Part of the Aquavate smart water bottle firmware
//
// Tracks individual wake events and aggregated backpack sessions for
// battery life analysis. Data stored in RTC memory (survives deep sleep,
// resets on power cycle).

#include "activity_stats.h"
#include "config.h"
#include <sys/time.h>

// External functions from drinks.cpp
extern uint32_t getCurrentUnixTime();
extern uint16_t drinksGetDrinkCount();

// RTC memory buffer - survives deep sleep, lost on power cycle
#define RTC_MAGIC_ACTIVITY 0x41435456  // "ACTV" in hex
RTC_DATA_ATTR ActivityBuffer rtc_activity_buffer;

// RAM tracking for current wake session
static CurrentWakeSession g_current_wake = {0, 0, WAKE_REASON_OTHER, false, 0};
static bool g_activity_initialized = false;

// Helper: Add motion wake event to circular buffer
static void addMotionEvent(const MotionWakeEvent& event) {
    uint8_t index = rtc_activity_buffer.motion_write_index;
    rtc_activity_buffer.motion_events[index] = event;
    rtc_activity_buffer.motion_write_index = (index + 1) % MOTION_WAKE_MAX_COUNT;
    if (rtc_activity_buffer.motion_count < MOTION_WAKE_MAX_COUNT) {
        rtc_activity_buffer.motion_count++;
    }
}

// Helper: Add backpack session to circular buffer
static void addBackpackSession(const BackpackSession& session) {
    uint8_t index = rtc_activity_buffer.session_write_index;
    rtc_activity_buffer.sessions[index] = session;
    rtc_activity_buffer.session_write_index = (index + 1) % BACKPACK_SESSION_MAX_COUNT;
    if (rtc_activity_buffer.session_count < BACKPACK_SESSION_MAX_COUNT) {
        rtc_activity_buffer.session_count++;
    }
}

void activityStatsInit() {
    // Initialize fresh buffer (power cycle or first boot)
    rtc_activity_buffer.magic = RTC_MAGIC_ACTIVITY;
    rtc_activity_buffer.motion_write_index = 0;
    rtc_activity_buffer.motion_count = 0;
    rtc_activity_buffer.session_write_index = 0;
    rtc_activity_buffer.session_count = 0;
    rtc_activity_buffer.current_session_start = 0;
    rtc_activity_buffer.current_timer_wake_count = 0;
    rtc_activity_buffer._reserved = 0;

    // Clear current wake tracking
    g_current_wake = {0, 0, WAKE_REASON_OTHER, false, 0};
    g_activity_initialized = true;

    Serial.println("Activity: Initialized fresh buffer (power cycle)");
}

void activityStatsSaveToRTC() {
    // Buffer is already in RTC memory, just ensure magic is set
    rtc_activity_buffer.magic = RTC_MAGIC_ACTIVITY;
    Serial.print("Activity: Saved to RTC - ");
    Serial.print(rtc_activity_buffer.motion_count);
    Serial.print(" motion events, ");
    Serial.print(rtc_activity_buffer.session_count);
    Serial.println(" backpack sessions");
}

bool activityStatsRestoreFromRTC() {
    if (rtc_activity_buffer.magic != RTC_MAGIC_ACTIVITY) {
        Serial.println("Activity: RTC magic invalid, buffer not restored");
        return false;
    }

    g_activity_initialized = true;
    Serial.print("Activity: Restored from RTC - ");
    Serial.print(rtc_activity_buffer.motion_count);
    Serial.print(" motion events, ");
    Serial.print(rtc_activity_buffer.session_count);
    Serial.println(" backpack sessions");

    if (rtc_activity_buffer.current_session_start != 0) {
        Serial.print("Activity: In backpack mode, timer wakes: ");
        Serial.println(rtc_activity_buffer.current_timer_wake_count);
    }

    return true;
}

void activityStatsRecordWakeStart(WakeReason reason) {
    // If we were in backpack mode and woke via motion, finalize that session
    if (reason == WAKE_REASON_MOTION && rtc_activity_buffer.current_session_start != 0) {
        activityStatsFinalizeBackpackSession(EXIT_MOTION_DETECTED);
    }

    // If timer wake during backpack mode, increment session counter
    if (reason == WAKE_REASON_TIMER && rtc_activity_buffer.current_session_start != 0) {
        rtc_activity_buffer.current_timer_wake_count++;
    }

    // Start tracking this wake
    g_current_wake.wake_timestamp = getCurrentUnixTime();
    g_current_wake.wake_millis = millis();
    g_current_wake.wake_reason = reason;
    g_current_wake.recorded = false;
    g_current_wake.drink_count_at_wake = drinksGetDrinkCount();

    Serial.print("Activity: Wake started - reason=");
    Serial.print(reason);
    Serial.print(", drinks=");
    Serial.println(g_current_wake.drink_count_at_wake);
}

void activityStatsRecordNormalSleep() {
    if (g_current_wake.recorded) {
        return;  // Already recorded this wake
    }

    uint32_t awake_ms = millis() - g_current_wake.wake_millis;
    bool drink_taken = drinksGetDrinkCount() > g_current_wake.drink_count_at_wake;

    MotionWakeEvent event;
    event.timestamp = g_current_wake.wake_timestamp;
    event.duration_sec = awake_ms / 1000;
    event.wake_reason = g_current_wake.wake_reason;
    event.sleep_type = SLEEP_TYPE_NORMAL | (drink_taken ? SLEEP_TYPE_DRINK_TAKEN_FLAG : 0);

    addMotionEvent(event);
    g_current_wake.recorded = true;

    Serial.print("Activity: Recorded motion wake - duration=");
    Serial.print(event.duration_sec);
    Serial.print("s, drink=");
    Serial.print(drink_taken ? "yes" : "no");
    Serial.println(", entering normal sleep");
}

void activityStatsRecordExtendedSleep() {
    if (!g_current_wake.recorded) {
        // Record the motion wake that led to extended sleep
        uint32_t awake_ms = millis() - g_current_wake.wake_millis;
        bool drink_taken = drinksGetDrinkCount() > g_current_wake.drink_count_at_wake;

        MotionWakeEvent event;
        event.timestamp = g_current_wake.wake_timestamp;
        event.duration_sec = awake_ms / 1000;
        event.wake_reason = g_current_wake.wake_reason;
        event.sleep_type = SLEEP_TYPE_EXTENDED | (drink_taken ? SLEEP_TYPE_DRINK_TAKEN_FLAG : 0);

        addMotionEvent(event);
        g_current_wake.recorded = true;

        Serial.print("Activity: Recorded motion wake - duration=");
        Serial.print(event.duration_sec);
        Serial.print("s, drink=");
        Serial.print(drink_taken ? "yes" : "no");
        Serial.println(", entering extended sleep");
    }

    // Start tracking backpack session if not already
    if (rtc_activity_buffer.current_session_start == 0) {
        rtc_activity_buffer.current_session_start = getCurrentUnixTime();
        rtc_activity_buffer.current_timer_wake_count = 0;
        Serial.println("Activity: Started new backpack session");
    }
}

void activityStatsRecordTimerWake() {
    // Increment timer wake counter for current backpack session
    rtc_activity_buffer.current_timer_wake_count++;

    // Reset current wake tracking for this timer wake
    g_current_wake.wake_timestamp = getCurrentUnixTime();
    g_current_wake.wake_millis = millis();
    g_current_wake.wake_reason = WAKE_REASON_TIMER;
    g_current_wake.recorded = true;  // Timer wakes don't get individual records

    Serial.print("Activity: Timer wake #");
    Serial.println(rtc_activity_buffer.current_timer_wake_count);
}

void activityStatsFinalizeBackpackSession(BackpackExitReason reason) {
    if (rtc_activity_buffer.current_session_start == 0) {
        return;  // Not in backpack mode
    }

    uint32_t now = getCurrentUnixTime();

    BackpackSession session;
    session.start_timestamp = rtc_activity_buffer.current_session_start;
    session.duration_sec = now - rtc_activity_buffer.current_session_start;
    session.timer_wake_count = rtc_activity_buffer.current_timer_wake_count;
    session.exit_reason = reason;
    session.flags = 0;

    addBackpackSession(session);

    Serial.print("Activity: Finalized backpack session - duration=");
    Serial.print(session.duration_sec);
    Serial.print("s, timer wakes=");
    Serial.print(session.timer_wake_count);
    Serial.print(", exit reason=");
    Serial.println(reason);

    // Reset current session tracking
    rtc_activity_buffer.current_session_start = 0;
    rtc_activity_buffer.current_timer_wake_count = 0;
}

uint8_t activityStatsGetMotionEvents(MotionWakeEvent* buffer, uint8_t max_count) {
    uint8_t count = min(rtc_activity_buffer.motion_count, max_count);

    // Return events in chronological order (oldest first)
    // The circular buffer has newest at (write_index - 1), oldest depends on whether full
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx;
        if (rtc_activity_buffer.motion_count < MOTION_WAKE_MAX_COUNT) {
            // Buffer not full - events start at 0
            idx = i;
        } else {
            // Buffer full - oldest is at write_index
            idx = (rtc_activity_buffer.motion_write_index + i) % MOTION_WAKE_MAX_COUNT;
        }
        buffer[i] = rtc_activity_buffer.motion_events[idx];
    }

    return count;
}

uint8_t activityStatsGetBackpackSessions(BackpackSession* buffer, uint8_t max_count) {
    uint8_t count = min(rtc_activity_buffer.session_count, max_count);

    // Return sessions in chronological order (oldest first)
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx;
        if (rtc_activity_buffer.session_count < BACKPACK_SESSION_MAX_COUNT) {
            idx = i;
        } else {
            idx = (rtc_activity_buffer.session_write_index + i) % BACKPACK_SESSION_MAX_COUNT;
        }
        buffer[i] = rtc_activity_buffer.sessions[idx];
    }

    return count;
}

uint8_t activityStatsGetMotionEventCount() {
    return rtc_activity_buffer.motion_count;
}

uint8_t activityStatsGetBackpackSessionCount() {
    return rtc_activity_buffer.session_count;
}

bool activityStatsIsInBackpackMode() {
    return rtc_activity_buffer.current_session_start != 0;
}

uint32_t activityStatsGetCurrentSessionStart() {
    return rtc_activity_buffer.current_session_start;
}

uint16_t activityStatsGetCurrentTimerWakeCount() {
    return rtc_activity_buffer.current_timer_wake_count;
}
