# Architecture Context

## Shape: Resident Native Host + Ephemeral Web UI

```
┌──────────────────────── AzaanTray.exe (always running, ≤5 MB) ────────────────────────┐
│ engine/ (C11, pure)   host/ (C++ subset, Win32)                                        │
│  prayer math  ◄────── scheduler ── alerts ── audio (miniaudio, opened only to play)   │
│  hijri, makruh        tray icon ── taskbar widget (Direct2D/DirectWrite)              │
│                       config store (JSON)    platform events (TZ, DPI, power, Explorer)│
│                       flyout manager ──► creates WebView2 only on demand ─────────┐    │
└───────────────────────────────────────────────────────────────────────────────────┼────┘
                                   PostWebMessageAsJson / chrome.webview.postMessage │
┌──────────── msedgewebview2.exe process group (exists only while UI open + linger) ─▼──┐
│ ui/ React + TypeScript + Vite + Tailwind, served from https://app.azaantray.invalid/  │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

## Stack

| Layer | Technology | Role |
| --- | --- | --- |
| Prayer engine | C11, no OS headers, `libm` only | All astronomical and prayer-time math, Hijri, forbidden windows. One implementation, used by host and CLI. |
| Native host | C++ subset (see code-standards) compiled with **w64devkit (GCC, mingw-w64)**; MSVC build kept working for contributors | Win32 message loop, tray, taskbar widget, scheduler, alerts, config, WebView2 hosting. |
| Rendering (widget) | Direct2D + DirectWrite (`ID2D1DCRenderTarget` or HWND target), GDI fallback | Crisp two-line text on light/dark taskbars. |
| Window chrome (flyout) | DWM attributes: corner preference, immersive dark mode, system backdrop | Win11 look. Corner/dark mode need build 22000+, backdrop needs 22621+. Win10 gets square, solid fallback. |
| Audio | miniaudio (single-header C, vendored) | MP3/WAV decode and playback. Device opened only while playing. |
| JSON | Small vendored C JSON library (cJSON or equivalent, chosen in Phase 0) | Config file and bridge messages. |
| UI embedding | Microsoft Edge WebView2 (Evergreen runtime), `WebView2Loader.dll` shipped beside the exe | Renders the React UI. The static loader lib does not link with MinGW. |
| UI | React + TypeScript (strict) + Vite + Tailwind CSS + shadcn/ui (Radix) restyled to Fluent tokens + Lucide icons | Today, Month and Settings views. |
| Test oracle (dev only) | adhan-js (Bun script, never shipped) | Generates golden fixtures for the C engine. |
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
  - `tray` — `Shell_NotifyIconW`, tooltip, context menu, balloon notifications.
  - `alerts` — decides what to do at an event (Meeting Guard, mute, missed
    policy) and calls `audio` / `tray`.
  - `audio` — miniaudio wrapper: play(file, volume), stop. Releases device
    and decoder after playback.
  - `config` — load/validate/migrate/save `config.json`, atomic writes.
  - `flyout` — WebView2 window lifecycle, bridge dispatch, linger timer.
  - `platform` — DPI, theme (light/dark, accent), taskbar geometry, startup
    registration, paths (portable vs installed vs MSIX).
- `native/cli/` — `--print` code path (same exe, no window created).
- `native/tests/` — engine unit tests and golden tests; host logic tests for
  scheduler and alerts with a fake clock.
- `ui/` — React app. Talks to the host **only** through `ui/src/bridge/`.
  Includes a mock bridge so the UI runs in a normal browser (`bun run dev`).
- `shared/protocol/` — Bridge message schema (JSON Schema + docs). The source
  of truth for both the TS types and the C++ handlers.
- `packaging/` — MSIX manifest and assets, winget manifests, zip layout.
- `scripts/` — build, perf measurement, fixture generation, release helpers.

## Storage Model

- **Config**: one `config.json` (schema-versioned, `"schemaVersion": 1`).
  - Installed (MSIX or zip without marker): `%LOCALAPPDATA%\AzaanTray\config.json`.
    Under MSIX this path is package-redirected automatically.
  - Portable: if a file named `portable` sits next to `AzaanTray.exe`, the
    config lives in `.\data\config.json`.
  - Writes are atomic: write `config.json.tmp`, flush, `MoveFileExW(REPLACE_EXISTING)`.
  - The host owns the file. The UI never touches disk; it sends `config.set`
    over the bridge.
- **WebView2 user data folder**: `%LOCALAPPDATA%\AzaanTray\WebView2` (portable:
  `.\data\WebView2`). Never next to the exe (read-only under MSIX).
- **Audio**: bundled clips in `assets/audio/` (licence-cleared only). User
  files are referenced by absolute path and never copied.
- **City database**: bundled JSON/CSV in the UI bundle only (never loaded by
  the host).
- **Logs**: off by default; when enabled, `%LOCALAPPDATA%\AzaanTray\log.txt`,
  capped at 256 KB with one rollover.

## Auth and Access Model

- No accounts, no network, no telemetry. The app makes **zero** outbound
  network requests. WebView2 navigation is restricted to the virtual host.
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
| `AzaanTray.exe` size | ≤ 1.5 MB (excl. `WebView2Loader.dll`) | CI artifact |
| UI JS bundle | ≤ 250 KB gzipped (excl. city DB, lazy-loaded) | Vite build report |
| Flyout open, cold (no WebView2 alive) | measured in Phase 0; target ≤ 600 ms | timestamps host click → UI `app.ready` |
| Flyout open, warm (within linger) | ≤ 100 ms | same |
| WebView2 group while open | target ≤ 100 MB, ceiling 150 MB total private WS | perf script sums processes using our user-data folder |
| After close + linger (default 60 s) | 0 WebView2 processes | perf script |

`EmptyWorkingSet` may be called after closing the UI or finishing audio, but
**budgets must also hold in private bytes**, so trimming is not a substitute
for actually freeing memory.

## Invariants

1. The resident host never links or loads WebView2, the UI bundle, or the
   audio device except while that feature is actively in use. After use,
   all of it is released.
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
7. The app makes no network requests and collects no data.
8. A failure in WebView2 (missing runtime, crash) never affects the widget,
   tray, or alerts.
