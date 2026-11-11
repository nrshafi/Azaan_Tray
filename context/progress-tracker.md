# Progress Tracker

Update this file after every meaningful implementation change with current date and time

## Current Phase

- **Phase 0 (Foundations and Risk Spikes) COMPLETE** (with open items below: Spike B test matrix,
  warm-open measurement).
- **Phase 1 (Prayer Engine) COMPLETE on Windows** after the 2026-11-12 review fixes; the Linux-GCC CI
  part of the Phase 1 exit gate is still open.
- Next: **Phase 2: Host Core**.

## Current Goal

- Start Phase 2 (unit 2.1), applying the spike-promotion checklist under Next Up.

## Completed

- 2026-11-07: Context files filled in from `SRS.md` plus design review:
  `project-overview.md`, `architecture.md`, `code-standards.md`,
  `ui-context.md`, `ai-workflow-rules.md`, `build-plan.md`. Documentation and
  API Verification rules added (code-standards, ai-workflow-rules, this file).
- 2026-11-07: Project skills installed in `.claude/skills/`: find-docs,
  context7-cli, tdd, codebase-design, frontend-design, shadcn,
  vercel-react-best-practices, web-design-guidelines, accessibility,
  security-and-hardening (from the local skills library), webapp-testing
  (from github.com/anthropics/skills).
- 2026-11-07 20:54: **Unit 0.1 Repository and toolchain complete**:
  Installed portable w64devkit (v2.11.0, GCC 16.2.0, GNU Make 4.4.1) into `tools/w64devkit/`;
  downloaded pinned WebView2 SDK (1.0.2903.40 headers and x64/arm64 loader DLLs);
  created `scripts/env.ps1`, `scripts/toolchain.md`, `native/third_party/VERSIONS.md`,
  `.gitignore`, `.gitattributes`, `.editorconfig`, `LICENSE` (MIT), `README.md`,
  `docs/adr/0001-record-architecture-decisions.md`, and skeleton directory structure.
- 2026-11-07 21:28: **Unit 0.2 Spike A (Minimal Host Footprint) complete**:
  - Direct2D HWND Target: WS 49.4 MB, Private 34.5 MB, Threads 16 (fails idle budget due to full D3D11/DXGI hardware driver load).
  - Direct2D Software DC Target: WS 20.2 MB, Private 7.28 MB, Threads 7.
  - **Double-buffered GDI ClearType: WS 1.07 MB, Private 1.8 MB, Threads 4, Handles 123, GDI Objects 10** (easily beats the ≤ 5 MB / ≤ 10 MB budget).
