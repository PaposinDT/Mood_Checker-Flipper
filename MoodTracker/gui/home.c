#include "home.h"
#include "../quotes/quotes.h"
#include "../storage/database.h"
#include <gui/view.h>

#define HOME_VISIBLE_ITEMS 5
#define MENU_ITEMS 6

static const char* menu_labels[MENU_ITEMS] = {
    "New Entry",
    "Graph",
    "Calendar",
    "Stats",
    "Heatmap",
    "Settings",
};

typedef struct {
    MoodTrackerApp* app;
    uint8_t cursor;
    uint8_t top;
    char today_str[24];
    char streak_str[20];
    bool has_today;
} HomeViewModel;

static void home_draw_callback(Canvas* canvas, void* model) {
    HomeViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Mood Tracker");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, m->has_today ? m->today_str : "No entry today");
    canvas_draw_str(canvas, 2, 32, m->streak_str);
    canvas_draw_line(canvas, 0, 36, 128, 36);

    for(uint8_t i = 0; i < HOME_VISIBLE_ITEMS; i++) {
        uint8_t item = m->top + i;
        if(item >= MENU_ITEMS) break;
        uint8_t y = 45 + i * 8;
        if(item == m->cursor) {
            canvas_draw_box(canvas, 0, y - 7, 128, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 2, y, ">");
            canvas_draw_str(canvas, 12, y, menu_labels[item]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 12, y, menu_labels[item]);
        }
    }

    if(m->top > 0) canvas_draw_str(canvas, 121, 45, "^");
    if((m->top + HOME_VISIBLE_ITEMS) < MENU_ITEMS) canvas_draw_str(canvas, 121, 61, "v");
}

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
                if(model->cursor < model->top) model->top = model->cursor;
                if(model->cursor >= model->top + HOME_VISIBLE_ITEMS) {
                    model->top = model->cursor - HOME_VISIBLE_ITEMS + 1;
                }
                consumed = true;
                break;
            case InputKeyDown:
                if(model->cursor < MENU_ITEMS - 1) model->cursor++;
                else model->cursor = 0;
                if(model->cursor < model->top) model->top = model->cursor;
                if(model->cursor >= model->top + HOME_VISIBLE_ITEMS) {
                    model->top = model->cursor - HOME_VISIBLE_ITEMS + 1;
                }
                consumed = true;
                break;
            case InputKeyOk: {
                uint32_t evt = 0;
                switch(model->cursor) {
                    case 0: evt = HomeEventNewEntry; break;
                    case 1: evt = HomeEventViewGraph; break;
                    case 2: evt = HomeEventViewCalendar; break;
                    case 3: evt = HomeEventViewStats; break;
                    case 4: evt = HomeEventViewHeatmap; break;
                    case 5: evt = HomeEventSettings; break;
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

void* home_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, home_draw_callback);
    view_set_input_callback(view, home_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(HomeViewModel));

    HomeViewModel* model = view_get_model(view);
    model->app = app;
    model->cursor = 0;
    model->top = 0;
    model->has_today = false;
    model->today_str[0] = '\0';
    model->streak_str[0] = '\0';
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
    if(!app || !app->view_home) return;
    View* view = app->view_home;
    HomeViewModel* model = view_get_model(view);

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t today_key = PACK_DATE(now.year, now.month, now.day);

    MoodEntry today_entries[MOOD_MAX_ENTRIES_PER_DAY];
    uint8_t cnt = mood_db_get_day(app, today_key, today_entries, MOOD_MAX_ENTRIES_PER_DAY);

    if(cnt > 0) {
        model->has_today = true;
        uint16_t sm = 0, ss = 0, se = 0;
        for(uint8_t i = 0; i < cnt; i++) {
            sm += today_entries[i].mood;
            ss += today_entries[i].stress;
            se += today_entries[i].energy;
        }
        snprintf(model->today_str, sizeof(model->today_str), "Today M%u S%u E%u", sm / cnt, ss / cnt, se / cnt);
    } else {
        model->has_today = false;
        strncpy(model->today_str, "No entry today", sizeof(model->today_str) - 1);
        model->today_str[sizeof(model->today_str) - 1] = '\0';
    }

    snprintf(model->streak_str, sizeof(model->streak_str), "Streak: %lu d", (unsigned long)app->stats.current_streak);

    view_commit_model(view, true);
}
