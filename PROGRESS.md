# Aquavate - Active Development Progress

**Last Updated:** 2026-01-28 (Session 14)
**Current Branch:** `settings-option1-disclosure-groups`
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
- No DEBUG controls in production sections — separate Debug section at bottom
- Connection status merged into Device section (no standalone section)
- Keep-alive while on Settings — cancel idle disconnect timer on appear, send periodic pings (30s), restart idle timer on disappear

### Branches & Progress

| # | Branch | Option | Status |
|---|--------|--------|--------|
| 1 | `settings-option1-disclosure-groups` | Collapsible Sections (DisclosureGroup) | ✅ Built (uncommitted on branch) |
| 2 | `settings-option2-essential-advanced` | Essential + Advanced Split | ⬜ Pending |
| 3 | `settings-option3-connection-cards` | Connection-Aware Cards | ⬜ Pending |
| 4 | `settings-option4-category-tabs` | Category Tabs | ⬜ Pending |
| 5 | `settings-option5-smart-contextual` | Smart Contextual (sub-pages) | ⬜ Pending |

### Implementation Log

- [x] Plan created (063-settings-page-redesign.md)
- [x] GitHub issue created (#87)
- [x] Base branch created: `settings-page-redesign` from `master`
- [x] Option 1: Implemented and builds successfully (not yet committed — user stages manually)
- [ ] Option 1: User review in simulator
- [ ] Option 2: Implement and build
- [ ] Option 3: Implement and build
- [ ] Option 4: Implement and build
- [ ] Option 5: Implement and build
- [ ] User selects preferred option
- [ ] Final implementation on clean branch
- [ ] PR created referencing #87

### Option 1 Details (current branch)

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
- Keep-alive mechanism: cancels idle timer on appear, pings every 30s, restarts timer on disappear
- Device Settings section always visible but disabled/dimmed when disconnected

### Uncommitted Files

- `ios/Aquavate/Aquavate/Views/SettingsView.swift` — Option 1 implementation
- `Plans/063-settings-page-redesign.md` — Plan file (new)
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

Session 14: Iterative settings redesign with 5 layout options.
Currently on branch: settings-option1-disclosure-groups (has uncommitted changes).
Option 1 is built and ready for user review in simulator.
Next step: User reviews Option 1, then implement Options 2-5 on separate branches.
Each new option branch should be created from master (not from option1).
Read the plan at Plans/063-settings-page-redesign.md for full details of all 5 options.
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
