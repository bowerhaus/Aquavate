#ifndef AQUAVATE_EPD_H
#define AQUAVATE_EPD_H

#include <Adafruit_ThinkInk.h>

/**
 * Aquavate - corrected SSD1680 driving for the GDEY0213B74 panel (Plan 079)
 *
 * Background. The stock Adafruit EPD driver leaves this panel electrically
 * biased between refreshes: display() never powers down, and the update value
 * 0xF4 omits the disable-analog/disable-clock bits, so the charge pump keeps
 * running and +/-15/20V stay on the panel until the next powerUp() - hours
 * overnight, days in backpack mode. The observed symptom was the image
 * degrading into salt-and-pepper speckle while the bottle sat completely idle,
 * which at viewing distance reads as the display "fading" to grey.
 *
 * Fixes applied here (all default-on):
 *   FIX 1  0xF4 -> 0xF7      controller powers itself down as the final
 *                            waveform step; no DC bias between refreshes
 *   FIX 2  VSH2 0x00 -> 0xA8 Adafruit sends an out-of-range source voltage
 *   FIX 3  0x18 0x80         select the internal temperature sensor; the POR
 *                            default is an external one that is not fitted
 *   FIX 4  1500 -> 2500 ms   blind refresh wait (BUSY is not routed on this
 *                            FeatherWing and cannot be)
 *
 * Outcome. After these fixes plus repeated black/white flush cycles
 * (EPD TEST), the display recovered and the investigation was closed without
 * replacing the panel. Which change did it is NOT established - see
 * Plans/079-epaper-fading-fix.md, "If the display degrades again", before
 * assuming. Levers that were swept and had no effect at all: VCOM (any value,
 * both waveform modes), forced temperature bins, refresh timing. Overdriving
 * the waveform (see aquavate_lut_strong) made rendering worse, not better.
 *
 * Everything is contained in this one header plus the object declaration in
 * main.cpp, so the whole change reverts in a single step. Runtime diagnostics
 * are the EPD * serial commands in serial_commands.cpp.
 */

// Post-MASTER_ACTIVATE wait. BUSY is not connected on the FeatherWing (and
// cannot be - it is not routed on this variant), so refresh timing is blind and
// permanent. A GDEY0213B74 full update is ~2s typical at 25C and longer cold;
// the stock driver waits 1.5s total. Sized for worst case, not room
// temperature. Phase 4 sweeps this empirically - see setRefreshWaitMs().
#define AQUAVATE_EPD_REFRESH_WAIT_MS 2500

// The stock busy_wait() falls back to this fixed delay when no BUSY pin is
// present (BUSY_WAIT in Adafruit_SSD1680.cpp). update() calls busy_wait() and
// then delays a further default_refresh_delay, so the two together make up the
// total blind wait.
#define AQUAVATE_EPD_BASE_BUSY_WAIT_MS 500

// Corrected init sequence for GDEY0213B74. Differences from Adafruit's
// ssd1680_default_init_code (drivers/Adafruit_SSD1680.cpp) are marked.
// Non-const: setVcomOverride() pokes the VCOM value in place.
static uint8_t aquavate_ssd1680_init[] = {
    SSD1680_SW_RESET, 0,                          // soft reset
    0xFF, 20,                                     // busy wait
    SSD1680_DATA_MODE, 1, 0x03,                   // RAM data entry mode
    SSD1680_WRITE_BORDER, 1, 0x05,                // border color
    SSD1680_TEMP_CONTROL, 1, 0x80,                // FIX 3: internal temp sensor
    SSD1680_WRITE_VCOM, 1, 0x36,                  // Finding 6: swept, no effect
    SSD1680_GATE_VOLTAGE, 1, 0x17,                // gate voltage
    SSD1680_SOURCE_VOLTAGE, 3, 0x41, 0xA8, 0x32,  // FIX 2: VSH2 0x00 -> 0xA8
    SSD1680_SET_RAMXCOUNT, 1, 1,
    SSD1680_SET_RAMYCOUNT, 2, 0, 0,
    0xFE};

// Identical sequence with NO VCOM write, so the panel uses its
// factory-calibrated OTP VCOM (Finding 6 test).
static const uint8_t aquavate_ssd1680_init_otp_vcom[] = {
    SSD1680_SW_RESET, 0,                          // soft reset
    0xFF, 20,                                     // busy wait
    SSD1680_DATA_MODE, 1, 0x03,                   // RAM data entry mode
    SSD1680_WRITE_BORDER, 1, 0x05,                // border color
    SSD1680_TEMP_CONTROL, 1, 0x80,                // internal temp sensor
    SSD1680_GATE_VOLTAGE, 1, 0x17,                // gate voltage
    SSD1680_SOURCE_VOLTAGE, 3, 0x41, 0xA8, 0x32,  // VSH2 corrected
    SSD1680_SET_RAMXCOUNT, 1, 1,
    SSD1680_SET_RAMYCOUNT, 2, 0, 0,
    0xFE};

