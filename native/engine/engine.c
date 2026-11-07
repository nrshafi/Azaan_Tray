/*
 * Azaan Tray - Core Prayer Calculation Engine
 * Pure C11, no OS headers, libm only.
 *
 * Formulas cite:
 * - Jean Meeus, "Astronomical Algorithms", 2nd Edition (1998)
 * - PrayTimes.org Calculation Methods
 * - Adhan JS reference algorithms
 */

#include "engine.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Standard sun altitude at sunrise/sunset including refraction (-34') and semi-diameter (-16') */
static const double SUNRISE_SUNSET_ALTITUDE_DEG = -0.8333;

static inline double deg2rad(double deg) {
    return deg * (M_PI / 180.0);
}

static inline double rad2deg(double rad) {
    return rad * (180.0 / M_PI);
}

static inline double unwind_angle(double angle_deg) {
    double a = fmod(angle_deg, 360.0);
    if (a < 0.0) a += 360.0;
    return a;
}

static inline double normalize_minutes(double minutes) {
    double m = fmod(minutes, 1440.0);
    if (m < 0.0) m += 1440.0;
    return m;
}

/*
 * Meeus Chapter 7: Julian Day
 * Calculates JD for Gregorian date Y-M-D at 12:00:00 UTC.
 */
double engine_julian_day(int year, int month, int day) {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    double a = floor((double)year / 100.0);
    double b = 2.0 - a + floor(a / 4.0);
    return floor(365.25 * ((double)year + 4716.0)) +
           floor(30.6001 * ((double)month + 1.0)) +
           ((double)day + 0.5) + b - 1524.5;
}

/*
 * Meeus Chapter 25 & 28: Solar Coordinates & Equation of Time
 */
void engine_solar_coordinates(double jd, double* out_declination_deg, double* out_equation_of_time_min) {
    double t = (jd - 2451545.0) / 36525.0;

    /* Geometric mean longitude of the Sun (degrees) */
    double l0 = 280.46646 + 36000.76983 * t + 0.0003032 * t * t;
    l0 = unwind_angle(l0);

    /* Mean anomaly of the Sun (degrees) */
    double m = 357.52911 + 35999.05029 * t - 0.0001537 * t * t;
    m = unwind_angle(m);
    double m_rad = deg2rad(m);

    /* Sun's equation of center (degrees) */
    double c = (1.914602 - 0.004817 * t - 0.000014 * t * t) * sin(m_rad) +
               (0.019993 - 0.000101 * t) * sin(2.0 * m_rad) +
               0.000289 * sin(3.0 * m_rad);

    /* True longitude of the Sun */
    double true_long = l0 + c;

    /* Apparent longitude */
    double omega = 125.04 - 1934.136 * t;
    double lambda = true_long - 0.00569 - 0.00478 * sin(deg2rad(omega));
    double lambda_rad = deg2rad(lambda);

    /* Mean obliquity of the ecliptic */
    double u = t / 100.0;
    double eps0 = 23.0 + (26.0 + (21.448 - 4680.93 * u - 1.55 * u * u + 1999.25 * u * u * u) / 60.0) / 60.0;
    double eps = eps0 + 0.00256 * cos(deg2rad(omega));
    double eps_rad = deg2rad(eps);

    /* Declination delta: sin(delta) = sin(eps) * sin(lambda) */
    double delta_rad = asin(sin(eps_rad) * sin(lambda_rad));
    if (out_declination_deg) {
        *out_declination_deg = rad2deg(delta_rad);
    }

    /* Equation of time (Meeus Chapter 28) */
    double y = tan(eps_rad / 2.0);
    y *= y;
    double l0_rad = deg2rad(l0);
    double e = 0.016708634 - 0.000042037 * t - 0.0000001267 * t * t;

    double eot_rad = y * sin(2.0 * l0_rad) -
                     2.0 * e * sin(m_rad) +
                     4.0 * e * y * sin(m_rad) * cos(2.0 * l0_rad) -
                     0.5 * y * y * sin(4.0 * l0_rad) -
                     1.25 * e * e * sin(2.0 * m_rad);

    /* In minutes of time: 4 minutes per degree */
    if (out_equation_of_time_min) {
        *out_equation_of_time_min = rad2deg(eot_rad) * 4.0;
    }
}

/*
 * Calculate hour angle for a target altitude (degrees).
 * Returns hour angle in degrees [0, 180], or NAN if unreachable.
 */
static double calculate_hour_angle(double lat_deg, double declination_deg, double altitude_deg) {
    double phi = deg2rad(lat_deg);
    double delta = deg2rad(declination_deg);
    double alpha = deg2rad(altitude_deg);

    double cos_h = (sin(alpha) - sin(phi) * sin(delta)) / (cos(phi) * cos(delta));
    if (cos_h < -1.0 || cos_h > 1.0) {
        return NAN;
    }
    return rad2deg(acos(cos_h));
}

