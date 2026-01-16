//
//  SplashView.swift
//  Aquavate
//
//  Created by Claude Code on 16/01/2026.
//

import SwiftUI

struct SplashView: View {
    @State private var scale: CGFloat = 0.8
    @State private var opacity: Double = 0.0

    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "drop.fill")
                .font(.system(size: 80))
                .foregroundStyle(.blue)
                .scaleEffect(scale)
                .opacity(opacity)

            Text("Aquavate")
                .font(.largeTitle)
                .fontWeight(.bold)
                .opacity(opacity)
        }
        .onAppear {
            withAnimation(.easeOut(duration: 0.8)) {
                scale = 1.0
                opacity = 1.0
            }
        }
    }
}

#Preview {
    SplashView()
}
