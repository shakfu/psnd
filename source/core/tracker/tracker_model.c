/**
 * tracker_model.c - Implementation of tracker data model
 */

#include "tracker_model.h"
#include "tracker_plugin.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/*============================================================================
 * Event Params
 *============================================================================*/

TrackerEventParams* tracker_event_params_new(void) {
    TrackerEventParams* params = calloc(1, sizeof(TrackerEventParams));
    if (params) {
        params->probability = 100;  /* default: always play */
    }
    return params;
}

void tracker_event_params_free(TrackerEventParams* params) {
    free(params);
}

TrackerEventParams* tracker_event_params_clone(const TrackerEventParams* params) {
    if (!params) return NULL;
    TrackerEventParams* copy = malloc(sizeof(TrackerEventParams));
    if (copy) {
        memcpy(copy, params, sizeof(TrackerEventParams));
    }
    return copy;
}

/*============================================================================
 * Phrase
 *============================================================================*/

TrackerPhrase* tracker_phrase_new(int initial_capacity) {
    TrackerPhrase* phrase = calloc(1, sizeof(TrackerPhrase));
    if (!phrase) return NULL;

    if (initial_capacity > 0) {
        phrase->events = calloc(initial_capacity, sizeof(TrackerEvent));
        if (!phrase->events) {
            free(phrase);
            return NULL;
        }
        phrase->capacity = initial_capacity;
    }

    return phrase;
}

void tracker_phrase_free(TrackerPhrase* phrase) {
    if (!phrase) return;

    /* Free extended params for each event */
    for (int i = 0; i < phrase->count; i++) {
        tracker_event_params_free(phrase->events[i].params);
    }

    free(phrase->events);
    free(phrase);
}

void tracker_phrase_clear(TrackerPhrase* phrase) {
    if (!phrase) return;

    /* Free extended params */
    for (int i = 0; i < phrase->count; i++) {
        tracker_event_params_free(phrase->events[i].params);
        phrase->events[i].params = NULL;
    }

    phrase->count = 0;
}

bool tracker_phrase_add_event(TrackerPhrase* phrase, const TrackerEvent* event) {
    if (!phrase || !event) return false;

    /* Grow if needed */
    if (phrase->count >= phrase->capacity) {
        int new_cap = phrase->capacity ? phrase->capacity * 2 : 16;
        TrackerEvent* new_events = realloc(phrase->events,
                                           new_cap * sizeof(TrackerEvent));
        if (!new_events) return false;
        phrase->events = new_events;
        phrase->capacity = new_cap;
    }

    /* Copy event */
    TrackerEvent* dest = &phrase->events[phrase->count];
    memcpy(dest, event, sizeof(TrackerEvent));

    /* Deep copy params if present */
    if (event->params) {
        dest->params = tracker_event_params_clone(event->params);
        if (!dest->params) return false;
    }

    phrase->count++;
    return true;
}

TrackerPhrase* tracker_phrase_clone(const TrackerPhrase* phrase) {
    if (!phrase) return NULL;

    TrackerPhrase* copy = tracker_phrase_new(phrase->capacity);
    if (!copy) return NULL;

    for (int i = 0; i < phrase->count; i++) {
        if (!tracker_phrase_add_event(copy, &phrase->events[i])) {
            tracker_phrase_free(copy);
            return NULL;
        }
    }

    return copy;
}

/*============================================================================
 * FX Chain
 *============================================================================*/

void tracker_fx_chain_init(TrackerFxChain* chain) {
    if (!chain) return;
    memset(chain, 0, sizeof(TrackerFxChain));
}

static void tracker_fx_entry_clear(TrackerFxEntry* entry) {
    if (!entry) return;
    free(entry->name);
    free(entry->params);
    free(entry->language_id);
    memset(entry, 0, sizeof(TrackerFxEntry));
}

void tracker_fx_chain_clear(TrackerFxChain* chain) {
    if (!chain) return;

    for (int i = 0; i < chain->count; i++) {
        tracker_fx_entry_clear(&chain->entries[i]);
    }

    free(chain->entries);
    chain->entries = NULL;
    chain->count = 0;
    chain->capacity = 0;
}

