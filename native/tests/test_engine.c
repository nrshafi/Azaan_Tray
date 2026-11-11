#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "../engine/engine.h"
#include "cJSON.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define ASSERT_TRUE(expr) do { \
    g_tests_run++; \
    if (!(expr)) { \
        printf("FAIL: %s (line %d)\n", #expr, __LINE__); \
    } else { \
        g_tests_passed++; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, max_diff) do { \
    g_tests_run++; \
    double diff = fabs((a) - (b)); \
    if (diff > (max_diff)) { \
        printf("FAIL: |%s - %s| = |%f - %f| = %f > %f (line %d)\n", \
               #a, #b, (double)(a), (double)(b), diff, (double)(max_diff), __LINE__); \
    } else { \
        g_tests_passed++; \
    } \
} while(0)

static void test_julian_day() {
    /* 2000 Jan 1 00:00:00 UTC = 2451544.5 (J2000.0 is 12:00 UTC = 2451545.0) */
    double jd = engine_julian_day(2000, 1, 1);
    ASSERT_NEAR(jd, 2451544.5, 0.001);

    /* 1999 Dec 31 00:00:00 UTC = 2451543.5 */
    jd = engine_julian_day(1999, 12, 31);
    ASSERT_NEAR(jd, 2451543.5, 0.001);

    /* Unix epoch 1970-01-01 00:00:00 UTC = 2440587.5 */
    jd = engine_julian_day(1970, 1, 1);
    ASSERT_NEAR(jd, 2440587.5, 0.001);

    /* Leap year 2024-02-29 00:00:00 UTC = 2460369.5 */
    jd = engine_julian_day(2024, 2, 29);
    ASSERT_NEAR(jd, 2460369.5, 0.001);

    /* Reference date 2026-11-09 00:00:00 UTC = 2461353.5 */
    jd = engine_julian_day(2026, 11, 9);
    ASSERT_NEAR(jd, 2461353.5, 0.001);
}

static void test_solar_coordinates() {
    /* At J2000.0 (near winter solstice): declination is ~ -23.0 deg */
    double decl = 0.0, eot = 0.0;
    engine_solar_coordinates(2451545.0, &decl, &eot);
    ASSERT_TRUE(decl < -22.5 && decl > -23.5);
    ASSERT_TRUE(eot < 0.0 && eot > -15.0);

    /* Summer solstice ~ June 21: declination is near +23.44 deg */
    double jd_summer = engine_julian_day(2024, 6, 21);
    engine_solar_coordinates(jd_summer, &decl, &eot);
    ASSERT_NEAR(decl, 23.44, 0.5);
}

static void test_hijri_dates() {
    hijri_date_t h;
    engine_get_hijri_date(2024, 3, 12, 0, &h);
    ASSERT_TRUE(h.year == 1445);
    ASSERT_TRUE(h.month == 9); /* Ramadan */
    ASSERT_TRUE(h.day >= 1 && h.day <= 2);
    ASSERT_TRUE(strcmp(h.month_name_en, "Ramadan") == 0);

    /* Test adjustment: +1 day */
    hijri_date_t h_adj;
    engine_get_hijri_date(2024, 3, 12, 1, &h_adj);
    ASSERT_TRUE(h_adj.day == h.day + 1 || (h.day >= 29 && h_adj.day == 1));
}

static void test_dhaka_prayer_times() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_KARACHI, &settings);
    settings.madhab = MADHAB_HANAFI;

    prayer_times_t times;
    /* Dhaka: 23.8103 N, 90.4125 E on 2024-03-20 */
    bool ok = engine_calculate_times(2024, 3, 20, 23.8103, 90.4125, &settings, &times);
    ASSERT_TRUE(ok);

    /* UTC to Dhaka local (+6h = +360 min) */
    double fajr_local = fmod(times.fajr_utc_min + 360.0, 1440.0);
    double sunrise_local = fmod(times.sunrise_utc_min + 360.0, 1440.0);
    double dhuhr_local = fmod(times.dhuhr_utc_min + 360.0, 1440.0);
    double asr_local = fmod(times.asr_utc_min + 360.0, 1440.0);
    double sunset_local = fmod(times.sunset_utc_min + 360.0, 1440.0);
    double isha_local = fmod(times.isha_utc_min + 360.0, 1440.0);

    /* In Dhaka local time:
       Fajr is ~04:47 AM (287 min)
       Sunrise is ~06:02 AM (362 min)
       Dhuhr is ~12:07 PM (727 min)
       Asr is ~16:28 PM (988 min)
       Sunset is ~18:10 PM (1090 min)
       Isha is ~19:25 PM (1165 min)
    */
    ASSERT_NEAR(fajr_local, 287.0, 2.0);
    ASSERT_NEAR(sunrise_local, 362.0, 2.0);
    ASSERT_NEAR(dhuhr_local, 727.0, 2.0);
    ASSERT_NEAR(asr_local, 988.0, 2.0);
    ASSERT_NEAR(sunset_local, 1090.0, 2.0);
    ASSERT_NEAR(isha_local, 1165.0, 2.0);

    /* Verify local progression */
    ASSERT_TRUE(fajr_local < sunrise_local);
    ASSERT_TRUE(sunrise_local < dhuhr_local);
    ASSERT_TRUE(dhuhr_local < asr_local);
    ASSERT_TRUE(asr_local < sunset_local);
    ASSERT_TRUE(sunset_local < isha_local);

    /* Verify forbidden windows */
    ASSERT_NEAR(times.sunrise_forbidden_end_utc_min - times.sunrise_utc_min, 15.0, 0.01);
    ASSERT_NEAR(times.dhuhr_utc_min - times.zawal_forbidden_start_utc_min, 10.0, 0.01);
    ASSERT_NEAR(times.sunset_utc_min - times.sunset_forbidden_start_utc_min, 15.0, 0.01);
}

