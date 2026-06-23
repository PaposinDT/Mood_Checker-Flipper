#include "stats.h"
#include <gui/view.h>

static const char* weekday_names[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};

typedef struct {
    MoodTrackerApp* app;
    uint8_t         scroll;  // scroll offset in lines
    uint8_t         max_scroll;
} StatsViewModel;

#define STATS_LINES 18
#define LINES_PER_SCREEN 6

static void stats_draw_callback(Canvas* canvas, void* model_v) {
    StatsViewModel* m = model_v;
    MoodTrackerApp* app = m->app;
    MoodStats* s = &app->stats;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "STATISTICS");
    canvas_set_color(canvas, ColorBlack);

    // Build stat lines
    char lines[STATS_LINES][40];
    uint8_t lc = 0;

    snprintf(lines[lc++], 40, "Total entries: %lu", (unsigned long)s->total_entries);
    snprintf(lines[lc++], 40, "Days tracked: %lu",  (unsigned long)s->total_days);
    snprintf(lines[lc++], 40, "Streak: %lu days",   (unsigned long)s->current_streak);
    snprintf(lines[lc++], 40, "Best streak: %lu",   (unsigned long)s->longest_streak);
    snprintf(lines[lc++], 40, "Avg Mood: %.1f",     (double)s->all_time_avg_mood);
    snprintf(lines[lc++], 40, "Avg Stress: %.1f",   (double)s->all_time_avg_stress);
    snprintf(lines[lc++], 40, "Avg Energy: %.1f",   (double)s->all_time_avg_energy);
    snprintf(lines[lc++], 40, "Best mood: %.1f",    (double)s->best_mood);
    snprintf(lines[lc++], 40, "Worst mood: %.1f",   (double)s->worst_mood);
    snprintf(lines[lc++], 40, "Best day: %s",
        (s->total_days > 0) ? weekday_names[s->best_weekday % 7] : "-");
    snprintf(lines[lc++], 40, "Worst day: %s",
        (s->total_days > 0) ? weekday_names[s->worst_weekday % 7] : "-");
    snprintf(lines[lc++], 40, "Mood-Stress r: %.2f", (double)s->mood_stress_corr);
    snprintf(lines[lc++], 40, "Mood-Energy r: %.2f", (double)s->mood_energy_corr);

    // Correlation interpretation
    if(s->total_days >= 7) {
        const char* ms = (s->mood_stress_corr > 0.3f) ? "stress up=mood up" :
                         (s->mood_stress_corr < -0.3f)? "stress up=mood dn" : "no link";
        const char* me = (s->mood_energy_corr > 0.3f) ? "energy up=mood up" :
                         (s->mood_energy_corr < -0.3f)? "energy up=mood dn" : "no link";
        snprintf(lines[lc++], 40, "  -> %s", ms);
        snprintf(lines[lc++], 40, "  -> %s", me);
    }

    m->max_scroll = (lc > LINES_PER_SCREEN) ? (lc - LINES_PER_SCREEN) : 0;

    // Draw visible lines
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < LINES_PER_SCREEN && (m->scroll + i) < lc; i++) {
        canvas_draw_str(canvas, 2, 14 + i * 9, lines[m->scroll + i]);
    }

    // Scrollbar
    if(m->max_scroll > 0) {
        uint8_t sb_h = (uint8_t)(40 * LINES_PER_SCREEN / lc);
        uint8_t sb_y = 12 + (uint8_t)(40 * m->scroll / lc);
        canvas_draw_frame(canvas, 125, 12, 3, 40);
        canvas_draw_box(canvas, 126, sb_y, 1, sb_h);
    }

    // Nav hint
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop, "[up/dn] scroll");
}

static bool stats_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View* view = ctx;
    StatsViewModel* m = view_get_model(view);
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
            case InputKeyUp:
                if(m->scroll > 0) m->scroll--;
                consumed = true;
                break;
            case InputKeyDown:
                if(m->scroll < m->max_scroll) m->scroll++;
                consumed = true;
                break;
            default: break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

void* stats_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, stats_draw_callback);
    view_set_input_callback(view, stats_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(StatsViewModel));

    StatsViewModel* m = view_get_model(view);
    m->app = app;
    m->scroll = 0;
    m->max_scroll = 0;
    view_commit_model(view, false);

    app->view_stats = view;
    return view;
}

void stats_view_free(MoodTrackerApp* app) {
    if(app->view_stats) {
        view_free(app->view_stats);
        app->view_stats = NULL;
    }
}

void stats_view_refresh(MoodTrackerApp* app) {
    View* view = app->view_stats;
    StatsViewModel* m = view_get_model(view);
    m->scroll = 0;
    view_commit_model(view, true);
}
