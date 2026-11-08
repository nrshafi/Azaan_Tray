# Autonomous Optimization Loop — Azaan Tray

You are running unattended for several hours in the repository
`I:\Development\Azaan_Tray` on Windows 11 (PowerShell + w64devkit GCC/Make).
Your job is to repeatedly **find → implement → test → measure → keep or
revert → record** optimizations, one at a time, until the time budget or a
stop condition is reached. Nobody will answer questions while you run, so
when you're unsure, take the conservative option and write the doubt down in
the log instead of guessing.

---

## 0. Ground rules (these apply in every iteration)

1. **Correctness beats speed.** Prayer times are religious obligations. A
   change that moves any computed time by even one minute, alters a Hijri
   date, or changes a forbidden window is a **regression** and must be
   reverted, however fast it is.
2. **Never weaken the safety net.** Do not edit, delete, skip, or loosen
   existing tests, fixtures (`native/tests/fixtures/`), tolerances, or
   `-Werror -Wall -Wextra -Wpedantic`. You may **add** tests.
3. **Never use `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations`, or
   `-ffinite-math-only`**: they change floating-point results. Individual
   safe flags such as `-fno-math-errno` are allowed if the golden diff
   (section 2) stays byte-identical.
4. **One optimization per commit.** Small, isolated, revertable.
5. **Stay in scope.** Optimize code that exists: `native/engine/`,
   `native/cli/`, `native/tests/`, `native/Makefile`, and the `spikes/`
   prototypes. Do **not** start Phase 2 (`native/host/` is empty on purpose)
   or build new features, UI, or packaging. If an idea only applies to
   future phases, put it in the backlog under "Deferred (future phase)".
6. **No new dependencies.** No new libraries, vendored code, or package
   installs.
7. **API verification.** Before writing code against any Win32, WebView2,
   GCC flag, or C library API you haven't already seen used in this repo,
   follow `context/code-standards.md` → "Documentation and API
   Verification", and record the source in `context/progress-tracker.md` →
   "API Verification Log". If you can't verify an API, skip that idea and log
   it as "unverified".
8. **Git hygiene.** Work only on branch `perf/auto-loop-<YYYYMMDD-HHMM>`.
   Never push, never force, never rewrite history, never touch `master`.
   Commit messages use `perf(<area>): <what> (<measured gain>)`.
9. **Never claim a gain you didn't measure.** Every kept change needs
   before/after numbers from the same machine in the same session.

---

## 1. Startup (do this once)

1. Read, in order: `AGENTS.md`, then every file it lists under `context/`,
   then `docs/adr/0007-cross-cutting-optimizations-and-power-efficiency.md`.
   You need to know what has already been optimized so you don't redo it
   (FAST_SINCOS, multiple-angle identities, `engine_calculate_month` sliding
   window, GDI timer suspension, WebView2 TrySuspend/pre-warm).
2. Set up the toolchain in the shell:
   ```powershell
   . .\scripts\env.ps1
   ```
   Confirm `gcc --version` and `make --version` work.
3. Make sure the tree is clean (`git status`), then create the work branch:
   ```powershell
   git switch -c perf/auto-loop-$(Get-Date -Format yyyyMMdd-HHmm)
   ```
4. Clean build and test the baseline:
   ```powershell
   make -C native clean
   make -C native all
   ```
   All tests must pass (currently 2,593 assertions). If the baseline
   fails, **stop**, write the failure to the log, and end the run.
5. **Build the measurement harness if it doesn't exist** (commit it on its
   own as `test(bench): add engine benchmark + golden snapshot`):
   - `native/tests/bench_engine.c`: times, using
     `QueryPerformanceCounter`, (a) N = 100,000 calls to
     `engine_calculate_times` across a fixed set of cities/dates and
     (b) N = 5,000 calls to `engine_calculate_month`. Run each 5 times
     and print the **median** ns/call. Also print a checksum of all
     outputs so the compiler can't dead-code-eliminate the work.
   - `make -C native bench` target that builds and runs it with the same
     `CFLAGS`.
   - `scripts/golden-snapshot.ps1`: runs `native\bin\azaan.exe --json`
     for a fixed grid: at least 12 cities (include Dhaka, Mecca, London,
     Oslo, Tromsø 69.65N, Reykjavik, Sydney, Cape Town, New York,
     Anchorage, Singapore, Quito on the equator) × at least 12 dates
     (solstices, equinoxes, 2024-02-29, 2023-12-31→2024-01-01 boundary, a
     Ramadan date). Concatenate everything into one file and print its
     SHA-256.
6. Record the **baseline** in the log (section 4): test count, bench
   medians, golden SHA-256, and the byte sizes of `native/bin/azaan.exe`,
   `native/bin/test_engine.exe`, and each `spikes/*.exe` you can rebuild.
   Save the baseline golden output to
   `native/tests/golden/baseline.json` and commit it.

---

## 2. The loop

Repeat until a stop condition (section 3) is met.

### Step A: Discover
Pick ONE candidate from the backlog in the log. If the backlog has fewer
than 5 open items, refill it first by profiling and reading code:
- Use `gcc -O2 -pg` / `gprof`, or count calls with temporary counters, on
  `bench_engine` to find the hottest functions. Remove temporary
  instrumentation before committing.
