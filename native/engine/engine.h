#ifndef AZAAN_ENGINE_H
#define AZAAN_ENGINE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALC_METHOD_MWL = 0,               /* Muslim World League (18/17) */
    CALC_METHOD_EGYPTIAN,              /* Egyptian General Authority (19.5/17.5) */
    CALC_METHOD_KARACHI,               /* Univ of Islamic Sciences, Karachi (18/18) */
    CALC_METHOD_UMM_AL_QURA,           /* Umm al-Qura, Makkah (18.5/90m interval) */
    CALC_METHOD_DUBAI,                 /* Dubai (18.2/18.2) */
    CALC_METHOD_MOONSIGHTING,          /* Moonsighting Committee */
    CALC_METHOD_NORTH_AMERICA,         /* ISNA (15/15) */
    CALC_METHOD_KUWAIT,                /* Kuwait (18/17.5) */
    CALC_METHOD_QATAR,                 /* Qatar (18/90m interval) */
    CALC_METHOD_SINGAPORE,             /* MUIS Singapore (20/18) */
    CALC_METHOD_TEHRAN,                /* Inst of Geophysics, Univ of Tehran */
    CALC_METHOD_TURKEY,                /* Diyanet, Turkey (18/17) */
    CALC_METHOD_COUNT
} calc_method_t;

typedef enum {
    MADHAB_STANDARD = 0,               /* Shafi, Maliki, Hanbali (shadow factor 1) */
    MADHAB_HANAFI = 1                  /* Hanafi (shadow factor 2) */
} madhab_t;

typedef enum {
    HIGH_LAT_NONE = 0,
    HIGH_LAT_MIDNIGHT,                 /* Middle of the night (max 1/2 of night) */
    HIGH_LAT_SEVENTH,                  /* 1/7th of night */
    HIGH_LAT_ANGLE_BASED               /* Angle / 60 fraction of night */
} high_lat_rule_t;

typedef struct {
    calc_method_t method;
    madhab_t madhab;
    high_lat_rule_t high_lat_rule;
    /* Manual angle overrides (used if > 0) */
    double fajr_angle_deg;
    double isha_angle_deg;
    int isha_interval_min;             /* e.g. 90 if > 0 */
    int maghrib_interval_min;          /* interval after sunset if > 0 */
    /* Per-prayer minute adjustments (-60 to +60) */
    int fajr_offset_min;
    int sunrise_offset_min;
    int dhuhr_offset_min;
    int asr_offset_min;
    int sunset_offset_min;
    int maghrib_offset_min;
    int isha_offset_min;
} engine_settings_t;

typedef struct {
    /* Times in minutes from UTC midnight [0.0, 1440.0), or NAN if event doesn't occur */
    double fajr_utc_min;
    double sunrise_utc_min;
    double dhuhr_utc_min;
    double asr_utc_min;
    double sunset_utc_min;
    double maghrib_utc_min;
    double isha_utc_min;

    /* Extra calculated times */
    double midnight_utc_min;
    double last_third_utc_min;

    /* Forbidden (Makruh) windows: [start, end) in UTC minutes */
    double sunrise_forbidden_end_utc_min;  /* Sunrise to +15 min */
    double zenith_forbidden_start_utc_min; /* Dhuhr - 10 min to Dhuhr */
    double sunset_forbidden_start_utc_min; /* Sunset - 15 min to Sunset */
} prayer_times_t;

typedef struct {
    int year;                          /* e.g. 1446 */
    int month;                         /* 1 to 12 */
    int day;                           /* 1 to 30 */
    const char* month_name_en;         /* e.g. "Ramadan" */
    const char* month_name_ar;         /* e.g. "رمضان" */
} hijri_date_t;

/* Initialize default settings for a given method */
void engine_get_default_settings(calc_method_t method, engine_settings_t* out_settings);

/* Calculate prayer times for a given Gregorian date and location */
bool engine_calculate_times(
    int year, int month, int day,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    prayer_times_t* out_times
);

/* Tabular Hijri date calculation with user adjustment (-2 to +2 days) */
void engine_get_hijri_date(int year, int month, int day, int adjustment_days, hijri_date_t* out_hijri);

/* Returns standard Julian Day for Gregorian Y-M-D at 12:00:00 UTC */
double engine_julian_day(int year, int month, int day);

/* Solar position helper */
void engine_solar_coordinates(double jd, double* out_declination_deg, double* out_equation_of_time_min);

#ifdef __cplusplus
}
#endif

#endif /* AZAAN_ENGINE_H */
