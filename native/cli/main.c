#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "../engine/engine.h"

/* FILETIME counts 100 ns intervals since 1601-01-01 UTC; Unix epoch offset in those units */
#define FILETIME_UNIX_EPOCH_100NS 116444736000000000LL

/*
 * Format an engine UTC time as local HH:MM using the Windows time-zone rules that apply on
 * that instant (DST and historical rule changes via the dynamic time-zone information),
 * not just today's offset. A NULL tz prints UTC (`--utc`: output independent of the machine).
 */
static void utc_to_local_time_str(int year, int month, int day, double utc_min,
                                  const DYNAMIC_TIME_ZONE_INFORMATION* tz,
                                  char* out_buf, size_t buf_sz) {
    snprintf(out_buf, buf_sz, "--:--");
    if (isnan(utc_min)) return;

    int64_t epoch_ms = engine_utc_minutes_to_epoch_ms(year, month, day, utc_min);
    int64_t ft100ns = epoch_ms * 10000LL + FILETIME_UNIX_EPOCH_100NS;
    if (ft100ns < 0) return;

    FILETIME ft;
    ft.dwLowDateTime = (DWORD)((uint64_t)ft100ns & 0xFFFFFFFFu);
    ft.dwHighDateTime = (DWORD)((uint64_t)ft100ns >> 32);
    SYSTEMTIME utc_st, local_st;
    if (!FileTimeToSystemTime(&ft, &utc_st)) return;
    if (!tz) {
        local_st = utc_st;
    } else if (!SystemTimeToTzSpecificLocalTimeEx(tz, &utc_st, &local_st)) {
        return;
    }
    snprintf(out_buf, buf_sz, "%02u:%02u", (unsigned)local_st.wHour, (unsigned)local_st.wMinute);
}

static bool parse_double_arg(const char* s, double min_v, double max_v, double* out) {
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || isnan(v) || v < min_v || v > max_v) return false;
    *out = v;
    return true;
}

