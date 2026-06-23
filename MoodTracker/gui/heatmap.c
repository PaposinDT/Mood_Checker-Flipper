#include "heatmap.h"
#include "../storage/database.h"
#include <gui/view.h>

// ─── Cell geometry ────────────────────────────────────────────────────────────
// 18 weeks × 7 days = 126 cells
// Each cell: 4px wide, 1px gap → 5px/col  (18*5=90px, fits in 128)
// Each cell: 7px tall, 1px gap → 8px/row  (7*8=56px, fits in 64)

#define HM_CW   4   // cell width
#define HM_CH   6   // cell height
#define HM_GX   1   // horizontal gap
#define HM_GY   1   // vertical gap
#define HM_X0   7   // left margin (for day labels M/W/F)
#define HM_Y0   9   // top margin (for month labels)

// ─── Accurate date helpers ────────────────────────────────────────────────────

// Days since a fixed epoch (1 Jan 2000) — no approximation
static uint32_t date_to_days(uint16_t y, uint8_t m, uint8_t d) {
    // Rata Die algorithm (accurate for all Gregorian dates)
    static const uint16_t days_before_month[] =
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t days = (uint32_t)(y - 1) * 365
                  + (y - 1) / 4
                  - (y - 1) / 100
                  + (y - 1) / 400
                  + days_before_month[m - 1]
                  + d;
    bool leap = (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0));
    if(leap && m > 2) days++;
    return days;
}

// Convert days back to year/month/day
static void days_to_date(uint32_t n, uint16_t* y, uint8_t* m, uint8_t* d) {
    // Gregorian calendar reverse conversion
    uint32_t z     = n;
    uint32_t era   = (z >= 1 ? z - 1 : z) / 146097;
    uint32_t doe   = z - era * 146097;
    uint32_t yoe   = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    uint32_t y_val = yoe + era * 400;
    uint32_t doy   = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint32_t mp    = (5 * doy + 2) / 153;
    uint32_t d_val = doy - (153 * mp + 2) / 5 + 1;
    uint32_t m_val = (mp < 10) ? mp + 3 : mp - 9;
    if(m_val <= 2) y_val++;
    *y = (uint16_t)y_val;
    *m = (uint8_t)m_val;
    *d = (uint8_t)d_val;
}

// Day of week from Rata Die day number: 0=Mon … 6=Sun
static uint8_t day_of_week_from_days(uint32_t n) {
    return (uint8_t)((n + 6) % 7);   // 1 Jan 0001 = Monday
}

// ─── Cell fill level 0..4 from avg mood ──────────────────────────────────────

static uint8_t mood_to_level(float avg) {
    if(avg <= 0)    return 0;
    if(avg <= 2.5f) return 1;
    if(avg <= 5.0f) return 2;
    if(avg <= 7.5f) return 3;
    return 4;
}

// ─── Draw a single cell ───────────────────────────────────────────────────────

static void draw_cell(Canvas* canvas, uint8_t px, uint8_t py, uint8_t level) {
    switch(level) {
        case 0:
            canvas_draw_frame(canvas, px, py, HM_CW, HM_CH);
            break;
        case 1:
            canvas_draw_frame(canvas, px, py, HM_CW, HM_CH);
            canvas_draw_dot(canvas, px + 1, py + 2);
            break;
        case 2: {
            uint8_t half = HM_CH / 2 + 1;
            canvas_draw_frame(canvas, px, py, HM_CW, HM_CH);
            canvas_draw_box(canvas, px + 1, py + HM_CH - half, HM_CW - 2, half - 1);
            break;
        }
        case 3:
            canvas_draw_box(canvas, px, py, HM_CW, HM_CH);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_dot(canvas, px + 1, py + 1);
            canvas_set_color(canvas, ColorBlack);
            break;
        case 4:
        default:
            canvas_draw_box(canvas, px, py, HM_CW, HM_CH);
            break;
    }
}

// ─── Draw callback ────────────────────────────────────────────────────────────

