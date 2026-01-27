//
//  BottleGraphicView.swift
//  Aquavate
//
//  Bottle graphic view faithful to the firmware e-paper display design.
//  Proportions based on firmware display.cpp drawBottleGraphic() (40x90px).
//

import SwiftUI

/// Bottle graphic view that matches the firmware e-paper display
struct BottleGraphicView: View {
    /// Fill percentage (0.0 to 1.0)
    let fillPercent: Double

    /// Whether to show "?" overlay (for calibration prompts)
    var showQuestionMark: Bool = false

    /// Target size for the bottle (scales proportionally)
    var targetHeight: CGFloat = 180

    /// Water color (default blue)
    var waterColor: Color = .blue

    // Proportions from firmware (40x90 base dimensions)
    private let baseWidth: CGFloat = 40
    private let baseHeight: CGFloat = 90

    // Component dimensions (in base units)
    private let capWidth: CGFloat = 12
    private let capHeight: CGFloat = 10
    private let neckWidth: CGFloat = 16
    private let neckHeight: CGFloat = 10
    private let bodyHeight: CGFloat = 70
    private let cornerRadius: CGFloat = 8

    var body: some View {
        let scale = targetHeight / baseHeight

        ZStack {
            // Bottle outline
            BottleOutlineShape(
                capWidth: capWidth,
                capHeight: capHeight,
                neckWidth: neckWidth,
                neckHeight: neckHeight,
                cornerRadius: cornerRadius
            )
            .stroke(Color.primary, lineWidth: 2)

            // White interior (behind water)
            BottleBodyShape(
                yOffset: (capHeight + neckHeight) * scale,
                cornerRadius: cornerRadius * scale
            )
            .fill(Color.white)
            .padding(2)

            // Water fill
            if fillPercent > 0 {
                BottleBodyShape(
                    yOffset: (capHeight + neckHeight) * scale,
                    cornerRadius: cornerRadius * scale
                )
                .fill(waterColor.opacity(0.7))
                .mask(
                    VStack(spacing: 0) {
                        Spacer()
                        Rectangle()
                            .frame(height: bodyHeight * scale * CGFloat(min(max(fillPercent, 0), 1)))
                    }
                )
                .padding(4)
            }

            // Question mark overlay for calibration
            if showQuestionMark {
                Text("?")
                    .font(.system(size: 36 * scale, weight: .bold))
                    .foregroundColor(fillPercent > 0.5 ? .white : .orange)
                    .offset(y: 15 * scale) // Center in body area
            }
        }
        .frame(width: baseWidth * scale, height: baseHeight * scale)
    }
}

// MARK: - Bottle Outline Shape

/// Custom shape for the bottle outline (cap + neck + body)
struct BottleOutlineShape: Shape {
    let capWidth: CGFloat
    let capHeight: CGFloat
    let neckWidth: CGFloat
    let neckHeight: CGFloat
    let cornerRadius: CGFloat

    func path(in rect: CGRect) -> Path {
        var path = Path()
        let scale = min(rect.width / 40, rect.height / 90)

        // Cap (centered at top)
        let scaledCapWidth = capWidth * scale
        let scaledCapHeight = capHeight * scale
        let capX = (rect.width - scaledCapWidth) / 2
        path.addRoundedRect(
            in: CGRect(x: capX, y: 0, width: scaledCapWidth, height: scaledCapHeight),
            cornerSize: CGSize(width: 2 * scale, height: 2 * scale)
        )

        // Neck (below cap)
        let scaledNeckWidth = neckWidth * scale
        let scaledNeckHeight = neckHeight * scale
        let neckX = (rect.width - scaledNeckWidth) / 2
        let neckY = scaledCapHeight
        path.addRect(CGRect(x: neckX, y: neckY, width: scaledNeckWidth, height: scaledNeckHeight))

        // Body (full width with rounded corners)
        let bodyY = scaledCapHeight + scaledNeckHeight
        let scaledBodyHeight = 70 * scale
        let scaledCornerRadius = cornerRadius * scale
        path.addRoundedRect(
            in: CGRect(x: 0, y: bodyY, width: rect.width, height: scaledBodyHeight),
            cornerSize: CGSize(width: scaledCornerRadius, height: scaledCornerRadius)
        )

        return path
    }
}

// MARK: - Bottle Body Shape

/// Custom shape for just the bottle body (used for fills)
struct BottleBodyShape: Shape {
    let yOffset: CGFloat
    let cornerRadius: CGFloat

    func path(in rect: CGRect) -> Path {
        var path = Path()

        // Body starts at yOffset and fills remaining height
        let bodyRect = CGRect(
            x: 0,
            y: yOffset,
            width: rect.width,
            height: rect.height - yOffset
        )

        path.addRoundedRect(
            in: bodyRect,
            cornerSize: CGSize(width: cornerRadius, height: cornerRadius)
        )

        return path
    }
}

// MARK: - Preview

#Preview("Empty Bottle") {
    BottleGraphicView(fillPercent: 0.0, showQuestionMark: true)
        .padding()
}

#Preview("Full Bottle") {
    BottleGraphicView(fillPercent: 1.0, showQuestionMark: true)
        .padding()
}

#Preview("Half Full") {
    BottleGraphicView(fillPercent: 0.5)
        .padding()
}

#Preview("Sizes") {
    HStack(spacing: 40) {
        BottleGraphicView(fillPercent: 0.3, targetHeight: 90)
        BottleGraphicView(fillPercent: 0.6, targetHeight: 180)
        BottleGraphicView(fillPercent: 0.9, targetHeight: 270)
    }
    .padding()
}
