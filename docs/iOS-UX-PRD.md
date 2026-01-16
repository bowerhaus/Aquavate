# Aquavate iOS App - UX Product Requirements Document

**Version:** 1.0
**Date:** 2026-01-16
**Status:** Draft for Review

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

### 2.3 Calibration Wizard (Modal)

**Purpose:** Set up tare weight and capacity for user's specific bottle

**Presentation:** Full-screen modal sheet (not dismissible by swipe during calibration)

**Step 1: Tare (Empty Bottle)**
```
┌─────────────────────────────────┐
│           Step 1 of 2           │  ← Progress indicator
│                                 │
│   ┌─────────────────────────┐   │
│   │                         │   │
│   │   [Empty Bottle Icon]   │   │  ← SF Symbol: waterbottle
│   │                         │   │
│   └─────────────────────────┘   │
│                                 │
│   Place Your Empty Bottle       │  ← Headline
│                                 │
│   Remove any water and place    │
│   your empty bottle on the      │
│   puck. Keep it still.          │  ← Instructions
│                                 │
│   ⏳ Measuring...               │  ← Status (or ✓ Stable when ready)
│                                 │
│   ┌─────────────────────────┐   │
│   │   Set Empty Weight      │   │  ← Primary button (enabled when stable)
│   └─────────────────────────┘   │
│                                 │
└─────────────────────────────────┘
```

**Step 2: Capacity (Full Bottle)**
```
┌─────────────────────────────────┐
│           Step 2 of 2           │
│                                 │
│   ┌─────────────────────────┐   │
│   │                         │   │
│   │  [Full Bottle Icon]     │   │
│   │                         │   │
│   └─────────────────────────┘   │
│                                 │
│   Fill Your Bottle              │
│                                 │
│   Fill to your normal level     │
│   and enter the capacity.       │
│                                 │
│   ┌─────────────────────────┐   │
│   │  [Capacity Picker]      │   │  ← Stepper: 500-1500ml in 50ml steps
│   │       750 ml            │   │
│   └─────────────────────────┘   │
│                                 │
│   ┌─────────────────────────┐   │
│   │   Complete Setup        │   │
│   └─────────────────────────┘   │
│                                 │
└─────────────────────────────────┘
```

**Completion:**
- Haptic: Success (light impact)
- Brief "Setup Complete ✓" message (1s)
- Dismiss modal → Navigate to Home Screen

**Edge Cases:**
| Scenario | Behavior |
|----------|----------|
| Weight not stable | "Hold still..." message, button disabled |
| Capacity < 100ml | Error: "Capacity must be at least 100ml" |
| User cancels mid-flow | Alert: "Cancel setup? You'll need to complete this later." |
| Connection lost | Alert: "Connection lost. Tap Retry to reconnect." |

---

### 2.4 Home Screen (Main Tab)

**Purpose:** At-a-glance view of current bottle level and daily progress

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
│     │      420 ml       │       │  ← Large text (28pt Bold)
│     │    remaining      │       │  ← Subtext (15pt Regular)
│     │                   │       │
│      ╲                 ╱        │
│       └───────────────┘         │  ← Circular progress ring (200pt)
│                                 │
│   ┌─────────────────────────┐   │
│   │ Today's Goal        60% │   │
│   │ ████████░░░░░░░░░░░░░░  │   │  ← Progress bar
│   │ 1,200ml / 2,000ml       │   │
│   └─────────────────────────┘   │
│                                 │
│   ─────────────────────────────│
│                                 │
│   Recent Drinks                 │  ← Section header
│                                 │
│   💧 2:30 PM    200ml   420ml  │  ← DrinkListItem
│   ─────────────────────────────│
│   💧 11:45 AM   150ml   620ml  │
│   ─────────────────────────────│
│   💧 9:15 AM    250ml   770ml  │
│                                 │
└─────────────────────────────────┘
│  🏠 Home  │  📊 History  │  ⚙️  │  ← Tab bar
└─────────────────────────────────┘
```

**Data Displayed:**
| Element | Source | Format |
|---------|--------|--------|
| Bottle level | BLE Current State (real-time) | "{X} ml remaining" |
| Daily total | BLE Current State or CoreData sum | "{X}ml / {goal}ml" |
| Recent drinks | CoreData (last 5 today) | Time, amount, level after |
| Sync status | Last BLE sync timestamp | "Last synced {X} ago" |
| Connection dot | BLE connection state | Green/Orange/Gray |

**Pull-to-Refresh:**
- Triggers manual sync if connected
- Shows refresh spinner until complete
- If disconnected: Attempts reconnection

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

### 2.6 Settings Screen

**Purpose:** Device configuration and app preferences

**Layout:**
```
┌─────────────────────────────────┐
│  Settings                       │
│                                 │
│  BOTTLE CONFIGURATION           │  ← Section header
│  ┌─────────────────────────────┐│
│  │ Name           My Bottle    ││
│  ├─────────────────────────────┤│
│  │ Capacity           750ml    ││
│  ├─────────────────────────────┤│
│  │ Daily Goal        2000ml    ││  ← Tappable to edit
│  └─────────────────────────────┘│
│                                 │
│  DEVICE                         │
│  ┌─────────────────────────────┐│
│  │ 🔋 Battery            85%   ││  ← Green/orange/red by level
│  ├─────────────────────────────┤│
│  │ 📶 Status    ● Connected    ││
│  ├─────────────────────────────┤│
│  │ ↻ Last Sync      2:45 PM    ││
│  ├─────────────────────────────┤│
│  │ ⏱ Device Time      ✓ Set    ││  ← Warning if not set
│  └─────────────────────────────┘│
│                                 │
│  DEVICE COMMANDS                │
│  ┌─────────────────────────────┐│
│  │ ⚖️ Tare Bottle              ││  ← Tappable action
│  ├─────────────────────────────┤│
│  │ 🔄 Reset Daily Total        ││
│  ├─────────────────────────────┤│
│  │ 🗑 Clear History     ⚠️     ││  ← Destructive (red text)
│  └─────────────────────────────┘│
│                                 │
│  PREFERENCES                    │
│  ┌─────────────────────────────┐│
│  │ 📏 Use Ounces        [OFF]  ││  ← Toggle
│  ├─────────────────────────────┤│
│  │ 🔔 Notifications     [ON]   ││
│  └─────────────────────────────┘│
│                                 │
│  ABOUT                          │
│  ┌─────────────────────────────┐│
│  │ Version      1.0.0 (Build 1)││
│  ├─────────────────────────────┤│
│  │ 🔗 GitHub Repository    →   ││
│  └─────────────────────────────┘│
│                                 │
└─────────────────────────────────┘
```

**Command Actions:**

| Command | Tap Behavior | Confirmation |
|---------|--------------|--------------|
| Tare Bottle | Sends TARE_NOW (0x01) | None (instant) |
| Reset Daily Total | Sends RESET_DAILY (0x05) | Alert: "Reset today's total?" |
| Clear History | Sends CLEAR_HISTORY (0x06) | Alert: "This cannot be undone." |

**Command Feedback:**
- Success: Green banner "Bottle tared" (2s)
- Failure: Red banner "Command failed. Try again." (4s)
- Disconnected: Alert "Connect to bottle first"

**Device Time Warning:**
If `time_valid` flag is false:
```
│  │ ⏱ Device Time    ⚠️ Not Set ││  ← Orange warning icon
```
- Tap row → Sends time sync command automatically

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

### Flow 2: Daily Usage

```
Morning:
    │
    ▼
