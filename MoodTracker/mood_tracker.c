#include "mood_tracker.h"
#include "gui/home.h"
#include "gui/input.h"
#include "gui/graphs.h"
#include "gui/calendar.h"
#include "gui/stats.h"
#include "gui/heatmap.h"
#include "gui/settings.h"
#include "storage/database.h"
#include "storage/csv.h"
#include "analytics/statistics.h"
#include "analytics/streak.h"
#include "quotes/quotes.h"

// ─── Forward declarations ─────────────────────────────────────────────────────

void mood_scene_main_on_enter(void* ctx);
bool mood_scene_main_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_main_on_exit(void* ctx);

void mood_scene_home_on_enter(void* ctx);
bool mood_scene_home_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_home_on_exit(void* ctx);

void mood_scene_input_on_enter(void* ctx);
bool mood_scene_input_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_input_on_exit(void* ctx);

void mood_scene_input_note_on_enter(void* ctx);
bool mood_scene_input_note_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_input_note_on_exit(void* ctx);

void mood_scene_graph_on_enter(void* ctx);
bool mood_scene_graph_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_graph_on_exit(void* ctx);

void mood_scene_calendar_on_enter(void* ctx);
bool mood_scene_calendar_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_calendar_on_exit(void* ctx);

void mood_scene_stats_on_enter(void* ctx);
bool mood_scene_stats_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_stats_on_exit(void* ctx);

void mood_scene_heatmap_on_enter(void* ctx);
bool mood_scene_heatmap_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_heatmap_on_exit(void* ctx);

void mood_scene_settings_on_enter(void* ctx);
bool mood_scene_settings_on_event(void* ctx, SceneManagerEvent event);
void mood_scene_settings_on_exit(void* ctx);

// ─── Scene handler tables (must match MoodScene enum order) ──────────────────

void (*const mood_scene_on_enter_handlers[])(void*) = {
    mood_scene_main_on_enter,
    mood_scene_home_on_enter,
    mood_scene_input_on_enter,
    mood_scene_input_note_on_enter,
    mood_scene_graph_on_enter,
    mood_scene_calendar_on_enter,
    mood_scene_stats_on_enter,
    mood_scene_heatmap_on_enter,
    mood_scene_settings_on_enter,
};

bool (*const mood_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    mood_scene_main_on_event,
    mood_scene_home_on_event,
    mood_scene_input_on_event,
    mood_scene_input_note_on_event,
    mood_scene_graph_on_event,
    mood_scene_calendar_on_event,
    mood_scene_stats_on_event,
    mood_scene_heatmap_on_event,
    mood_scene_settings_on_event,
};

void (*const mood_scene_on_exit_handlers[])(void*) = {
    mood_scene_main_on_exit,
    mood_scene_home_on_exit,
    mood_scene_input_on_exit,
    mood_scene_input_note_on_exit,
    mood_scene_graph_on_exit,
    mood_scene_calendar_on_exit,
    mood_scene_stats_on_exit,
    mood_scene_heatmap_on_exit,
    mood_scene_settings_on_exit,
};

static const SceneManagerHandlers mood_scene_manager_handlers = {
    .on_enter_handlers = mood_scene_on_enter_handlers,
    .on_event_handlers = mood_scene_on_event_handlers,
    .on_exit_handlers  = mood_scene_on_exit_handlers,
    .scene_num         = MoodSceneCount,
};

// ─── View-dispatcher callbacks ────────────────────────────────────────────────

static bool mood_custom_event_callback(void* ctx, uint32_t event) {
    furi_assert(ctx);
    MoodTrackerApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool mood_back_event_callback(void* ctx) {
    furi_assert(ctx);
    MoodTrackerApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

// ─── App alloc ───────────────────────────────────────────────────────────────

static MoodTrackerApp* mood_tracker_app_alloc(void) {
    MoodTrackerApp* app = malloc(sizeof(MoodTrackerApp));
    memset(app, 0, sizeof(MoodTrackerApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->storage       = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&mood_scene_manager_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, mood_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, mood_back_event_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Shared modules
    app->popup         = popup_alloc();
    app->text_input    = text_input_alloc();
    app->widget        = widget_alloc();
    app->var_item_list = variable_item_list_alloc();

    // Register views
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewHome,      home_view_alloc(app));
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewInput,     input_view_alloc(app));
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewGraph,     graph_view_alloc(app));
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewCalendar,  calendar_view_alloc(app));
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewStats,     stats_view_alloc(app));
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewHeatmap,   heatmap_view_alloc(app));
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewSettings,
        variable_item_list_get_view(app->var_item_list));
    // FIX: register TextInput view so InputNote scene can switch to it
    view_dispatcher_add_view(
        app->view_dispatcher, MoodViewTextInput,
        text_input_get_view(app->text_input));

    // Defaults
    app->config.theme               = MoodThemeLight;
    app->config.pin_enabled         = false;
    app->config.show_quote_on_start = true;
    app->config.reminder_enabled    = false;
    app->config.reminder_hour       = 20;
    app->config.reminder_minute     = 0;
    app->config.default_graph_mode  = GraphModeMoodStressEnergy;
    app->config.graph_days          = 30;

    app->graph_days_shown   = 30;
    app->current_graph_mode = GraphModeMoodStressEnergy;
    app->data_dirty         = false;
    app->daily_quote_index  = 0;

    return app;
}

