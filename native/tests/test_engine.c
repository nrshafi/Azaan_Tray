/*
 * Engine tests. Policy (context/code-standards.md -> Testing): test behaviour users depend on,
 * against independent reference data, through the public API in engine.h. Expected values come
 * from generated fixtures (scripts/gen-fixtures), not hand-typed numbers. A new test needs a
 * reason the existing ones would miss: a real bug, an untested public behaviour, or a new
 * fast path that must equal the reference path.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "../engine/engine.h"
#include "cJSON.h"

static int g_failures = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { g_failures++; printf("  FAIL (line %d): ", __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static void run(const char* name, void (*fn)(const char*), const char* arg) {
    int before = g_failures;
    fn(arg);
    printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", name);
}

static cJSON* load_json(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char* buf = NULL;
    long n = -1;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n >= 0 && fseek(f, 0, SEEK_SET) == 0 && (buf = malloc((size_t)n + 1)) != NULL) {
        buf[fread(buf, 1, (size_t)n, f)] = '\0';
    }
    fclose(f);
    cJSON* root = buf ? cJSON_Parse(buf) : NULL;
    free(buf);
    return root;
}

static const char* json_str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

/* ---- 1. Prayer times match adhan-js 4.4.3 to the second ---- */

static bool fixture_settings(const cJSON* e, engine_settings_t* s) {
    static const char* METHOD_NAMES[] = {
        "MuslimWorldLeague", "Egyptian", "Karachi", "UmmAlQura", "Dubai", "MoonsightingCommittee",
        "NorthAmerica", "Kuwait", "Qatar", "Singapore", "Tehran", "Turkey"
    };
    int mi = -1;
    for (int i = 0; i < (int)(sizeof(METHOD_NAMES) / sizeof(METHOD_NAMES[0])); i++) {
        if (strcmp(METHOD_NAMES[i], json_str(e, "method")) == 0) mi = i;
    }
    if (mi < 0) return false;
    engine_get_default_settings((calc_method_t)mi, s);
    s->madhab = (strcmp(json_str(e, "madhab"), "Hanafi") == 0) ? MADHAB_HANAFI : MADHAB_STANDARD;

    const char* hl = json_str(e, "high_lat_rule");
    if (strcmp(hl, "MiddleOfTheNight") == 0) s->high_lat_rule = HIGH_LAT_MIDDLE_OF_NIGHT;
    else if (strcmp(hl, "SeventhOfTheNight") == 0) s->high_lat_rule = HIGH_LAT_SEVENTH_OF_NIGHT;
    else if (strcmp(hl, "TwilightAngle") == 0) s->high_lat_rule = HIGH_LAT_TWILIGHT_ANGLE;
    else return false;

    const char* pr = json_str(e, "polar_resolution");
    if (strcmp(pr, "Unresolved") == 0) s->polar_resolution = POLAR_UNRESOLVED;
    else if (strcmp(pr, "AqrabYaum") == 0) s->polar_resolution = POLAR_AQRAB_YAUM;
    else if (strcmp(pr, "AqrabBalad") == 0) s->polar_resolution = POLAR_AQRAB_BALAD;
    else return false;
    return true;
}

/*
 * Every field equals adhan-js to the second (adhan-js keeps stray sub-second milliseconds after
 * rounding), lands on a whole minute, and is NaN exactly where adhan-js has no time.
 */
