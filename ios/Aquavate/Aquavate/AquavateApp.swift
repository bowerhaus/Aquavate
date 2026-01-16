//
//  AquavateApp.swift
//  Aquavate
//
//  Created by Andy Bower on 11/01/2026.
//

import SwiftUI

@main
struct AquavateApp: App {
    @State private var showSplash = true
    @StateObject private var bleManager = BLEManager()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            if showSplash {
                SplashView()
                    .onAppear {
                        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                            withAnimation(.easeInOut(duration: 0.5)) {
                                showSplash = false
                            }
                        }
                    }
            } else {
                ContentView()
                    .environmentObject(bleManager)
            }
        }
        .onChange(of: scenePhase) { oldPhase, newPhase in
            handleScenePhaseChange(from: oldPhase, to: newPhase)
        }
    }

    private func handleScenePhaseChange(from oldPhase: ScenePhase, to newPhase: ScenePhase) {
        switch newPhase {
        case .background:
            // Disconnect when app goes to background to allow bottle to sleep
            if bleManager.connectionState.isConnected {
                bleManager.disconnect()
            }
        case .active:
            // Reconnect when app returns to foreground
            if bleManager.connectionState == .disconnected && bleManager.isBluetoothReady {
                bleManager.reconnectToLastDevice()
            }
        case .inactive:
            // Transitional state, no action needed
            break
        @unknown default:
            break
        }
    }
}