// Experimental full-refresh waveform, copied from Adafruit's
// ssd1680_fpc7519_init_code (drivers/Adafruit_SSD1680.cpp) - the hand-rolled
// LUT they ship for the FPC-7519 rev.b sibling variant of this same
// FeatherWing, whose OTP waveform was evidently inadequate. Used here as a
// diagnostic: if an explicit RAM LUT changes the failure mode at all, the OTP
// waveform is implicated and a tuned LUT is the path forward.
// Layout: 5x12 VS rows (L0 black, L1/L2 grays, L3 white, L4), 12x7 timing
// groups, 9 FR/XON bytes.
//
// TESTED: rendered very differently from the OTP waveform (noisy but readable),
// which proved the waveform lever works on this hardware. Not an improvement -
// it is tuned for the sibling panel variant. Kept as a diagnostic only; enable
// with EPD LUT ON.
static const uint8_t aquavate_lut_fpc7519[] = {
    SSD1680_WRITE_LUT, 153,
    // VS section: 5 rows x 12 bytes
    0x20, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L0 black
    0x08, 0x48, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L1
    0x02, 0x48, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L2
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L3 white
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L4
    // Timing section: 12 groups x 7 bytes
    0x0a, 0x19, 0x00, 0x03, 0x08, 0x00, 0x00,  // G0
    0x14, 0x01, 0x00, 0x14, 0x01, 0x00, 0x03,  // G1
    0x0a, 0x03, 0x00, 0x08, 0x19, 0x00, 0x00,  // G2
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,  // G3
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G4
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G5
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G6
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G9
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G10
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G11
    // FR / XON section: 9 bytes
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0xFE};

// Strengthened variant of the LUT above for an aged panel that drives
// correctly but does not hold state: same VS polarity sequences (preserving
// approximate DC balance), drive durations roughly doubled, final phase
// lengthened and repeated. Waveform runs ~2x longer - raise the blind wait
// (EPD WAIT 5000) when testing.
//
// TESTED: made rendering WORSE (more noise, black fill unchanged). Longer drive
// is not the answer - do not re-try this direction without new evidence. These
// timings are a hand-scaled guess, not vendor values. Kept as recorded negative
// result; enable with EPD LUT STRONG.
static const uint8_t aquavate_lut_strong[] = {
    SSD1680_WRITE_LUT, 153,
    // VS section: identical polarity sequences to the fpc7519 LUT
    0x20, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L0 black
    0x08, 0x48, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L1
    0x02, 0x48, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L2
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L3 white
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // L4
    // Timing section: durations ~2x, final group held longer and repeated
    0x14, 0x32, 0x00, 0x06, 0x10, 0x00, 0x00,  // G0 (was 0a 19 . 03 08 . .)
    0x28, 0x02, 0x00, 0x28, 0x02, 0x00, 0x03,  // G1 (was 14 01 . 14 01 . 03)
    0x14, 0x06, 0x00, 0x10, 0x32, 0x00, 0x00,  // G2 (was 0a 03 . 08 19 . .)
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,  // G3 final hold (was 01 ... 01)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G4
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G5
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G6
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G9
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G10
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // G11
    // FR / XON section: unchanged
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0xFE};

class AquavateEPD : public ThinkInk_213_Mono_GDEY0213B74 {
 public:
  using ThinkInk_213_Mono_GDEY0213B74::ThinkInk_213_Mono_GDEY0213B74;

  void begin(thinkinkmode_t mode = THINKINK_MONO) {
    ThinkInk_213_Mono_GDEY0213B74::begin(mode);

    // Read at powerUp()/update() time, so setting them after begin() is fine.
    _epd_init_code = aquavate_ssd1680_init;

    // FIX 1 - the important one. Display Update Control 2 (0x22). Adafruit
    // sends 0xF4, which omits bit 1 (disable analog) and bit 0 (disable clock),
    // so the charge pump keeps running and +/-15/20V stay on the panel until the
    // next powerUp() - hours overnight, days in backpack mode. 0xF7 makes the
    // controller sequence its own power-down as the final step of the waveform.
    //
    // Deliberately NOT fixed by passing display(true): with no BUSY pin that
    // fires SW_RESET partway through the waveform and truncates it, which is
    // worse. Let the controller do it.
    _display_update_val = 0xF7;

    setRefreshWaitMs(AQUAVATE_EPD_REFRESH_WAIT_MS);
  }

