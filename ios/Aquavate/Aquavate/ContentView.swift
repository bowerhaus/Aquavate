//
//  ContentView.swift
//  Aquavate
//
//  Created by Andy Bower on 11/01/2026.
//

import SwiftUI

struct ContentView: View {
    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "drop.fill")
                .font(.system(size: 60))
                .foregroundStyle(.blue)

            Text("Aquavate")
                .font(.largeTitle)
                .fontWeight(.bold)

            Text("Hello, World!")
                .font(.title2)
                .foregroundStyle(.secondary)
        }
        .padding()
    }
}

#Preview {
    ContentView()
}
