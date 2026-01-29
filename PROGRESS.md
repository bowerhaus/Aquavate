# Aquavate - Active Development Progress

**Last Updated:** 2026-01-29 (Session 20)
**Current Branch:** `settings-option1-disclosure-groups`
**GitHub Issue:** #87
**Plan:** [063-settings-page-redesign.md](Plans/063-settings-page-redesign.md)

---

## Current Task

**Settings Page Redesign (Issue #87)** - Redesign the settings page with progressive disclosure, clearer connection states, and logical hierarchy.

### Decision

User chose **Option 1 (DisclosureGroup layout)** as the preferred approach, with significant cleanup applied. The other 4 options (on their own branches) are no longer being pursued.

### What Was Done This Session (Session 20)

Started from the existing `settings-option1-disclosure-groups` branch and applied:

1. **Removed Items** (decided during earlier Option 2 review, now applied to Option 1):
   - Sync Time command (auto-syncs on connect)
   - Current Weight, Device Name, Bottle Capacity, Time Set flag, Last Synced, Unsynced Records, Syncing progress displays
   - Health Status, Notification Status, Current Status (hydration urgency) rows
   - Danger Zone section header (Clear History moved into Commands)
   - Debug section (scan/connect/disconnect/cancel controls)
   - Bottle Details DisclosureGroup
   - Entire Device Info DisclosureGroup

2. **Relocated Items:**
   - Calibrated status + Calibrate Bottle → moved to Device Settings section (top)
   - Clear Device History → moved into Commands DisclosureGroup (red destructive button with confirmation alert)
   - Earlier Reminders toggle → moved from Debug into Reminder Options
   - Sync to Health → moved from Notifications into Goals & Tracking
   - Reminders Today → moved outside Reminder Options DisclosureGroup (standalone row below Hydration Reminders toggle)

3. **Keep-Alive Fix Added:**
   - BLEManager: `beginKeepAlive()` / `endKeepAlive()` API with reference counting
   - BLEManager: `keepAliveCount` + `keepAlivePingTimer` private state
   - BLEManager: Guard in `startIdleDisconnectTimer()` (no-op while keep-alive active)
   - BLEManager: Ping timer suspend/resume in `appDidEnterBackground()`/`appDidBecomeActive()`
   - SettingsView: Simplified to `.onAppear { bleManager.beginKeepAlive() }` / `.onDisappear { bleManager.endKeepAlive() }`

4. **Layout Tweaks (user-directed):**
   - Shake to Empty moved to bottom of Device Settings section
   - Reminder Options placed immediately below Hydration Reminders toggle
   - Reminders Today shown as standalone row (not inside DisclosureGroup)

### Current Layout (5 sections)

1. **Device** — compact connection status + battery, disconnected banner, errors/warnings
2. **Goals & Tracking** — Daily Goal, Sync to Health toggle
3. **Notifications** — Hydration Reminders toggle, Reminders Today (standalone), Reminder Options DisclosureGroup (Limit Daily, Earlier Reminders, Back On Track)
4. **Device Settings** (disabled/dimmed when disconnected) — Calibrated status, Calibrate Bottle, Commands DisclosureGroup (Tare, Reset Daily, Sync History, Clear Device History with confirmation), Shake to Empty
5. **Diagnostics** — Sleep Mode Analysis
6. **About** — GitHub link

### Uncommitted Files

- `ios/Aquavate/Aquavate/Views/SettingsView.swift` — Option 1 with all cleanup + keep-alive
- `ios/Aquavate/Aquavate/Services/BLEManager.swift` — Keep-alive API added

### Build Status

Build verified successfully after all changes.

### What's Next

- User reviews on device
- User decides if further tweaks needed
- Commit and create PR referencing #87

---

## Recently Completed

- **Daily Goal Setting from iOS App (Issue #83)** - [Plan 062](Plans/062-daily-goal-setting.md) ✅ COMPLETE (PR #86)
- **Calibration Circular Dependency Fix (Issue #84)** - [Plan 062](Plans/062-calibration-circular-dependency.md) ✅ COMPLETE (PR #85)
- **Faded Blue Behind Indicator (Issue #81)** - [Plan 061](Plans/061-faded-blue-behind-indicator.md) ✅ COMPLETE
- **iOS Calibration Flow (Issue #30)** - [Plan 060](Plans/060-ios-calibration-flow.md) ✅ COMPLETE
- **LittleFS Drink Storage / NVS Fragmentation Fix (Issue #76)** - [Plan 059](Plans/059-littlefs-drink-storage.md)
- Drink Baseline Hysteresis Fix (Issue #76) - [Plan 057](Plans/057-drink-baseline-hysteresis.md)
- Unified Sessions View Fix (Issue #74) - [Plan 056](Plans/056-unified-sessions-view.md)

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md - Settings Page Redesign (Issue #87).

Session 20: Applied full cleanup to Option 1 (DisclosureGroup layout) on branch settings-option1-disclosure-groups.
Currently on branch: settings-option1-disclosure-groups (has uncommitted changes in SettingsView.swift and BLEManager.swift).

What was done:
- Applied all "Removed Items" from earlier review (removed Sync Time, Current Weight, Device Name, Bottle Capacity, Time Set, Last Synced, Unsynced Records, Syncing progress, Health Status, Notification Status, Current Status, Danger Zone section, Debug section, Bottle Details, Device Info DisclosureGroup)
- Relocated: Calibrated + Calibrate Bottle to Device Settings, Clear History into Commands with confirmation alert, Earlier Reminders into Reminder Options, Sync to Health into Goals & Tracking, Reminders Today as standalone row
- Added BLEManager keep-alive API (beginKeepAlive/endKeepAlive with reference counting, periodic pings, background suspend/resume)
- SettingsView uses beginKeepAlive/endKeepAlive instead of local timer
- Layout tweaks: Shake to Empty at bottom of Device Settings, Reminder Options right after Hydration Reminders, Reminders Today outside DisclosureGroup
- Build verified successfully

What's next:
- User reviews on device
- Further tweaks if needed
- Commit and PR referencing #87
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
