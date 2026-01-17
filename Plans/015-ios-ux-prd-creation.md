# iOS UX PRD Creation Plan

## Context

The technical implementation plan for iOS BLE integration (Phase 4) is complete and saved to [Plans/014-ios-ble-coredata-integration.md](Plans/014-ios-ble-coredata-integration.md). Before starting implementation, we need to create a detailed UX PRD that defines the user experience, interaction patterns, and visual design.

## Current State

**Existing Documentation:**
- [docs/PRD.md](docs/PRD.md) - Firmware + high-level iOS requirements (basic screen outline only)
- [Plans/012-ios-placeholder-ui.md](Plans/012-ios-placeholder-ui.md) - Placeholder UI implementation (mock data)
- iOS app has working views: HomeView, HistoryView, SettingsView with basic layouts

**What's Missing:**
- User flows (first-time setup, daily usage, error recovery)
- Interaction details (gestures, animations, transitions)
- Visual specifications (spacing, colors, typography)
- Edge cases and error states (BLE off, not paired, sync failures)
- Empty and loading states
- Notification strategy
- Accessibility requirements

## Objective

Create **[docs/iOS-UX-PRD.md](docs/iOS-UX-PRD.md)** that provides complete UX specifications for implementing the iOS app UI (primarily Phases 4.4-4.6 of the technical plan).

## Document Structure

### 1. User Personas & Scenarios

**Primary Persona:**
- Name, age, occupation
- Daily water consumption patterns
- Pain points with hydration tracking
- Technology comfort level
- Key goals

**Key Usage Scenarios:**
1. First-time setup (unboxing → pairing → calibration)
2. Daily usage (morning hydration check → drinking throughout day → evening review)
3. Weekly review (history analysis, pattern identification)
4. Maintenance (battery check, calibration, bottle switching)
5. Error recovery (Bluetooth off, connection lost, sync failure)

### 2. Screen-by-Screen Specifications

For **each screen** (Home, History, Settings, Pairing):

#### Screen Name

**Purpose:** One-sentence goal

**Layout Specification:**
```
┌─────────────────────────────┐
│  [Navigation Bar]           │  ← Specify exact placement
│                             │
│     [Primary Component]     │  ← Main UI element with dimensions
│                             │
│  [Secondary Info]           │  ← Supporting data display
│                             │
│  [Action Buttons]           │  ← CTAs with hierarchy
└─────────────────────────────┘
```

**User Actions:**
- Primary: Main action user takes on this screen
- Secondary: Supporting actions
- Gestures: Tap, swipe, long-press behaviors
- Navigation: How to enter/exit this screen

**Data Displayed:**
- Real-time data (from BLE): What updates live
- Persisted data (from CoreData): What's stored locally
- Format: ml vs oz, timestamps, percentages

**State Variations:**

**Normal State:**
- All data available, BLE connected
- What user sees in happy path

**Empty State:**
- No bottle paired: Show illustration + "Pair Your Bottle" CTA
- No drinks today: Show motivational message + empty list
- No history data: Show onboarding hint

**Loading State:**
- Syncing drinks: Progress indicator with count
- Connecting: Spinner with "Connecting to Aquavate-1234..."
- Calibrating: Step-by-step progress

**Error State:**
- BLE disconnected: Amber dot + "Reconnect" button
- BLE off: Banner with "Turn on Bluetooth" deep link
- Sync failed: Retry button with error message
- Time not set: Warning banner "Set device time"

**Animations:**
- Entry: How screen appears (fade, slide, scale)
- Exit: How screen dismisses
- Data updates: How new drinks appear (fade in, slide from top)
- Progress: How bottle level animates (smooth fill transition)

---

#### Calibration Wizard (NEW - 2026-01-17)

**Purpose:** Guide user through two-point calibration to establish bottle's scale factor

**Context:** Firmware standalone calibration removed (IRAM savings). iOS app now performs calibration and writes results to firmware via BLE Bottle Config characteristic.

**Entry Points:**
- First-time setup flow (after pairing)
- Settings → "Calibrate Bottle" button
- Alert when "calibrated" flag is false in Current State