/*
 * Calculate Asr altitude for Shafi (factor 1) or Hanafi (factor 2).
 */
static double calculate_asr_altitude(double lat_deg, double declination_deg, madhab_t madhab) {
    double phi = deg2rad(lat_deg);
    double delta = deg2rad(declination_deg);
    double factor = (madhab == MADHAB_HANAFI) ? 2.0 : 1.0;

    double diff = fabs(phi - delta);
    double cot_alt = factor + tan(diff);
    return rad2deg(atan(1.0 / cot_alt));
}

void engine_get_default_settings(calc_method_t method, engine_settings_t* out_settings) {
    if (!out_settings) return;

    *out_settings = (engine_settings_t){
        .method = method,
        .madhab = MADHAB_STANDARD,
        .high_lat_rule = HIGH_LAT_NONE,
        .fajr_angle_deg = 18.0,
        .isha_angle_deg = 17.0,
        .isha_interval_min = 0,
        .maghrib_interval_min = 0,
        .fajr_offset_min = 0,
        .sunrise_offset_min = 0,
        .dhuhr_offset_min = 0,
        .asr_offset_min = 0,
        .sunset_offset_min = 0,
        .maghrib_offset_min = 0,
        .isha_offset_min = 0
    };

    switch (method) {
    case CALC_METHOD_MWL:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 17.0;
        break;
    case CALC_METHOD_EGYPTIAN:
        out_settings->fajr_angle_deg = 19.5;
        out_settings->isha_angle_deg = 17.5;
        break;
    case CALC_METHOD_KARACHI:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 18.0;
        break;
    case CALC_METHOD_UMM_AL_QURA:
        out_settings->fajr_angle_deg = 18.5;
        out_settings->isha_interval_min = 90;
        break;
    case CALC_METHOD_DUBAI:
        out_settings->fajr_angle_deg = 18.2;
        out_settings->isha_angle_deg = 18.2;
        break;
    case CALC_METHOD_MOONSIGHTING:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 18.0;
        break;
    case CALC_METHOD_NORTH_AMERICA:
        out_settings->fajr_angle_deg = 15.0;
        out_settings->isha_angle_deg = 15.0;
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
        break;
    case CALC_METHOD_TEHRAN:
        out_settings->fajr_angle_deg = 17.7;
        out_settings->isha_angle_deg = 14.0;
        out_settings->maghrib_interval_min = 0; // 4.5 angle
        break;
    case CALC_METHOD_TURKEY:
        out_settings->fajr_angle_deg = 18.0;
        out_settings->isha_angle_deg = 17.0;
        break;
    default:
        break;
    }
}

