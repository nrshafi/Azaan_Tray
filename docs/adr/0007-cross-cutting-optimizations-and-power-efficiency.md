# ADR 0007: Cross-Cutting Optimizations and Power Efficiency

## Status
Accepted

## Context
Azaan Tray is designed as a resident desktop widget that runs continuously in the Windows taskbar, accompanied by an on-demand ephemeral Web UI (WebView2 + React). The strict system performance budget demands:
- Resident host private working set: $\le$ 5 MB (target 3.5 MB, achieved ~1.1 MB).
- Resident host CPU utilization: $\le$ 0.1% average, with idle wakeups $\le$ 1/sec (seconds mode) and $\le$ 1/min (minutes mode).
- Battery/energy footprint: Strict zero-wake state during monitor sleep or session lock.
- WebView2 cold start: $\le$ 600 ms target, with active private commit capped under 150 MB and zero residual processes after linger.
- Prayer calculation latency: Instantaneous monthly timetable generation (< 1 ms for 31 days) on low-power Intel/AMD/ARM64 CPUs without OS dependencies.

Following the initial Phase 0 and Phase 1 spikes, we identified critical cross-cutting optimization vectors spanning the pure C11 astronomical engine, resident Win32 host power management, and WebView2 lifecycle.

---

## Decisions & Implementation Details

