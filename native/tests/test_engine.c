#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "../engine/engine.h"

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
    /* J2000.0 epoch: 2000 Jan 1 12:00 UTC = 2451545.0 */
    double jd = engine_julian_day(2000, 1, 1);
    ASSERT_NEAR(jd, 2451545.0, 0.001);

    /* 1999 Dec 31 12:00 UTC = 2451544.0 */
    jd = engine_julian_day(1999, 12, 31);
    ASSERT_NEAR(jd, 2451544.0, 0.001);

    /* Leap year 2024-02-29 12:00 UTC = 2460370.0 */
    jd = engine_julian_day(2024, 2, 29);
    ASSERT_NEAR(jd, 2460370.0, 0.001);
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
    printf("Debug: 2024-03-12 Hijri day=%d month=%d (%s)\n", h.day, h.month, h.month_name_en);
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
    ASSERT_NEAR(times.dhuhr_utc_min - times.zenith_forbidden_start_utc_min, 10.0, 0.01);
    ASSERT_NEAR(times.sunset_utc_min - times.sunset_forbidden_start_utc_min, 15.0, 0.01);
}

static void test_high_latitude_oslo() {
    engine_settings_t settings;
    engine_get_default_settings(CALC_METHOD_MWL, &settings);
    settings.high_lat_rule = HIGH_LAT_SEVENTH;

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

int main() {
    printf("--- Running Azaan Tray Engine Tests ---\n");

    test_julian_day();
    test_solar_coordinates();
    test_hijri_dates();
    test_dhaka_prayer_times();
    test_high_latitude_oslo();

    printf("\nTests run: %d, Passed: %d, Failed: %d\n",
           g_tests_run, g_tests_passed, g_tests_run - g_tests_passed);

    if (g_tests_run == g_tests_passed) {
        printf("ALL TESTS PASSED!\n");
        return 0;
    }
    return 1;
}
