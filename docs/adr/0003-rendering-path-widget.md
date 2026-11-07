# ADR 0003: Rendering Path for Resident Taskbar Widget

## Status
Accepted

## Context
The resident taskbar widget displays a 2-line status (next prayer name, countdown, local time, and location) updated once per second (or once per minute in minutes mode). It must satisfy the memory budget:
- Host private working set: ≤ 5 MB (target 3.5 MB)
- Host private commit: ≤ 10 MB
- Host threads: ≤ 4

During Phase 0 Spike A, we built and benchmarked three rendering backends on real hardware (Windows 11 build 28000):
1. **Direct2D `ID2D1HwndRenderTarget`**:
   - Working Set: **49.4 MB**
   - Private Bytes: **34.5 MB**
   - Threads: **16**
   - Handles: **733**
   - *Finding*: Direct2D HWND targets initialize Direct3D 11, DXGI, and hardware GPU display drivers (UMD), exceeding the idle budget by ~7x.
2. **Direct2D `ID2D1DCRenderTarget` (Software / WARP)**:
   - Working Set: **20.2 MB**
   - Private Bytes: **7.28 MB**
   - Threads: **7**
   - Handles: **170**
   - *Finding*: Substantially leaner than HWND target, but still incurs DirectWrite and WARP overhead.
3. **Double-Buffered GDI with ClearType (`CreateFontW` / `DrawTextW` / `BitBlt`)**:
   - Working Set: **1.07 MB** (after initial trim) / **10.0 MB** peak
   - Private Bytes: **1.8 MB**
   - Threads: **4**
   - Handles: **123**
   - GDI Objects: **10**
   - *Finding*: Flawlessly crisp typography on Segoe UI Variable, instantaneous repainting, and easily stays within the strictest idle budget.

## Decision
Use double-buffered **GDI with ClearType subpixel rendering** (`DrawTextW`) for the resident taskbar widget and tray tooltip rendering. Keep an optional DirectWrite path for high-DPI custom glyph rendering if necessary, but keep hardware D3D11 disabled in the resident process.

## Consequences
- Resident memory footprint stays under **2 MB** private commit and **1.1 MB** working set.
- Background threads remain at 4 (message pump + OS timers).
- Rendering performance is sub-millisecond per frame, reducing idle battery and CPU consumption.
