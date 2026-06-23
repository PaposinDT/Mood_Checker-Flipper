#include "statistics.h"
#include "../storage/database.h"
#include <math.h>

// ─── Accurate date helpers ────────────────────────────────────────────────────

// Rata Die — days since 1 Jan 0001
static uint32_t date_to_days(uint16_t y, uint8_t m, uint8_t d) {
    static const uint16_t dpm[] =
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t days = (uint32_t)(y - 1) * 365
                  + (y - 1) / 4
                  - (y - 1) / 100
                  + (y - 1) / 400
                  + dpm[m - 1]
                  + d;
    bool leap = (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0));
    if(leap && m > 2) days++;
    return days;
}

// Day of week: 0=Mon … 6=Sun from Rata Die
static uint8_t day_of_week(uint16_t y, uint8_t m, uint8_t d) {
    return (uint8_t)((date_to_days(y, m, d) + 6) % 7);
}

// ─── Pearson Correlation ──────────────────────────────────────────────────────

float mood_pearson(const float* a, const float* b, uint32_t n) {
    if(n < 2) return 0.0f;

    float sum_a = 0, sum_b = 0;
    for(uint32_t i = 0; i < n; i++) {
        sum_a += a[i];
        sum_b += b[i];
    }
    float mean_a = sum_a / (float)n;
    float mean_b = sum_b / (float)n;

    float num = 0, den_a = 0, den_b = 0;
    for(uint32_t i = 0; i < n; i++) {
        float da = a[i] - mean_a;
        float db = b[i] - mean_b;
        num   += da * db;
        den_a += da * da;
        den_b += db * db;
    }

    float den = sqrtf(den_a * den_b);
    if(den < 1e-6f) return 0.0f;
    return num / den;
}

// ─── Main compute ─────────────────────────────────────────────────────────────

void mood_stats_compute(MoodTrackerApp* app) {
    MoodStats* s = &app->stats;
    memset(s, 0, sizeof(MoodStats));
    if(app->entry_count == 0) return;

    float sum_mood = 0, sum_stress = 0, sum_energy = 0;
    float best_mood  = 0;
    float worst_mood = 11;

    float wd_mood_sum[7] = {0};
    uint32_t wd_count[7] = {0};

    float* mood_arr = malloc(sizeof(float) * MOOD_MAX_ENTRIES);
    float* stress_arr = malloc(sizeof(float) * MOOD_MAX_ENTRIES);
    float* energy_arr = malloc(sizeof(float) * MOOD_MAX_ENTRIES);
    if(!mood_arr || !stress_arr || !energy_arr) {
        free(mood_arr);
        free(stress_arr);
        free(energy_arr);
        return;
    }
    uint32_t day_pts = 0;

    uint32_t prev_day_key = 0;
    float day_mood = 0, day_stress = 0, day_energy = 0;
    uint8_t day_cnt = 0;
    uint32_t total_days = 0;

    for(uint32_t i = 0; i <= app->entry_count; i++) {
        uint32_t cur_key = (i < app->entry_count) ?
            PACK_DATE(app->entries[i].year,
                      app->entries[i].month,
                      app->entries[i].day) : 0xFFFFFFFFu;

        if(i > 0 && cur_key != prev_day_key) {
            float dm = day_mood   / day_cnt;
            float ds = day_stress / day_cnt;
            float de = day_energy / day_cnt;

            if(dm > best_mood)  best_mood  = dm;
            if(dm < worst_mood) worst_mood = dm;

            uint8_t wd = day_of_week(
                (uint16_t)UNPACK_YEAR(prev_day_key),
                (uint8_t) UNPACK_MONTH(prev_day_key),
                (uint8_t) UNPACK_DAY(prev_day_key));
            wd_mood_sum[wd] += dm;
            wd_count[wd]++;

            if(day_pts < MOOD_MAX_ENTRIES) {
                mood_arr[day_pts]   = dm;
                stress_arr[day_pts] = ds;
                energy_arr[day_pts] = de;
                day_pts++;
            }

            total_days++;
            day_mood = day_stress = day_energy = 0;
            day_cnt = 0;
        }

        if(i < app->entry_count) {
            day_mood   += app->entries[i].mood;
            day_stress += app->entries[i].stress;
            day_energy += app->entries[i].energy;
            day_cnt++;
            prev_day_key = cur_key;

            sum_mood   += app->entries[i].mood;
            sum_stress += app->entries[i].stress;
            sum_energy += app->entries[i].energy;
        }
    }

    s->total_entries       = app->entry_count;
    s->total_days          = total_days;
    s->all_time_avg_mood   = (app->entry_count > 0) ? sum_mood   / app->entry_count : 0;
    s->all_time_avg_stress = (app->entry_count > 0) ? sum_stress / app->entry_count : 0;
    s->all_time_avg_energy = (app->entry_count > 0) ? sum_energy / app->entry_count : 0;
    s->best_mood           = best_mood;
    s->worst_mood          = (worst_mood > 10) ? 0 : worst_mood;

    float best_wd = -1, worst_wd = 11;
    for(int w = 0; w < 7; w++) {
        if(wd_count[w] == 0) continue;
        float avg = wd_mood_sum[w] / (float)wd_count[w];
        if(avg > best_wd)  { best_wd  = avg; s->best_weekday  = (uint8_t)w; }
        if(avg < worst_wd) { worst_wd = avg; s->worst_weekday = (uint8_t)w; }
    }

    s->mood_stress_corr = mood_pearson(mood_arr, stress_arr, day_pts);
    s->mood_energy_corr = mood_pearson(mood_arr, energy_arr, day_pts);

    free(mood_arr);
    free(stress_arr);
    free(energy_arr);
}