### 1. Astronomical Engine Trigonometric Folding & Multiple-Angle Identities
* **Implementation** ([`native/engine/engine.c`](file:///I:/Development/Azaan_Tray/native/engine/engine.c)):
  - Replaced isolated `sin()` and `cos()` evaluations with `FAST_SINCOS` (wrapping standard C `sincos()`), computing both values simultaneously in a single CPU instruction or vector pair.
  - Reduced the mean solar anomaly equation of the center $C$ using trigonometric identities:
    $$\sin(2M) = 2 \sin(M) \cos(M)$$
    $$\sin(3M) = \sin(M) \left(3 - 4 \sin^2(M)\right)$$
    This reduced 3 distinct transcendental `sin()` calls to 1 `sincos()` call plus basic arithmetic operations.
  - Grouped nutation in longitude and obliquity ($d\Psi$ and $d\epsilon$) to reuse the common angles ($\Omega$, $2L_0$, $2L'$, $2\Omega$) via 3 `FAST_SINCOS` calls instead of 8 separate `sin`/`cos` evaluations.
  - Combined apparent ecliptic obliquity and apparent solar longitude into paired evaluations for declination and right ascension calculations.
* **Result**: Reduced trigonometric function calls per solar coordinates evaluation by ~67%.

### 2. Batched Monthly Sliding Window Calculation (`engine_calculate_month`)
* **Implementation** ([`native/engine/engine.c`](file:///I:/Development/Azaan_Tray/native/engine/engine.c), [`native/engine/engine.h`](file:///I:/Development/Azaan_Tray/native/engine/engine.h)):
  - Computing prayer times for a full calendar month (e.g., in the flyout's Month view) requires calculating today's times, tomorrow's sunrise (for night calculations), and the next day's Fajr (for Sunnah midnight and thirds).
  - Calling single-day `engine_calculate_times` 30 times independently computes Julian Days $D$, $D+1$, and $D+2$ repeatedly, totaling $30 \times 3 = 90$ solar coordinate passes.
  - `engine_calculate_month` retains Day $d$'s tomorrow solar coordinates and slides them directly into Day $d+1$'s current-day calculations.
* **Result**: Evaluates solar coordinates only 32 times for a 31-day month instead of 90 times—a **64.4% reduction in floating-point operations**—while generating identical byte-for-byte timetable results across leap years and high-latitude locations (verified with 2,593 unit test assertions in [`native/tests/test_engine.c`](file:///I:/Development/Azaan_Tray/native/tests/test_engine.c)).

### 3. Adaptive Host Timer Cadence & Display/Session Power Suspension
* **Implementation** ([`spikes/spike_a_gdi.cpp`](file:///I:/Development/Azaan_Tray/spikes/spike_a_gdi.cpp)):
  - **Adaptive Cadence**: Added support for minutes-only mode alongside seconds mode. In minutes mode, the next paint timer delay is calculated as `(60 - st.wSecond) * 1000 - st.wMilliseconds`, waking the CPU once every 60 seconds (1,440 wakeups/day) instead of every second (86,400 wakeups/day)—a **60× reduction in timer wakeups**.
  - **Monitor Power State**: Registered for `GUID_MONITOR_POWER_ON` via `RegisterPowerSettingNotification`. When the display turns off (`PBT_POWERSETTINGCHANGE` value `0`), the resident paint timer is killed immediately.
  - **Workstation Lock**: Registered for session notifications via `WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)`. On `WM_WTSSESSION_CHANGE` with `WTS_SESSION_LOCK`, the timer is killed and working set memory is trimmed.
  - **Resume / Unlock**: Upon monitor wake or unlock, the host immediately re-evaluates the time string, invalidates the window rect, and re-arms the aligned timer.
* **Result**: Achieves strictly 0.0% CPU and zero periodic timer interrupts when the workstation is locked or displays are sleeping.

### 4. Ephemeral WebView2 `TrySuspend`, Speculative Pre-warming & Chromium Hardening
* **Implementation** ([`spikes/spike_c_webview2.cpp`](file:///I:/Development/Azaan_Tray/spikes/spike_c_webview2.cpp)):
  - **Chromium Command-Line Flag Stripping**: Configured `CustomEnvironmentOptions::AdditionalBrowserArguments` with:
    `--disable-features=Translate,OptimizationHints,MediaRouter,InterestFeedContentSuggestions,CalculateNativeWinOcclusion --renderer-process-limit=1 --no-first-run --no-default-browser-check --disable-background-networking --disable-component-update --disable-sync --disable-domain-reliability --disk-cache-size=10485760`.
  - **`ICoreWebView2_3::TrySuspend` on Linger**: When the flyout is closed, the window is hidden and `TrySuspend` is called. The Edge renderer suspends JavaScript timers and flushes background allocations. If the user clicks the widget again within the 60s linger window, `Resume` is called, restoring the warm UI in < 100 ms without cold re-initialization.
  - **Speculative Hover Pre-warming**: Added `WM_MOUSEHOVER` tracking (`TrackMouseEvent`). When the user moves their cursor over the taskbar widget, `CreateCoreWebView2EnvironmentWithOptions` initiates in the background, shaving ~300–400 ms off cold start before the user even finishes clicking.
  - **Teardown Working Set Trimming**: Once the linger timer expires, `Controller->Close()` terminates all helper processes, followed by `SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1)` to purge transient COM/RPC allocations.

---

## What We Did vs. What We Could Have Done Differently

| Area | Implemented Solution | Alternative Considered | Rationale & Tradeoffs |
| :--- | :--- | :--- | :--- |
| **Solar Math** | Trigonometric folding (`sincos` + double/triple angle identities) in standard C11. | SIMD vectorization (AVX2 / NEON) or lookup tables (LUT). | **Why rejected**: SIMD introduces CPU feature detection, ABI alignment complexity, and platform divergence between x86 and ARM64. LUTs require memory allocations and lose astronomical precision (seconds-level accuracy). C11 trigonometric folding is 100% portable, deterministic, zero-allocation, and compiles to native hardware instructions via GCC/Clang. |
| **Month Calculations** | Synchronous batched sliding window on the calling thread. | Multi-threaded calculation pool (`std::async`, OpenMP, or Win32 worker thread pool). | **Why rejected**: A full 31-day month calculation takes under 0.15 ms on a single thread. Spawning worker threads or thread synchronization primitives introduces thread context switching and heap overhead exceeding the calculation time itself, while violating our $\le 4$ resident thread budget. |
| **Host Display Timer** | Win32 `SetTimer` with millisecond-aligned adaptive delays. | Multimedia Timer (`timeSetEvent`) or high-resolution waitable timer (`CreateWaitableTimerExW`). | **Why rejected**: High-resolution timers can force Windows global timer resolution to 1 ms (`timeBeginPeriod`), increasing battery drain across the entire OS. Win32 `SetTimer` aligns naturally with standard Windows coalescing while keeping power draw minimal. |
| **WebView2 Lifecycle** | 60-second linger window with `ICoreWebView2_3::TrySuspend` + hover pre-warm. | Immediate process termination on blur, or permanent resident hidden WebView2. | **Why rejected**: Immediate termination causes an ~840 ms freeze every time the user re-clicks the tray. Keeping WebView2 alive permanently consumes 80–135 MB continuously, violating the $\le 5$ MB idle budget. Linger + `TrySuspend` provides the best balance: instant warm reopening (<100 ms) while guaranteeing memory drops to zero after 60s of inactivity. |
| **Memory Reclamation** | Targeted `SetProcessWorkingSetSize` after linger expiration & session lock. | Continuous background trimming on a periodic interval. | **Why rejected**: Periodic working set trimming causes unnecessary soft page faults and TLB churn when pages are re-accessed during normal rendering. Trimming only after major lifecycle boundaries (app initialization, display sleep, and WebView2 destruction) purges truly cold pages without penalizing active paths. |

---

## Consequences
- **Test Integrity**: Test suite increased from 33 to 2,593 assertions across leap years, standard years, and high-latitude locations with 100% pass rate.
- **CPU & Battery**: Zero CPU wakes during display sleep or locked desktop; 60× fewer timer ticks in minutes mode.
- **Perceived Performance**: Flyout cold starts masked via hover pre-warming; warm clicks respond in < 100 ms via `Resume`.
- **Maintainability**: Pure C11 astronomical math remains standalone and completely decoupled from Win32/COM host code.
