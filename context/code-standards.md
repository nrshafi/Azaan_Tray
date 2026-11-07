# Code Standards

## General

- Keep modules small and single-purpose; follow the boundaries in
  `architecture.md`.
- Fix root causes; do not layer workarounds (especially around Explorer or
  timing bugs: find the message or event you are missing).
- Every byte resident in the host must justify itself. Before adding a
  dependency or a long-lived allocation to `native/host`, state its memory
  cost in the PR or progress notes.
- No dead code, no speculative abstraction. Add a seam only when a test or
  a second implementation needs it.
- Licence hygiene: every vendored file keeps its licence header, and
  `native/third_party/VERSIONS.md` records name, version, source URL and
  licence. The project is **MIT**: only MIT-compatible dependencies (MIT,
  BSD, zlib, MIT-0/public domain, CC0; CC BY for data with attribution).
  No GPL/LGPL/AGPL code is copied or vendored.

## Documentation and API Verification

Never rely on trained model memory or assumptions for external APIs,
libraries, frameworks, SDKs, or CLI tools. Training data is often outdated
and out of sync with installed package versions.

### Verification Scope

- Mandatory for all third-party dependencies, frameworks, SDKs, external
  APIs, and CLI tools. In this project that explicitly includes: Win32,
  Direct2D/DirectWrite, DWM, Shell (`Shell_NotifyIconW`,
  `SHQueryUserNotificationState`), WebView2 SDK, MSIX/AppxManifest schema,
  miniaudio, the JSON library, React, Vite, Tailwind, shadcn/ui, Radix,
  Vitest, adhan-js, w64devkit/GCC flags, `makeappx`/`signtool`, winget
  manifests, GitHub Actions.
- Core language built-ins (C11 standard library, C++ core language,
  ECMAScript built-ins) do not require documentation lookups.

### Verification Hierarchy

1. **Check installed versions.** Inspect the project's dependency manifest
   (`ui/package.json` + lockfile, `native/third_party/VERSIONS.md`, the
   WebView2 SDK version in `native/third_party/webview2/`, the w64devkit
   version in `scripts/toolchain.md`) to verify the exact installed major and
   minor versions before querying documentation.
2. **Query official documentation, in this priority order:**
   - Priority 1: Project-specific or package-specific CLI tools (e.g.
     `npx shadcn@<pinned> --help`, `npx vite --help`, `gcc --help`,
     `makeappx /?`).
   - Priority 2: A documentation MCP server (Context7) or similar retrieval
     tool (resolve the library/package identifier, then query its docs). Use
     the `find-docs` skill (`npx ctx7@latest library …` then `docs …`). If
     Context7 MCP is not installed, recommend it to the user (`context7-cli`
     skill → `ctx7 setup`).
   - Priority 3: Direct web fetch of official documentation
     (learn.microsoft.com for Win32/WebView2/MSIX, the library's own site or
     repo README for vendored C libraries), or a targeted web search.
3. **Halt when docs are unavailable.** If documentation cannot be retrieved
   through any of the above methods, stop and notify the user. Never write
   code based on unverified assumptions.

### Additional Verification Standards

- **Local type/interface definitions as ground truth**: When documentation
  and local code diverge, inspect installed type definitions or interface
  files (`.d.ts` in `node_modules`, `WebView2.h` from the pinned SDK,
  mingw-w64 headers such as `d2d1.h`/`dwmapi.h`/`shellapi.h` in the
  toolchain) or run the type checker (`tsc --noEmit`, a compile of the
  native target). The installed definitions for the active version are the
  final authority.
- **CLI flag verification**: Never guess CLI flags for any tool. Run
  `<command> --help` or check official tool documentation before executing
  commands with unfamiliar flags.
- **Major version migration discipline**: When modifying features that depend
  on recent major version upgrades of any dependency (e.g. Tailwind config
  model, React major, WebView2 interface versions `ICoreWebView2_N`), read
  the official migration guide before proceeding, to avoid mixing deprecated
  idioms into modern code.
- **Doc citation in progress records**: Record the documentation URL, CLI
  command, or query used when introducing new API patterns or library
  configurations, in `progress-tracker.md` → "API Verification Log".
- **Windows version gating**: Any Win32/DWM/WebView2 API newer than Windows 10
  22H2 or the oldest supported WebView2 runtime must be feature-detected at
  runtime (`GetProcAddress`, `QueryInterface`, checking the `HRESULT`). The
  doc page's "supported starting with build N" line is cited in the code
  comment.

## C (native/engine)

- C11, `-Wall -Wextra -Wpedantic -Werror`, no VLAs, no `malloc` in compute
  paths.
