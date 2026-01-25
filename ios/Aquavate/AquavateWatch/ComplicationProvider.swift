//
//  ComplicationProvider.swift
//  AquavateWatch
//
//  Created by Claude Code on 22/01/2026.
//

import SwiftUI
import WidgetKit

// MARK: - Timeline Entry

struct HydrationEntry: TimelineEntry {
    let date: Date
    let state: HydrationState?
}

// MARK: - Timeline Provider

struct HydrationTimelineProvider: TimelineProvider {
    typealias Entry = HydrationEntry

    func placeholder(in context: Context) -> HydrationEntry {
        HydrationEntry(date: Date(), state: nil)
    }

    func getSnapshot(in context: Context, completion: @escaping (HydrationEntry) -> Void) {
        // Load from shared data
        let state = SharedDataManager.shared.loadHydrationState()
        let entry = HydrationEntry(date: Date(), state: state)
        completion(entry)
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<HydrationEntry>) -> Void) {
        // Load from shared data
        let state = SharedDataManager.shared.loadHydrationState()
        let entry = HydrationEntry(date: Date(), state: state)

        // Update every 15 minutes
        let nextUpdate = Calendar.current.date(byAdding: .minute, value: 15, to: Date())!
        let timeline = Timeline(entries: [entry], policy: .after(nextUpdate))
        completion(timeline)
    }
}

// MARK: - Complication Views

struct GraphicCircularView: View {
    let entry: HydrationEntry

    private var progress: Double {
        entry.state?.progress ?? 0
    }

    private var color: Color {
        entry.state?.urgency.color ?? .blue
    }

    var body: some View {
        Gauge(value: progress) {
            Image(systemName: "drop.fill")
                .foregroundColor(color)
        }
        .gaugeStyle(.accessoryCircularCapacity)
        .tint(color)
    }
}

struct GraphicCornerView: View {
    let entry: HydrationEntry

    private var progress: Double {
        entry.state?.progress ?? 0
    }

    private var color: Color {
        entry.state?.urgency.color ?? .blue
    }

    private var percentText: String {
        "\(Int(progress * 100))%"
    }

    var body: some View {
        VStack(alignment: .leading) {
            Image(systemName: "drop.fill")
                .foregroundColor(color)
            Text(percentText)
                .font(.caption)
        }
    }
}

struct GraphicRectangularView: View {
    let entry: HydrationEntry

    private var liters: Double {
        Double(entry.state?.dailyTotalMl ?? 0) / 1000.0
    }

    private var goalLiters: Double {
        Double(entry.state?.dailyGoalMl ?? 2000) / 1000.0
    }

    private var progress: Double {
        entry.state?.progress ?? 0
    }

    private var color: Color {
        entry.state?.urgency.color ?? .blue
    }

    var body: some View {
        HStack {
            Image(systemName: "drop.fill")
                .font(.title2)
                .foregroundColor(color)

            VStack(alignment: .leading) {
                Text("Hydration")
                    .font(.caption)
                    .foregroundColor(.secondary)
                Text("\(liters, specifier: "%.1f") / \(goalLiters, specifier: "%.1f")L")
                    .font(.headline)
                ProgressView(value: progress)
                    .tint(color)
            }
        }
    }
}

// MARK: - Widget Configuration

struct AquavateComplication: Widget {
    let kind: String = "AquavateComplication"

    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: HydrationTimelineProvider()) { entry in
            ComplicationEntryView(entry: entry)
        }
        .configurationDisplayName("Hydration")
        .description("Track your daily water intake")
        .supportedFamilies([
            .accessoryCircular,
            .accessoryCorner,
            .accessoryRectangular
        ])
    }
}

struct ComplicationEntryView: View {
    @Environment(\.widgetFamily) var family
    let entry: HydrationEntry

    var body: some View {
        switch family {
        case .accessoryCircular:
            GraphicCircularView(entry: entry)
        case .accessoryCorner:
            GraphicCornerView(entry: entry)
        case .accessoryRectangular:
            GraphicRectangularView(entry: entry)
        default:
            GraphicCircularView(entry: entry)
        }
    }
}

// MARK: - Previews

#Preview("Circular", as: .accessoryCircular) {
    AquavateComplication()
} timeline: {
    HydrationEntry(date: Date(), state: nil)
}

#Preview("Corner", as: .accessoryCorner) {
    AquavateComplication()
} timeline: {
    HydrationEntry(date: Date(), state: nil)
}

#Preview("Rectangular", as: .accessoryRectangular) {
    AquavateComplication()
} timeline: {
    HydrationEntry(date: Date(), state: nil)
}
