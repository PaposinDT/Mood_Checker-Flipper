#include "graphs.h"
#include "../storage/database.h"
#include <gui/view.h>

#define GFX_X 13
#define GFX_Y 16
#define GFX_W 101
#define GFX_H 34

typedef struct {
    MoodTrackerApp* app;
    MoodDayStats points[MOOD_GRAPH_POINTS];
    uint32_t point_count;
} GraphViewModel;

static uint8_t val_to_y(float v) {
    float clamped = (v < 1.0f) ? 1.0f : (v > 10.0f) ? 10.0f : v;
    return (uint8_t)(GFX_Y + GFX_H - (uint8_t)(((clamped - 1.0f) * GFX_H) / 9.0f));
}

static void draw_dotted_line(Canvas* canvas, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t step_px) {
    int dx = (int)x1 - (int)x0;
    int dy = (int)y1 - (int)y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    if(steps < 1) steps = 1;

    for(int s = 0; s <= steps; s += step_px) {
        int px = (int)x0 + (s * dx) / steps;
        int py = (int)y0 + (s * dy) / steps;
        if(px >= 0 && px < 128 && py >= 0 && py < 64) canvas_draw_dot(canvas, px, py);
    }
}

static void graph_draw_callback(Canvas* canvas, void* model_v) {
    GraphViewModel* m = model_v;
    MoodTrackerApp* app = m->app;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    const char* mode_str[] = {"M+S+E", "Mood", "Stress", "Energy"};
    char title[24];
    snprintf(title, sizeof(title), "%s %ud", mode_str[app->current_graph_mode], app->graph_days_shown);
    canvas_draw_str(canvas, 2, 8, title);
    canvas_draw_line(canvas, 0, 10, 128, 10);

    if(m->point_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "No data yet");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Add first entry");
        canvas_draw_str(canvas, 2, 62, "OK mode  < > range");
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, GFX_Y + 2, "10");
    canvas_draw_str(canvas, 4, GFX_Y + GFX_H / 2 + 2, "5");
    canvas_draw_str(canvas, 4, GFX_Y + GFX_H + 2, "1");
    canvas_draw_frame(canvas, GFX_X, GFX_Y, GFX_W, GFX_H);

    if(m->point_count < 2) {
        uint8_t x = GFX_X + GFX_W / 2;
        if(app->current_graph_mode == GraphModeMoodStressEnergy || app->current_graph_mode == GraphModeMoodOnly) {
            canvas_draw_disc(canvas, x, val_to_y(m->points[0].avg_mood), 1);
        }
        if(app->current_graph_mode == GraphModeMoodStressEnergy || app->current_graph_mode == GraphModeStressOnly) {
            canvas_draw_dot(canvas, x + 3, val_to_y(m->points[0].avg_stress));
        }
        if(app->current_graph_mode == GraphModeMoodStressEnergy || app->current_graph_mode == GraphModeEnergyOnly) {
            canvas_draw_dot(canvas, x + 6, val_to_y(m->points[0].avg_energy));
        }
    } else {
        float x_step = (float)(GFX_W - 2) / (float)(m->point_count - 1);
        for(uint32_t i = 1; i < m->point_count; i++) {
            uint8_t x0 = GFX_X + 1 + (uint8_t)((i - 1) * x_step);
            uint8_t x1 = GFX_X + 1 + (uint8_t)(i * x_step);

            if(app->current_graph_mode == GraphModeMoodStressEnergy || app->current_graph_mode == GraphModeMoodOnly) {
                canvas_draw_line(canvas, x0, val_to_y(m->points[i - 1].avg_mood), x1, val_to_y(m->points[i].avg_mood));
            }
            if(app->current_graph_mode == GraphModeMoodStressEnergy || app->current_graph_mode == GraphModeStressOnly) {
                draw_dotted_line(canvas, x0, val_to_y(m->points[i - 1].avg_stress), x1, val_to_y(m->points[i].avg_stress), 2);
            }
            if(app->current_graph_mode == GraphModeMoodStressEnergy || app->current_graph_mode == GraphModeEnergyOnly) {
                draw_dotted_line(canvas, x0, val_to_y(m->points[i - 1].avg_energy), x1, val_to_y(m->points[i].avg_energy), 4);
            }
        }
    }

    canvas_set_font(canvas, FontSecondary);
    if(app->current_graph_mode == GraphModeMoodStressEnergy) {
        canvas_draw_str(canvas, 2, 58, "M=line S=dash E=dot");
    } else {
        canvas_draw_str(canvas, 2, 58, "OK mode  < > range");
    }
}

static bool graph_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View* view = ctx;
    GraphViewModel* m = view_get_model(view);
    MoodTrackerApp* app = m->app;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        switch(event->key) {
            case InputKeyOk:
                view_dispatcher_send_custom_event(app->view_dispatcher, GraphEventToggleMode);
                consumed = true;
                break;
            case InputKeyLeft:
                view_dispatcher_send_custom_event(app->view_dispatcher, GraphEventZoomIn);
                consumed = true;
                break;
            case InputKeyRight:
                view_dispatcher_send_custom_event(app->view_dispatcher, GraphEventZoomOut);
                consumed = true;
                break;
            default:
                break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

void* graph_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, graph_draw_callback);
    view_set_input_callback(view, graph_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(GraphViewModel));

    GraphViewModel* m = view_get_model(view);
    m->app = app;
    m->point_count = 0;
    view_commit_model(view, false);

    app->view_graph = view;
    return view;
}

void graph_view_free(MoodTrackerApp* app) {
    if(app->view_graph) {
        view_free(app->view_graph);
        app->view_graph = NULL;
    }
}

void graph_view_refresh(MoodTrackerApp* app) {
    if(!app || !app->view_graph) return;
    View* view = app->view_graph;
    GraphViewModel* m = view_get_model(view);
    m->point_count = 0;

    if(app->entry_count == 0) {
        view_commit_model(view, true);
        return;
    }

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t today_key = PACK_DATE(now.year, now.month, now.day);

    uint32_t pts = mood_db_get_range(app, 0, today_key, m->points, MOOD_GRAPH_POINTS);
    uint32_t max_points = app->graph_days_shown;
    if(max_points > MOOD_GRAPH_POINTS) max_points = MOOD_GRAPH_POINTS;
    if(pts > max_points) {
        uint32_t skip = pts - max_points;
        memmove(m->points, m->points + skip, (pts - skip) * sizeof(MoodDayStats));
        pts -= skip;
    }

    m->point_count = pts;
    view_commit_model(view, true);
}
