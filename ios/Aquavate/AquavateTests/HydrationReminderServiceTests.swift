import XCTest
@testable import Aquavate

// Tests for HydrationReminderService covering pace, deficit, urgency, and throttle reset.
//
// The service is @MainActor so the test class must be too.
// updateState() internally calls syncStateToWatch() — Watch/AppGroup calls fail
// silently in test context (no WCSession, no App Group) which is harmless.

@MainActor
final class HydrationReminderServiceTests: XCTestCase {

    var service: HydrationReminderService!

    override func setUp() {
        service = HydrationReminderService()
    }

    override func tearDown() {
        service = nil  // deinit invalidates the periodic evaluation timer
    }

    // Build a Date at a specific wall-clock hour using the same calendar the service uses.
    private func makeDate(hour: Int, minute: Int = 0) -> Date {
        var comps = Calendar.current.dateComponents([.year, .month, .day], from: Date())
        comps.hour = hour
        comps.minute = minute
        comps.second = 0
        return Calendar.current.date(from: comps)!
    }

    // MARK: - roundToNearest50

    func testRound_exactMultiple_unchanged() {
        XCTAssertEqual(HydrationReminderService.roundToNearest50(200), 200)
    }

    func testRound_justBelow_roundsDown() {
        XCTAssertEqual(HydrationReminderService.roundToNearest50(224), 200)
    }

    func testRound_atMidpoint_roundsUp() {
        XCTAssertEqual(HydrationReminderService.roundToNearest50(225), 250)
    }

    func testRound_zero_isZero() {
        XCTAssertEqual(HydrationReminderService.roundToNearest50(0), 0)
    }

    // MARK: - Pace (calculateExpectedIntake)
    // Active hours: 7am–10pm (15-hour window); goal = 2500ml default

    func testExpectedIntake_beforeActiveHours_isZero() {
        service.now = { self.makeDate(hour: 6) }
        XCTAssertEqual(service.calculateExpectedIntake(), 0)
    }

    func testExpectedIntake_atActiveHoursStart_isZero() {
        service.now = { self.makeDate(hour: 7, minute: 0) }
        XCTAssertEqual(service.calculateExpectedIntake(), 0)
    }

    func testExpectedIntake_atActiveHoursEnd_isGoal() {
        service.now = { self.makeDate(hour: 22) }
        XCTAssertEqual(service.calculateExpectedIntake(), service.dailyGoalMl)
    }

    func testExpectedIntake_atNoon_isOneThirdOfGoal() {
        // Noon = 5h into 15h window → 300/900 * 2500 = 833ml (integer division)
        service.now = { self.makeDate(hour: 12, minute: 0) }
        let expected = (5 * 60 * service.dailyGoalMl) / (15 * 60)
        XCTAssertEqual(service.calculateExpectedIntake(), expected)
    }

    // MARK: - Deficit (calculateDeficit)

    func testDeficit_zeroDrinkAtDayStart_isZero() {
        // No intake expected before active hours — deficit is 0
        service.now = { self.makeDate(hour: 7) }
        // dailyTotalMl is 0 by default; no updateState needed
        XCTAssertEqual(service.calculateDeficit(), 0)
    }

    func testDeficit_zeroDrinkAtAfternoon_isPositive() {
        // 2pm = 7h in → expected ~1167ml; 0 consumed → large positive deficit
        service.now = { self.makeDate(hour: 14) }
        XCTAssertGreaterThan(service.calculateDeficit(), 0)
    }

    // MARK: - Urgency

    func testUrgency_onPaceAtDayStart_isOnTrack() {
        service.now = { self.makeDate(hour: 7) }
        service.updateState(totalMl: 0, goalMl: 2500, lastDrink: nil)
        XCTAssertEqual(service.currentUrgency, .onTrack)
    }

    func testUrgency_goalAchieved_isOnTrack() {
        service.now = { self.makeDate(hour: 14) }
        service.updateState(totalMl: 2500, goalMl: 2500, lastDrink: nil)
        XCTAssertEqual(service.currentUrgency, .onTrack)
    }

    func testUrgency_slightlyBehind_isAttention() {
        // 10am = 3h in → expected = 500ml; 400ml consumed → 100ml deficit
        // 100ml < 20% threshold (500ml) → attention
        service.now = { self.makeDate(hour: 10) }
        service.updateState(totalMl: 400, goalMl: 2500, lastDrink: nil)
        XCTAssertEqual(service.currentUrgency, .attention)
    }

    func testUrgency_twentyPercentBehind_isOverdue() {
        // 2pm = 7h in → expected = 1167ml; 0ml consumed → deficit 1167ml
        // 1167ml >= 20% of 2500 (500ml) → overdue
        service.now = { self.makeDate(hour: 14) }
        service.updateState(totalMl: 0, goalMl: 2500, lastDrink: nil)
        XCTAssertEqual(service.currentUrgency, .overdue)
    }

    // MARK: - Throttle reset (checkBackOnTrack)
    // Verifies that notification state is cleared when urgency improves.
    // Full escalation-guard testing requires a mock NotificationManager (beyond MVP scope).

    func testBackOnTrack_nilPreviousUrgency_isNoOp() {
        // Guard in checkBackOnTrack should bail without crashing
        service.now = { self.makeDate(hour: 14) }
        service.updateState(totalMl: 0, goalMl: 2500, lastDrink: nil)
        XCTAssertEqual(service.currentUrgency, .overdue)
        service.checkBackOnTrack(previousUrgency: nil)
        // Urgency unchanged (no-op)
        XCTAssertEqual(service.currentUrgency, .overdue)
    }

    func testBackOnTrack_overdueToOnTrack_resetsNotifiedUrgency() {
        // Transition from overdue → onTrack should reset lastNotifiedUrgency to nil
        service.now = { self.makeDate(hour: 14) }
        service.updateState(totalMl: 0, goalMl: 2500, lastDrink: nil)
        XCTAssertEqual(service.currentUrgency, .overdue)

        let previousUrgency = service.currentUrgency
        service.updateState(totalMl: 2500, goalMl: 2500, lastDrink: Date())
        XCTAssertEqual(service.currentUrgency, .onTrack)

        service.checkBackOnTrack(previousUrgency: previousUrgency)
        XCTAssertNil(service.lastNotifiedUrgency)
    }
}
