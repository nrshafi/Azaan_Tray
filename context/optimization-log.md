# Optimization Loop Log

## Run perf/auto-loop-20261109-0110, started 2026-11-09 01:10

### Baseline
| Metric | Value |
|---|---|
| Tests | 2613 pass |
| calculate_times median | 3919.25 ns/call |
| calculate_month median | 59057.60 ns/call |
| azaan.exe size | 97280 bytes |
| test_engine.exe size | 67072 bytes |
| spikes/spike_a.exe size | 66653 bytes |
| spikes/spike_a_dc.exe size | 66133 bytes |
| spikes/spike_a_gdi.exe size | 23552 bytes |
| spikes/spike_b.exe size | 69974 bytes |
| spikes/spike_c.exe size | 86528 bytes |
| Golden SHA-256 | 95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a |

### Backlog
- [ ] Idea 1: Elide unused `transit_hours` and `sunset_hours` in `create_solar_time_with_coords` for tomorrow's solar time when not carried forward as full day, and propagate `sin_lat`/`cos_lat`. Expected gain: 5-8% on calculate_times, 2-4% on calculate_month, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 7: Vectorize / unroll monthly sliding window coordinate stepping in `engine_calculate_month`. Expected gain: 2-4% month time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 8: Factor Horner's polynomial evaluations for `theta0`, `l0`, `lp`, and `omega` in `compute_solar_coordinates`. Expected gain: 1-3% time, Risk: Medium, Files: `native/engine/engine.c`
- [ ] Idea 9: Dead code stripping and crt runtime minimization in `azaan.exe` (remove unnecessary stdio formats). Expected gain: 5-10% binary size, Risk: Medium, Files: `native/cli/main.c`, `native/Makefile`
- [ ] Idea 10: Struct alignment and cache-line packing for `solar_time_t` (currently 152 bytes). Expected gain: 1-2% time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 11: Pass `solar_coords_t` by const pointer instead of pass-by-value across internal functions. Expected gain: 1-2% time, Risk: Low, Files: `native/engine/engine.c`
- Deferred (future phase): GDI text caching and window invalidation optimizations in `native/host/` once Phase 2/3 host is built.

### Iterations

#### #1 Elide unused transit and sunset calculations for tomorrow: REJECTED
- Hypothesis: In compute_day_prayers, eliding unused transit_hours and sunset_hours for tomorrow_solar_time when tomorrow is not carried forward as a full day reduces engine_calculate_times by >= 3% with byte-identical output.
- Change: `native/engine/engine.c`. Passed `full_tomorrow` flag to skip `corrected_transit` and `sunset_hours` in `create_solar_time_with_coords`.
- Result: Test gate failed. In high-latitude locations (such as Oslo with 1/7th rule), tomorrow's safe Fajr calculation requires tomorrow's `night_sec`, which depends on tomorrow's `sunset_sec = hours_to_truncated_seconds(solar_time.sunset_hours)`. Setting tomorrow's `sunset_hours` to NAN caused `night_sec` to be NAN, altering midnight / Sunnah times by 13-16 minutes in high-latitude months.
- Golden: N/A (failed test gate before golden check)
- Commit: reverted
- Notes / next idea it suggests: Ground rule 1 applied: correctness beats speed. Tomorrow's sunset is an essential dependency for high-latitude night duration when computing Sunnah times. Reverted cleanly to green.

---

## Prior Run: perf/auto-loop-20261109-0025, started 2026-11-09 00:25

### Baseline
| Metric | Value |
|---|---|
| Tests | 2593 pass |
| calculate_times median | 6077.83 ns/call |
| calculate_month median | 69444.48 ns/call |
| azaan.exe size | 96256 bytes |
| test_engine.exe size | 64000 bytes |
| Golden SHA-256 | 95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a |