// ─── App free ────────────────────────────────────────────────────────────────

static void mood_tracker_app_free(MoodTrackerApp* app) {
    furi_assert(app);

    if(app->data_dirty) mood_db_save(app);
    mood_config_save(app);

    view_dispatcher_remove_view(app->view_dispatcher, MoodViewHome);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewInput);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewGraph);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewCalendar);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewStats);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewHeatmap);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, MoodViewTextInput);

    home_view_free(app);
    input_view_free(app);
    graph_view_free(app);
    calendar_view_free(app);
    stats_view_free(app);
    heatmap_view_free(app);

    popup_free(app->popup);
    text_input_free(app->text_input);
    widget_free(app->widget);
    variable_item_list_free(app->var_item_list);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

// ─── Scene implementations ────────────────────────────────────────────────────

// Main (entry gate — goes straight to Home, no PIN)
void mood_scene_main_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    scene_manager_next_scene(app->scene_manager, MoodSceneHome);
}
bool mood_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx); UNUSED(event);
    return false;
}
void mood_scene_main_on_exit(void* ctx) { UNUSED(ctx); }

// ── Home ─────────────────────────────────────────────────────────────────────

void mood_scene_home_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    home_view_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewHome);
}
bool mood_scene_home_on_event(void* ctx, SceneManagerEvent event) {
    MoodTrackerApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case HomeEventNewEntry:
                scene_manager_next_scene(app->scene_manager, MoodSceneInput);
                consumed = true; break;
            case HomeEventViewGraph:
                scene_manager_next_scene(app->scene_manager, MoodSceneGraph);
                consumed = true; break;
            case HomeEventViewCalendar:
                scene_manager_next_scene(app->scene_manager, MoodSceneCalendar);
                consumed = true; break;
            case HomeEventViewStats:
                scene_manager_next_scene(app->scene_manager, MoodSceneStats);
                consumed = true; break;
            case HomeEventViewHeatmap:
                scene_manager_next_scene(app->scene_manager, MoodSceneHeatmap);
                consumed = true; break;
            case HomeEventSettings:
                scene_manager_next_scene(app->scene_manager, MoodSceneSettings);
                consumed = true; break;
            default: break;
        }
    }
    return consumed;
}
void mood_scene_home_on_exit(void* ctx) { UNUSED(ctx); }

// ── Input (mood / stress / energy sliders) ───────────────────────────────────

void mood_scene_input_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    app->input_step = 0;
    memset(&app->pending_entry, 0, sizeof(MoodEntry));
    app->pending_entry.mood   = 5;
    app->pending_entry.stress = 5;
    app->pending_entry.energy = 5;
    FuriHalRtcDateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    app->pending_entry.year   = dt.year;
    app->pending_entry.month  = dt.month;
    app->pending_entry.day    = dt.day;
    app->pending_entry.hour   = dt.hour;
    app->pending_entry.minute = dt.minute;
    input_view_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewInput);
}

bool mood_scene_input_on_event(void* ctx, SceneManagerEvent event) {
    MoodTrackerApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case InputEventNext:
                app->input_step++;
                if(app->input_step >= 3) {
                    // All three values collected → go to note scene
                    scene_manager_next_scene(
                        app->scene_manager, MoodSceneInputNote);
                } else {
                    input_view_refresh(app);
                }
                consumed = true;
                break;
            default: break;
        }
    }
    return consumed;
}
void mood_scene_input_on_exit(void* ctx) { UNUSED(ctx); }

// ── InputNote (TextInput) ─────────────────────────────────────────────────────
// FIX: configure TextInput properly and switch to MoodViewTextInput (not Input).
// FIX: InputEventSave is handled HERE, not in the Input scene.

void mood_scene_input_note_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;

    // Clear any leftover note text
    app->pending_entry.note[0] = '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Note (optional, OK to skip)");
    text_input_set_result_callback(
        app->text_input,
        mood_input_note_callback,   // calls view_dispatcher_send_custom_event(InputEventSave)
        app,
        app->pending_entry.note,
        MOOD_NOTE_MAX_LEN,
        true);   // clear_default_text = true

    // FIX: switch to the TextInput view, not MoodViewInput
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewTextInput);
}

