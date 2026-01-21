# HealthKit Water Intake Integration Plan

## Overview
Integrate Apple HealthKit to sync individual drink records as water intake samples. Each drink logged from the bottle creates a separate HealthKit sample with its timestamp. Deletions remove the corresponding HealthKit sample.

## Complexity: Low-Moderate
- HealthKit water intake is a straightforward API
- CoreData already has `syncedToHealth` field (partial foundation)
- Main work: HealthKitManager service + wiring into existing flows

## Developer Account
✅ **Free Apple Developer account works** for development and personal device testing. Paid account ($99/year) only needed for App Store distribution.

---

## Files to Modify

| File | Changes |
|------|---------|
| `Aquavate.xcdatamodel/contents` | Add `healthKitSampleUUID` (UUID) to CDDrinkRecord |
| **New:** `Services/HealthKitManager.swift` | HealthKit authorization + read/write/delete |
| `CoreData/PersistenceController.swift` | Update save/delete to include HealthKit UUID |
| `AquavateApp.swift` | Initialize HealthKitManager, inject into environment |
| `Views/SettingsView.swift` | Add HealthKit authorization toggle/status |
| `Info.plist` | Add HealthKit usage descriptions |
| `Aquavate.entitlements` | Enable HealthKit capability |

---

## Implementation Steps

### Step 1: Xcode Project Configuration
1. Open project in Xcode → Signing & Capabilities → Add "HealthKit"
2. Add to `Info.plist`:
   ```xml
   <key>NSHealthShareUsageDescription</key>
   <string>Aquavate reads your water intake history to avoid duplicates.</string>
   <key>NSHealthUpdateUsageDescription</key>
   <string>Aquavate logs your water intake from your smart bottle to Apple Health.</string>
   ```

### Step 2: CoreData Schema Update
Add `healthKitSampleUUID` (Optional UUID) to `CDDrinkRecord` entity.

### Step 3: Create HealthKitManager Service (~80 lines)
```swift
// Services/HealthKitManager.swift
import HealthKit
import Foundation

@MainActor
class HealthKitManager: ObservableObject {
    private let healthStore = HKHealthStore()
    private let waterType = HKQuantityType(.dietaryWater)

    @Published var isAuthorized = false
    @Published var authorizationStatus: HKAuthorizationStatus = .notDetermined

    var isHealthKitAvailable: Bool {
        HKHealthStore.isHealthDataAvailable()
    }

    func checkAuthorizationStatus() {
        authorizationStatus = healthStore.authorizationStatus(for: waterType)
        isAuthorized = authorizationStatus == .sharingAuthorized
    }

    func requestAuthorization() async throws {
        let typesToShare: Set<HKSampleType> = [waterType]
        let typesToRead: Set<HKObjectType> = [waterType]
        try await healthStore.requestAuthorization(toShare: typesToShare, read: typesToRead)
        checkAuthorizationStatus()
    }

    func logWater(milliliters: Int, date: Date) async throws -> UUID {
        guard isAuthorized else { throw HealthKitError.notAuthorized }

        let quantity = HKQuantity(unit: .literUnit(with: .milli), doubleValue: Double(milliliters))
        let sample = HKQuantitySample(type: waterType, quantity: quantity, start: date, end: date)
        try await healthStore.save(sample)
        return sample.uuid
    }

    func deleteWaterSample(uuid: UUID) async throws {
        guard isAuthorized else { throw HealthKitError.notAuthorized }

        let predicate = HKQuery.predicateForObject(with: uuid)
        let samples = try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<[HKSample], Error>) in
            let query = HKSampleQuery(sampleType: waterType, predicate: predicate, limit: 1, sortDescriptors: nil) { _, samples, error in
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
        }
    }

    enum HealthKitError: LocalizedError {
        case notAuthorized
        case notAvailable

        var errorDescription: String? {
            switch self {
            case .notAuthorized: return "HealthKit access not authorized"
            case .notAvailable: return "HealthKit is not available on this device"
            }
        }
    }
}
```

### Step 4: Update PersistenceController
Add methods to:
- `markDrinkSyncedToHealth(id: UUID, healthKitUUID: UUID)` - Update after HealthKit save
- Modify `deleteDrinkRecord` to return the healthKitSampleUUID before deletion

### Step 5: Wire Into Drink Sync Flow
In `BLEManager.saveDrinkRecords()` or after CoreData save:
```swift
// After saving drinks to CoreData
for drink in newDrinks where !drink.syncedToHealth {
    if healthKitManager.isAuthorized {
        do {
            let hkUUID = try await healthKitManager.logWater(
                milliliters: Int(drink.amountMl),
                date: drink.timestamp
            )
            persistenceController.markDrinkSyncedToHealth(id: drink.id, healthKitUUID: hkUUID)
        } catch {
            // Log error, drink stays with syncedToHealth=false for retry
        }
    }
}
```

### Step 6: Wire Into Delete Flow
In the existing pessimistic delete flow (after bottle confirms deletion):
```swift
// Before deleting from CoreData, get the HealthKit UUID
if let hkUUID = drink.healthKitSampleUUID {
    try? await healthKitManager.deleteWaterSample(uuid: hkUUID)
}
// Then delete from CoreData
```

### Step 7: Settings UI
Add HealthKit section to SettingsView:
```swift
Section("Apple Health") {
    if healthKitManager.isHealthKitAvailable {
        Toggle("Sync to Health", isOn: $healthKitEnabled)
            .onChange(of: healthKitEnabled) { enabled in
                if enabled {
                    Task { try? await healthKitManager.requestAuthorization() }
                }
            }

        if healthKitManager.isAuthorized {
            Label("Connected", systemImage: "checkmark.circle.fill")
                .foregroundColor(.green)
        }
    } else {
        Text("HealthKit not available on this device")
            .foregroundColor(.secondary)
    }
}
```

### Step 8: Backfill Historical Data (Optional)
Add option in settings to sync existing drinks that have `syncedToHealth=false`.

---

## Verification

1. **Build & run** on physical device (HealthKit requires real device)
2. **Enable HealthKit** in Settings - verify authorization prompt appears
3. **Sync drinks from bottle** - verify they appear in Apple Health app under Nutrition → Water
4. **Delete a drink** in Aquavate - verify it's removed from Apple Health
5. **Check timestamps** - drinks should show correct time in Health app
6. **Test iPad** - verify graceful degradation (HealthKit not available message)

---

## Design Decisions

- **Opt-in from Settings**: User must manually enable HealthKit sync. No permission prompts on first launch.
- **Individual drinks**: Each drink creates a separate HealthKit sample with timestamp (not daily totals).
- **UUID tracking**: Store HealthKit sample UUID to enable precise deletion.

---

## Edge Cases Handled

| Scenario | Handling |
|----------|----------|
| HealthKit not available (iPad) | Check `isHealthDataAvailable()`, hide toggle |
| User denies authorization | Show status, allow retry from Settings |
| HealthKit write fails | Keep `syncedToHealth=false`, retry on next sync |
| Delete fails in HealthKit | Log error, continue with CoreData delete |
| Historical drinks pre-HealthKit | Backfill option in settings |
