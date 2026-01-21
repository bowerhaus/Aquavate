//
//  ContentView.swift
//  Aquavate
//
//  Created by Andy Bower on 11/01/2026.
//

import SwiftUI

struct ContentView: View {
    #if DEBUG
    @State private var showDebug = false
    #endif

    var body: some View {
        TabView {
            HomeView()
                .tabItem {
                    Label("Home", systemImage: "drop.fill")
                }

            HistoryView()
                .tabItem {
                    Label("History", systemImage: "chart.bar.fill")
                }

            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gear")
                }

            #if DEBUG
            DebugView()
                .tabItem {
                    Label("Debug", systemImage: "ladybug.fill")
                }
            #endif
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(BLEManager())
        .environmentObject(HealthKitManager())
}
