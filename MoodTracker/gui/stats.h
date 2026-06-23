#pragma once
#include "../mood_tracker.h"

void* stats_view_alloc(MoodTrackerApp* app);
void  stats_view_free(MoodTrackerApp* app);
void  stats_view_refresh(MoodTrackerApp* app);