bool tracker_fx_chain_append(TrackerFxChain* chain, const char* name,
                             const char* params, const char* lang_id) {
    if (!chain || !name) return false;

    /* Grow if needed */
    if (chain->count >= chain->capacity) {
        int new_cap = chain->capacity ? chain->capacity * 2 : 4;
        TrackerFxEntry* new_entries = realloc(chain->entries,
                                              new_cap * sizeof(TrackerFxEntry));
        if (!new_entries) return false;
        chain->entries = new_entries;
        chain->capacity = new_cap;

        /* Zero new entries */
        memset(&chain->entries[chain->count], 0,
               (new_cap - chain->count) * sizeof(TrackerFxEntry));
    }

    TrackerFxEntry* entry = &chain->entries[chain->count];
    entry->name = str_dup(name);
    entry->params = str_dup(params);
    entry->language_id = str_dup(lang_id);
    entry->enabled = true;

    if (!entry->name) {
        tracker_fx_entry_clear(entry);
        return false;
    }

    chain->count++;
    return true;
}

bool tracker_fx_chain_insert(TrackerFxChain* chain, int index, const char* name,
                             const char* params, const char* lang_id) {
    if (!chain || !name) return false;
    if (index < 0 || index > chain->count) return false;

    /* Append at end is just append */
    if (index == chain->count) {
        return tracker_fx_chain_append(chain, name, params, lang_id);
    }

    /* Grow if needed */
    if (chain->count >= chain->capacity) {
        int new_cap = chain->capacity ? chain->capacity * 2 : 4;
        TrackerFxEntry* new_entries = realloc(chain->entries,
                                              new_cap * sizeof(TrackerFxEntry));
        if (!new_entries) return false;
        chain->entries = new_entries;
        chain->capacity = new_cap;
    }

    /* Shift entries */
    memmove(&chain->entries[index + 1], &chain->entries[index],
            (chain->count - index) * sizeof(TrackerFxEntry));

    /* Insert new entry */
    TrackerFxEntry* entry = &chain->entries[index];
    memset(entry, 0, sizeof(TrackerFxEntry));
    entry->name = str_dup(name);
    entry->params = str_dup(params);
    entry->language_id = str_dup(lang_id);
    entry->enabled = true;

    if (!entry->name) {
        /* Shift back */
        memmove(&chain->entries[index], &chain->entries[index + 1],
                (chain->count - index) * sizeof(TrackerFxEntry));
        return false;
    }

    chain->count++;
    return true;
}

bool tracker_fx_chain_remove(TrackerFxChain* chain, int index) {
    if (!chain) return false;
    if (index < 0 || index >= chain->count) return false;

    tracker_fx_entry_clear(&chain->entries[index]);

    /* Shift remaining entries */
    if (index < chain->count - 1) {
        memmove(&chain->entries[index], &chain->entries[index + 1],
                (chain->count - index - 1) * sizeof(TrackerFxEntry));
    }

    chain->count--;
    return true;
}

bool tracker_fx_chain_move(TrackerFxChain* chain, int from_index, int to_index) {
    if (!chain) return false;
    if (from_index < 0 || from_index >= chain->count) return false;
    if (to_index < 0 || to_index >= chain->count) return false;
    if (from_index == to_index) return true;

    TrackerFxEntry temp = chain->entries[from_index];

    if (from_index < to_index) {
        /* Shift down */
        memmove(&chain->entries[from_index], &chain->entries[from_index + 1],
                (to_index - from_index) * sizeof(TrackerFxEntry));
    } else {
        /* Shift up */
        memmove(&chain->entries[to_index + 1], &chain->entries[to_index],
                (from_index - to_index) * sizeof(TrackerFxEntry));
    }

    chain->entries[to_index] = temp;
    return true;
}

void tracker_fx_chain_set_enabled(TrackerFxChain* chain, int index, bool enabled) {
    if (!chain) return;
    if (index < 0 || index >= chain->count) return;
    chain->entries[index].enabled = enabled;
}

TrackerFxEntry* tracker_fx_chain_get(TrackerFxChain* chain, int index) {
    if (!chain) return NULL;
    if (index < 0 || index >= chain->count) return NULL;
    return &chain->entries[index];
}

