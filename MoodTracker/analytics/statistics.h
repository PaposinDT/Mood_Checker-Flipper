#pragma once
#include "../mood_tracker.h"

// Recompute app->stats from all entries
void mood_stats_compute(MoodTrackerApp* app);

// Get weekly average for last N weeks (out must hold n_weeks MoodDayStats)
void mood_stats_weekly(MoodTrackerApp* app, MoodDayStats* out, uint8_t n_weeks);

// Get monthly average for last N months
void mood_stats_monthly(MoodTrackerApp* app, MoodDayStats* out, uint8_t n_months);

// Pearson correlation between two float arrays
float mood_pearson(const float* a, const float* b, uint32_t n);