### Backlog
- [x] Idea 1: Cache `sin(lat)` and `cos(lat)` and pass into `corrected_hour_angle` / `altitude_of_celestial_body` instead of computing 5 times per hour-angle call. Expected gain: 8-15% time, Risk: Low, Files: `native/engine/engine.c`
- [x] Idea 2: Add `-fno-math-errno` to `OPTFLAGS` in `native/Makefile` to permit inlining math builtins without `errno` checking. Expected gain: 3-8% time, Risk: Low, Files: `native/Makefile` (Tested in #3: Result +0.6%, rejected)
- [x] Idea 3: Avoid redundant day+2 solar coordinates calculation when computing only tomorrow's Fajr in `engine_calculate_times`. Expected gain: 10-20% on `calculate_times`, Risk: Medium, Files: `native/engine/engine.c`
- [x] Idea 4: Precompute `sin(dec)` and `cos(dec)` in `solar_coords_t` / `solar_time_t` to avoid repeating `deg2rad(d2)` and trig in `corrected_hour_angle`. Expected gain: 4-8% time, Risk: Low, Files: `native/engine/engine.c`
- [x] Idea 5: Pair FAST_SINCOS for delta and h in `corrected_hour_angle`. Expected gain: 3-5% time, Risk: Low, Files: `native/engine/engine.c` (Tested in #5: Result 0%, rejected)
- [x] Idea 6: Linear Julian Day stepping, inline `add_one_day` calendar helper, and shared `h0_angle` between sunrise & sunset in `create_solar_time_with_coords`. Expected gain: 5-8% time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 7: Vectorize / unroll monthly sliding window coordinate stepping in `engine_calculate_month`. Expected gain: 2-4% month time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 8: Factor Horner's polynomial evaluations for `theta0`, `l0`, `lp`, and `omega` in `compute_solar_coordinates`. Expected gain: 1-3% time, Risk: Medium, Files: `native/engine/engine.c`
- [ ] Idea 9: Dead code stripping and crt runtime minimization in `azaan.exe` (remove unnecessary stdio formats). Expected gain: 5-10% binary size, Risk: Medium, Files: `native/cli/main.c`, `native/Makefile`
- [ ] Idea 10: Struct alignment and cache-line packing for `solar_time_t` (currently 144 bytes). Expected gain: 1-2% time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 11: Pass `solar_coords_t` by const pointer instead of pass-by-value across internal functions. Expected gain: 1-2% time, Risk: Low, Files: `native/engine/engine.c`
- Deferred (future phase): GDI text caching and window invalidation optimizations in `native/host/` once Phase 2/3 host is built.

### Iterations

#### #1 Cache sin/cos(latitude) and fold declination trig: KEPT
- Hypothesis: Caching sin/cos(latitude) in solar_time_t and passing to corrected_hour_angle / altitude_of_celestial_body reduces engine_calculate_times by >= 3% with byte-identical output.
- Change: `native/engine/engine.c`, `native/tests/test_engine.c`. Added `sin_lat` and `cos_lat` to `solar_time_t` computed once via `FAST_SINCOS`; reused directly in `corrected_hour_angle` and `altitude_of_celestial_body`; used `FAST_SINCOS` on `d2_rad`. Added pinning test in `test_engine.c`.
- Result:
  - `calculate_times`: 6077.83 -> 5836.74 ns/call (-3.97%)
  - `calculate_month`: 69444.48 -> 65599.52 ns/call (-5.54%)
  - `azaan.exe` size: 96256 -> 96256 bytes (0.00%)
  - `test_engine.exe` size: 64000 -> 66048 bytes (+3.20%, added 20 test assertions)
- Golden: identical (SHA-256: `95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`)
- Commit: `ba88e01`
- Notes / next idea it suggests: Now that hour angle evaluation avoids repeated trig, inspect the day+2 solar coordinates evaluation in `compute_day_prayers` and compiler flag `-fno-math-errno`.

#### #2 Skip unrequested prayers and defer tomorrow solar coords: KEPT
- Hypothesis: Skipping unrequested prayer calculations (dhuhr, asr, sunset, maghrib, isha) and lazily evaluating tomorrow's solar time in compute_day_prayers reduces engine_calculate_times by >= 10% with byte-identical output.
- Change: `native/engine/engine.c`. Conditioned dhuhr, asr, sunset, maghrib, and isha calculations in `compute_day_prayers` on non-NULL output pointers. Deferred computation of `tomorrow_solar_time` and `next_tomorrow` (Day D+2/D+3) only when `need_tomorrow` is required for night duration or caller requested `out_solar_tomorrow`.
- Result:
  - `calculate_times`: 5836.74 -> 4371.62 ns/call (-25.10% iteration gain; -28.07% cumulative)
  - `calculate_month`: 65599.52 -> 64760.48 ns/call (-1.28% iteration gain; -6.75% cumulative)
  - `azaan.exe` size: 96256 -> 97280 bytes (+1.06%)
  - `test_engine.exe` size: 66048 -> 66560 bytes (+0.78%)
- Golden: identical (SHA-256: `95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`)
- Commit: `7ec958d`
- Notes / next idea it suggests: Day+2 solar elimination delivered a 25.1% latency cut. Test compiler math optimizations (-fno-math-errno) and struct pointer passing next.

#### #3 Add -fno-math-errno to Makefile OPTFLAGS: REJECTED
- Hypothesis: Adding -fno-math-errno to OPTFLAGS in native/Makefile allows GCC to inline math builtins without errno setting overhead, reducing calculation latency by >= 3% while remaining byte-identical.
- Change: `native/Makefile`. Added `-fno-math-errno` to `OPTFLAGS`.
- Result:
  - `calculate_times`: 4371.62 -> 4347.22 ns/call (-0.56% iteration gain, second test: 4342.62 ns/call / -0.66%)
  - `calculate_month`: 64760.48 -> 64563.28 ns/call (-0.30% iteration gain, second test: 64401.26 ns/call / -0.55%)
  - `azaan.exe` size: 97280 -> 97280 bytes (0.00%)
  - `test_engine.exe` size: 66560 -> 66560 bytes (0.00%)
- Golden: identical (SHA-256: `95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`)
- Commit: reverted
- Notes / next idea it suggests: Gain of ~0.6% was below the >= 3% threshold after 6 confirmation runs. On x86_64 mingw with -Os, standard math functions already use fast builtins or inline calls; errno checks do not dominate. Reverted cleanly.

#### #4 Precompute sin/cos(declination) in solar_time_t: KEPT
- Hypothesis: Caching sin/cos(declination) in solar_time_t eliminates repeated declination trigonometry in corrected_hour_angle, reducing calculate_times and calculate_month by >= 3% with byte-identical output.
- Change: `native/engine/engine.c`. Stored `sin_dec` and `cos_dec` in `solar_time_t` evaluated once per day via `FAST_SINCOS(d2_rad)` in `create_solar_time_with_coords`; passed directly into `corrected_hour_angle`, avoiding `deg2rad` and trigonometric calls on every hour angle evaluation.
- Result:
  - `calculate_times`: 4371.62 -> 4192.21 ns/call (-4.10% iteration gain; -31.02% cumulative)
  - `calculate_month`: 64760.48 -> 61185.30 ns/call (-5.52% iteration gain; -11.89% cumulative)
  - `azaan.exe` size: 97280 -> 97280 bytes (0.00%)
  - `test_engine.exe` size: 66560 -> 66560 bytes (0.00%)
- Golden: identical (SHA-256: `95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`)
- Commit: `307050c`
- Notes / next idea it suggests: Monthly calculation improved by 5.5% and single-day times by 4.1%. Test passing solar_time_t via pointers next.

#### #5 Pair FAST_SINCOS for delta and h in corrected_hour_angle: REJECTED
- Hypothesis: In corrected_hour_angle, evaluating delta and h using paired FAST_SINCOS eliminates redundant cos(delta) and deg2rad evaluations between altitude calculation and Newton-Raphson term4, improving calculation time by >= 3% with byte-identical output.
- Change: `native/engine/engine.c`. Replaced isolated `sin()`/`cos()` calls for `delta` and `h` with `FAST_SINCOS`, passing trigonometric outputs directly to `altitude_of_celestial_body` and `term4`.
- Result:
  - `calculate_times`: 4192.21 -> 4204.94 ns/call (+0.30% noise, confirmation 4230.85 ns/call / +0.92%)
  - `calculate_month`: 61185.30 -> 60923.84 ns/call (-0.43% noise, confirmation 61305.14 ns/call / +0.20%)
  - `azaan.exe` size: 97280 -> 97280 bytes (0.00%)
  - `test_engine.exe` size: 66560 -> 66560 bytes (0.00%)
- Golden: identical (SHA-256: `95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`)
- Commit: reverted
- Notes / next idea it suggests: Results were within +/-0.5% noise across 6 runs (< 3% threshold). x86_64 GCC already optimizes paired trig registers in this basic block. Reverted cleanly.

#### #6 Linear Julian Day stepping, add_one_day helper, and shared h0_angle: KEPT
- Hypothesis: Replacing Meeus inverse calendar division with a fast inline calendar stepper (`add_one_day`), stepping Julian day linearly in `calculate_month`, and sharing the invariant `h0_angle` computation between sunrise and sunset in `create_solar_time_with_coords` reduces calculation latency by >= 3% with byte-identical output.
- Change: `native/engine/engine.c`. Implemented inline `add_one_day` for non-boundary dates (day < 28 or day < dim); passed Julian day `jd` directly into `compute_day_prayers` and stepped linearly by `+ (double)(day - 1)` in `engine_calculate_month`; factored `corrected_hour_angle_from_m` to compute `sin(deg2rad(SOLAR_ALTITUDE_DEG))` and `acos(ratio)` once across sunrise and sunset.
- Result:
  - `calculate_times`: 4192.21 -> 3879.72 ns/call (-7.45% iteration gain; -36.17% cumulative)
  - `calculate_month`: 61185.30 -> 57084.64 ns/call (-6.70% iteration gain; -17.80% cumulative)
  - `azaan.exe` size: 97280 -> 97280 bytes (0.00%)
  - `test_engine.exe` size: 66560 -> 66560 bytes (0.00%)
- Golden: identical (SHA-256: `95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`)
- Commit: `568e816`
- Notes / next idea it suggests: Excellent combined gains (-7.5% single-day, -6.7% month). Cumulative latency reduction is now 36.2% on single-day and 17.8% on monthly calculations.

---

## 5. Final Summary (End of Run)

### Overview
- **Branch**: `perf/auto-loop-20261109-0025`
- **Total Iterations**: 6
- **Kept**: 4 (Iterations #1, #2, #4, #6)
- **Rejected**: 2 (Iterations #3, #5 - both cleanly reverted)
- **Status of Gates**:
  - Build gate: 0 warnings, 0 errors.
  - Test gate: 2,613 assertions pass (100% pass rate, +20 tests added).
  - Golden gate: MATCH (`95aed7f803b3806d23a62fa88ff74455dda70a6b794e0a1d18900feb7501255a`).
  - Gain gate: Passed in all kept iterations (each achieved >= 3% latency reduction).

### Cumulative Metric Progression
| Metric | Baseline | Iteration #1 | Iteration #2 | Iteration #4 | Iteration #6 (Final) | Cumulative Delta |
|---|---|---|---|---|---|---|
| Tests Passed | 2,593 | 2,613 | 2,613 | 2,613 | 2,613 | **+20 assertions (+0.77%)** |
| `calculate_times` median | 6,077.83 ns | 5,836.74 ns | 4,371.62 ns | 4,192.21 ns | 3,879.72 ns | **-2,198.11 ns (-36.17%)** |
| `calculate_month` median | 69,444.48 ns | 65,599.52 ns | 64,760.48 ns | 61,185.30 ns | 57,084.64 ns | **-12,359.84 ns (-17.80%)** |
| `azaan.exe` size | 96,256 B | 96,256 B | 97,280 B | 97,280 B | 97,280 B | **+1,024 B (+1.06%)** |
| `test_engine.exe` size | 64,000 B | 66,048 B | 66,560 B | 66,560 B | 66,560 B | **+2,560 B (+4.00%)** |
| Golden Snapshot SHA-256 | `95aed7f...` | `95aed7f...` | `95aed7f...` | `95aed7f...` | `95aed7f...` | **MATCH (Identical)** |

### Commits Produced
1. `ba88e01`: `perf(engine): cache sin/cos(lat) and reuse in hour angle evaluations (-4.0% times, -5.5% month)`
2. `7ec958d`: `perf(engine): skip unrequested prayer paths and defer tomorrow solar coords (-25.1% times, -1.3% month)`
3. `307050c`: `perf(engine): precompute sin/cos(declination) in solar_time_t (-4.1% times, -5.5% month)`
4. `568e816`: `perf(engine): linear JD stepping, add_one_day helper, and shared h0_angle (-7.5% times, -6.7% month)`

### Top Remaining Backlog Ideas (Ranked by Expected Value)
1. **Idea 9 (Binary size minimization)**: Dead code stripping and crt runtime minimization in `azaan.exe` (remove unnecessary stdio formats). Expected value: high for binary size (-5% to -10%), zero risk to engine math.
2. **Idea 7 (Vectorize / unroll monthly sliding window)**: Optimize coordinate array traversal and sliding window state handoff in `engine_calculate_month`. Expected gain: 2-4% month latency.
3. **Idea 8 (Horner's polynomial factoring)**: Optimize Horner's polynomial evaluations for `theta0`, `l0`, `lp`, and `omega` in `compute_solar_coordinates`. Expected gain: 1-3% time.
4. **Idea 10 (Struct packing)**: Align and pack `solar_time_t` for optimal cache-line utilization. Expected gain: 1-2% time.
5. **Idea 11 (Pointer passing for coordinates)**: Pass `solar_coords_t` (24 bytes) via pointer instead of value in internal functions. Expected gain: 1% time.

### Human Review Notes & Verification
- **LTO limitation**: `w64devkit` GCC 16.2.0 does not include LTO support (`cc1.exe: error: LTO support has not been enabled in this configuration`), so whole-program optimization flags (`-flto`) cannot be used with this compiler distribution.
- **Floating-point bit equivalence**: All four kept optimizations strictly preserve the IEEE 754 double-precision bit-identical output across all 12 cities $\times$ 12 seasonal dates matrix tested by the golden snapshot runner.
- **Safety**: No global state, hidden singletons, or thread-safety hazards were introduced. All functions remain pure and reentrant.
