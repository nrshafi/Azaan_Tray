/*
 * Azaan Tray - Core Prayer Calculation Engine
 * Pure C11, no OS headers, libm only.
 *
 * Citations:
 * - Jean Meeus, "Astronomical Algorithms", 2nd Edition, Willmann-Bell (1998)
 * - Batoul Apps, adhan-js v4.4.3 reference algorithms (MIT)
 * - PrayTimes.org Calculation Equations
 */

#define _GNU_SOURCE
#include "engine.h"
#include <math.h>

#if defined(__GNUC__) || defined(__clang__)
#define FAST_SINCOS(rad, s, c) sincos(rad, s, c)
#else
#define FAST_SINCOS(rad, s, c) do { *(s) = sin(rad); *(c) = cos(rad); } while(0)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Standard refraction (-34') + solar semi-diameter (-16') = -50' = -50/60 deg (Meeus p. 101) */
static const double SOLAR_ALTITUDE_DEG = -50.0 / 60.0;

static inline double deg2rad(double deg) {
    return deg * (M_PI / 180.0);
}

static inline double rad2deg(double rad) {
    return rad * (180.0 / M_PI);
}

/* Meeus / MathUtils: normalize number to [0, max) */
static inline double normalize_to_scale(double num, double max_val) {
    return num - max_val * floor(num / max_val);
}

static inline double unwind_angle(double angle_deg) {
    return normalize_to_scale(angle_deg, 360.0);
}

static inline double quadrant_shift_angle(double angle_deg) {
    if (angle_deg >= -180.0 && angle_deg <= 180.0) {
        return angle_deg;
    }
    return angle_deg - 360.0 * round(angle_deg / 360.0);
}

/* Meeus Chapter 7, page 60: Julian Day at 00:00:00 UTC for Gregorian date Y-M-D */
double engine_julian_day(int year, int month, int day) {
    int y = (month > 2) ? year : (year - 1);
    int m = (month > 2) ? month : (month + 12);
    double d = (double)day;
    int a = y / 100;
    int b = 2 - a + (a / 4);
    int i0 = (int)(365.25 * (double)(y + 4716));
    int i1 = (int)(30.6001 * (double)(m + 1));
    return (double)i0 + (double)i1 + d + (double)b - 1524.5;
}

static bool is_leap_year(int year) {
    if (year % 4 != 0) return false;
    if (year % 100 == 0 && year % 400 != 0) return false;
    return true;
}

