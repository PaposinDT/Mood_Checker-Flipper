#include "streak.h"

// ─── Accurate Rata Die date-to-days ──────────────────────────────────────────

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

// ─── Streak compute ───────────────────────────────────────────────────────────

void mood_streak_compute(MoodTrackerApp* app) {
    app->stats.current_streak = 0;
    app->stats.longest_streak = 0;

    if(app->entry_count == 0) return;

    uint32_t day_nums[MOOD_MAX_ENTRIES];
    uint32_t day_count = 0;
    uint32_t prev_key  = 0;

    for(uint32_t i = 0; i < app->entry_count; i++) {
        uint32_t k = PACK_DATE(
            app->entries[i].year,
            app->entries[i].month,
            app->entries[i].day);
        if(k == prev_key) continue;
        day_nums[day_count++] = date_to_days(
            app->entries[i].year,
            app->entries[i].month,
            app->entries[i].day);
        prev_key = k;
    }

    if(day_count == 0) return;

    // Insertion sort
    for(uint32_t i = 1; i < day_count; i++) {
        uint32_t key = day_nums[i];
        int32_t  j   = (int32_t)i - 1;
        while(j >= 0 && day_nums[j] > key) {
            day_nums[j + 1] = day_nums[j];
            j--;
        }
        day_nums[j + 1] = key;
    }

    // Deduplicate
    uint32_t dedup[MOOD_MAX_ENTRIES];
    uint32_t dc = 0;
    for(uint32_t i = 0; i < day_count; i++) {
        if(dc == 0 || dedup[dc - 1] != day_nums[i])
            dedup[dc++] = day_nums[i];
    }
    day_count = dc;

    // Longest streak
    uint32_t longest = 1, cur = 1;
    for(uint32_t i = 1; i < day_count; i++) {
        if(dedup[i] == dedup[i - 1] + 1) {
            cur++;
            if(cur > longest) longest = cur;
        } else {
            cur = 1;
        }
    }
    app->stats.longest_streak = longest;

    // Current streak backwards from today
    FuriHalRtcDateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t today    = date_to_days(now.year, now.month, now.day);
    uint32_t streak   = 0;
    int32_t  idx      = (int32_t)day_count - 1;
    uint32_t expected = today;

    if(idx >= 0 && dedup[idx] == today) {
        streak = 1; expected = today - 1; idx--;
    } else if(idx >= 0 && dedup[idx] == today - 1) {
        streak = 1; expected = today - 2; idx--;
    }

    while(idx >= 0 && dedup[idx] == expected) {
        streak++; expected--; idx--;
    }

    app->stats.current_streak = streak;
}
