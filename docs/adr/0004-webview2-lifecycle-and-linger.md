# ADR 0004: WebView2 Lifecycle and Linger Strategy

## Status
Accepted

## Context
Azaan Tray uses an ephemeral web flyout (React + Tailwind + Vite) rendered inside Microsoft Edge WebView2. We needed to measure:
1. Cold start latency to first interactive paint
2. Memory consumption of the `msedgewebview2.exe` process group while active
3. Process rundown and teardown behavior upon closing

During Phase 0 Spike C, we measured the following on real hardware:
- **Cold start (click to interactive bridge message)**: **844 ms** (Environment init: 16 ms, Webview navigation + JS ready: ~828 ms).
- **Process group memory while active**: 5 processes (`browser`, `renderer`, `gpu-process`, `network-service`, `storage-service`), consuming **135.5 MB** private commit (within the 150 MB ceiling).
- **Clean shutdown**: Calling `ICoreWebView2Controller::Close()` and releasing interfaces results in **0 remaining processes**; all memory is completely released back to the OS.

## Decision
- Destroying the WebView2 controller immediately on focus loss would impose an 844 ms cold-start latency every time the user clicks the widget.
- Therefore, we adopt a **60-second linger strategy** (user-configurable):
  - When the flyout closes / loses focus, the window is hidden (`ShowWindow(SW_HIDE)`), but the controller lingers in memory for 60 seconds.
  - If the user re-opens the flyout within 60 seconds, warm open time is **< 50 ms** (instant).
  - After 60 seconds of inactivity with the UI hidden, `Controller->Close()` is executed and all COM pointers are released. The entire WebView2 process tree terminates, returning the application to its resident ≤ 2 MB footprint.
  - If the user explicitly sets linger timeout to `0`, immediate destruction on close is performed.

## Consequences
- Fast, fluid repeat interactions for the user.
- Zero memory leakage or persistent multi-hundred megabyte footprint when the app is sitting idle.
