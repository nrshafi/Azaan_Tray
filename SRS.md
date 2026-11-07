# Software Requirements Specification (SRS)

## Project: Windows Lightweight Desktop Prayer & Taskbar Monitor (`AthanTray-Win`)

**Version:** 1.0.0

**Target Platform:** Windows 10 & 11 (x64)

**Architecture:** Lightweight Native Host (C / Win32 / Direct2D) + Ephemeral TypeScript/React WebView2 UI Flyout

---

## 1. Project Overview & Objectives

### 1.1 Purpose

`AthanTray-Win` is a high-performance, open-source Windows desktop application designed to provide accurate offline Islamic prayer time calculations, live taskbar countdowns, and instant audio/visual notifications with a **persistent background RAM footprint under 5 MB**.

### 1.2 Core Architectural Philosophy

To solve the heavy memory overhead seen in traditional desktop frameworks (such as .NET WPF/WinUI 3 or permanent Electron/Tauri shells consuming 130+ MB), this application enforces an **Ephemeral UI Architecture**:

1. **The 24/7 Background Daemon (~3.5 – 4.5 MB RAM):** A native C/Win32 host running via portable compilation that handles offline astronomical math, system tray integration, Win32 taskbar Direct2D rendering, and audio alerts. Zero browser or UI frameworks run continuously in the background.
2. **The On-Demand UI Flyout (~50 – 70 MB RAM *only while open*):** Built with **React + TypeScript + Tailwind CSS**, rendered via Microsoft Edge WebView2. It is spawned dynamically the moment the user clicks the taskbar widget and **completely destroyed** (`DestroyWindow` / process termination) the moment it loses focus or closes—returning background RAM instantly to ~4 MB.

---

## 2. Technical Stack & Dependencies

| Layer | Technology | Purpose |
| --- | --- | --- |
| **Native Background Host** | C (Compiled via portable `TCC` or `w64devkit`) | Manages Win32 window handles, taskbar docking, Direct2D drawing, and timer loops. |
| **UI Frontend** | React, TypeScript, Vite, Tailwind CSS, Lucide Icons | Renders the modern Windows 11 Fluent UI timetable, settings, and city/calculation picker. |
| **UI Embedding** | Microsoft Edge WebView2 Control (`WebView2Loader.dll`) | Renders the TypeScript UI inside an ephemeral frameless window natively supported by Windows 10/11. |
| **Math & Calculations** | Pure TypeScript / C Math Module | Offline solar trigonometry (Karachi, ISNA, MWL, Umm al-Qura angles; Standard & Hanafi Asr). |
| **Packaging & Distribution** | Microsoft MSIX (Desktop Bridge) + GitHub Actions | Free Microsoft Store signing, portable `.zip`, and automated GitHub Releases. |

---

## 3. Functional Requirements

### 3.1 Taskbar & System Tray Integration

* **Taskbar Docking (`Shell_TrayWnd`):**
* The native host attaches a borderless child window (`WS_POPUP`) directly to the Windows taskbar next to `TrayNotifyWnd`.
* Renders a crisp 2-line compact view using Direct2D and DirectWrite (`Segoe UI Variable`):
* **Line 1:** Next prayer name + live countdown (e.g., `Asr  -01:14:22`).
* **Line 2:** Current Waqt end time or Mosque Jamaat time (e.g., `Jmt  04:15 PM`).




* **System Tray Icon (`Shell_NotifyIconW`):**
* Sits silently in the Windows system tray with a tooltip displaying today's full timetable.
* Left-click spawns the React/TypeScript Settings & Timetable flyout.
* Right-click displays a native Win32 context menu (`Open Timetable`, `Mute Audio 1h`, `Settings`, `Exit`).


* **Explorer Restart Survival:**
* Must listen for the registered Windows message `WM_TASKBARCREATED` to instantly re-dock the taskbar widget if `explorer.exe` restarts or display scaling changes.



### 3.2 Offline Astronomical Calculation Engine

