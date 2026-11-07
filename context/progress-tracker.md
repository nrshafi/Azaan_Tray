# Progress Tracker

Update this file after every meaningful implementation change with current date and time

## Current Phase

- Planning complete. Phase 0 (Foundations and Risk Spikes) not started.

## Current Goal

- Answer Open Question 3 (default Adhan audio; it does not block Phase 0),
  then start unit 0.1 (repository and toolchain).

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

## In Progress

- None.

## Next Up

- 0.1 Repository and toolchain → 0.2 / 0.3 / 0.4 spikes → 0.5 ADRs.

## Open Questions

1. ~~Host language~~: **Resolved 2026-11-07, C++20 subset** (see
   Architecture Decisions).
2. ~~Licence~~: **Resolved 2026-11-07, MIT** (see Architecture Decisions).
3. **Default Adhan audio**: which recording(s) can be shipped legally (CC0
   or with written permission)? Until resolved, ship a chime only.
4. **Linger timeout default**: set from the Phase 0 cold-open measurement
   (proposal: 60 s).
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

## Session Notes

- Dev machine: Windows 11 Pro build 28000. Also test on a Windows 10 22H2 VM.
- Context7 MCP is not configured in this agent environment yet. Recommended:
  `npx ctx7@latest setup` (see `.claude/skills/context7-cli`). Until then use
  `npx ctx7@latest library|docs …` (the `find-docs` skill) or official web docs.
- The project is not yet a git repository (unit 0.1 does `git init`).
