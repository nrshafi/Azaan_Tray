# ADR 0002: Toolchain and Host Language Choice

## Status
Accepted

## Context
The project needs a native resident host with minimal memory footprint (≤ 5 MB private working set) and clean Win32 integration, alongside a pure C11 prayer calculation engine. We evaluated TinyCC (TCC), MSVC, and w64devkit (GCC 16 + MinGW-w64).

## Decision
- **Toolchain**: Use portable `w64devkit` (GCC 16.2.0, GNU Make 4.4.1). No global system installation is required; `scripts/env.ps1` sets the session PATH.
- **Prayer Engine**: Pure C11 (`native/engine`), standard math library only, zero OS dependencies, fully portable to Linux for CI verification.
- **Resident Host**: C++20 subset (`native/host`) compiled with `-fno-exceptions -fno-rtti`, no iostreams, no heavyweight STL containers in the hot path. Static linking (`-static`) ensures zero dependency on external MinGW runtime DLLs.

## Consequences
- COM APIs for Direct2D, DirectWrite, and WebView2 are written with clean RAII templates without the overhead of heavy C vtables.
- Binary footprint remains tiny (~66 KB for the host executable).
- The host compiles cleanly under MinGW-w64 and can also be maintained for MSVC contributors.