**Layout Specification - Step 1 (Empty Measurement):**
```
┌─────────────────────────────┐
│  [X Close] Calibration 1/2  │  ← Navigation bar with step indicator
│                             │
│    🍶                       │  ← Bottle icon (empty)
│                             │
│  Place Empty Bottle         │  ← Title (24pt, bold)
│                             │
│  Remove the bottle from     │  ← Instructions (16pt, gray)
│  the base and empty it      │
│  completely. Place back     │
│  when ready.                │
│                             │
│  [Current: 1245 ADC]        │  ← Live weight reading (monospace, 14pt)
│  [Stability: ●●●○○]         │  ← Stability indicator (5 dots)
│                             │
│  [ Tap When Stable ]        │  ← Primary CTA (disabled until stable)
│                             │
│  [< Back]                   │  ← Secondary action
└─────────────────────────────┘
```

**Layout Specification - Step 2 (Full Measurement):**
```
┌─────────────────────────────┐
│  [X Close] Calibration 2/2  │  ← Step indicator updated
│                             │
│    🍶💧                     │  ← Bottle icon (filled)
│                             │
│  Fill Bottle to 830ml       │  ← Title (24pt, bold)
│                             │
│  Fill the bottle to the     │  ← Instructions (16pt, gray)
│  830ml line. Place upright  │
│  on the base when ready.    │
│                             │
│  [Current: 7892 ADC]        │  ← Live weight reading
│  [Stability: ●●●●●]         │  ← Stability indicator (all filled)
│                             │
│  [ Tap When Stable ]        │  ← Primary CTA (enabled when stable)
│                             │
│  [< Back]                   │  ← Return to step 1
└─────────────────────────────┘
```

**Layout Specification - Success Screen:**
```
┌─────────────────────────────┐
│  [Done]                      │  ← Dismiss button
│                             │
│         ✓                   │  ← Success checkmark (green, 72pt)
│                             │
│  Calibration Complete!      │  ← Title (24pt, bold)
│                             │
│  Scale: 8.2 ADC/g           │  ← Calculated scale factor
│  Tare: 1245 ADC             │  ← Empty baseline
│                             │
│  Your bottle is ready       │  ← Confirmation message
│  to track water intake!     │
│                             │
│  [    Continue    ]         │  ← Primary CTA (dismisses modal)
└─────────────────────────────┘
```

**User Actions:**

**Step 1 (Empty):**
- Primary: Tap "Tap When Stable" → advance to Step 2
- Secondary: Tap "Back" → cancel calibration (show alert)
- Secondary: Tap "X" → cancel calibration (show alert)
- Automatic: Live ADC reading updates every 500ms
- Automatic: Stability dots fill when weight stable for 2s

**Step 2 (Full):**
- Primary: Tap "Tap When Stable" → calculate scale factor → show success
- Secondary: Tap "Back" → return to Step 1
- Secondary: Tap "X" → cancel calibration (show alert)
- Automatic: Live ADC reading updates every 500ms
- Automatic: Stability dots fill when weight stable for 2s

**Success Screen:**
- Primary: Tap "Continue" → write calibration to firmware → dismiss modal
- Secondary: Tap "Done" (nav bar) → same as Continue

**Data Displayed:**

**Real-time (BLE Current State):**
- `currentWeightG` → ADC reading (raw sensor value)
- `isStable` flag → drives stability indicator

**Calculated:**
- Scale factor = (fullADC - emptyADC) / 830g
- Validation: scale factor must be 5-15 ADC/g (typical range)

**Written to firmware (BLE Bottle Config):**
```swift
bleManager.writeBottleConfig(
    scaleFactor: calculatedScaleFactor,
    tareWeight: emptyADC,
    capacity: 830,
    goal: 2400  // Default daily goal
)
```

**State Variations:**

**Normal State (Each Step):**
- Live ADC reading updating
- Stability indicator showing 1-5 dots
- CTA button disabled until stable (5/5 dots)

**Stable State:**
- All 5 stability dots filled (green)
- CTA button enabled (blue, pulsing glow)
- Haptic feedback when stability achieved

**Loading State (Success → Dismiss):**
- Show spinner: "Writing calibration..."
- Disable Continue button during write
- Auto-dismiss on success

**Error States:**

**Disconnection During Calibration:**
- Show alert: "Connection lost. Calibration incomplete."
- CTA: "Reconnect" → return to Settings

