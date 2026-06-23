#include "graphs.h"
#include "../storage/database.h"
#include <gui/view.h>

// Graph canvas area
#define GFX_X       4
#define GFX_Y       12
#define GFX_W       110
#define GFX_H       42
#define GFX_LEGEND_X 116

typedef struct {
    MoodTrackerApp* app;
    MoodDayStats    points[MOOD_GRAPH_POINTS];
    uint32_t        point_count;
} GraphViewModel;

// ─── Draw ─────────────────────────────────────────────────────────────────────

// Map value 1..10 to Y pixel within graph area
static uint8_t val_to_y(float v) {
    // v=10 -> GFX_Y, v=1 -> GFX_Y+GFX_H
    float clamped = (v < 1) ? 1 : (v > 10) ? 10 : v;
    return (uint8_t)(GFX_Y + GFX_H - (uint8_t)((clamped - 1.0f) / 9.0f * GFX_H));
}

static void graph_draw_callback(Canvas* canvas, void* model_v) {
    GraphViewModel* m = model_v;
    MoodTrackerApp* app = m->app;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    const char* mode_str[] = {"M+S+E", "MOOD", "STRESS", "ENERGY"};
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "Graph:");
    canvas_draw_str(canvas, 36, 8, mode_str[app->current_graph_mode]);

    // Days shown
    char days_str[8];
    snprintf(days_str, sizeof(days_str), "%ud", app->graph_days_shown);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, days_str);

    // Graph border
    canvas_draw_frame(canvas, GFX_X - 1, GFX_Y - 1, GFX_W + 2, GFX_H + 2);

    // Y-axis labels (1, 5, 10)
    canvas_draw_str(canvas, 0, GFX_Y + 4, "10");
    canvas_draw_str(canvas, 0, GFX_Y + GFX_H / 2 + 2, "5");
    canvas_draw_str(canvas, 0, GFX_Y + GFX_H + 4, "1");

    if(m->point_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter,
            "No data");
        return;
    }

    // X step
    float x_step = (float)GFX_W / (float)(m->point_count > 1 ? m->point_count - 1 : 1);

    // Draw lines
    GraphMode mode = app->current_graph_mode;

    for(uint32_t i = 1; i < m->point_count; i++) {
        uint8_t x0 = GFX_X + (uint8_t)((i - 1) * x_step);
        uint8_t x1 = GFX_X + (uint8_t)(i * x_step);

        if(mode == GraphModeMoodStressEnergy || mode == GraphModeMoodOnly) {
            // Mood: solid line
            uint8_t y0 = val_to_y(m->points[i-1].avg_mood);
            uint8_t y1 = val_to_y(m->points[i].avg_mood);
            canvas_draw_line(canvas, x0, y0, x1, y1);
        }

        if(mode == GraphModeMoodStressEnergy || mode == GraphModeStressOnly) {
            // Stress: dashed (draw every other pixel)
            uint8_t y0 = val_to_y(m->points[i-1].avg_stress);
            uint8_t y1 = val_to_y(m->points[i].avg_stress);
            // Simple dashed: draw half the pixels
            int dx = (int)x1 - x0;
            int dy = (int)y1 - y0;
            int steps = (dx > dy ? dx : dy);
            if(steps < 1) steps = 1;
            for(int s = 0; s <= steps; s += 2) {
                uint8_t px = x0 + (uint8_t)(s * dx / steps);
                uint8_t py = y0 + (uint8_t)(s * (dy >= 0 ? dy : -dy) / steps);
                if(dy < 0) py = y0 - (uint8_t)(s * (-dy) / steps);
                canvas_draw_dot(canvas, px, py);
            }
        }

        if(mode == GraphModeMoodStressEnergy || mode == GraphModeEnergyOnly) {
            // Energy: dotted (every 3 pixels)
            uint8_t y0 = val_to_y(m->points[i-1].avg_energy);
            uint8_t y1 = val_to_y(m->points[i].avg_energy);
            int dx = (int)x1 - x0;
            int dy = (int)y1 - y0;
            int steps = (dx > dy ? dx : dy);
            if(steps < 1) steps = 1;
            for(int s = 0; s <= steps; s += 3) {
                uint8_t px = x0 + (uint8_t)(s * dx / steps);
                int py_i  = (int)y0 + (s * dy / steps);
                uint8_t py = (uint8_t)CLAMP(py_i, GFX_Y, GFX_Y + GFX_H);
                canvas_draw_dot(canvas, px, py);
            }
        }
    }

    // Legend (right side, tiny)
    if(mode == GraphModeMoodStressEnergy) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, GFX_LEGEND_X, 20, "M");
        canvas_draw_str(canvas, GFX_LEGEND_X, 30, "S");
        canvas_draw_str(canvas, GFX_LEGEND_X, 40, "E");
        // Legend line styles
        canvas_draw_line(canvas, GFX_LEGEND_X - 5, 18, GFX_LEGEND_X - 1, 18);     // solid
        canvas_draw_dot(canvas,  GFX_LEGEND_X - 5, 28);
        canvas_draw_dot(canvas,  GFX_LEGEND_X - 3, 28);                             // dashed
        canvas_draw_dot(canvas,  GFX_LEGEND_X - 5, 38);
        canvas_draw_dot(canvas,  GFX_LEGEND_X - 2, 38);                             // dotted
    }

    // Navigation hint
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignTop,
        "[OK]mode [<]zoom- [>]zoom+");
}

// ─── Input ────────────────────────────────────────────────────────────────────

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
            default: break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

// ─── Alloc / Free / Refresh ──────────────────────────────────────────────────

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
    View* view = app->view_graph;
    GraphViewModel* m = view_get_model(view);

    // Collect last N days of aggregated data
    FuriHalRtcDateTime now;
    furi_hal_rtc_get_datetime(&now);

    // Compute from_key (today - graph_days_shown)
    // Approximate: subtract days linearly
    uint32_t today_approx = (uint32_t)now.year * 10000 + now.month * 100 + now.day;
    // For simplicity use entry scanning
    uint32_t pts = mood_db_get_range(app,
        0,               // from beginning
        today_approx,    // to today
        m->points,
        MOOD_GRAPH_POINTS);

    // Take only last graph_days_shown points
    uint32_t skip = 0;
    if(pts > app->graph_days_shown) {
        skip = pts - app->graph_days_shown;
    }
    if(skip > 0) {
        memmove(m->points, m->points + skip, (pts - skip) * sizeof(MoodDayStats));
        pts -= skip;
    }

    m->point_count = pts;
    view_commit_model(view, true);
}
