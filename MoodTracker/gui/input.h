#pragma once
#include "../mood_tracker.h"

typedef enum {
    MoodInputEventNext = 100,
    MoodInputEventSave,
    MoodInputEventCancel,
} MoodInputCustomEvent;

// Called by TextInput when the user confirms the note
void mood_input_note_callback(void* ctx);

void* input_view_alloc(MoodTrackerApp* app);
void  input_view_free(MoodTrackerApp* app);
void  input_view_refresh(MoodTrackerApp* app);