Pick Up Bottle ──► Puck wakes, starts advertising
    │
    ▼
iPhone Auto-reconnects (background)
    │
    ▼
Sync any overnight drinks (automatic)
    │
    ▼
User opens app
    │
    ▼
Home Screen shows current state
    │
    ├── Bottle level: 750ml (full)
    ├── Daily total: 0ml
    └── Connection: ● Connected

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
If app open: UI updates in real-time
If app closed: Updates on next open/sync

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

### Flow 5: Calibration Recalibration

```
Settings Screen
    │
    ▼
Tap "Tare Bottle"
    │
    ▼
Command sent to puck
    │
    ▼
[Weight Stable?]
    │
    ├── Yes → Tare saved
    │         Banner: "Bottle tared ✓"
    │
    └── No → Error
              Banner: "Hold bottle still and try again"
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
| Long-press | Not used (MVP) | Reserved for future |

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

**Purpose:** Display bottle level or daily progress

**Props:**
```swift
struct CircularProgressView {
    let current: Int      // Current value (ml)
    let total: Int        // Maximum value (ml)
    let color: Color      // Ring color
}
```

**Animation:**
- Duration: 0.5s
- Curve: ease-out
- Animates both ring trim and center text

**Center Text:**
```
     420 ml        ← 28pt Bold, primary color
    remaining      ← 15pt Regular, secondary color
```

**Accessibility:**
- VoiceOver: "Bottle level: 420 milliliters of 750 milliliters remaining"

---

### DrinkListItem (Existing)

**Purpose:** Single drink record in list

**Layout:**
```
┌───────────────────────────────┐
│  💧  2:30 PM    200ml   420ml │
│   │     │         │       │   │
│   │     │         │       └── Level after (secondary)
│   │     │         └────────── Amount (primary)
│   │     └──────────────────── Timestamp (secondary)
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

### Local Notifications

| Trigger | Title | Body | Time | Action |
|---------|-------|------|------|--------|
| Goal reminder | "Time for water! 💧" | "You've had {X}ml today. Keep going!" | User-set (default 3 PM) | Open app |
| Goal achieved | "Goal reached! 🎉" | "You hit your {goal}ml goal today!" | When daily_total ≥ goal | Open app |
| Low battery | "Bottle battery low" | "Aquavate at {X}%. Charge soon." | When battery < 20% | Open app |
| Sync reminder | "Sync your bottle" | "Haven't synced in 24 hours." | 24h since last sync | Open app |

**Settings:**
- All notifications can be disabled in Settings
- Goal reminder time is configurable
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

### Calibration Edge Cases

| Scenario | Detection | Behavior |
|----------|-----------|----------|
| Weight unstable | stable flag = false | "Hold still..." message, button disabled |
| Capacity < 100ml | User input validation | Error message, prevent continue |
| Calibration timeout (60s) | No stable reading | Cancel wizard, show error |
| User exits mid-calibration | Cancel button or swipe | Confirm alert, return to pairing |
| Connection lost during | Disconnect event | Alert, offer retry from current step |

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

| Screen | Status | Phase |
|--------|--------|-------|
| Splash Screen | ✅ Exists | - |
| Pairing Screen | 🆕 New | 4.1 |
| Calibration Wizard | 🆕 New | 4.5 |
| Home Screen | 🔄 Modify | 4.2-4.6 |
| History Screen | 🔄 Modify | 4.3-4.4 |
| Settings Screen | 🔄 Modify | 4.2-4.5 |

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

**Document Status:** Ready for Review

**Next Steps:**
1. User reviews this document
2. Feedback incorporated (if any)
3. Final approval
4. Begin Phase 4.1 implementation
