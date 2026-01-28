# Settings Page Redesign - Progressive Disclosure Options

## Current State Analysis

The settings page currently has **9 sections** with ~30+ individual settings:

| Section | Items | Connection Required | Visibility |
|---------|-------|---------------------|------------|
| Device Connection | 1-5 (DEBUG adds more) | Status only | Always visible |
| Bottle Configuration | 3 | Daily Goal only | Always visible |
| Device Info | 7 | All | **Hidden when disconnected** |
| Device Commands | 6 | All | **Hidden when disconnected** |
| Gestures | 1 | Yes | **Hidden when disconnected** |
| Diagnostics | 1 | No | Always visible |
| Apple Health | 2 | No | Always visible |
| Hydration Reminders | 6 | No | Always visible |
| About | 1 | No | Always visible |

**Current Problems:**
1. Too many sections visible at once - cognitive overload
2. Device Info/Commands/Gestures **disappear** when disconnected (confusing - user might not know these exist)
3. No hierarchy - essential settings (Daily Goal, Reminders) mixed with rarely-used (Clear History, Tare)
4. Hydration Reminders section is verbose with 6 sub-items

---

## Option 1: Collapsible Sections (DisclosureGroup)

**Approach:** Use SwiftUI `DisclosureGroup` to collapse less-used settings within each section.

**Layout:**
```
┌─────────────────────────────────────┐
│ Device Connection                   │
│   Status: Connected ●               │
│   (DEBUG controls if debug build)   │
├─────────────────────────────────────┤
│ Bottle                              │
│   Daily Goal: 2000ml  >             │
│   ▼ Details                         │  ← DisclosureGroup
│      Device: Aquavate-ABC           │
│      Capacity: 830ml                │
├─────────────────────────────────────┤
│ Device                              │  ← Shows DISABLED when disconnected
│   Battery: 85%  🔋                  │
│   Calibrate Bottle  >               │
│   ▼ Commands                        │  ← DisclosureGroup
│      Tare (Zero Scale)              │
│      Reset Daily Total              │
│      Sync Time                      │
│      Clear History                  │
│   ▼ More Info                       │  ← DisclosureGroup
│      Current Weight: 450g           │
│      Calibrated: Yes                │
│      Time Set: Yes                  │
│      Shake to Empty: On             │
├─────────────────────────────────────┤
│ Notifications                       │
│   Hydration Reminders: On  ●        │
│   ▼ Reminder Settings               │  ← DisclosureGroup
│      Current Status: On Track       │
│      Reminders Today: 2/8           │
│      Limit Daily: On                │
│      Back On Track Alerts: On       │
├─────────────────────────────────────┤
│ Integrations                        │
│   Apple Health: On  ❤️              │
│   Sleep Analysis  >                 │
├─────────────────────────────────────┤
│ About                               │
│   GitHub Repository  ↗              │
└─────────────────────────────────────┘
```

**Pros:**
- All settings discoverable (nothing hidden, just collapsed)
- Device section always visible, clearly disabled when disconnected
- User can expand only what they need
- Familiar iOS pattern

**Cons:**
- Still 6 sections to scroll
- DisclosureGroups add visual complexity
- Collapse state doesn't persist between views

---

## Option 2: Essential + Advanced Split

**Approach:** Two-tier structure: essential settings up top, advanced settings in a single "Advanced" section at the bottom.

