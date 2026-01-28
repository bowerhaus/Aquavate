# Plan: Simplify Human Figure Behind Indicator to Faded Blue

## Summary

Replace the amber/red "behind target" overlay in the human figure with a single low-opacity blue fill. Remove visual distinction between Attention and Overdue states while keeping the deficit text.

## Current Behavior

- Solid blue fill from bottom up showing actual water consumed
- Amber overlay (1-19% behind pace)
- Red gradient overlay (20%+ behind pace)
- Text showing "Xml behind" when behind target

## New Behavior

- Solid blue fill from bottom up showing actual water consumed (unchanged)
- **Single faded blue overlay (~30% opacity)** showing deficit area
- No color distinction between behind levels
- Text showing "Xml behind" when behind target (unchanged)

## Files to Modify

### 1. [HumanFigureProgressView.swift](ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift)

**Changes:**
- Remove `attentionColor` and `overdueColor` constants
- Add new `behindColor` constant (blue at ~30% opacity)
- Simplify the deficit overlay logic:
  - Remove conditional gradient (amber → red)
  - Replace with single faded blue rectangle
  - Keep height calculation logic (shows from current fill level to expected level)
- Keep `isBehindTarget` and deficit text display logic unchanged

**Lines affected:** ~52-117 (deficit zone rendering section)

### 2. No changes to other files

- [HydrationState.swift](ios/Aquavate/Aquavate/Models/HydrationState.swift) - Keep `HydrationUrgency` enum (may be used for notifications)
- [HydrationReminderService.swift](ios/Aquavate/Aquavate/Services/HydrationReminderService.swift) - Keep urgency calculations
- [HomeView.swift](ios/Aquavate/Aquavate/Views/HomeView.swift) - No changes needed (still passes urgency, view just ignores it)

## Implementation Details

Replace the gradient-based behind indicator with:

```swift
// Behind indicator - faded blue showing deficit
let behindColor = Color.blue.opacity(0.3)

// Simple rectangle fill instead of gradient
Rectangle()
    .fill(behindColor)
    .frame(width: figureWidth, height: expectedHeight)
    .mask(...)  // Same masking logic as before
```

## Verification

1. Build the iOS app: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
2. Run in simulator and verify:
   - Blue fill shows correctly for consumed water
   - When behind target, faded blue appears above the solid blue
   - Deficit text ("Xml behind") still displays
   - No amber or red colors anywhere in the figure
