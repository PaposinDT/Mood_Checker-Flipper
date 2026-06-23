#pragma once
#include "../mood_tracker.h"

typedef enum {
    GraphEventToggleMode = 200,
    GraphEventZoomIn,
    GraphEventZoomOut,
} GraphEvent;

void* graph_view_alloc(MoodTrackerApp* app);
void  graph_view_free(MoodTrackerApp* app);
void  graph_view_refresh(MoodTrackerApp* app);