**Invalid Scale Factor:**
- If calculated scale factor < 5 or > 15 ADC/g
- Show alert: "Calibration failed. Scale factor out of range (8.2 expected, got X.X). Please try again."
- CTA: "Retry" → restart from Step 1

**Unstable Reading:**
- If user tries to tap CTA while unstable
- Shake animation on CTA button
- Show tooltip: "Hold still until all dots are filled"

**User Cancels Mid-Flow:**
- Show alert: "Cancel calibration? Your bottle will not be calibrated."
- CTAs: "Keep Calibrating" (primary), "Cancel Calibration" (destructive)

**Animations:**

**Entry:**
- Modal slides up from bottom (0.3s ease-out)
- Content fades in (0.2s delay)

**Step Transitions:**
- Slide left/right (0.3s) when advancing/going back
- Title cross-fade (0.2s)

**Stability Indicator:**
- Dots fill sequentially (0.1s each) as stability increases
- Pulse animation when all 5 filled

**Success Screen:**
- Checkmark scales in with spring animation (0.5s)
- Scale factor/tare fade in sequentially (0.1s stagger)

**Exit:**
- Modal slides down to bottom (0.3s ease-in)

**BLE Integration:**

**Subscribe to notifications:**
- Current State characteristic (for live ADC + stability flag)

**Read during calibration:**
- `bleManager.currentWeightG` → display as ADC reading
- `bleManager.isStable` → drive stability indicator

**Write on success:**
- `bleManager.writeBottleConfig(...)` → persist to firmware NVS

**Accessibility:**

- VoiceOver labels for each step: "Calibration step 1 of 2: Place empty bottle"
- Announce ADC updates every 2s (not every 500ms to avoid spam)
- Announce stability: "Weight stable, ready to continue" (when 5/5 dots)
- Announce success: "Calibration complete. Scale factor: 8.2 ADC per gram"
- Dynamic Type support for all text (min 16pt body, 24pt title)

**Edge Cases:**

| Scenario | Behavior |
|----------|----------|
| Weight not stable (< 2s) | CTA disabled, show "Hold still..." tooltip |
| Invalid scale factor | Alert: "Calibration failed. Try again." → restart |
| BLE disconnects during flow | Alert: "Connection lost" → return to Settings |
| User exits mid-calibration | Alert: "Cancel calibration?" with confirm |
| Firmware write fails | Alert: "Failed to save calibration. Please retry." |
| Scale factor = 0 or negative | Alert: "Invalid measurement. Ensure bottle is filled." |

---

### 3. User Flows (Critical Paths)

**Flow Diagrams** for:

#### First-Time Setup Flow
```
App Launch
   ↓
Splash Screen (2s fade-in animation)
   ↓
[No Bottle Paired] Check
   ↓ Yes
Pairing Screen
   ├─ Tap "Pair Your Bottle"
   ↓
Scanning Sheet
   ├─ "Looking for Aquavate devices..."
   ├─ Device found: "Aquavate-1234" appears
   ↓ Tap device
Connecting...
   ↓
Connection Success
   ↓
Calibration Wizard (Modal)
   ├─ Step 1: Remove bottle → Tare
   ├─ Step 2: Fill bottle → Set capacity
   ├─ Step 3: Confirmation → Done
   ↓
Home Screen (Connected)
```

#### Daily Usage Flow
```
Morning:
  Pick Up Bottle → (Auto-wake, advertise)
    ↓ (Background)
  iOS Auto-reconnects
    ↓
  Sync drinks (if any overnight)
    ↓
  User opens app → See current bottle level

Midday:
  Drink 200ml → Set down bottle
    ↓ (Real-time)
  Firmware updates daily total
    ↓
  iOS receives notification
    ↓ (If app open)
  Bottle level animates down
  Daily progress bar updates
    ↓ (If app closed)
  Data syncs on next open

Evening:
  Open app → Tap History tab
    ↓
  See today's drinks (5 records)
    ↓
  Tap day in calendar → See detail view
```

#### Error Recovery Flow
```
BLE Off Scenario:
  User opens app
    ↓
  App detects BLE powered off
    ↓
  Home screen shows:
    ├─ Gray connection dot
    ├─ Banner: "Bluetooth is off"
    └─ Button: "Turn On Bluetooth"
  ↓ Tap button
  iOS Settings deep link
    ↓ User enables BLE
  Return to app
    ↓
  Auto-reconnect starts
    ↓
  Green dot + "Connected"
```

