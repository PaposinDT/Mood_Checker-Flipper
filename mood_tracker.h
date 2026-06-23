#include "settings.h"
#include <gui/modules/variable_item_list.h>

// ─── Item value-change callbacks ─────────────────────────────────────────────

static void settings_theme_changed(VariableItem* item) {
    MoodTrackerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->config.theme = (MoodTheme)idx;
    variable_item_set_current_value_text(item, idx == 0 ? "Light" : "Dark");
}

static void settings_graph_days_changed(VariableItem* item) {
    MoodTrackerApp* app = variable_item_get_context(item);
    static const uint8_t values[] = {7, 14, 30, 60, 90};
    uint8_t idx = variable_item_get_current_value_index(item);
    app->config.graph_days = values[idx];
    app->graph_days_shown  = values[idx];
    static const char* labels[] = {"7d", "14d", "30d", "60d", "90d"};
    variable_item_set_current_value_text(item, labels[idx]);
}

static void settings_quote_changed(VariableItem* item) {
    MoodTrackerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->config.show_quote_on_start = (idx == 1);
    variable_item_set_current_value_text(item, idx == 1 ? "On" : "Off");
}

// ─── Enter callback for action rows (Export / Import / Backup) ───────────────
// FIX: was a C++ lambda — replaced with a proper static C function.

static void settings_enter_callback(void* ctx, uint32_t index) {
    MoodTrackerApp* app = ctx;
    /* Item layout:
       0 = Theme
       1 = Graph days
       2 = Daily quote
       3 = Export CSV
       4 = Import CSV
       5 = Backup CSV   */
    switch(index) {
        case 3:
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SettingsEventExportCSV);
            break;
        case 4:
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SettingsEventImportCSV);
            break;
        case 5:
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SettingsEventBackup);
            break;
        default:
            break;
    }
}

// ─── Build / rebuild the settings list ───────────────────────────────────────

void settings_view_build(MoodTrackerApp* app) {
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);

    VariableItem* item;

    // 0 – Theme
    item = variable_item_list_add(vil, "Theme", 2, settings_theme_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)app->config.theme);
    variable_item_set_current_value_text(
        item, app->config.theme == MoodThemeDark ? "Dark" : "Light");

    // 1 – Graph days
    item = variable_item_list_add(
        vil, "Graph days", 5, settings_graph_days_changed, app);
    {
        static const uint8_t v[] = {7, 14, 30, 60, 90};
        static const char*   l[] = {"7d", "14d", "30d", "60d", "90d"};
        uint8_t sel = 2;
        for(uint8_t i = 0; i < 5; i++) {
            if(v[i] == app->config.graph_days) {
                sel = i;
                break;
            }
        }
        variable_item_set_current_value_index(item, sel);
        variable_item_set_current_value_text(item, l[sel]);
    }

    // 2 – Daily quote
    item = variable_item_list_add(
        vil, "Daily quote", 2, settings_quote_changed, app);
    variable_item_set_current_value_index(
        item, app->config.show_quote_on_start ? 1 : 0);
    variable_item_set_current_value_text(
        item, app->config.show_quote_on_start ? "On" : "Off");

    // 3 – Export CSV  (action row – no values)
    variable_item_list_add(vil, "Export CSV", 0, NULL, NULL);

    // 4 – Import CSV
    variable_item_list_add(vil, "Import CSV", 0, NULL, NULL);

    // 5 – Backup CSV
    variable_item_list_add(vil, "Backup CSV", 0, NULL, NULL);

    // Single enter-callback handles all rows
    variable_item_list_set_enter_callback(vil, settings_enter_callback, app);
}
