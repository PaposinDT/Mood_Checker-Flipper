#pragma once
#include "../mood_tracker.h"

typedef struct {
    float mood_stress;   // Pearson r
    float mood_energy;   // Pearson r
    float stress_energy; // Pearson r
    const char* mood_stress_desc;
    const char* mood_energy_desc;
} MoodCorrelation;

// Compute correlations using last n_days of data
void mood_correlation_compute(MoodTrackerApp* app, MoodCorrelation* out, uint32_t n_days);