static int day_of_year(int year, int month, int day) {
    int feb = is_leap_year(year) ? 29 : 28;
    const int months[12] = { 31, feb, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int doy = 0;
    for (int i = 0; i < month - 1 && i < 12; i++) {
        doy += months[i];
    }
    return doy + day;
}

static int get_days_in_month(int year, int month) {
    if (month < 1 || month > 12) return 0;
    int feb = is_leap_year(year) ? 29 : 28;
    const int months[12] = { 31, feb, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return months[month - 1];
}

static inline void add_one_day(int year, int month, int day, int* out_y, int* out_m, int* out_d) {
    if (day < 28) {
        *out_y = year;
        *out_m = month;
        *out_d = day + 1;
        return;
    }
    int dim = get_days_in_month(year, month);
    if (day < dim) {
        *out_y = year;
        *out_m = month;
        *out_d = day + 1;
    } else if (month < 12) {
        *out_y = year;
        *out_m = month + 1;
        *out_d = 1;
    } else {
        *out_y = year + 1;
        *out_m = 1;
        *out_d = 1;
    }
}

static void add_days_to_date(int year, int month, int day, int delta_days, int* out_y, int* out_m, int* out_d) {
    /* Convert via Julian Day at 00:00 UTC (Meeus Ch. 7 inverse) */
    double jd = engine_julian_day(year, month, day) + (double)delta_days + 0.5;
    double z = floor(jd);
    double a = z;
    if (z >= 2299161.0) {
        double alpha = floor((z - 1867216.25) / 36524.25);
        a = z + 1.0 + alpha - floor(alpha / 4.0);
    }
    double b = a + 1524.0;
    double c = floor((b - 122.1) / 365.25);
    double d = floor(365.25 * c);
    double e = floor((b - d) / 30.6001);

    *out_d = (int)floor(b - d - floor(30.6001 * e));
    *out_m = (int)((e < 14.0) ? (e - 1.0) : (e - 13.0));
    *out_y = (int)((*out_m > 2) ? (c - 4716.0) : (c - 4715.0));
}

typedef struct {
    double declination_deg;
    double right_ascension_deg;
    double apparent_sidereal_time_deg;
} solar_coords_t;

/*
 * Meeus Chapters 12, 22, 25: SolarCoordinates
 * Computes declination, right ascension, and apparent sidereal time for a given Julian Day.
 */
static solar_coords_t compute_solar_coordinates(double jd) {
    /* Meeus p. 163: Julian century from epoch J2000.0 */
    double t = (jd - 2451545.0) / 36525.0;

    /* Meeus p. 163: Geometric mean solar longitude L0 */
    double l0 = unwind_angle(280.4664567 + 36000.76983 * t + 0.0003032 * t * t);

    /* Meeus p. 144: Geometric mean lunar longitude Lp */
    double lp = unwind_angle(218.3165 + 481267.8813 * t);

    /* Meeus p. 144: Ascending lunar node longitude Omega */
    double omega = unwind_angle(125.04452 - 1934.136261 * t + 0.0020708 * t * t + (t * t * t) / 450000.0);

    /* Meeus p. 163: Mean solar anomaly M */
    double m_deg = unwind_angle(357.52911 + 35999.05029 * t - 0.0001537 * t * t);
    double m_rad = deg2rad(m_deg);
    double sin_m, cos_m;
    FAST_SINCOS(m_rad, &sin_m, &cos_m);

    /* Meeus p. 164: Sun's equation of the center C (via double & triple angle identities) */
    double sin_2m = 2.0 * sin_m * cos_m;
    double sin_3m = sin_m * (3.0 - 4.0 * sin_m * sin_m);
    double c = (1.914602 - 0.004817 * t - 0.000014 * t * t) * sin_m +
               (0.019993 - 0.000101 * t) * sin_2m +
               0.000289 * sin_3m;

    /* Meeus p. 164: Apparent solar longitude Lambda & obliquity correction */
    double omega_simple = 125.04 - 1934.136 * t;
    double sin_om_simple, cos_om_simple;
    FAST_SINCOS(deg2rad(omega_simple), &sin_om_simple, &cos_om_simple);

    double lambda_deg = unwind_angle(l0 + c - 0.00569 - 0.00478 * sin_om_simple);
    double lambda_rad = deg2rad(lambda_deg);

    /* Meeus p. 165: Mean sidereal time Theta0 */
    double theta0 = unwind_angle(280.46061837 + 360.98564736629 * (jd - 2451545.0) +
                                 0.000387933 * t * t - (t * t * t) / 38710000.0);

    /* Meeus p. 144: Nutation in longitude dPsi and obliquity dEpsilon */
    double l0_rad = deg2rad(l0);
    double lp_rad = deg2rad(lp);
    double om_rad = deg2rad(omega);

    double sin_om, cos_om;
    FAST_SINCOS(om_rad, &sin_om, &cos_om);

    double sin_2l0, cos_2l0;
    FAST_SINCOS(2.0 * l0_rad, &sin_2l0, &cos_2l0);

    double sin_2lp, cos_2lp;
    FAST_SINCOS(2.0 * lp_rad, &sin_2lp, &cos_2lp);

    double sin_2om = 2.0 * sin_om * cos_om;
    double cos_2om = cos_om * cos_om - sin_om * sin_om;

    double d_psi = (-17.2 / 3600.0) * sin_om -
                   (1.32 / 3600.0) * sin_2l0 -
                   (0.23 / 3600.0) * sin_2lp +
                   (0.21 / 3600.0) * sin_2om;

    double d_eps = (9.2 / 3600.0) * cos_om +
                   (0.57 / 3600.0) * cos_2l0 +
                   (0.10 / 3600.0) * cos_2lp -
                   (0.09 / 3600.0) * cos_2om;

    /* Meeus p. 147 & p. 165: Mean and apparent obliquity of the ecliptic */
    double eps0 = 23.439291 - 0.013004167 * t - 0.0000001639 * t * t + 0.0000005036 * t * t * t;
    double eps_app_rad = deg2rad(eps0 + 0.00256 * cos_om_simple);

    double sin_eps_app, cos_eps_app;
    FAST_SINCOS(eps_app_rad, &sin_eps_app, &cos_eps_app);

    double sin_lambda, cos_lambda;
    FAST_SINCOS(lambda_rad, &sin_lambda, &cos_lambda);

    solar_coords_t sc;
    /* Meeus p. 165: Declination and Right Ascension */
    sc.declination_deg = rad2deg(asin(sin_eps_app * sin_lambda));
    sc.right_ascension_deg = unwind_angle(rad2deg(atan2(cos_eps_app * sin_lambda, cos_lambda)));
    /* Meeus p. 88: Apparent sidereal time */
    sc.apparent_sidereal_time_deg = theta0 + (d_psi * 3600.0 * cos(deg2rad(eps0 + d_eps))) / 3600.0;
    return sc;
}

void engine_solar_coordinates(double jd, double* out_decl_deg, double* out_eot_min) {
    solar_coords_t sc = compute_solar_coordinates(jd);
    if (out_decl_deg) {
        *out_decl_deg = sc.declination_deg;
    }
    if (out_eot_min) {
        double t = (jd - 2451545.0) / 36525.0;
        double l0 = unwind_angle(280.4664567 + 36000.76983 * t + 0.0003032 * t * t);
        double delta_deg = quadrant_shift_angle(l0 - sc.right_ascension_deg);
        *out_eot_min = delta_deg * 4.0;
    }
}

/* Meeus p. 93: Altitude of celestial body */
static inline double altitude_of_celestial_body(double sin_phi, double cos_phi, double dec_deg, double lha_deg) {
    double delta = deg2rad(dec_deg);
    double h = deg2rad(lha_deg);
    return rad2deg(asin(sin_phi * sin(delta) + cos_phi * cos(delta) * cos(h)));
}

typedef struct {
    double lat_deg;
    double lng_deg;
    double sin_lat;
    double cos_lat;
    double sin_dec;
    double cos_dec;
    double sin_lat_sin_dec;
    double cos_lat_cos_dec;
    solar_coords_t solar;
    solar_coords_t next_solar;
    /* Precomputed interpolation deltas for Right Ascension and Declination */
    double ra_sum;   /* unwind_angle(a2 - a1) + unwind_angle(a3 - a2) */
    double ra_diff;  /* unwind_angle(a3 - a2) - unwind_angle(a2 - a1) */
    double dec_sum;  /* (d2 - d1) + (d3 - d2) */
    double dec_diff; /* (d3 - d2) - (d2 - d1) */
    double approx_transit;
    double transit_hours;
    double sunrise_hours;
    double sunset_hours;
} solar_time_t;

/* Meeus p. 102: Corrected transit in universal time (hours) */
static double corrected_transit(double m0, const solar_time_t* st) {
    double lw = -st->lng_deg;
    double theta = unwind_angle(st->solar.apparent_sidereal_time_deg + 360.985647 * m0);
    double a = unwind_angle(st->solar.right_ascension_deg + (m0 / 2.0) * (st->ra_sum + m0 * st->ra_diff));
    double h = quadrant_shift_angle(theta - lw - a);
    double dm = h / -360.0;
    return (m0 + dm) * 24.0;
}

/* Meeus p. 102: Corrected hour angle evaluation given transit offset m */
static inline double corrected_hour_angle_from_m(double m, double h0_deg, const solar_time_t* st) {
    double lw = -st->lng_deg;
    double theta = unwind_angle(st->solar.apparent_sidereal_time_deg + 360.985647 * m);
    double a = unwind_angle(st->solar.right_ascension_deg + (m / 2.0) * (st->ra_sum + m * st->ra_diff));
    double delta = st->solar.declination_deg + (m / 2.0) * (st->dec_sum + m * st->dec_diff);
    double h = theta - lw - a;
    double alt = altitude_of_celestial_body(st->sin_lat, st->cos_lat, delta, h);
    double term3 = alt - h0_deg;
    double term4 = 360.0 * cos(deg2rad(delta)) * st->cos_lat * sin(deg2rad(h));
    if (fabs(term4) < 1e-12) return NAN;
    double dm = term3 / term4;
    return (m + dm) * 24.0;
}

/* Meeus p. 102: Corrected hour angle in universal time (hours) */
static double corrected_hour_angle(double m0, double h0_deg, const solar_time_t* st, bool after_transit) {
    double term1 = sin(deg2rad(h0_deg)) - st->sin_lat_sin_dec;
    double ratio = term1 / st->cos_lat_cos_dec;
    if (ratio < -1.0 || ratio > 1.0) {
        return NAN;
    }

    double h0_angle = rad2deg(acos(ratio));
    double m = after_transit ? (m0 + h0_angle / 360.0) : (m0 - h0_angle / 360.0);
    return corrected_hour_angle_from_m(m, h0_deg, st);
}

static void init_solar_time_with_coords(
    solar_time_t* st,
    double lat_deg, double lng_deg,
    const solar_coords_t* prev, const solar_coords_t* curr, const solar_coords_t* next
) {
    st->lat_deg = lat_deg;
    st->lng_deg = lng_deg;
    double phi = deg2rad(lat_deg);
    FAST_SINCOS(phi, &st->sin_lat, &st->cos_lat);
    double d2_rad = deg2rad(curr->declination_deg);
    FAST_SINCOS(d2_rad, &st->sin_dec, &st->cos_dec);
    st->sin_lat_sin_dec = st->sin_lat * st->sin_dec;
    st->cos_lat_cos_dec = st->cos_lat * st->cos_dec;
    st->solar = *curr;
    st->next_solar = *next;

    double ra_a = unwind_angle(curr->right_ascension_deg - prev->right_ascension_deg);
    double ra_b = unwind_angle(next->right_ascension_deg - curr->right_ascension_deg);
    st->ra_diff = ra_b - ra_a;
    st->ra_sum = ra_a + ra_b;

    double dec_a = curr->declination_deg - prev->declination_deg;
    double dec_b = next->declination_deg - curr->declination_deg;
    st->dec_diff = dec_b - dec_a;
    st->dec_sum = dec_a + dec_b;

    double lw = -lng_deg;
    st->approx_transit = normalize_to_scale((st->solar.right_ascension_deg + lw - st->solar.apparent_sidereal_time_deg) / 360.0, 1.0);
    st->transit_hours = corrected_transit(st->approx_transit, st);

    double term1 = sin(deg2rad(SOLAR_ALTITUDE_DEG)) - st->sin_lat_sin_dec;
    double ratio = term1 / st->cos_lat_cos_dec;
    if (ratio < -1.0 || ratio > 1.0) {
        st->sunrise_hours = NAN;
        st->sunset_hours = NAN;
    } else {
        double h0_angle = rad2deg(acos(ratio));
        double delta_m = h0_angle / 360.0;
        st->sunrise_hours = corrected_hour_angle_from_m(st->approx_transit - delta_m, SOLAR_ALTITUDE_DEG, st);
        st->sunset_hours = corrected_hour_angle_from_m(st->approx_transit + delta_m, SOLAR_ALTITUDE_DEG, st);
    }
}

static void init_solar_time(solar_time_t* st, double jd, double lat_deg, double lng_deg) {
    solar_coords_t prev = compute_solar_coordinates(jd - 1.0);
    solar_coords_t curr = compute_solar_coordinates(jd);
    solar_coords_t next = compute_solar_coordinates(jd + 1.0);
    init_solar_time_with_coords(st, lat_deg, lng_deg, &prev, &curr, &next);
}

static double solar_time_hour_angle(const solar_time_t* st, double angle_deg, bool after_transit) {
    return corrected_hour_angle(st->approx_transit, angle_deg, st, after_transit);
}

static double solar_time_afternoon(const solar_time_t* st, double shadow_length) {
    double tangent = fabs(st->lat_deg - st->solar.declination_deg);
    double inverse = shadow_length + tan(deg2rad(tangent));
    double angle_deg = rad2deg(atan(1.0 / inverse));
    return solar_time_hour_angle(st, angle_deg, true);
}

/*
 * Convert fractional hours into integer-second truncated seconds from 00:00:00 UTC,
 * matching adhan-js TimeComponents:
 *   hours = floor(num)
 *   minutes = floor((num - hours) * 60)
 *   seconds = floor((num - (hours + minutes / 60)) * 3600)
 */
static double hours_to_truncated_seconds(double hours) {
    if (isnan(hours) || isinf(hours)) return NAN;
    double h = floor(hours);
    double m = floor((hours - h) * 60.0);
    double s = floor((hours - (h + m / 60.0)) * 3600.0);
    return h * 3600.0 + m * 60.0 + s;
}

/*
 * Round seconds from 00:00:00 UTC according to Rounding rule, matching adhan-js DateUtils.roundedMinute
 */
static double round_seconds_to_minutes(double total_seconds, rounding_rule_t rounding) {
    if (isnan(total_seconds)) return NAN;
    /* Extract positive modulo 60 seconds */
    double sec = fmod(total_seconds, 60.0);
    if (sec < 0.0) sec += 60.0;
    /* Round sec to nearest integer to avoid floating point representation drift */
    int sec_int = (int)llround(sec);
    if (sec_int == 60) sec_int = 0;

    double offset = (sec_int >= 30) ? (double)(60 - sec_int) : (double)(-sec_int);
    if (rounding == ROUNDING_UP) {
        offset = (double)(60 - sec_int);
    } else if (rounding == ROUNDING_NONE) {
        offset = 0.0;
    }
    return (total_seconds + offset) / 60.0;
}

/* Moonsighting Committee seasonal twilight helpers (Astronomical.js lines 265-354) */
static int days_since_solstice(int doy, int year, double latitude) {
    int northern_offset = 10;
    int southern_offset = is_leap_year(year) ? 173 : 172;
    int days_in_year = is_leap_year(year) ? 366 : 365;
    int dyy = 0;
    if (latitude >= 0.0) {
        dyy = doy + northern_offset;
        if (dyy >= days_in_year) dyy -= days_in_year;
    } else {
        dyy = doy - southern_offset;
        if (dyy < 0) dyy += days_in_year;
    }
    return dyy;
}

static double interpolate_dyy(int dyy, double a, double b, double c, double d) {
    if (dyy < 91) {
        return a + (b - a) / 91.0 * (double)dyy;
    } else if (dyy < 137) {
        return b + (c - b) / 46.0 * (double)(dyy - 91);
    } else if (dyy < 183) {
        return c + (d - c) / 46.0 * (double)(dyy - 137);
    } else if (dyy < 229) {
        return d + (c - d) / 46.0 * (double)(dyy - 183);
    } else if (dyy < 275) {
        return c + (b - c) / 46.0 * (double)(dyy - 229);
    } else {
        return b + (a - b) / 91.0 * (double)(dyy - 275);
    }
}

static double season_adjusted_morning_twilight_sec(double latitude, int doy, int year, double sunrise_sec) {
    double abs_lat = fabs(latitude);
    double a = 75.0 + (28.65 / 55.0) * abs_lat;
    double b = 75.0 + (19.44 / 55.0) * abs_lat;
    double c = 75.0 + (32.74 / 55.0) * abs_lat;
    double d = 75.0 + (48.10 / 55.0) * abs_lat;
    int dyy = days_since_solstice(doy, year, latitude);
    double adjustment = interpolate_dyy(dyy, a, b, c, d);
    return sunrise_sec + round(adjustment * -60.0);
}

static double season_adjusted_evening_twilight_sec(double latitude, int doy, int year, double sunset_sec, shafaq_t shafaq) {
    double abs_lat = fabs(latitude);
    double a, b, c, d;
    if (shafaq == SHAFAQ_AHMER) {
        a = 62.0 + (17.40 / 55.0) * abs_lat;
        b = 62.0 - (7.16 / 55.0) * abs_lat;
        c = 62.0 + (5.12 / 55.0) * abs_lat;
        d = 62.0 + (19.44 / 55.0) * abs_lat;
    } else if (shafaq == SHAFAQ_ABYAD) {
        a = 75.0 + (25.60 / 55.0) * abs_lat;
        b = 75.0 + (7.16 / 55.0) * abs_lat;
        c = 75.0 + (36.84 / 55.0) * abs_lat;
        d = 75.0 + (81.84 / 55.0) * abs_lat;
    } else {
        a = 75.0 + (25.60 / 55.0) * abs_lat;
        b = 75.0 + (2.05 / 55.0) * abs_lat;
        c = 75.0 - (9.21 / 55.0) * abs_lat;
        d = 75.0 + (6.14 / 55.0) * abs_lat;
    }
    int dyy = days_since_solstice(doy, year, latitude);
    double adjustment = interpolate_dyy(dyy, a, b, c, d);
    return sunset_sec + round(adjustment * 60.0);
}

/* Polar circle resolvers (PolarCircleResolution.js) */
static bool is_valid_solar_time(const solar_time_t* st) {
    return !isnan(st->sunrise_hours) && !isnan(st->sunset_hours);
}

static bool resolve_polar_circle(
    polar_resolution_t resolver,
    int year, int month, int day,
    double lat_deg, double lng_deg,
    solar_time_t* io_solar_time,
    solar_time_t* io_tomorrow_solar_time
) {
    if (resolver == POLAR_AQRAB_YAUM) {
        int max_days = 183; /* ceil(365 / 2) */
        for (int days_added = 1; days_added <= max_days; days_added++) {
            for (int dir_idx = 0; dir_idx < 2; dir_idx++) {
                int direction = (dir_idx == 0) ? 1 : -1;
                int ty, tm, td;
                add_days_to_date(year, month, day, direction * days_added, &ty, &tm, &td);
                int tmy, tmm, tmd;
                add_one_day(ty, tm, td, &tmy, &tmm, &tmd);

                solar_time_t st1, st2;
                init_solar_time(&st1, engine_julian_day(ty, tm, td), lat_deg, lng_deg);
                init_solar_time(&st2, engine_julian_day(tmy, tmm, tmd), lat_deg, lng_deg);
                if (is_valid_solar_time(&st1) && is_valid_solar_time(&st2)) {
                    *io_solar_time = st1;
                    *io_tomorrow_solar_time = st2;
                    return true;
                }
            }
        }
    } else if (resolver == POLAR_AQRAB_BALAD) {
        double sign = (lat_deg >= 0.0) ? 1.0 : -1.0;
        double test_lat = lat_deg - sign * 0.5;
        double jd_day = engine_julian_day(year, month, day);

        while (fabs(test_lat) >= 65.0) {
            solar_time_t st1, st2;
            init_solar_time(&st1, jd_day, test_lat, lng_deg);
            init_solar_time(&st2, jd_day + 1.0, test_lat, lng_deg);
            if (is_valid_solar_time(&st1) && is_valid_solar_time(&st2)) {
                *io_solar_time = st1;
                *io_tomorrow_solar_time = st2;
                return true;
            }
            test_lat -= sign * 0.5;
        }
    }
    return false;
}

void engine_get_default_settings(calc_method_t method, engine_settings_t* out_settings) {
    if (!out_settings) return;

    *out_settings = (engine_settings_t){
        .method = method,
        .madhab = MADHAB_STANDARD,
        .high_lat_rule = HIGH_LAT_MIDDLE_OF_NIGHT,
        .polar_resolution = POLAR_UNRESOLVED,
        .rounding = ROUNDING_NEAREST,
        .shafaq = SHAFAQ_GENERAL,
        .fajr_angle_deg = 0.0,
        .isha_angle_deg = 0.0,
        .isha_interval_min = 0,
        .maghrib_angle_deg = 0.0,
        .method_adjustments = { 0, 0, 0, 0, 0, 0, 0 },
        .user_adjustments = { 0, 0, 0, 0, 0, 0, 0 },
        .sunrise_forbidden_duration_min = 15,
        .zawal_forbidden_duration_min = 10,
        .sunset_forbidden_duration_min = 15
    };

    switch (method) {
    case CALC_METHOD_MWL:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 17.0;
        out_settings->method_adjustments.dhuhr = 1;
        break;
    case CALC_METHOD_EGYPTIAN:
        out_settings->fajr_angle_deg = 19.5;
        out_settings->isha_angle_deg = 17.5;
        out_settings->method_adjustments.dhuhr = 1;
        break;
    case CALC_METHOD_KARACHI:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 18.0;
        out_settings->method_adjustments.dhuhr = 1;
        break;
    case CALC_METHOD_UMM_AL_QURA:
        out_settings->fajr_angle_deg = 18.5;
        out_settings->isha_interval_min = 90;
        break;
    case CALC_METHOD_DUBAI:
        out_settings->fajr_angle_deg = 18.2;
        out_settings->isha_angle_deg = 18.2;
        out_settings->method_adjustments.sunrise = -3;
        out_settings->method_adjustments.dhuhr = 3;
        out_settings->method_adjustments.asr = 3;
        out_settings->method_adjustments.maghrib = 3;
        break;
    case CALC_METHOD_MOONSIGHTING:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 18.0;
        out_settings->method_adjustments.dhuhr = 5;
        out_settings->method_adjustments.maghrib = 3;
        break;
    case CALC_METHOD_NORTH_AMERICA:
        out_settings->fajr_angle_deg = 15.0;
        out_settings->isha_angle_deg = 15.0;
        out_settings->method_adjustments.dhuhr = 1;
        break;
    case CALC_METHOD_KUWAIT:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 17.5;
        break;
    case CALC_METHOD_QATAR:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_interval_min = 90;
        break;
    case CALC_METHOD_SINGAPORE:
        out_settings->fajr_angle_deg = 20.0;
        out_settings->isha_angle_deg = 18.0;
        out_settings->method_adjustments.dhuhr = 1;
        out_settings->rounding = ROUNDING_UP;
        break;
    case CALC_METHOD_TEHRAN:
        out_settings->fajr_angle_deg = 17.7;
        out_settings->isha_angle_deg = 14.0;
        out_settings->maghrib_angle_deg = 4.5;
        break;
    case CALC_METHOD_TURKEY:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 17.0;
        out_settings->method_adjustments.sunrise = -7;
        out_settings->method_adjustments.dhuhr = 5;
        out_settings->method_adjustments.asr = 4;
        out_settings->method_adjustments.maghrib = 7;
        break;
    default:
        break;
    }
}

/* Internal core calculation for a single day (returns times in minutes from 00:00:00 UTC of that day) */
static bool compute_day_prayers(
    int year, int month, int day, double jd,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    const solar_time_t* opt_solar_today,
    solar_time_t* out_solar_today,
    solar_time_t* out_solar_tomorrow,
    double* out_fajr_min,
    double* out_sunrise_min,
    double* out_dhuhr_min,
    double* out_asr_min,
    double* out_sunset_min,
    double* out_maghrib_min,
    double* out_isha_min
) {
    solar_time_t solar_time;
    if (opt_solar_today) {
        solar_time = *opt_solar_today;
    } else {
        init_solar_time(&solar_time, jd, lat_deg, lng_deg);
    }

    double fajr_sec = NAN;
    if (out_fajr_min) {
        fajr_sec = hours_to_truncated_seconds(solar_time_hour_angle(&solar_time, -settings->fajr_angle_deg, false));
    }

    bool need_tomorrow = (out_solar_tomorrow != NULL) ||
                         (out_isha_min != NULL && settings->isha_interval_min == 0) ||
                         (out_fajr_min != NULL && (
                             isnan(fajr_sec) ||
                             (settings->method == CALC_METHOD_MOONSIGHTING && lat_deg >= 55.0) ||
                             (settings->method != CALC_METHOD_MOONSIGHTING && settings->high_lat_rule != HIGH_LAT_MIDDLE_OF_NIGHT)
                         ));

    solar_time_t tomorrow_solar_time;
    if (need_tomorrow) {
        solar_coords_t next_tomorrow = compute_solar_coordinates(jd + 2.0);
        init_solar_time_with_coords(
            &tomorrow_solar_time,
            lat_deg, lng_deg,
            &solar_time.solar,
            &solar_time.next_solar,
            &next_tomorrow
        );

        if ((isnan(solar_time.sunrise_hours) || isnan(solar_time.sunset_hours) || isnan(tomorrow_solar_time.sunrise_hours)) &&
            settings->polar_resolution != POLAR_UNRESOLVED) {
            resolve_polar_circle(settings->polar_resolution, year, month, day, lat_deg, lng_deg, &solar_time, &tomorrow_solar_time);
        }

        if (out_solar_tomorrow) *out_solar_tomorrow = tomorrow_solar_time;
    } else {
        if ((isnan(solar_time.sunrise_hours) || isnan(solar_time.sunset_hours)) &&
            settings->polar_resolution != POLAR_UNRESOLVED) {
            solar_time_t dummy_tomorrow;
            resolve_polar_circle(settings->polar_resolution, year, month, day, lat_deg, lng_deg, &solar_time, &dummy_tomorrow);
        }
    }

    if (out_solar_today) *out_solar_today = solar_time;

    double dhuhr_sec = NAN;
    if (out_dhuhr_min) {
        dhuhr_sec = hours_to_truncated_seconds(solar_time.transit_hours);
    }

    double sunrise_sec = NAN;
    if (out_sunrise_min || out_fajr_min) {
        sunrise_sec = hours_to_truncated_seconds(solar_time.sunrise_hours);
    }

    double sunset_sec = NAN;
    if (out_sunset_min || out_maghrib_min || out_isha_min || need_tomorrow) {
        sunset_sec = hours_to_truncated_seconds(solar_time.sunset_hours);
    }

    double asr_sec = NAN;
    if (out_asr_min) {
        double shadow_factor = (settings->madhab == MADHAB_HANAFI) ? 2.0 : 1.0;
        asr_sec = hours_to_truncated_seconds(solar_time_afternoon(&solar_time, shadow_factor));
    }

    double night_sec = NAN;
    if (need_tomorrow) {
        double tomorrow_sunrise_sec = hours_to_truncated_seconds(tomorrow_solar_time.sunrise_hours) + 86400.0;
        night_sec = tomorrow_sunrise_sec - sunset_sec;
    }

    int doy = (out_fajr_min || out_isha_min) ? day_of_year(year, month, day) : 0;

    /* Finalize Fajr */
    if (out_fajr_min) {
        if (settings->method == CALC_METHOD_MOONSIGHTING && lat_deg >= 55.0 && !isnan(sunrise_sec) && !isnan(night_sec)) {
            fajr_sec = sunrise_sec - (night_sec / 7.0);
        }

        double safe_fajr_sec = NAN;
        if (!isnan(sunrise_sec)) {
            if (settings->method == CALC_METHOD_MOONSIGHTING) {
                safe_fajr_sec = season_adjusted_morning_twilight_sec(lat_deg, doy, year, sunrise_sec);
            } else if (!isnan(night_sec)) {
                double fajr_portion = 0.5;
                if (settings->high_lat_rule == HIGH_LAT_SEVENTH_OF_NIGHT) {
                    fajr_portion = 1.0 / 7.0;
                } else if (settings->high_lat_rule == HIGH_LAT_TWILIGHT_ANGLE) {
                    fajr_portion = settings->fajr_angle_deg / 60.0;
                }
                safe_fajr_sec = sunrise_sec - (fajr_portion * night_sec);
            }
        }
        if (isnan(fajr_sec) || (!isnan(safe_fajr_sec) && safe_fajr_sec > fajr_sec)) {
            fajr_sec = safe_fajr_sec;
        }
    }

    /* Finalize Isha */
    double isha_sec = NAN;
    if (out_isha_min) {
        if (settings->isha_interval_min > 0) {
            if (!isnan(sunset_sec)) {
                isha_sec = sunset_sec + (double)settings->isha_interval_min * 60.0;
            }
        } else {
            isha_sec = hours_to_truncated_seconds(solar_time_hour_angle(&solar_time, -settings->isha_angle_deg, true));
            if (settings->method == CALC_METHOD_MOONSIGHTING && lat_deg >= 55.0 && !isnan(sunset_sec) && !isnan(night_sec)) {
                isha_sec = sunset_sec + (night_sec / 7.0);
            }

            double safe_isha_sec = NAN;
            if (!isnan(sunset_sec)) {
                if (settings->method == CALC_METHOD_MOONSIGHTING) {
                    safe_isha_sec = season_adjusted_evening_twilight_sec(lat_deg, doy, year, sunset_sec, settings->shafaq);
                } else if (!isnan(night_sec)) {
                    double isha_portion = 0.5;
                    if (settings->high_lat_rule == HIGH_LAT_SEVENTH_OF_NIGHT) {
                        isha_portion = 1.0 / 7.0;
                    } else if (settings->high_lat_rule == HIGH_LAT_TWILIGHT_ANGLE) {
                        isha_portion = settings->isha_angle_deg / 60.0;
                    }
                    safe_isha_sec = sunset_sec + (isha_portion * night_sec);
                }
            }
            if (isnan(isha_sec) || (!isnan(safe_isha_sec) && safe_isha_sec < isha_sec)) {
                isha_sec = safe_isha_sec;
            }
        }
    }

    /* Finalize Maghrib */
    double maghrib_sec = NAN;
    if (out_maghrib_min) {
        maghrib_sec = sunset_sec;
        if (settings->maghrib_angle_deg > 0.0) {
            double angle_maghrib_sec = hours_to_truncated_seconds(solar_time_hour_angle(&solar_time, -settings->maghrib_angle_deg, true));
            if (!isnan(angle_maghrib_sec) && !isnan(sunset_sec) && !isnan(isha_sec)) {
                if (sunset_sec < angle_maghrib_sec && isha_sec > angle_maghrib_sec) {
                    maghrib_sec = angle_maghrib_sec;
                }
            }
        }
    }

    if (out_fajr_min) {
        double fajr_adj_sec = (double)(settings->method_adjustments.fajr + settings->user_adjustments.fajr) * 60.0;
        *out_fajr_min = round_seconds_to_minutes(fajr_sec + fajr_adj_sec, settings->rounding);
    }
    if (out_sunrise_min) {
        double sunrise_adj_sec = (double)(settings->method_adjustments.sunrise + settings->user_adjustments.sunrise) * 60.0;
        *out_sunrise_min = round_seconds_to_minutes(sunrise_sec + sunrise_adj_sec, settings->rounding);
    }
    if (out_dhuhr_min) {
        double dhuhr_adj_sec = (double)(settings->method_adjustments.dhuhr + settings->user_adjustments.dhuhr) * 60.0;
        *out_dhuhr_min = round_seconds_to_minutes(dhuhr_sec + dhuhr_adj_sec, settings->rounding);
    }
    if (out_asr_min) {
        double asr_adj_sec = (double)(settings->method_adjustments.asr + settings->user_adjustments.asr) * 60.0;
        *out_asr_min = round_seconds_to_minutes(asr_sec + asr_adj_sec, settings->rounding);
    }
    if (out_sunset_min) {
        double sunset_adj_sec = (double)(settings->method_adjustments.sunset + settings->user_adjustments.sunset) * 60.0;
        *out_sunset_min = round_seconds_to_minutes(sunset_sec + sunset_adj_sec, settings->rounding);
    }
    if (out_maghrib_min) {
        double maghrib_adj_sec = (double)(settings->method_adjustments.maghrib + settings->user_adjustments.maghrib) * 60.0;
        *out_maghrib_min = round_seconds_to_minutes(maghrib_sec + maghrib_adj_sec, settings->rounding);
    }
    if (out_isha_min) {
        double isha_adj_sec = (double)(settings->method_adjustments.isha + settings->user_adjustments.isha) * 60.0;
        *out_isha_min = round_seconds_to_minutes(isha_sec + isha_adj_sec, settings->rounding);
    }

    return true;
}

static void finalize_sunnah_and_forbidden(
    const engine_settings_t* settings,
    double tomorrow_fajr_min,
    prayer_times_t* out_times
) {
    if (!isnan(out_times->maghrib_utc_min) && !isnan(tomorrow_fajr_min)) {
        /* Tomorrow's Fajr relative to today's 00:00:00 UTC is +1440 minutes */
        double next_fajr_rel_min = tomorrow_fajr_min + 1440.0;
        double night_duration_sec = (next_fajr_rel_min - out_times->maghrib_utc_min) * 60.0;
        double maghrib_sec = out_times->maghrib_utc_min * 60.0;

        out_times->midnight_utc_min = round_seconds_to_minutes(maghrib_sec + night_duration_sec / 2.0, ROUNDING_NEAREST);
        out_times->first_third_utc_min = round_seconds_to_minutes(maghrib_sec + night_duration_sec / 3.0, ROUNDING_NEAREST);
        out_times->last_third_utc_min = round_seconds_to_minutes(maghrib_sec + night_duration_sec * (2.0 / 3.0), ROUNDING_NEAREST);
    } else {
        out_times->midnight_utc_min = NAN;
        out_times->first_third_utc_min = NAN;
        out_times->last_third_utc_min = NAN;
    }

    /* Configurable Forbidden (Makruh) Windows */
    int sr_dur = (settings->sunrise_forbidden_duration_min > 0) ? settings->sunrise_forbidden_duration_min : 15;
    int zw_dur = (settings->zawal_forbidden_duration_min > 0) ? settings->zawal_forbidden_duration_min : 10;
    int ss_dur = (settings->sunset_forbidden_duration_min > 0) ? settings->sunset_forbidden_duration_min : 15;

    if (!isnan(out_times->sunrise_utc_min)) {
        out_times->sunrise_forbidden_start_utc_min = out_times->sunrise_utc_min;
        out_times->sunrise_forbidden_end_utc_min = out_times->sunrise_utc_min + (double)sr_dur;
    } else {
        out_times->sunrise_forbidden_start_utc_min = NAN;
        out_times->sunrise_forbidden_end_utc_min = NAN;
    }

    if (!isnan(out_times->dhuhr_utc_min)) {
        out_times->zawal_forbidden_start_utc_min = out_times->dhuhr_utc_min - (double)zw_dur;
        out_times->zawal_forbidden_end_utc_min = out_times->dhuhr_utc_min;
    } else {
        out_times->zawal_forbidden_start_utc_min = NAN;
        out_times->zawal_forbidden_end_utc_min = NAN;
    }

    if (!isnan(out_times->sunset_utc_min)) {
        out_times->sunset_forbidden_start_utc_min = out_times->sunset_utc_min - (double)ss_dur;
        out_times->sunset_forbidden_end_utc_min = out_times->sunset_utc_min;
    } else {
        out_times->sunset_forbidden_start_utc_min = NAN;
        out_times->sunset_forbidden_end_utc_min = NAN;
    }
}

bool engine_calculate_times(
    int year, int month, int day,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    prayer_times_t* out_times
) {
    if (!settings || !out_times) return false;
    if (lat_deg < -90.0 || lat_deg > 90.0 || lng_deg < -180.0 || lng_deg > 180.0) return false;

    double jd = engine_julian_day(year, month, day);
    solar_time_t tomorrow_solar;
    compute_day_prayers(
        year, month, day, jd, lat_deg, lng_deg, settings,
        NULL,
        NULL,
        &tomorrow_solar,
        &out_times->fajr_utc_min,
        &out_times->sunrise_utc_min,
        &out_times->dhuhr_utc_min,
        &out_times->asr_utc_min,
        &out_times->sunset_utc_min,
        &out_times->maghrib_utc_min,
        &out_times->isha_utc_min
    );

    /*
     * SunnahTimes (Midnight, First Third, Last Third of the night):
     * Night is defined from today's Maghrib to tomorrow's Fajr.
     */
    int tmy, tmm, tmd;
    add_one_day(year, month, day, &tmy, &tmm, &tmd);
    double tomorrow_fajr_min = NAN;
    compute_day_prayers(
        tmy, tmm, tmd, jd + 1.0, lat_deg, lng_deg, settings,
        &tomorrow_solar,
        NULL,
        NULL,
        &tomorrow_fajr_min, NULL, NULL, NULL, NULL, NULL, NULL
    );

    finalize_sunnah_and_forbidden(settings, tomorrow_fajr_min, out_times);

    return true;
}

bool engine_calculate_month(
    int year, int month,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    prayer_times_t* out_times,
    int* out_day_count
) {
    if (!settings || !out_times) return false;
    if (month < 1 || month > 12) return false;
    if (lat_deg < -90.0 || lat_deg > 90.0 || lng_deg < -180.0 || lng_deg > 180.0) return false;

    int num_days = get_days_in_month(year, month);
    if (out_day_count) {
        *out_day_count = num_days;
    }

    solar_time_t tomorrow_solar;
    double jd_start = engine_julian_day(year, month, 1);

    for (int day = 1; day <= num_days; day++) {
        prayer_times_t* curr = &out_times[day - 1];
        const solar_time_t* opt_solar = (day == 1) ? NULL : &tomorrow_solar;
        double jd = jd_start + (double)(day - 1);

        compute_day_prayers(
            year, month, day, jd, lat_deg, lng_deg, settings,
            opt_solar,
            NULL,
            &tomorrow_solar,
            &curr->fajr_utc_min,
            &curr->sunrise_utc_min,
            &curr->dhuhr_utc_min,
            &curr->asr_utc_min,
            &curr->sunset_utc_min,
            &curr->maghrib_utc_min,
            &curr->isha_utc_min
        );
    }

    for (int day = 1; day < num_days; day++) {
        finalize_sunnah_and_forbidden(settings, out_times[day].fajr_utc_min, &out_times[day - 1]);
    }

    /* Last day of month: tomorrow is day 1 of next month */
    int tmy, tmm, tmd;
    add_one_day(year, month, num_days, &tmy, &tmm, &tmd);
    double next_month_fajr = NAN;
    compute_day_prayers(
        tmy, tmm, tmd, jd_start + (double)num_days, lat_deg, lng_deg, settings,
        &tomorrow_solar,
        NULL,
        NULL,
        &next_month_fajr, NULL, NULL, NULL, NULL, NULL, NULL
    );
    finalize_sunnah_and_forbidden(settings, next_month_fajr, &out_times[num_days - 1]);

    return true;
}

int64_t engine_utc_minutes_to_epoch_ms(int year, int month, int day, double utc_min) {
    if (isnan(utc_min)) return -1;
    /* Julian Day of 1970-01-01 00:00:00 UTC is 2440587.5 */
    double jd_day_start = engine_julian_day(year, month, day);
    int64_t day_epoch_ms = (int64_t)llround((jd_day_start - 2440587.5) * 86400000.0);
    int64_t offset_ms = (int64_t)llround(utc_min * 60000.0);
    return day_epoch_ms + offset_ms;
}

/* Tabular Islamic Calendar (30-year cycle algorithm) */
void engine_get_hijri_date(int year, int month, int day, int adjustment_days, hijri_date_t* out_hijri) {
    if (!out_hijri) return;

    static const char* MONTH_NAMES_EN[12] = {
        "Muharram", "Safar", "Rabi' al-Awwal", "Rabi' al-Thani",
        "Jumada al-Ula", "Jumada al-Akhirah", "Rajab", "Sha'ban",
        "Ramadan", "Shawwal", "Dhu al-Qi'dah", "Dhu al-Hijjah"
    };

    static const char* MONTH_NAMES_AR[12] = {
        "محرم", "صفر", "ربيع الأول", "ربيع الثاني",
        "جمادى الأولى", "جمادى الآخرة", "رجب", "شعبان",
        "رمضان", "شوال", "ذو القعدة", "ذو الحجة"
    };

    /* engine_julian_day returns JD at 00:00 UTC (ending in .5); add 0.5 to get integer chronological day number */
    double jd_noon = engine_julian_day(year, month, day) + 0.5 + (double)adjustment_days;

    double l = floor(jd_noon) - 1948440.0 + 10632.0;
    double n = floor((l - 1.0) / 10631.0);
    l = l - 10631.0 * n + 354.0;
    double j = (floor((10985.0 - l) / 5316.0)) * (floor((50.0 * l) / 17719.0)) +
               (floor(l / 5670.0)) * (floor((43.0 * l) / 15238.0));
    l = l - (floor((30.0 - j) / 15.0)) * (floor((17719.0 * j) / 50.0)) -
        (floor(j / 16.0)) * (floor((15238.0 * j) / 43.0)) + 29.0;

    int h_month = (int)floor((24.0 * l) / 709.0);
    int h_day = (int)(l - floor((709.0 * (double)h_month) / 24.0));
    int h_year = (int)(30.0 * n + j - 30.0);

    if (h_month < 1) h_month = 1;
    if (h_month > 12) h_month = 12;

    out_hijri->year = h_year;
    out_hijri->month = h_month;
    out_hijri->day = h_day;
    out_hijri->month_name_en = MONTH_NAMES_EN[h_month - 1];
    out_hijri->month_name_ar = MONTH_NAMES_AR[h_month - 1];
}