static void heatmap_draw_callback(Canvas* canvas, void* model_v) {
    HeatmapViewModel* m   = model_v;
    MoodTrackerApp*   app = m->app;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "HEATMAP");

    FuriHalRtcDateTime now;
    furi_hal_rtc_get_datetime(&now);

    uint32_t today_n  = date_to_days(now.year, now.month, now.day);
    uint8_t  today_wd = day_of_week_from_days(today_n); // 0=Mon

    // Total cells = MOOD_HEATMAP_WEEKS * 7
    // Rightmost column = current week; today is in that column at row today_wd.
    // Cell (week, wd) → days_ago = (MOOD_HEATMAP_WEEKS-1-week)*7 + (today_wd - wd)
    // Negative days_ago → future → draw empty.

    uint8_t prev_month = 0;

    for(uint8_t week = 0; week < MOOD_HEATMAP_WEEKS; week++) {
        for(uint8_t wd = 0; wd < 7; wd++) {
            int32_t days_ago = (int32_t)(MOOD_HEATMAP_WEEKS - 1 - week) * 7
                             + ((int32_t)today_wd - (int32_t)wd);

            uint8_t px = HM_X0 + week * (HM_CW + HM_GX);
            uint8_t py = HM_Y0 + wd   * (HM_CH + HM_GY);

            if(days_ago < 0) {
                // Future cell
                canvas_draw_frame(canvas, px, py, HM_CW, HM_CH);
                continue;
            }

            uint32_t cell_n = today_n - (uint32_t)days_ago;
            uint16_t cy; uint8_t cm, cd;
            days_to_date(cell_n, &cy, &cm, &cd);

            // Draw month label when month changes (top of column, first row)
            if(wd == 0 && cm != prev_month) {
                static const char* ml[] = {
                    "","J","F","M","A","M","J","J","A","S","O","N","D"
                };
                canvas_draw_str(canvas, px, HM_Y0 - 1, ml[cm]);
                prev_month = cm;
            }

            uint32_t key = PACK_DATE(cy, cm, cd);
            MoodDayStats stats;
            uint8_t level = 0;
            if(mood_db_get_day_stats(app, key, &stats)) {
                level = mood_to_level(stats.avg_mood);
            }

            draw_cell(canvas, px, py, level);
        }
    }

    // Day-of-week labels on left side
    canvas_set_font(canvas, FontSecondary);
    static const char* dl[] = {"M", " ", "W", " ", "F", " ", "S"};
    for(uint8_t wd = 0; wd < 7; wd++) {
        uint8_t py = HM_Y0 + wd * (HM_CH + HM_GY) + HM_CH - 1;
        canvas_draw_str(canvas, 0, py, dl[wd]);
    }

    // Legend bottom-right
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 100, 63, "1 2 3 4");
    canvas_draw_str(canvas,  88, 63, "lo");
    canvas_draw_frame(canvas, 99,  58, 4, 4);
    canvas_draw_dot (canvas, 101,  60);
    canvas_draw_box (canvas, 104,  58, 4, 4);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot (canvas, 106,  59);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box (canvas, 109,  58, 4, 4);

    // Navigation hint
    canvas_draw_str_aligned(canvas, 44, 58, AlignCenter, AlignTop,
        "[Back] exit");
}

// ─── Input callback ───────────────────────────────────────────────────────────

static bool heatmap_input_callback(InputEvent* event, void* ctx) {
    UNUSED(event);
    UNUSED(ctx);
    // All navigation (Back) is handled by the view dispatcher
    return false;
}

// ─── Alloc / Free / Refresh ──────────────────────────────────────────────────

void* heatmap_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, heatmap_draw_callback);
    view_set_input_callback(view, heatmap_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(HeatmapViewModel));

    HeatmapViewModel* m = view_get_model(view);
    m->app = app;
    view_commit_model(view, false);

    app->view_heatmap = view;
    return view;
}

void heatmap_view_free(MoodTrackerApp* app) {
    if(app->view_heatmap) {
        view_free(app->view_heatmap);
        app->view_heatmap = NULL;
    }
}

void heatmap_view_refresh(MoodTrackerApp* app) {
    View* view = app->view_heatmap;
    view_get_model(view);
    view_commit_model(view, true);
}
