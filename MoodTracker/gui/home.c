#include "home.h"
#include "../quotes/quotes.h"
#include <gui/view.h>
#include <gui/elements.h>

// ─── State ────────────────────────────────────────────────────────────────────

typedef struct {
    MoodTrackerApp* app;
    uint8_t         cursor;       // 0..5 menu items
    char            today_str[32];
    char            streak_str[24];
    char            quote_str[48];
    bool            has_today;
    uint8_t         today_mood;
    uint8_t         today_stress;
    uint8_t         today_energy;
} HomeViewModel;

#define MENU_ITEMS 6
static const char* menu_labels[MENU_ITEMS] = {
    "+ New Entry",
    "~ Graph",
    "@ Calendar",
    "# Stats",
    "% Heatmap",
    ". Settings",
};

// ─── Draw ─────────────────────────────────────────────────────────────────────

static void home_draw_callback(Canvas* canvas, void* model) {
    HomeViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Title bar ──
    canvas_draw_rframe(canvas, 0, 0, 128, 12, 0);
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "MOOD TRACKER");
    canvas_set_color(canvas, ColorBlack);

    // ── Today summary (top right) ──
    canvas_set_font(canvas, FontSecondary);
    if(m->has_today) {
        canvas_draw_str(canvas, 72, 22, m->today_str);
    } else {
        canvas_draw_str(canvas, 72, 22, "No entry today");
    }

    // ── Streak ──
    canvas_draw_str(canvas, 72, 32, m->streak_str);

    // ── Menu ──
    for(uint8_t i = 0; i < MENU_ITEMS; i++) {
        uint8_t y = 14 + i * 9;
        if(i == m->cursor) {
            canvas_draw_box(canvas, 0, y - 1, 68, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, y + 7, menu_labels[i]);
        if(i == m->cursor) canvas_set_color(canvas, ColorBlack);
    }

    // ── Quote (bottom) ──
    canvas_set_font(canvas, FontSecondary);
    // Truncate to fit 128px
    char q[33];
    strncpy(q, m->quote_str, 32);
    q[32] = '\0';
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop, q);

    // Bottom separator
    canvas_draw_line(canvas, 0, 55, 128, 55);
}

// ─── Input ────────────────────────────────────────────────────────────────────

static bool home_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View* view = ctx;
    HomeViewModel* model = view_get_model(view);
    MoodTrackerApp* app = model->app;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
            case InputKeyUp:
                if(model->cursor > 0) model->cursor--;
                else model->cursor = MENU_ITEMS - 1;
                consumed = true;
                break;
            case InputKeyDown:
                if(model->cursor < MENU_ITEMS - 1) model->cursor++;
                else model->cursor = 0;
                consumed = true;
                break;
            case InputKeyOk: {
                uint32_t evt = 0;
                switch(model->cursor) {
                    case 0: evt = HomeEventNewEntry;     break;
                    case 1: evt = HomeEventViewGraph;    break;
                    case 2: evt = HomeEventViewCalendar; break;
                    case 3: evt = HomeEventViewStats;    break;
                    case 4: evt = HomeEventViewHeatmap;  break;
                    case 5: evt = HomeEventSettings;     break;
                    default: break;
                }
                view_dispatcher_send_custom_event(app->view_dispatcher, evt);
                consumed = true;
                break;
            }
            default: break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

// ─── Alloc / Free ─────────────────────────────────────────────────────────────

void* home_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, home_draw_callback);
    view_set_input_callback(view, home_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(HomeViewModel));

    HomeViewModel* model = view_get_model(view);
    model->app    = app;
    model->cursor = 0;
    view_commit_model(view, false);

    app->view_home = view;
    return view;
}

void home_view_free(MoodTrackerApp* app) {
    if(app->view_home) {
        view_free(app->view_home);
        app->view_home = NULL;
    }
}

void home_view_refresh(MoodTrackerApp* app) {
    View* view = app->view_home;
    HomeViewModel* model = view_get_model(view);

    // Today's entry
    FuriHalRtcDateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t today_key = PACK_DATE(now.year, now.month, now.day);

    MoodEntry today_entries[MOOD_MAX_ENTRIES_PER_DAY];
    uint8_t cnt = mood_db_get_day(app, today_key, today_entries, MOOD_MAX_ENTRIES_PER_DAY);

    if(cnt > 0) {
        model->has_today = true;
        float sm = 0, ss = 0, se = 0;
        for(uint8_t i = 0; i < cnt; i++) {
            sm += today_entries[i].mood;
            ss += today_entries[i].stress;
            se += today_entries[i].energy;
        }
        model->today_mood   = (uint8_t)(sm / cnt + 0.5f);
        model->today_stress = (uint8_t)(ss / cnt + 0.5f);
        model->today_energy = (uint8_t)(se / cnt + 0.5f);
        snprintf(model->today_str, sizeof(model->today_str),
            "M%u S%u E%u",
            model->today_mood,
            model->today_stress,
            model->today_energy);
    } else {
        model->has_today = false;
        strncpy(model->today_str, "No entry today", sizeof(model->today_str) - 1);
    }

    // Streak
    snprintf(model->streak_str, sizeof(model->streak_str),
        "Streak: %lu days",
        (unsigned long)app->stats.current_streak);

    // Quote (rotate daily)
    const char* q = mood_quote_get(app->daily_quote_index);
    strncpy(model->quote_str, q, sizeof(model->quote_str) - 1);

    view_commit_model(view, true);
}