bool tracker_fx_chain_clone(TrackerFxChain* dest, const TrackerFxChain* src) {
    if (!dest) return false;
    tracker_fx_chain_init(dest);
    if (!src || src->count == 0) return true;

    for (int i = 0; i < src->count; i++) {
        const TrackerFxEntry* e = &src->entries[i];
        if (!tracker_fx_chain_append(dest, e->name, e->params, e->language_id)) {
            tracker_fx_chain_clear(dest);
            return false;
        }
        dest->entries[i].enabled = e->enabled;
    }

    return true;
}

/*============================================================================
 * Cell
 *============================================================================*/

void tracker_cell_init(TrackerCell* cell) {
    if (!cell) return;
    memset(cell, 0, sizeof(TrackerCell));
    cell->type = TRACKER_CELL_EMPTY;
    tracker_fx_chain_init(&cell->fx_chain);
}

void tracker_cell_clear(TrackerCell* cell) {
    if (!cell) return;

    free(cell->expression);
    free(cell->language_id);
    tracker_fx_chain_clear(&cell->fx_chain);
    /* The cell owns its compiled form; nothing else frees it */
    tracker_compiled_cell_free(cell->compiled);

    tracker_cell_init(cell);
}

void tracker_cell_set_expression(TrackerCell* cell, const char* expr, const char* lang_id) {
    if (!cell) return;

    free(cell->expression);
    free(cell->language_id);

    cell->expression = str_dup(expr);
    cell->language_id = str_dup(lang_id);
    cell->type = (expr && expr[0]) ? TRACKER_CELL_EXPRESSION : TRACKER_CELL_EMPTY;
    cell->dirty = true;
}

void tracker_cell_mark_dirty(TrackerCell* cell) {
    if (!cell) return;
    cell->dirty = true;
}

bool tracker_cell_clone(TrackerCell* dest, const TrackerCell* src) {
    if (!dest) return false;
    tracker_cell_init(dest);
    if (!src) return true;

    dest->type = src->type;
    dest->expression = str_dup(src->expression);
    dest->language_id = str_dup(src->language_id);
    dest->dirty = true;  /* new clone needs compilation */
    dest->compiled = NULL;

    if (!tracker_fx_chain_clone(&dest->fx_chain, &src->fx_chain)) {
        tracker_cell_clear(dest);
        return false;
    }

    return true;
}

/*============================================================================
 * Track
 *============================================================================*/

TrackerTrack* tracker_track_new(int num_rows, const char* name, uint8_t channel) {
    TrackerTrack* track = calloc(1, sizeof(TrackerTrack));
    if (!track) return NULL;

    track->name = str_dup(name);
    track->default_channel = channel;
    track->volume = 100;          /* default volume */
    track->pan = 0;               /* center pan */
    track->muted = false;
    track->solo = false;
    tracker_fx_chain_init(&track->fx_chain);

    if (num_rows > 0) {
        track->cells = calloc(num_rows, sizeof(TrackerCell));
        if (!track->cells) {
            free(track->name);
            free(track);
            return NULL;
        }

        for (int i = 0; i < num_rows; i++) {
            tracker_cell_init(&track->cells[i]);
        }
    }

    return track;
}

void tracker_track_free(TrackerTrack* track, int num_rows) {
    if (!track) return;

    free(track->name);
    tracker_fx_chain_clear(&track->fx_chain);

    if (track->cells) {
        for (int i = 0; i < num_rows; i++) {
            tracker_cell_clear(&track->cells[i]);
        }
        free(track->cells);
    }

    if (track->compiled_fx) {
        tracker_compiled_fx_chain_free(track->compiled_fx);
        free(track->compiled_fx);
    }

    free(track);
}

void tracker_track_resize(TrackerTrack* track, int old_rows, int new_rows) {
    if (!track || old_rows == new_rows) return;

    if (new_rows == 0) {
        /* Clear all cells */
        for (int i = 0; i < old_rows; i++) {
            tracker_cell_clear(&track->cells[i]);
        }
        free(track->cells);
        track->cells = NULL;
        return;
    }

    if (new_rows < old_rows) {
        /* Shrinking: clear cells that will be removed */
        for (int i = new_rows; i < old_rows; i++) {
            tracker_cell_clear(&track->cells[i]);
        }
    }

    TrackerCell* new_cells = realloc(track->cells, new_rows * sizeof(TrackerCell));
    if (!new_cells) return;  /* keep old allocation on failure */

    track->cells = new_cells;

    if (new_rows > old_rows) {
        /* Growing: initialize new cells */
        for (int i = old_rows; i < new_rows; i++) {
            tracker_cell_init(&track->cells[i]);
        }
    }
}