**Layout:**
```
┌─────────────────────────────────────┐
│ Device Status                       │
│   Aquavate-ABC  ● Connected         │
│   Battery: 85%  🔋                  │
├─────────────────────────────────────┤
│ Your Goals                          │
│   Daily Goal: 2000ml  >             │
│   Hydration Reminders: On  ●        │
│   Apple Health: On  ❤️              │
├─────────────────────────────────────┤
│ Device Setup                        │  ← Disabled when disconnected
│   Calibrate Bottle  >               │     (shows "Connect to configure")
│   (Required if not calibrated)      │
├─────────────────────────────────────┤
│ ▼ Advanced                          │  ← Single expandable section
│   ┌─────────────────────────────────┤
│   │ Device Commands                 │
│   │   Tare (Zero Scale)             │
│   │   Reset Daily Total             │
│   │   Sync Time                     │
│   │   Sync Drink History            │
│   │   Clear Device History          │
│   ├─────────────────────────────────┤
│   │ Device Info                     │
│   │   Current Weight: 450g          │
│   │   Calibrated: Yes               │
│   │   Time Set: Yes                 │
│   │   Shake to Empty: On            │
│   ├─────────────────────────────────┤
│   │ Reminder Options                │
│   │   Limit Daily Reminders: On     │
│   │   Back On Track Alerts: On      │
│   │   (DEBUG: Test Mode)            │
│   ├─────────────────────────────────┤
│   │ Diagnostics                     │
│   │   Sleep Mode Analysis  >        │
│   └─────────────────────────────────┤
├─────────────────────────────────────┤
│ About                               │
│   GitHub Repository  ↗              │
└─────────────────────────────────────┘
```

**Pros:**
- Clean, focused top section - most users only need these
- All advanced features in one place
- Obvious visual hierarchy
- Easier onboarding - new users see only what matters

**Cons:**
- "Calibrate Bottle" is essential for new users but might feel buried
- Grouping all advanced together loses categorical organization
- Users who frequently use advanced features must always expand

---

## Option 3: Connection-Aware Cards

**Approach:** Use card-style groupings with clear visual states for connected vs disconnected.

**Layout:**
```
┌─────────────────────────────────────┐
│ ╔═══════════════════════════════╗   │
│ ║  🔵 Aquavate-ABC              ║   │ ← Prominent connection card
│ ║  Connected • 85% Battery      ║   │
│ ╚═══════════════════════════════╝   │
├─────────────────────────────────────┤
│ Daily Goal                          │
│   2000ml                        >   │
├─────────────────────────────────────┤
│ Notifications                       │
│   Hydration Reminders          ⚙️   │ ← Tap opens Reminders detail view
│   Apple Health                  ❤️  │
├─────────────────────────────────────┤
│ Device Settings                     │
│   ┌─────────────────────────────┐   │
│   │ ⚠️ Connect bottle to access │   │ ← Banner when disconnected
│   └─────────────────────────────┘   │
│   Calibrate Bottle        [dimmed] >│ ← Visually disabled
│   Shake to Empty          [dimmed]  │
│   Device Commands         [dimmed] >│ ← NavigationLink to sub-page
│   Device Diagnostics              > │ ← Works offline
├─────────────────────────────────────┤
│ About                               │
│   GitHub Repository              ↗  │
└─────────────────────────────────────┘
```

**When Connected:**
```
│ Device Settings                     │
│   Calibrate Bottle                > │
│   Shake to Empty: On                │
│   Device Commands                 > │ ← Opens sub-page with all commands
│   Device Diagnostics              > │
```

**Pros:**
- Prominent connection status at top
- Clear disabled state with explanatory banner
- Device Commands grouped into sub-page reduces clutter
- User always knows device features exist (even when disconnected)

**Cons:**
- Adds a navigation level (Device Commands sub-page)
- Connection card takes vertical space
- Banner might feel like an error to some users

---

## Option 4: Category Tabs

**Approach:** Segmented control or tabs to separate settings into categories.