static bool parse_date_arg(const char* s, int* y, int* m, int* d) {
    int yy, mm, dd;
    char extra;
    if (sscanf(s, "%d-%d-%d%c", &yy, &mm, &dd, &extra) != 3) return false;
    static const int DIM[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (yy < 1900 || yy > 2200 || mm < 1 || mm > 12 || dd < 1 || dd > DIM[mm - 1]) return false;
    bool leap = (yy % 4 == 0 && yy % 100 != 0) || yy % 400 == 0;
    if (mm == 2 && dd == 29 && !leap) return false;
    *y = yy; *m = mm; *d = dd;
    return true;
}

static int usage_error(const char* msg, const char* arg) {
    fprintf(stderr, "Error: %s%s%s\n", msg, arg ? ": " : "", arg ? arg : "");
    fprintf(stderr, "Usage: azaan [--print] [--json] [--utc] [--date YYYY-MM-DD] [--lat DEG] [--lng DEG]\n");
    return 2;
}

int main(int argc, char** argv) {
    bool json_mode = false;
    bool utc_mode = false;
    double lat = 23.8103;  /* Default: Dhaka */
    double lng = 90.4125;
    calc_method_t method = CALC_METHOD_KARACHI;
    madhab_t madhab = MADHAB_HANAFI;

    SYSTEMTIME st;
    GetLocalTime(&st);
    int year = st.wYear;
    int month = st.wMonth;
    int day = st.wDay;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print") == 0) {
            /* Default mode */
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--utc") == 0) {
            utc_mode = true;
        } else if (strcmp(argv[i], "--date") == 0 && i + 1 < argc) {
            if (!parse_date_arg(argv[++i], &year, &month, &day)) return usage_error("invalid --date", argv[i]);
        } else if (strcmp(argv[i], "--lat") == 0 && i + 1 < argc) {
            if (!parse_double_arg(argv[++i], -90.0, 90.0, &lat)) return usage_error("invalid --lat", argv[i]);
        } else if (strcmp(argv[i], "--lng") == 0 && i + 1 < argc) {
            if (!parse_double_arg(argv[++i], -180.0, 180.0, &lng)) return usage_error("invalid --lng", argv[i]);
        } else {
            return usage_error("unknown or incomplete argument", argv[i]);
        }
    }

    /* Current Windows time zone with dynamic (per-year) DST rules */
    DYNAMIC_TIME_ZONE_INFORMATION tz_info;
    const DYNAMIC_TIME_ZONE_INFORMATION* tz = NULL; /* NULL = print UTC */
    if (!utc_mode) {
        if (GetDynamicTimeZoneInformation(&tz_info) == TIME_ZONE_ID_INVALID) {
            fprintf(stderr, "Error: cannot read the Windows time zone.\n");
            return 1;
        }
        tz = &tz_info;
    }

    engine_settings_t settings;
    engine_get_default_settings(method, &settings);
    settings.madhab = madhab;

    prayer_times_t times;
    if (!engine_calculate_times(year, month, day, lat, lng, &settings, &times)) {
        fprintf(stderr, "Error: Failed to calculate prayer times.\n");
        return 1;
    }

    hijri_date_t hijri;
    engine_get_hijri_date(year, month, day, 0, &hijri);

    char fajr[16], sunrise[16], dhuhr[16], asr[16], sunset[16], maghrib[16], isha[16];
    char midnight[16], last_third[16];
    utc_to_local_time_str(year, month, day, times.fajr_utc_min, tz, fajr, sizeof(fajr));
    utc_to_local_time_str(year, month, day, times.sunrise_utc_min, tz, sunrise, sizeof(sunrise));
    utc_to_local_time_str(year, month, day, times.dhuhr_utc_min, tz, dhuhr, sizeof(dhuhr));
    utc_to_local_time_str(year, month, day, times.asr_utc_min, tz, asr, sizeof(asr));
    utc_to_local_time_str(year, month, day, times.sunset_utc_min, tz, sunset, sizeof(sunset));
    utc_to_local_time_str(year, month, day, times.maghrib_utc_min, tz, maghrib, sizeof(maghrib));
    utc_to_local_time_str(year, month, day, times.isha_utc_min, tz, isha, sizeof(isha));
    utc_to_local_time_str(year, month, day, times.midnight_utc_min, tz, midnight, sizeof(midnight));
    utc_to_local_time_str(year, month, day, times.last_third_utc_min, tz, last_third, sizeof(last_third));

    if (json_mode) {
        printf("{\n");
        printf("  \"date\": \"%04d-%02d-%02d\",\n", year, month, day);
        printf("  \"hijri\": \"%d %s %d\",\n", hijri.day, hijri.month_name_en, hijri.year);
        printf("  \"location\": { \"lat\": %.4f, \"lng\": %.4f },\n", lat, lng);
        printf("  \"times\": {\n");
        printf("    \"fajr\": \"%s\",\n", fajr);
        printf("    \"sunrise\": \"%s\",\n", sunrise);
        printf("    \"dhuhr\": \"%s\",\n", dhuhr);
        printf("    \"asr\": \"%s\",\n", asr);
        printf("    \"sunset\": \"%s\",\n", sunset);
        printf("    \"maghrib\": \"%s\",\n", maghrib);
        printf("    \"isha\": \"%s\",\n", isha);
        printf("    \"midnight\": \"%s\",\n", midnight);
        printf("    \"last_third\": \"%s\"\n", last_third);
        printf("  }\n");
        printf("}\n");
    } else {
        printf("==================================================\n");
        printf(" Waqt-Tray - Islamic Prayer Times\n");
        printf(" Date: %04d-%02d-%02d | %d %s %d AH\n", year, month, day, hijri.day, hijri.month_name_en, hijri.year);
        printf(" Location: Lat %.4f, Lng %.4f\n", lat, lng);
        printf(" Method: Karachi | Madhab: Hanafi\n");
        printf("==================================================\n");
        printf("  Fajr:       %s\n", fajr);
        printf("  Sunrise:    %s\n", sunrise);
        printf("  Dhuhr:      %s\n", dhuhr);
        printf("  Asr:        %s\n", asr);
        printf("  Sunset:     %s\n", sunset);
        printf("  Maghrib:    %s\n", maghrib);
        printf("  Isha:       %s\n", isha);
        printf("--------------------------------------------------\n");
        printf("  Midnight:   %s\n", midnight);
        printf("  Last Third: %s\n", last_third);
        printf("==================================================\n");
    }

    return 0;
}
