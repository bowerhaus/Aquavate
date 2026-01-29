# Aquavate - Active Development Progress

**Last Updated:** 2026-01-29 (Session 22)
**Current Branch:** `bottle-capacity-setting` (from `settings-option5-smart-contextual`)
**GitHub Issue:** #90
**Plan:** [Plans/064-bottle-capacity-setting.md](Plans/064-bottle-capacity-setting.md)

---

## Current Task

Add ability to set bottle capacity from the iOS app (Issue #90).

### Implementation Steps

- [ ] Step 1: `firmware/include/config.h` — Add BOTTLE_CAPACITY min/max/default constants
- [ ] Step 2: `firmware/src/storage.cpp` + `firmware/include/storage.h` — Add NVS save/load for bottle capacity
- [ ] Step 3: `firmware/src/calibration.cpp` — Use dynamic bottle capacity in scale factor calculation and ADC validation threshold
- [ ] Step 4: `firmware/src/ble_service.cpp` — Wire up NVS persistence in bleLoadBottleConfig/bleSaveBottleConfig, fix hardcoded 830 in full_bottle_adc recalculation
- [ ] Step 5: `firmware/src/display.cpp` + `firmware/include/display.h` — Replace hardcoded 830 with dynamic capacity for clamping and fill percentage
- [ ] Step 6: `firmware/src/main.cpp` — Initialize bottle capacity on wake from NVS/BLE
- [ ] Step 7: `ios/.../BLEManager.swift` — Add published property, setter with debounce, handle in config read/write
- [ ] Step 8: `ios/.../SettingsView.swift` — Add bottle capacity control to Device Information page (text field + stepper, 10ml steps, 200–2000ml range)
- [ ] Step 9: Build firmware and iOS, verify compilation
- [ ] Step 10: UX — advise recalibration when capacity changes

### Calibration Impact Notes

The hardcoded 830ml affects calibration in multiple places:
- Scale factor calculation (`calibration.cpp:291`) — uses capacity to derive water weight
- ADC validation threshold (`calibration.cpp:191`) — 300,000 threshold assumes 830ml
- BLE full_bottle_adc recalculation (`ble_service.cpp:640`) — hardcodes 830.0f
- Display clamping/percentage (`display.cpp:804,809`) — hardcodes 830

iOS CalibrationManager already uses dynamic bottleCapacityMl from BLE.

---

## Recently Completed

- **Display Redraw on Wake Fix (Issue #88)** - [Plan 063](Plans/063-display-redraw-on-wake.md) ✅ COMPLETE
- **Settings Page Redesign (Issue #87)** - Plan 063 ✅ COMPLETE (PR #89)
- **Daily Goal Setting from iOS App (Issue #83)** - [Plan 062](Plans/062-daily-goal-setting.md) ✅ COMPLETE (PR #86)
- **Calibration Circular Dependency Fix (Issue #84)** - [Plan 062](Plans/062-calibration-circular-dependency.md) ✅ COMPLETE (PR #85)

---

## Context Recovery

To resume from this progress file:
```
Resume from PROGRESS.md — working on bottle capacity setting (Issue #90).

Session 22: Branch `bottle-capacity-setting` created from `settings-option5-smart-contextual`.
Plan at Plans/064-bottle-capacity-setting.md.
No implementation steps completed yet.
```

---

## Reference Documents

See [CLAUDE.md](CLAUDE.md) for full document index.
