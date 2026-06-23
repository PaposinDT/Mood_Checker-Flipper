#pragma once
#include "../mood_tracker.h"

typedef enum {
    SettingsEventExportCSV = 400,
    SettingsEventImportCSV,
    SettingsEventBackup,
} SettingsEvent;

void settings_view_build(MoodTrackerApp* app);
