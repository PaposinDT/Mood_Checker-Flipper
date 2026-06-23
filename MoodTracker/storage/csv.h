#pragma once
#include "../mood_tracker.h"

// CSV header line written at top of file
#define MOOD_CSV_HEADER "date,time,mood,stress,energy,note,primary\n"

// Export all entries to CSV at given path
bool mood_csv_export(MoodTrackerApp* app, const char* path);

// Import entries from CSV at given path (appends to app->entries)
bool mood_csv_import(MoodTrackerApp* app, const char* path);
