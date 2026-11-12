# Architecture Context

## Shape: Resident Native Host + Ephemeral Web UI

```
┌───────────────────────── WaqtTray.exe (always running, ≤5 MB) ────────────────────────┐
│ engine/ (C11, pure)   host/ (C++ subset, Win32)                                        │
│  prayer math  ◄────── scheduler ── alerts ── audio (miniaudio, opened only to play)   │
│  hijri, makruh        tray icon ── taskbar widget (GDI ClearType, ADR 0003)           │
│                       config store (JSON)    platform events (TZ, DPI, power, Explorer)│
│                       flyout manager ──► creates WebView2 only on demand ─────────┐    │
└───────────────────────────────────────────────────────────────────────────────────┼────┘
                                   PostWebMessageAsJson / chrome.webview.postMessage │
┌──────────── msedgewebview2.exe process group (exists only while UI open + linger) ─▼──┐
│ ui/ React + TypeScript + Vite + Tailwind, served from https://app.waqttray.invalid/   │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

## Stack

| Layer | Technology | Role |
| --- | --- | --- |
| Prayer engine | C11, no OS headers, `libm` only | All astronomical and prayer-time math, Hijri, forbidden windows. One implementation, used by host and CLI. |
| Native host | C++ subset (see code-standards) compiled with **w64devkit (GCC, mingw-w64)**; MSVC build kept working for contributors | Win32 message loop, tray, taskbar widget, scheduler, alerts, config, WebView2 hosting. |
| Rendering (widget) | Double-buffered GDI with ClearType (`DrawTextW` + `BitBlt`); no Direct2D/D3D in the resident process (ADR 0003: D2D HWND target 49 MB WS, DC target 20 MB, GDI 1.1 MB) | Crisp two-line text on light/dark taskbars within the ≤ 5 MB budget. |
| Window chrome (flyout) | DWM attributes: corner preference, immersive dark mode, system backdrop | Win11 look. Corner/dark mode need build 22000+, backdrop needs 22621+. Win10 gets square, solid fallback. |
| Audio | miniaudio (single-header C, vendored) | MP3/WAV decode and playback. Device opened only while playing. |
| JSON | Small vendored C JSON library (cJSON or equivalent, chosen in Phase 0) | Config file and bridge messages. |
| UI embedding | Microsoft Edge WebView2 (Evergreen runtime), `WebView2Loader.dll` shipped beside the exe | Renders the React UI. The static loader lib does not link with MinGW. |
| UI | React + TypeScript (strict) + Vite + Tailwind CSS + shadcn/ui (Radix) restyled to Fluent tokens + Lucide icons | Today, Month and Settings views. |
| Network (optional, ADR 0008) | WinHTTP, loaded on demand by the host only | Opt-in AlAdhan source, Open-Meteo place search, GitHub update check. Allowlisted hosts, HTTPS only. |
| Notifications | WinRT toasts with buttons (Stop adhan, Dismiss), Shell balloon fallback | Feasibility from a MinGW, unpackaged host is checked first (build plan unit 4.0). |
| Test oracle (dev only) | adhan-js and ICU `islamic-civil` (Bun script, never shipped) | Generates golden fixtures for the C engine (prayer times, Hijri). |
| Packaging | MSIX (`runFullTrust`, `StartupTask`), portable zip, winget manifest | Built by GitHub Actions on `windows-latest`. |

Exact versions are pinned in manifests (`ui/package.json`,
`native/third_party/VERSIONS.md`). Verify each against official docs per
`code-standards.md` → Documentation and API Verification.

## System Boundaries

- `native/engine/` — Pure functions: `(date, location, settings) → times`.
  No Win32, no I/O, no global state, no allocation in the hot path. Owns all
  prayer and Hijri math. Must compile on Linux GCC too, for CI tests.
- `native/host/` — The resident process. Sub-modules:
  - `app` — `wWinMain`, single-instance mutex, message loop, lifecycle.
  - `scheduler` — computes the next event (reminder, adhan, jamaat, forbidden
    start/end, day rollover) and arms **one** waitable timer for it.
    Re-plans on config change, `WM_TIMECHANGE`, time-zone change and resume.
  - `widget` — taskbar embedding, layout, rendering, display-mode fallbacks.
  - `tray` — `Shell_NotifyIconW`, tooltip, context menu.
  - `notify` — rich toasts with buttons (WinRT), balloon fallback, lock-screen
    status where the platform allows (MSIX only).
  - `alerts` — decides what to do at an event (Meeting Guard, mute, missed
    policy) and calls `audio` / `tray`.
  - `audio` — miniaudio wrapper: play(file, volume), stop. Releases device
    and decoder after playback.
  - `config` — load/validate/migrate/save `config.json`, atomic writes.
    Holds saved locations (profiles) and the active one.
  - `sources` — where a day's times come from: engine (default), cached
    AlAdhan month, or imported CSV timetable. Validates every external day
    and falls back to the engine per day (ADR 0008).
  - `net` — the only network client: WinHTTP loaded on demand, host
    allowlist, timeout and size cap (ADR 0008).
  - `flyout` — WebView2 window lifecycle, bridge dispatch, linger timer.
  - `platform` — DPI, theme (light/dark, accent), taskbar geometry, startup
    registration, paths (portable vs installed vs MSIX).
- `native/cli/` — `--print` code path (same exe, no window created). Also the
  data feed for the PowerToys Command Palette extension (`--print --json`).
- `integrations/cmdpalette/` — optional PowerToys Command Palette extension
  (separate .NET project and package). Reads `WaqtTray.exe --print --json`;
  never links host code.
- `native/tests/` — engine tests against generated fixtures; host logic tests
  for the scheduler, alert decisions, config and source parsing with a fake
  clock (scope: `code-standards.md` → Testing).
- `ui/` — React app. Talks to the host **only** through `ui/src/bridge/`.
  Includes a mock bridge so the UI runs in a normal browser (`bun run dev`).
- `shared/protocol/` — Bridge message schema (JSON Schema + docs). The source
  of truth for both the TS types and the C++ handlers.
- `packaging/` — MSIX manifest and assets, winget manifests, zip layout.
- `scripts/` — build, perf measurement, fixture generation, release helpers.

## Storage Model

- **Config**: one `config.json` (schema-versioned, `"schemaVersion": 1`).
  - Installed (MSIX or zip without marker): `%LOCALAPPDATA%\WaqtTray\config.json`.
    Under MSIX this path is package-redirected automatically.
  - Portable: if a file named `portable` sits next to `WaqtTray.exe`, the
    config lives in `.\data\config.json`.
  - Writes are atomic: write `config.json.tmp`, flush, `MoveFileExW(REPLACE_EXISTING)`.
  - The host owns the file. The UI never touches disk; it sends `config.set`
    over the bridge.
- **WebView2 user data folder**: `%LOCALAPPDATA%\WaqtTray\WebView2` (portable:
  `.\data\WebView2`). Never next to the exe (read-only under MSIX).
- **Audio**: bundled clips in `assets/audio/` (licence-cleared only). User
  files are referenced by absolute path and never copied.
- **City database**: bundled JSON/CSV in the UI bundle only (never loaded by
  the host).
- **Logs**: off by default; when enabled, `%LOCALAPPDATA%\WaqtTray\log.txt`,
  capped at 256 KB with one rollover.
- **Source cache** (ADR 0008): `%LOCALAPPDATA%\WaqtTray\cache\` — one JSON
  file per source, location, method and month. Safe to delete at any time.
- **Imported timetables**: `%LOCALAPPDATA%\WaqtTray\timetables\`, a
  validated copy of each imported CSV (the original file is never modified).

## Auth and Access Model

- No accounts, no telemetry. Network use is limited to the opt-in features in
  ADR 0008 (AlAdhan source, place search, update check), all off by default,
  all made by the host to allowlisted HTTPS hosts. The UI never fetches;
  WebView2 navigation and `connect-src` are restricted to the virtual host.
- Runs as the current user. Never requests elevation.
- Startup: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` for zip
  builds; `desktop:StartupTask` manifest extension for MSIX.

