#include "correlation.h"
#include "statistics.h"

void mood_correlation_compute(MoodTrackerApp* app, MoodCorrelation* out, uint32_t n_days) {
    if(!out) return;

    // Collect per-day averages for the last n_days
    uint32_t max_count = n_days;
    if(max_count > MOOD_MAX_ENTRIES) max_count = MOOD_MAX_ENTRIES;

    float* mood_arr = malloc(sizeof(float) * max_count);
    float* stress_arr = malloc(sizeof(float) * max_count);
    float* energy_arr = malloc(sizeof(float) * max_count);
    if(!mood_arr || !stress_arr || !energy_arr) {
        free(mood_arr);
        free(stress_arr);
        free(energy_arr);
        memset(out, 0, sizeof(MoodCorrelation));
        return;
    }
    uint32_t count = 0;

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t today_approx = (uint32_t)now.year * 10000 + now.month * 100 + now.day;

    // Walk entries, collect unique day averages
    uint32_t prev_key = 0;
    float dm = 0, ds = 0, de = 0;
    uint8_t dc = 0;

    for(uint32_t i = 0; i <= app->entry_count && count < max_count; i++) {
        uint32_t cur_key = (i < app->entry_count) ?
            PACK_DATE(app->entries[i].year,
                      app->entries[i].month,
                      app->entries[i].day) : 0xFFFFFFFF;

        if(i > 0 && cur_key != prev_key) {
            if(prev_key <= today_approx) {
                mood_arr[count]   = dm / dc;
                stress_arr[count] = ds / dc;
                energy_arr[count] = de / dc;
                count++;
            }
            dm = ds = de = 0;
            dc = 0;
        }
        if(i < app->entry_count) {
            dm += app->entries[i].mood;
            ds += app->entries[i].stress;
            de += app->entries[i].energy;
            dc++;
            prev_key = cur_key;
        }
    }

    out->mood_stress   = mood_pearson(mood_arr, stress_arr, count);
    out->mood_energy   = mood_pearson(mood_arr, energy_arr, count);
    out->stress_energy = mood_pearson(stress_arr, energy_arr, count);

    // Human-readable descriptions
    if(out->mood_stress > 0.4f)
        out->mood_stress_desc = "Stress boosts mood";
    else if(out->mood_stress < -0.4f)
        out->mood_stress_desc = "Stress lowers mood";
    else
        out->mood_stress_desc = "Stress/mood independent";

    if(out->mood_energy > 0.4f)
        out->mood_energy_desc = "Energy boosts mood";
    else if(out->mood_energy < -0.4f)
        out->mood_energy_desc = "Energy lowers mood";
    else
        out->mood_energy_desc = "Energy/mood independent";

    free(mood_arr);
    free(stress_arr);
    free(energy_arr);
}
