# Optimization Loop Log

## Run perf/auto-loop-20261109-0025, started 2026-11-09 00:25

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
- [ ] Idea 1: Cache `sin(lat)` and `cos(lat)` and pass into `corrected_hour_angle` / `altitude_of_celestial_body` instead of computing 5 times per hour-angle call. Expected gain: 8-15% time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 2: Add `-fno-math-errno` to `OPTFLAGS` in `native/Makefile` to permit inlining math builtins without `errno` checking. Expected gain: 3-8% time, Risk: Low, Files: `native/Makefile`
- [ ] Idea 3: Avoid redundant day+2 solar coordinates calculation when computing only tomorrow's Fajr in `engine_calculate_times`. Expected gain: 10-20% on `calculate_times`, Risk: Medium, Files: `native/engine/engine.c`
- [ ] Idea 4: Pass `solar_time_t` (144 bytes) and `solar_coords_t` by const pointer instead of pass-by-value across internal functions. Expected gain: 3-6% time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 5: Precompute `sin(dec)` and `cos(dec)` in `solar_coords_t` / `solar_time_t` to avoid repeating `deg2rad(d2)` and trig in `corrected_hour_angle`. Expected gain: 4-8% time, Risk: Low, Files: `native/engine/engine.c`
- [ ] Idea 6: Optimize `deg2rad` / `rad2deg` conversions and redundant angle normalizations across Meeus solar routines. Expected gain: 3-5% time, Risk: Medium, Files: `native/engine/engine.c`
- Deferred (future phase): GDI text caching and window invalidation optimizations in `native/host/` once Phase 2/3 host is built.

### Iterations
