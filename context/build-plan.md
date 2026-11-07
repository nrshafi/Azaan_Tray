# Build Plan: Azaan Tray v1.0

This file sets the order of work. Each **unit** is sized for one agent
session and has acceptance criteria. Do units in order unless a unit says it
can run in parallel. Mark progress in `progress-tracker.md`, not here. Edit
this file only when the plan itself changes.

---

## Changes from the SRS

The SRS (`/SRS.md`) is the starting point. These changes were made after
checking it against official documentation (sources are in
`progress-tracker.md` → API Verification Log):

| # | SRS said | Plan does | Why |
| --- | --- | --- | --- |
| 1 | Compile with TCC or w64devkit | **w64devkit (GCC/mingw-w64)** only; the MSVC build kept compiling for contributors | TCC has no workable Direct2D, DirectWrite or WebView2 COM headers, and no C++ support. |
| 2 | Host written in C | **Engine in C11, host in a C++20 subset** (no exceptions/RTTI/iostreams) | Direct2D, DirectWrite and WebView2 are COM. In C, every call is `p->lpVtbl->M(p, …)` and every WebView2 callback needs a hand-built vtable. A C++ subset has the same memory footprint and halves the glue code. **Confirmed by the user on 2026-11-07.** |
| 3 | Shared TypeScript **and** C prayer math | **One engine, in C.** The UI asks the host for times over the bridge | Two implementations drift. The host must compute times with the UI closed anyway. |
| 4 | UI destroyed the instant it loses focus; open in < 100 ms; ≤ 70 MB | **Destroy after a linger timeout** (default 60 s, configurable; 0 = immediate). Budgets: cold open measured in Phase 0 (target ≤ 600 ms), warm ≤ 100 ms, WebView2 group ≤ 100 MB target / 150 MB ceiling | WebView2 always starts a browser, a renderer and a GPU process group, so < 100 ms cold and ≤ 70 MB are unlikely. Lingering keeps repeat clicks instant and still returns to 0 MB. |
| 5 | `WebView2Loader` unspecified | Ship **`WebView2Loader.dll`** beside the exe | Microsoft documents shipping either the static lib or the DLL. The static lib does not link with MinGW. |
| 6 | `QUNS_BUSY` = Do Not Disturb | `QUNS_BUSY` = **full-screen app or Presentation Settings**. Windows 11 Do Not Disturb/Focus is **not** reported by this API (open question) | Microsoft Learn enum docs. |
| 7 | Mica/Acrylic + rounded corners everywhere | Feature-detect: corners and dark frame need Win11 **build 22000+**, system backdrop needs **22621+**. Win10 gets a solid square window | DWMWINDOWATTRIBUTE docs. |
| 8 | `config.json` next to the exe | `%LOCALAPPDATA%\AzaanTray\` by default; **portable mode** when a `portable` marker file sits next to the exe | The MSIX install folder is read-only. |
| 9 | HKCU Run key for startup | HKCU Run for the zip; **`desktop:StartupTask`** for MSIX | The Store build should use the packaged startup mechanism. |
| 10 | Toast notifications | v1 uses **`Shell_NotifyIconW` balloon (`NIF_INFO`)** notifications, which Windows 10/11 render as toasts. WinRT toasts post-1.0 | No WinRT activation or AUMID shortcut setup in the resident host. |
| 11 | GPS lookup | **Offline city search** (bundled GeoNames subset, CC BY 4.0) + manual lat/lng | Avoids WinRT Geolocation and network access. |
| 12 | Default Adhan audio clips | Ship **only licence-cleared audio** (CC0 or explicit permission); otherwise a soft chime + "choose your own Adhan" | Many Adhan recordings online have unclear rights. Open source and the Store need clean licences. |
| 13 | — | **Added**: pre-prayer reminder, Jamaat per prayer, Jumu'ah, Hijri date + adjustment, Ramadan mode, per-prayer offsets, display-mode fallbacks (floating / tray-only), sleep/resume, DST and time-zone handling, single instance, `--print --json` CLI | Gaps against Awqat Salaat and real-world use. |
| 14 | Reference link to a Google redirect | Direct Microsoft Learn links (below) | — |

---

## Phase 0: Foundations and Risk Spikes

Goal: prove the three risky parts on real hardware and record numbers. Spike
code lives in `spikes/` and is deleted (or promoted) at the end of the phase.

**0.1 Repository and toolchain**
- `git init`, `.gitignore`, `.editorconfig`, `.gitattributes` (LF for source,
  CRLF for `.rc`/`.ps1` where needed), `LICENSE` (MIT), `README.md`
  stub, `docs/adr/0001-record-architecture-decisions.md`.
- Install portable w64devkit into `tools/w64devkit/` (git-ignored). Record
  the version and SHA-256 in `scripts/toolchain.md`. Add a `scripts/env.ps1`
  that puts it on PATH for the session only.
- Download the WebView2 SDK NuGet package (pinned version), extract
  `WebView2.h`, `WebView2EnvironmentOptions.h` and the x64/arm64
  `WebView2Loader.dll` into `native/third_party/webview2/`. Record the
  version in `VERSIONS.md`.
- ✅ `gcc --version` works from `scripts/env.ps1`; the tree matches
  `architecture.md` → System Boundaries (empty folders with `.gitkeep`).

**0.2 Spike A: minimal host footprint**
- Win32 window plus tray icon, Direct2D/DirectWrite drawing a two-line
  string updated every second, Per-Monitor V2 manifest, `-static` link.
- Measure with a first version of `scripts/perf.ps1` (private WS, private
  bytes, handles, GDI, threads) after 10 min idle.
- ✅ Numbers recorded. Private WS ≤ 5 MB, or a written finding of which
  DLL/allocation exceeds the budget (compare `ID2D1DCRenderTarget` vs HWND
  target vs GDI-only).

**0.3 Spike B: taskbar embedding**
- Find `Shell_TrayWnd` → `TrayNotifyWnd`, `SetParent` the widget into the
  taskbar, position it left of the tray, re-dock on `TaskbarCreated`.
  Test on Windows 11 (incl. this dev machine's build 28000), Windows 10
  22H2 (VM), taskbar auto-hide, 125/150/200 % DPI, secondary monitor
  (`Shell_SecondaryTrayWnd`), and with TrafficMonitor running at the same
  time (both widgets must not overlap).
- ✅ Widget visible and correctly placed in every case, or each failure is
  listed with the fallback mode that covers it (floating / tray-only).

**0.4 Spike C: WebView2 lifecycle from MinGW**
- Load `WebView2Loader.dll`, create an environment with an explicit
  user-data folder, a controller in a borderless popup, map a virtual host
  to a local folder (`SetVirtualHostNameToFolderMapping`), round-trip one
  JSON message, then `Close()` the controller and release everything.
- Measure: click-to-first-paint cold and warm, private WS sum of the
  `msedgewebview2.exe` group while open, and time until all those processes
  exit after release.
- ✅ Numbers recorded. All WebView2 processes exit after release. A linger
  default is chosen based on the measured cold-open time.

**0.5 Decide and document**
- Write ADRs: 0002 toolchain + language (w64devkit; C++20 subset host, C11
  engine, as already decided), 0003 rendering
  path, 0004 WebView2 lifecycle + linger, 0005 JSON library choice,
  0006 licence (MIT, as already decided).
- ✅ `progress-tracker.md` holds the Phase 0 numbers and decisions. Open
  questions 1–4 are answered or explicitly deferred.

---

## Phase 1: Prayer Engine (`native/engine`, pure C, TDD)

Use the `tdd` skill. Seams: `engine.h` public functions only.

**1.1 Fixture generator**: `scripts/gen-fixtures/` (Bun, dev-only
dependency `adhan`). Outputs `native/tests/fixtures/*.json` for a matrix:
cities Dhaka, Karachi, Makkah, Cairo, Istanbul, London, Oslo (high
latitude), Reykjavik, New York, Sydney, Jakarta, Auckland (date line);
dates covering both solstices, both equinoxes, DST change days, Feb 29;
every method; Standard and Hanafi. ✅ Script reproducible; fixtures
committed; adhan-js version pinned.

**1.2 Solar position + Dhuhr/Sunrise/Sunset** (Julian day, declination,
equation of time, hour angle for altitude, refraction −0.833°).
✅ Golden tests pass for these three times.

**1.3 Fajr/Isha by angle, Isha by interval, method table.** ✅ All methods
pass golden tests at non-polar cities.

**1.4 Asr (Standard/Hanafi), Maghrib offset, per-prayer adjustments,
rounding rule.** ✅ Golden tests pass.

**1.5 High-latitude rules + polar edge cases** (`NAN` handling).
✅ Oslo/Reykjavik summer cases pass; no `NAN` reaches the caller after a
rule is applied.

**1.6 Night times + forbidden windows** (midnight, last third, makruh
windows from settings). ✅ Unit tests with hand-worked examples.

**1.7 Hijri (tabular) + adjustment.** ✅ Tests against a published table for
two years; ±2 day adjustment works.

**1.8 CLI `--print [--date] [--json]`.** Uses engine + Windows TZ conversion.
✅ Output for Dhaka today matches the Islamic Foundation Bangladesh
timetable within ±2 min (Karachi, Hanafi).

Exit: `make -C native test` green on Windows (w64devkit) and in CI on
Linux GCC.

---

## Phase 2: Host Core (`native/host`)

**2.1 App skeleton**: `wWinMain`, single-instance mutex (second launch
signals the first to open the flyout), hidden message window, clean exit,
`.rc` with version info and icon, manifest (PMv2 DPI, `supportedOS`,
UTF-8 code page).

**2.2 Paths + config**: portable detection, `%LOCALAPPDATA%` paths,
`config.json` load/validate/defaults/migrate/atomic save; unknown keys
preserved. ✅ Unit tests for parse, validation, migration and round-trip.

**2.3 Clock + time zone**: UTC→local per date via Windows TZ rules; handle
`WM_TIMECHANGE`, `WM_SETTINGCHANGE` and time-zone change. ✅ Tests with an
injected clock across DST change days.

**2.4 Scheduler**: computes the next event list for today and tomorrow;
one waitable timer; re-plan triggers (config, time, TZ, resume via
`WM_POWERBROADCAST`/`PBT_APMRESUMEAUTOMATIC`, day rollover). ✅ Fake-clock
tests: events in order, rollover at midnight, resume skips stale audio.

**2.5 Tray**: icon (theme-aware), tooltip with the day, context menu (Open,
Mute 1 h, Stop Adhan, Settings, Exit), re-add on `TaskbarCreated`.
✅ Manual checklist items T1–T4 pass.

**2.6 Startup registration**: HKCU Run (zip) / detect MSIX and defer to
StartupTask (`GetCurrentPackageFullName`). ✅ Toggle works in both modes.

---

## Phase 3: Taskbar Widget

**3.1 Renderer**: Direct2D/DirectWrite text layout, cached text formats,
redraw only when the string changes; minutes mode wakes once per minute.

**3.2 Embedding + positioning** (from Spike B): DPI, auto-hide, taskbar
edge (bottom/top on Win10), secondary monitors (setting: primary/all),
`TaskbarCreated` re-dock, a user offset setting.

**3.3 Theme + states**: light/dark taskbar detection, accent, "soon" and
forbidden colours, text templates for countdown modes.

**3.4 Display-mode fallbacks**: floating draggable mini-window (position
saved), tray-only (icon tooltip + optional minutes badge in the icon).
Automatic fallback to floating if embedding fails.

**3.5 Interaction**: hover tooltip, left-click → flyout (stub until Phase 6),
right-click → tray menu.

✅ Phase exit: manual checklist W1–W8 pass on Win11 and Win10; perf budget
holds.

---

## Phase 4: Alerts and Audio

**4.1 Vendor miniaudio** (pinned, licence recorded). Wrapper:
`audio_play(path, volume)`, `audio_stop()`, completion callback posted to
the UI thread. Device and decoder closed after playback. ✅ After playback,
threads and private bytes are back to the idle baseline.

**4.2 Alert policy**: reminder, adhan, jamaat; per-prayer toggles; Fajr
audio; mute-until; missed-alert rule; Meeting Guard via
`SHQueryUserNotificationState` (`QUNS_BUSY`, `QUNS_RUNNING_D3D_FULL_SCREEN`,
`QUNS_PRESENTATION_MODE`, `QUNS_NOT_PRESENT` → silent; `QUNS_QUIET_TIME` →
respect `NIIF_RESPECT_QUIET_TIME`). ✅ Fake-clock + fake-state tests for
every branch.

**4.3 Notifications**: balloon via `NIF_INFO` with app icon, title "Asr",
body "Asr has started. Jamaat at 16:15". Clicking it opens the flyout.

**4.4 Bundled audio**: licence-cleared clips only, plus a soft chime.
`assets/audio/LICENSES.md`.

---

## Phase 5: UI App (`ui/`), can start in parallel after Phase 1

**5.1 Scaffold**: Vite + React + TS (strict) + Tailwind + shadcn/ui at
pinned, doc-verified versions; tokens from `ui-context.md`; i18n helper;
ESLint; Vitest. ✅ `typecheck`, `test`, `build` scripts pass.

**5.2 Protocol + bridge**: `shared/protocol/messages.schema.json`, TS types
and validators, `bridge/host.ts` (WebView2) and `bridge/mock.ts` (browser
dev, with canned data from a fixture). ✅ Unit tests for parsing valid,
invalid and oversized messages.

**5.3 Today view.** **5.4 Month view** (+ print CSS). **5.5 Settings shell
+ Location** (lazy city DB search, manual coordinates, auto-suggest method
by country). **5.6 Calculation + Adjustments.** **5.7 Jamaat + Jumu'ah.**
**5.8 Alerts and Audio** (preview/stop via bridge, file picker via host).
**5.9 Taskbar and Appearance + Startup + About/Licences.**

Each view: ✅ works against the mock bridge, passes an `accessibility`
check, and a Playwright smoke test (`webapp-testing` skill) passes.

---

## Phase 6: Flyout Host Integration

**6.1 Flyout window**: borderless popup, DWM corner, dark mode and
backdrop (feature-detected), anchored to the widget/tray, Esc/deactivate
closes, linger timer, WebView2 transparent background where supported.

**6.2 Settings window**: normal window, Mica, resizable, same WebView2
environment.

**6.3 Bridge dispatch in C++**: allowlist, validation, handlers for every
message in `shared/protocol`. Live config apply → scheduler re-plan →
widget refresh.

**6.4 WebView2 hardening**: virtual host only, block navigation and new
windows, disable devtools, context menus, accelerators and autofill in
Release; CSP meta in `index.html`; feature-detect `ICoreWebView2_N`
interfaces.

**6.5 Missing runtime path**: detect via
`GetAvailableCoreWebView2BrowserVersionString`; show a native message box
with a link to the Microsoft download page; widget, tray and alerts keep
working.

✅ Phase exit: success criteria 3 and 4 in `project-overview.md` pass; no
WebView2 processes remain after linger.

---

## Phase 7: Hardening

- Perf: `scripts/perf.ps1` full budget run on Win10 + Win11; 72-hour soak
  (no growth in private bytes, handles or GDI).
- Robustness: Explorer kill loop, sleep/resume loop, time-zone switch, DST
  (set clock), monitor hot-plug, DPI change, low-disk config write failure,
  corrupted `config.json` (falls back to defaults, keeps a `.bak`).
- Security pass with `security-and-hardening` skill over bridge + file
  handling.
- Accessibility pass (UI + Narrator on widget/tray).
- Crash safety: no unhandled path ends the process silently; optional log.

---

## Phase 8: Packaging and Release

**8.1 CI** (`.github/workflows/ci.yml`): on push/PR: engine tests (Linux +
Windows), host build (w64devkit on `windows-latest`), UI typecheck/test/build,
artifact upload.

**8.2 Portable zip**: `AzaanTray.exe`, `WebView2Loader.dll`, `ui/` dist,
`assets/`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `portable` marker variant.

**8.3 MSIX**: `packaging/msix/AppxManifest.xml` (`runFullTrust`,
`desktop:StartupTask`, `uap:VisualElements`), Store logos, `makeappx pack`
on the CI runner. Local sideload test with a self-signed dev certificate.
Run Windows App Certification Kit.

**8.4 Release workflow** (`release.yml` on tag `v*`): build, attach zip +
msix to the GitHub Release, generate checksums.

**8.5 Store submission**: Partner Center (free individual account), privacy
policy page ("no data collected"), listing text and screenshots, age rating.

**8.6 winget**: manifest PR to `microsoft/winget-pkgs` for the zip (or MSIX).

**8.7 Optional signing for zip**: SignPath Foundation (free for OSS) or
Azure Trusted Signing. Decide in Open Questions.

---

## Post-1.0 Backlog (not scheduled)

WinRT toasts with Stop/Snooze buttons · Bangla/Urdu/Arabic translations (RTL)
· Qibla direction · CSV import of a mosque timetable · Linux port (engine
reused; SNI tray; Waybar JSON via `--print --json`) · ARM64 build ·
Windows 11 Do Not Disturb detection if a supported API exists.

---

## Manual Test Checklist (referenced above)

- **T1** Tray icon appears on start; tooltip shows the day. **T2** Context
  menu items all work. **T3** Icon returns after an Explorer restart. **T4**
  Icon follows the light/dark taskbar.
- **W1** Widget placement on Win11 centred and left taskbar. **W2** Win10
  bottom/top taskbar. **W3** 100/125/150/200 % DPI and a live DPI change.
  **W4** Auto-hide taskbar. **W5** Secondary monitor. **W6** Explorer restart.
  **W7** Coexists with TrafficMonitor. **W8** Fallback to floating when
  embedding is forced to fail.

---

## References

- Win32 desktop apps: https://learn.microsoft.com/en-us/windows/win32/desktop-app-technologies
- `Shell_NotifyIconW`: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw
- `QUERY_USER_NOTIFICATION_STATE`: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ne-shellapi-query_user_notification_state
- `DWMWINDOWATTRIBUTE`: https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
- Direct2D: https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-portal
- DirectWrite: https://learn.microsoft.com/en-us/windows/win32/directwrite/direct-write-portal
- High DPI desktop apps: https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
- WebView2 overview: https://learn.microsoft.com/en-us/microsoft-edge/webview2/
- WebView2 process model: https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-model
- WebView2 distribution (runtime detection, loader DLL): https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution
- WebView2 in MinGW (community demo): https://github.com/jchv/webview2-in-mingw
- MSIX desktop packaging: https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-root
- Free Store registration for individuals (Sept 2025): https://blogs.windows.com/windowsdeveloper/2025/09/10/free-developer-registration-for-individual-developers-on-microsoft-store/
- winget manifests: https://github.com/microsoft/winget-pkgs
- w64devkit: https://github.com/skeeto/w64devkit
- miniaudio: https://miniaud.io/ · https://github.com/mackron/miniaudio
- adhan (reference algorithms, MIT): https://github.com/batoulapps/adhan-js
- PrayTimes.org calculation notes: https://praytimes.org/calculation
- Jean Meeus, *Astronomical Algorithms*, 2nd ed., Willmann-Bell, 1998.
- GeoNames (city data, CC BY 4.0): https://www.geonames.org/
- TrafficMonitor (taskbar embedding reference): https://github.com/zhongyang219/TrafficMonitor
- Awqat Salaat (feature reference): https://github.com/Khiro95/Awqat-Salaat
