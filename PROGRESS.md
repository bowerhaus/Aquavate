# Aquavate - Active Development Progress

**Last Updated:** 2026-01-29 (Session 20)
**Current Branch:** `settings-option5-smart-contextual`
**GitHub Issue:** #87
**Plan:** Plans/063-settings-page-redesign.md (not yet created as file)

---

## Current Task

No active task. Settings Page Redesign (Issue #87) is complete — awaiting PR merge.

### Approach

Iterative implementation: 5 layout options, each on its own branch off `master`, for side-by-side comparison. User picks favourite or hybrid for final version.

### Shared Principles (all options)
- Disabled + banner when disconnected (never hidden)
- Calibrate Bottle always visible at top level
- Connection status merged into Device section (no standalone section)
- Keep-alive while on Settings via `beginKeepAlive()` / `endKeepAlive()` API in BLEManager

### Removed Items (all options — decided during Option 2 review)

These items have been removed from the settings UI across all options. The underlying code remains (used internally or by other views).

**Removed entirely:**
- Sync Time command (auto-syncs on connect)
- Current Weight display
- Device Name display
- Bottle Capacity display
- Time Set flag
- Last Synced display
- Unsynced Records display
- Syncing progress display
- Current Status (hydration urgency)
- Danger Zone section header
- Debug section (scan/connect/disconnect/cancel)
- Error message display in Device Status section (disconnected reason, connection timeout, etc.)

**Reinstated (session 20):**
- Health Status (HealthKit authorized/not) → added to Health section in Goals & Health sub-page
- Notification Status (authorized/not) → added to Notifications section in Goals & Health sub-page

**Relocated:**
- Calibrated flag → moved to Device Setup section (above Calibrate Bottle)
- Clear Device History → moved to Commands (red destructive button with confirmation alert)
- Earlier Reminders toggle → moved from Debug into Reminder Options (above Back On Track Alerts)
- Reminder Options subsection → moved above Commands in Advanced

### Branches & Progress

| # | Branch | Option | Status |
|---|--------|--------|--------|
| 1 | `settings-option1-disclosure-groups` | Collapsible Sections (DisclosureGroup) | ✅ Built (uncommitted on branch, does NOT have keep-alive fix or BLEManager changes) |
| 2 | `settings-option2-essential-advanced` | Essential + Advanced Split | ✅ Built (uncommitted on branch, HAS keep-alive fix + background handling) |
| 3 | `settings-option3-connection-cards` | Connection-Aware Cards | ✅ Built (committed on branch, HAS keep-alive fix) |
| 4 | `settings-option4-category-tabs` | Category Tabs | ✅ Built (committed on branch, HAS keep-alive fix) |
| 5 | `settings-option5-smart-contextual` | Smart Contextual (sub-pages) | ✅ **SELECTED** — Built (uncommitted on branch, HAS keep-alive fix) |

### Implementation Log

- [x] Plan created (063-settings-page-redesign.md)
- [x] GitHub issue created (#87)
- [x] Base branch created: `settings-page-redesign` from `master`
- [x] Option 1: Implemented (DisclosureGroup layout — 6 sections with collapsible sub-groups)
- [x] Option 1: User review on device
- [x] Option 2: Implemented (Essential + Advanced Split — 4 top-level sections + single Advanced expander)
- [x] Option 2: User review on device
- [x] Option 3: Implemented (Connection-Aware Cards — connection badges + visual dimming)
- [x] Option 3: User review on device
- [x] Option 4: Implemented (Category Tabs — segmented picker with 3 tabs)
- [x] Option 4: User review on device
- [x] Option 5: Implemented (Smart Contextual — sub-pages with contextual summaries)
- [x] Option 5: User review on device
- [x] User selects preferred option → **Option 5 (Smart Contextual)**
- [x] Session 20 tweaks: reinstated Health/Notification status flags, removed error message row, updated disconnected info text
- [x] Final implementation on clean branch (commit + PR)
- [x] PR created referencing #87

### Option 1 Details (branch: settings-option1-disclosure-groups)

Layout: 6 sections (down from 9):
1. **Device** — compact connection status + battery in one row, disconnection banner, errors/warnings
2. **Goals & Tracking** — Daily Goal + Calibrate Bottle (always visible, disabled when disconnected) + collapsible Bottle Details
3. **Notifications** — Reminders toggle + Health toggle + collapsible Reminder Options (status, limits, back-on-track)
4. **Device Settings** — Shake to Empty + collapsible Device Info + collapsible Commands + collapsible Danger Zone (whole section disabled at 50% opacity when disconnected, with footer text)
5. **Diagnostics** — Sleep Analysis (unchanged, works offline)
6. **About** — GitHub link
7. **Debug** (#if DEBUG) — scan/connect/disconnect + test mode

Key features:
- DisclosureGroups for progressive disclosure (all collapsed by default)
- Device Settings section always visible but disabled/dimmed when disconnected
- **Note:** Does NOT have BLEManager keep-alive fix. Needs `beginKeepAlive()`/`endKeepAlive()` applied.

### Option 2 Details (branch: settings-option2-essential-advanced)

Layout: 4 essential sections + 1 Advanced expander:
1. **Device Status** — compact connection status + battery, disconnection banner, errors/warnings
2. **Your Goals** — Daily Goal + Hydration Reminders toggle + Apple Health toggle
3. **Device Setup** — Calibrated status + Calibrate Bottle link (disabled when disconnected, footer hint)
4. **Advanced** — single DisclosureGroup containing:
   - Reminder Options (reminders today, limit daily, earlier reminders, back on track — only when reminders enabled)
   - Commands (Tare, Reset Daily, Sync History, Clear Device History with confirmation)
   - Gestures (Shake to Empty)
   - Diagnostics (Sleep Analysis — works offline)
5. **About** — GitHub link

Key features:
- Clean, focused top 3 sections — most users only need these
- Single "Advanced" expander groups all rarely-used items with inline sub-headers
- Individual items disabled when disconnected (not whole section)
- Clear Device History uses destructive button with confirmation alert
- Keep-alive via `beginKeepAlive()` / `endKeepAlive()` API in BLEManager

### Option 3 Details (branch: settings-option3-connection-cards)

Layout: 6 sections (7 when reminders enabled) — flat structure, no disclosure groups:
1. **Device Status** — compact connection status + battery, disconnection banner, errors/warnings
2. **Your Goals** — Daily Goal, Hydration Reminders toggle, Apple Health toggle
3. **Device Setup** [connection-aware card] — Calibrated status + Calibrate Bottle
4. **Device Controls** [connection-aware card] — Tare, Reset Daily, Sync History, Clear Device History (with confirmation), Shake to Empty
5. **Reminder Options** (conditional — only when reminders enabled) — Reminders Today, Limit Daily, Earlier Reminders, Back On Track
6. **Diagnostics** — Sleep Analysis (works offline)
7. **About** — GitHub link

Key features:
- **Connection-aware section headers** — sections requiring BLE show a `ConnectionAwareHeader` with a colored dot (green = connected, gray = not connected) and status text
- **Section-level dimming** — connection-dependent section content uses `.opacity(0.5)` when disconnected, making entire card visually distinct
- **Footer messages** — disconnected cards show contextual footer ("Connect your bottle to calibrate and configure")
- **Flat structure** — no DisclosureGroups or progressive disclosure; visual card treatment replaces hidden content
- All settings always visible; connection state is communicated through visual treatment rather than hiding or collapsing
- Keep-alive via `beginKeepAlive()` / `endKeepAlive()` API in BLEManager

### Option 4 Details (branch: settings-option4-category-tabs)

Layout: Segmented picker with 3 tabs — **General**, **Device**, **More**:

**General tab:**
1. **Device Status** — compact connection status + battery, disconnection banner, errors/warnings
2. **Your Goals** — Daily Goal, Hydration Reminders toggle, Apple Health toggle

**Device tab:**
1. **Device Setup** — Calibrated status + Calibrate Bottle (dimmed when disconnected, footer hint)
2. **Device Controls** — Tare, Reset Daily, Sync History, Clear Device History (with confirmation), Shake to Empty (footer hint when disconnected)

**More tab:**
1. **Reminder Options** (conditional — only when reminders enabled) — Reminders Today, Limit Daily, Earlier Reminders, Back On Track
2. **Diagnostics** — Sleep Analysis (works offline)
3. **About** — GitHub link

Key features:
- **Segmented picker** at top of list switches between 3 category tabs
- **Reduced scrolling** — each tab shows only 2-3 sections instead of 6-7
- **Logical grouping** — daily-use items in General, hardware in Device, extras in More
- **Connection state** — Device tab items disabled + dimmed when disconnected, with footer hints
- All settings always visible within their tab; no hiding or collapsing
- Keep-alive via `beginKeepAlive()` / `endKeepAlive()` API in BLEManager

### Option 5 Details (branch: settings-option5-smart-contextual)

Layout: Apple Settings-style with main page categories + drill-down sub-pages:

**Main page:**
1. **Device Status** (inline) — compact connection status + battery, disconnected banner, errors/warnings
2. **Category navigation rows** (each is a NavigationLink with contextual summary):
   - **Goals & Health** → sub-page with Daily Goal, Hydration Reminders toggle, Apple Health toggle
     - Summary: "2000ml goal · Reminders on · Health sync on"
   - **Device Information** → sub-page with Calibration section + Diagnostics section (Sleep Mode Analysis)
     - Summary: "Calibrated" / "Not calibrated" / "Calibration, sleep analysis" (when disconnected)
   - **Device Controls** → sub-page with Tare, Reset Daily, Sync, Clear History, Shake to Empty
     - Summary: "Tare, reset, sync, gestures" / "3 unsynced records" / "Requires connection"
   - **Reminder Options** (conditional — only when reminders enabled) → sub-page
     - Summary: "3/10 sent today"
3. **About** — GitHub link (inline)

Key features:
- **Ultra-compact main page** — just 4-5 category rows with contextual summaries, minimal scrolling
- **Drill-down sub-pages** — full settings content on dedicated pages (Apple Settings pattern)
- **Contextual summaries** — each row shows live status info (goal, calibration state, unsynced count, reminders sent)
- **Smart summary colors** — "Not calibrated" shown in orange, others in secondary gray
- **Aligned icons** — all category rows use 28pt icon frame for visual alignment
- **Sub-page structure** — GoalsSettingsPage, DeviceInformationPage, DeviceControlsPage, ReminderOptionsPage (private structs in same file)
- **Device Information** combines Calibration + Diagnostics (Sleep Mode Analysis) on one sub-page
- Connection-dependent sub-pages show disabled items + footer hints when disconnected
- Keep-alive via `beginKeepAlive()` / `endKeepAlive()` API in BLEManager

### Keep-Alive Implementation (on option2, option3, option4, and option5 branches)

**Problem:** The original approach (cancel idle timer in `onAppear`) was defeated because `startIdleDisconnectTimer()` is called from connection completion and sync completion, restarting the timer while on Settings.

**Solution:** Added `beginKeepAlive()` / `endKeepAlive()` to BLEManager using reference counting:
- While `keepAliveCount > 0`, `startIdleDisconnectTimer()` is a no-op
- Periodic pings (30s) sent automatically to prevent firmware sleep
- SettingsView just calls `.onAppear { bleManager.beginKeepAlive() }` / `.onDisappear { bleManager.endKeepAlive() }`

**Background handling:** When app enters background, ping timer is stopped immediately so the normal 5-second background disconnect can proceed. When app returns to foreground, if keep-alive is still active (Settings was open), pings resume automatically.

Files changed (keep-alive):
- `ios/.../BLEManager.swift` — added `keepAliveCount`, `keepAlivePingTimer`, `beginKeepAlive()`, `endKeepAlive()`, guard in `startIdleDisconnectTimer()`, ping timer suspend/resume in `appDidEnterBackground()`/`appDidBecomeActive()`
- `ios/.../SettingsView.swift` — simplified to two-line `.onAppear`/`.onDisappear`

### Uncommitted Files (on option5 branch)

- `ios/Aquavate/Aquavate/Views/SettingsView.swift` — Option 5 layout + keep-alive
- `ios/Aquavate/Aquavate/Services/BLEManager.swift` — Keep-alive API + background handling
- `PROGRESS.md` — This file

---

## Recently Completed

- **Settings Page Redesign (Issue #87)** - Plan 063 ✅ COMPLETE — Option 5 (Smart Contextual sub-pages) selected. Keep-alive fix, Health/Notification status flags, error message cleanup. iOS-UX-PRD updated.
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
Resume from PROGRESS.md — no active task.

Session 20: Settings Page Redesign (Issue #87) completed.
- Option 5 (Smart Contextual) selected after reviewing all 5 options on device
- Final tweaks: reinstated Health/Notification status flags, removed error message row, updated info text
- BLEManager keep-alive fix committed
- iOS-UX-PRD.md Section 2.6 updated to reflect new Settings layout
- PR created referencing #87
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
