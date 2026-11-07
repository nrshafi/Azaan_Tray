# ADR 0005: JSON Library for Host and Bridge Protocol

## Status
Accepted

## Context
The native host needs a lightweight C/C++ JSON parser to:
1. Load, validate, and write `config.json` atomically.
2. Parse and generate bridge messages between the host and WebView2.

Requirements:
- MIT or MIT-compatible licence
- Single-header or minimal source footprint (no heavyweight dependencies or dynamic runtime libraries)
- Safe memory handling with strict bounds checking
- Small compiled code size (< 25 KB)

## Decision
Adopt **cJSON** (v1.7.18) as a vendored dependency in `native/third_party/cjson/`.
- Permissive MIT licence.
- Only two files: `cJSON.c` and `cJSON.h`.
- Easily compiled directly into the host binary with `-O2`.
- Robust parsing and formatting with simple validation helpers.

## Consequences
- No external runtime DLL dependencies.
- Binary footprint overhead is negligible (~15 KB).
- Simple API for constructing and parsing bridge JSON payloads.
