#pragma once
#include "../mood_tracker.h"

// Load all entries from CSV (rebuilds index)
bool mood_db_load(MoodTrackerApp* app);

// Save all entries to CSV + rebuild binary index
bool mood_db_save(MoodTrackerApp* app);

// Load / save app configuration
bool mood_config_load(MoodTrackerApp* app);
bool mood_config_save(MoodTrackerApp* app);

// Find entries for a given date key (YYYYMMDD), returns count
uint8_t mood_db_get_day(MoodTrackerApp* app, uint32_t date_key,
                         MoodEntry* out, uint8_t max_out);

// Get aggregated day stats for a date key
bool mood_db_get_day_stats(MoodTrackerApp* app, uint32_t date_key,
                            MoodDayStats* out);

// Get aggregated stats for a range [from_key..to_key]
uint32_t mood_db_get_range(MoodTrackerApp* app,
                            uint32_t from_key, uint32_t to_key,
                            MoodDayStats* out, uint32_t max_out);