static void test_high_latitude_oslo() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_MWL, &settings);
    settings.high_lat_rule = HIGH_LAT_SEVENTH_OF_NIGHT;

    prayer_times_t times;
    /* Oslo (59.91 N) on Summer Solstice */
    bool ok = engine_calculate_times(2024, 6, 21, 59.9139, 10.7522, &settings, &times);
    ASSERT_TRUE(ok);

    /* With 1/7th rule, fajr and isha must not be NAN */
    ASSERT_TRUE(!isnan(times.fajr_utc_min));
    ASSERT_TRUE(!isnan(times.sunrise_utc_min));
    ASSERT_TRUE(!isnan(times.dhuhr_utc_min));
    ASSERT_TRUE(!isnan(times.asr_utc_min));
    ASSERT_TRUE(!isnan(times.sunset_utc_min));
    ASSERT_TRUE(!isnan(times.isha_utc_min));
}

static void assert_time_equal(double a, double b) {
    if (isnan(a) && isnan(b)) {
        g_tests_run++;
        g_tests_passed++;
    } else {
        ASSERT_NEAR(a, b, 0.0001);
    }
}

static void assert_prayer_times_equal(const prayer_times_t* a, const prayer_times_t* b) {
    assert_time_equal(a->fajr_utc_min, b->fajr_utc_min);
    assert_time_equal(a->sunrise_utc_min, b->sunrise_utc_min);
    assert_time_equal(a->dhuhr_utc_min, b->dhuhr_utc_min);
    assert_time_equal(a->asr_utc_min, b->asr_utc_min);
    assert_time_equal(a->sunset_utc_min, b->sunset_utc_min);
    assert_time_equal(a->maghrib_utc_min, b->maghrib_utc_min);
    assert_time_equal(a->isha_utc_min, b->isha_utc_min);
    assert_time_equal(a->midnight_utc_min, b->midnight_utc_min);
    assert_time_equal(a->first_third_utc_min, b->first_third_utc_min);
    assert_time_equal(a->last_third_utc_min, b->last_third_utc_min);
    assert_time_equal(a->sunrise_forbidden_start_utc_min, b->sunrise_forbidden_start_utc_min);
    assert_time_equal(a->sunrise_forbidden_end_utc_min, b->sunrise_forbidden_end_utc_min);
    assert_time_equal(a->zawal_forbidden_start_utc_min, b->zawal_forbidden_start_utc_min);
    assert_time_equal(a->zawal_forbidden_end_utc_min, b->zawal_forbidden_end_utc_min);
    assert_time_equal(a->sunset_forbidden_start_utc_min, b->sunset_forbidden_start_utc_min);
    assert_time_equal(a->sunset_forbidden_end_utc_min, b->sunset_forbidden_end_utc_min);
}

