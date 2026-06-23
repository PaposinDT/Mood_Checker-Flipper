#include "calendar.h"
#include "../storage/database.h"
#include <gui/view.h>

// ─── Date helpers ─────────────────────────────────────────────────────────────

static uint8_t days_in_month(uint16_t y, uint8_t m) {
    static const uint8_t dom[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if(m < 1 || m > 12) return 30;
    uint8_t d = dom[m - 1];
    if(m == 2) {
        bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        if(leap) d = 29;
    }
    return d;
}

// Day of week: 0=Mon..6=Sun
static uint8_t weekday_of_first(uint16_t y, uint8_t m) {
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    uint16_t yy = y;
    if(m < 3) yy--;
    return (uint8_t)((yy + yy/4 - yy/100 + yy/400 + t[m-1] + 1) % 7);
}

// ─── Model ────────────────────────────────────────────────────────────────────

typedef struct {
    MoodTrackerApp* app;
    uint16_t        disp_year;
    uint8_t         disp_month;
    // Cached mood average per day (0 = no data, 1-10)
    float           day_mood[32];   // index by day number 1-31
    uint8_t         selected_day;
} CalendarViewModel;

// ─── Draw ─────────────────────────────────────────────────────────────────────

static void calendar_draw_callback(Canvas* canvas, void* model_v) {
    CalendarViewModel* m = model_v;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    static const char* months[] = {
        "","Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };

    // Header: "< Jan 2025 >"
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "<");
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, months[m->disp_month]);
    char yr[8];
    snprintf(yr, sizeof(yr), "%u", m->disp_year);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignTop, yr);
    canvas_draw_str(canvas, 122, 8, ">");

    // Day headers: M T W T F S S
    static const char* dh[] = {"M","T","W","T","F","S","S"};
    for(uint8_t i = 0; i < 7; i++) {
        canvas_draw_str(canvas, 4 + i * 18, 20, dh[i]);
    }
    canvas_draw_line(canvas, 0, 22, 128, 22);

    uint8_t first_wd = weekday_of_first(m->disp_year, m->disp_month);
    uint8_t total_d  = days_in_month(m->disp_year, m->disp_month);

    for(uint8_t day = 1; day <= total_d; day++) {
        uint8_t cell = first_wd + day - 1;
        uint8_t col  = cell % 7;
        uint8_t row  = cell / 7;

        uint8_t cx = 4 + col * 18;
        uint8_t cy = 24 + row * 8;

        float mood = m->day_mood[day];

        // Draw mood indicator
        if(mood > 0) {
            // Filled box: size proportional to mood (1=tiny, 10=full)
            uint8_t sz = (uint8_t)(mood / 10.0f * 5 + 1);
            uint8_t ox = (5 - sz) / 2;
            canvas_draw_box(canvas, cx + ox, cy + ox, sz, sz);
        }

        // Day number
        canvas_set_font(canvas, FontSecondary);
        char ds[4];
        snprintf(ds, sizeof(ds), "%u", day);
        canvas_draw_str(canvas, cx + 6, cy + 7, ds);

        // Selected day highlight
        if(day == m->selected_day) {
            canvas_draw_frame(canvas, cx - 1, cy - 1, 16, 9);
        }
    }

    // Bottom legend
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop,
        "[<>]month [OK]select");
}

// ─── Input ────────────────────────────────────────────────────────────────────

static bool calendar_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View* view = ctx;
    CalendarViewModel* m = view_get_model(view);
    MoodTrackerApp* app = m->app;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        switch(event->key) {
            case InputKeyLeft:
                view_dispatcher_send_custom_event(app->view_dispatcher,
                    CalendarEventPrevMonth);
                consumed = true;
                break;
            case InputKeyRight:
                view_dispatcher_send_custom_event(app->view_dispatcher,
                    CalendarEventNextMonth);
                consumed = true;
                break;
            default: break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

// ─── Alloc / Free / Refresh ──────────────────────────────────────────────────

void* calendar_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, calendar_draw_callback);
    view_set_input_callback(view, calendar_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(CalendarViewModel));

    CalendarViewModel* m = view_get_model(view);
    m->app = app;
    m->selected_day = 1;
    view_commit_model(view, false);

    app->view_calendar = view;
    return view;
}

void calendar_view_free(MoodTrackerApp* app) {
    if(app->view_calendar) {
        view_free(app->view_calendar);
        app->view_calendar = NULL;
    }
}

void calendar_view_refresh(MoodTrackerApp* app) {
    View* view = app->view_calendar;
    CalendarViewModel* m = view_get_model(view);

    FuriHalRtcDateTime now;
    furi_hal_rtc_get_datetime(&now);

    // Compute displayed month from offset
    int32_t total_months = (int32_t)now.year * 12 + now.month +
                            app->calendar_month_offset;
    m->disp_year  = (uint16_t)(total_months / 12);
    m->disp_month = (uint8_t)(total_months % 12);
    if(m->disp_month == 0) { m->disp_month = 12; m->disp_year--; }

    // Clear
    memset(m->day_mood, 0, sizeof(m->day_mood));

    // Fill mood data for displayed month
    uint8_t dmax = days_in_month(m->disp_year, m->disp_month);
    for(uint8_t d = 1; d <= dmax; d++) {
        uint32_t key = PACK_DATE(m->disp_year, m->disp_month, d);
        MoodDayStats stats;
        if(mood_db_get_day_stats(app, key, &stats)) {
            m->day_mood[d] = stats.avg_mood;
        }
    }

    // Default selected day
    if(app->calendar_month_offset == 0) {
        m->selected_day = now.day;
    } else {
        m->selected_day = 1;
    }

    view_commit_model(view, true);
}