- All angles in degrees at the API surface; radians internally with
  `deg2rad`/`rad2deg` helpers. Name units in identifiers (`lat_deg`,
  `jd`, `minutes_utc`).
- Public API in `engine.h` only; everything else `static`.
- Times returned as minutes-from-UTC-midnight (`double`) or `NAN` when the
  event does not occur (polar day/night before high-latitude adjustment).
- Each formula cites its source (Meeus chapter, PrayTimes.org section, or
  adhan reference) in a comment.

## C++ (native/host)

- Language: C++20, compiled with `-fno-exceptions -fno-rtti`. Use as
  "C with RAII": structs, free functions, small RAII wrappers for HANDLE /
  COM pointers (`ComPtr`-like template written in-repo, ~40 lines).
- No iostreams, no `std::thread`, no `<regex>`, no `std::function` in hot
  paths. `std::array`, `std::string_view`, `<algorithm>` are fine.
  Anything that pulls in a large static chunk of libstdc++ needs a size check.
- Unicode only: `W` APIs, `wchar_t` for Win32 strings, UTF-8 for JSON and
  files. Convert at the boundary with `MultiByteToWideChar` /
  `WideCharToMultiByte`.
- Every `HRESULT` and Win32 `BOOL` return is checked. Failures log (if logging
  is enabled) and degrade gracefully; nothing in the host may crash on a
  missing feature.
- Per-Monitor V2 DPI awareness declared in the app manifest; all sizes come
  from `GetDpiForWindow` scaling, never hard-coded pixels.
- One UI thread. Background threads only where a library requires one
  (miniaudio playback) and they must end when playback ends.
- Static linking of libgcc/libstdc++ for the MinGW build (`-static`), so the
  zip needs no MinGW runtime DLLs.

## TypeScript / React (ui/)

- `strict: true`, `noUncheckedIndexedAccess: true`. No `any`; use `unknown` +
  narrowing at the bridge boundary.
- All bridge payloads are parsed by schema validators generated from or
  written against `shared/protocol/`. No unchecked casts of host messages.
- Function components and hooks only. State: React state + a small context
  for config. No global state library unless a real need appears.
- No network calls (`fetch` to remote URLs is a bug). City data is bundled
  and lazy-loaded.
- User-visible strings go through the i18n helper (`t('key')`), even though
  v1 ships English only. Layouts must survive RTL (`dir="rtl"`).

## Styling

- Use the CSS custom property tokens from `ui-context.md`; no hard-coded hex
  values in components.
- Tailwind utility classes mapped to those tokens; follow the radius scale.
- Respect `prefers-reduced-motion` and `prefers-color-scheme`.

## Bridge (host ⇄ UI)

- Every message type is declared in `shared/protocol/` first, then
  implemented on both sides in the same change.
- Host: validate type allowlist, payload shape, string lengths, numeric
  ranges (lat −90..90, lng −180..180, offsets −60..60, volume 0..100), and
  file paths (must exist, extension `.mp3`/`.wav`) before acting.
- Responses always use the envelope in `architecture.md`; errors carry a
  stable `code` string and a human message.

## Data and Storage

- Only the host reads/writes `config.json`. Writes are atomic.
- Config has `schemaVersion`; every change to its shape ships a migration
  function and a test.
- Unknown keys in config are preserved on save (forward compatibility).

## Testing

- Engine: golden tests against adhan-js fixtures (±1 min) plus unit tests
  for edge cases (polar latitudes, DST days, date-line longitudes, leap years).
- Host: scheduler and alerts take an injected clock and are tested without
  a window. Win32 glue is verified manually against the checklist in
  `build-plan.md`.
- UI: Vitest + Testing Library for components and bridge parsing; Playwright
  smoke test against `npm run dev` with the mock bridge.
- Commands that must pass before a unit is complete:
  `make -C native test`, `npm --prefix ui run typecheck`,
  `npm --prefix ui run test`, `npm --prefix ui run build`.

## File Organization

- `native/engine/` — `engine.h`, `solar.c`, `prayer.c`, `methods.c`,
  `hijri.c`, `makruh.c`.
- `native/host/` — one `.cpp`/`.h` pair per sub-module listed in
  `architecture.md`; `resources/` for `.rc`, manifest, icons.
- `native/third_party/` — vendored deps, each in its own folder with licence.
- `native/tests/` — `test_*.c`, `fixtures/*.json`.
- `ui/src/` — `bridge/`, `views/` (`Today`, `Month`, `settings/*`),
  `components/` (`ui/` for shadcn-generated), `lib/`, `i18n/`, `styles/`.
- `shared/protocol/` — `messages.schema.json`, `README.md`.
- `packaging/msix/`, `packaging/winget/`, `scripts/`, `docs/adr/`.