static void test_calculate_month_leap_year_february() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_KARACHI, &settings);
    settings.madhab = MADHAB_HANAFI;

    prayer_times_t month_times[31];
    int day_count = 0;
    bool ok = engine_calculate_month(2024, 2, 23.8103, 90.4125, &settings, month_times, &day_count);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(day_count == 29);

    for (int d = 1; d <= 29; d++) {
        prayer_times_t single_day;
        bool single_ok = engine_calculate_times(2024, 2, d, 23.8103, 90.4125, &settings, &single_day);
        ASSERT_TRUE(single_ok);
        assert_prayer_times_equal(&month_times[d - 1], &single_day);
    }
}

static void test_calculate_month_30_days() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_NORTH_AMERICA, &settings);

    prayer_times_t month_times[31];
    int day_count = 0;
    /* New York in April (30 days) */
    bool ok = engine_calculate_month(2024, 4, 40.7128, -74.0060, &settings, month_times, &day_count);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(day_count == 30);

    for (int d = 1; d <= 30; d++) {
        prayer_times_t single_day;
        bool single_ok = engine_calculate_times(2024, 4, d, 40.7128, -74.0060, &settings, &single_day);
        ASSERT_TRUE(single_ok);
        assert_prayer_times_equal(&month_times[d - 1], &single_day);
    }
}

static void test_calculate_month_31_days() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_MWL, &settings);

    prayer_times_t month_times[31];
    int day_count = 0;
    /* London in January (31 days) */
    bool ok = engine_calculate_month(2024, 1, 51.5074, -0.1278, &settings, month_times, &day_count);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(day_count == 31);

    for (int d = 1; d <= 31; d++) {
        prayer_times_t single_day;
        bool single_ok = engine_calculate_times(2024, 1, d, 51.5074, -0.1278, &settings, &single_day);
        ASSERT_TRUE(single_ok);
        assert_prayer_times_equal(&month_times[d - 1], &single_day);
    }
}

static void test_calculate_month_polar_high_lat() {
    /* High latitude: Oslo (59.91 N) with 1/7th rule */
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_MWL, &settings);
    settings.high_lat_rule = HIGH_LAT_SEVENTH_OF_NIGHT;

    prayer_times_t month_times[31];
    int day_count = 0;
    bool ok = engine_calculate_month(2024, 6, 59.9139, 10.7522, &settings, month_times, &day_count);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(day_count == 30);

    for (int d = 1; d <= 30; d++) {
        prayer_times_t single_day;
        bool single_ok = engine_calculate_times(2024, 6, d, 59.9139, 10.7522, &settings, &single_day);
        ASSERT_TRUE(single_ok);
        assert_prayer_times_equal(&month_times[d - 1], &single_day);
    }

    /* Polar location: Tromso (69.65 N) with POLAR_AQRAB_BALAD */
    settings.polar_resolution = POLAR_AQRAB_BALAD;
    ok = engine_calculate_month(2024, 6, 69.6492, 18.9553, &settings, month_times, &day_count);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(day_count == 30);

    for (int d = 1; d <= 30; d++) {
        prayer_times_t single_day;
        bool single_ok = engine_calculate_times(2024, 6, d, 69.6492, 18.9553, &settings, &single_day);
        ASSERT_TRUE(single_ok);
        assert_prayer_times_equal(&month_times[d - 1], &single_day);
    }
}

/*
 * Regression: the per-day path once skipped tomorrow's night length when computing the next
 * day's Fajr for Sunnah times, which broke midnight / last third whenever the safe-Fajr bound
 * applies (Fajr near solar midnight, date-line longitudes). Expected values are from adhan-js.
 */