/*============================================================================
 * Pattern
 *============================================================================*/

TrackerPattern* tracker_pattern_new(int num_rows, int num_tracks, const char* name) {
    TrackerPattern* pattern = calloc(1, sizeof(TrackerPattern));
    if (!pattern) return NULL;

    pattern->name = str_dup(name);
    pattern->num_rows = num_rows;
    pattern->num_tracks = 0;

    if (num_tracks > 0) {
        pattern->tracks = calloc(num_tracks, sizeof(TrackerTrack));
        if (!pattern->tracks) {
            free(pattern->name);
            free(pattern);
            return NULL;
        }

        /* Create default tracks */
        for (int i = 0; i < num_tracks; i++) {
            TrackerTrack* track = tracker_track_new(num_rows, NULL, i % 16);
            if (!track) {
                tracker_pattern_free(pattern);
                return NULL;
            }
            /* Tracks are stored inline: take the contents, free the shell */
            pattern->tracks[i] = *track;
            free(track);
            pattern->num_tracks++;
        }
    }

    return pattern;
}

void tracker_pattern_free(TrackerPattern* pattern) {
    if (!pattern) return;

    free(pattern->name);

    for (int i = 0; i < pattern->num_tracks; i++) {
        /* Free track contents but not the track struct itself (it's inline) */
        free(pattern->tracks[i].name);
        tracker_fx_chain_clear(&pattern->tracks[i].fx_chain);
        if (pattern->tracks[i].compiled_fx) {
            tracker_compiled_fx_chain_free(pattern->tracks[i].compiled_fx);
            free(pattern->tracks[i].compiled_fx);
        }
        if (pattern->tracks[i].cells) {
            for (int j = 0; j < pattern->num_rows; j++) {
                tracker_cell_clear(&pattern->tracks[i].cells[j]);
            }
            free(pattern->tracks[i].cells);
        }
    }

    free(pattern->tracks);
    free(pattern);
}

TrackerCell* tracker_pattern_get_cell(TrackerPattern* pattern, int row, int track) {
    if (!pattern) return NULL;
    if (row < 0 || row >= pattern->num_rows) return NULL;
    if (track < 0 || track >= pattern->num_tracks) return NULL;

    return &pattern->tracks[track].cells[row];
}

bool tracker_pattern_add_track(TrackerPattern* pattern, const char* name, uint8_t channel) {
    if (!pattern) return false;

    /* Reallocate tracks array */
    TrackerTrack* new_tracks = realloc(pattern->tracks,
                                       (pattern->num_tracks + 1) * sizeof(TrackerTrack));
    if (!new_tracks) return false;

    pattern->tracks = new_tracks;

    /* Initialize new track */
    TrackerTrack* track = &pattern->tracks[pattern->num_tracks];
    memset(track, 0, sizeof(TrackerTrack));
    track->name = str_dup(name);
    track->default_channel = channel;
    tracker_fx_chain_init(&track->fx_chain);

    if (pattern->num_rows > 0) {
        track->cells = calloc(pattern->num_rows, sizeof(TrackerCell));
        if (!track->cells) {
            free(track->name);
            return false;
        }

        for (int i = 0; i < pattern->num_rows; i++) {
            tracker_cell_init(&track->cells[i]);
        }
    }

    pattern->num_tracks++;
    return true;
}

