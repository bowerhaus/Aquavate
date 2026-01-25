# Plan: Fix Orange Line Rounding Issue (Issue #48) ✅ COMPLETE

## Problem Summary

A thin orange line appears above the blue fill in the human figure when the user is slightly behind pace (1-24ml), even though no "behind target" text is displayed. This creates confusing visual feedback.

## Root Cause

Inconsistent thresholds between visual overlay and text display:

| Component | Condition | Threshold |
|-----------|-----------|-----------|
| Orange overlay | `expectedProgress > progress` | 1ml (exact comparison) |
| "Behind target" text | `roundedDeficitMl >= 50` | 50ml (rounded) |

Small deficits trigger the overlay but round to 0ml for text, causing the mismatch.

## Implementation

### File to Modify

`ios/Aquavate/Aquavate/Components/HumanFigureProgressView.swift`

### Change

**Line 60**, change:
```swift
if expectedCurrent != nil && expectedProgress > progress {
```

To:
```swift
if expectedCurrent != nil && isBehindTarget {
```

## Verification

1. Build iOS app in Xcode
2. Test with SwiftUI previews - verify "On Track" preview shows no orange
3. Test on device/simulator with small deficits (< 25ml) - should show no orange
4. Test with deficits >= 25ml (rounds to 50) - should show orange overlay and text together