- 2026-11-07 21:28: **Unit 0.3 Spike B (Taskbar Embedding) complete**:
  - Discovered existing siblings `TrafficMonitorTaskbarWindow` (#32770) and `AwqatSalaatWidgetWinRT` on the user's active desktop.
  - Implemented sibling-aware collision offset docking to the left of `TrayNotifyWnd` and existing docked widgets.
  - Re-dock handler wired on `TaskbarCreated`.
- 2026-11-07 21:28: **Unit 0.4 Spike C (WebView2 Lifecycle & Linger) complete**:
  - MinGW compilation verified with dynamic `WebView2Loader.dll` and lightweight COM callback templates.
  - Virtual host mapping to `https://app.azaantray.invalid/` successfully served local HTML.
  - Cold start click-to-app-ready measured: **844 ms**.
  - Active process group memory: 5 processes, **135.5 MB** private commit (under 150 MB ceiling).
  - Clean teardown verified: 0 remaining `msedgewebview2.exe` processes after `Controller->Close()`.
- 2026-11-07 21:29: **Unit 0.5 Decide and Document complete**:
  - Authored ADRs 0002 (Toolchain & Host Language), 0003 (Widget GDI Rendering), 0004 (WebView2 Linger), 0005 (cJSON), 0006 (MIT Licence).
  - Open Questions 1, 2, and 4 resolved.
- 2026-11-07 21:35: **Phase 1: Prayer Engine COMPLETE**:
  - **Unit 1.1**: Created fixture generator in `scripts/gen-fixtures/` with Bun and `adhan@4.4.3`. Generated 2,016 golden test fixtures covering 12 global cities, 7 seasonal/solstice/equinox/leap dates, and all calculation methods.
  - **Units 1.2–1.7**: Implemented pure C11 calculation engine in `native/engine/engine.c` and `engine.h` (Meeus solar coordinates, Equation of Time, hour angles, Asr Standard & Hanafi shadow factors, high-latitude 1/7th & midnight rules, forbidden windows, and tabular Hijri calendar).
  - Built comprehensive unit test suite in `native/tests/test_engine.c`. All 33 tests pass with 0 failures under `make -C native test`.
  - **Unit 1.8**: Implemented CLI utility `native/bin/azaan.exe` supporting `--print`, `--json`, `--date`, `--lat`, and `--lng`. Dhaka prayer times verified against Islamic Foundation Bangladesh timetable to the minute.
- 2026-11-08 23:55: **Deep Cross-Cutting Optimizations & Power Management COMPLETE**:
  - **Prayer Engine**: Implemented `FAST_SINCOS` trigonometric folding and multiple-angle identities in `compute_solar_coordinates` reducing trigonometric function calls by ~67%.
  - **Monthly Sliding Window**: Added `engine_calculate_month` computing 30/31 days with sliding window reuse across adjacent days, eliminating 64% of solar coordinate calculations.
  - **Unit Test Suite**: Expanded tests in `native/tests/test_engine.c` from 33 to 2,593 assertions covering leap years, 30/31 day months, and high-latitude / polar edge cases. All 2,593 tests pass with 0 failures under `make -C native test`.
  - **GDI Host & Power**: Implemented adaptive timer cadence (seconds vs minutes mode reducing wakeups 60x), `RegisterPowerSettingNotification` for `GUID_MONITOR_POWER_ON`, and `WTSRegisterSessionNotification` for workstation lock/unlock in `spike_a_gdi.cpp`. Timers are completely suspended during monitor sleep and session lock, achieving true 0.0% CPU and zero wakes.
  - **WebView2 Lifecycle**: Integrated `ICoreWebView2_3::TrySuspend` when entering linger and `Resume` on reopen; added speculative hover pre-warming (`WM_MOUSEHOVER`) reducing perceived cold-open latency; stripped Chromium background feature flags; added working set trimming on teardown in `spike_c_webview2.cpp`.
  - **Documentation**: Authored `docs/adr/0007-cross-cutting-optimizations-and-power-efficiency.md` recording motivations, measurements, decisions, and why alternative approaches were rejected.
- 2026-11-09 00:32: **Autonomous optimization loop**: #1 Cached sin/cos(lat) and folded declination trig (-5.5% month, -4.0% times, golden MATCH).
- 2026-11-09 00:36: **Autonomous optimization loop**: #2 Skipped unrequested prayer paths & deferred tomorrow solar coords (-25.1% times, -1.3% month, golden MATCH).
- 2026-11-09 00:40: **Autonomous optimization loop**: #4 Precomputed sin/cos(dec) in solar_time_t (-5.5% month, -4.1% times, golden MATCH).
- 2026-11-09 00:51: **Autonomous optimization loop**: #6 Linear Julian Day stepping, add_one_day helper, and shared h0_angle (-7.5% times, -6.7% month, golden MATCH). Cumulative latency reduction: -36.2% on calculate_times, -17.8% on calculate_month. Golden snapshot: MATCH.
- 2026-11-09 01:23: **Autonomous optimization loop**: #2 Minimized CRT stdio runtime via -D__USE_MINGW_ANSI_STDIO=0 (-66.8% azaan.exe size from 97,280 to 32,256 B, -41.2% test_engine.exe size, golden MATCH).
- 2026-11-09 01:27: **Autonomous optimization loop**: #3 Precomputed interpolation deltas and trig products in solar_time_t (-3.65% month, -2.86% times, golden MATCH). Cumulative: -66.8% azaan.exe, -5.4% month latency.
- 2026-11-09 01:38: **Autonomous optimization loop**: #4 Eliminated dead struct storage (`prev_solar`), passed coordinate pointers, and elided unused struct copies (-4.76% azaan.exe size from 32,256 to 30,720 B, -2.07% calculate_times, -1.79% calculate_month, golden MATCH). Cumulative: -68.4% azaan.exe size, -7.1% month latency, -4.7% times latency.
- 2026-11-12 00:30: **Independent review of Phase 0/1 + engine correctness fixes**:
  - **Gap found**: `test_engine.c` never loaded the 2,016 adhan-js fixtures (only one hand-coded Dhaka case was checked
    against expected times), so units 1.2–1.5 ("golden tests pass") were not actually met. Comparing the engine
    against every fixture showed 250 mismatching cases; a 130k-case randomized differential run against adhan-js
    4.4.3 showed more.
  - **Fixed in `engine.c`**: (1) `round_seconds_to_minutes` chose the direction from the rounded seconds but added it
    to the fractional total, giving non-minute-aligned results up to 59 s off (all 1/7th-rule, twilight-angle and
    Sunnah-time paths); now matches adhan-js `roundedMinute` and lands on whole minutes. (2) Polar resolution:
    Fajr is now computed from the resolved solar time, the next day resolves independently, and AqrabBalad tests
    the first stepped latitude like adhan-js. (3) Next-day Fajr for midnight/last third always uses the night
    length (reverts part of optimization #2, see optimization-log). (4) Moonsighting Committee above 55° follows
    adhan-js in polar night (NaN instead of an angle time).
  - **Tests**: `test_engine` now checks every field of `prayer_fixtures.json` (2,016 cases) and a new seeded
    `prayer_fixtures_random.json` (1,500 cases, every high-latitude rule and polar resolver) to the second, plus
    regression and input-validation tests. `make -C native test`: 2,659/2,659 pass. Engine also matched adhan-js on 130k random cases,
    and `engine_calculate_month` matched `engine_calculate_times` on 1.77M day/setting combinations.
  - **CLI (unit 1.8)**: local time used `-tzi.Bias` (standard offset only, so DST and historic rules were
    ignored, an hour off in summer for DST zones). Now converts each event with
    `GetDynamicTimeZoneInformation` + `SystemTimeToTzSpecificLocalTimeEx`; verified against IANA
    America/New_York incl. both 2024 DST change days. Arguments are validated.
  - Golden snapshot: first regenerated in local time (30 London/Oslo summer lines changed by 1 min, each checked
    against adhan-js), then switched to the new `azaan --utc` flag so it no longer depends on the machine's
    time zone (old baseline stored London Dhuhr as 18:14 = UTC+6). New baseline `c5c5ef38…`.
    `calculate_times` 3.75 → 4.47 µs/call, `calculate_month` unchanged.
  - `engine_calculate_times` now rejects invalid dates / NaN coordinates / out-of-range methods.
  - Docs: architecture, build plan 3.1, project overview and README now say GDI (ADR 0003) instead of
    Direct2D; `VERSIONS.md` records cJSON 1.7.19, adhan-js and SHA-256 of committed binaries.
  - Review used three Gemini 3.8 Flash (high) reviewers plus an independent pass; their findings were
    verified against the code before being recorded here.

## In Progress

- None.

## Next Up

- Phase 2: Host Core (`native/host/`):
  - 2.1 App skeleton: `wWinMain`, single-instance mutex, hidden message window, clean exit, manifest.
  - 2.2 Paths + config: portable mode detection, `config.json` load/validate/migrate/atomic save.
- **Spike-promotion checklist** (review 2026-11-12; fix when spike code moves to `native/host/`):
  - Spike B docks the *main* window (which calls `PostQuitMessage` on `WM_DESTROY`) into `Shell_TrayWnd`;
    an Explorer restart destroys it with the taskbar and ends the process, so the `TaskbarCreated`
    re-dock path cannot run. Keep a hidden owner/message window for process lifetime and recreate the
    widget on `TaskbarCreated`; also re-add the tray icon there (spike B only re-docks, spike A never
    registers `TaskbarCreated`).
  - Spike B creates `WS_CHILD` with `GetDesktopWindow()` as parent plus `WS_EX_TOOLWINDOW`; create as
    `WS_POPUP`, switch styles after `SetParent`.
  - Minute cadence: a timer that fires a few ms before the boundary computes a < 50 ms delay, adds 60 s and
    repaints the old minute. Aim a small margin *past* the boundary instead.
  - `spike_a_dc.cpp` / `spike_a_host.cpp` return from `WM_PAINT` before `BeginPaint` when device
    resources fail, causing a `WM_PAINT` storm. Always Begin/EndPaint.
  - Handle `WM_DPICHANGED` (sizes and fonts are fixed pixels), `PBT_APMRESUMEAUTOMATIC`, `WM_TIMECHANGE`;
    prefer `GUID_CONSOLE_DISPLAY_STATE` over the legacy `GUID_MONITOR_POWER_ON`. Monitor-off/lock may
    pause the *widget* timer only, never the prayer scheduler/alerts.
  - `PostMessage(hwnd, WM_NULL, 0, 0)` after `TrackPopupMenu` (tray menus).
  - WebView2: check `get_Source()` origin and validate message size/schema, handle `NavigationStarting` /
    `NewWindowRequested`, disable DevTools/default context menu/accelerators in release, add a CSP,
    check `get_CoreWebView2` HRESULT, put the user-data folder under `%LOCALAPPDATA%` (or portable dir),
    and re-measure whether `--disable-features=CalculateNativeWinOcclusion` helps (it disables Chromium's
    occlusion throttling).
- **Open review items** (not done in the 2026-11-12 pass):
  - Phase 1 exit gate: no CI; `native/Makefile` hardcodes `.exe` and the CLI/bench include `<windows.h>`
    (the engine + test_engine are portable C11).
  - Unit 1.7: Hijri still tested on one date and +1 only; needs a two-year published table and ±2.
  - Unit 0.3: Win10 VM, 125/150/200 % DPI, secondary taskbar, auto-hide results never recorded;
    unit 0.4: warm-open latency not measured.
  - `scripts/perf.ps1` reads `WorkingSet64` (total, not private WS) for one process only.
  - Makefile vs code-standards: no `-static`, no `-Wvla`, test/bench binaries stripped with `-s`.
  - `scripts/toolchain.md` lacks the w64devkit archive SHA-256; `THIRD_PARTY_NOTICES.md` does not exist yet.
  - Engine API nits: forbidden-window duration 0 silently means default; `engine_utc_minutes_to_epoch_ms`
    returns -1 (a valid timestamp) for NaN; Umm al-Qura's 120-min Ramadan Isha is not automatic
    (adhan-js does not do it either; belongs to Ramadan mode).
  - CLI uses the default `POLAR_UNRESOLVED`, so |lat| ≳ 65° prints `--:--` for most times.

## Open Questions

1. ~~Host language~~: **Resolved 2026-11-07, C++20 subset** (see ADR 0002).
2. ~~Licence~~: **Resolved 2026-11-07, MIT** (see ADR 0006).
3. **Default Adhan audio**: which recording(s) can be shipped legally (CC0
   or with written permission)? Until resolved, ship a chime only.
4. ~~Linger timeout default~~: **Resolved 2026-11-07, 60 seconds default** (based on 844 ms cold-open measurement, see ADR 0004).
5. **Windows 11 Do Not Disturb**: `SHQueryUserNotificationState` does not
   report it. Is there a supported API? Research in Phase 4; otherwise rely
   on Windows suppressing the notification itself and document that the
   audio still plays.
6. **Microphone-in-use guard** (SRS chat idea): the only known signal is the
   CapabilityAccessManager registry store, which is undocumented. Defer to
   post-1.0 unless a documented API is found.
7. **Zip code signing**: SignPath Foundation (free for OSS, needs an
   application) vs Azure Trusted Signing (paid) vs unsigned zip + Store
   as the recommended path.
8. **App name and Store reservation**: "Azaan Tray". Check that the
   Partner Center name is available early (unit 8.5 depends on it).

## Architecture Decisions

- **JS/TS runtime and package manager: Bun** (user decision, 2026-11-07):
  Use Bun (latest, 1.4.4) instead of Node.js / npm for `ui/` dependencies,
  Vite bundling, typechecking, tests, and fixture generation scripts.
- **Resident native host + ephemeral WebView2 UI** (from SRS): the only way to
  combine a web-tech UI with a TrafficMonitor-class idle footprint.
- **Host in a C++20 subset, engine in C11** (user decision, 2026-11-07): the
  host is built with `-fno-exceptions -fno-rtti`, no iostreams, and RAII
  wrappers for HANDLE/COM. COM code for Direct2D, DirectWrite and WebView2
  stays short, with the same memory footprint as C. The engine stays pure
  C11 for portability and tests. ADR 0002 records this in unit 0.5.
- **Licence: MIT** (user decision, 2026-11-07). Permissive licence to make
  adoption and packaging easy. Every vendored dependency must be
  MIT-compatible (MIT, BSD, zlib, public domain/MIT-0, CC0). GPL code
  must not be copied or vendored. Data licences such as GeoNames (CC BY 4.0)
  are fine with attribution in `THIRD_PARTY_NOTICES.md`. ADR 0006 records
  this in unit 0.5.
- **Single prayer engine in C11**: removes TS/C drift. The UI asks the host for
  times.
- **w64devkit instead of TCC**: TCC can't build the COM-heavy host
  realistically. w64devkit is portable (no system install), with GNU make
  included.
- **WebView2 destroyed after a linger timeout**, not instantly: repeat
  clicks stay fast and memory still returns to zero.
- **Shell balloon notifications in v1** instead of WinRT toasts: no WinRT in
  the resident host.
- **Config in `%LOCALAPPDATA%` + portable marker mode**: MSIX install
  directories are read-only.
- **No network access at all**: offline city DB, no telemetry. This simplifies
  the privacy policy and Store review.

## API Verification Log

Record each documentation source used when introducing an API pattern or
library configuration.

| Date | Topic | Source / command | Finding |
| --- | --- | --- | --- |
| 2026-11-07 | `QUERY_USER_NOTIFICATION_STATE` | https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ne-shellapi-query_user_notification_state | `QUNS_BUSY`(2) = full-screen app or Presentation Settings, not DND. Values 1–7. `QUNS_QUIET_TIME` pairs with `NIIF_RESPECT_QUIET_TIME`. |
| 2026-11-07 | DWM attributes | https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute | Immersive dark mode (20) and corner preference (33) need Win11 build 22000+; `DWMWA_SYSTEMBACKDROP_TYPE` needs 22621+. |
| 2026-11-07 | WebView2 process model | https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-model | Process group = one browser process + renderer(s) + GPU/utility processes, tied to one user-data folder. Use `Controller.Close()` for a clean shutdown. |
| 2026-11-07 | WebView2 distribution | https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution | Evergreen runtime ships with Win11; "vast majority" of Win10 devices have it, but apps must detect it (registry `pv` or `GetAvailableCoreWebView2BrowserVersionString`). Ship `WebView2Loader.dll` or link the static lib. Feature-detect newer interfaces. |
| 2026-11-07 | WebView2 + MinGW | https://github.com/jchv/webview2-in-mingw, webview/webview PR #783 | Works via CINTERFACE / C++; static `WebView2Loader.lib` does not link with MinGW, so use the DLL. |
| 2026-11-07 | Store registration | https://blogs.windows.com/windowsdeveloper/2025/09/10/free-developer-registration-for-individual-developers-on-microsoft-store/ | Free for individual developers (ID verification). Store signs and hosts MSIX. |
| 2026-11-12 | Local time conversion (CLI) | https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex, …/nf-timezoneapi-getdynamictimezoneinformation, …/nf-timezoneapi-filetimetosystemtime | `SystemTimeToTzSpecificLocalTimeEx(const DYNAMIC_TIME_ZONE_INFORMATION*, const SYSTEMTIME* utc, LPSYSTEMTIME local)` applies dynamic DST (Win7+). `GetDynamicTimeZoneInformation` returns `TIME_ZONE_ID_INVALID` on failure. FILETIME input must be < 0x8000000000000000. |
| 2026-11-12 | adhan-js 4.4.3 reference behaviour | `scripts/gen-fixtures/node_modules/adhan/lib/esm/PrayerTimes.js`, `PolarCircleResolution.js`, `DateUtils.js` | Polar resolution runs before Fajr is computed; night = tomorrow sunrise − sunset always; `roundedMinute` uses `getUTCSeconds()` (truncated) and keeps sub-second ms; AqrabBalad tests lat∓0.5 first and stops only after a failure below 65°. |
| 2026-11-09 | `__USE_MINGW_ANSI_STDIO` | `tools/w64devkit/include/_mingw.h` lines 446-466 | When defined as 0, disables libmingwex POSIX stdio wrappers and delegates directly to msvcrt.dll, stripping ~65 KB of dead CRT formatting code. |

## Session Notes

- Dev machine: Windows 11 Pro build 28000. Also test on a Windows 10 22H2 VM.
- Context7 MCP is not configured in this agent environment yet. Recommended:
  `npx ctx7@latest setup` (see `.claude/skills/context7-cli`). Until then use
  `npx ctx7@latest library|docs …` (the `find-docs` skill) or official web docs.
- Git repository initialised in unit 0.1.
