#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "../engine/engine.h"

static void minutes_to_time_str(double utc_min, int tz_offset_min, char* out_buf, size_t buf_sz) {
    if (isnan(utc_min)) {
        snprintf(out_buf, buf_sz, "--:--");
        return;
    }
    double local_min = fmod(utc_min + (double)tz_offset_min, 1440.0);
    if (local_min < 0.0) local_min += 1440.0;
    int h = (int)(local_min / 60.0);
    int m = (int)fmod(local_min, 60.0);
    snprintf(out_buf, buf_sz, "%02d:%02d", h, m);
}

int main(int argc, char** argv) {
    bool json_mode = false;
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
        } else if (strcmp(argv[i], "--date") == 0 && i + 1 < argc) {
            sscanf(argv[++i], "%d-%d-%d", &year, &month, &day);
        } else if (strcmp(argv[i], "--lat") == 0 && i + 1 < argc) {
            lat = atof(argv[++i]);
        } else if (strcmp(argv[i], "--lng") == 0 && i + 1 < argc) {
            lng = atof(argv[++i]);
        }
    }

    /* Get current local time zone offset in minutes */
    TIME_ZONE_INFORMATION tzi;
    GetTimeZoneInformation(&tzi);
    int tz_offset_min = -tzi.Bias;

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
    minutes_to_time_str(times.fajr_utc_min, tz_offset_min, fajr, sizeof(fajr));
    minutes_to_time_str(times.sunrise_utc_min, tz_offset_min, sunrise, sizeof(sunrise));
    minutes_to_time_str(times.dhuhr_utc_min, tz_offset_min, dhuhr, sizeof(dhuhr));
    minutes_to_time_str(times.asr_utc_min, tz_offset_min, asr, sizeof(asr));
    minutes_to_time_str(times.sunset_utc_min, tz_offset_min, sunset, sizeof(sunset));
    minutes_to_time_str(times.maghrib_utc_min, tz_offset_min, maghrib, sizeof(maghrib));
    minutes_to_time_str(times.isha_utc_min, tz_offset_min, isha, sizeof(isha));
    minutes_to_time_str(times.midnight_utc_min, tz_offset_min, midnight, sizeof(midnight));
    minutes_to_time_str(times.last_third_utc_min, tz_offset_min, last_third, sizeof(last_third));

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
        printf(" Azaan Tray - Islamic Prayer Times\n");
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