static void test_sunnah_times_need_full_next_day_fajr() {
    engine_settings_t s;
    engine_get_default_settings(CALC_METHOD_MWL, &s);
    prayer_times_t t;

    ASSERT_TRUE(engine_calculate_times(2027, 1, 20, -52.0, 50.8, &s, &t));
    ASSERT_NEAR(t.midnight_utc_min, 1127.0, 0.001);
    ASSERT_NEAR(t.last_third_utc_min, 1167.0, 0.001);

    ASSERT_TRUE(engine_calculate_times(2027, 6, 12, 0.0, -180.0, &s, &t));
    ASSERT_NEAR(t.midnight_utc_min, 2342.0, 0.001);
    ASSERT_NEAR(t.last_third_utc_min, 2521.0, 0.001);

    ASSERT_TRUE(engine_calculate_times(2027, 8, 8, 56.0, 97.6, &s, &t));
    ASSERT_NEAR(t.midnight_utc_min, 929.0, 0.001);
    ASSERT_NEAR(t.last_third_utc_min, 971.0, 0.001);

    /* Same cases through the monthly sliding window must agree with the per-day path */
    prayer_times_t month[31];
    int dc = 0;
    ASSERT_TRUE(engine_calculate_month(2027, 1, -52.0, 50.8, &s, month, &dc));
    ASSERT_NEAR(month[19].midnight_utc_min, 1127.0, 0.001);
}

/* ROUNDING_NEAREST / ROUNDING_UP results must land exactly on a minute, including the
 * night-fraction paths (1/7th rule, Sunnah times) that produce fractional seconds. */
static void test_rounded_times_are_whole_minutes() {
    engine_settings_t s;
    engine_get_default_settings(CALC_METHOD_MWL, &s);
    s.high_lat_rule = HIGH_LAT_SEVENTH_OF_NIGHT;
    for (int r = 0; r < 2; r++) {
        s.rounding = (r == 0) ? ROUNDING_NEAREST : ROUNDING_UP;
        prayer_times_t month[31];
        int dc = 0;
        ASSERT_TRUE(engine_calculate_month(2024, 6, 59.9139, 10.7522, &s, month, &dc));
        bool all_whole = true;
        for (int d = 0; d < dc; d++) {
            const double* v = (const double*)&month[d];
            for (size_t k = 0; k < sizeof(prayer_times_t) / sizeof(double); k++) {
                if (!isnan(v[k]) && v[k] != floor(v[k])) all_whole = false;
            }
        }
        ASSERT_TRUE(all_whole);
    }
    /* Oslo 2024-06-21 Fajr (1/7th rule) is 01:09:29.857 UTC raw: nearest is 01:09, up is 01:10 */
    prayer_times_t t;
    s.rounding = ROUNDING_NEAREST;
    ASSERT_TRUE(engine_calculate_times(2024, 6, 21, 59.9139, 10.7522, &s, &t));
    ASSERT_NEAR(t.fajr_utc_min, 69.0, 0.0001);
    s.rounding = ROUNDING_UP;
    ASSERT_TRUE(engine_calculate_times(2024, 6, 21, 59.9139, 10.7522, &s, &t));
    ASSERT_NEAR(t.fajr_utc_min, 70.0, 0.0001);
}