**Layout:**
```
┌─────────────────────────────────────┐
│  [General]  [Device]  [About]       │ ← Segmented control
├─────────────────────────────────────┤
│ ┌─── GENERAL TAB ───────────────┐   │
│ │ Device Status                 │   │
│ │   Aquavate-ABC  ● Connected   │   │
│ │   Battery: 85%                │   │
│ ├───────────────────────────────┤   │
│ │ Daily Goal                    │   │
│ │   2000ml                    > │   │
│ ├───────────────────────────────┤   │
│ │ Hydration Reminders           │   │
│ │   Enabled: On                 │   │
│ │   Current Status: On Track    │   │
│ │   Limit Daily: On             │   │
│ │   Back On Track: On           │   │
│ ├───────────────────────────────┤   │
│ │ Apple Health                  │   │
│ │   Sync to Health: On          │   │
│ │   Status: Connected           │   │
│ └───────────────────────────────┘   │
├─────────────────────────────────────┤
│ ┌─── DEVICE TAB ────────────────┐   │
│ │ Device Info                   │   │
│ │   Current Weight: 450g        │   │
│ │   Calibrated: Yes             │   │
│ │   Time Set: Yes               │   │
│ ├───────────────────────────────┤   │
│ │ Calibration                   │   │
│ │   Calibrate Bottle          > │   │
│ ├───────────────────────────────┤   │
│ │ Commands                      │   │
│ │   Tare (Zero Scale)           │   │
│ │   Reset Daily Total           │   │
│ │   Sync Time                   │   │
│ │   Sync Drink History          │   │
│ │   Clear Device History        │   │
│ ├───────────────────────────────┤   │
│ │ Gestures                      │   │
│ │   Shake to Empty: On          │   │
│ ├───────────────────────────────┤   │
│ │ Diagnostics                   │   │
│ │   Sleep Mode Analysis       > │   │
│ └───────────────────────────────┘   │
├─────────────────────────────────────┤
│ ┌─── ABOUT TAB ─────────────────┐   │
│ │ Aquavate v1.0                 │   │
│ │ GitHub Repository           ↗ │   │
│ └───────────────────────────────┘   │
└─────────────────────────────────────┘
```

**Pros:**
- Each tab is focused and short
- Clear mental model: General (daily use), Device (hardware), About (info)
- Device tab can be entirely disabled when disconnected
- Scales well if more settings added later

**Cons:**
- Less common iOS pattern for Settings (tabs usually for main navigation)
- Loses at-a-glance view of everything
- User must tap to discover device settings exist
- Tab bar takes vertical space

---

## Option 5: Smart Contextual Layout (Recommended)

**Approach:** Hybrid approach - minimal default view with contextual expansion and clear connection states.

**Layout - Not Connected:**
```
┌─────────────────────────────────────┐
│ ┌───────────────────────────────┐   │
│ │ 🔵 Not Connected              │   │
│ │ Pull to refresh on Home to    │   │
│ │ connect your bottle           │   │
│ └───────────────────────────────┘   │
├─────────────────────────────────────┤
│ Daily Goal                          │
│   2000ml                            │ ← Shows value but disabled
│   (Connect bottle to change)        │ ← Helpful hint
├─────────────────────────────────────┤
│ Notifications                       │
│   Hydration Reminders: On       >   │ ← NavigationLink to detail
│   Apple Health: On              ❤️  │
├─────────────────────────────────────┤
│ Device                              │
│   Setup & Commands              >   │ ← Disabled, shows lock icon
│   Diagnostics                   >   │ ← Works offline
├─────────────────────────────────────┤
│ About  >                            │
└─────────────────────────────────────┘
```

**Layout - Connected:**
```
┌─────────────────────────────────────┐
│ ┌───────────────────────────────┐   │
│ │ 🟢 Aquavate-ABC               │   │
│ │ Battery: 85%  •  Calibrated ✓ │   │
│ └───────────────────────────────┘   │
├─────────────────────────────────────┤
│ Daily Goal                          │
│   2000ml                        >   │
├─────────────────────────────────────┤
│ Notifications                       │
│   Hydration Reminders: On       >   │
│   Apple Health: On              ❤️  │
├─────────────────────────────────────┤
│ Device                              │
│   Calibrate Bottle              >   │ ← Prominent if not calibrated
│   Setup & Commands              >   │ ← NavigationLink to sub-page
│   Diagnostics                   >   │
├─────────────────────────────────────┤
│ About  >                            │
└─────────────────────────────────────┘
```

