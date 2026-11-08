#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../engine/engine.h"

typedef struct {
    const char* name;
    double lat;
    double lng;
} city_target_t;

static const city_target_t BENCH_CITIES[] = {
    { "Dhaka",      23.8103,   90.4125 },
    { "Mecca",      21.4225,   39.8262 },
    { "London",     51.5074,   -0.1278 },
    { "Oslo",       59.9139,   10.7522 },
    { "Tromso",     69.6492,   18.9553 },
    { "Reykjavik",  64.1466,  -21.9426 },
    { "Sydney",    -33.8688,  151.2093 },
    { "Cape Town", -33.9249,   18.4241 },
    { "New York",   40.7128,  -74.0060 },
    { "Anchorage",  61.2181, -149.9003 },
    { "Singapore",   1.3521,  103.8198 },
    { "Quito",      -0.1807,  -78.4678 }
};
static const size_t NUM_BENCH_CITIES = sizeof(BENCH_CITIES) / sizeof(BENCH_CITIES[0]);

typedef struct {
    int year;
    int month;
    int day;
} date_target_t;

static const date_target_t BENCH_DATES[] = {
    { 2024,  6, 20 }, /* Summer solstice 2024 */
    { 2024, 12, 21 }, /* Winter solstice 2024 */
    { 2024,  3, 20 }, /* Spring equinox 2024 */
    { 2024,  9, 22 }, /* Autumn equinox 2024 */
    { 2024,  2, 29 }, /* Leap day */
    { 2023, 12, 31 }, /* Year end boundary */
    { 2024,  1,  1 }, /* New year boundary */
    { 2024,  3, 12 }, /* Ramadan date */
    { 2024,  7, 15 }, /* High latitude summer */
    { 2024,  1, 15 }, /* High latitude winter */
    { 2023,  6, 21 }, /* Summer solstice 2023 */
    { 2023,  3, 21 }  /* Equinox 2023 */
};
static const size_t NUM_BENCH_DATES = sizeof(BENCH_DATES) / sizeof(BENCH_DATES[0]);

static inline uint64_t hash_combine(uint64_t h, double val) {
    uint64_t u;
    if (isnan(val)) {
        u = 0x7FF8000000000000ULL;
    } else {
        memcpy(&u, &val, sizeof(u));
    }
    return h ^ (u + 0x517cc1b727220a95ULL + (h << 6) + (h >> 2));
}

static inline uint64_t hash_times(uint64_t h, const prayer_times_t* t) {
    h = hash_combine(h, t->fajr_utc_min);
    h = hash_combine(h, t->sunrise_utc_min);
    h = hash_combine(h, t->dhuhr_utc_min);
    h = hash_combine(h, t->asr_utc_min);
    h = hash_combine(h, t->sunset_utc_min);
    h = hash_combine(h, t->maghrib_utc_min);
    h = hash_combine(h, t->isha_utc_min);
    h = hash_combine(h, t->midnight_utc_min);
    h = hash_combine(h, t->last_third_utc_min);
    return h;
}

static void sort_doubles(double* arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (arr[j] < arr[i]) {
                double tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}

int main(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    engine_settings_t settings[CALC_METHOD_COUNT];
    for (int m = 0; m < CALC_METHOD_COUNT; m++) {
        engine_get_default_settings((calc_method_t)m, &settings[m]);
        settings[m].madhab = (m % 2 == 0) ? MADHAB_STANDARD : MADHAB_HANAFI;
    }

    const int N_TIMES = 100000;
    const int N_MONTH = 5000;
    const int RUNS = 5;

    double times_ns[RUNS];
    double month_ns[RUNS];
    uint64_t final_checksum = 0;

    printf("=== Azaan Engine Benchmark ===\n");
    printf("Warming up...\n");

    /* Warm-up run */
    prayer_times_t pt_dummy;
    for (int i = 0; i < 10000; i++) {
        const city_target_t* c = &BENCH_CITIES[i % NUM_BENCH_CITIES];
        const date_target_t* d = &BENCH_DATES[i % NUM_BENCH_DATES];
        engine_calculate_times(d->year, d->month, d->day, c->lat, c->lng, &settings[i % CALC_METHOD_COUNT], &pt_dummy);
    }

    printf("Benchmarking engine_calculate_times (N = %d, %d runs)...\n", N_TIMES, RUNS);
    for (int r = 0; r < RUNS; r++) {
        uint64_t chk = 0;
        LARGE_INTEGER start, end;
        QueryPerformanceCounter(&start);

        for (int i = 0; i < N_TIMES; i++) {
            const city_target_t* c = &BENCH_CITIES[i % NUM_BENCH_CITIES];
            const date_target_t* d = &BENCH_DATES[i % NUM_BENCH_DATES];
            prayer_times_t pt;
            engine_calculate_times(d->year, d->month, d->day, c->lat, c->lng, &settings[i % CALC_METHOD_COUNT], &pt);
            chk = hash_times(chk, &pt);
        }

        QueryPerformanceCounter(&end);
        double elapsed_sec = (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        times_ns[r] = (elapsed_sec * 1e9) / (double)N_TIMES;
        final_checksum ^= chk;
        printf("  Run %d: %.2f ns/call\n", r + 1, times_ns[r]);
    }

    printf("Benchmarking engine_calculate_month (N = %d, %d runs)...\n", N_MONTH, RUNS);
    prayer_times_t month_buf[31];
    for (int r = 0; r < RUNS; r++) {
        uint64_t chk = 0;
        LARGE_INTEGER start, end;
        QueryPerformanceCounter(&start);

        for (int i = 0; i < N_MONTH; i++) {
            const city_target_t* c = &BENCH_CITIES[i % NUM_BENCH_CITIES];
            int year = 2023 + (i % 3);
            int month = 1 + (i % 12);
            int day_count = 0;
            engine_calculate_month(year, month, c->lat, c->lng, &settings[i % CALC_METHOD_COUNT], month_buf, &day_count);
            for (int d = 0; d < day_count; d++) {
                chk = hash_times(chk, &month_buf[d]);
            }
        }

        QueryPerformanceCounter(&end);
        double elapsed_sec = (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        month_ns[r] = (elapsed_sec * 1e9) / (double)N_MONTH;
        final_checksum ^= chk;
        printf("  Run %d: %.2f ns/call\n", r + 1, month_ns[r]);
    }

    sort_doubles(times_ns, RUNS);
    sort_doubles(month_ns, RUNS);

    double median_times = times_ns[RUNS / 2];
    double median_month = month_ns[RUNS / 2];

    printf("\n=== Summary Results ===\n");
    printf("Checksum: 0x%016llX\n", (unsigned long long)final_checksum);
    printf("calculate_times median: %.2f ns/call\n", median_times);
    printf("calculate_month median: %.2f ns/call\n", median_month);

    return 0;
}