#### Sync Failure Recovery Flow
```
Mid-sync Disconnection:
  Syncing chunk 3/10 (60 records)
    ↓ Connection lost
  BLEManager saves partial records (60)
    ↓
  Home screen shows:
    ├─ Orange connection dot
    ├─ "Sync incomplete: 140 records remaining"
    └─ Button: "Retry Sync"
  ↓ Tap retry
  Reconnect → Resume sync from chunk 4
    ↓
  Complete sync (chunks 4-10)
    ↓
  Green dot + "Synced"
```

### 4. Interaction Patterns

**Navigation:**
- Tab bar (bottom): Home, History, Settings
- Modal sheets: Pairing wizard, calibration, commands
- Navigation stack: History → Day Detail
- Alerts: Destructive actions (Clear History)

**Gestures:**
- Tap: Standard button/list item selection
- Pull-to-refresh: Manual sync trigger on Home/History
- Swipe: Potential future use (delete drink record)
- Long-press: Not used in MVP

**Transitions:**
- Tab switching: Cross-fade (default)
- Modal presentation: Slide up from bottom
- Alert presentation: Scale + fade (system default)
- List updates: Fade in new items from top

**Feedback:**
- Haptic: Success (light impact), error (notification), button tap (soft)
- Visual: Button press animation (scale 0.95), progress indicators
- Audio: None (respects system silent mode)

### 5. Visual Design Guidelines

**Color Palette:**
- Primary: `#007AFF` (iOS System Blue) - Buttons, active states
- Success: `#34C759` (System Green) - Connected, goal achieved
- Warning: `#FF9500` (System Orange) - Low battery, partial sync
- Error: `#FF3B30` (System Red) - Disconnected, errors
- Neutral: `#8E8E93` (System Gray) - Secondary text, disabled states
- Background: System (white/black adaptive for dark mode)

**Typography (SF Pro):**
- Large Title: 34pt Bold - Screen titles
- Headline: 17pt Semibold - Section headers
- Body: 17pt Regular - Primary text
- Callout: 16pt Regular - List item text
- Caption 1: 12pt Regular - Timestamps, metadata
- Caption 2: 11pt Regular - Very small labels

**Spacing (8pt Grid):**
- Screen padding: 16pt horizontal, 20pt vertical
- Component spacing: 24pt between major sections
- List item spacing: 12pt vertical padding
- Button height: 50pt (minimum touch target 44pt)

**Icons:**
- Source: SF Symbols 5.0
- Size: Small 16pt, Regular 20pt, Large 28pt
- Weight: Matches adjacent text weight
- Key icons:
  - `drop.fill` - Water/drinks
  - `bolt.fill` - Battery
  - `antenna.radiowaves.left.and.right` - BLE connection
  - `chart.bar.fill` - History
  - `gear` - Settings
  - `checkmark.circle.fill` - Success

**Component Styles:**

**Buttons:**
- Primary: Filled blue, white text, 13pt corner radius
- Secondary: Outlined blue, blue text, 13pt corner radius
- Destructive: Filled red, white text, 13pt corner radius
- All buttons: 50pt height, centered text

**Cards:**
- Background: System secondary background
- Corner radius: 12pt
- Shadow: 0pt Y offset, 4pt blur, 10% opacity
- Padding: 16pt all sides

**Progress Rings:**
- Bottle level: 200pt diameter, 16pt stroke, blue
- Daily goal: 8pt stroke, gradient blue→green
- Animation: 0.5s ease-out when value changes

### 6. Component Catalog

**CircularProgressView (Existing)**
- Purpose: Display bottle level or daily progress
- Props: current (Int), total (Int), color (Color)
- Animation: Smooth arc trim transition
- Center text: "{current}ml" (large) + "of {total}ml" (small)

**DrinkListItem (Existing)**
- Purpose: Single drink record in list
- Layout: Icon (drop.fill) | Timestamp | Amount | Level
- Example: "💧 2:30 PM    200ml    450ml remaining"
- Tap action: None (MVP), future: drill-down detail