// ─── Weekly aggregation (accurate) ───────────────────────────────────────────

void mood_stats_weekly(MoodTrackerApp* app, MoodDayStats* out, uint8_t n_weeks) {
    for(int w = 0; w < n_weeks; w++) {
        out[w].date_key    = 0;
        out[w].avg_mood    = 0;
        out[w].avg_stress  = 0;
        out[w].avg_energy  = 0;
        out[w].entry_count = 0;
    }

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    uint32_t today_n = date_to_days(now.year, now.month, now.day);
    uint8_t  today_wd = (uint8_t)((today_n + 6) % 7); // 0=Mon

    // Day number of the start of the current ISO week (Monday)
    uint32_t week_start = today_n - today_wd;

    for(uint32_t i = 0; i < app->entry_count; i++) {
        uint32_t en = date_to_days(
            app->entries[i].year,
            app->entries[i].month,
            app->entries[i].day);
        if(en > today_n) continue;

        // How many complete weeks ago did this entry's week start?
        uint32_t entry_week_start = en - ((en + 6) % 7);
        if(week_start < entry_week_start) continue; // future week

        uint32_t weeks_ago = (week_start - entry_week_start) / 7;
        if(weeks_ago >= n_weeks) continue;

        out[weeks_ago].avg_mood   += app->entries[i].mood;
        out[weeks_ago].avg_stress += app->entries[i].stress;
        out[weeks_ago].avg_energy += app->entries[i].energy;
        out[weeks_ago].entry_count++;
    }

    for(int w = 0; w < n_weeks; w++) {
        if(out[w].entry_count > 0) {
            float c = (float)out[w].entry_count;
            out[w].avg_mood   /= c;
            out[w].avg_stress /= c;
            out[w].avg_energy /= c;
        }
        out[w].date_key = (uint32_t)w; // week index as label
    }
}

// ─── Monthly aggregation ──────────────────────────────────────────────────────

void mood_stats_monthly(MoodTrackerApp* app, MoodDayStats* out, uint8_t n_months) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    for(int m = 0; m < n_months; m++) {
        out[m].date_key    = 0;
        out[m].avg_mood    = 0;
        out[m].avg_stress  = 0;
        out[m].avg_energy  = 0;
        out[m].entry_count = 0;
    }

    for(uint32_t i = 0; i < app->entry_count; i++) {
        int32_t my = (int32_t)now.year   * 12 + now.month;
        int32_t ey = (int32_t)app->entries[i].year * 12 + app->entries[i].month;
        int32_t mo = my - ey;
        if(mo < 0 || mo >= n_months) continue;

        out[mo].avg_mood   += app->entries[i].mood;
        out[mo].avg_stress += app->entries[i].stress;
        out[mo].avg_energy += app->entries[i].energy;
        out[mo].entry_count++;
    }

    for(int m = 0; m < n_months; m++) {
        if(out[m].entry_count > 0) {
            float c = (float)out[m].entry_count;
            out[m].avg_mood   /= c;
            out[m].avg_stress /= c;
            out[m].avg_energy /= c;
        }
        int32_t tot  = (int32_t)now.year * 12 + now.month - m;
        out[m].date_key = (uint32_t)((tot / 12) * 10000 + (tot % 12) * 100);
    }
}