* **Calculation Parameters (Configurable via `config.json`):**
* **Fajr / Isha Angles:** Standard conventions (Karachi $18^\circ$, MWL $18^\circ$, ISNA $15^\circ$, Umm al-Qura, etc.) or custom degrees.
* **Asr Juristic Methods:** Standard (Shafi'i/Maliki/Hanbali) vs. Hanafi shadow-length multiplier.
* **High-Latitude Rules:** Angle-based, Night-middle, or Seventh-night adjustments for northern/southern latitudes.


* **Prohibited (Makruh) Prayer Time Alerts:**
* Automatically calculates and flags the **3 forbidden prayer windows**:
1. *Post-Sunrise:* Sunrise to +15 minutes.
2. *Zawal (Solar Noon):* ~10 minutes prior to Dhuhr.
3. *Pre-Sunset (Ghurub):* Last ~15 minutes before Maghrib.


* Turns taskbar text amber/red or displays a `[Makruh]` indicator during these windows.



### 3.3 Smart Notification & Meeting Guard

* **Windows API State Check:**
* Prior to playing any Adhan audio file, call the Win32 shell API `SHQueryUserNotificationState(&state)`.
* If the user is running a fullscreen game (`QUNS_RUNNING_D3D_FULL_screen`), presenting slides (`QUNS_PRESENTATION_MODE`), or in Do Not Disturb (`QUNS_BUSY`), **automatically suppress loud audio** and fallback to a gentle Windows toast notification or soft 2-second beep.


* **Custom Audio Support:**
* Users can select custom `.mp3` or `.wav` files for each individual prayer or disable audio alerts entirely for specific prayers (e.g., Fajr only).



### 3.4 Ephemeral Settings & Timetable UI (React / TypeScript)

* **Design Guidelines:**
* Clean Windows 11 Fluent UI aesthetics with rounded corners, dark/light mode sync, and smooth tab navigation.


* **Views Required:**
1. **Dashboard / Timetable:** Today's complete schedule (Fajr, Sunrise, Dhuhr, Asr, Sunset, Maghrib, Isha, Tahajjud, Midnight) with active countdown progress bars.
2. **Monthly Calendar Grid:** Full month table showing exact prayer timings for any selected month/year.
3. **Settings Panel:**


* Calculation method dropdown & manual angle inputs.
* Coordinate / City selector (GPS lookup or manual latitude/longitude + timezone).
* Audio file manager and volume slider.
* Startup toggle ("Run when Windows starts").



---

## 4. Non-Functional Requirements & Performance Targets

* **Memory Footprint:**
* Background Daemon & Taskbar Host: **$\le$ 5.0 MB RAM** at all times.
* React WebView2 Flyout: **$\le$ 70 MB RAM** when open, dropping to **0 MB** immediately upon closing (`window.destroy()`).


* **Startup Speed:** Cold start of the background daemon in under 50 milliseconds.
* **CPU Usage:** $\le$ 0.1% average CPU utilization during 1-second countdown updates.
* **Portability:** 100% self-contained folder structure storing all configurations in a local `config.json` file. Zero registry pollution outside of user startup keys and zero files written to `C:\Program Files` unless installed via MSIX.

---

## 5. Project Directory Structure

```text
athan-tray-win/
├── include/                  # C Win32 headers & definitions
├── src-native/               # C host daemon, Win32 taskbar HWND, Direct2D renderer, and Win32 interop
├── src-ui/                   # React + TypeScript Vite frontend (Timetable, Settings, Tailwind UI)
├── core-engine/              # Shared TypeScript / C offline astronomical calculation logic
├── assets/                   # Default icons, system tray PNGs, default Adhan audio clips
├── package.json              # Frontend dependencies and build scripts
├── AppxManifest.xml          # Microsoft Store MSIX packaging manifest (runFullTrust capability)
└── .github/
    └── workflows/
        └── release.yml       # Automated GitHub Actions CI/CD for MSIX and portable zip builds

```

---

## 6. References & External Documentation

1. **Microsoft Win32 Desktop App Development:** [Win32 API Documentation](https://www.google.com/search?q=https://learn.microsoft.com/en-us/windows/win32/desktop-app-portal)
2. **Direct2D & DirectWrite Graphics:** [Direct2D Portal](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-portal)
3. **Microsoft Edge WebView2 SDK:** [WebView2 Documentation](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)
4. **Desktop Window Manager (DWM) API:** [DwmSetWindowAttribute Reference](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmsetwindowattribute)
5. **Astronomical Algorithms:** *Astronomical Algorithms* by Jean Meeus (Solar coordinate and prayer angle formulas).
6. **Reference Open-Source Taskbar Tool:** [TrafficMonitor GitHub Repository](https://github.com/zhongyang219/TrafficMonitor)

---