bool tracker_pattern_remove_track(TrackerPattern* pattern, int track_index) {
    if (!pattern) return false;
    if (track_index < 0 || track_index >= pattern->num_tracks) return false;

    /* Free track contents */
    TrackerTrack* track = &pattern->tracks[track_index];
    free(track->name);
    tracker_fx_chain_clear(&track->fx_chain);
    if (track->cells) {
        for (int i = 0; i < pattern->num_rows; i++) {
            tracker_cell_clear(&track->cells[i]);
        }
        free(track->cells);
    }

    /* Shift remaining tracks */
    if (track_index < pattern->num_tracks - 1) {
        memmove(&pattern->tracks[track_index],
                &pattern->tracks[track_index + 1],
                (pattern->num_tracks - track_index - 1) * sizeof(TrackerTrack));
    }

    pattern->num_tracks--;

    /* Optionally shrink allocation (or leave it for potential future use) */
    return true;
}

void tracker_pattern_set_rows(TrackerPattern* pattern, int new_num_rows) {
    if (!pattern || new_num_rows < 0) return;
    if (new_num_rows == pattern->num_rows) return;

    for (int i = 0; i < pattern->num_tracks; i++) {
        tracker_track_resize(&pattern->tracks[i], pattern->num_rows, new_num_rows);
    }

    pattern->num_rows = new_num_rows;
}

/*============================================================================
 * Song
 *============================================================================*/

TrackerSong* tracker_song_new(const char* name) {
    TrackerSong* song = calloc(1, sizeof(TrackerSong));
    if (!song) return NULL;

    song->name = str_dup(name);
    song->bpm = TRACKER_DEFAULT_BPM;
    song->rows_per_beat = TRACKER_DEFAULT_RPB;
    song->ticks_per_row = TRACKER_DEFAULT_TPR;
    song->spillover_mode = TRACKER_SPILLOVER_LAYER;
    tracker_fx_chain_init(&song->master_fx);
    tracker_phrase_library_init(&song->phrase_library);

    return song;
}

void tracker_song_free(TrackerSong* song) {
    if (!song) return;

    free(song->name);
    free(song->author);
    free(song->default_language_id);

    /* Free patterns */
    for (int i = 0; i < song->num_patterns; i++) {
        tracker_pattern_free(song->patterns[i]);
    }
    free(song->patterns);

    /* Free sequence */
    free(song->sequence);

    /* Free master FX */
    tracker_fx_chain_clear(&song->master_fx);

    /* Free phrase library */
    tracker_phrase_library_clear(&song->phrase_library);

    if (song->compiled_master_fx) {
        tracker_compiled_fx_chain_free(song->compiled_master_fx);
        free(song->compiled_master_fx);
    }

    free(song);
}

int tracker_song_add_pattern(TrackerSong* song, TrackerPattern* pattern) {
    if (!song || !pattern) return -1;

    /* Grow if needed */
    if (song->num_patterns >= song->patterns_capacity) {
        int new_cap = song->patterns_capacity ? song->patterns_capacity * 2 : 8;
        TrackerPattern** new_patterns = realloc(song->patterns,
                                                new_cap * sizeof(TrackerPattern*));
        if (!new_patterns) return -1;
        song->patterns = new_patterns;
        song->patterns_capacity = new_cap;
    }

    int index = song->num_patterns;
    song->patterns[index] = pattern;
    song->num_patterns++;

    return index;
}

bool tracker_song_remove_pattern(TrackerSong* song, int pattern_index) {
    if (!song) return false;
    if (pattern_index < 0 || pattern_index >= song->num_patterns) return false;

    /* Free the pattern */
    tracker_pattern_free(song->patterns[pattern_index]);

    /* Shift remaining patterns */
    if (pattern_index < song->num_patterns - 1) {
        memmove(&song->patterns[pattern_index],
                &song->patterns[pattern_index + 1],
                (song->num_patterns - pattern_index - 1) * sizeof(TrackerPattern*));
    }

    song->num_patterns--;

    /* Update sequence entries that reference patterns after this one */
    for (int i = 0; i < song->sequence_length; i++) {
        if (song->sequence[i].pattern_index == pattern_index) {
            /* Remove this sequence entry */
            if (i < song->sequence_length - 1) {
                memmove(&song->sequence[i],
                        &song->sequence[i + 1],
                        (song->sequence_length - i - 1) * sizeof(TrackerSequenceEntry));
            }
            song->sequence_length--;
            i--;  /* recheck this index */
        } else if (song->sequence[i].pattern_index > pattern_index) {
            song->sequence[i].pattern_index--;
        }
    }

    return true;
}

