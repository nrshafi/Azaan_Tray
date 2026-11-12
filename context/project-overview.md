# Waqt-Tray

## Identity

Chosen 2026-11-12. The display name is **Waqt-Tray** (hyphenated). Technical
identifiers drop the hyphen:

| Surface | Value |
| --- | --- |
| Display name (tray tooltip, window titles, CLI banner) | Waqt-Tray |
| Store and GitHub release title | Waqt-Tray — Lightweight Taskbar Prayer Times & Adhan |
| Executable / process | `WaqtTray.exe` |
| Config, logs, WebView2 data | `%LOCALAPPDATA%\WaqtTray\` |
| Virtual host | `https://app.waqttray.invalid/` |
| Single-instance mutex | `Local\WaqtTray` |
| Host window class | `WaqtTray` |

Listing keywords (Store, winget, GitHub) include both **Adhan** and **Azaan**
so South Asian and international search still find it. The repository folder
stays `Azaan_Tray`. The Phase 1 dev CLI binary stays `native/bin/azaan.exe`
until the host exe owns `--print`. Partner Center reservation of the display
name is still unchecked (unit 8.5).

## Overview

Waqt-Tray is a free, open-source Windows 10/11 desktop app that calculates
Islamic prayer times offline, shows a live countdown inside the Windows
taskbar, and plays Adhan or notification alerts. It is for desktop users
whose existing prayer apps are either abandoned (e.g. "My Prayers", built on
retired UWP background APIs) or heavy (e.g. Awqat Salaat at ~130 MB RAM
because it keeps a .NET/WinUI tree resident). Waqt-Tray's 24/7 background
process targets the footprint class of TrafficMonitor (~3.5 MB): a native
Win32 host that never loads a browser engine or XAML until the user opens
the UI, and destroys that UI again after use.

`SRS.md` (repo root) is the original requirements input. Where this folder
and the SRS disagree, **the context files win**. The deltas are listed in
`build-plan.md` → "Changes from the SRS".

## Goals

1. Background host (tray + taskbar widget + scheduler, UI closed) stays at
   **≤ 5 MB private working set** and **≤ 10 MB private bytes** on Windows 11,
   measured as defined in `architecture.md` → Performance Budget.
2. Prayer times match a reference implementation (adhan-js, same method and
   settings) **to the second** for a fixed test matrix of cities, dates
   and methods plus a seeded random sweep, fully offline.
3. The flyout UI opens on click and, after closing, leaves **zero**
   `msedgewebview2.exe` processes belonging to the app once the linger
   timeout ends.
4. Ships through the Microsoft Store (MSIX), `winget`, and a portable `.zip`
   on GitHub Releases, all built by GitHub Actions.

## Core User Flow

1. User installs from the Store, winget, or unzips the portable build and runs
   `WaqtTray.exe`.
2. First run: the host detects no location and opens the Settings UI on the
   Location page. The user searches a city (offline list) or types
   latitude/longitude. A calculation method is pre-selected from the region
   (e.g. Karachi + Hanafi for Bangladesh/Pakistan/India). The user confirms.
3. The taskbar widget appears next to the notification area:
   line 1 `Asr  -01:14:22`, line 2 `Ends 17:12` (or the Jamaat time).
4. N minutes before each prayer, a reminder notification appears (optional).
   At prayer time the Adhan plays, unless Meeting Guard detects a full-screen
   app or presentation, in which case only a quiet notification is shown.
5. Left-clicking the widget or tray icon opens the Today flyout (full
   timetable, Hijri date, forbidden-time status). Clicking away closes it.
6. Right-click shows a native menu: Open timetable, Mute for 1 hour,
   Stop Adhan, Settings, Exit.

## Features

### Prayer Time Engine (offline, native C)

- Fajr, Sunrise, Dhuhr, Asr, Sunset/Maghrib, Isha; plus Midnight (Islamic),
  First and Last third of the night (Tahajjud).
- Methods: Muslim World League, ISNA, Egypt, Umm al-Qura, Karachi, Tehran,
  Gulf, Kuwait, Qatar, Singapore, Turkey (Diyanet), Moonsighting Committee,
  Dubai, plus Custom (Fajr/Isha angle or fixed Isha interval).
- Regional methods (picked 2026-11-12): Gulf (19.5°, Isha 90 min), France
  UOIF (12°/12°), Russia (16°/15°), Malaysia JAKIM (20°/18°), Indonesia
  Kemenag (20°/18°), Tunisia (18°/18°), Algeria (18°/17°), Morocco
  (19°/17°), Portugal (18°, Maghrib +3 min, Isha 77 min), Jordan (18°/18°,
  Maghrib +5 min), Germany (18°/16.5°).
- Polar regions: above 65° a polar rule is applied automatically, so no
  prayer shows `--:--`.
