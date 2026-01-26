# Fix: Unified Sessions View for Activity Stats

**Status:** ✅ Complete (2026-01-26)
**Issue:** [#74](https://github.com/bowerhaus/Aquavate/issues/74)

## Problem
The current Activity Stats view has two confusing separate sections:
1. "Recent Motion Wakes" - shows wake periods with misleading "Backpack" badge
2. "Backpack Sessions" - shows actual backpack mode durations

Users see a short duration (3m 26s) with "Backpack" label and think it's the backpack duration, when it's actually the wake period before entering backpack mode.

## Solution
Replace the two separate sections with a **single unified "Sessions" list** showing all activity chronologically:

| Timestamp | Duration | Type | Drink |
|-----------|----------|------|-------|
| 10:30 AM | 2m 15s | Normal | 💧 |
| 10:33 AM | 1h 23m | Backpack | - |
| 11:56 AM | 45s | Normal | - |

Each row shows:
- **When**: Session start time/date
- **Duration**: How long the session lasted
- **Type**: "Normal" (awake/active) or "Backpack" (extended sleep)
- **Drink**: Water drop icon if a drink was taken during Normal sessions

## Implementation

### File: [ios/Aquavate/Aquavate/Views/ActivityStatsView.swift](ios/Aquavate/Aquavate/Views/ActivityStatsView.swift)

1. **Create a unified session model** to merge both data types:
```swift
enum SessionType {
    case normal(drinkTaken: Bool)
    case backpack(timerWakes: Int16)
}

struct UnifiedSession: Identifiable {
    let id: UUID
    let timestamp: Date
    let durationSec: Int32
    let type: SessionType
}
```

2. **Create computed property** to merge and sort both CoreData collections:
```swift
private var unifiedSessions: [UnifiedSession] {
    var sessions: [UnifiedSession] = []

    // Add motion wake events as "Normal" sessions
    for event in motionWakeEvents {
        sessions.append(UnifiedSession(
            id: event.id ?? UUID(),
            timestamp: event.timestamp ?? Date(),
            durationSec: event.durationSec,
            type: .normal(drinkTaken: event.drinkTaken)
        ))
    }

    // Add backpack sessions
    for session in backpackSessions {
        sessions.append(UnifiedSession(
            id: session.id ?? UUID(),
            timestamp: session.startTimestamp ?? Date(),
            durationSec: session.durationSec,
            type: .backpack(timerWakes: session.timerWakeCount)
        ))
    }

    // Sort by timestamp descending (most recent first)
    return sessions.sorted { $0.timestamp > $1.timestamp }
}
```

3. **Replace the two sections** with a single "Sessions" section (filtered to last 7 days):
```swift
if !sessionsLast7Days.isEmpty {
    Section("Sessions") {
        ForEach(sessionsLast7Days) { session in
            HStack {
                // Timestamp
                VStack(alignment: .leading, spacing: 2) {
                    Text(session.timestamp, style: .time)
                        .font(.headline)
                    Text(session.timestamp, style: .date)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                // Drink indicator (for normal sessions)
                if case .normal(let drinkTaken) = session.type, drinkTaken {
                    Image(systemName: "drop.fill")
                        .foregroundStyle(.blue)
                        .font(.caption)
                }

                Spacer()

                // Duration and type
                VStack(alignment: .trailing, spacing: 2) {
                    Text(formatDuration(Int(session.durationSec)))
                        .font(.subheadline)

                    // Session type badge
                    switch session.type {
                    case .normal:
                        Text("Normal")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    case .backpack(let timerWakes):
                        HStack(spacing: 2) {
                            Image(systemName: "bag")
                            Text("Backpack")
                            if timerWakes > 0 {
                                Text("(\(timerWakes))")
                            }
                        }
                        .font(.caption)
                        .foregroundStyle(.orange)
                    }
                }
            }
            .padding(.vertical, 2)
        }
    }
}
```

4. **Update summary section** to show last 7 days totals:
```swift
// Computed property for 7-day filtering
private var sessionsLast7Days: [UnifiedSession] {
    let sevenDaysAgo = Calendar.current.date(byAdding: .day, value: -7, to: Date()) ?? Date()
    return unifiedSessions.filter { $0.timestamp >= sevenDaysAgo }
}

private var normalSessionsLast7Days: Int {
    sessionsLast7Days.filter {
        if case .normal = $0.type { return true }
        return false
    }.count
}

private var backpackSessionsLast7Days: Int {
    sessionsLast7Days.filter {
        if case .backpack = $0.type { return true }
        return false
    }.count
}

// Summary section
Section("Last 7 Days") {
    HStack {
        Image(systemName: "hand.raised.fill")
            .foregroundStyle(.blue)
        Text("Normal Sessions")
        Spacer()
        Text("\(normalSessionsLast7Days)")
            .foregroundStyle(.secondary)
    }
    HStack {
        Image(systemName: "bag")
            .foregroundStyle(.orange)
        Text("Backpack Sessions")
        Spacer()
        Text("\(backpackSessionsLast7Days)")
            .foregroundStyle(.secondary)
    }
}
```

## Verification
1. Build the iOS app: `cd ios/Aquavate && xcodebuild -scheme Aquavate -destination 'platform=iOS Simulator,name=iPhone 17' build`
2. Run in simulator and connect to bottle
3. Verify Activity Stats shows single "Sessions" section with both types interleaved chronologically
4. Verify Normal sessions show water drop when drink was taken
5. Verify Backpack sessions show correct 1+ hour durations with timer wake counts
