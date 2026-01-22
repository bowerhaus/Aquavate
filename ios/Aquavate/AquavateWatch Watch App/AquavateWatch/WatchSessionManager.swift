//
//  WatchSessionManager.swift
//  AquavateWatch
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation
import Combine
import WatchConnectivity
import WidgetKit

/// Manages communication between Apple Watch and iPhone via WatchConnectivity
class WatchSessionManager: NSObject, ObservableObject {
    static let shared = WatchSessionManager()

    // MARK: - Published Properties

    @Published var hydrationState: HydrationState?
    @Published private(set) var isReachable = false

    // MARK: - Private Properties

    private var session: WCSession?

    // MARK: - Initialization

    override init() {
        super.init()
        setupSession()
        loadFromSharedData()
    }

    private func setupSession() {
        guard WCSession.isSupported() else {
            print("[WatchSession] WCSession not supported")
            return
        }

        session = WCSession.default
        session?.delegate = self
        session?.activate()
        print("[WatchSession] Session activated")
    }

    // MARK: - Shared Data

    private func loadFromSharedData() {
        if let state = SharedDataManager.shared.loadHydrationState() {
            hydrationState = state
            print("[WatchSession] Loaded state from shared data")
        }
    }

    private func saveToSharedData(_ state: HydrationState) {
        SharedDataManager.shared.saveHydrationState(state)
    }

    // MARK: - Complication Updates

    private func reloadComplications() {
        WidgetCenter.shared.reloadAllTimelines()
        print("[WatchSession] Reloaded all widget timelines")
    }
}

// MARK: - WCSessionDelegate

extension WatchSessionManager: WCSessionDelegate {
    nonisolated func session(_ session: WCSession, activationDidCompleteWith activationState: WCSessionActivationState, error: Error?) {
        Task { @MainActor in
            if let error = error {
                print("[WatchSession] Activation failed: \(error)")
                return
            }
            print("[WatchSession] Activation complete: \(activationState.rawValue)")
            isReachable = session.isReachable
        }
    }

    nonisolated func sessionReachabilityDidChange(_ session: WCSession) {
        Task { @MainActor in
            print("[WatchSession] Reachability changed: \(session.isReachable)")
            isReachable = session.isReachable
        }
    }

    // MARK: - Receiving Data

    nonisolated func session(_ session: WCSession, didReceiveApplicationContext applicationContext: [String: Any]) {
        Task { @MainActor in
            print("[WatchSession] Received application context")
            handleReceivedData(applicationContext)
        }
    }

    nonisolated func session(_ session: WCSession, didReceiveUserInfo userInfo: [String: Any]) {
        Task { @MainActor in
            print("[WatchSession] Received user info (complication update)")
            handleReceivedData(userInfo)
            reloadComplications()
        }
    }

    nonisolated func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        Task { @MainActor in
            print("[WatchSession] Received message")
            handleReceivedData(message)
        }
    }

    // MARK: - Private Methods

    private func handleReceivedData(_ data: [String: Any]) {
        guard let stateData = data["hydrationState"] as? Data else {
            print("[WatchSession] No hydration state in received data")
            return
        }

        do {
            let state = try JSONDecoder().decode(HydrationState.self, from: stateData)
            hydrationState = state
            saveToSharedData(state)
            print("[WatchSession] Updated hydration state: \(state.dailyTotalMl)ml, urgency=\(state.urgency)")

            // Reload complications when state changes
            reloadComplications()
        } catch {
            print("[WatchSession] Failed to decode hydration state: \(error)")
        }
    }
}
