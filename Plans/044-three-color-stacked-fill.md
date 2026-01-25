# Plan: Three-Color Stacked Human Figure Fill (Issue #50)

## Overview

Modify `HumanFigureProgressView` to show three stacked color zones when behind target:
- **Blue**: Actual intake progress (bottom)
- **Orange**: Deficit up to the 20% overdue threshold
- **Red**: Deficit beyond 20% (only when significantly behind)

## Current Implementation

In [HumanFigureProgressView.swift](ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift):
- Blue rectangle masked to human figure shows actual progress
- Single "expected progress fill" rectangle shows the entire deficit in one color
- Color determined by `urgency.color` (orange for attention, red for overdue)

## Implementation Approach

### Key Calculations

The overdue threshold is 20% behind pace. We need to calculate:
1. `actualProgress` - current intake as fraction of goal (existing)
2. `expectedProgress` - expected intake as fraction of goal (existing)
3. `overdueThresholdProgress` - the point where orange ends and red begins

The overdue threshold in terms of progress:
```
overdueThresholdProgress = expectedProgress * 0.8
```
(20% behind expected means you're at 80% of expected)

### Visual Layer Structure (bottom to top)

1. **Red layer** (if deficit > 20%): from `overdueThresholdProgress` to `expectedProgress`
2. **Orange layer** (if any deficit): from `actualProgress` to `min(expectedProgress, overdueThresholdProgress)`
3. **Blue layer**: from 0 to `actualProgress`
4. **Outline**: on top

### Code Changes

Add computed properties:
```swift
/// Progress level at which overdue begins (80% of expected = 20% behind)
private var overdueThresholdProgress: Double {
    expectedProgress * 0.8
}

/// Whether deficit exceeds the overdue threshold (20%+)
private var isOverdue: Bool {
    progress < overdueThresholdProgress
}
```

Replace single expected progress fill with two layers:

**Orange layer** - shows deficit up to overdue threshold:
```swift
// Orange zone: from actual to overdue threshold (or expected if not overdue)
if isBehindTarget {
    let orangeTop = isOverdue ? overdueThresholdProgress : expectedProgress
    let orangeHeight = max(0, orangeTop - progress)
    // Rectangle from progress to orangeTop
}
```

**Red layer** - shows deficit beyond overdue threshold:
```swift
// Red zone: from overdue threshold to expected (only when 20%+ behind)
if isBehindTarget && isOverdue {
    let redHeight = expectedProgress - overdueThresholdProgress
    // Rectangle from overdueThresholdProgress to expectedProgress
}
```

## File to Modify

- [ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift](ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift)

## Verification

1. Build the iOS app: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 16' build`
2. Check SwiftUI previews show correct behavior:
   - "Attention" preview (15% behind): blue + orange only
   - "Overdue" preview (30%+ behind): blue + orange + red
   - "On Track" preview: blue only
3. Manual testing in simulator with various deficit levels