bool mood_scene_input_note_on_event(void* ctx, SceneManagerEvent event) {
    MoodTrackerApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == InputEventSave) {
        // Determine primary flag
        uint32_t today = PACK_DATE(
            app->pending_entry.year,
            app->pending_entry.month,
            app->pending_entry.day);
        app->pending_entry.is_primary = true;
        for(uint32_t i = 0; i < app->entry_count; i++) {
            uint32_t k = PACK_DATE(
                app->entries[i].year,
                app->entries[i].month,
                app->entries[i].day);
            if(k == today) {
                app->pending_entry.is_primary = false;
                break;
            }
        }

        // Save
        if(app->entry_count < MOOD_MAX_ENTRIES) {
            app->entries[app->entry_count++] = app->pending_entry;
            app->data_dirty = true;
            mood_db_save(app);
            mood_stats_compute(app);
            mood_streak_compute(app);
        }

        // Return to Home (pop Input + InputNote from stack)
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MoodSceneHome);
        consumed = true;
    }
    return consumed;
}
void mood_scene_input_note_on_exit(void* ctx) { UNUSED(ctx); }

// ── Graph ─────────────────────────────────────────────────────────────────────

void mood_scene_graph_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    graph_view_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewGraph);
}
bool mood_scene_graph_on_event(void* ctx, SceneManagerEvent event) {
    MoodTrackerApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case GraphEventToggleMode:
                app->current_graph_mode = (GraphMode)((app->current_graph_mode + 1) % 4);
                graph_view_refresh(app);
                consumed = true; break;
            case GraphEventZoomIn:
                if(app->graph_days_shown > 7) app->graph_days_shown -= 7;
                graph_view_refresh(app);
                consumed = true; break;
            case GraphEventZoomOut:
                if(app->graph_days_shown < 90) app->graph_days_shown += 7;
                graph_view_refresh(app);
                consumed = true; break;
            default: break;
        }
    }
    return consumed;
}
void mood_scene_graph_on_exit(void* ctx) { UNUSED(ctx); }

// ── Calendar ──────────────────────────────────────────────────────────────────

void mood_scene_calendar_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    app->calendar_month_offset = 0;
    calendar_view_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewCalendar);
}
bool mood_scene_calendar_on_event(void* ctx, SceneManagerEvent event) {
    MoodTrackerApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case CalendarEventPrevMonth:
                app->calendar_month_offset--;
                calendar_view_refresh(app);
                consumed = true; break;
            case CalendarEventNextMonth:
                app->calendar_month_offset++;
                calendar_view_refresh(app);
                consumed = true; break;
            default: break;
        }
    }
    return consumed;
}
void mood_scene_calendar_on_exit(void* ctx) { UNUSED(ctx); }

// ── Stats ─────────────────────────────────────────────────────────────────────

void mood_scene_stats_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    stats_view_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewStats);
}
bool mood_scene_stats_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx); UNUSED(event);
    return false;
}
void mood_scene_stats_on_exit(void* ctx) { UNUSED(ctx); }

// ── Heatmap ───────────────────────────────────────────────────────────────────
// FIX: now switches to MoodViewHeatmap (its own real view)

void mood_scene_heatmap_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    heatmap_view_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewHeatmap);
}
bool mood_scene_heatmap_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx); UNUSED(event);
    return false;
}
void mood_scene_heatmap_on_exit(void* ctx) { UNUSED(ctx); }

// ── Settings ──────────────────────────────────────────────────────────────────

void mood_scene_settings_on_enter(void* ctx) {
    MoodTrackerApp* app = ctx;
    settings_view_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoodViewSettings);
}
bool mood_scene_settings_on_event(void* ctx, SceneManagerEvent event) {
    MoodTrackerApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case SettingsEventExportCSV:
                mood_csv_export(app, MOOD_TRACKER_CSV_PATH);
                consumed = true; break;
            case SettingsEventImportCSV:
                mood_csv_import(app, MOOD_TRACKER_CSV_PATH);
                consumed = true; break;
            case SettingsEventBackup:
                mood_csv_export(app, MOOD_TRACKER_BAK_PATH);
                consumed = true; break;
            default: break;
        }
    }
    return consumed;
}
void mood_scene_settings_on_exit(void* ctx) {
    MoodTrackerApp* app = ctx;
    mood_config_save(app);
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int32_t mood_tracker_app(void* p) {
    UNUSED(p);

    MoodTrackerApp* app = mood_tracker_app_alloc();

    storage_simply_mkdir(app->storage, MOOD_TRACKER_APP_PATH);

    mood_config_load(app);
    mood_db_load(app);
    mood_stats_compute(app);
    mood_streak_compute(app);

    app->daily_quote_index =
        (uint8_t)(app->stats.total_days % MOOD_QUOTES_COUNT);

    scene_manager_next_scene(app->scene_manager, MoodSceneMain);
    view_dispatcher_run(app->view_dispatcher);

    mood_tracker_app_free(app);
    return 0;
}
