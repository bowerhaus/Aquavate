# Aquavate - Active Development Progress

**Last Updated:** 2026-01-27
**Current Branch:** `ota-firmware-updates`

---

## Current Task

**OTA Firmware Updates** (Issue #78) - [Plan 058](Plans/058-ota-firmware-updates.md)

**Status:** DEFERRED - Awaiting ESP32-S3 hardware

**Decision:** OTA implementation deferred until ESP32-S3 Feather is available:
- ESP32 V2 has insufficient IRAM (94.5% used, only ~6KB headroom)
- BLE OTA transfer too slow (~5 minutes for 720KB firmware)
- ESP32-S3 enables WiFi OTA (~10 seconds) with 512KB IRAM

**Branch:** `ota-firmware-updates` - Pushed for future implementation

**To resume implementation when S3 arrives:**
```
git checkout ota-firmware-updates
Resume from PROGRESS.md
```

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md
```

---

## Recently Completed

- ✅ Drink Baseline Hysteresis Fix (Issue #76) - [Plan 057](Plans/057-drink-baseline-hysteresis.md)
  - Fixed drinks sometimes not recorded when second drink taken during same session
  - Added drift threshold (15ml) to prevent baseline contamination during drink detection
  - Only firmware changes (config.h, drinks.cpp)

- ✅ Unified Sessions View Fix (Issue #74) - [Plan 056](Plans/056-unified-sessions-view.md)
  - Replaced confusing separate "Recent Motion Wakes" and "Backpack Sessions" sections
  - New unified "Sessions" list shows both types chronologically
  - Summary changed from "Since Last Charge" to "Last 7 Days"
  - Users can now clearly see backpack sessions with correct 1+ hour durations

- ✅ Repeated Amber Notification Fix (Issue #72) - [Plan 055](Plans/055-repeated-amber-notification-fix.md)
  - Fixed `lastNotifiedUrgency` not persisting across app restarts (notifications were re-sent)
  - Updated urgency colors: attention = RGB(247,239,151), overdue = red
  - Human figure now uses smooth gradient from attention to overdue color
  - Added white background layer to fix color blending issues

- ✅ iOS Day Boundary Fix (Issue #70) - [Plan 054](Plans/054-ios-day-boundary-fix.md)
  - Fixed drinks from previous day showing after midnight
  - Made @FetchRequest predicate dynamic with onAppear and significantTimeChangeNotification

- ✅ Notification Threshold Adjustment (Issue #67) - [Plan 053](Plans/053-notification-threshold-adjustment.md)
  - Increased minimum deficit thresholds for hydration notifications (50ml → 150ml)

- ✅ iOS Memory Exhaustion Fix (Issue #28) - [Plan 052](Plans/052-ios-memory-exhaustion-fix.md)
  - Fixed memory leaks from WatchConnectivity queue, CoreData @FetchRequest, NotificationManager captures
  - App now runs 60+ minutes without memory exhaustion

- ✅ Foreground Notification Fix (Issue #56) - [Plan 051](Plans/051-foreground-notification-fix.md)
  - Fixed hydration reminders not appearing when app is in foreground
  - Added UNUserNotificationCenterDelegate to NotificationManager

---

## Branch Status

- `master` - Stable baseline
- `ota-firmware-updates` - Issue #78, deferred to ESP32-S3 (branch saved for future)

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
