#pragma once
#include "../mood_tracker.h"

// Custom events fired from Home view
typedef enum {
    HomeEventNewEntry = 0,
    HomeEventViewGraph,
    HomeEventViewCalendar,
    HomeEventViewStats,
    HomeEventViewHeatmap,
    HomeEventSettings,
} HomeEvent;

// Allocate and free the home view (returns View*)
void* home_view_alloc(MoodTrackerApp* app);
void  home_view_free(MoodTrackerApp* app);

// Refresh display data (call before switching to view)
void  home_view_refresh(MoodTrackerApp* app);