  /**
   * Set the total blind wait after MASTER_ACTIVATE, in milliseconds.
   *
   * Tunes only the refresh wait. busy_wait() is deliberately left alone: it is
   * also called twice in powerUp() and once in powerDown(), so overriding it
   * would multiply this delay across every refresh rather than applying it once.
   */
  void setRefreshWaitMs(uint16_t ms) {
    if (ms < AQUAVATE_EPD_BASE_BUSY_WAIT_MS) {
      ms = AQUAVATE_EPD_BASE_BUSY_WAIT_MS;
    }
    // update() waits busy_wait() + default_refresh_delay when BUSY is absent
    default_refresh_delay = ms - AQUAVATE_EPD_BASE_BUSY_WAIT_MS;
  }

  uint16_t getRefreshWaitMs() const {
    return default_refresh_delay + AQUAVATE_EPD_BASE_BUSY_WAIT_MS;
  }

  /**
   * Force the waveform temperature bin instead of using the internal sensor.
   *
   * The panel needs more drive than its nominal-bin OTP waveform now provides
   * (fridge test: mixed content renders far better cold, i.e. with the longer
   * cold-bin waveform - the opposite of healthy e-ink behaviour). Writing a
   * forced temperature to 0x1A and skipping the sensor load makes the
   * controller select the waveform for that bin at any real temperature.
   * Colder bin = longer, harder drive. Trade-off: slower refresh and more ink
   * stress, so use the warmest bin that renders cleanly.
   */
  void setForcedTemperature(int8_t degC) {
    _forced_temp_degc = degC;
    _forced_temp_enabled = true;
  }

  void clearForcedTemperature() { _forced_temp_enabled = false; }
  bool isTemperatureForced() const { return _forced_temp_enabled; }
  int8_t getForcedTemperature() const { return _forced_temp_degc; }

  /**
   * VCOM control (Finding 6). The panel carries a factory-calibrated VCOM in
   * OTP; the init override replaces it with a generic value. A mismatched VCOM
   * leaves a standing DC offset across the ink, and marginal pixels relax
   * under it as soon as the active drive ends - observed as a perfect image
   * mid-waveform collapsing to salt-and-pepper at completion, worse warm
   * (mobile ink) than cold (viscous). useOtpVcom() drops the override.
   */
  void useOtpVcom() {
    _epd_init_code = aquavate_ssd1680_init_otp_vcom;
    _vcom_overridden = false;
  }

  void setVcomOverride(uint8_t val) {
    // Walk the (cmd, argc, args...) structure to find the VCOM entry, so a
    // data byte that happens to equal 0x2C can't be corrupted.
    uint8_t* p = aquavate_ssd1680_init;
    while (p[0] != 0xFE) {
      uint8_t cmd = p[0];
      uint8_t argc = p[1];
      if (cmd == SSD1680_WRITE_VCOM && argc == 1) {
        p[2] = val;
        break;
      }
      p += 2 + (cmd == 0xFF ? 0 : argc);
    }
    _epd_init_code = aquavate_ssd1680_init;
    _vcom_overridden = true;
    _vcom_value = val;
  }

  bool isVcomOverridden() const { return _vcom_overridden; }
  uint8_t getVcomValue() const { return _vcom_value; }

  /**
   * Experimental RAM LUT (diagnostic). When active, update() must NOT set the
   * load-LUT-from-OTP bit or the controller overwrites the RAM LUT - so the
   * update value drops to 0xC7 (also skips the temp load, which only matters
   * for OTP bin selection anyway).
   */
  void useExperimentalLut() { _epd_lut_code = aquavate_lut_fpc7519; }
  void useStrongLut() { _epd_lut_code = aquavate_lut_strong; }
  void clearExperimentalLut() { _epd_lut_code = NULL; }
  bool isExperimentalLutActive() const { return _epd_lut_code != NULL; }
  bool isStrongLutActive() const { return _epd_lut_code == aquavate_lut_strong; }

  void powerUp() {
    Adafruit_SSD1680::powerUp();  // also writes the RAM LUT if one is set

    if (isExperimentalLutActive()) {
      // Keep the RAM LUT: no OTP LUT load (bit 4), no temp load (bit 5)
      _display_update_val = 0xC7;
    } else if (_forced_temp_enabled) {
      // 12-bit signed temperature: degrees in the high byte, fraction (unused)
      // in the top nibble of the low byte.
      uint8_t buf[2] = {(uint8_t)_forced_temp_degc, 0x00};
      EPD_command(SSD1680_TEMP_WRITE, buf, 2);
      // Drop bit 5 (load temperature) so update() doesn't overwrite the forced
      // value with the internal sensor reading before the LUT load.
      _display_update_val = 0xF7 & ~0x20;  // 0xD7
    } else {
      _display_update_val = 0xF7;
    }
  }

 private:
  bool _forced_temp_enabled = false;
  int8_t _forced_temp_degc = 0;
  bool _vcom_overridden = true;   // begin() points at the 0x36-override init
  uint8_t _vcom_value = 0x36;
};

#endif  // AQUAVATE_EPD_H
