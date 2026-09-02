# E-Paper Display Fading — Root Cause and Fix

**GitHub Issue:** [#124](https://github.com/bowerhaus/Aquavate/issues/124)
**Branch:** `epaper-fading-fix`
**Status:** CLOSED — driver fixes shipped, display recovered, panel not replaced.
> Jump to [Outcome](#outcome-closing-summary) for the closing summary and the
> "if it degrades again" playbook.

> When opening the PR for this work, include `Closes #124` in the description so
> the issue closes on merge.

## Symptom

The 2.13" e-paper display on the bottle is increasingly faded. Black text and
graphics render grey rather than solid black. This has developed gradually over
months of use rather than appearing suddenly, which points at cumulative panel
degradation rather than a rendering bug.

## Context

- Panel: Adafruit 2.13" Mono eInk FeatherWing (ADA4195), GDEY0213B74 / SSD1680
- Driver: `adafruit/Adafruit EPD@^4.5.0` (installed: 4.6.9)
- Wrapper class: `ThinkInk_213_Mono_GDEY0213B74`
- Wiring: `EPD_BUSY -1`, `EPD_RESET -1` (neither connected on the FeatherWing)

The drawing code in [display.cpp](../firmware/src/display.cpp) is not implicated.
Every defect below is in how the panel is *driven* around each refresh — the
Adafruit driver's register sequence plus how this project calls into it.

---

## Root Cause Analysis

### Finding 1 — Panel is left electrically biased between refreshes

**This is the leading candidate for the permanent fading.**

Two independent contributors combine:

**(a) `display()` never powers the panel down.**

```cpp
// .pio/libdeps/adafruit_feather/Adafruit EPD/src/Adafruit_EPD.h:88
void display(bool sleep = false);
```

The default is `false`, so `powerDown()` is skipped. Every call site in this
project uses the bare form:

| File | Lines |
|------|-------|
| `firmware/src/display.cpp` | 727, 771, 798, 823, 908 |
| `firmware/src/ui_calibration.cpp` | 62, 77, 93, 109, 127, 156, 174, 200, 217, 232, 332 |
| `firmware/src/main.cpp` | 610, 1041 |

**(b) The update command doesn't power down either.**

```cpp
// drivers/Adafruit_SSD1680.h:60
uint8_t _display_update_val = 0xF4;
```

Sent as Display Update Control 2 (`0x22`). `0xF4` = enable clock, enable analog,
load temperature, load LUT (mode 1), display. It **omits bit 1 (disable analog)
and bit 0 (disable clock)**. The canonical full-update value for this controller
is `0xF7`, which includes both.

**Consequence:** after every refresh the charge pump keeps running and
VGH/VSH/VSL (≈ +20 V / +15 V / −15 V) remain applied to the panel until the next
`powerUp()`. Since [main.cpp:733](../firmware/src/main.cpp#L733) only re-initialises
the panel on wake, that is hours overnight and **days at a time in
backpack/extended sleep mode**.

Sustained DC bias across an electrophoretic layer is a well-documented cause of
irreversible contrast loss. Vendor guidance is uniformly to power down or
deep-sleep the panel immediately after refresh. This has been happening on every
refresh for the life of the project.

### Finding 2 — VSH2 drive voltage set to an invalid value

```cpp
// drivers/Adafruit_SSD1680.cpp — ssd1680_default_init_code
SSD1680_SOURCE_VOLTAGE, 3, 0x41, 0x00, 0x32,   // VSH1, VSH2, VSL
```

The Good Display reference init for this panel uses `0x41, 0xA8, 0x32` (VSH2 ≈
5 V). Adafruit sends `0x00`, which is outside the controller's defined range. If
the factory OTP full-update waveform uses VSH2 for any drive phase — many
SSD1680 waveforms do — those phases drive at an undefined/near-zero level and
dark pixels settle grey instead of black.

This would have been present from day one and is *not* age-related, so it
explains part of the absolute contrast level but not the trend.

### Finding 3 — Temperature sensor never selected

`SSD1680_TEMP_CONTROL` (`0x18`) is defined in `Adafruit_SSD1680.h:18` but never
sent. The power-on default selects an **external** sensor, and there isn't one on
the FeatherWing. The controller picks its OTP waveform based on that reading, so
the waveform is temperature-matched to a meaningless value. The reference init
sends `0x18, 0x80` (internal sensor).

Also makes behaviour drift with ambient temperature in unpredictable ways.

### Finding 4 — Refresh timing is blind and marginal

With `EPD_BUSY -1` and `EPD_RESET -1`
([config.h:202-204](../firmware/src/config.h#L202-L204)):

- `busy_wait()` degrades to `delay(500)`; `update()` adds `delay(1000)` →
  **1.5 s total**. A GDEY0213B74 full update is ~2 s typical at 25 °C, longer cold.
- `powerDown()` cannot deep-sleep (no RST pin), so it falls back to `SW_RESET`.

Neither pin is a wiring oversight. Adafruit's guide for this board states that on
the FeatherWing "neither of these lines are connected" and that both `EPD_RESET`
and `EPD_BUSY` **must** be `-1` or the panel will not update
([Arduino Usage](https://learn.adafruit.com/adafruit-2-13-eink-display-breakouts-and-featherwings/arduino-usage),
[Pinouts](https://learn.adafruit.com/adafruit-2-13-eink-display-breakouts-and-featherwings/pinouts)).
BUSY is routed only on the *breakout* variant, not the FeatherWing.

Do not mistake the `RST` pin visible on the Feather V2 / the FeatherWing header
for the panel's reset — that is the microcontroller reset net (hence Adafruit's
`// can set to -1 and share with microcontroller Reset!` comment). Driving it
would reset the ESP32.

Consequences, both permanent:

1. All refresh timing is open-loop and must be sized for worst-case (cold)
   conditions — see Phase 4.
2. True panel deep-sleep (`0x10`) is unavailable, because exiting it requires a
   hardware reset; this is why the library guards it behind `if (_reset_pin >= 0)`.
   `0xF7` still disables the analog and clock as the final step of the waveform,
   which is the part that matters for Finding 1. Deep sleep would only add a small
   standby-current saving on top.

**Important interaction:** today this is largely benign *because* Finding 1 means
nothing is sent after `MASTER_ACTIVATE`, so the waveform runs to completion
unmolested. Naively fixing Finding 1 by switching call sites to `display(true)`
would send `SW_RESET` at t=1.5 s and **abort the waveform mid-cycle** — worse
fading plus DC imbalance. Findings 1 and 4 must be fixed together, and the
preferred fix for Finding 1 is `0xF7` (the controller sequences its own
power-down as the final step) rather than `display(true)`.

### Finding 5 — Far more refreshes than necessary

[main.cpp:1284](../firmware/src/main.cpp#L1284) takes a **single unaveraged**
`nau.read()`, which feeds a 5 ml display threshold checked every 5 s:

```c
#define DISPLAY_UPDATE_THRESHOLD_ML     5.0f
#define DISPLAY_UPDATE_INTERVAL_MS      5000
```

Stated sensor accuracy is ±10–15 ml, and `displayNeedsUpdate()` compares against
the last *rendered* value with no hysteresis. Noise alone can therefore retrigger
a full refresh every 5 s on a completely stationary bottle. This is a large
multiplier on the damage from Findings 1–3.

Secondary: a tap wake can perform up to three full refreshes
(`displayTapWakeFeedback` → `drawWelcomeScreen` → `displayForceUpdate`).

### Finding 6 (speculative) — VCOM override

The init code sends `SSD1680_WRITE_VCOM, 1, 0x36`, overriding the panel's
factory-calibrated OTP VCOM with a generic value. A mismatched VCOM both washes
out contrast and leaves a standing DC offset on the common plane. Plausible but
unconfirmed — test separately from Findings 2/3 so the effect is attributable.

---

## Verification Constraint

**No bench current measurement is available.** The cleanest confirmation of
Finding 1 would be sleep current (~200 µA if the analog is off vs ~1–3 mA if it
is not), but that is out of reach.

This does **not** gate the work. Findings 1–4 are defects against the controller
datasheet and the panel vendor's reference init regardless of whether they are
the cause of the fading — the fixes are correct on their own merits. Verification
therefore uses two substitutes that need no extra hardware:

1. **Battery-drop test** (indirect proxy for Finding 1). The boot log already
   prints `Battery: %.2fV (%d%%)`. Charge fully, place in backpack mode
   undisturbed for 48 h, wake and read the voltage. A ~1–3 mA continuous draw vs
   ~200 µA produces a dramatically different drop; the difference before/after
   the fix should be unmistakable even with a crude measurement.
2. **Photographic contrast comparison** (the metric that actually matters). Fixed
   lighting, fixed camera distance, sheet of white paper in frame as a reference,
   same screen content each time.

---

## Plan

### Phase 0 — Baseline (do before changing anything)

- [ ] Photograph the current display: main screen, fixed lighting, white paper
      reference in frame. Save to `docs/images/epd-baseline-*.jpg`.
- [ ] Record boot-log battery voltage, then leave the bottle in backpack mode
      undisturbed for 48 h and record it again. This is the "before" number for
      the battery-drop test.
- [x] Add `rtc_refresh_count` (RTC_DATA_ATTR, alongside `rtc_wake_count` in
      [display.cpp:46](../firmware/src/display.cpp#L46)), incremented on every
      panel refresh and printed in the boot log. Quantifies Finding 5 — we
      currently have no idea whether the panel sees 10 or 500 refreshes a day.
      **Done.** All 16 `display()` call sites now route through a new
      `displayRefreshPanel(epd)` helper in `display.cpp`, so the tally is
      complete rather than partial. Counter resets on power cycle (same as
      `rtc_wake_count`). Boot log prints
      `E-Paper: OK (N refreshes since power-on)`.
- [ ] Let it run one normal day and record refreshes/day.

### Phase 1 — Contrast recovery test (is any of it reversible?)

Determines whether we are looking at recoverable residual charge/ghosting or
permanent ink damage. Cheap, and it sets expectations for Phase 2.

- [x] Add a serial command `EPD TEST` (follow the `matchWordsPrefix` pattern in
      [serial_commands.cpp:1180](../firmware/src/serial_commands.cpp#L1180)) that
      drives 10 alternating full-black / full-white refreshes then redraws the
      main screen. **Done.** `EPD TEST [cycles]`, default 10, range 1–50.
      Handler in `serial_commands.cpp` delegates to `epdContrastTest()` in
      `main.cpp` (where the `display` object lives). Falls back to the welcome
      screen if the bottle isn't calibrated, so it never leaves the panel blank.
- [x] Run it, photograph after, compare against the Phase 0 baseline.
- **If black recovers noticeably:** a meaningful share is reversible → schedule a
  periodic conditioning cycle in Phase 3.
- **If it does not recover:** the ink is genuinely degraded. Phase 2 will stop
  further decline but will not undo it; plan on the spare panel from
  [Plan 002](002-bom-adafruit-feather.md).

#### Result (first run, 10 cycles) — INCONCLUSIVE, test was invalid

Observations:

- During the flush, full-screen black looked completely solid and full-screen
  white completely clean.
- The main screen redrawn straight after looked badly degraded — grey, speckled,
  half-developed text and bitmaps on a clean white background.
- A single isolated refresh **30 s later** (via `TARE`, nothing else running)
  came back **almost identical** — still degraded.
- User reports the normal display is "sometimes much better than this".

**Two hypotheses were raised and both are now dead:**

1. *Ink is healthy because black went fully black.* **Invalid conclusion.** A
   full-screen black frame has no white in view, so the eye reads it as solid
   black even when it is well short. The main screen puts grey text directly
   against white, where the same deficit is obvious. The two viewing conditions
   are not comparable, so the flush frames tell us nothing about achievable
   contrast. Findings 2 (VSH2) and 6 (VCOM) are therefore **not** cleared.
2. *Degradation is back-to-back refresh collision (Finding 4).* **Refuted** by
   the isolated 30 s-later refresh, which should have been clean and was not.
   Timing is not the cause of a single bad frame.

**Test fixed anyway:** `epdContrastTest()` now waits `EPD_TEST_SETTLE_MS`
(3000 ms) after each refresh, so bursts cannot confound future runs.

#### Next diagnostic — `EPD PATTERN`

Re-running `EPD TEST` cannot resolve this; it reproduces the same unreferenced
viewing condition. Added `EPD PATTERN` instead: **one isolated refresh** drawing
solid black beside solid white in the *same* frame, plus 1px line pairs and text
in both polarities.

| Observation | Conclusion |
|---|---|
| Black half looks as dark as the flush did, text still grey | Fault is in fine-detail rendering, not panel contrast |
| Black half is *also* grey against the adjacent white | Panel contrast genuinely degraded — Findings 1/2/6 back in play |
| Fine lines speckled or dropped rather than uniformly grey | Framebuffer/SRAM integrity fault — a cause not in the original six findings |

That last row is a live possibility worth stating: the FeatherWing buffers the
frame in **external SRAM** (`SRAM_CS`). Marginal SRAM or SPI would corrupt fine
detail while leaving uniform fills visually intact — bit errors are invisible in
a solid field and glaring in text. This would also explain the
sometimes-better/sometimes-worse variability better than any fixed
misconfiguration does.

#### `EPD PATTERN` result — confirms Finding 1

Observed on the panel (user reading; an earlier reading of the photo that
described the two halves as behaving differently was wrong — glare and an
oblique angle):

- **Both halves are equally degraded**, and both show salt-and-pepper speckle.
- Within each section the left side is slightly better than the right — a
  consistent horizontal gradient.
- **The image degrades while the bottle stands idle with no activity at all.**
- Battery at wake: **4.2 V** — a full cell.

**This is Finding 1.** Individual pixels drift out of state while the panel sits
under continuous DC bias; the drift accumulates as speckle, and at normal
viewing distance an accumulating speckle field reads as grey. That grey is the
"fading" this whole plan started from.

Every observation now fits one mechanism:

| Observation | Explained by |
|---|---|
| Degrades while idle, no activity | Panel left biased between refreshes — the direct signature |
| Both halves equally affected | Global electrical effect, not content-dependent |
| Salt-and-pepper rather than uniform grey | Per-pixel drift, not a contrast ceiling |
| "Sometimes much better" | Quality tracks time since last refresh, not the refresh itself |
| Left-to-right gradient within each section | Drive droop along the source-line direction — consistent with invalid VSH2 (Finding 2) |

**Ruled out:**

- **Sagging supply / charge-pump starvation** — 4.2 V is a full cell.
- **Refresh timing (Finding 4)** — refuted earlier by the isolated-refresh test.
- **SRAM framebuffer corruption** — would not explain degradation *while idle
  with no writes occurring*.
- **Permanent ink damage** — a refresh restores the image, so what decays is the
  displayed frame, not the ink. Much of the fading should be recoverable.

**Conclusion: the plan's original ranking was right.** Findings 1 and 2 are the
answer; the detours through timing and SRAM were explaining the wrong thing.
Proceed to Phase 2.

#### Post-Phase-2 result — fixes flashed, symptom unchanged; hypothesis refined

With `AquavateEPD` live (`0xF7`, VSH2 `0xA8`, internal temp sensor, 2500 ms
wait):

- `EPD TEST` solid frames are **perfect** — pure black, pure white, zero
  speckle. Every pixel reaches both rails. Ink degradation is ruled out by
  direct observation.
- `EPD PATTERN` is **identical to before the fixes** — salt-and-pepper over both
  halves. The init corrections changed nothing about the mixed-content failure.
- User confirms the firmware was **not reflashed during the months** the fading
  developed → software constant, symptom progressive → the change is physical.
  (A library-version-drift theory was briefly considered and is dead.)

**Refined hypothesis — dynamic under-drive from aged boost passives.** A pixel
in the pattern's solid-black half fails where the same pixel in a full-black
frame succeeds; the only difference is that mixed content makes the source
drivers switch during the scan. Uniform frame = static load = fine; mixed frame
= dynamic load = rail sag = incompletely driven pixels = speckle. And Finding 1
supplies the aging mechanism: the charge pump and its capacitors have run
**continuously for the life of the bottle** instead of ~2 s per refresh.
The DC bias wore out the boost circuitry, not the ink. `0xF7` stops further
wear; it cannot repair it.

**Discriminating test (no reflash):** run `EPD PATTERN` 2–3× back-to-back.

- Cleaner each pass → under-drive confirmed. Compensations to try: booster
  soft-start (`SSD1680_BOOST_SOFTSTART` `0x0C` — present in the Good Display
  reference init, omitted by Adafruit), and/or double-refresh for mixed content.
- Identical each pass → deterministic waveform fault; look at OTP LUT next.

Note for the hardware fallback: if this is aged passives, they sit on the
FeatherWing or panel FPC — establish which before assuming the Plan 002 spare
*panel* fixes it.

#### Temperature — the likely progressive variable

Further user facts reframe the aged-passives idea:

- Ambient is **~25 °C** (initially reported as 30 °C, corrected next day) and has
  warmed over the degradation window.
- Git dates: the refresh-reduction change ("Display is now smarter for updates",
  Plan 009) landed **2026-01-14**; last display-related flash **2026-02-08**;
  degradation developed in the months after — i.e. **winter → summer with the
  software frozen**.

Combined mechanism that fits every observation without any component aging:

1. DC bias between refreshes (Finding 1) has existed since day one.
2. Plan 009 reduced refresh frequency → each image sits longer under that bias
   (visibility multiplier, not a cause).
3. E-ink particle mobility rises steeply with temperature → bias-driven decay
   accelerates as the room warmed, plus self-heating from the always-on charge
   pump. Spring/summer pushed decay past the visibility threshold.
4. "Sometimes much better" = cool mornings vs hot afternoons.

The environment changed, not the hardware. (Aged boost passives remain possible
but are no longer required by the evidence.)

**Fridge test (next, free, decisive):** at room temp run `EPD PATTERN` twice
back-to-back (under-drive check: does pass 2 improve?). Then fridge 20–30 min
(not freezer), run `EPD PATTERN` within a minute of removal, photograph, compare.

- Cold render much cleaner → temperature confirmed operative. Post-fix the
  internal temp sensor should already be compensating — if it isn't enough, next
  lever is booster soft-start (`SSD1680_BOOST_SOFTSTART` `0x0C`, in the vendor
  reference init, omitted by Adafruit).
- Cold render identical → temperature ruled out; back to waveform/drive.

(Wipe condensation off and let it dry before charging.)

#### Fridge test result — CONFIRMED: cold renders much better

"After putting the bottle in the fridge for some time, the inks are coming out
much better."

This is the **anomalous direction** and that makes it diagnostic. Healthy e-ink
performs *worse* cold (particle mobility drops). A panel that improves cold is
benefiting not from the cold but from the **waveform the controller selects for
cold** — longer, harder drive phases. Read together with the room-temp bin being
the *nominal* one (~25 °C):

- **The panel genuinely needs more drive than its factory OTP waveform provides
  at nominal temperature.** Mild, real degradation of the ink medium — the
  expected legacy of months under continuous DC bias. `0xF7` stops further
  aging; it cannot restore the lost margin.
- The internal-temp-sensor fix (Finding 3) was correct but insufficient — the
  controller *is* now reading ~25 °C accurately, and the 25 °C waveform is what
  fails.
- Phase 4's blind-delay sweep cannot help rendering: the waveform length is
  controller-internal. The delay only needs to outlast it.

**Practical fix: drive the panel as if it were colder.** The SSD1680 accepts a
forced temperature via `TEMP_WRITE` (`0x1A`); with the "load temperature" bit
(bit 5) dropped from Display Update Control 2 (`0xF7` → `0xD7`), the forced
value survives to LUT selection and the controller uses that bin's waveform.
(The inverse trick — forcing a *hot* bin for a shorter waveform — is the basis
of vendor "fast refresh" modes.)

Implemented in `AquavateEPD::setForcedTemperature()` /
`clearForcedTemperature()` via a `powerUp()` override, plus serial command
`EPD TEMP <degC>` / `EPD TEMP OFF` / `EPD TEMP` (report). Not persisted.

**Bin sweep protocol (room temperature, no fridge):** flash, then
`EPD TEMP 15` → `EPD PATTERN`, then `10`, `5`, `0`, `-10`, photographing each.
Pick the **warmest bin that renders cleanly** — colder bins mean slower
refreshes and more ink stress, so overshoot buys nothing. Note the refresh
visibly lengthens as the bin drops; verify the blind wait (`EPD WAIT`) still
outlasts the waveform at the chosen bin — budget up from 2500 ms if needed.

Once the bin is chosen, decide the production policy: simplest is a fixed
forced bin (e.g. real-temp-minus-15) set in `begin()`; smarter is offsetting the
internal sensor reading, but the SSD1680 has no native offset — it would need a
read-modify-write of the temp register, which the no-MISO wiring makes
impractical. Fixed bin is fine for a bottle that lives indoors.

#### Bin sweep result — null, but with the decisive observation

The sweep showed **no difference between bins at all**. However:

> "When the pattern is refreshing, the screen briefly flashes with a perfect
> image each time. And no salt and pepper. And that's for all bins. But then
> when it comes to actually draw [the pattern], it goes back to the salt and
> pepper."

**A perfect image forms mid-waveform and collapses to speckle as the waveform
completes.** This reframes everything:

- The panel **can** form the image perfectly — every pixel reaches its correct
  state under active drive. Drive strength, waveform bin, drive voltages: all
  sufficient. (Explains the null bin result — the bins alter drive phases, and
  drive was never the failing part. It also means the forced-temp write may or
  may not be taking effect; with drive ruled out as the lever, that ambiguity
  no longer matters.)
- The failure is **at drive release**: marginal pixels relax out of position
  when the waveform ends.

**What pulls driven pixels out of position after drive ends? A standing DC
offset on the common plane — Finding 6, the VCOM override.** The generic
`0x2C = 0x36` replaces the panel's factory-calibrated OTP VCOM. The resulting
offset field sits across the ink permanently; pixels near threshold relax under
it the instant the hold on them releases. This also explains:

- **The fridge result:** cold ink is viscous — driven pixels can't relax even
  under the offset. Warm ink is mobile — relaxation is immediate.
- **Pre-fix idle decay:** with the analog left on (old `0xF4`), the offset acted
  continuously on the standing image for hours.
- **Progressive seasonal onset:** relaxation rate scales with temperature.

Finding 6 is promoted from "speculative, test last" to **prime suspect**.

#### Phase 5 pulled forward — `EPD VCOM` command

`AquavateEPD` now carries two init tables — the existing one (VCOM override,
value pokeable in place) and one with **no VCOM write** so the factory OTP value
stands. Serial control, not persisted:

- `EPD VCOM OFF` (alias `OTP`) — drop the override, use factory calibration
- `EPD VCOM <val>` — set an override value (decimal or 0x hex)
- `EPD VCOM` — report

**Test:** reflash → `EPD VCOM OFF` → `EPD PATTERN`. If the pattern holds after
the waveform completes — no collapse to speckle — Finding 6 is confirmed; make
OTP VCOM the default in `begin()` and the display fix is essentially done.

**If OTP VCOM does not help:** the remaining explanation is degraded ink
bistability — pixels drive correctly but no longer hold state unaided. Options
then: a custom LUT with a stronger final/hold phase (significant effort), or
the spare-panel swap. The `EPD VCOM <val>` form also allows a manual VCOM sweep
(e.g. 0x20–0x50) to hunt for a value that holds better than both 0x36 and OTP,
which is cheap to try before concluding bistability loss.

#### VCOM result — OTP did not help. LUT experiment — decisive

`EPD VCOM OFF` (factory OTP VCOM) did **not** change the failure. Finding 6 is
out as the cause.

Next discovery: Adafruit's driver ships a complete hand-rolled 153-byte waveform
LUT (`ssd1680_fpc7519_init_code`) for the FPC-7519 rev.b sibling variant of this
same FeatherWing — precedent that these panels' OTP waveforms can be inadequate
— plus a library hook (`_epd_lut_code`, applied in `powerUp()`) to load a RAM
LUT without modifying the library. Wired up as `EPD LUT ON` (update value
switches to `0xC7` so the OTP LUT-load doesn't overwrite the RAM LUT).

**Result: renders very differently — noisy on both halves but just about
readable.** The waveform lever demonstrably works on this hardware, the OTP
waveform is implicated in the failure, and the panel responds to tuning — i.e.
it is not dead, and panel replacement is now optional rather than inevitable.
(Noise is expected: that LUT is tuned for the sibling variant, and pairs with
VCOM `0x24` / VSH2 `0xAE` in Adafruit's companion init.)

#### Current experiment matrix (all serial, not persisted)

- `EPD LUT ON` + `EPD VCOM 0x24` → `EPD PATTERN`, then micro-sweep VCOM
  (`0x1C`/`0x20`/`0x28`/`0x2C`) with the LUT on.
- `EPD LUT STRONG` (new build): same VS polarity sequences (approximate DC
  balance preserved), drive durations ~2×, final hold phase lengthened and
  repeated — targets a panel that drives correctly but doesn't hold. Waveform
  runs ~2× longer: **set `EPD WAIT 5000` first.**
- Endgame: whichever LUT+VCOM combination renders cleanly gets baked into
  `aquavate_epd.h` as the default. If no combination is clean enough, replace
  the FeatherWing knowing the driver fixes protect the new panel.

#### Final results — waveform levers exhausted

- VCOM sweep (0x1C–0x2C) with fpc7519 LUT: **no difference** — VCOM never had
  any visible effect on this panel, in any waveform mode.
- `EPD LUT STRONG`: **noise worse**, black fill about the same. Longer drive
  makes it worse, not better — the "aged ink just needs more charge" theory is
  broken. Overdrive is not the answer either.

At this point panel replacement was recommended. **It was not carried out — see
"Outcome" at the end of this document, which supersedes that recommendation.**

### Phase 2 — Driver correctness fixes

All contained in one new header so the change is reversible in a single revert.

- [ ] Create `firmware/include/aquavate_epd.h`:

```cpp
#ifndef AQUAVATE_EPD_H
#define AQUAVATE_EPD_H

#include <Adafruit_ThinkInk.h>

// Corrected SSD1680 init for GDEY0213B74. Differences from the Adafruit default
// (drivers/Adafruit_SSD1680.cpp) are marked; see Plans/079.
static const uint8_t aquavate_ssd1680_init[] = {
    SSD1680_SW_RESET, 0,
    0xFF, 20,
    SSD1680_DATA_MODE, 1, 0x03,
    SSD1680_WRITE_BORDER, 1, 0x05,
    SSD1680_TEMP_CONTROL, 1, 0x80,               // FIX 3: internal temp sensor
    SSD1680_WRITE_VCOM, 1, 0x36,                 // Finding 6: candidate to drop
    SSD1680_GATE_VOLTAGE, 1, 0x17,
    SSD1680_SOURCE_VOLTAGE, 3, 0x41, 0xA8, 0x32, // FIX 2: VSH2 0x00 -> 0xA8
    SSD1680_SET_RAMXCOUNT, 1, 1,
    SSD1680_SET_RAMYCOUNT, 2, 0, 0,
    0xFE};

class AquavateEPD : public ThinkInk_213_Mono_GDEY0213B74 {
 public:
  using ThinkInk_213_Mono_GDEY0213B74::ThinkInk_213_Mono_GDEY0213B74;

  void begin(thinkinkmode_t mode = THINKINK_MONO) {
    ThinkInk_213_Mono_GDEY0213B74::begin(mode);
    _epd_init_code = aquavate_ssd1680_init;
    _display_update_val = 0xF7;   // FIX 1: controller disables analog + clock
  }

 protected:
  // FIX 4: no BUSY pin, so wait blind. Base class waits 1.5s; full update is
  // ~2s typical and longer when cold.
  void busy_wait() override { delay(2500); }
};

#endif
```

> **Implemented — with one correction to the header above.** The published
> snippet overrides `busy_wait()`, which is wrong: `busy_wait()` is called
> **four times per refresh** — twice in `powerUp()` (once directly, once via the
> init code's `0xFF` marker), once in `update()`, once in `powerDown()`. A 2500 ms
> override would have made every refresh ~8.5 s, not 2.5 s.
>
> The shipped [aquavate_epd.h](../firmware/include/aquavate_epd.h) instead sets
> `default_refresh_delay`, which `update()` uses *only* when no BUSY pin is
> present. That tunes the post-`MASTER_ACTIVATE` wait alone and leaves
> `powerUp()`/`powerDown()` latency untouched. Exposed as
> `setRefreshWaitMs()` / `getRefreshWaitMs()` and driven from serial via
> `EPD WAIT [ms]`, so the Phase 4 sweep needs no reflashing.

- [x] Change the object declaration in
      [main.cpp:132](../firmware/src/main.cpp#L132) to `AquavateEPD display(...)`.
      No other signature changes are needed: `displayInit()` and the
      `ui_calibration` helpers take a `ThinkInk_213_Mono_GDEY0213B74&`, a subclass
      instance binds to that fine, `busy_wait()` is virtual so the override
      applies through base-class calls, and `_epd_init_code` /
      `_display_update_val` are member data read at `powerUp()`/`update()` time.
- [x] Build and check IRAM. Expected impact is negligible (a few hundred bytes of
      flash, no IRAM), but IOS_MODE only has ~9.7 KB headroom so confirm.
      **Confirmed:** Flash 61.0% (+528 bytes total including the new serial
      commands), RAM unchanged at 11.7%, no IRAM pressure.
- [ ] Flash, photograph, compare against Phase 0 and Phase 1.
- [ ] Repeat the 48 h backpack battery-drop test. Compare against Phase 0.

Deliberately **not** doing: switching call sites to `display(true)`. `0xF7` makes
the controller power itself down as the last step of the waveform, which is safer
than an externally-timed `SW_RESET` given we are waiting blind (see Finding 4).

### Phase 3 — Reduce refresh count

Do this *after* Phase 2 so the two effects stay separable in the photos.

- [ ] Average N samples in the `SensorSnapshot` read at
      [main.cpp:1284](../firmware/src/main.cpp#L1284) rather than a single
      `nau.read()`, or reuse the existing `weightMeasureStable()` path.
- [ ] Raise `DISPLAY_UPDATE_THRESHOLD_ML` from `5.0f` to ~15–20 ml so it is not
      below the sensor's own accuracy.
- [ ] Add hysteresis to `displayNeedsUpdate()` so a value hovering on the
      threshold cannot ping-pong.
- [ ] Review the tap-wake path — three full refreshes per wake
      (`displayTapWakeFeedback` → `drawWelcomeScreen` → `displayForceUpdate`) is
      more than the UX needs.
- [ ] Re-read `rtc_refresh_count` after a normal day and compare against Phase 0.
- [ ] If Phase 1 showed recovery: add a conditioning cycle (a few black/white
      flushes) on a schedule — e.g. daily at rollover, or every N refreshes.

### Phase 4 — Tune the blind refresh delay

**BUSY is not available.** Confirmed by inspection: the FeatherWing brings out
neither BUSY nor RST (RST is tied to the Feather's own reset line). There is no
pad to solder to. Blind timing is therefore permanent, not a stopgap — so the
delay needs to be tuned empirically rather than guessed, and the 2500 ms in
Phase 2 is a starting point, not an answer.

- [ ] Extend the `EPD TEST` command from Phase 1 to accept a delay argument
      (`EPD TEST 1500`) that overrides `busy_wait()` for that run via a
      settable member on `AquavateEPD`.
- [ ] Sweep 1500 / 2000 / 2500 / 3000 / 4000 ms, photographing a fixed
      high-contrast test pattern at each. Find the point past which contrast
      stops improving.
- [ ] Set the override to that value **plus margin for cold operation** — the
      waveform lengthens at low temperature and we have no sensor feedback to
      adapt with, so the fixed delay must be sized for the worst case, not room
      temperature. If contrast plateaus at 2500 ms indoors, budget meaningfully
      above it.
- [ ] Measure the wake-latency cost of the chosen value. If it is perceptible,
      that trade is worth raising before locking it in — a slower wake is a fair
      price for not damaging the panel, but it should be a deliberate choice.

Note that `0xF7` (Phase 2) means nothing is transmitted after `MASTER_ACTIVATE`,
so an under-long delay no longer truncates the waveform — it only risks the next
SPI command arriving while the controller is still busy. That makes this tuning
lower-stakes than it would have been with the `display(true)` approach, but it
still matters for the *next* refresh's integrity.

### Phase 5 — Isolate the VCOM question (Finding 6)

Only once Phases 2–3 have settled, so the effect is attributable.

- [ ] Remove the `SSD1680_WRITE_VCOM, 1, 0x36` line from `aquavate_ssd1680_init`
      so the panel uses its factory OTP VCOM.
- [ ] Photograph and compare. Keep whichever is better.

---

## Success Criteria

- Black renders visibly darker in the fixed-lighting photo comparison.
- Backpack-mode battery drop over 48 h is materially lower than the Phase 0
  baseline (confirms Finding 1 indirectly).
- Refreshes/day drops substantially after Phase 3.
- No regression in display correctness, calibration UI, or wake latency.

## Risks and Rollback

| Risk | Mitigation |
|------|------------|
| `0xF7` behaves unexpectedly on this panel | Whole change is one header + one declaration; revert is trivial |
| Longer blind wait slows wake perceptibly | Measure in Phase 4 and choose deliberately — no BUSY pin exists, so this cost cannot be engineered away |
| Fixed delay too short in cold conditions | Size for worst case in Phase 4, not room temperature; no sensor feedback is available to adapt with |
| Corrected init code makes contrast *worse* | Phased, photographed at each step, so attributable |
| Ink damage is permanent | Phase 1 tells us early; fallback is the spare panel from Plan 002 |
| IRAM overflow in IOS_MODE | Check build output at Phase 2 before flashing |

## Notes

- Findings 1–3 affect **every** Adafruit EPD user of this panel, not just this
  project. Worth an upstream issue against `adafruit/Adafruit_EPD` once confirmed.
- Nothing here is testable by the `[env:native]` unit tests — the display path is
  hardware-only. Verification is manual and photographic throughout.

---

# Outcome (closing summary)

**Status: closed. Panel NOT replaced — the display recovered.** This section
supersedes the replace-the-panel recommendation earlier in the document.

## What shipped

All default-on in [aquavate_epd.h](../firmware/include/aquavate_epd.h) plus one
declaration change in [main.cpp](../firmware/src/main.cpp):

| Fix | Change | Finding |
|-----|--------|---------|
| 1 | `_display_update_val` `0xF4` → `0xF7` — controller powers itself down as the final waveform step, so the panel is no longer DC-biased between refreshes | 1 |
| 2 | VSH2 `0x00` → `0xA8` — Adafruit sends an out-of-range source voltage | 2 |
| 3 | `0x18 0x80` — select the internal temperature sensor (POR default is an external one that isn't fitted) | 3 |
| 4 | Blind refresh wait 1500 → 2500 ms, runtime-tunable | 4 |

Plus instrumentation: `rtc_refresh_count` and `displayRefreshPanel()` — **every
panel refresh must go through that helper or the tally silently undercounts.**

## Why it is not certain which fix worked

The display recovered, but attribution was never established. Three candidates,
all plausible, none isolated:

1. **`0xF7`** — stops the image decaying between refreshes. The mechanism best
   supported by the evidence (degradation was observed while completely idle).
2. **Repeated black/white flush cycles** from the many `EPD TEST` runs. This was
   literally the Phase 1 hypothesis: conditioning recovers reversible ghosting.
   Dozens of full-contrast cycles were run during diagnosis.
3. **Ambient temperature.** The fridge test showed a large, real temperature
   sensitivity, and the room was ~25 °C during the worst readings. If the room
   has since cooled, some recovery may simply be that — and would reverse.

Phase 5's controlled A/B never ran, so treat all three as live.

## If the display degrades again

Do these in order. They are chosen so each answers a different question.

1. **`EPD PATTERN`** — is it the same failure mode? (salt-and-pepper speckle
   over both halves, solid fills clean). If it looks different, this playbook
   may not apply.
2. **Note the ambient temperature and compare against a cold run.** Chill the
   bottle 20–30 min and re-run `EPD PATTERN`. If cold is markedly better, the
   dominant variable is temperature, not wear.
3. **`EPD TEST`** (10+ black/white cycles, ~60 s). If this recovers it, the
   damage is reversible ghosting → **implement a scheduled conditioning cycle**
   (Phase 3's optional item: a flush at daily rollover or every N refreshes).
   This is the single most likely productive fix and was never implemented.
4. **Check `rtc_refresh_count`** in the boot log. High refreshes/day means
   Phase 3 (reduce refresh count) is overdue and the panel is being worn
   unnecessarily.
5. **Confirm `0xF7` is still in force** — that `AquavateEPD` is the declared
   type in main.cpp and no call site bypasses `displayRefreshPanel()`.

### Do NOT re-run these — swept, no effect

Recorded so the same ground isn't covered twice:

| Lever | Result |
|-------|--------|
| VCOM — `0x36`, factory OTP, and `0x1C`–`0x2C` sweep, in both waveform modes | **No visible effect, ever** |
| Forced temperature bins (`EPD TEMP`, +15 °C down to −10 °C) | No difference between bins |
| Refresh timing / blind wait sweep | Not implicated; an isolated refresh 30 s after any burst was still degraded |
| Battery / supply sag | 4.20 V (full cell) during worst symptoms |
| SRAM framebuffer corruption | Ruled out — degradation continued while idle with no writes |
| `EPD LUT STRONG` (drive durations ~2×) | **Worse** — more noise. Overdrive is not the answer |

The one lever that *did* change behaviour was the waveform LUT (`EPD LUT ON`,
Adafruit's fpc7519 LUT): visibly different rendering, proving the OTP waveform
is reachable and the panel responds to waveform choice. That is the direction
with remaining headroom if a future attempt is needed — but it needs a LUT
tuned for *this* panel, which the two tried were not.

## Diagnostic commands (retained)

Kept deliberately — ~3.5 KB flash, no IRAM, and they turned open-ended
guesswork into same-day experiments. None are persisted; all reset on reboot.

| Command | Purpose |
|---------|---------|
| `EPD PATTERN` | Black beside white in one frame + 1px lines both polarities. The main diagnostic; should render clean on a healthy panel |
| `EPD TEST [cycles]` | Black/white flush cycles (settles 3 s per refresh so bursts don't confound the result) |
| `EPD WAIT [ms]` | Blind refresh delay, 500–6000 |
| `EPD TEMP [degC\|OFF]` | Force the waveform temperature bin |
| `EPD VCOM [val\|OFF]` | VCOM override value, or factory OTP |
| `EPD LUT [ON\|STRONG\|OFF]` | RAM waveform: fpc7519 / strengthened / OTP |

## Left undone (deliberately)

- **Phase 3 — reduce refresh count.** The most valuable remaining work. A 5 ml
  threshold against ±10–15 ml sensor accuracy, no hysteresis, checked every 5 s,
  means sensor noise alone triggers full refreshes on a stationary bottle. Fewer
  refreshes = less panel wear. Also add hysteresis and average the samples.
- **Scheduled conditioning cycle.** Cheap insurance, and directly indicated if
  the flush cycles are what recovered the panel.
- **Overnight idle-stability test.** The clean pass/fail on whether `0xF7` fixed
  the decay mechanism. Refresh, photograph, leave untouched overnight,
  photograph again without waking.
- **Phase 4 delay sweep / Phase 5 VCOM A/B.** Phase 5 is largely moot given VCOM
  showed no effect at any value.
- **Upstream issue** against `adafruit/Adafruit_EPD` for Findings 1–3, which
  affect every user of this panel.
