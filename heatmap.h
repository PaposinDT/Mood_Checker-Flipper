#include "calendar.h"
#include "../storage/database.h"
#include <gui/view.h>

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

static uint8_t weekday_of_first(uint16_t y, uint8_t m) {
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    uint16_t yy = y;
    if(m < 3) yy--;
    return (uint8_t)((yy + yy / 4 - yy / 100 + yy / 400 + t[m - 1] + 6) % 7);
}

typedef struct {
    MoodTrackerApp* app;
    uint16_t disp_year;
    uint8_t disp_month;
    float day_mood[32];
} CalendarViewModel;

static uint8_t mood_to_size(float mood) {
    if(mood <= 0.0f) return 0;
    if(mood < 4.0f) return 2;
    if(mood < 7.0f) return 3;
    return 5;
}

static void calendar_draw_callback(Canvas* canvas, void* model_v) {
    CalendarViewModel* m = model_v;

    static const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char title[20];
    snprintf(title, sizeof(title), "%s %u", months[m->disp_month], m->disp_year);
    canvas_draw_str(canvas, 0, 8, "<");
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, title);
    canvas_draw_str(canvas, 122, 8, ">");
    canvas_draw_line(canvas, 0, 10, 128, 10);

    const char* dh = "M  T  W  T  F  S  S";
    canvas_draw_str(canvas, 18, 19, dh);

    uint8_t first_wd = weekday_of_first(m->disp_year, m->disp_month);
    uint8_t total_d = days_in_month(m->disp_year, m->disp_month);

    bool has_data = false;
    for(uint8_t i = 1; i <= total_d; i++) {
        if(m->day_mood[i] > 0.0f) {
            has_data = true;
            break;
        }
    }

    uint8_t cell_w = 16;
    uint8_t cell_h = 7;
    uint8_t start_x = 14;
    uint8_t start_y = 24;

    for(uint8_t day = 1; day <= total_d; day++) {
        uint8_t cell = first_wd + day - 1;
        uint8_t col = cell % 7;
        uint8_t row = cell / 7;
        uint8_t x = start_x + col * cell_w;
        uint8_t y = start_y + row * cell_h;
        uint8_t sz = mood_to_size(m->day_mood[day]);

        if(sz == 0) {
            canvas_draw_frame(canvas, x, y, 5, 5);
        } else if(sz <= 3) {
            canvas_draw_box(canvas, x + 1, y + 1, sz, sz);
        } else {
            canvas_draw_box(canvas, x, y, 5, 5);
        }
    }

    if(!has_data) {
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Empty month");
    } else {
        canvas_draw_str(canvas, 2, 62, "Box size = mood");
    }
}

static bool calendar_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View* view = ctx;
    CalendarViewModel* m = view_get_model(view);
    MoodTrackerApp* app = m->app;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            view_dispatcher_send_custom_event(app->view_dispatcher, CalendarEventPrevMonth);
            consumed = true;
        } else if(event->key == InputKeyRight) {
            view_dispatcher_send_custom_event(app->view_dispatcher, CalendarEventNextMonth);
            consumed = true;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

void* calendar_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, calendar_draw_callback);
    view_set_input_callback(view, calendar_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(CalendarViewModel));

    CalendarViewModel* m = view_get_model(view);
    m->app = app;
    m->disp_year = 2026;
    m->disp_month = 1;
    memset(m->day_mood, 0, sizeof(m->day_mood));
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
    if(!app || !app->view_calendar) return;
    View* view = app->view_calendar;
    CalendarViewModel* m = view_get_model(view);

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    int32_t total_months = (int32_t)now.year * 12 + (int32_t)now.month - 1 + app->calendar_month_offset;
    m->disp_year = (uint16_t)(total_months / 12);
    m->disp_month = (uint8_t)(total_months % 12 + 1);

    memset(m->day_mood, 0, sizeof(m->day_mood));

    uint8_t dmax = days_in_month(m->disp_year, m->disp_month);
    for(uint8_t d = 1; d <= dmax; d++) {
        MoodDayStats stats;
        uint32_t key = PACK_DATE(m->disp_year, m->disp_month, d);
        if(mood_db_get_day_stats(app, key, &stats)) {
            m->day_mood[d] = stats.avg_mood;
        }
    }

    view_commit_model(view, true);
}