bool engine_calculate_times(
    int year, int month, int day,
    double lat_deg, double lng_deg,
    const engine_settings_t* settings,
    prayer_times_t* out_times
) {
    if (!settings || !out_times) return false;

    double jd = engine_julian_day(year, month, day);
    double declination = 0.0;
    double eot = 0.0;
    engine_solar_coordinates(jd, &declination, &eot);

    /* Solar noon in UTC minutes from midnight */
    double solar_noon_utc = 720.0 - (lng_deg * 4.0) - eot;

    /* Dhuhr is transit time (+ 65 sec safety buffer standard in Adhan) */
    double dhuhr = solar_noon_utc + (65.0 / 60.0);

    /* Sunrise & Sunset */
    double sunrise = NAN;
    double sunset = NAN;
    double ha_sun = calculate_hour_angle(lat_deg, declination, SUNRISE_SUNSET_ALTITUDE_DEG);
    if (!isnan(ha_sun)) {
        double diff_min = (ha_sun / 15.0) * 60.0;
        sunrise = solar_noon_utc - diff_min;
        sunset = solar_noon_utc + diff_min;
    }

    /* Asr */
    double asr = NAN;
    double asr_alt = calculate_asr_altitude(lat_deg, declination, settings->madhab);
    double ha_asr = calculate_hour_angle(lat_deg, declination, asr_alt);
    if (!isnan(ha_asr)) {
        asr = solar_noon_utc + (ha_asr / 15.0) * 60.0;
    }

    /* Fajr */
    double fajr = NAN;
    double fajr_angle = settings->fajr_angle_deg > 0.0 ? settings->fajr_angle_deg : 18.0;
    double ha_fajr = calculate_hour_angle(lat_deg, declination, -fajr_angle);
    if (!isnan(ha_fajr)) {
        fajr = solar_noon_utc - (ha_fajr / 15.0) * 60.0;
    }

    /* Maghrib */
    double maghrib = sunset;
    if (settings->maghrib_interval_min > 0 && !isnan(sunset)) {
        maghrib = sunset + settings->maghrib_interval_min;
    }

    /* Isha */
    double isha = NAN;
    if (settings->isha_interval_min > 0 && !isnan(maghrib)) {
        isha = maghrib + settings->isha_interval_min;
    } else {
        double isha_angle = settings->isha_angle_deg > 0.0 ? settings->isha_angle_deg : 17.0;
        double ha_isha = calculate_hour_angle(lat_deg, declination, -isha_angle);
        if (!isnan(ha_isha)) {
            isha = solar_noon_utc + (ha_isha / 15.0) * 60.0;
        }
    }

    /* High Latitude Adjustments */
    if (!isnan(sunrise) && !isnan(sunset)) {
        double night_duration = (1440.0 - sunset) + sunrise; // night from sunset to sunrise
        if (night_duration < 0.0) night_duration += 1440.0;

        if (settings->high_lat_rule == HIGH_LAT_SEVENTH) {
            double portion = night_duration / 7.0;
            if (isnan(fajr) || (sunrise - fajr) > portion) {
                fajr = sunrise - portion;
            }
            if (settings->isha_interval_min <= 0) {
                if (isnan(isha) || (isha - sunset) > portion) {
                    isha = sunset + portion;
                }
            }
        } else if (settings->high_lat_rule == HIGH_LAT_MIDNIGHT) {
            double portion = night_duration / 2.0;
            if (isnan(fajr) || (sunrise - fajr) > portion) {
                fajr = sunrise - portion;
            }
            if (settings->isha_interval_min <= 0) {
                if (isnan(isha) || (isha - sunset) > portion) {
                    isha = sunset + portion;
                }
            }
        } else if (settings->high_lat_rule == HIGH_LAT_ANGLE_BASED) {
            double fajr_portion = (fajr_angle / 60.0) * night_duration;
            if (isnan(fajr) || (sunrise - fajr) > fajr_portion) {
                fajr = sunrise - fajr_portion;
            }
            double isha_angle = settings->isha_angle_deg > 0.0 ? settings->isha_angle_deg : 17.0;
            double isha_portion = (isha_angle / 60.0) * night_duration;
            if (settings->isha_interval_min <= 0) {
                if (isnan(isha) || (isha - sunset) > isha_portion) {
                    isha = sunset + isha_portion;
                }
            }
        }
    }

    /* Apply user offsets */
    if (!isnan(fajr)) fajr += settings->fajr_offset_min;
    if (!isnan(sunrise)) sunrise += settings->sunrise_offset_min;
    if (!isnan(dhuhr)) dhuhr += settings->dhuhr_offset_min;
    if (!isnan(asr)) asr += settings->asr_offset_min;
    if (!isnan(sunset)) sunset += settings->sunset_offset_min;
    if (!isnan(maghrib)) maghrib += settings->maghrib_offset_min;
    if (!isnan(isha)) isha += settings->isha_offset_min;

    /* Normalize into [0, 1440) */
    out_times->fajr_utc_min = isnan(fajr) ? NAN : normalize_minutes(fajr);
    out_times->sunrise_utc_min = isnan(sunrise) ? NAN : normalize_minutes(sunrise);
    out_times->dhuhr_utc_min = isnan(dhuhr) ? NAN : normalize_minutes(dhuhr);
    out_times->asr_utc_min = isnan(asr) ? NAN : normalize_minutes(asr);
    out_times->sunset_utc_min = isnan(sunset) ? NAN : normalize_minutes(sunset);
    out_times->maghrib_utc_min = isnan(maghrib) ? NAN : normalize_minutes(maghrib);
    out_times->isha_utc_min = isnan(isha) ? NAN : normalize_minutes(isha);

    /* Extra times: Midnight & Last Third of Night */
    if (!isnan(sunset) && !isnan(sunrise)) {
        double night_len = (sunrise < sunset) ? (1440.0 - sunset + sunrise) : (sunrise - sunset);
        out_times->midnight_utc_min = normalize_minutes(sunset + night_len / 2.0);
        out_times->last_third_utc_min = normalize_minutes(sunset + (night_len * 2.0 / 3.0));
    } else {
        out_times->midnight_utc_min = NAN;
        out_times->last_third_utc_min = NAN;
    }

    /* Forbidden Windows */
    out_times->sunrise_forbidden_end_utc_min = isnan(sunrise) ? NAN : normalize_minutes(sunrise + 15.0);
    out_times->zenith_forbidden_start_utc_min = isnan(dhuhr) ? NAN : normalize_minutes(dhuhr - 10.0);
    out_times->sunset_forbidden_start_utc_min = isnan(sunset) ? NAN : normalize_minutes(sunset - 15.0);

    return true;
}

/* Tabular Islamic Calendar (Kuwaiti algorithm) */
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

    double jd = engine_julian_day(year, month, day) + (double)adjustment_days;

    /* Epoch of Islamic calendar is JD 1948439.5 (July 16, 622 CE Julian) */
    double l = floor(jd) - 1948440.0 + 10632.0;
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