**Sub-page: Setup & Commands**
```
┌─────────────────────────────────────┐
│ < Settings     Setup & Commands     │
├─────────────────────────────────────┤
│ Device Info                         │
│   Current Weight: 450g              │
│   Time Set: Yes                     │
├─────────────────────────────────────┤
│ Gestures                            │
│   Shake to Empty: On                │
├─────────────────────────────────────┤
│ Commands                            │
│   Tare (Zero Scale)                 │
│   Reset Daily Total                 │
│   Sync Time                         │
│   Sync Drink History (3 records)    │
├─────────────────────────────────────┤
│ Danger Zone                         │
│   Clear Device History              │
└─────────────────────────────────────┘
```

**Sub-page: Hydration Reminders**
```
┌─────────────────────────────────────┐
│ < Settings     Reminders            │
├─────────────────────────────────────┤
│ Hydration Reminders                 │
│   Enabled: On                       │
│   Status: Authorized ✓              │
├─────────────────────────────────────┤
│ Current Status                      │
│   🟡 Falling Behind                 │
│   Reminders Today: 2/8              │
├─────────────────────────────────────┤
│ Options                             │
│   Limit Daily Reminders: On         │
│   Back On Track Alerts: On          │
├─────────────────────────────────────┤
│ (DEBUG: Test Mode)                  │
└─────────────────────────────────────┘
```

**Pros:**
- Main settings page is short and scannable (5 sections max)
- Device settings always visible, clearly disabled when appropriate
- Sub-pages for detail (reminders, device commands) reduce clutter
- "Calibrate Bottle" promoted to top level when needed
- Connection status is prominent with quick info (battery, calibration)
- Helpful hints when disconnected

**Cons:**
- Adds navigation depth (2 sub-pages)
- Requires building 2 new sub-views
- Slightly more complex implementation

---

## Iterative Implementation Strategy

### Approach: One Branch Per Option

Each option will be implemented on its own branch off `master`. This allows:
- Side-by-side comparison by switching branches in the simulator
- No risk of losing work
- Cherry-picking favourite elements from multiple options into a final version
- Easy cleanup — delete rejected branches when done

### Branches

| Order | Branch Name | Option |
|-------|-------------|--------|
| 1 | `settings-option1-disclosure-groups` | Collapsible Sections (DisclosureGroup) |
| 2 | `settings-option2-essential-advanced` | Essential + Advanced Split |
| 3 | `settings-option3-connection-cards` | Connection-Aware Cards |
| 4 | `settings-option4-category-tabs` | Category Tabs |
| 5 | `settings-option5-smart-contextual` | Smart Contextual (sub-pages) |

### Shared Principles (applied to ALL options)

These user preferences apply regardless of layout choice:
- **Disabled + banner when disconnected** — device settings are greyed out with an info message, never hidden
- **Calibrate Bottle always visible** — promoted to a top-level position
- **No DEBUG controls in production sections** — remove `#if DEBUG` scan/connect/disconnect buttons from the Device section entirely. The settings page should reflect the production experience.
- **Separate Debug section** — all debug-only items (BLE scan/connect/disconnect, hydration reminder test mode) collected into a single "Debug" section at the bottom, wrapped in `#if DEBUG`
- **Connection status merged into Device section** — no standalone "Device Connection" section; status shown as a compact row within the Device section
- **Keep-alive while on Settings** — cancel idle disconnect timer on appear, send periodic pings (30s) to prevent firmware sleep, restart idle timer on disappear
- **Single file change** — only `ios/Aquavate/Aquavate/Views/SettingsView.swift` (except Option 5 which needs sub-views)

### Workflow Per Option

1. Create branch from `master`
2. Implement the layout in `SettingsView.swift`
3. Build for iPhone 17 simulator to confirm it compiles
4. User reviews in simulator, gives feedback
5. Move to next option (or iterate on current one)

### After All Options Reviewed

- User picks their favourite (or a hybrid)
- Final implementation on a clean branch (e.g. `settings-redesign`)
- Create PR referencing issue #87

### Verification (each option)

1. `xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
2. Visual check: disconnected state shows disabled items + banner
3. Visual check: Calibrate Bottle visible at top level
4. Visual check: progressive disclosure works (collapse/expand or tabs)
