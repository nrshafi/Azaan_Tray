#ifndef AZAAN_ENGINE_H
#define AZAAN_ENGINE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALC_METHOD_MWL = 0,               /* Muslim World League */
    CALC_METHOD_EGYPTIAN,              /* Egyptian General Authority of Survey */
    CALC_METHOD_KARACHI,               /* University of Islamic Sciences, Karachi */
    CALC_METHOD_UMM_AL_QURA,           /* Umm al-Qura University, Makkah */
    CALC_METHOD_DUBAI,                 /* Dubai */
    CALC_METHOD_MOONSIGHTING,          /* Moonsighting Committee */
    CALC_METHOD_NORTH_AMERICA,         /* ISNA */
    CALC_METHOD_KUWAIT,                /* Kuwait */
    CALC_METHOD_QATAR,                 /* Qatar */
    CALC_METHOD_SINGAPORE,             /* MUIS Singapore */
    CALC_METHOD_TEHRAN,                /* Institute of Geophysics, University of Tehran */
    CALC_METHOD_TURKEY,                /* Diyanet, Turkey */
    CALC_METHOD_OTHER,                 /* Custom / Other */
    CALC_METHOD_COUNT
} calc_method_t;

typedef enum {
    MADHAB_STANDARD = 0,               /* Shafi'i, Maliki, Hanbali (shadow factor 1) */
    MADHAB_HANAFI = 1                  /* Hanafi (shadow factor 2) */
} madhab_t;

typedef enum {
    HIGH_LAT_MIDDLE_OF_NIGHT = 0,      /* Middle of the night (1/2) */
    HIGH_LAT_SEVENTH_OF_NIGHT,         /* One-seventh of the night (1/7) */
    HIGH_LAT_TWILIGHT_ANGLE            /* Angle / 60 fraction of night */
} high_lat_rule_t;

typedef enum {
    POLAR_UNRESOLVED = 0,              /* Leave NAN if sun does not rise/set */
    POLAR_AQRAB_YAUM,                  /* Closest day where sun rises and sets */
    POLAR_AQRAB_BALAD                  /* Closest latitude (step 0.5 deg from >= 65) */
} polar_resolution_t;

typedef enum {
    ROUNDING_NEAREST = 0,              /* Round to nearest minute (>= 30s rounds up) */
    ROUNDING_UP,                       /* Round up to next minute if seconds > 0 */
    ROUNDING_NONE                      /* Keep exact seconds */
} rounding_rule_t;

typedef enum {
    SHAFAQ_GENERAL = 0,                /* General twilight (Moonsighting Committee default) */
    SHAFAQ_AHMER,                      /* Red twilight */
    SHAFAQ_ABYAD                       /* White twilight */
} shafaq_t;

typedef struct {
    int fajr;
    int sunrise;
    int dhuhr;
    int asr;
    int sunset;
    int maghrib;
    int isha;
} prayer_adjustments_t;

typedef struct {
    calc_method_t method;
    madhab_t madhab;
    high_lat_rule_t high_lat_rule;
    polar_resolution_t polar_resolution;
    rounding_rule_t rounding;
    shafaq_t shafaq;

    /* Angles and intervals set by method or custom configuration */
    double fajr_angle_deg;
    double isha_angle_deg;
    int isha_interval_min;             /* Minutes after Maghrib (used if > 0) */
    double maghrib_angle_deg;          /* Degrees below horizon (used if > 0, e.g. Tehran 4.5) */

    /* Built-in method adjustments (minutes) */
    prayer_adjustments_t method_adjustments;

    /* User per-prayer minute adjustments (-60 to +60) */
    prayer_adjustments_t user_adjustments;

    /* Configurable forbidden (makruh) window durations in minutes */
    int sunrise_forbidden_duration_min; /* Default 15 min after sunrise */
    int zawal_forbidden_duration_min;   /* Default 10 min before Dhuhr */
    int sunset_forbidden_duration_min;  /* Default 15 min before sunset */
} engine_settings_t;

typedef struct {
    /*
     * Times in minutes from 00:00:00 UTC of the requested date (year, month, day).
     * Can be negative (falls on previous UTC day) or >= 1440.0 (falls on next UTC day).
     * NAN if event does not occur and polar_resolution is POLAR_UNRESOLVED.
     */
    double fajr_utc_min;
    double sunrise_utc_min;
    double dhuhr_utc_min;
    double asr_utc_min;
    double sunset_utc_min;
    double maghrib_utc_min;
    double isha_utc_min;

    /* Night times (from Maghrib to next day's Fajr, matching SunnahTimes) */
    double midnight_utc_min;
    double first_third_utc_min;
    double last_third_utc_min;

    /* Forbidden (makruh) windows in minutes from 00:00:00 UTC of the requested date */
    double sunrise_forbidden_start_utc_min;
    double sunrise_forbidden_end_utc_min;
    double zawal_forbidden_start_utc_min;
    double zawal_forbidden_end_utc_min;
    double sunset_forbidden_start_utc_min;
    double sunset_forbidden_end_utc_min;
} prayer_times_t;

typedef struct {
    int year;                          /* e.g. 1445 */
    int month;                         /* 1 to 12 */
    int day;                           /* 1 to 30 */
    const char* month_name_en;         /* e.g. "Ramadan" */
    const char* month_name_ar;         /* e.g. "رمضان" */
} hijri_date_t;

/* Initialize default settings for a given calculation method */
void engine_get_default_settings(calc_method_t method, engine_settings_t* out_settings);

/* Calculate prayer times, Sunnah night times, and forbidden windows for a Gregorian date */
bool engine_calculate_times(
    int year, int month, int day,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    prayer_times_t* out_times
);

/* Calculate prayer times for an entire Gregorian month (days 1..days_in_month) with sliding window reuse.
 * out_times must point to an array with capacity for at least 31 prayer_times_t elements.
 * If out_day_count is non-NULL, receives the number of days in that month (28, 29, 30, or 31).
 */
bool engine_calculate_month(
    int year, int month,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    prayer_times_t* out_times,
    int* out_day_count
);

/* Convert a Gregorian date + relative UTC minutes into Unix epoch milliseconds */
int64_t engine_utc_minutes_to_epoch_ms(int year, int month, int day, double utc_min);

/* Tabular Hijri date calculation with user adjustment (-2 to +2 days) */
void engine_get_hijri_date(int year, int month, int day, int adjustment_days, hijri_date_t* out_hijri);

/* Returns Julian Day for Gregorian Y-M-D at 00:00:00 UTC (Meeus Ch. 7, p. 60) */
double engine_julian_day(int year, int month, int day);

/* Calculate solar declination (deg) and equation of time (min) for a Julian Day */
void engine_solar_coordinates(double jd, double* out_decl_deg, double* out_eot_min);

#ifdef __cplusplus
}
#endif

#endif /* AZAAN_ENGINE_H */
