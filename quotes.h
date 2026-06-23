#include "stats.h"
#include <gui/view.h>

static const char* weekday_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

typedef struct {
    MoodTrackerApp* app;
    uint8_t scroll;
    uint8_t max_scroll;
} StatsViewModel;

#define STATS_LINES 14
#define LINES_PER_SCREEN 5

static void stats_fmt_1(char* out, size_t out_size, const char* label, float value) {
    int32_t v10 = (int32_t)(value * 10.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
    snprintf(out, out_size, "%s: %ld.%ld", label, (long)(v10 / 10), (long)abs(v10 % 10));
}

static void stats_fmt_2(char* out, size_t out_size, const char* label, float value) {
    int32_t v100 = (int32_t)(value * 100.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
    const char* sign = "";
    if(v100 < 0) {
        sign = "-";
        v100 = -v100;
    }
    snprintf(out, out_size, "%s: %s%ld.%02ld", label, sign, (long)(v100 / 100), (long)(v100 % 100));
}

static void stats_draw_callback(Canvas* canvas, void* model_v) {
    StatsViewModel* m = model_v;
    MoodTrackerApp* app = m->app;
    MoodStats* s = &app->stats;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Stats");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(app->entry_count == 0 || s->total_entries == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No data yet");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, "Add first entry");
        canvas_draw_str(canvas, 2, 62, "Back: return");
        m->max_scroll = 0;
        return;
    }

    char lines[STATS_LINES][32];
    uint8_t lc = 0;

    snprintf(lines[lc++], sizeof(lines[0]), "Entries: %lu", (unsigned long)s->total_entries);
    snprintf(lines[lc++], sizeof(lines[0]), "Days: %lu", (unsigned long)s->total_days);
    snprintf(lines[lc++], sizeof(lines[0]), "Streak: %lu d", (unsigned long)s->current_streak);
    snprintf(lines[lc++], sizeof(lines[0]), "Best streak: %lu d", (unsigned long)s->longest_streak);
    stats_fmt_1(lines[lc++], sizeof(lines[0]), "Mood avg", s->all_time_avg_mood);
    stats_fmt_1(lines[lc++], sizeof(lines[0]), "Stress avg", s->all_time_avg_stress);
    stats_fmt_1(lines[lc++], sizeof(lines[0]), "Energy avg", s->all_time_avg_energy);
    stats_fmt_1(lines[lc++], sizeof(lines[0]), "Best mood", s->best_mood);
    stats_fmt_1(lines[lc++], sizeof(lines[0]), "Worst mood", s->worst_mood);
    snprintf(lines[lc++], sizeof(lines[0]), "Best day: %s", weekday_names[s->best_weekday % 7]);
    snprintf(lines[lc++], sizeof(lines[0]), "Worst day: %s", weekday_names[s->worst_weekday % 7]);
    stats_fmt_2(lines[lc++], sizeof(lines[0]), "Mood/Stress", s->mood_stress_corr);
    stats_fmt_2(lines[lc++], sizeof(lines[0]), "Mood/Energy", s->mood_energy_corr);

    m->max_scroll = (lc > LINES_PER_SCREEN) ? (lc - LINES_PER_SCREEN) : 0;
    if(m->scroll > m->max_scroll) m->scroll = m->max_scroll;

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < LINES_PER_SCREEN && (m->scroll + i) < lc; i++) {
        canvas_draw_str(canvas, 2, 23 + i * 8, lines[m->scroll + i]);
    }

    if(m->max_scroll > 0) {
        canvas_draw_frame(canvas, 124, 16, 3, 40);
        uint8_t sb_y = 17 + (uint8_t)((36 * m->scroll) / m->max_scroll);
        canvas_draw_box(canvas, 125, sb_y, 1, 4);
    }

    canvas_draw_str(canvas, 2, 62, "Up/Dn scroll");
}

static bool stats_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View* view = ctx;
    StatsViewModel* m = view_get_model(view);
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            if(m->scroll > 0) m->scroll--;
            consumed = true;
        } else if(event->key == InputKeyDown) {
            if(m->scroll < m->max_scroll) m->scroll++;
            consumed = true;
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
    if(!app || !app->view_stats) return;
    View* view = app->view_stats;
    StatsViewModel* m = view_get_model(view);
    m->scroll = 0;
    m->max_scroll = 0;
    view_commit_model(view, true);
}
