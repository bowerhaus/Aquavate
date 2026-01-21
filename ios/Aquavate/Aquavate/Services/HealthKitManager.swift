//
//  HealthKitManager.swift
//  Aquavate
//
//  Created by Claude Code on 21/01/2026.
//

import Foundation
import HealthKit

@MainActor
class HealthKitManager: ObservableObject {
    private let healthStore = HKHealthStore()
    private let waterType = HKQuantityType(.dietaryWater)

    @Published var isAuthorized = false
    @Published var authorizationStatus: HKAuthorizationStatus = .notDetermined

    /// Whether HealthKit is available on this device (not available on iPad)
    var isHealthKitAvailable: Bool {
        HKHealthStore.isHealthDataAvailable()
    }

    /// User preference for whether to sync to HealthKit (stored in UserDefaults)
    @Published var isEnabled: Bool {
        didSet {
            UserDefaults.standard.set(isEnabled, forKey: "healthKitSyncEnabled")
        }
    }

    init() {
        self.isEnabled = UserDefaults.standard.bool(forKey: "healthKitSyncEnabled")
        if isHealthKitAvailable {
            checkAuthorizationStatus()
        }
    }

    /// Check current authorization status for water intake
    func checkAuthorizationStatus() {
        authorizationStatus = healthStore.authorizationStatus(for: waterType)
        isAuthorized = authorizationStatus == .sharingAuthorized
    }

    /// Request authorization to read and write water intake data
    func requestAuthorization() async throws {
        guard isHealthKitAvailable else {
            throw HealthKitError.notAvailable
        }

        let typesToShare: Set<HKSampleType> = [waterType]
        let typesToRead: Set<HKObjectType> = [waterType]

        try await healthStore.requestAuthorization(toShare: typesToShare, read: typesToRead)
        checkAuthorizationStatus()
    }

    /// Log a water intake sample to HealthKit
    /// - Parameters:
    ///   - milliliters: Amount of water in ml
    ///   - date: Timestamp of the drink
    /// - Returns: The UUID of the created HealthKit sample
    func logWater(milliliters: Int, date: Date) async throws -> UUID {
        guard isHealthKitAvailable else {
            throw HealthKitError.notAvailable
        }
        guard isAuthorized else {
            throw HealthKitError.notAuthorized
        }

        let quantity = HKQuantity(unit: .literUnit(with: .milli), doubleValue: Double(milliliters))
        let sample = HKQuantitySample(type: waterType, quantity: quantity, start: date, end: date)

        try await healthStore.save(sample)
        print("[HealthKit] Logged \(milliliters)ml water at \(date)")
        return sample.uuid
    }

    /// Delete a water intake sample from HealthKit by its UUID
    /// - Parameter uuid: The UUID of the sample to delete
    func deleteWaterSample(uuid: UUID) async throws {
        guard isHealthKitAvailable else {
            throw HealthKitError.notAvailable
        }
        guard isAuthorized else {
            throw HealthKitError.notAuthorized
        }

        let predicate = HKQuery.predicateForObject(with: uuid)

        let samples = try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<[HKSample], Error>) in
            let query = HKSampleQuery(
                sampleType: waterType,
                predicate: predicate,
                limit: 1,
                sortDescriptors: nil
            ) { _, samples, error in
                if let error = error {
                    continuation.resume(throwing: error)
                } else {
                    continuation.resume(returning: samples ?? [])
                }
            }
            healthStore.execute(query)
        }

        if let sample = samples.first {
            try await healthStore.delete(sample)
            print("[HealthKit] Deleted water sample: \(uuid)")
        } else {
            print("[HealthKit] Sample not found for deletion: \(uuid)")
        }
    }

    // MARK: - Error Types

    enum HealthKitError: LocalizedError {
        case notAuthorized
        case notAvailable

        var errorDescription: String? {
            switch self {
            case .notAuthorized:
                return "HealthKit access not authorized"
            case .notAvailable:
                return "HealthKit is not available on this device"
            }
        }
    }
}
