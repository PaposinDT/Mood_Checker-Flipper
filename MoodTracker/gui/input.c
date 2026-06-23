#include "input.h"
#include <gui/view.h>
#include <gui/elements.h>

// ─── Labels ───────────────────────────────────────────────────────────────────

static const char* mood_label(uint8_t v) {
    if(v <= 2)  return "Terrible";
    if(v <= 4)  return "Bad";
    if(v <= 6)  return "Okay";
    if(v <= 8)  return "Good";
    return "Great!";
}

static const char* stress_label(uint8_t v) {
    if(v <= 2)  return "Calm";
    if(v <= 4)  return "Mild";
    if(v <= 6)  return "Moderate";
    if(v <= 8)  return "High";
    return "Max!";
}

static const char* energy_label(uint8_t v) {
    if(v <= 2)  return "Drained";
    if(v <= 4)  return "Low";
    if(v <= 6)  return "Normal";
    if(v <= 8)  return "High";
    return "Wired!";
}

// ─── Model ────────────────────────────────────────────────────────────────────

typedef struct {
    MoodTrackerApp* app;
} InputViewModel;

// ─── Draw ─────────────────────────────────────────────────────────────────────

static void input_draw_callback(Canvas* canvas, void* model_v) {
    InputViewModel* m   = model_v;
    MoodTrackerApp* app = m->app;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    const char* titles[] = {"MOOD", "STRESS", "ENERGY"};
    uint8_t     step     = app->input_step;
    if(step > 2) step = 2;

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, titles[step]);

    // Step indicator (dots)
    for(uint8_t i = 0; i < 3; i++) {
        uint8_t x = 52 + i * 12;
        if(i == step)
            canvas_draw_box(canvas, x, 11, 8, 4);
        else
            canvas_draw_frame(canvas, x, 11, 8, 4);
    }

    // Current value (big)
    uint8_t val = 5;
    if(step == 0)       val = app->pending_entry.mood;
    else if(step == 1)  val = app->pending_entry.stress;
    else                val = app->pending_entry.energy;

    char val_str[4];
    snprintf(val_str, sizeof(val_str), "%u", val);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, val_str);

    // Text label
    canvas_set_font(canvas, FontSecondary);
    const char* lbl = "";
    if(step == 0)       lbl = mood_label(val);
    else if(step == 1)  lbl = stress_label(val);
    else                lbl = energy_label(val);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, lbl);

    // Slider bar
    uint8_t bar_x = 4;
    uint8_t bar_y = 46;
    uint8_t bar_w = 120;
    uint8_t bar_h = 6;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    uint8_t fill = (uint8_t)(((uint32_t)(val - 1) * (bar_w - 2)) / 9);
    if(fill > 0)
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill, bar_h - 2);

    for(uint8_t i = 0; i <= 9; i++) {
        uint8_t tx = bar_x + 1 + (uint8_t)(i * (bar_w - 2) / 9);
        canvas_draw_line(canvas, tx, bar_y + bar_h, tx, bar_y + bar_h + 2);
    }

    // Hint
    canvas_set_font(canvas, FontSecondary);
    if(step < 2) {
        canvas_draw_str_aligned(
            canvas, 64, 56, AlignCenter, AlignTop, "[OK] Next  [Back] Cancel");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 56, AlignCenter, AlignTop, "[OK] Add note  [Back] Cancel");
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────

static bool input_input_callback(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    View*           view = ctx;
    InputViewModel* m    = view_get_model(view);
    MoodTrackerApp* app  = m->app;
    bool            consumed = false;

    uint8_t  step    = app->input_step;
    if(step > 2) step = 2;
    uint8_t* val_ptr = NULL;
    if(step == 0)       val_ptr = &app->pending_entry.mood;
    else if(step == 1)  val_ptr = &app->pending_entry.stress;
    else                val_ptr = &app->pending_entry.energy;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
            case InputKeyRight:
                if(*val_ptr < MOOD_MAX_VALUE) (*val_ptr)++;
                consumed = true;
                break;
            case InputKeyLeft:
                if(*val_ptr > MOOD_MIN_VALUE) (*val_ptr)--;
                consumed = true;
                break;
            case InputKeyUp:
                *val_ptr = (uint8_t)CLAMP((int)*val_ptr + 2,
                    MOOD_MIN_VALUE, MOOD_MAX_VALUE);
                consumed = true;
                break;
            case InputKeyDown:
                *val_ptr = (uint8_t)CLAMP((int)*val_ptr - 2,
                    MOOD_MIN_VALUE, MOOD_MAX_VALUE);
                consumed = true;
                break;
            case InputKeyOk:
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, InputEventNext);
                consumed = true;
                break;
            default:
                break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

// ─── Note callback (called by TextInput module) ───────────────────────────────

void mood_input_note_callback(void* ctx) {
    MoodTrackerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, InputEventSave);
}

// ─── Alloc / Free / Refresh ──────────────────────────────────────────────────

void* input_view_alloc(MoodTrackerApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, input_draw_callback);
    view_set_input_callback(view, input_input_callback);
    view_set_context(view, view);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(InputViewModel));

    InputViewModel* m = view_get_model(view);
    m->app = app;
    view_commit_model(view, false);

    app->view_input = view;
    return view;
}

void input_view_free(MoodTrackerApp* app) {
    if(app->view_input) {
        view_free(app->view_input);
        app->view_input = NULL;
    }
}

void input_view_refresh(MoodTrackerApp* app) {
    View* view = app->view_input;
    view_get_model(view);
    view_commit_model(view, true);
}