## Bridge Protocol (host ⇄ UI)

- Transport: `ICoreWebView2::PostWebMessageAsJson` (host→UI) and
  `window.chrome.webview.postMessage` (UI→host).
- Envelope: `{ "v": 1, "id": "<uuid|null>", "type": "<ns.verb>", "payload": {…} }`.
  Requests carry an `id`; responses echo it with `type: "<ns.verb>.result"`
  or `"error"`.
- Initial message set: `app.hello`, `times.day`, `times.month`, `config.get`,
  `config.set`, `config.changed` (push), `audio.preview`, `audio.stop`,
  `file.pickAudio` (host shows the native file dialog), `system.info`,
  `window.close`, `window.resize`.
- The host validates **every** inbound message (type allowlist, payload
  schema, size ≤ 64 KB) and ignores messages whose source is not the
  virtual-host origin.

## Performance Budget

| Metric | Budget | How measured |
| --- | --- | --- |
| Host private working set, UI closed, 10 min idle | ≤ 5 MB (target 3.5) | Task Manager → Details → "Memory (active private working set)", or `scripts/perf.ps1` |
| Host private bytes (commit) | ≤ 10 MB | `Get-Process` `PrivateMemorySize64` |
| Host idle CPU | ≤ 0.1 % avg; ≤ 1 wake/s with seconds shown, ≤ 1 wake/min in minutes mode | perf script over 10 min / Windows Performance Analyzer |
| Host threads / GDI objects / handles idle | ≤ 3 / ≤ 30 / ≤ 200 | perf script |
| Cold start to widget visible | ≤ 150 ms | ETW/timestamp log |
| `WaqtTray.exe` size | ≤ 1.5 MB (excl. `WebView2Loader.dll`) | CI artifact |
| UI JS bundle | ≤ 250 KB gzipped (excl. city DB, lazy-loaded) | Vite build report |
| Flyout open, cold (no WebView2 alive) | measured in Phase 0; target ≤ 600 ms | timestamps host click → UI `app.ready` |
| Flyout open, warm (within linger) | ≤ 100 ms | same |
| WebView2 group while open | target ≤ 100 MB, ceiling 150 MB total private WS | perf script sums processes using our user-data folder |
| After close + linger (default 60 s) | 0 WebView2 processes | perf script |

`EmptyWorkingSet` may be called after closing the UI or finishing audio, but
**budgets must also hold in private bytes**, so trimming is not a substitute
for actually freeing memory.

## Invariants

1. The resident host never links or loads WebView2, the UI bundle, the
   audio device, the network stack, or WinRT notifications except while that
   feature is actively in use. After use, all of it is released.
2. Prayer math exists in exactly one place: `native/engine/`. The UI never
   computes prayer times; it asks the host.
3. `native/engine/` has no OS dependencies, no global mutable state, and is
   deterministic for a given input.
4. No polling loops. Time-driven work uses one waitable timer armed for the
   next event, plus a repaint tick only while the widget shows seconds.
5. All times are computed in UTC and converted with the Windows time-zone
   rules for that date (`SystemTimeToTzSpecificLocalTimeEx`), never with a
   cached offset.
6. The UI is untrusted input to the host: every bridge message is validated
   before use.
7. The app collects no data. It makes network requests only for the opt-in
   features in ADR 0008, only from the host, and only to allowlisted hosts.
   Without network, every feature except those still works (engine fallback).
8. A failure in WebView2 (missing runtime, crash) never affects the widget,
   tray, or alerts.
