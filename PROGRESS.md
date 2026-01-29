# Aquavate - Active Development Progress

**Last Updated:** 2026-01-29 (Session 16)
**Current Branch:** `settings-option2-essential-advanced`
**GitHub Issue:** #87
**Plan:** [063-settings-page-redesign.md](Plans/063-settings-page-redesign.md)

---

## Current Task

**Settings Page Redesign (Issue #87)** - Redesign the settings page with progressive disclosure, clearer connection states, and logical hierarchy.

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
- Health Status (HealthKit authorized/not)
- Current Status (hydration urgency)
- Notification Status (authorized/not)
- Danger Zone section header
- Debug section (scan/connect/disconnect/cancel)

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
| 3 | `settings-option3-connection-cards` | Connection-Aware Cards | ⬜ Pending |
| 4 | `settings-option4-category-tabs` | Category Tabs | ⬜ Pending |
| 5 | `settings-option5-smart-contextual` | Smart Contextual (sub-pages) | ⬜ Pending |

### Implementation Log

- [x] Plan created (063-settings-page-redesign.md)
- [x] GitHub issue created (#87)
- [x] Base branch created: `settings-page-redesign` from `master`
- [x] Option 1: Implemented (DisclosureGroup layout — 6 sections with collapsible sub-groups)
- [ ] Option 1: User review on device
- [x] Option 2: Implemented (Essential + Advanced Split — 4 top-level sections + single Advanced expander)
- [ ] Option 2: User review on device
- [ ] Option 3: Implement and build
- [ ] Option 4: Implement and build
- [ ] Option 5: Implement and build
- [ ] User selects preferred option
- [ ] Final implementation on clean branch
- [ ] PR created referencing #87

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

### Keep-Alive Implementation (on option2 branch)

**Problem:** The original approach (cancel idle timer in `onAppear`) was defeated because `startIdleDisconnectTimer()` is called from connection completion and sync completion, restarting the timer while on Settings.

**Solution:** Added `beginKeepAlive()` / `endKeepAlive()` to BLEManager using reference counting:
- While `keepAliveCount > 0`, `startIdleDisconnectTimer()` is a no-op
- Periodic pings (30s) sent automatically to prevent firmware sleep
- SettingsView just calls `.onAppear { bleManager.beginKeepAlive() }` / `.onDisappear { bleManager.endKeepAlive() }`

**Background handling:** When app enters background, ping timer is stopped immediately so the normal 5-second background disconnect can proceed. When app returns to foreground, if keep-alive is still active (Settings was open), pings resume automatically.

Files changed on option2 branch:
- `ios/.../BLEManager.swift` — added `keepAliveCount`, `keepAlivePingTimer`, `beginKeepAlive()`, `endKeepAlive()`, guard in `startIdleDisconnectTimer()`, ping timer suspend/resume in `appDidEnterBackground()`/`appDidBecomeActive()`
- `ios/.../SettingsView.swift` — simplified to two-line `.onAppear`/`.onDisappear`

### Uncommitted Files (on option2 branch)

- `ios/Aquavate/Aquavate/Views/SettingsView.swift` — Option 2 layout + keep-alive
- `ios/Aquavate/Aquavate/Services/BLEManager.swift` — Keep-alive API + background handling
- `PROGRESS.md` — This file

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

Session 16: Continued settings redesign on Option 2 branch.
Currently on branch: settings-option2-essential-advanced (has uncommitted changes).

What was done this session:
- Tidied Option 2 settings by removing many items from the UI (see "Removed Items" section)
- Removed: Disconnect, Cancel, Scan from Debug; Sync Time from Commands; Current Weight,
  Device Name, Capacity, Time Set, Last Synced, Unsynced Records, Syncing from Device Info;
  Health Status, Current Status, Notification Status from Reminder Options; entire Debug section
- Relocated: Calibrated flag → Device Setup; Clear Device History → Commands (with confirmation
  alert); Earlier Reminders → Reminder Options; Reminder Options moved above Commands
- Removed entire Device Info subsection and Danger Zone subsection from Advanced
- Updated PROGRESS.md with "Removed Items" shared decisions applying to all options

What's done overall:
- Options 1 and 2 are built (uncommitted on their respective branches)
- Option 2 has had significant UI cleanup this session
- BLEManager keep-alive fix implemented on option2 branch
- Option 1 branch does NOT have the keep-alive fix or UI cleanup yet

What's next:
- User reviews Option 2 on device
- Decide whether to build Options 3-5 or proceed with Option 2
- If building other options, apply the same "Removed Items" decisions
- Each new option branch needs the BLEManager keep-alive changes too
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