static void test_invalid_inputs_rejected() {
    engine_settings_t s;
    engine_get_default_settings(CALC_METHOD_MWL, &s);
    prayer_times_t t;
    prayer_times_t month[31];
    ASSERT_TRUE(!engine_calculate_times(2024, 13, 1, 0.0, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_times(2024, 0, 1, 0.0, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_times(2023, 2, 29, 0.0, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_times(2024, 4, 31, 0.0, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_times(2024, 4, 0, 0.0, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_times(2024, 4, 1, NAN, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_times(2024, 4, 1, 0.0, 180.5, &s, &t));
    ASSERT_TRUE(engine_calculate_times(2024, 2, 29, 0.0, 0.0, &s, &t));
    s.method = CALC_METHOD_COUNT;
    ASSERT_TRUE(!engine_calculate_times(2024, 4, 1, 0.0, 0.0, &s, &t));
    ASSERT_TRUE(!engine_calculate_month(2024, 4, 0.0, 0.0, &s, month, NULL));
}

static void test_hour_angle_and_solar_times_pinning() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_MWL, &settings);
    settings.rounding = ROUNDING_NONE;

    prayer_times_t t1, t2;
    /* Mecca 2024-06-20 */
    bool ok1 = engine_calculate_times(2024, 6, 20, 21.4225, 39.8262, &settings, &t1);
    ASSERT_TRUE(ok1);
    ASSERT_TRUE(!isnan(t1.fajr_utc_min));
    ASSERT_TRUE(!isnan(t1.sunrise_utc_min));
    ASSERT_TRUE(!isnan(t1.sunset_utc_min));
    ASSERT_TRUE(!isnan(t1.asr_utc_min));

    /* London 2024-12-21 */
    bool ok2 = engine_calculate_times(2024, 12, 21, 51.5074, -0.1278, &settings, &t2);
    ASSERT_TRUE(ok2);
    ASSERT_TRUE(!isnan(t2.fajr_utc_min));
    ASSERT_TRUE(!isnan(t2.sunrise_utc_min));
    ASSERT_TRUE(!isnan(t2.sunset_utc_min));
    ASSERT_TRUE(!isnan(t2.asr_utc_min));

    /* Dhaka 2024-03-20 */
    prayer_times_t t3;
    bool ok3 = engine_calculate_times(2024, 3, 20, 23.8103, 90.4125, &settings, &t3);
    ASSERT_TRUE(ok3);
    ASSERT_TRUE(!isnan(t3.fajr_utc_min));
    ASSERT_TRUE(!isnan(t3.sunrise_utc_min));
    ASSERT_TRUE(!isnan(t3.dhuhr_utc_min));
    ASSERT_TRUE(!isnan(t3.asr_utc_min));
    ASSERT_TRUE(!isnan(t3.sunset_utc_min));
    ASSERT_TRUE(!isnan(t3.isha_utc_min));
    ASSERT_TRUE(t3.sunrise_utc_min > t3.fajr_utc_min);
    ASSERT_TRUE(t3.sunset_utc_min > t3.asr_utc_min);

    /* Assert astronomical progression */
    ASSERT_TRUE(t1.sunrise_utc_min > t1.fajr_utc_min);
    ASSERT_TRUE(t1.dhuhr_utc_min > t1.sunrise_utc_min);
    ASSERT_TRUE(t1.asr_utc_min > t1.dhuhr_utc_min);
    ASSERT_TRUE(t1.sunset_utc_min > t1.asr_utc_min);
    ASSERT_TRUE(t1.isha_utc_min > t1.sunset_utc_min);

    ASSERT_TRUE(t2.sunrise_utc_min > t2.fajr_utc_min);
    ASSERT_TRUE(t2.dhuhr_utc_min > t2.sunrise_utc_min);
    ASSERT_TRUE(t2.asr_utc_min > t2.dhuhr_utc_min);
    ASSERT_TRUE(t2.sunset_utc_min > t2.asr_utc_min);
    ASSERT_TRUE(t2.isha_utc_min > t2.sunset_utc_min);
}

/* ---- Golden fixtures generated by scripts/gen-fixtures (adhan-js 4.4.3) ---- */

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char* buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static const char* json_str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static bool fixture_settings(const cJSON* e, engine_settings_t* s) {
    static const char* METHOD_NAMES[] = {
        "MuslimWorldLeague", "Egyptian", "Karachi", "UmmAlQura", "Dubai", "MoonsightingCommittee",
        "NorthAmerica", "Kuwait", "Qatar", "Singapore", "Tehran", "Turkey"
    };
    const char* method = json_str(e, "method");
    int mi = -1;
    for (int i = 0; i < (int)(sizeof(METHOD_NAMES) / sizeof(METHOD_NAMES[0])); i++) {
        if (strcmp(METHOD_NAMES[i], method) == 0) mi = i;
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
 * Every fixture field must match adhan-js to the second (adhan-js keeps stray sub-second
 * milliseconds after rounding; the engine lands exactly on the minute). Fields adhan-js
 * reports as invalid dates (null) must be NAN.
 */
static void test_golden_fixture_file(const char* path, int expected_cases) {
    char* text = read_file(path);
    ASSERT_TRUE(text != NULL);
    if (!text) {
        printf("FAIL: cannot read fixture file %s\n", path);
        return;
    }
    cJSON* root = cJSON_Parse(text);
    free(text);
    ASSERT_TRUE(cJSON_IsArray(root));
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return;
    }

    static const char* KEYS[] = {
        "fajr_ms", "sunrise_ms", "dhuhr_ms", "asr_ms", "sunset_ms", "maghrib_ms", "isha_ms",
        "midnight_ms", "last_third_ms"
    };
    int cases = 0, failed_fields = 0, checked_fields = 0, reported = 0;
    const cJSON* e = NULL;
    cJSON_ArrayForEach(e, root) {
        cases++;
        engine_settings_t s;
        int y = 0, mo = 0, d = 0;
        if (!fixture_settings(e, &s) || sscanf(json_str(e, "date"), "%d-%d-%d", &y, &mo, &d) != 3) {
            failed_fields++;
            printf("FAIL: unparseable fixture #%d in %s\n", cases, path);
            continue;
        }
        double lat = cJSON_GetObjectItemCaseSensitive(e, "lat")->valuedouble;
        double lng = cJSON_GetObjectItemCaseSensitive(e, "lng")->valuedouble;

        prayer_times_t t;
        if (!engine_calculate_times(y, mo, d, lat, lng, &s, &t)) {
            failed_fields++;
            printf("FAIL: engine_calculate_times rejected fixture #%d in %s\n", cases, path);
            continue;
        }
        const double got_min[9] = {
            t.fajr_utc_min, t.sunrise_utc_min, t.dhuhr_utc_min, t.asr_utc_min, t.sunset_utc_min,
            t.maghrib_utc_min, t.isha_utc_min, t.midnight_utc_min, t.last_third_utc_min
        };
        for (int k = 0; k < 9; k++) {
            checked_fields++;
            const cJSON* ex = cJSON_GetObjectItemCaseSensitive(e, KEYS[k]);
            bool expect_nan = !cJSON_IsNumber(ex);
            bool ok;
            long long diff_ms = 0;
            if (expect_nan || isnan(got_min[k])) {
                ok = expect_nan && isnan(got_min[k]);
            } else {
                long long got_ms = (long long)engine_utc_minutes_to_epoch_ms(y, mo, d, got_min[k]);
                diff_ms = got_ms - (long long)ex->valuedouble;
                ok = (diff_ms > -1000 && diff_ms < 1000);
            }
            if (!ok) {
                failed_fields++;
                if (reported++ < 20) {
                    printf("FAIL: %s %s lat=%.4f lng=%.4f %s %s %s/%s %s: diff %+lld ms%s\n",
                           json_str(e, "city"), json_str(e, "date"), lat, lng, json_str(e, "method"),
                           json_str(e, "madhab"), json_str(e, "high_lat_rule"), json_str(e, "polar_resolution"),
                           KEYS[k], diff_ms, (expect_nan || isnan(got_min[k])) ? " (NaN mismatch)" : "");
                }
            }
        }
    }
    cJSON_Delete(root);

    /* One assertion per fixture file keeps the summary readable; details are printed above. */
    g_tests_run++;
    if (cases == expected_cases && failed_fields == 0) {
        g_tests_passed++;
    } else {
        printf("FAIL: golden %s: %d cases (expected %d), %d/%d fields mismatched\n",
               path, cases, expected_cases, failed_fields, checked_fields);
    }
    printf("Golden %s: %d cases, %d fields checked, %d mismatched\n", path, cases, checked_fields, failed_fields);
}

int main(int argc, char** argv) {
    /* Fixture directory defaults to the one used by `make -C native test` */
    const char* fixture_dir = (argc > 1) ? argv[1] : "tests/fixtures";
    char path_matrix[512], path_random[512];
    snprintf(path_matrix, sizeof(path_matrix), "%s/prayer_fixtures.json", fixture_dir);
    snprintf(path_random, sizeof(path_random), "%s/prayer_fixtures_random.json", fixture_dir);

    printf("--- Running Waqt-Tray Engine Tests ---\n");

    test_golden_fixture_file(path_matrix, 2016);
    test_golden_fixture_file(path_random, 1500);

    test_julian_day();
    test_solar_coordinates();
    test_hijri_dates();
    test_dhaka_prayer_times();
    test_high_latitude_oslo();
    test_calculate_month_leap_year_february();
    test_calculate_month_30_days();
    test_calculate_month_31_days();
    test_calculate_month_polar_high_lat();
    test_hour_angle_and_solar_times_pinning();
    test_sunnah_times_need_full_next_day_fajr();
    test_rounded_times_are_whole_minutes();
    test_invalid_inputs_rejected();

    printf("\nTests run: %d, Passed: %d, Failed: %d\n",
           g_tests_run, g_tests_passed, g_tests_run - g_tests_passed);

    if (g_tests_run == g_tests_passed) {
        printf("ALL TESTS PASSED!\n");
        return 0;
    }
    return 1;
}
