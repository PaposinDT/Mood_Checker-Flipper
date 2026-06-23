#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// ─── Constants ───────────────────────────────────────────────────────────────

#define MOOD_TRACKER_APP_PATH       "/ext/apps_data/mood_tracker"
#define MOOD_TRACKER_CSV_PATH       "/ext/apps_data/mood_tracker/entries.csv"
#define MOOD_TRACKER_IDX_PATH       "/ext/apps_data/mood_tracker/entries.idx"
#define MOOD_TRACKER_CFG_PATH       "/ext/apps_data/mood_tracker/config.cfg"
#define MOOD_TRACKER_BAK_PATH       "/ext/apps_data/mood_tracker/backup.csv"

#define MOOD_MAX_ENTRIES            1024
#define MOOD_NOTE_MAX_LEN           64
#define MOOD_PIN_MAX_LEN            6
#define MOOD_MAX_ENTRIES_PER_DAY    4
#define MOOD_GRAPH_POINTS           90
#define MOOD_HEATMAP_WEEKS          18
#define MOOD_QUOTES_COUNT           30

#define MOOD_MIN_VALUE              1
#define MOOD_MAX_VALUE              10

// ─── Enumerations ────────────────────────────────────────────────────────────

typedef enum {
    MoodViewHome = 0,
    MoodViewInput,
    MoodViewGraph,
    MoodViewCalendar,
    MoodViewStats,
    MoodViewHeatmap,
    MoodViewSettings,
    MoodViewTextInput,
    MoodViewCount,
} MoodView;

typedef enum {
    MoodSceneMain = 0,
    MoodSceneHome,
    MoodSceneInput,
    MoodSceneInputNote,
    MoodSceneGraph,
    MoodSceneCalendar,
    MoodSceneStats,
    MoodSceneHeatmap,
    MoodSceneSettings,
    MoodSceneCount,
} MoodScene;

typedef enum {
    MoodThemeLight = 0,
    MoodThemeDark,
} MoodTheme;

typedef enum {
    GraphModeMoodStressEnergy = 0,
    GraphModeMoodOnly,
    GraphModeStressOnly,
    GraphModeEnergyOnly,
} GraphMode;

// ─── Data Structures ─────────────────────────────────────────────────────────

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  mood;
    uint8_t  stress;
    uint8_t  energy;
    char     note[MOOD_NOTE_MAX_LEN];
    bool     is_primary;
} MoodEntry;

typedef struct {
    uint32_t date_key;
    uint32_t csv_offset;
    uint8_t  entry_count;
} MoodIndexRecord;

typedef struct {
    uint32_t date_key;
    float    avg_mood;
    float    avg_stress;
    float    avg_energy;
    uint8_t  entry_count;
} MoodDayStats;

typedef struct {
    uint32_t total_entries;
    uint32_t total_days;
    uint32_t current_streak;
    uint32_t longest_streak;
    float    all_time_avg_mood;
    float    all_time_avg_stress;
    float    all_time_avg_energy;
    float    best_mood;
    float    worst_mood;
    uint8_t  best_weekday;
    uint8_t  worst_weekday;
    float    mood_stress_corr;
    float    mood_energy_corr;
} MoodStats;

typedef struct {
    MoodTheme theme;
    bool      pin_enabled;
    char      pin[MOOD_PIN_MAX_LEN + 1];
    bool      show_quote_on_start;
    bool      reminder_enabled;
    uint8_t   reminder_hour;
    uint8_t   reminder_minute;
    GraphMode default_graph_mode;
    uint8_t   graph_days;
} MoodConfig;

// ─── Main App State ──────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;
    Storage*         storage;
    NotificationApp* notifications;

    void* view_home;
    void* view_input;
    void* view_graph;
    void* view_calendar;
    void* view_stats;
    void* view_heatmap;

    Popup*            popup;
    TextInput*        text_input;
    Widget*           widget;
    VariableItemList* var_item_list;

    MoodEntry    entries[MOOD_MAX_ENTRIES];
    uint32_t     entry_count;
    MoodConfig   config;
    MoodStats    stats;

    MoodEntry    pending_entry;
    uint8_t      input_step;

    GraphMode    current_graph_mode;
    uint8_t      graph_days_shown;
    int32_t      calendar_month_offset;

    uint8_t      daily_quote_index;
    bool         data_dirty;
} MoodTrackerApp;

// ─── Entry point ─────────────────────────────────────────────────────────────

int32_t mood_tracker_app(void* p);

// ─── Utility Macros ──────────────────────────────────────────────────────────

#define PACK_DATE(y, m, d)   ((uint32_t)(y) * 10000u + (m) * 100u + (d))
#define UNPACK_YEAR(k)       ((k) / 10000u)
#define UNPACK_MONTH(k)      (((k) % 10000u) / 100u)
#define UNPACK_DAY(k)        ((k) % 100u)

#define ARRAY_SIZE(a)        (sizeof(a) / sizeof((a)[0]))