**ConnectionStatusIndicator (New)**
- Purpose: Show BLE connection state
- Layout: Colored dot (8pt) + Text label
- States:
  - Green + "Connected"
  - Gray + "Disconnected"
  - Orange + "Connecting..." (with pulse animation)
  - Red + "Connection Failed"
- Placement: Top-right of navigation bar (all tabs)

**SyncProgressBanner (New)**
- Purpose: Show sync progress during chunked transfer
- Layout: Progress bar + "Syncing 142 records..." text
- Progress: 0.0-1.0 based on chunk_index/total_chunks
- Auto-dismiss: Fades out 1s after complete
- Error variant: Red background + "Sync failed" + Retry button

**BatteryIndicator (New)**
- Purpose: Show device battery level
- Layout: SF Symbol `battery.100` with fill based on %
- Colors: Green (>20%), Orange (10-20%), Red (<10%)
- Placement: Settings screen, device info section

### 7. Notification Strategy

**Local Notifications:**

| Trigger | Title | Body | Timing | Actionable |
|---------|-------|------|--------|------------|
| Daily goal reminder | "Time for water!" | "You've had {X}ml today. Keep going!" | 3 PM (user configurable) | Open app |
| Goal achieved | "Goal reached! 🎉" | "You hit your 2L goal today!" | When daily_total >= goal | Open app |
| Low battery | "Bottle battery low" | "Aquavate-1234 at 15%. Charge soon." | When battery < 20% | Open app |
| Sync reminder | "Sync your bottle" | "Last synced 24 hours ago." | 24h since last sync | Open app |

**In-App Banners:**

| Event | Message | Style | Duration |
|-------|---------|-------|----------|
| Connected | "Connected to Aquavate-1234" | Green banner, checkmark icon | 2s auto-dismiss |
| Disconnected | "Connection lost" | Orange banner, warning icon | Persistent until reconnect |
| Sync started | "Syncing..." | Blue banner, spinner | Until complete |
| Sync complete | "Synced 142 records" | Green banner, checkmark | 2s auto-dismiss |
| Command success | "Bottle tared" | Green banner | 2s auto-dismiss |
| Command failed | "Command failed. Try again." | Red banner, X icon | 4s auto-dismiss |

**Banner Placement:** Below navigation bar, above content, full-width

### 8. Accessibility Requirements

**VoiceOver:**
- All buttons: Descriptive labels (not just icon)
  - Good: "Connect to bottle"
  - Bad: "Connect"
- Progress rings: "Bottle level: 450 milliliters of 750"
- Connection status: "Bluetooth connected" (not just green dot)
- List items: "Drank 200 milliliters at 2:30 PM, 450 milliliters remaining"

**Dynamic Type:**
- All text scales with system text size settings
- Minimum: Small (-2 from default)
- Maximum: AX5 (+5 from default)
- Layouts adapt: Single-column on larger text sizes

**Color & Contrast:**
- All text meets WCAG AA (4.5:1 contrast minimum)
- Connection indicators use text labels (not just color)
- High Contrast mode: Increase border thickness, darker text

**Touch Targets:**
- Minimum: 44×44pt (Apple HIG requirement)
- All buttons, list items, tappable areas meet minimum
- Spacing between adjacent targets: ≥8pt

**Haptic Feedback:**
- Success actions: Light impact
- Error states: Notification haptic
- Button taps: Soft impact
- Respects system haptic settings (can disable)

### 9. Edge Cases & Error Handling

**Pairing Edge Cases:**

| Scenario | Behavior |
|----------|----------|
| No devices found after 30s | Show "No devices found" message + "Retry" button |
| Multiple Aquavate devices | Show list picker (future: MVP auto-pairs first) |
| Connection timeout | Show "Connection timed out" alert + retry |
| Already paired | Skip pairing, go directly to Home |

**Sync Edge Cases:**

| Scenario | Behavior |
|----------|----------|
| 0 unsynced records | Skip sync, show "No new data" |
| 600 unsynced records (max) | Sync all, no warning (takes ~30s) |
| Mid-sync disconnect | Save partial, show "Sync incomplete" + retry |
| Duplicate records | CoreData constraint prevents (silent) |
| Corrupted chunk data | Log error, skip chunk, mark failed |

**Time Sync Edge Cases:**

| Scenario | Behavior |
|----------|----------|
| time_valid = false | Show warning banner "Device time not set" |
| Timezone change | Resync time on next connection |
| Device time drifts | Resync every connection (not just first) |

