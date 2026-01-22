//
//  WatchConnectivityManager.swift
//  Aquavate
//
//  Created by Claude Code on 22/01/2026.
//

import Foundation
import WatchConnectivity

/// Manages communication between iPhone app and Apple Watch via WatchConnectivity
@MainActor
class WatchConnectivityManager: NSObject, ObservableObject {
    static let shared = WatchConnectivityManager()

    // MARK: - Published Properties

    @Published private(set) var isWatchAppInstalled = false
    @Published private(set) var isReachable = false
    @Published private(set) var isPaired = false

    // MARK: - Private Properties

    private var session: WCSession?

    // MARK: - Initialization

    override init() {
        super.init()
        setupSession()
    }

    private func setupSession() {
        guard WCSession.isSupported() else {
            print("[WatchConnectivity] WCSession not supported on this device")
            return
        }

        session = WCSession.default
        session?.delegate = self
        session?.activate()
        print("[WatchConnectivity] Session activated")
    }

    // MARK: - Public Methods

    /// Send hydration state to Apple Watch
    func sendHydrationState(_ state: HydrationState) {
        guard let session = session, session.activationState == .activated else {
            print("[WatchConnectivity] Session not activated")
            return
        }

        guard isWatchAppInstalled else {
            print("[WatchConnectivity] Watch app not installed")
            return
        }

        do {
            let data = try JSONEncoder().encode(state)
            let context: [String: Any] = ["hydrationState": data]

            // Use updateApplicationContext for latest state (replaces previous)
            try session.updateApplicationContext(context)
            print("[WatchConnectivity] Sent hydration state via application context")
        } catch {
            print("[WatchConnectivity] Failed to send hydration state: \(error)")
        }
    }

    /// Update complication with current hydration state
    func updateComplication(with state: HydrationState) {
        guard let session = session, session.activationState == .activated else {
            print("[WatchConnectivity] Session not activated")
            return
        }

        guard isWatchAppInstalled else {
            print("[WatchConnectivity] Watch app not installed")
            return
        }

        #if !targetEnvironment(simulator)
        // transferCurrentComplicationUserInfo only available on device
        guard session.remainingComplicationUserInfoTransfers > 0 else {
            print("[WatchConnectivity] No remaining complication transfers")
            // Fall back to application context
            sendHydrationState(state)
            return
        }

        do {
            let data = try JSONEncoder().encode(state)
            let userInfo: [String: Any] = ["hydrationState": data]

            session.transferCurrentComplicationUserInfo(userInfo)
            print("[WatchConnectivity] Sent complication update")
        } catch {
            print("[WatchConnectivity] Failed to encode complication data: \(error)")
        }
        #else
        // On simulator, just use application context
        sendHydrationState(state)
        #endif
    }

    /// Send an immediate message to Watch (only works when Watch app is reachable)
    func sendImmediateMessage(_ message: [String: Any]) {
        guard let session = session, session.isReachable else {
            print("[WatchConnectivity] Watch not reachable for immediate message")
            return
        }

        session.sendMessage(message, replyHandler: nil) { error in
            print("[WatchConnectivity] Failed to send message: \(error)")
        }
    }
}

// MARK: - WCSessionDelegate

extension WatchConnectivityManager: WCSessionDelegate {
    nonisolated func session(_ session: WCSession, activationDidCompleteWith activationState: WCSessionActivationState, error: Error?) {
        Task { @MainActor in
            if let error = error {
                print("[WatchConnectivity] Activation failed: \(error)")
                return
            }

            print("[WatchConnectivity] Activation complete: \(activationState.rawValue)")
            updateSessionState(session)
        }
    }

    nonisolated func sessionDidBecomeInactive(_ session: WCSession) {
        print("[WatchConnectivity] Session became inactive")
    }

    nonisolated func sessionDidDeactivate(_ session: WCSession) {
        print("[WatchConnectivity] Session deactivated")
        // Reactivate for switching Watch devices
        Task { @MainActor in
            self.session?.activate()
        }
    }

    nonisolated func sessionWatchStateDidChange(_ session: WCSession) {
        Task { @MainActor in
            print("[WatchConnectivity] Watch state changed")
            updateSessionState(session)
        }
    }

    nonisolated func sessionReachabilityDidChange(_ session: WCSession) {
        Task { @MainActor in
            print("[WatchConnectivity] Reachability changed: \(session.isReachable)")
            self.isReachable = session.isReachable
        }
    }

    // MARK: - Receiving Messages

    nonisolated func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        Task { @MainActor in
            print("[WatchConnectivity] Received message: \(message)")
            // Handle messages from Watch if needed
        }
    }

    nonisolated func session(_ session: WCSession, didReceiveApplicationContext applicationContext: [String: Any]) {
        Task { @MainActor in
            print("[WatchConnectivity] Received application context from Watch")
            // Handle context updates from Watch if needed
        }
    }

    // MARK: - Private Methods

    private func updateSessionState(_ session: WCSession) {
        isPaired = session.isPaired
        isWatchAppInstalled = session.isWatchAppInstalled
        isReachable = session.isReachable
        print("[WatchConnectivity] State - paired: \(isPaired), installed: \(isWatchAppInstalled), reachable: \(isReachable)")
    }
}
