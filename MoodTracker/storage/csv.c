#include "csv.h"
#include <storage/storage.h>

// ─── Export ───────────────────────────────────────────────────────────────────

bool mood_csv_export(MoodTrackerApp* app, const char* path) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return false;
    }

    // Header
    storage_file_write(f, MOOD_CSV_HEADER, strlen(MOOD_CSV_HEADER));

    // Rows
    char row[192];
    for(uint32_t i = 0; i < app->entry_count; i++) {
        MoodEntry* e = &app->entries[i];

        // Escape note: replace commas with semicolons, strip newlines
        char safe_note[MOOD_NOTE_MAX_LEN];
        strncpy(safe_note, e->note, MOOD_NOTE_MAX_LEN - 1);
        safe_note[MOOD_NOTE_MAX_LEN - 1] = '\0';
        for(char* p = safe_note; *p; p++) {
            if(*p == ',' || *p == '\n' || *p == '\r') *p = ';';
        }

        int len = snprintf(row, sizeof(row),
            "%04u-%02u-%02u,%02u:%02u,%u,%u,%u,%s,%d\n",
            e->year, e->month, e->day,
            e->hour, e->minute,
            e->mood, e->stress, e->energy,
            safe_note,
            (int)e->is_primary);

        if(len > 0) {
            storage_file_write(f, row, (uint32_t)len);
        }
    }

    storage_file_close(f);
    storage_file_free(f);
    return true;
}

// ─── Import ───────────────────────────────────────────────────────────────────

bool mood_csv_import(MoodTrackerApp* app, const char* path) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false;
    }

    char line[192];
    uint32_t pos = 0;
    char ch;
    bool first_line = true;

    while(storage_file_read(f, &ch, 1) == 1) {
        if(ch == '\n' || ch == '\r') {
            if(pos == 0) continue; // skip empty lines

            line[pos] = '\0';
            pos = 0;

            // Skip header
            if(first_line) {
                first_line = false;
                if(strncmp(line, "date", 4) == 0) continue;
            }

            // Parse: date,time,mood,stress,energy,note,primary
            // date  = YYYY-MM-DD
            // time  = HH:MM
            if(app->entry_count >= MOOD_MAX_ENTRIES) break;

            MoodEntry e;
            memset(&e, 0, sizeof(MoodEntry));

            char date_s[12], time_s[8], note_s[MOOD_NOTE_MAX_LEN];
            int mood_i, stress_i, energy_i, primary_i;

            // Use a custom CSV split to handle note with semicolons
            char* tok;
            char tmp[192];
            strncpy(tmp, line, sizeof(tmp) - 1);

            int col = 0;
            char* p = tmp;
            char* start = p;
            while(col < 7) {
                // Find next comma or end
                char* comma = strchr(p, ',');
                if(comma) *comma = '\0';

                switch(col) {
                    case 0: strncpy(date_s, p, 11); date_s[11] = '\0'; break;
                    case 1: strncpy(time_s, p, 7);  time_s[7]  = '\0'; break;
                    case 2: mood_i    = atoi(p); break;
                    case 3: stress_i  = atoi(p); break;
                    case 4: energy_i  = atoi(p); break;
                    case 5: strncpy(note_s, p, MOOD_NOTE_MAX_LEN - 1);
                            note_s[MOOD_NOTE_MAX_LEN - 1] = '\0'; break;
                    case 6: primary_i = atoi(p); break;
                    default: break;
                }

                if(comma) p = comma + 1;
                else break;
                col++;
                (void)start; // suppress warning
            }

            if(col < 6) continue; // malformed line

            // Parse date
            int yr = 0, mo = 0, dy = 0;
            sscanf(date_s, "%d-%d-%d", &yr, &mo, &dy);
            e.year  = (uint16_t)yr;
            e.month = (uint8_t)mo;
            e.day   = (uint8_t)dy;

            // Parse time
            int hr = 0, mn = 0;
            sscanf(time_s, "%d:%d", &hr, &mn);
            e.hour   = (uint8_t)hr;
            e.minute = (uint8_t)mn;

            e.mood       = (uint8_t)CLAMP(mood_i,   MOOD_MIN_VALUE, MOOD_MAX_VALUE);
            e.stress     = (uint8_t)CLAMP(stress_i, MOOD_MIN_VALUE, MOOD_MAX_VALUE);
            e.energy     = (uint8_t)CLAMP(energy_i, MOOD_MIN_VALUE, MOOD_MAX_VALUE);
            e.is_primary = (primary_i != 0);
            strncpy(e.note, note_s, MOOD_NOTE_MAX_LEN - 1);

            // Skip duplicates (same date+time already loaded)
            bool dup = false;
            for(uint32_t i = 0; i < app->entry_count; i++) {
                if(app->entries[i].year   == e.year   &&
                   app->entries[i].month  == e.month  &&
                   app->entries[i].day    == e.day    &&
                   app->entries[i].hour   == e.hour   &&
                   app->entries[i].minute == e.minute) {
                    dup = true;
                    break;
                }
            }
            if(!dup) {
                app->entries[app->entry_count++] = e;
            }

        } else {
            if(pos < sizeof(line) - 1) {
                line[pos++] = ch;
            }
        }
    }

    storage_file_close(f);
    storage_file_free(f);
    return true;
}