static void prayer_times_match_adhan(const char* path) {
    static const char* KEYS[] = {
        "fajr_ms", "sunrise_ms", "dhuhr_ms", "asr_ms", "sunset_ms", "maghrib_ms", "isha_ms",
        "midnight_ms", "last_third_ms"
    };
    cJSON* root = load_json(path);
    CHECK(cJSON_IsArray(root) && cJSON_GetArraySize(root) > 0, "cannot read fixtures %s", path);
    if (!cJSON_IsArray(root)) { cJSON_Delete(root); return; }

    int cases = 0, bad = 0;
    const cJSON* e = NULL;
    cJSON_ArrayForEach(e, root) {
        cases++;
        engine_settings_t s;
        int y = 0, mo = 0, d = 0;
        const cJSON* jlat = cJSON_GetObjectItemCaseSensitive(e, "lat");
        const cJSON* jlng = cJSON_GetObjectItemCaseSensitive(e, "lng");
        prayer_times_t t;
        if (!fixture_settings(e, &s) || sscanf(json_str(e, "date"), "%d-%d-%d", &y, &mo, &d) != 3 ||
            !cJSON_IsNumber(jlat) || !cJSON_IsNumber(jlng) ||
            !engine_calculate_times(y, mo, d, jlat->valuedouble, jlng->valuedouble, &s, &t)) {
            CHECK(false, "fixture #%d (%s) could not be evaluated", cases, json_str(e, "city"));
            continue;
        }
        const double got[9] = {
            t.fajr_utc_min, t.sunrise_utc_min, t.dhuhr_utc_min, t.asr_utc_min, t.sunset_utc_min,
            t.maghrib_utc_min, t.isha_utc_min, t.midnight_utc_min, t.last_third_utc_min
        };
        for (int k = 0; k < 9; k++) {
            const cJSON* ex = cJSON_GetObjectItemCaseSensitive(e, KEYS[k]);
            bool ok;
            long long diff_ms = 0;
            if (!cJSON_IsNumber(ex) || isnan(got[k])) {
                ok = !cJSON_IsNumber(ex) && isnan(got[k]);
            } else {
                diff_ms = (long long)engine_utc_minutes_to_epoch_ms(y, mo, d, got[k]) - (long long)ex->valuedouble;
                ok = diff_ms > -1000 && diff_ms < 1000 && got[k] == floor(got[k]);
            }
            if (!ok && bad++ < 20) {
                CHECK(false, "%s %s lat=%.4f lng=%.4f %s %s %s/%s %s: engine %.4f min, diff %+lld ms",
                      json_str(e, "city"), json_str(e, "date"), jlat->valuedouble, jlng->valuedouble,
                      json_str(e, "method"), json_str(e, "madhab"), json_str(e, "high_lat_rule"),
                      json_str(e, "polar_resolution"), KEYS[k], got[k], diff_ms);
            } else if (!ok) {
                g_failures++;
            }
        }
    }
    printf("  %s: %d cases, %d mismatched fields\n", path, cases, bad);
    cJSON_Delete(root);
}

/* ---- 2. Hijri dates match ICU islamic-civil; the adjustment shifts by whole days ---- */

static void hijri_matches_reference(const char* path) {
    cJSON* root = load_json(path);
    int n = cJSON_IsArray(root) ? cJSON_GetArraySize(root) : 0;
    CHECK(n > 0, "cannot read Hijri fixtures %s", path);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        const cJSON* row = cJSON_GetArrayItem(root, i);
        int v[6];
        for (int j = 0; j < 6; j++) v[j] = cJSON_GetArrayItem(row, j)->valueint;
        for (int adj = -2; adj <= 2; adj++) {
            if (i + adj < 0 || i + adj >= n) continue;
            const cJSON* want = cJSON_GetArrayItem(root, i + adj);
            hijri_date_t h;
            engine_get_hijri_date(v[0], v[1], v[2], adj, &h);
            if (h.year != cJSON_GetArrayItem(want, 3)->valueint || h.month != cJSON_GetArrayItem(want, 4)->valueint ||
                h.day != cJSON_GetArrayItem(want, 5)->valueint) {
                if (bad++ < 10) {
                    CHECK(false, "%04d-%02d-%02d adj %+d: engine %d/%d/%d", v[0], v[1], v[2], adj, h.year, h.month, h.day);
                } else {
                    g_failures++;
                }
            }
        }
    }
    printf("  %s: %d days x 5 adjustments, %d mismatches\n", path, n, bad);
    cJSON_Delete(root);
}

/* ---- 3. The monthly sliding-window fast path equals the per-day path ---- */

static bool same_time(double a, double b) { return (isnan(a) && isnan(b)) || a == b; }

