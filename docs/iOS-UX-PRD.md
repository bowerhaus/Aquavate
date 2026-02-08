# Aquavate iOS App - UX Product Requirements Document

**Version:** 1.20
**Date:** 2026-01-30
**Status:** Approved (Import/Export)

**Changelog:**
- **v1.20 (2026-01-30):** Added Data import/export backup to Settings (Issue #93). New "Data" category row on main Settings page with DataSettingsPage sub-page. Export as versioned JSON, import with Merge (skip duplicates) or Replace (clear and restore) modes. ImportPreviewSheet for reviewing backup before import. See Section 2.6.
- **v1.19 (2026-01-29):** Settings page redesigned with Apple Settings-style sub-pages (Issue #87). Main page shows category rows with contextual summaries; drill-down to Goals & Health, Device Information, Device Controls, Reminder Options. Added BLE keep-alive, Health/Notification status flags. Removed error message display, unused status rows. See Section 2.6.
- **v1.18 (2026-01-28):** Simplified behind-target indicator to faded blue (Issue #81). Replaced amber/red gradient with 30% opacity blue. Removes visual distinction between urgency levels while keeping deficit text. See Section 2.9.
- **v1.17 (2026-01-27):** Bottle-driven calibration (Issue #30). iOS sends START/CANCEL commands, bottle runs state machine and broadcasts state changes, iOS mirrors with rich UI. Simplified to 4 screens: Welcome → Empty → Full → Complete. See Section 2.3.
- **v1.16 (2026-01-26):** Unified Sessions view in Activity Stats (Issue #74). Replaced confusing separate "Recent Motion Wakes" and "Backpack Sessions" sections with single chronological "Sessions" list. Summary changed from "Since Last Charge" to "Last 7 Days". See Section 2.7.
- **v1.15 (2026-01-25):** Increased notification threshold from 50ml to 150ml behind pace (Issue #67). Early Notifications toggle (DEBUG only) lowers threshold to 50ml. See Section 7.
- **v1.14 (2026-01-25):** Updated "Bottle is Asleep" alert message to reflect single-tap wake capability (Issue #63). Message now says "Tap or tilt your bottle to wake it up". See Section 2.4.
- **v1.13 (2026-01-25):** Bottle level now shows last known value with "(recent)" indicator when disconnected (Issue #57). Section is hidden until first connection. See Section 2.4.
- **v1.12 (2026-01-24):** Added Retry/Cancel buttons to "Bottle is Asleep" alert (Issue #52). Users can now tap Retry after waking bottle instead of manually pulling down again. See Section 2.4.
- **v1.11 (2026-01-24):** Three-color stacked fill for human figure (Issue #50). When behind target, shows orange for deficit up to 20%, red for deficit beyond 20%. See Section 2.9.
- **v1.10 (2026-01-24):** Activity Stats now persist in CoreData (Issue #36 Comment). Users can view cached data when disconnected with "Last synced X ago" timestamp. Diagnostics section accessible when disconnected.
- **v1.9 (2026-01-24):** Added Hydration Reminders with pace-based urgency model (Issue #27). Added Apple Watch companion app with complications. Added target intake visualization on HomeView. See Section 2.8 (Watch App) and Section 7 (Notification Strategy).
- **v1.8 (2026-01-23):** Added Diagnostics section to Settings with Activity Stats view (Issue #36). Shows motion wake events and backpack sessions for battery analysis. Includes "drink taken" indicator (water drop icon) for wakes where user took a drink.
- **v1.7 (2026-01-23):** Added Gestures section to Settings with Shake-to-Empty toggle. Setting syncs to firmware via BLE Device Settings characteristic.
- **v1.6 (2026-01-22):** Settings page cleanup - replaced static "Name" with live "Device" showing connected device name, removed unused "Use Ounces" toggle, removed Version row from About section.
- **v1.5 (2026-01-21):** Added Apple HealthKit integration (Section 2.7). Drinks sync to Health app as water intake samples.
- **v1.4 (2026-01-21):** Bidirectional drink record sync. Swipe-to-delete now requires bottle connection and uses pessimistic delete with firmware confirmation. HomeView shows ALL today's drinks (not just recent 5).
- **v1.3 (2026-01-20):** Added swipe-to-delete for drink records (Section 4 Gestures). Updated Reset Daily to also clear today's CoreData records.
- **v1.2 (2026-01-18):** Added Pull-to-Refresh sync for Home screen (Section 2.4). Connection stays open 60s for real-time updates. Settings connection controls wrapped in `#if DEBUG` (Section 2.6).
- **v1.1 (2026-01-17):** Updated Calibration Wizard (Section 2.3) - iOS now performs two-point calibration instead of firmware. Added live ADC readings, stability indicators, and BLE integration details.
- **v1.0 (2026-01-16):** Initial approved version

---

## 1. User Personas & Scenarios

### Primary Persona: Sarah

**Demographics:**
- Age: 32, marketing manager
- Tech comfort: High (iPhone user, uses fitness apps)
- Daily routine: Office-based, busy schedule with meetings

**Hydration Patterns:**
- Knows she should drink more water but forgets during busy periods
- Currently uses a 750ml reusable bottle
- Wants to track intake without manual logging
- Goals: 2L per day minimum

**Pain Points:**
- Forgets to drink during back-to-back meetings
- Loses track of how much she's consumed
- Finds manual logging tedious and abandons it

**Key Goals:**
- Automatic tracking that "just works"
- Quick glance at daily progress
- Simple setup, minimal maintenance

---

### Key Usage Scenarios

#### Scenario 1: First-Time Setup (Unboxing → Paired → Calibrated)
Sarah unboxes her Aquavate puck, places it on her desk, and opens the iOS app for the first time. The app guides her through pairing with her device and calibrating for her specific bottle. Within 5 minutes, she's ready to start tracking.

#### Scenario 2: Daily Usage (Morning → Evening)
Sarah picks up her bottle throughout the day. Each time she sets it down after drinking, the puck detects the change. When she opens the app during a break, she sees her current progress without having to do anything.

#### Scenario 3: Weekly Review
On Sunday evening, Sarah opens the History tab to see how she did during the week. She notices she consistently drinks less on meeting-heavy days and decides to set a reminder.

#### Scenario 4: Maintenance
Sarah's puck shows low battery. She charges it, and when reconnecting, the app automatically syncs any drinks recorded while disconnected.

#### Scenario 5: Error Recovery
Sarah's Bluetooth is accidentally turned off. When she opens the app, she sees a clear message explaining the issue and a button to fix it.

---

## 2. Screen-by-Screen Specifications

### 2.1 Splash Screen

**Purpose:** Brand introduction and app initialization

**Layout:**
```
┌─────────────────────────────────┐
│                                 │
│                                 │
│                                 │
│          💧                     │  ← SF Symbol: drop.fill (64pt)
│                                 │
│        Aquavate                 │  ← Large Title (34pt Bold)
│                                 │
│                                 │
│                                 │
└─────────────────────────────────┘
```

**Behavior:**
- Display for 2 seconds with fade-in animation
- Transition: Cross-fade to main content
- If no bottle paired: Transition to Pairing Screen
- If bottle paired: Transition to Home Screen (TabView)

**Animation:**
- Icon: Scale 0.8 → 1.0 with opacity 0 → 1 over 0.5s
- Text: Fade in 0.3s after icon completes

---

### 2.2 Pairing Screen

**Purpose:** Connect to an Aquavate device for the first time

**Layout:**
```
┌─────────────────────────────────┐
│                                 │
│      💧 Aquavate               │  ← Header with icon
│                                 │
│   ┌─────────────────────────┐   │
│   │                         │   │
│   │    [Bottle Graphic]     │   │  ← Illustration of puck + bottle
│   │                         │   │
│   └─────────────────────────┘   │
│                                 │
│   Welcome to Aquavate!          │  ← Headline (17pt Semibold)
│                                 │
│   Place your bottle on the      │
│   puck and tap below to pair.   │  ← Body text (17pt Regular)
│                                 │
│   ┌─────────────────────────┐   │
│   │   Pair Your Bottle      │   │  ← Primary button (filled blue)
│   └─────────────────────────┘   │
│                                 │
└─────────────────────────────────┘
```

**User Actions:**
- Tap "Pair Your Bottle" → Opens scanning sheet

**Scanning Sheet (Modal):**
```
┌─────────────────────────────────┐
│  ─────                          │  ← Drag indicator
│                                 │
│   Looking for devices...        │  ← Headline with spinner
│                                 │
│   Make sure your Aquavate       │
│   puck is nearby and the        │
│   bottle is on top.             │
│                                 │
│   ┌─────────────────────────┐   │
│   │ ○ Aquavate-1A2B         │   │  ← Device found (tap to connect)
│   └─────────────────────────┘   │
│                                 │
│   [Cancel]                      │  ← Secondary action
│                                 │
└─────────────────────────────────┘
```

**State Variations:**

| State | Behavior |
|-------|----------|
| Scanning | Spinner + "Looking for devices..." |
| Device Found | Device appears in list with signal indicator |
| Connecting | "Connecting to Aquavate-1A2B..." with spinner |
| Connected | Dismiss sheet, proceed to calibration |
| No Devices (30s) | "No devices found" + Retry button |
| BLE Off | "Turn on Bluetooth" banner + Settings link |
| Timeout | "Connection timed out" alert + Retry |

---

### 2.3 Calibration Wizard (Modal) - UPDATED 2026-01-27

**Purpose:** Guide user through two-point calibration to establish bottle's scale factor

**Context:** Bottle-driven calibration with iOS mirroring. iOS sends START/CANCEL commands, bottle runs its existing standalone calibration state machine, and broadcasts state changes via BLE notifications. iOS mirrors the bottle state with a rich UI. The simplified flow has 4 screens: Welcome → Empty → Full → Complete.

**Presentation:** Full-screen modal sheet (not dismissible by swipe during calibration)

**Architecture:**
```
┌─────────────────┐                    ┌─────────────────┐
│    iOS App      │                    │     Bottle      │
├─────────────────┤                    ├─────────────────┤
│                 │  START_CALIBRATION │                 │
│  "Calibrate"    │ ──────────────────>│ Enters calib    │
│   button tap    │                    │ state machine   │
│                 │                    │                 │
│                 │  STATE_NOTIFY      │ Detects stable  │
│  Updates rich   │ <─────────────────-│ upright, runs   │
│  UI to mirror   │  (state + weights) │ existing logic  │
│  bottle state   │                    │                 │
│                 │  CANCEL_CALIB      │                 │
│  Cancel button  │ ──────────────────>│ Returns to idle │
└─────────────────┘                    └─────────────────┘
```

**Welcome Screen:**
```
┌─────────────────────────────────┐
│  [Cancel]                       │
│                                 │
│        🍶                       │  ← Bottle icon (empty)
│                                 │
│   Calibrate Your Bottle         │  ← Headline (22pt Bold)
│                                 │
│   Calibration teaches your      │
│   bottle to measure water       │  ← Body text (17pt Regular)
│   accurately. You'll weigh it   │
│   empty, then full with a       │
│   known amount.                 │
│                                 │
│   ┌─────────────────────────┐   │
│   │   Start Calibration     │   │  ← Primary CTA
│   └─────────────────────────┘   │
│                                 │
└─────────────────────────────────┘
```

**Step 1: Empty Bottle**
```
┌─────────────────────────────────┐
│  [Cancel]     ● ─ ○             │  ← Progress indicator (step 1 of 2)
│                                 │
│        🍶                       │  ← Bottle icon (empty)
│                                 │
│   Step 1: Empty Bottle          │  ← Headline
│                                 │
│   1. Empty your bottle          │
│      completely and put         │  ← Instructions
│      the cap on                 │
│   2. Set the bottle upright     │
│      on a flat surface          │
│                                 │
│   Weight: 245g ●●●●○            │  ← Stability indicator
│   Waiting for stable            │  ← Status message
│   measurement...                │
│                                 │
└─────────────────────────────────┘
```

*Note: When measuring, shows progress bar instead of stability indicator*

**Step 2: Full Bottle**
```
┌─────────────────────────────────┐
│  [Cancel]     ● ─ ●             │  ← Progress indicator (step 2 of 2)
│                                 │
│        🍶💧                     │  ← Bottle icon (filled)
│                                 │
│   Step 2: Full Bottle           │  ← Headline
│                                 │
│   1. Fill the bottle with       │
│      exactly 830ml of water     │  ← Instructions
│   2. Put the cap on and set     │
│      it upright on a flat       │
│      surface                    │
│                                 │
│   Weight: 1075g ●●●●●           │  ← Stability indicator
│   Waiting for stable            │  ← Status message
│   measurement...                │
│                                 │
└─────────────────────────────────┘
```

**Success Screen:**
```
┌─────────────────────────────────┐
│                                 │
│           ✓                     │  ← Large checkmark (green, 72pt)
│                                 │
│   Calibration Complete!         │  ← Headline
│                                 │
│   Your bottle is now            │
│   calibrated and ready for      │
│   accurate drink tracking.      │
│                                 │
│   ┌─────────────────────────┐   │
│   │        Done             │   │  ← Primary CTA
│   └─────────────────────────┘   │
│                                 │
└─────────────────────────────────┘
```

**Data Flow:**
1. **Start:** iOS sends START_CALIBRATION (0x20) command
2. **Step 1:** Bottle broadcasts `CAL_WAIT_EMPTY` → `CAL_MEASURE_EMPTY` → captures emptyADC
3. **Step 2:** Bottle broadcasts `CAL_WAIT_FULL` → `CAL_MEASURE_FULL` → captures fullADC
4. **Complete:** Bottle calculates scale factor, stores to NVS, broadcasts `CAL_COMPLETE`
5. iOS receives final state with emptyADC, fullADC, shows success

**State Notifications (Bottle → iOS):**
| State | Value | iOS Action |
|-------|-------|------------|
| `CAL_IDLE` | 0 | Show Welcome screen |
| `CAL_WAIT_EMPTY` | 3 | Show Step 1 with stability indicator |
| `CAL_MEASURE_EMPTY` | 4 | Show Step 1 with measuring progress |
| `CAL_WAIT_FULL` | 6 | Show Step 2 with stability indicator |
| `CAL_MEASURE_FULL` | 7 | Show Step 2 with measuring progress |
| `CAL_COMPLETE` | 9 | Show Success screen |
| `CAL_ERROR` | 10 | Show error with retry option |

**Completion:**
- Haptic: Success (light impact)
- Dismiss modal → Navigate to Home Screen
- Bottle has already saved calibration to NVS

**Edge Cases:**
| Scenario | Behavior |
|----------|----------|
| Weight not stable | Stability indicator shows progress, bottle auto-measures when stable |
| BLE disconnect during calibration | Alert: "Connection lost" → return to Settings |
| User taps Cancel | iOS sends CANCEL_CALIBRATION (0x21), bottle returns to idle |
| Calibration error | Bottle broadcasts CAL_ERROR, iOS shows retry option |
| Full < Empty ADC | Bottle broadcasts CAL_ERROR, iOS shows "Fill bottle properly" |

---

### 2.4 Home Screen (Main Tab)

**Purpose:** At-a-glance view of daily progress toward hydration goal (PRIMARY) and current bottle level (SECONDARY)

**Layout:**
```
┌─────────────────────────────────┐
│  Aquavate              ● ───┐   │  ← Navigation title + connection dot
│                             │   │
│   ↻ Last synced 5 min ago   │   │  ← Sync status (caption)
│                                 │
│       ┌───────────────┐         │
│      ╱                 ╲        │
│     │                   │       │
│     │     1,200 ml      │       │  ← Large text (28pt Bold) - DAILY TOTAL
│     │  of 2,000ml goal  │       │  ← Subtext (15pt Regular)
│     │                   │       │
│      ╲                 ╱        │
│       └───────────────┘         │  ← Circular progress ring (200pt) - PRIMARY
│                                 │
│   ┌─────────────────────────┐   │
│   │ Bottle Level        56% │   │
│   │ ████████░░░░░░░░░░░░░░  │   │  ← Progress bar - SECONDARY
│   │ 420ml / 750ml           │   │
│   └─────────────────────────┘   │
│                                 │
│   ─────────────────────────────│
│                                 │
│   Recent Drinks                 │  ← Section header
│                                 │
│   💧 200ml      2:30 PM  420ml │  ← DrinkListItem (amount emphasized)
│   ─────────────────────────────│
│   💧 150ml     11:45 AM  620ml │
│   ─────────────────────────────│
│   💧 250ml      9:15 AM  770ml │
│                                 │
└─────────────────────────────────┘
│  🏠 Home  │  📊 History  │  ⚙️  │  ← Tab bar
└─────────────────────────────────┘
```

**Data Displayed:**
| Element | Source | Format |
|---------|--------|--------|
| Daily total (PRIMARY) | CoreData sum (always) | "{X} ml of {goal}ml goal" |
| Bottle level (SECONDARY) | BLE Current State (persisted) | "{X}ml / {capacity}ml" + optional "(recent)" |
| Today's drinks | CoreData (ALL today's drinks) | Amount (bold), time, level after |
| Sync status | Last BLE sync timestamp | "Last synced {X} ago" |
| Connection dot | BLE connection state | Green/Orange/Gray |

**Bottle Level Display States (Added 2026-01-25):**
| State | Display |
|-------|---------|
| Never connected | Section hidden entirely |
| Connected | Shows live value (e.g., "56%") |
| Disconnected with previous data | Shows last known value with "(recent)" suffix (e.g., "56% (recent)") |

**Pull-to-Refresh (Updated 2026-01-18):**
- If disconnected: Scans and connects to bottle, syncs, stays connected 60s
- If connected: Syncs, resets idle timer
- Shows refresh spinner until sync complete
- Connection stays open for 60 seconds for real-time updates
- Auto-disconnects after 60s idle (battery conservation)
- Disconnect after 5s delay when app goes to background (allows in-progress sync to complete)
- Persistent auto-reconnection via `CBCentralManager.connect()` in both foreground and background (Issue #114)
- When disconnected in foreground, app silently auto-connects when bottle advertises (no manual refresh needed)

**Pull-to-Refresh Alerts:**
- "Bottle is Asleep" alert if scan times out (~10s) with no devices found
  - Message: "Tap or tilt your bottle to wake it up, then hit Retry."
  - Buttons: **Retry** (triggers new connection attempt) | **Cancel** (dismisses alert)
- "Sync Error" alert if connection fails or sync interrupted
- "Bluetooth is turned off" error if Bluetooth unavailable

**State Variations:**

**Empty State (No drinks today):**
```
│   Recent Drinks                 │
│                                 │
│   💧                            │
│   No drinks recorded yet        │
│   Start drinking to see your    │
│   activity here!                │
```

**Disconnected State:**
- Connection dot: Gray
- Banner below nav: "Bottle disconnected" (orange background)
- Tap banner → Attempt reconnection

**Syncing State:**
- Banner: "Syncing 142 records..." with progress bar
- Progress: chunk_index / total_chunks

**Animations:**
- Bottle level ring: 0.5s ease-out on value change
- Daily progress bar: 0.3s ease-out on value change
- New drink appears: Fade in + slide from top (0.3s)

---

### 2.5 History Screen

**Purpose:** View historical intake data by day

**Layout:**
```
┌─────────────────────────────────┐
│  History                        │
│                                 │
│   January 2026                  │  ← Month header
│                                 │
│   ┌───┬───┬───┬───┬───┬───┬───┐ │
│   │Mon│Tue│Wed│Thu│Fri│Sat│Sun│ │
│   ├───┼───┼───┼───┼───┼───┼───┤ │
│   │13 │14 │15 │[16]│17 │18 │19 │ │  ← [16] = selected/today
│   │1.8│2.1│1.5│1.2│ - │ - │ - │ │  ← Daily totals (L)
│   │ ● │ ● │ ○ │ ○ │   │   │   │ │  ← Goal indicators (●=met ○=not)
│   └───┴───┴───┴───┴───┴───┴───┘ │
│                                 │
│   ─────────────────────────────│
│                                 │
│   Thursday, 16 January         │  ← Selected day header
│   1,200ml • 6 drinks           │  ← Summary
│                                 │
│   💧 2:30 PM    200ml   420ml  │
│   ─────────────────────────────│
│   💧 11:45 AM   150ml   620ml  │
│   ─────────────────────────────│
│   💧 9:15 AM    250ml   770ml  │
│   ─────────────────────────────│
│   💧 8:00 AM    180ml  1020ml  │
│   ─────────────────────────────│
│   ... (scrollable)             │
│                                 │
└─────────────────────────────────┘
```

**User Actions:**
- Swipe calendar left/right → Previous/next week
- Tap day → Shows that day's drink list below
- Pull-to-refresh → Syncs latest data

**Data Displayed:**
| Element | Source | Format |
|---------|--------|--------|
| Calendar grid | CoreData aggregation | 7-day view |
| Daily totals | Sum of drinks per day | "X.X" (liters) |
| Goal indicator | Daily total vs goal | ● (met) or ○ (not met) |
| Drink list | CoreData filtered by day | Full list, scrollable |

**Empty States:**

**No data for selected day:**
```
│   Thursday, 16 January         │
│                                 │
│   💧                            │
│   No drinks recorded            │
│   on this day                   │
```

**No history at all (new user):**
```
│   Your history will appear      │
│   here once you start drinking! │
```

---

### 2.6 Settings Screen (Redesigned 2026-01-29, Issue #87)

**Purpose:** Device configuration and app preferences

**Architecture:** Apple Settings-style with main page showing category rows with contextual summaries. Each category is a NavigationLink that drills down to a dedicated sub-page. Keep-alive prevents BLE idle disconnect while Settings is open.

**Main Page Layout:**
```
┌─────────────────────────────────┐
│  Settings                       │
│                                 │
│  DEVICE STATUS                  │
│  ┌─────────────────────────────┐│
│  │ 📶 ● Aquavate-A3F2    🔋85%││  ← Connection dot + battery
│  ├─────────────────────────────┤│
│  │ ℹ️ Some options require     ││  ← Info banner (when disconnected)
│  │    connection to the bottle ││
│  │    to be displayed. Pull to ││
│  │    refresh on Home to       ││
│  │    connect.                 ││
│  └─────────────────────────────┘│
│                                 │
│  ┌─────────────────────────────┐│
│  │ 🎯 Goals & Health       →  ││  ← NavigationLink
│  │    2000ml goal · Reminders  ││  ← Contextual summary
│  │    on · Health sync on      ││
│  ├─────────────────────────────┤│
│  │ ⚙️ Device Information   →  ││
│  │    Calibrated               ││  ← Or "Not calibrated" (orange)
│  ├─────────────────────────────┤│
│  │ 🎚 Device Controls      →  ││
│  │    Tare, reset, sync,      ││  ← Or "Requires connection"
│  │    gestures                 ││
│  ├─────────────────────────────┤│
│  │ 🔔 Reminder Options     →  ││  ← Only when reminders enabled
│  │    3/10 sent today          ││
│  ├─────────────────────────────┤│
│  │ 💾 Data                 →  ││
│  │    42 drink records         ││  ← Record count summary
│  └─────────────────────────────┘│
│                                 │
│  ABOUT                          │
│  ┌─────────────────────────────┐│
│  │ 🔗 GitHub Repository    →  ││
│  └─────────────────────────────┘│
│                                 │
└─────────────────────────────────┘
```

**Sub-Pages:**

**Goals & Health** (`GoalsSettingsPage`):
- **Daily Goal** section — Tappable row opens wheel picker sheet (1000–4000ml in 250ml steps). Disabled when disconnected.
- **Notifications** section — Hydration Reminders toggle + Notification Status (Authorized / Not Authorized)
- **Health** section — Sync to Health toggle + Health Status (Authorized / Not Authorized). Shows "HealthKit not available" on iPad.

**Device Information** (`DeviceInformationPage`):
- **Calibration** section — Calibrated status (Yes/No) + Calibrate Bottle link (opens CalibrationViewWrapper). Disabled + dimmed when disconnected with footer hint.
- **Diagnostics** section — Sleep Mode Analysis (ActivityStatsView). Works offline.

**Device Controls** (`DeviceControlsPage`):
- **Commands** section — Tare (Zero Scale), Reset Daily Total, Sync Drink History (conditional, when unsynced records exist), Clear Device History (destructive, with confirmation alert). All disabled when disconnected with footer hint.
- **Gestures** section — Shake to Empty toggle with description. Disabled when disconnected.

**Reminder Options** (`ReminderOptionsPage`) — only accessible when reminders enabled:
- Reminders Today count (with limit if enabled)
- Limit Daily Reminders toggle
- Earlier Reminders toggle
- Back On Track Alerts toggle

**Data** (`DataSettingsPage`):
- **Your Data** section — Count rows for bottles, drink records, wake events, backpack sessions (with icons)
- **Export** section — "Export Backup" button exports all app data as versioned JSON (`Aquavate-Backup-YYYY-MM-DD.json`). Opens share sheet via `UIActivityViewController`. Footer explains what's included.
- **Import** section — "Import Backup" button opens `.fileImporter` for JSON files. Shows `ImportPreviewSheet` with backup info (date, version, content counts) and two import modes:
  - **Merge** — Adds new records, skips duplicates (by UUID)
  - **Replace All** — Destructive (confirmation alert), clears all data before importing
- **Format:** Versioned JSON with `formatVersion: 1`, includes bottles, drink records, wake events, backpack sessions, and app settings (daily goal, notification preferences, HealthKit sync). Excludes device-specific fields (`peripheralIdentifier`, `batteryPercent`, etc.).

**Command Actions:**

| Command | Tap Behavior | Confirmation |
|---------|--------------|--------------|
| Calibrate Bottle | Opens Calibration Wizard modal | None (wizard has own confirm flow) |
| Tare (Zero Scale) | Sends TARE_NOW (0x01) | None (instant) |
| Reset Daily Total | Sends RESET_DAILY (0x05) + deletes today's CoreData records | None (instant) |
| Clear Device History | Sends CLEAR_HISTORY (0x06) | Alert: "This will permanently delete all drink history stored on the device. This cannot be undone." |
| Sync Drink History | Starts drink sync | None (instant) |

**Contextual Summaries (main page):**

| Category | Connected | Disconnected |
|----------|-----------|--------------|
| Goals & Health | "2000ml goal · Reminders on · Health sync on" | Same (works offline) |
| Device Information | "Calibrated" or "Not calibrated" (orange) | "Calibration, sleep analysis" |
| Device Controls | "Tare, reset, sync, gestures" or "3 unsynced records" | "Requires connection" |
| Reminder Options | "3/10 sent today" or "3 sent today" | Same (works offline) |
| Data | "42 drink records" (count of drink records) | Same (works offline) |

**Connection Behavior:**
- Keep-alive via `beginKeepAlive()` / `endKeepAlive()` API prevents idle disconnect while Settings is open
- Periodic BLE pings (30s) keep firmware awake
- When app enters background, keep-alive suspends so normal 5s background disconnect proceeds
- When app returns to foreground with Settings still open, keep-alive resumes

**Removed Items (Issue #87):**
The following were removed from the settings UI. Underlying code remains for internal use:
- Sync Time command (auto-syncs on connect), Current Weight, Device Name, Bottle Capacity, Time Set flag, Last Synced, Unsynced Records count, Syncing progress, Current Status (hydration urgency), Danger Zone section header, Debug section (scan/connect/disconnect/cancel), Error message display in Device Status

**Apple Health Integration:**

| Element | Behavior |
|---------|----------|
| Toggle "Sync to Health" | When enabled, triggers iOS HealthKit authorization prompt |
| Health Status row | Shows green checkmark "Authorized" or orange "Not Authorized" |
| iPad handling | Section shows "HealthKit not available on this device" (gray text) |

**Sync Behavior:**
- Each drink synced from bottle creates a water intake sample in HealthKit
- Samples include accurate timestamp from drink record
- Deleting a drink in Aquavate removes the corresponding HealthKit sample
- Drinks are marked with `syncedToHealth` flag to prevent duplicates
- HealthKit sample UUID stored for deletion support

**Day Boundary Note:**
Aquavate uses **midnight** as the daily reset, matching Apple Health. Daily totals align between both systems.

---

### 2.7 Activity Stats View (Issue #36, updated Issue #74)

**Purpose:** Diagnostic view for analyzing bottle activity and sleep mode behavior for battery analysis

**Access:** Settings → Diagnostics → Activity Stats

**Layout (Updated 2026-01-26 - Unified Sessions):**
```
┌─────────────────────────────────┐
│  < Settings    Activity Stats   │
│                                 │
│  LAST 7 DAYS                    │
│  ┌─────────────────────────────┐│
│  │ ✋ Normal Sessions      38  ││
│  ├─────────────────────────────┤│
│  │ 🎒 Backpack Sessions     3  ││
│  └─────────────────────────────┘│
│                                 │
│  SESSIONS                       │  ← Unified list (both types)
│  ┌─────────────────────────────┐│
│  │ 2:30 PM  💧         45s     ││  ← Normal session (water drop = drink)
│  │ 26 Jan 2026        Normal   ││
│  ├─────────────────────────────┤│
│  │ 11:15 AM           1h 23m   ││  ← Backpack session (orange text)
│  │ 26 Jan 2026     🎒 Backpack ││
│  ├─────────────────────────────┤│
│  │ 9:00 AM             32s     ││  ← Normal session (no drink)
│  │ 26 Jan 2026        Normal   ││
│  └─────────────────────────────┘│
│                                 │
└─────────────────────────────────┘
```

**Data Displayed:**

| Element | Source | Description |
|---------|--------|-------------|
| Normal Sessions (7d) | CoreData count | Normal sessions in last 7 days |
| Backpack Sessions (7d) | CoreData count | Backpack sessions in last 7 days |
| Sessions List | CoreData (merged) | Both types interleaved chronologically, most recent first |

**Session Row Display:**

| Session Type | Timestamp | Duration | Badge | Drink Indicator |
|--------------|-----------|----------|-------|-----------------|
| Normal | Time + Date | Seconds/minutes | "Normal" (gray) | 💧 if drink taken |
| Backpack | Time + Date | Hours/minutes | "🎒 Backpack (N)" (orange) | N/A |

**Design Rationale (Issue #74):**
- Previous layout had separate "Recent Motion Wakes" and "Backpack Sessions" sections
- Users confused short wake durations (3m 26s) labeled "Backpack" for backpack duration
- Unified chronological list makes session flow clearer
- Backpack sessions now show correct 1+ hour durations prominently

**Drink Taken Indicator:**
- Blue water drop icon (`drop.fill`) shown next to Normal sessions where user took a drink
- No icon shown for Normal sessions without a drink
- Not applicable to Backpack sessions

**Behavior:**
- **Lazy loading:** Data fetched only when view appears (not on app launch)
- **Pull-to-refresh:** Re-fetches all activity data (requires connection)
- **Disconnected state:** Shows cached data from CoreData with "Last synced X ago" timestamp
- **Loading state:** Progress indicator while fetching chunks
- **Persistence:** Data stored in CoreData entities (CDMotionWakeEvent, CDBackpackSession)

**Edge Cases:**

| Scenario | Behavior |
|----------|----------|
| Not connected (with cached data) | Shows cached data with "Last synced X ago" timestamp |
| Not connected (no cached data) | Shows "No activity data. Connect to bottle to sync." |
| No activity data (connected) | Shows "No activity recorded since last charge" |
| Fetch error | Shows error message with retry option |
| In backpack mode | Shows current session start time and timer wake count |

---

### 2.8 Apple Watch App (Issue #27)

**Purpose:** Companion app showing hydration status with pace-based deficit tracking and complications

**App Structure:**
```
ios/Aquavate/
  AquavateWatch Watch App/
    AquavateWatch/
      AquavateWatchApp.swift
      ContentView.swift
      ComplicationProvider.swift
      WatchSessionManager.swift
      WatchNotificationManager.swift
```

**Watch Home View:**
```
┌─────────────────────────────────┐
│                                 │
│           💧                    │  ← Large water drop (colored by urgency)
│                                 │
│       1.2L / 2.5L               │  ← Daily progress
│                                 │
│     367ml to catch up           │  ← Status text (or "On track ✓")
│                                 │
└─────────────────────────────────┘
```

**Status Text Variants:**

| Condition | Display |
|-----------|---------|
| Deficit ≥ 50ml | "{deficit}ml to catch up" |
| Deficit < 50ml | "On track ✓" |
| Goal achieved | "Goal reached! 🎉" |

**Urgency Colors:**

| Urgency | Color | Condition |
|---------|-------|-----------|
| On Track | Blue | deficit ≤ 0 |
| Attention | Amber | 0 < deficit < 20% of goal |
| Overdue | Red | deficit ≥ 20% of goal |

**Complications:**
- `graphicCircular` - Small water drop with progress ring
- `graphicCorner` - Larger drop with percentage

**Data Sync:**
- Uses WatchConnectivity framework
- iPhone sends state via `updateApplicationContext()` and `transferUserInfo()`
- Updates on: app launch, BLE drink sync, periodic timer (60s)

**Notifications:**
- iPhone triggers local notifications on Watch via `transferUserInfo()`
- Watch schedules notification + plays haptic
- Works regardless of iPhone lock state

---

### 2.9 HomeView Target Visualization (Issue #27, updated Issue #81)

**Purpose:** Visual pace tracking showing expected vs actual progress

**Layout (HumanFigureProgressView):**
```
┌─────────────────────────────────┐
│                                 │
│      [Human figure graphic]     │
│      ┌─────────────────────┐    │
│      │  ████████░░░░░░░░░  │    │  ← Blue fill = actual
│      │  ████████████░░░░░  │    │  ← Faded blue (30% opacity) = deficit
│      └─────────────────────┘    │
│                                 │
│      250 ml behind target       │  ← Text (secondary color)
│                                 │
└─────────────────────────────────┘
```

**Visual Behavior:**

| State | Visualization |
|-------|--------------|
| On track | Blue fill only (actual progress) |
| Behind target | Blue fill (actual) + faded blue at 30% opacity (deficit zone from actual to expected) |

**Text Display:**
- Shows "X ml behind target" when rounded deficit ≥ 50ml
- Text uses secondary color (neutral, not urgency-colored)
- Hidden when on track or deficit < 50ml

**Note:** Urgency levels (attention/overdue) are still calculated for notification purposes but are not visually distinguished in the human figure display.

---

## 3. User Flows (Critical Paths)

### Flow 1: First-Time Setup

```
App Launch
    │
    ▼
Splash Screen (2s)
    │
    ▼
[No Bottle Paired?] ──Yes──► Pairing Screen
    │                            │
    No                           ▼
    │                       Tap "Pair Your Bottle"
    ▼                            │
Home Screen                      ▼
                            Scanning Sheet
                                 │
                            Device Found
                                 │
                                 ▼
                            Tap Device
                                 │
                                 ▼
                            "Connecting..."
                                 │
                                 ▼
                            Connected ✓
                                 │
                                 ▼
                            Calibration Wizard
                                 │
                            ┌────┴────┐
                            ▼         ▼
                        Step 1     Step 2
                        (Tare)   (Capacity)
                            │         │
                            └────┬────┘
                                 │
                                 ▼
                            Setup Complete
                                 │
                                 ▼
                            Home Screen (Connected)
```

**Time to Complete:** ~3-5 minutes

---

### Flow 2: Daily Usage (Updated 2026-01-18)

```
Morning:
    │
    ▼
Pick Up Bottle ──► Puck wakes, starts advertising
    │
    ▼
User opens app (disconnected state)
    │
    ▼
Pull down to refresh
    │
    ▼
App scans, connects, syncs
    │
    ▼
Home Screen shows current state
    │
    ├── Bottle level: 750ml (full)
    ├── Daily total: 0ml
    └── Connection: ● Connected (60s idle timer)

Throughout Day:
    │
    ▼
Drink 200ml → Set down bottle
    │
    ▼
Puck detects weight change
    │
    ▼
Updates daily total (on device)
    │
    ▼
Sends BLE notification
    │
    ▼
If app open + connected: UI updates in real-time
If app closed/disconnected: Updates on next pull-to-refresh

Evening:
    │
    ▼
Open app → History tab
    │
    ▼
View today's drinks (6 records)
    │
    ▼
Review weekly patterns
```

---

### Flow 3: Error Recovery (BLE Off)

```
User opens app
    │
    ▼
BLEManager checks CBCentralManager state
    │
    ▼
state == .poweredOff
    │
    ▼
Home Screen displays:
    ├── Gray connection dot
    ├── Banner: "Bluetooth is off"
    └── Button: "Turn On Bluetooth"
           │
           ▼
       Tap button
           │
           ▼
       Open iOS Settings (deep link)
           │
           ▼
       User enables Bluetooth
           │
           ▼
       Return to app
           │
           ▼
       state == .poweredOn
           │
           ▼
       Auto-scan for known device
           │
           ▼
       Connect → Sync → Ready
           │
           ▼
       Green dot + "Connected"
```

---

### Flow 4: Sync Recovery (Mid-Sync Disconnect)

```
Syncing in progress
    │
    ▼
"Syncing 142 records..." (chunk 3/10)
    │
    ▼
Connection lost
    │
    ▼
BLEManager saves partial records (60 synced)
    │
    ▼
Home Screen shows:
    ├── Orange dot
    ├── Banner: "Sync incomplete: 82 records remaining"
    └── Button: "Retry"
           │
           ▼
       Tap "Retry"
           │
           ▼
       Reconnect attempt
           │
           ▼
       Connection restored
           │
           ▼
       Query unsynced count (82)
           │
           ▼
       Resume sync
           │
           ▼
       "Syncing 82 records..."
           │
           ▼
       Complete → Green dot + "Synced"
```

---

### Flow 5: Calibration Wizard (First-Time or Recalibration)

```
Settings Screen
    │
    ▼
Tap "Calibrate Bottle"
    │
    ▼
Calibration Wizard Modal
    │
    ▼
Step 1: Empty Measurement
    │
    ├─► Subscribe to BLE Current State
    │
    ├─► Display live ADC reading
    │
    ├─► Monitor stability (isStable flag)
    │       │
    │       ▼
    │   [Stable for 2s?]
    │       ├── No → Show "Hold still..." (1-4 dots)
    │       │         CTA disabled
    │       │
    │       └── Yes → Fill all 5 dots
    │                 Enable CTA (pulsing blue)
    │                 Haptic feedback
    │
    ▼
Tap "Tap When Stable"
    │
    ├─► Save emptyADC = currentWeightG
    │
    ▼
Step 2: Full Measurement
    │
    ├─► Continue reading BLE Current State
    │
    ├─► Display live ADC reading
    │
    ├─► Wait for stability (same as Step 1)
    │
    ▼
Tap "Tap When Stable"
    │
    ├─► Save fullADC = currentWeightG
    │
    ▼
Calculate Scale Factor
    │
    ├─► scaleFactor = (fullADC - emptyADC) / 830.0
    │
    ▼
[Valid Scale Factor?] (5-15 ADC/g)
    │
    ├── No → Alert: "Calibration failed. Try again."
    │         Return to Step 1
    │
    └── Yes → Show Success Screen
              │
              ▼
          Write to Firmware
              │
              ├─► BLE Bottle Config characteristic
              ├─► scaleFactor, emptyADC, capacity=830, goal=2400
              │
              ▼
          [Write Success?]
              │
              ├── No → Alert: "Failed to save. Please retry."
              │
              └── Yes → Banner: "Calibration complete ✓"
                        Dismiss modal
                        Return to Settings
                        (calibrated flag now true)
```

**Alternative Flow: Tare Only (Quick Recalibration)**
```
Settings Screen
    │
    ▼
Tap "Tare Bottle"
    │
    ▼
Command sent to puck (TARE_NOW 0x01)
    │
    ├─► Updates empty baseline to current weight
    ├─► Keeps existing scale factor
    │
    ▼
[Weight Stable?]
    │
    ├── Yes → Banner: "Bottle tared ✓"
    │
    └── No → Banner: "Hold bottle still and try again"
```

---

## 4. Interaction Patterns

### Navigation Model

| Component | Pattern | Notes |
|-----------|---------|-------|
| Main navigation | TabView (bottom tabs) | Home, History, Settings |
| Pairing | Full-screen replacement | Only shown when no bottle paired |
| Scanning | Modal sheet (medium) | Draggable, dismissible |
| Calibration | Full-screen modal | Not dismissible during calibration |
| Alerts | System alert | Confirmations, errors |
| Banners | In-app notification | Connection, sync status |

### Gestures

| Gesture | Location | Action |
|---------|----------|--------|
| Tap | Buttons, list items, tabs | Primary interaction |
| Pull-to-refresh | Home, History | Trigger manual sync |
| Swipe horizontal | History calendar | Navigate weeks |
| Swipe left | Drink list items | Delete drink record (requires connection) |
| Long-press | Not used (MVP) | Reserved for future |

### Swipe-to-Delete (Drink Records) - Updated 2026-01-21

**Purpose:** Allow users to remove individual drink records with bidirectional sync to firmware

**Availability:**
- HomeView: Today's Drinks list
- HistoryView: Selected day's drink list

**Behavior (Bidirectional Sync):**
- Standard iOS swipe-left gesture reveals red "Delete" button
- **Requires bottle connection** - if disconnected, shows "Connection Required" alert
- Uses **pessimistic delete** - waits for firmware confirmation before removing from CoreData
- Firmware soft-deletes record (sets 0x04 flag) and recalculates daily total
- 5-second timeout - if no confirmation, record stays in place (user can retry)
- Both iOS and firmware now have consistent view of deleted records

**Visual:**
```
┌───────────────────────────────┐
│  💧  200ml        420ml       │ ← Swipe left
│      2:30 PM      remaining   │
└───────────────────────────────┘
                      ┌─────────┐
                      │ Delete  │ ← Red background
                      └─────────┘
```

**Connection Required Alert:**
```
┌─────────────────────────────────┐
│       Connection Required       │
│                                 │
│  Please connect to your bottle  │
│  before deleting drinks. This   │
│  ensures both the app and       │
│  bottle stay in sync.           │
│                                 │
│           [OK]                  │
└─────────────────────────────────┘
```

**Edge Cases:**
| Scenario | Behavior |
|----------|----------|
| Bottle disconnected | Shows "Connection Required" alert |
| Delete timeout (5s) | Record stays in place, user can retry |
| Record rolled off circular buffer | Firmware returns success (effectively deleted) |
| Old record (no firmwareRecordId) | Deletes locally only (backwards compatibility) |

### Transitions

| Transition | Animation | Duration |
|------------|-----------|----------|
| Tab switch | Cross-fade | 0.25s (system) |
| Modal present | Slide up + dim | 0.3s ease-out |
| Modal dismiss | Slide down | 0.3s ease-in |
| Alert present | Scale + fade | 0.2s (system) |
| Banner show | Slide from top | 0.3s ease-out |
| Banner dismiss | Fade out | 0.2s ease-in |

### Feedback

| Event | Haptic | Visual |
|-------|--------|--------|
| Button tap | Soft impact | Scale 0.95 briefly |
| Success action | Light impact | Green banner/checkmark |
| Error | Notification | Red banner/alert |
| Goal achieved | Success | Confetti animation (optional) |
| Connection change | Selection | Dot color transition |

---

## 5. Visual Design Guidelines

### Color Palette

| Name | Hex | Usage |
|------|-----|-------|
| Primary | `#007AFF` | Buttons, links, active states |
| Success | `#34C759` | Connected, goal met, confirmations |
| Warning | `#FF9500` | Low battery (10-20%), partial sync |
| Error | `#FF3B30` | Disconnected, failures, destructive |
| Neutral | `#8E8E93` | Secondary text, disabled, placeholders |
| Background | System | Adaptive (white/black for dark mode) |

### Typography (SF Pro)

| Style | Size/Weight | Usage |
|-------|-------------|-------|
| Large Title | 34pt Bold | Screen titles (Home, History, Settings) |
| Title 2 | 22pt Bold | Section headers in modals |
| Headline | 17pt Semibold | Section headers, emphasis |
| Body | 17pt Regular | Primary body text |
| Callout | 16pt Regular | List item primary text |
| Subheadline | 15pt Regular | Supporting text |
| Caption 1 | 12pt Regular | Timestamps, metadata |
| Caption 2 | 11pt Regular | Very small labels |

### Spacing (8pt Grid)

| Element | Value |
|---------|-------|
| Screen horizontal padding | 16pt |
| Screen vertical padding | 20pt |
| Section spacing | 24pt |
| Component spacing | 16pt |
| List item vertical padding | 12pt |
| Minimum button height | 50pt |
| Minimum touch target | 44×44pt |

### Icons (SF Symbols 5.0)

| Purpose | Symbol | Weight |
|---------|--------|--------|
| Water/drinks | `drop.fill` | Regular |
| Battery | `battery.100` (dynamic) | Regular |
| BLE connection | `antenna.radiowaves.left.and.right` | Regular |
| History | `chart.bar.fill` | Regular |
| Settings | `gear` | Regular |
| Home | `house.fill` | Regular |
| Success | `checkmark.circle.fill` | Regular |
| Warning | `exclamationmark.triangle.fill` | Regular |
| Sync | `arrow.triangle.2.circlepath` | Regular |
| Time | `clock.fill` | Regular |

### Component Styles

**Primary Button:**
- Background: Primary blue
- Text: White, 17pt Semibold
- Corner radius: 13pt
- Height: 50pt
- Tap: Scale to 0.95 briefly

**Secondary Button:**
- Background: Clear
- Border: 1pt Primary blue
- Text: Primary blue, 17pt Semibold
- Corner radius: 13pt
- Height: 50pt

**Destructive Button:**
- Background: Error red
- Text: White, 17pt Semibold
- Used only in alerts/confirmations

**Cards:**
- Background: System secondary background
- Corner radius: 12pt
- Shadow: 0pt Y, 4pt blur, 10% opacity
- Padding: 16pt all sides

**List Rows:**
- Height: 44pt minimum
- Separator: System gray, 0.5pt
- Chevron: System gray, for navigation rows

**Progress Ring:**
- Diameter: 200pt (home screen)
- Stroke width: 16pt
- Track: System gray (20% opacity)
- Fill: Primary blue → Success green gradient
- Animation: 0.5s ease-out

---

## 6. Component Catalog

### ConnectionStatusIndicator

**Purpose:** Show BLE connection state at a glance

**Placement:** Navigation bar trailing edge (all tabs)

**Variants:**

| State | Dot Color | Label | Animation |
|-------|-----------|-------|-----------|
| Connected | Green | "Connected" | None |
| Connecting | Orange | "Connecting..." | Pulse (scale 1.0→1.15→1.0, 1s loop) |
| Disconnected | Gray | "Disconnected" | None |
| Failed | Red | "Connection Failed" | None |
| BLE Off | Gray | "Bluetooth Off" | None |

**Layout:**
```
┌───────────────────────────────┐
│                    [●] Label  │
└───────────────────────────────┘
                     └─ 8pt dot
```

**Accessibility:**
- VoiceOver: "Bluetooth connected" / "Bluetooth disconnected"
- Tap action: If disconnected, attempts reconnection

---

### SyncProgressBanner

**Purpose:** Show sync progress during chunked transfer

**Placement:** Below navigation bar, full width

**Layout:**
```
┌───────────────────────────────┐
│  ↻ Syncing 142 records...     │
│  ████████░░░░░░░░░░  40%      │
└───────────────────────────────┘
```

**Variants:**

| State | Background | Text |
|-------|------------|------|
| In Progress | Blue (10% opacity) | "Syncing {n} records..." |
| Complete | Green (10% opacity) | "Synced {n} records ✓" |
| Failed | Red (10% opacity) | "Sync failed" + Retry button |

**Behavior:**
- Appears: Slide from top when sync starts
- Progress: Updates with each chunk ACK
- Complete: Shows for 2s, then fades out
- Failed: Stays visible with Retry button

---

### CircularProgressView (Existing)

**Purpose:** Display daily goal progress (PRIMARY) or bottle level

**Props:**
```swift
struct CircularProgressView {
    let current: Int      // Current value (ml)
    let total: Int        // Maximum value (ml)
    let color: Color      // Ring color
    let label: String     // Customizable label (default: "remaining")
}
```

**Animation:**
- Duration: 0.5s
- Curve: ease-out
- Animates both ring trim and center text

**Center Text (Daily Goal - PRIMARY usage):**
```
    1,200 ml       ← 28pt Bold, primary color
 of 2,000ml goal   ← 15pt Regular, secondary color
```

**Accessibility:**
- VoiceOver: "Daily progress: 1200 milliliters of 2000 milliliters goal"

---

### DrinkListItem (Existing)

**Purpose:** Single drink record in list - emphasizes amount consumed over remaining

**Layout:**
```
┌───────────────────────────────┐
│  💧  200ml        420ml       │
│   │    │            │         │
│   │    │            └──────── Level after (secondary, de-emphasized)
│   │    └───────────────────── Amount consumed (headline, bold)
│   │                           │
│   │  2:30 PM      remaining   │
│   │    │            │         │
│   │    │            └──────── Label (caption, tertiary)
│   │    └───────────────────── Timestamp (subheadline, secondary)
│   └────────────────────────── Icon (blue)
└───────────────────────────────┘
```

**Accessibility:**
- VoiceOver: "Drank 200 milliliters at 2:30 PM, 420 milliliters remaining"

---

### BatteryIndicator

**Purpose:** Show device battery level

**Layout:**
```
[🔋] 85%
```

**Dynamic Symbol:**
| Level | Symbol | Color |
|-------|--------|-------|
| 100% | `battery.100` | Green |
| 75% | `battery.75` | Green |
| 50% | `battery.50` | Green |
| 25% | `battery.25` | Orange |
| <20% | `battery.25` | Red |
| Charging | `battery.100.bolt` | Green |

---

### InAppBanner

**Purpose:** Temporary notification messages

**Types:**

| Type | Background | Icon | Duration |
|------|------------|------|----------|
| Success | Green (10%) | checkmark.circle.fill | 2s auto-dismiss |
| Warning | Orange (10%) | exclamationmark.triangle.fill | 4s auto-dismiss |
| Error | Red (10%) | xmark.circle.fill | 4s auto-dismiss |
| Info | Blue (10%) | info.circle.fill | 3s auto-dismiss |
| Persistent | Orange (10%) | varies | Until resolved |

**Layout:**
```
┌───────────────────────────────┐
│  ✓  Connected to Aquavate-1A2B│
└───────────────────────────────┘
```

**Behavior:**
- Enter: Slide from top, 0.3s ease-out
- Exit: Fade out, 0.2s ease-in
- Multiple banners: Stack vertically (max 3)

---

## 7. Notification Strategy

### Hydration Reminders (Pace-Based Model) - Issue #27

Smart reminders based on whether user is on pace to meet daily goal. Uses pace-based urgency rather than time since last drink.

**Pace Calculation:**
```
Expected intake = (elapsed active hours / 15) × dailyGoalMl
Deficit = expected - dailyTotalMl
```

**Urgency Levels:**

| Urgency | Condition | Color | Notification |
|---------|-----------|-------|--------------|
| On Track | deficit ≤ 0 | Blue | None |
| Attention | 0 < deficit < 20% of goal | Amber | "Time to hydrate! You're Xml behind pace." |
| Overdue | deficit ≥ 20% of goal | Red | "You're falling behind! Drink Xml to catch up." |

**Configuration:**
- Active hours: 7am-10pm (15 hours)
- Quiet hours: 10pm-7am (no reminders)
- Max 12 reminders per day (configurable via "Limit Daily Reminders" toggle)
- 50ml rounding: Deficits rounded to nearest 50ml, suppressed if <150ml (50ml with Early Notifications)
- Escalation model: Only notify when urgency increases (no repeated same-level notifications)

**Notification Types:**

| Trigger | Title | Body | Platform |
|---------|-------|------|----------|
| Behind pace (attention) | "Hydration Reminder" | "Time to hydrate! You're Xml behind pace." | iPhone + Watch |
| Behind pace (overdue) | "Hydration Reminder" | "You're falling behind! Drink Xml to catch up." | iPhone + Watch |
| Goal achieved | "Goal Reached! 💧" | "Good job! You've hit your daily hydration goal." | iPhone + Watch |
| Back on track (optional) | "Back On Track! ✓" | "Nice work catching up on your hydration." | iPhone + Watch |
| Low battery | "Bottle battery low" | "Aquavate at {X}%. Charge soon." | iPhone |

**Settings (SettingsView):**
- Hydration Reminders toggle (main on/off)
- Limit Daily Reminders toggle (enforces 12/day max, enabled by default)
- Back On Track Alerts toggle (optional, disabled by default)
- Early Notifications toggle (DEBUG only, lowers threshold from 150ml to 50ml)

**Background Notifications:**
- Uses BGAppRefreshTask for background delivery
- Scheduled when app enters background (~15 min intervals)
- iOS controls actual timing based on app usage patterns

### Apple Watch Notifications

Watch receives notifications via two mechanisms:
1. **iOS mirroring** - Standard iOS notification mirroring (when iPhone locked)
2. **Local notifications** - iPhone-triggered local notifications on Watch (always reliable)

Watch notifications include haptic feedback via `WKInterfaceDevice.current().play(.notification)`.

### Legacy Notifications

| Trigger | Title | Body | Time | Action |
|---------|-------|------|------|--------|
| Low battery | "Bottle battery low" | "Aquavate at {X}%. Charge soon." | When battery < 20% | Open app |
| Sync reminder | "Sync your bottle" | "Haven't synced in 24 hours." | 24h since last sync | Open app |

**Settings:**
- All notifications can be disabled in Settings
- Respects iOS notification permissions

### In-App Banners

| Event | Message | Style |
|-------|---------|-------|
| Connected | "Connected to {device_name}" | Success, 2s |
| Disconnected | "Connection lost" | Warning, persistent |
| Reconnecting | "Reconnecting..." | Info, persistent |
| Sync started | "Syncing..." | Info, persistent |
| Sync complete | "Synced {n} records" | Success, 2s |
| Command success | "{action} complete" | Success, 2s |
| Command failed | "Command failed. Try again." | Error, 4s |
| BLE off | "Bluetooth is off" | Warning, persistent + action |
| Time not set | "Device time not set" | Warning, persistent + action |

---

## 8. Accessibility Requirements

### VoiceOver

**Labels (not just icons):**
- Button: "Connect to bottle" (not "Connect")
- Tab: "Home, tab 1 of 3" (system default)
- Progress: "Daily goal progress: 60 percent, 1200 milliliters of 2000"

**Hints:**
- Pull-to-refresh: "Pull down to sync"
- Tappable row: "Double tap to open"

**Announcements:**
- Connection change: "Bottle connected" / "Bottle disconnected"
- Sync complete: "Sync complete, 142 records imported"
- Goal achieved: "Congratulations! You reached your hydration goal"

### Dynamic Type

**Supported range:**
- Minimum: Small (accessibility small)
- Maximum: AX5 (accessibility extra-extra-extra-large)

**Layout adaptation:**
- Single-column layout adjusts spacing
- Icons scale with text
- Circular progress ring maintains fixed size
- List items increase height for larger text

### Color & Contrast

**Requirements:**
- All text: WCAG AA minimum (4.5:1 contrast)
- Large text (18pt+): 3:1 minimum
- Interactive elements: Non-color indicators (icons, labels)

**High Contrast Mode:**
- Increase border widths to 2pt
- Use solid fills instead of transparency
- Darker text colors

### Touch Targets

**Minimum sizes:**
- All interactive elements: 44×44pt
- Spacing between targets: ≥8pt

### Reduce Motion

**When enabled:**
- Disable pulse animations
- Replace slides with fades
- Progress ring changes instantly
- Banners appear/disappear without animation

---

## 9. Edge Cases & Error Handling

### Connection Edge Cases

| Scenario | Detection | Behavior |
|----------|-----------|----------|
| BLE off | CBCentralManager.state == .poweredOff | Show persistent banner with Settings link |
| BLE unauthorized | state == .unauthorized | Show "Allow Bluetooth" alert |
| Device not found (30s) | Scan timeout | "No devices found" + Retry button |
| Connection timeout | 10s connect timeout | "Connection timed out" + Retry |
| Multiple devices found | >1 Aquavate peripheral | Show picker list (MVP: auto-select first) |
| Device in use | Connection refused | "Device connected elsewhere" alert |

### Sync Edge Cases

| Scenario | Detection | Behavior |
|----------|-----------|----------|
| 0 unsynced records | unsynced_count == 0 | Skip sync, no banner |
| 600 records (max) | Full buffer | Sync all (~30s), show progress |
| Mid-sync disconnect | Connection drop | Save partial, show incomplete banner |
| Duplicate records | CoreData constraint | Silent rejection (no error) |
| Corrupted chunk | Parse failure | Log error, skip chunk, continue |
| Timeout during sync | 30s no response | Show error, offer retry |

### Time Edge Cases

| Scenario | Detection | Behavior |
|----------|-----------|----------|
| time_valid = false | Flag in Current State | Show "Time not set" warning + auto-fix |
| Timezone change | App returns to foreground | Resync time on next connection |
| Clock drift | Consecutive connections | Resync every connection |

### Calibration Edge Cases (Updated 2026-01-27)

| Scenario | Detection | Behavior |
|----------|-----------|----------|
| BLE disconnect | Connection lost during calibration | Alert: "Connection lost. Calibration incomplete." → Return to Settings |
| User taps Cancel | User taps Cancel button | iOS sends CANCEL_CALIBRATION (0x21), bottle returns to idle, modal dismissed |
| Calibration error | Bottle broadcasts CAL_ERROR state | Alert: "Calibration failed" with retry option |
| Full < Empty ADC | Bottle detects invalid measurement | Bottle broadcasts CAL_ERROR, iOS shows "Ensure bottle was filled properly" |
| Bottle timeout | No stable reading for bottle timeout | Bottle broadcasts CAL_ERROR, iOS shows retry option |

*Note: With bottle-driven calibration, most edge cases (timeouts, validation) are handled by the bottle's state machine. iOS simply mirrors the state and shows appropriate UI.*

### Data Edge Cases

| Scenario | Detection | Behavior |
|----------|-----------|----------|
| No history data | CoreData empty | Show onboarding message |
| Future timestamps | timestamp > now | Log warning, display anyway |
| Negative amounts | amount_ml < 0 | Treat as refill, different icon |
| Very large drink (>1000ml) | amount_ml > 1000 | Display normally, no validation |
| Corrupt CoreData | Core Data error | Show error alert, offer reset |

---

## 10. Animation Specifications

### Bottle Level Ring

| Property | Value |
|----------|-------|
| Duration | 0.5s |
| Curve | ease-out |
| Trigger | bottle_level value change |
| Animates | Ring trim path + center text value |

**SwiftUI:**
```swift
.animation(.easeOut(duration: 0.5), value: bottleLevel)
```

### Daily Progress Bar

| Property | Value |
|----------|-------|
| Duration | 0.3s |
| Curve | ease-out |
| Trigger | daily_total value change |

### Drink List Item Appear

| Property | Value |
|----------|-------|
| Duration | 0.3s |
| Curve | ease-out |
| Effect | Opacity 0→1 + Y offset -20→0 |
| Trigger | New item added to list |

**SwiftUI:**
```swift
.transition(.asymmetric(
    insertion: .opacity.combined(with: .move(edge: .top)),
    removal: .opacity
))
```

### Connection Dot Pulse

| Property | Value |
|----------|-------|
| Duration | 1.0s |
| Curve | ease-in-out |
| Effect | Scale 1.0→1.15→1.0 |
| Repeat | Forever (while connecting) |

### Tab Switching

| Property | Value |
|----------|-------|
| Duration | 0.25s (system default) |
| Effect | Cross-fade |

### Modal Presentation

| Property | Value |
|----------|-------|
| Duration | 0.3s |
| Curve | ease-out |
| Effect | Slide from bottom + background dim |

### Banner Show/Hide

| Property | Value |
|----------|-------|
| Show duration | 0.3s |
| Hide duration | 0.2s |
| Curve | ease-out / ease-in |
| Effect | Slide from top + fade |

### Pull-to-Refresh

| Property | Value |
|----------|-------|
| Threshold | 60pt pull distance |
| Effect | Standard UIRefreshControl spinner |

---

## 11. Implementation Notes (Phase 4 Mapping)

### Phase 4.1: BLE Foundation

**No UX changes** - Backend only (scanning, connection, discovery)

### Phase 4.2: Current State & Battery Notifications

**UX additions:**
- Add `ConnectionStatusIndicator` to navigation bar
- Wire `CircularProgressView` to live BLE data
- Wire daily goal progress to live data
- Add `BatteryIndicator` to Settings

**Files to modify:**
- HomeView.swift - Replace mock data with BLE state
- SettingsView.swift - Add battery indicator, connection status

### Phase 4.3: CoreData Schema & Persistence

**UX additions:**
- History tab reads from CoreData
- Recent drinks list reads from CoreData
- Empty states for no data

**Files to modify:**
- HistoryView.swift - Bind to CoreData fetch
- HomeView.swift - Bind recent drinks to CoreData

### Phase 4.4: Drink Sync Protocol

**UX additions:**
- Add `SyncProgressBanner` component
- Implement drink list item animations (fade in)
- Show sync progress during chunked transfer

**New files:**
- Components/SyncProgressBanner.swift

**Files to modify:**
- HomeView.swift - Add sync banner
- HistoryView.swift - Add pull-to-refresh sync

### Phase 4.5: Commands & Time Sync

**UX additions:**
- Make Settings command rows functional (Tare, Reset, Clear)
- Add confirmation alerts for destructive actions
- Add success/error banners for command feedback
- Add time_valid warning banner

**New files:**
- Components/InAppBanner.swift

**Files to modify:**
- SettingsView.swift - Wire up commands
- HomeView.swift - Add time warning if needed

### Phase 4.6: Background Mode & Polish

**UX additions:**
- Implement reconnection flow UI
- Add "Last synced X ago" timestamp
- Polish all animations
- Add disconnect/reconnect banners
- Implement pull-to-refresh reconnection

**Files to modify:**
- HomeView.swift - Reconnection UI, last synced time
- All views - Final polish pass

---

## Appendix: Screen Inventory

| Screen | Status | Phase | Notes |
|--------|--------|-------|-------|
| Splash Screen | ✅ Exists | - | No changes |
| Pairing Screen | 🆕 New | 4.1 | Device scanning |
| Calibration Wizard | 🔄 Updated | 4.7 | Two-point calibration (updated 2026-01-17) |
| Home Screen | ✅ Complete | 4.2-4.6 | Wire BLE data, target visualization (Issue #27) |
| History Screen | ✅ Complete | 4.3-4.4 | Wire CoreData |
| Settings Screen | ✅ Complete | 4.2-4.5 | Redesigned with sub-pages (Issue #87). Calibrate, Diagnostics, Hydration Reminders |
| Activity Stats | ✅ Complete | - | Battery diagnostics with offline support (Issue #36, 2026-01-24) |
| Watch App | ✅ Complete | - | Companion app + complications (Issue #27, 2026-01-24) |

| Component | Status | Phase |
|-----------|--------|-------|
| CircularProgressView | ✅ Exists | 4.2 (wire data) |
| DrinkListItem | ✅ Exists | 4.3 (wire data) |
| ConnectionStatusIndicator | 🆕 New | 4.2 |
| SyncProgressBanner | 🆕 New | 4.4 |
| BatteryIndicator | 🆕 New | 4.2 |
| InAppBanner | 🆕 New | 4.5 |

---

## Approval

This UX PRD defines the complete user experience for the Aquavate iOS app. Upon approval, Phase 4 implementation will begin following both this document and the technical plan in [Plans/014-ios-ble-coredata-integration.md](../Plans/014-ios-ble-coredata-integration.md).

**Document Status:** Approved (v1.20)

**Update Note (2026-01-24 - Activity Stats Persistence):**
- Activity stats now persist in CoreData (Issue #36 Comment)
- Users can view cached data when disconnected with "Last synced X ago" timestamp
- Diagnostics section accessible when disconnected in SettingsView
- Follows existing drink record persistence pattern

**Update Note (2026-01-24 - Daily Reminder Limit Toggle):**
- Added "Limit Daily Reminders" toggle to Settings → Hydration Reminders (Issue #45)
- When enabled (default): enforces 12 reminders/day maximum
- When disabled: allows unlimited reminders (respects quiet hours and escalation)
- "Reminders Today" display shows "X/12" when limit enabled, just "X" when disabled
- Fixed back-on-track notification timing bug (was using stale urgency data)

**Update Note (2026-01-24 - Hydration Reminders + Watch App):**
- Added pace-based hydration reminder system (Issue #27)
- Reminders based on deficit from expected intake, not time since last drink
- Active hours: 7am-10pm, quiet hours: 10pm-7am, max 12 reminders/day
- Urgency colors: Blue (on track) → Amber (attention) → Red (overdue)
- 50ml rounding for deficit display (values <50ml suppressed)
- Added Apple Watch companion app with complications
- Watch shows large colored water drop + "Xml to catch up" / "On track ✓"
- Watch local notifications triggered by iPhone for reliable delivery
- Added target intake visualization to HomeView (shows expected vs actual)
- Back On Track notification (optional, disabled by default)
- Background notifications via BGAppRefreshTask

**Update Note (2026-01-23 - Activity Stats):**
- Added Diagnostics section to Settings screen
- New Activity Stats view for battery analysis (Issue #36)
- Shows motion wake events, backpack sessions, and "drink taken" indicators
- Lazy loading design - data fetched only when view opened

**Update Note (2026-01-23 - Shake-to-Empty Toggle):**
- Added Gestures section to Settings screen (visible when connected)
- Toggle to enable/disable shake-to-empty gesture
- Setting syncs to firmware via new BLE Device Settings characteristic (UUID 0006)
- Default: enabled (preserves existing behavior)

**Update Note (2026-01-21 - HealthKit):**
- Apple HealthKit integration implemented (Settings toggle, auto-sync)
- Each drink creates a water intake sample in Health app
- Deleting drinks removes corresponding HealthKit samples
- Day boundary now uses midnight (aligns with HealthKit)

**Update Note (2026-01-21 - Bidirectional Sync):**
- Bidirectional drink record sync implemented
- Swipe-to-delete now requires bottle connection (pessimistic delete)
- HomeView shows ALL today's drinks (removed 5-drink limit)
- Daily total always calculated from CoreData sum

**Update Note (2026-01-18):**
- Pull-to-Refresh sync implemented for Home screen
- Connection stays open 60 seconds for real-time updates
- Settings connection controls wrapped in `#if DEBUG` (release builds use pull-to-refresh)
- "Bottle is Asleep" alert when device not found

**Update Note (2026-01-17):**
- Calibration Wizard updated for iOS-based two-point calibration
- Firmware standalone calibration removed (IRAM optimization)
- All calibration UX now handled by iOS app with live ADC readings
- Phase 4.7 added to implementation plan

**Next Steps:**
1. ✅ Phase 4.1-4.6 implementation (Complete)
2. ✅ Pull-to-Refresh sync (Complete - 2026-01-18)
3. ✅ Bidirectional drink sync (Complete - 2026-01-21)
4. ✅ HealthKit integration (Complete - 2026-01-21)
5. ✅ Hydration Reminders + Apple Watch App (Complete - 2026-01-24, Issue #27)
6. ✅ Activity Stats Persistence (Complete - 2026-01-24, Issue #36 Comment)
7. Phase 4.7 implementation (Calibration Wizard) - Optional/Future
8. Begin Phase 5 (Advanced features)
