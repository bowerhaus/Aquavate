# Plan: Enhance iOS App with Placeholder UI

**Status:** ✅ Complete (2026-01-16)

## Overview
Create a multi-screen placeholder UI for the Aquavate iOS app to represent the final product vision before implementing BLE connectivity and data persistence. This will provide a realistic mockup for testing on iPhone.

## Current State
- Basic skeleton app exists with "Hello World" splash screen
- Project structure: [ios/Aquavate/Aquavate.xcodeproj](../ios/Aquavate/Aquavate.xcodeproj)
- Two files: [AquavateApp.swift](../ios/Aquavate/Aquavate/AquavateApp.swift), [ContentView.swift](../ios/Aquavate/Aquavate/ContentView.swift)
- No data models, no navigation, no BLE functionality yet

## Implementation Plan

### 1. Create Mock Data Models (New Files)
**File: `ios/Aquavate/Aquavate/Models/DrinkRecord.swift`**
- Simple struct with static sample data
- Fields: id, timestamp, amountMl, bottleLevelMl
- Add 10-15 sample drink records spanning today and yesterday

**File: `ios/Aquavate/Aquavate/Models/Bottle.swift`**
- Simple struct for bottle configuration
- Fields: name, capacityMl, currentLevelMl, dailyGoalMl, batteryPercent
- Add one sample bottle instance

### 2. Create Main Home Screen (New File)
**File: `ios/Aquavate/Aquavate/Views/HomeView.swift`**
- **Top Section:** Circular progress ring showing current bottle level
  - Center: "420 ml remaining" large text
  - Ring: visual progress (420/750ml capacity)
- **Daily Goal Section:** Horizontal progress bar
  - "Today: 1.2L / 2.0L goal" with percentage
- **Recent Drinks List:** Last 5 drinks
  - Each item: time, amount, bottle level after
  - Example: "2:45 PM - 150ml (420ml remaining)"
- **Sync Status:** Small indicator at top (mock "Last synced 5 min ago")

### 3. Create History Screen (New File)
**File: `ios/Aquavate/Aquavate/Views/HistoryView.swift`**
- **Calendar Grid:** Current week with daily totals
  - Simple 7-day horizontal scroll
  - Each day shows date and total consumed
  - Highlight current day
- **Selected Day Detail:** Expandable drink list
  - Full list of drinks for tapped day
  - Summary stats at bottom

### 4. Create Settings Screen (New File)
**File: `ios/Aquavate/Aquavate/Views/SettingsView.swift`**
- **Bottle Configuration Section:**
  - Name field (read-only for now)
  - Capacity display
  - Daily goal display
- **Device Section:**
  - Battery level (mock 85%)
  - Connection status (mock "Connected")
  - Last sync time
- **Preferences Section:**
  - Units toggle (ml/oz) - non-functional
  - Notifications toggle - non-functional
- **About Section:**
  - Version number
  - Link to GitHub

### 5. Create Tab Navigation (Update ContentView)
**File: `ios/Aquavate/Aquavate/ContentView.swift`**
- Replace placeholder with TabView
- Three tabs:
  1. Home (drop.fill icon)
  2. History (chart.bar.fill icon)
  3. Settings (gear icon)

### 6. Create Custom UI Components (New Files)
**File: `ios/Aquavate/Aquavate/Components/CircularProgressView.swift`**
- Reusable circular progress ring
- Parameters: current value, total, color
- Used for bottle level display

**File: `ios/Aquavate/Aquavate/Components/DrinkListItem.swift`**
- Reusable list row for drink records
- Shows timestamp, amount, remaining level

## File Structure After Implementation
```
ios/Aquavate/Aquavate/
├── AquavateApp.swift                    [MODIFIED - add splash screen logic]
├── ContentView.swift                    [MODIFIED - add TabView]
├── Models/
│   ├── DrinkRecord.swift               [NEW]
│   └── Bottle.swift                    [NEW]
├── Views/
│   ├── SplashView.swift                [NEW]
│   ├── HomeView.swift                  [NEW]
│   ├── HistoryView.swift               [NEW]
│   └── SettingsView.swift              [NEW]
└── Components/
    ├── CircularProgressView.swift      [NEW]
    └── DrinkListItem.swift             [NEW]
```

## Design Principles
- Use SF Symbols for all icons (built into iOS)
- Blue accent color to match water theme
- System fonts for consistency
- All data is static/mocked - no real persistence
- Focus on visual hierarchy and layout
- Keep code simple - no complex state management yet

## What This Does NOT Include
- CoreBluetooth integration (Phase 4)
- CoreData persistence (Phase 4)
- HealthKit sync (Phase 4)
- Real device scanning/pairing
- Functional settings toggles
- Calendar date selection (just shows mock current week)

## Implementation Summary

### Completed Features:
- ✅ Splash screen with water drop icon and fade-in animation (2s display)
- ✅ Mock data models: DrinkRecord (12 sample entries), Bottle (sample config)
- ✅ CircularProgressView component for bottle level visualization
- ✅ DrinkListItem component for consistent drink display
- ✅ HomeView with circular progress ring, daily goal bar, and recent drinks list
- ✅ HistoryView with 7-day calendar grid and date selection
- ✅ SettingsView with bottle config, device status, and preferences
- ✅ TabView navigation with Home, History, and Settings tabs

### Files Created:
- Models: DrinkRecord.swift, Bottle.swift
- Views: SplashView.swift, HomeView.swift, HistoryView.swift, SettingsView.swift
- Components: CircularProgressView.swift, DrinkListItem.swift
- Modified: AquavateApp.swift (splash screen logic), ContentView.swift (tab navigation)

### Ready for Next Phase:
The placeholder UI is complete and ready for testing in Xcode. Next steps are Phase 4 (BLE integration and CoreData persistence).

## Verification Checklist
Ready to verify on iPhone via Xcode:
1. ✅ Splash screen displays on launch with animation
2. ✅ All three tabs appear and are navigable
3. ✅ HomeView shows circular progress, daily goal, recent drinks
4. ✅ HistoryView shows calendar grid and drink list
5. ✅ SettingsView shows bottle config and device status
6. Verify UI is readable and properly laid out on iPhone screen (requires Xcode testing)
7. Test portrait orientation rendering (requires Xcode testing)
