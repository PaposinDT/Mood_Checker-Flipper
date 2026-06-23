#include "database.h"
#include "csv.h"
#include <storage/storage.h>

// ─── Config Load / Save ───────────────────────────────────────────────────────

bool mood_config_load(MoodTrackerApp* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, MOOD_TRACKER_CFG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false; // use defaults
    }

    char line[128];
    uint32_t read = 0;
    while((read = storage_file_read(f, line, sizeof(line) - 1)) > 0) {
        line[read] = '\0';
        char key[32], val[96];
        if(sscanf(line, "%31[^=]=%95[^\n]", key, val) == 2) {
            if(strcmp(key, "theme") == 0)
                app->config.theme = (MoodTheme)atoi(val);
            else if(strcmp(key, "pin_enabled") == 0)
                app->config.pin_enabled = atoi(val) != 0;
            else if(strcmp(key, "pin") == 0)
                strncpy(app->config.pin, val, MOOD_PIN_MAX_LEN);
            else if(strcmp(key, "show_quote") == 0)
                app->config.show_quote_on_start = atoi(val) != 0;
            else if(strcmp(key, "reminder_enabled") == 0)
                app->config.reminder_enabled = atoi(val) != 0;
            else if(strcmp(key, "reminder_hour") == 0)
                app->config.reminder_hour = (uint8_t)atoi(val);
            else if(strcmp(key, "reminder_minute") == 0)
                app->config.reminder_minute = (uint8_t)atoi(val);
            else if(strcmp(key, "graph_mode") == 0)
                app->config.default_graph_mode = (GraphMode)atoi(val);
            else if(strcmp(key, "graph_days") == 0)
                app->config.graph_days = (uint8_t)atoi(val);
        }
    }

    storage_file_close(f);
    storage_file_free(f);
    return true;
}

bool mood_config_save(MoodTrackerApp* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, MOOD_TRACKER_CFG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return false;
    }

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "theme=%d\n"
        "pin_enabled=%d\n"
        "pin=%s\n"
        "show_quote=%d\n"
        "reminder_enabled=%d\n"
        "reminder_hour=%d\n"
        "reminder_minute=%d\n"
        "graph_mode=%d\n"
        "graph_days=%d\n",
        (int)app->config.theme,
        (int)app->config.pin_enabled,
        app->config.pin,
        (int)app->config.show_quote_on_start,
        (int)app->config.reminder_enabled,
        (int)app->config.reminder_hour,
        (int)app->config.reminder_minute,
        (int)app->config.default_graph_mode,
        (int)app->config.graph_days);

    storage_file_write(f, buf, (uint32_t)len);
    storage_file_close(f);
    storage_file_free(f);
    return true;
}

// ─── Database Load / Save ─────────────────────────────────────────────────────

bool mood_db_load(MoodTrackerApp* app) {
    app->entry_count = 0;
    return mood_csv_import(app, MOOD_TRACKER_CSV_PATH);
}

bool mood_db_save(MoodTrackerApp* app) {
    bool ok = mood_csv_export(app, MOOD_TRACKER_CSV_PATH);
    if(!ok) return false;

    // Rebuild binary index
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, MOOD_TRACKER_IDX_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return false;
    }

    // Write index header: entry count (4 bytes)
    storage_file_write(f, &app->entry_count, sizeof(uint32_t));

    // Build index records grouped by day
    uint32_t i = 0;
    while(i < app->entry_count) {
        uint32_t day_key = PACK_DATE(
            app->entries[i].year,
            app->entries[i].month,
            app->entries[i].day);

        MoodIndexRecord rec;
        rec.date_key    = day_key;
        rec.csv_offset  = i; // use entry index (not byte offset, simpler)
        rec.entry_count = 0;

        uint32_t j = i;
        while(j < app->entry_count) {
            uint32_t k = PACK_DATE(
                app->entries[j].year,
                app->entries[j].month,
                app->entries[j].day);
            if(k != day_key) break;
            rec.entry_count++;
            j++;
        }

        storage_file_write(f, &rec, sizeof(MoodIndexRecord));
        i = j;
    }

    storage_file_close(f);
    storage_file_free(f);
    app->data_dirty = false;
    return true;
}

// ─── Query Helpers ────────────────────────────────────────────────────────────

uint8_t mood_db_get_day(MoodTrackerApp* app, uint32_t date_key,
                         MoodEntry* out, uint8_t max_out) {
    uint8_t count = 0;
    for(uint32_t i = 0; i < app->entry_count && count < max_out; i++) {
        uint32_t k = PACK_DATE(
            app->entries[i].year,
            app->entries[i].month,
            app->entries[i].day);
        if(k == date_key) {
            if(out) out[count] = app->entries[i];
            count++;
        }
    }
    return count;
}

bool mood_db_get_day_stats(MoodTrackerApp* app, uint32_t date_key,
                            MoodDayStats* out) {
    MoodEntry tmp[MOOD_MAX_ENTRIES_PER_DAY];
    uint8_t cnt = mood_db_get_day(app, date_key, tmp, MOOD_MAX_ENTRIES_PER_DAY);
    if(cnt == 0) return false;

    out->date_key    = date_key;
    out->entry_count = cnt;

    float sm = 0, ss = 0, se = 0;
    for(uint8_t i = 0; i < cnt; i++) {
        sm += tmp[i].mood;
        ss += tmp[i].stress;
        se += tmp[i].energy;
    }
    out->avg_mood   = sm / cnt;
    out->avg_stress = ss / cnt;
    out->avg_energy = se / cnt;
    return true;
}

uint32_t mood_db_get_range(MoodTrackerApp* app,
                            uint32_t from_key, uint32_t to_key,
                            MoodDayStats* out, uint32_t max_out) {
    uint32_t count = 0;

    // Collect unique day keys in range
    uint32_t prev_key = 0;
    for(uint32_t i = 0; i < app->entry_count && count < max_out; i++) {
        uint32_t k = PACK_DATE(
            app->entries[i].year,
            app->entries[i].month,
            app->entries[i].day);
        if(k < from_key || k > to_key) continue;
        if(k == prev_key) continue; // already processed

        if(mood_db_get_day_stats(app, k, &out[count])) {
            count++;
            prev_key = k;
        }
    }
    return count;
}
