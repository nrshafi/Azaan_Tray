# Azaan Tray

A lightweight Windows taskbar widget and flyout for Islamic prayer times, built with a native resident host (C++ Win32/Direct2D) and an on-demand ephemeral Web UI (WebView2 + React).

## Development Setup

1. **Prerequisites**:
   - [Bun](https://bun.sh) (v1.4+)
   - Microsoft Edge WebView2 Evergreen Runtime (pre-installed on Windows 11)
2. **Toolchain**:
   - Portable `w64devkit` is installed in `tools/w64devkit/`.
   - Run `scripts/env.ps1` to put GCC and Make on your PowerShell session PATH.

## Documentation
See the `/context` directory for comprehensive design specifications:
- `context/project-overview.md` — Product definition and goals
- `context/architecture.md` — System architecture and performance budget
- `context/code-standards.md` — Implementation rules and conventions
- `context/build-plan.md` — Phased build units
- `context/progress-tracker.md` — Current progress