**Calibration Edge Cases:**

| Scenario | Behavior |
|----------|----------|
| Weight not stable | Show "Hold still..." message until stable |
| Invalid capacity (< 100ml) | Show error "Capacity must be at least 100ml" |
| Calibration timeout | Cancel wizard, show "Calibration incomplete" |
| User exits mid-calibration | Alert: "Cancel calibration?" with confirm |

### 10. Animation Specifications

**Bottle Level Changes:**
- Duration: 0.5s
- Curve: `easeOut`
- Behavior: Smooth arc trim from old value to new
- Visual: Progress ring animates, center text counts up/down

**Drink Appears in List:**
- Duration: 0.3s
- Curve: `easeOut`
- Behavior: Fade in (opacity 0→1) + slide from top (offset -20→0)
- Visual: New item appears at top, pushes others down

**Connection Status Changes:**
- Duration: 0.2s
- Curve: `easeInOut`
- Behavior: Color cross-fade + scale pulse (1.0→1.2→1.0)
- Visual: Dot color changes, brief pulse on state change

**Tab Switching:**
- Duration: 0.25s
- Curve: Default (system)
- Behavior: Cross-fade between views
- Visual: Smooth transition, no slide

**Modal Presentation:**
- Duration: 0.3s
- Curve: `easeOut`
- Behavior: Slide up from bottom + background dim
- Visual: Sheet covers screen with rounded corners

**Pull-to-Refresh:**
- Behavior: Standard UIRefreshControl
- Visual: Spinner appears, rotates until sync complete
- Threshold: 60pt pull distance

### 11. Implementation Notes (for Phase 4.4-4.6)

**Phase 4.4 (Drink Sync Protocol):**
- Add `SyncProgressBanner` component
- Implement fade-in animation for new `DrinkListItem` entries
- Show progress: "Syncing chunk 3 of 10..."

**Phase 4.5 (Commands & Time Sync):**
- Add command buttons to Settings: "Tare Bottle", "Reset Daily Total"
- Show confirmation alert before destructive commands
- Display success/error banners after command execution
- Show time_valid warning banner if false

**Phase 4.6 (Background Mode & Polish):**
- Add `ConnectionStatusIndicator` to navigation bar (all tabs)
- Implement reconnection UI flow (disconnected banner → tap → reconnect)
- Add "Last synced: 5 mins ago" timestamp to Home screen
- Polish animations for all state transitions

## Deliverable

**File:** `docs/iOS-UX-PRD.md`

**Structure:**
1. User Personas & Scenarios
2. Screen-by-Screen Specifications (Home, History, Settings, Pairing)
3. User Flows (5 critical paths with diagrams)
4. Interaction Patterns (navigation, gestures, transitions)
5. Visual Design Guidelines (colors, typography, spacing, icons)
6. Component Catalog (all reusable UI components)
7. Notification Strategy (local notifications + in-app banners)
8. Accessibility Requirements (VoiceOver, Dynamic Type, contrast, touch targets)
9. Edge Cases & Error Handling (comprehensive coverage)
10. Animation Specifications (all transitions with timing/curves)
11. Implementation Notes (mapping to Phases 4.4-4.6)

**Length:** Approximately 3000-4000 words with diagrams

**Approval:** User reviews and approves before Phase 4 implementation begins

## Success Criteria

UX PRD is complete when:
- [ ] All 4 main screens fully specified (layout, states, animations)
- [ ] 5 critical user flows documented with diagrams
- [ ] All edge cases identified with defined behaviors
- [ ] Visual design system documented (colors, typography, spacing)
- [ ] Component catalog complete with props and usage
- [ ] Accessibility requirements specified
- [ ] Animation timing and curves defined
- [ ] Maps clearly to Phase 4.4-4.6 implementation tasks
- [ ] User approves the UX direction

## Timeline

Following Option A approach (UX PRD before implementation):
- **Day 1-2:** Draft UX PRD document
- **Day 3:** User review and feedback
- **Day 4:** Revisions based on feedback
- **Day 5:** Final approval → Begin Phase 4 implementation

This UX PRD will serve as the definitive reference for all UI/UX decisions during Phase 4 implementation, ensuring consistency and preventing mid-implementation design changes.