TrackerPattern* tracker_song_get_pattern(TrackerSong* song, int index) {
    if (!song) return NULL;
    if (index < 0 || index >= song->num_patterns) return NULL;
    return song->patterns[index];
}

bool tracker_song_append_to_sequence(TrackerSong* song, int pattern_index, int repeat_count) {
    if (!song) return false;
    if (pattern_index < 0 || pattern_index >= song->num_patterns) return false;
    if (repeat_count < 1) repeat_count = 1;

    /* Grow if needed */
    if (song->sequence_length >= song->sequence_capacity) {
        int new_cap = song->sequence_capacity ? song->sequence_capacity * 2 : 16;
        TrackerSequenceEntry* new_seq = realloc(song->sequence,
                                                new_cap * sizeof(TrackerSequenceEntry));
        if (!new_seq) return false;
        song->sequence = new_seq;
        song->sequence_capacity = new_cap;
    }

    TrackerSequenceEntry* entry = &song->sequence[song->sequence_length];
    entry->pattern_index = pattern_index;
    entry->repeat_count = repeat_count;
    song->sequence_length++;

    return true;
}

/*============================================================================
 * Phrase Library
 *============================================================================*/

void tracker_phrase_library_init(TrackerPhraseLibrary* lib) {
    if (!lib) return;
    lib->entries = NULL;
    lib->count = 0;
    lib->capacity = 0;
}

void tracker_phrase_library_clear(TrackerPhraseLibrary* lib) {
    if (!lib) return;

    for (int i = 0; i < lib->count; i++) {
        free(lib->entries[i].name);
        free(lib->entries[i].expression);
        free(lib->entries[i].language_id);
    }
    free(lib->entries);

    lib->entries = NULL;
    lib->count = 0;
    lib->capacity = 0;
}

int tracker_phrase_library_find(TrackerPhraseLibrary* lib, const char* name) {
    if (!lib || !name) return -1;

    for (int i = 0; i < lib->count; i++) {
        if (lib->entries[i].name && strcmp(lib->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

TrackerPhraseEntry* tracker_phrase_library_get(TrackerPhraseLibrary* lib, const char* name) {
    int idx = tracker_phrase_library_find(lib, name);
    if (idx < 0) return NULL;
    return &lib->entries[idx];
}

bool tracker_phrase_library_add(TrackerPhraseLibrary* lib, const char* name,
                                 const char* expression, const char* language_id) {
    if (!lib || !name || !expression) return false;

    /* Check if phrase already exists - update it */
    int existing = tracker_phrase_library_find(lib, name);
    if (existing >= 0) {
        TrackerPhraseEntry* entry = &lib->entries[existing];
        free(entry->expression);
        free(entry->language_id);
        entry->expression = str_dup(expression);
        entry->language_id = language_id ? str_dup(language_id) : NULL;
        return true;
    }

    /* Grow if needed */
    if (lib->count >= lib->capacity) {
        int new_cap = lib->capacity ? lib->capacity * 2 : 8;
        TrackerPhraseEntry* new_entries = realloc(lib->entries,
                                                   new_cap * sizeof(TrackerPhraseEntry));
        if (!new_entries) return false;
        lib->entries = new_entries;
        lib->capacity = new_cap;
    }

    /* Add new entry */
    TrackerPhraseEntry* entry = &lib->entries[lib->count];
    entry->name = str_dup(name);
    entry->expression = str_dup(expression);
    entry->language_id = language_id ? str_dup(language_id) : NULL;
    lib->count++;

    return true;
}

bool tracker_phrase_library_remove(TrackerPhraseLibrary* lib, const char* name) {
    if (!lib || !name) return false;

    int idx = tracker_phrase_library_find(lib, name);
    if (idx < 0) return false;

    /* Free entry data */
    free(lib->entries[idx].name);
    free(lib->entries[idx].expression);
    free(lib->entries[idx].language_id);

    /* Shift remaining entries */
    if (idx < lib->count - 1) {
        memmove(&lib->entries[idx], &lib->entries[idx + 1],
                (lib->count - idx - 1) * sizeof(TrackerPhraseEntry));
    }
    lib->count--;

    return true;
}
