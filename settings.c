#pragma once
#include "../mood_tracker.h"

typedef struct {
    MoodTrackerApp* app;
} HeatmapViewModel;

void* heatmap_view_alloc(MoodTrackerApp* app);
void  heatmap_view_free(MoodTrackerApp* app);
void  heatmap_view_refresh(MoodTrackerApp* app);
