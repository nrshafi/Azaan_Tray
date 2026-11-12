# ADR 0008: Optional Network Features (Online Sources, Place Search, Update Check)

## Status
Accepted (2026-11-12)

## Context
v1 was planned with **zero** network access (architecture invariant 7). On
2026-11-12 the user picked these v1 features from the Awqat Salaat comparison
(see `progress-tracker.md`, feature picker):

- AlAdhan API as an alternative prayer-time source, with a monthly cache and
  offline fallback, and a side-by-side comparison with the engine.
- Online place search for locations missing from the bundled city list.
- Update check for the portable zip build (the Store updates itself).

Research findings (sources in the API Verification Log):
- **AlAdhan** (`api.aladhan.com`): free, no key, 23 methods plus custom,
  rate limit about 12 requests/s, responses cacheable for 1 h. Its times are
  calculated with formulas much like the engine's; the value is for users
  whose local timetable matches an AlAdhan method or tuning.
- **Open-Meteo geocoding** (`geocoding-api.open-meteo.com`): free, no key,
  fair use for non-commercial/open-source apps. Preferred over Nominatim,
  whose policy forbids autocomplete and requires 1 request/s per app.
- **GitHub Releases API** for the update check: unauthenticated, 60
  requests/hour per IP.
- Every online request reveals the user's location (or that they run the app).

## Decision
1. **Off by default, opt-in per feature.** The offline engine stays the
   default source. Each network feature has its own setting and a one-line
   disclosure of what is sent and to whom.
2. **The host is the only network client.** The UI never fetches. Its CSP
   keeps `connect-src` at the virtual host only. The UI asks the host over
   the bridge (`location.search`, `source.fetchMonth`, `update.check`), so
   one place enforces the allowlist, timeouts, size limits and validation.
3. **Allowlist, HTTPS only:** `api.aladhan.com`,
   `geocoding-api.open-meteo.com`, `api.github.com`. No other hosts. No
   cookies, no identifiers, generic User-Agent `WaqtTray/<version>`.
4. **Network stack on demand.** WinHTTP is loaded with `LoadLibraryW` only for
   a request and unloaded afterwards, so the idle footprint is unchanged
   (invariant 1). One request at a time, with a 10 s timeout and a
   256 KB response cap.
5. **Monthly cache and fallback.** An online source is fetched once per
   month per location and method, ahead of the month boundary, and stored in
   `%LOCALAPPDATA%\WaqtTray\cache\`. If a fetch fails, or the cached data
   is missing or invalid, the day falls back to the engine. The UI shows
   which source each day came from.
6. **Responses are untrusted input:** parsed with cJSON, validated (dates,
   ordered times, plausible ranges), and rejected wholesale on any error.
7. **No telemetry, ever.** The update check sends nothing beyond the HTTP
   request itself.
8. **CSV import** of an official or mosque timetable (for example the
   Islamic Foundation Bangladesh table) is a local file read, not network,
   and goes through the same validation and fallback as an online source.

## Consequences
- Invariant 7 changes from "no network" to "network only through the
  opt-in features above".
- The Store privacy policy and README must list each network feature, the
  host it contacts, and what it sends (coordinates or the search text).
- AlAdhan and Open-Meteo can change or disappear. The engine fallback means
  the app keeps working, and the source setting reports the failure.
- Rejected: Mawaqit (private API), the official Diyanet API (needs
  per-user credentials), and scraping sites without an API (Qatar Calendar
  House, Islamic Foundation Bangladesh). CSV import covers official
  timetables without scraping.
