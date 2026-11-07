# ADR 0006: Open Source Licence (MIT)

## Status
Accepted

## Context
Azaan Tray is intended for broad public distribution, community contributions, and packaging via Windows Package Manager (`winget`) and the Microsoft Store. We need a clear, permissive licence that allows free personal and commercial use while keeping dependency compliance unambiguous.

## Decision
License the project under the **MIT Licence**.
- All code authored in this repository is licensed under MIT.
- All vendored third-party libraries must be MIT-compatible (MIT, BSD, zlib, public domain/CC0).
- No GPL, LGPL, or AGPL dependencies may be vendored or statically linked into the application.
- Open datasets (e.g. GeoNames city subset under CC BY 4.0) will be attributed in `THIRD_PARTY_NOTICES.md`.

## Consequences
- Clean legal posture for Microsoft Store submission.
- Straightforward distribution for both portable and installer packages.
