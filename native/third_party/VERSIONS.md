# Third-Party Dependencies

## Microsoft.Web.WebView2
- **Version**: 1.0.2903.40
- **Source**: https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.2903.40
- **Files**:
  - `native/third_party/webview2/WebView2.h`
  - `native/third_party/webview2/WebView2EnvironmentOptions.h`
  - `native/third_party/webview2/x64/WebView2Loader.dll`
  - `native/third_party/webview2/arm64/WebView2Loader.dll`
- **Licence**: Microsoft WebView2 SDK License (Permissive binary redistribution with application)
  - `native/third_party/webview2/x64/libWebView2Loader.dll.a` (MinGW import library)
- **SHA-256 of committed binaries** (recorded 2026-11-12 from the files in the repo; re-verify against
  the NuGet package when upgrading):
  - `x64/WebView2Loader.dll`: `462b36fd1be6ca9f7563466a89e57c41ef4a4def3e0a84fa885d203aea4a3aaf`
  - `arm64/WebView2Loader.dll`: `cb561ae4537e39bba5dd8af1f648f189d9562b1003fd110a9e889ace527fb4e8`
  - `x64/libWebView2Loader.dll.a`: `a1f4aac221741de8e7aba1267afa1e392902fabde4722d50b3edad5db5e284dd`

## cJSON
- **Version**: 1.7.19 (`CJSON_VERSION_*` in `cJSON.h`)
- **Source**: https://github.com/DaveGamble/cJSON
- **Files**: `native/third_party/cjson/cJSON.c`, `cJSON.h`, `LICENSE`
- **Licence**: MIT
- **Used by**: engine golden-fixture tests (`native/tests/test_engine.c`); planned for config and bridge
  messages (ADR 0005). The engine itself does not link it.
- **SHA-256**: `cJSON.c` `298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d`,
  `cJSON.h` `25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee`

## adhan-js (dev-only, fixture generation)
- **Version**: 4.4.3 (pinned in `scripts/gen-fixtures/package.json`)
- **Source**: https://github.com/batoulapps/adhan-js
- **Licence**: MIT
- **Use**: reference implementation for `native/tests/fixtures/*.json`; not shipped.
