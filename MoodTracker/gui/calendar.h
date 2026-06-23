#pragma once
#include "../mood_tracker.h"

typedef enum {
    CalendarEventPrevMonth = 300,
    CalendarEventNextMonth,
    CalendarEventSelectDay,
} CalendarEvent;

void* calendar_view_alloc(MoodTrackerApp* app);
void  calendar_view_free(MoodTrackerApp* app);
void  calendar_view_refresh(MoodTrackerApp* app);