static void month_equals_days(const char* unused) {
    (void)unused;
    struct { const char* what; calc_method_t m; madhab_t mh; high_lat_rule_t hl; polar_resolution_t pr;
             int y, mo; double lat, lng; } cases[] = {
        { "leap February, Dhaka", CALC_METHOD_KARACHI, MADHAB_HANAFI, HIGH_LAT_MIDDLE_OF_NIGHT, POLAR_UNRESOLVED, 2024, 2, 23.8103, 90.4125 },
        { "polar summer, Tromso", CALC_METHOD_MWL, MADHAB_STANDARD, HIGH_LAT_SEVENTH_OF_NIGHT, POLAR_AQRAB_BALAD, 2024, 6, 69.6492, 18.9553 },
        { "polar night, AqrabYaum", CALC_METHOD_MOONSIGHTING, MADHAB_STANDARD, HIGH_LAT_TWILIGHT_ANGLE, POLAR_AQRAB_YAUM, 2024, 12, 70.0, 25.0 },
        { "safe-Fajr bound, south", CALC_METHOD_MWL, MADHAB_STANDARD, HIGH_LAT_MIDDLE_OF_NIGHT, POLAR_UNRESOLVED, 2027, 1, -52.0, 50.8 },
        { "date line, year rollover", CALC_METHOD_NORTH_AMERICA, MADHAB_STANDARD, HIGH_LAT_MIDDLE_OF_NIGHT, POLAR_UNRESOLVED, 2027, 12, 0.0, -180.0 },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        engine_settings_t s;
        engine_get_default_settings(cases[c].m, &s);
        s.madhab = cases[c].mh;
        s.high_lat_rule = cases[c].hl;
        s.polar_resolution = cases[c].pr;
        prayer_times_t month[31];
        int days = 0;
        if (!engine_calculate_month(cases[c].y, cases[c].mo, cases[c].lat, cases[c].lng, &s, month, &days)) {
            CHECK(false, "%s: engine_calculate_month failed", cases[c].what);
            continue;
        }
        for (int d = 1; d <= days; d++) {
            prayer_times_t one;
            engine_calculate_times(cases[c].y, cases[c].mo, d, cases[c].lat, cases[c].lng, &s, &one);
            const double* a = (const double*)&month[d - 1];
            const double* b = (const double*)&one;
            size_t k = 0;
            while (k < sizeof(prayer_times_t) / sizeof(double) && same_time(a[k], b[k])) k++;
            if (k < sizeof(prayer_times_t) / sizeof(double)) {
                CHECK(false, "%s: day %d field %d: month %.4f vs day %.4f", cases[c].what, d, (int)k, a[k], b[k]);
                break;
            }
        }
    }
}

/* ---- 4. Forbidden windows follow the configured durations ---- */

static void forbidden_windows_follow_settings(const char* unused) {
    (void)unused;
    engine_settings_t s;
    engine_get_default_settings(CALC_METHOD_KARACHI, &s);
    s.sunrise_forbidden_duration_min = 20;
    s.zawal_forbidden_duration_min = 5;
    s.sunset_forbidden_duration_min = 12;
    prayer_times_t t;
    CHECK(engine_calculate_times(2024, 3, 20, 23.8103, 90.4125, &s, &t), "calculation failed");
    CHECK(t.sunrise_forbidden_start_utc_min == t.sunrise_utc_min &&
          t.sunrise_forbidden_end_utc_min == t.sunrise_utc_min + 20, "sunrise window");
    CHECK(t.zawal_forbidden_start_utc_min == t.dhuhr_utc_min - 5 &&
          t.zawal_forbidden_end_utc_min == t.dhuhr_utc_min, "zawal window");
    CHECK(t.sunset_forbidden_start_utc_min == t.sunset_utc_min - 12 &&
          t.sunset_forbidden_end_utc_min == t.sunset_utc_min, "sunset window");
}

/* ---- 5. Bad input from config or callers is rejected, not computed ---- */

static void invalid_input_rejected(const char* unused) {
    (void)unused;
    engine_settings_t s;
    engine_get_default_settings(CALC_METHOD_MWL, &s);
    prayer_times_t t, month[31];
    CHECK(!engine_calculate_times(2023, 2, 29, 0.0, 0.0, &s, &t), "non-leap Feb 29 accepted");
    CHECK(!engine_calculate_times(2024, 13, 1, 0.0, 0.0, &s, &t), "month 13 accepted");
    CHECK(!engine_calculate_times(2024, 4, 1, NAN, 0.0, &s, &t), "NaN latitude accepted");
    s.method = CALC_METHOD_COUNT;
    CHECK(!engine_calculate_times(2024, 4, 1, 0.0, 0.0, &s, &t), "unknown method accepted");
    CHECK(!engine_calculate_month(2024, 4, 0.0, 0.0, &s, month, NULL), "unknown method accepted (month)");
}

int main(int argc, char** argv) {
    /* Fixture directory defaults to the one used by `make -C native test` */
    const char* dir = (argc > 1) ? argv[1] : "tests/fixtures";
    char matrix[512], sweep[512], hijri[512];
    snprintf(matrix, sizeof(matrix), "%s/prayer_fixtures.json", dir);
    snprintf(sweep, sizeof(sweep), "%s/prayer_fixtures_random.json", dir);
    snprintf(hijri, sizeof(hijri), "%s/hijri_fixtures.json", dir);

    run("prayer times match adhan-js (city matrix)", prayer_times_match_adhan, matrix);
    run("prayer times match adhan-js (sweep + regressions)", prayer_times_match_adhan, sweep);
    run("Hijri dates match ICU islamic-civil, with +/-2 adjustment", hijri_matches_reference, hijri);
    run("monthly fast path equals per-day path", month_equals_days, NULL);
    run("forbidden windows follow settings", forbidden_windows_follow_settings, NULL);
    run("invalid input rejected", invalid_input_rejected, NULL);

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("\nAll engine tests passed\n");
    return 0;
}