- Asr: Standard (Shafi'i/Maliki/Hanbali) and Hanafi.
- High-latitude rules: Middle of the night, One-seventh of the night,
  Twilight angle.
- Per-prayer minute adjustments.
- Forbidden (makruh) windows with configurable durations: after sunrise
  (default 15 min), zawal before Dhuhr (default 10 min), before sunset
  (default 15 min).
- Hijri date (tabular calculation) with a user ±2-day adjustment.

### Prayer-Time Sources (picked 2026-11-12, ADR 0008)

- Default: the offline engine.
- Optional, opt-in: **AlAdhan API** (free, no key). Fetched once per month
  and cached; any day that can't be fetched or fails validation falls back
  to the engine.
- **Import a timetable from CSV** (a mosque's or authority's published
  times, e.g. Islamic Foundation Bangladesh), for a month or a year.
- **Compare sources**: today's times from the engine and from the chosen
  source side by side, with the difference in minutes.

### Location

- Offline city search and manual latitude/longitude.
- Optional **online place search** (Open-Meteo geocoding, opt-in) for places
  missing from the offline list.
- **Saved locations** (home, office, travel), switchable from the tray menu.

### Taskbar Widget and Tray

- Embedded taskbar widget (two lines, GDI ClearType per ADR 0003), with fallback
  display modes: floating mini-window, or tray icon only.
- Countdown modes: time to next Adhan, time to Jamaat, time left in the
  current prayer window.
- Colour states: normal, "soon" (configurable, default ≤ 10 min), forbidden
  window (amber).
- Tray icon with full-day tooltip and native context menu.
- **Compact mode**: one short line that alternates prayer name and countdown.
- **Drag to a custom position**, with "Auto position" to reset.
- Correct placement on Windows 11 centred taskbars and beside the Widgets
  button; moves aside for other injected widgets (TrafficMonitor, Awqat).
- **Choose the display**: primary, a specific monitor, or all.
- **Widget background**: transparent, solid colour or accent.
- **"Near" indicator** with an intensity setting, and a **"just entered"**
  highlight for N minutes after a prayer starts.
- **Next prayer on the lock screen** (Store/MSIX build only, if Windows
  allows it for this app type; checked in unit 8.9).
- Survives Explorer restarts, DPI changes, monitor changes, sleep/resume,
  time-zone and DST changes.

### Alerts

- Pre-prayer reminder (per prayer, 0–60 min, default off).
- Adhan at prayer time: per-prayer on/off and audio file (`.mp3`/`.wav`),
  with a separate Fajr Adhan, volume, Stop and Mute-for-1-hour actions.
- Meeting Guard: before playing audio, query
  `SHQueryUserNotificationState`. If a full-screen app, D3D exclusive app,
  presentation mode, or locked or screensaver session is detected, show a
  silent notification only.
- Missed-alert policy: after resume from sleep, prayers that started more
  than 5 min ago are not announced with audio.
- **Rich notifications** with Stop adhan / Dismiss buttons; click opens the
  flyout. Balloon notifications remain the fallback.
- **Dismiss the current reminder** from the widget menu or the notification.
- **Reminder sound**: a short chime (not adhan) with an optional repeat until
  dismissed.
- **Forbidden-window alert**: optional quiet notification when the sunrise,
  zawal or pre-sunset window starts.
- **Suhoor alarm** in Ramadan, N minutes before Fajr.
- **Friday reminders**: Jumu'ah reminder and Surah Al-Kahf reminder.

### Iqamah / Jamaat and Jumu'ah

- Per-prayer Jamaat time: fixed clock time or offset from Adhan.
- Friday: Dhuhr is labelled Jumu'ah, with its own Jamaat time.

### Ramadan Mode

- Toggle (or automatic by Hijri month): Suhoor-ends (Fajr) and Iftar
  (Maghrib) countdown labels.

### UI (WebView2 + React, opened on demand)

- Today: full schedule, progress through current prayer window, forbidden
  status, Hijri and Gregorian date.
- Month: table for any month/year, printable.
- Settings: Location, Calculation, Sources, Adjustments, Jamaat, Alerts and
  Audio, Taskbar and Appearance, Startup, About/Licences.
- **First-run setup wizard**: location, suggested method, adhan choice,
  taskbar or tray mode.
- **"How times are calculated"** page: methods, angles, Asr schools,
  high-latitude rules.
- Appearance: theme and accent colour, 12/24-hour override, show/hide
  location, Gregorian date and Sunrise.

### Maintenance and Integrations

- **Update check** for the zip build (opt-in; the Store updates itself).
- **Diagnostic log** (opt-in, local only), for bug reports.
- **PowerToys Command Palette extension** showing the next prayer, fed by
  `WaqtTray.exe --print --json` (separate optional package).

### CLI

- `WaqtTray.exe --print [--date YYYY-MM-DD] [--json]` prints a day's times
  (for debugging and for a future Linux status bar integration).

## Scope

### In Scope (v1.0)

- Windows 10 22H2 and Windows 11 (x64; ARM64 build if CI allows at no
  extra code cost).
- English UI, with an i18n-ready string layer (no hard-coded UI strings).
- Distribution: Microsoft Store (MSIX), winget, portable zip.

### Out of Scope (v1.0)

- Linux and macOS builds (the engine is kept platform-neutral C so this is
  possible later).
- Qibla compass, Quran reader, Duas or Azkar content.
- Online sources other than AlAdhan (MAWAQIT has no public API; Diyanet
  needs per-user credentials; JAKIM, Kemenag and MUIS sources deferred).
  Network use is limited to the opt-in features in ADR 0008.
- GPS/WinRT geolocation auto-detect (offline or online place search plus
  manual coordinates only).
- Translations other than English (structure only).
- Telemetry or analytics of any kind.

## Success Criteria

1. `make -C native test` passes: every fixture case matches the adhan-js
   reference to the second, and Hijri dates match the ICU reference.
2. A perf script reports host private working set ≤ 5 MB after 10 minutes
   idle with the UI closed, on Windows 11 and Windows 10.
3. After the flyout closes and the linger timeout ends, no
   `msedgewebview2.exe` process with the app's user-data folder remains.
4. The widget re-appears within 2 s after `taskkill /f /im explorer.exe` and
   an Explorer restart.
5. An alert fires within ±1 s of the computed time, including across a DST
   change and after resume from sleep (no audio for a stale prayer).
6. The MSIX passes Windows App Certification Kit and is accepted by the
   Store; the portable zip runs with no installer and no admin rights.