- Look for: repeated transcendental calls on invariant inputs (e.g.
  `sin/cos(latitude)` recomputed in `corrected_hour_angle` and
  `altitude_of_celestial_body`), repeated `deg2rad` conversions, redundant
  day+2 solar work when computing only tomorrow's Fajr, branches inside hot
  loops, struct copies that could be pointers, `double`→`int` rounding
  paths, string formatting in the CLI, and binary size (dead code, unused
  stdio, `-flto`, `--gc-sections` effectiveness, `-fno-math-errno`).
- For `spikes/`: per-paint GDI allocations, unnecessary `InvalidateRect`
  when text is unchanged, polling that could be event-driven. These have
  no automated tests, so only take spike changes you can verify by
  rebuilding cleanly and reasoning carefully; label them "unmeasured"
  unless you can measure them.

Score each idea by **expected gain × confidence ÷ risk** and take the top
one. Write a one-line hypothesis: *"Caching sin/cos(lat) per call cuts
engine_calculate_times by ≥10% with byte-identical output."*

### Step B: Implement
- Make the smallest change that tests the hypothesis.
- If the change touches engine math, **add a focused test first** (TDD)
  that pins the current behaviour of the function you're touching, and
  confirm it passes before the change.
- Keep the code standards in `context/code-standards.md`: C11, no UB, no
  globals with hidden state, readable names, a brief comment explaining any
  non-obvious identity or algebraic trick.

### Step C: Verify (all gates must pass; otherwise go to Step E: revert)
```powershell
make -C native clean
make -C native all          # zero warnings, all tests pass
make -C native bench        # 3 runs, take the median of medians
.\scripts\golden-snapshot.ps1
```
1. **Build gate:** no warnings, no errors.
2. **Test gate:** 100% pass; the test count is ≥ the previous count.
3. **Golden gate:** SHA-256 identical to baseline. If not, diff against
   `native/tests/golden/baseline.json`. **Any** difference means revert.
   Don't "fix" it by regenerating the baseline.
4. **Gain gate:** the change must improve at least one metric by
   **≥ 3%** (time or binary size) and must not regress any other metric
   by more than 1%. If the result is within noise (< 3%), re-run the
   bench 3 more times; if it's still < 3%, revert. Exception: if a
   simplification makes the code clearly simpler with no measurable cost,
   you may keep it, labelled "neutral/cleanup".

### Step D: Keep
- `git add` only the intended files; `git commit` with the measured gain
  in the message.
- Append the result to the log (section 4).
- Update `context/progress-tracker.md` with a dated one-line entry (date
  and time) under a "Autonomous optimization loop" bullet. Keep it short;
  details go in the log.
- If the change alters architecture, invariants, or build flags, update
  the relevant `context/` file or add a short ADR
  (`docs/adr/0008-...`, then the next number), as `AGENTS.md` requires.

### Step E: Revert
- `git restore --staged . ; git restore .` and delete any untracked files
  you created for this attempt (`git clean -n` first to see them; only
  remove your own files).
- Rebuild and re-run the tests to confirm you're back to green.
- Log the attempt as **rejected**, with the reason and numbers. Rejected
  ideas are valuable, because they stop you or a future run from trying
  the same thing again.

### Step F: Re-discover
Re-profile after every **kept** change, because the hot spots move. Then go
back to Step A.

---

## 3. Stop conditions

Stop the loop and go to section 5 when ANY of these is true:
- The wall-clock budget is used up (default **4 hours** from startup;
  leave 15 minutes for the final report).
- **6 consecutive** attempts were rejected.
- The backlog is empty and two profiling passes found nothing above 3%
  expected gain.
- The baseline can't be restored to green after a revert (stop
  immediately and report the exact state; do not try heroic fixes).
- Any instruction here would require breaking a ground rule.

---

## 4. The log: `context/optimization-log.md`

Create it at startup and keep it current after every iteration. Structure:

```markdown
# Optimization Loop Log

## Run <branch name>, started <date time>
### Baseline
| Metric | Value |
|---|---|
| Tests | 2593 pass |
| calculate_times median | … ns/call |
| calculate_month median | … ns/call |
| azaan.exe size | … bytes |
| test_engine.exe size | … bytes |
| Golden SHA-256 | … |

### Backlog
- [ ] <idea>: expected gain, risk, files
- Deferred (future phase): …

### Iterations
#### #1 <title>: KEPT | REJECTED | NEUTRAL
- Hypothesis: …
- Change: files, 1-2 sentences
- Result: before → after for each metric (median of 3)
- Golden: identical / DIFF (reverted)
- Commit: <sha> | reverted
- Notes / next idea it suggests: …
```

---

## 5. Final report (end of run)

1. Make sure the tree is clean, all tests pass, and golden is identical to
   baseline.
2. Append a **Summary** to the log: total iterations, kept/rejected counts,
   cumulative before→after for every baseline metric, list of commits, top 5
   remaining backlog ideas ranked by expected value, and any doubts or
   unverified assumptions a human should review.
3. Commit the log and tracker update:
   `docs(perf): optimization loop summary`.
4. Do **not** merge into `master` and do **not** push. A human will review
   the branch.

---

## 6. Known repo quirks

- A stray empty directory `native/-p/` exists, left behind by a
  `mkdir -p` run under cmd. Removing it is fine, and counts as a
  cleanup commit.
- Build outputs (`*.exe`, `bin/`) are git-ignored; don't commit
  binaries.
- `make` must run from a shell where `scripts\env.ps1` has been
  dot-sourced; otherwise the system GCC (if any) may differ from the
  pinned w64devkit GCC 16.2.0, and benchmark numbers won't be comparable.
- Benchmarks are noisy on a desktop. Close nothing, change nothing about
  the machine, and always compare medians from the same session.
