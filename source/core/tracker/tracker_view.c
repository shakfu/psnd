/**
 * tracker_view.c - Core view lifecycle, rendering, input handling
 */

#include "tracker_view.h"
#include "tracker_plugin.h"
#include "tracker_midi_import.h"
#include "../shared/midi/events.h"
#include "../include/loki/midi_export.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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

/**
 * Derive a ".mid" output path from a source path.
 *
 * Replaces the source's extension with ".mid", or appends ".mid" when it has
 * none. The whole result must fit in `out_sz` including the terminator; a
 * source path that is too long is rejected rather than truncated, since a
 * truncated path would silently name the wrong file.
 *
 * @param src    Source path (must not be NULL)
 * @param out    Destination buffer
 * @param out_sz Size of `out` in bytes
 * @return       true if the path was written, false if it would not fit
 */
static bool derive_midi_path(const char* src, char* out, size_t out_sz) {
    static const char kExt[] = ".mid";
    const size_t kExtLen = sizeof(kExt) - 1;

    if (!src || !out || out_sz == 0) return false;

    /* Length of src up to (excluding) its extension, if any. Only a dot in the
     * final path component counts, so "a.b/song" is not treated as having
     * extension "b/song". */
    size_t stem_len = strlen(src);
    const char* dot = strrchr(src, '.');
    if (dot && !strpbrk(dot, "/\\")) {
        stem_len = (size_t)(dot - src);
    }

    if (stem_len + kExtLen + 1 > out_sz) return false;

    memcpy(out, src, stem_len);
    memcpy(out + stem_len, kExt, kExtLen + 1); /* includes terminator */
    return true;
}

/*============================================================================
 * MIDI Export
 *============================================================================*/

/**
 * Export tracker song to MIDI file.
 * Iterates through all patterns and cells, evaluates expressions,
 * and writes events to the shared MIDI buffer for export.
 *
 * @param view     The tracker view
 * @param filename Output filename
 * @return         true on success, false on failure
 */
static bool tracker_export_midi(TrackerView* view, const char* filename) {
    if (!view || !view->song || !filename) return false;

    TrackerSong* song = view->song;

    /* Calculate ticks per quarter note
     * Standard MIDI uses 480 ticks/quarter
     * We have: ticks_per_row * rows_per_beat = ticks per beat (quarter) */
    int ticks_per_quarter = song->ticks_per_row * song->rows_per_beat;

    /* Initialize the shared event buffer */
    if (shared_midi_events_init(ticks_per_quarter) != 0) {
        return false;
    }
    shared_midi_events_clear();

    /* Add tempo event at start */
    shared_midi_events_tempo(0, song->bpm);

    /* Track absolute row offset for pattern chaining */
    int total_rows = 0;

    /* Iterate through all patterns */
    for (int p = 0; p < song->num_patterns; p++) {
        TrackerPattern* pattern = song->patterns[p];
        if (!pattern) continue;

        /* Check mute/solo state */
        bool has_solo = false;
        for (int t = 0; t < pattern->num_tracks; t++) {
            if (pattern->tracks[t].solo) {
                has_solo = true;
                break;
            }
        }

        /* Iterate through tracks */
        for (int t = 0; t < pattern->num_tracks; t++) {
            TrackerTrack* track = &pattern->tracks[t];

            /* Skip muted tracks, or non-solo tracks when solo is active */
            if (track->muted) continue;
            if (has_solo && !track->solo) continue;

            uint8_t channel = track->default_channel;

            /* Iterate through rows */
            for (int r = 0; r < pattern->num_rows; r++) {
                TrackerCell* cell = &track->cells[r];

                if (cell->type != TRACKER_CELL_EXPRESSION || !cell->expression) {
                    continue;
                }

                /* Compile cell if needed */
                if (!cell->compiled || cell->dirty) {
                    const char* error = NULL;
                    cell->compiled = tracker_compile_cell(cell,
                        song->default_language_id, &error);
                    cell->dirty = false;
                    if (!cell->compiled) continue;
                }

                /* Set up context for evaluation */
                TrackerContext ctx;
                tracker_context_init(&ctx);
                ctx.current_pattern = p;
                ctx.current_track = t;
                ctx.current_row = r;
                ctx.total_tracks = pattern->num_tracks;
                ctx.total_rows = pattern->num_rows;
                ctx.bpm = song->bpm;
                ctx.rows_per_beat = song->rows_per_beat;
                ctx.ticks_per_row = song->ticks_per_row;
                ctx.channel = channel;
                ctx.track_name = track->name;
                ctx.song_name = song->name;
                ctx.random_seed = (total_rows + r) * 1000 + t;

                /* Evaluate cell to get phrase */
                TrackerPhrase* phrase = tracker_evaluate_cell(cell->compiled, &ctx);
                if (!phrase) continue;

                /* Calculate base tick for this row */
                int64_t base_tick = (int64_t)(total_rows + r) * song->ticks_per_row;

                /* Convert phrase events to MIDI events */
                for (int e = 0; e < phrase->count; e++) {
                    TrackerEvent* ev = &phrase->events[e];

                    /* Calculate absolute tick for this event */
                    int64_t event_tick = base_tick +
                        (int64_t)ev->offset_rows * song->ticks_per_row +
                        ev->offset_ticks;

                    /* Use event channel if specified, otherwise track default */
                    uint8_t ev_channel = ev->channel ? ev->channel : channel;

                    switch (ev->type) {
                        case TRACKER_EVENT_NOTE_ON:
                            shared_midi_events_note_on((int)event_tick, ev_channel,
                                ev->data1, ev->data2);

                            /* Add note-off if gate is specified */
                            if (ev->gate_rows > 0 || ev->gate_ticks > 0) {
                                int64_t off_tick = event_tick +
                                    (int64_t)ev->gate_rows * song->ticks_per_row +
                                    ev->gate_ticks;
                                shared_midi_events_note_off((int)off_tick, ev_channel,
                                    ev->data1);
                            }
                            break;

                        case TRACKER_EVENT_NOTE_OFF:
                            shared_midi_events_note_off((int)event_tick, ev_channel,
                                ev->data1);
                            break;

                        case TRACKER_EVENT_CC:
                            shared_midi_events_cc((int)event_tick, ev_channel,
                                ev->data1, ev->data2);
                            break;

                        case TRACKER_EVENT_PROGRAM_CHANGE:
                            shared_midi_events_program((int)event_tick, ev_channel,
                                ev->data1);
                            break;

                        default:
                            /* Skip unsupported event types */
                            break;
                    }
                }

                tracker_phrase_free(phrase);
            }
        }

        total_rows += pattern->num_rows;
    }

    /* Sort events by tick */
    shared_midi_events_sort();

    /* Export to file */
    int result = loki_midi_export_shared(filename);

    /* Cleanup */
    shared_midi_events_cleanup();

    return result == 0;
}

/*============================================================================
 * View State
 *============================================================================*/

void tracker_view_state_init(TrackerViewState* state) {
    if (!state) return;
    memset(state, 0, sizeof(TrackerViewState));

    state->view_mode = TRACKER_VIEW_MODE_PATTERN;
    state->edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    state->selection.type = TRACKER_SEL_NONE;
    state->follow_playback = true;
    state->show_row_numbers = true;
    state->show_track_headers = true;
    state->show_transport = true;
    state->show_status_line = true;
    state->highlight_current_row = true;
    state->highlight_beat_rows = true;
    state->beat_highlight_interval = 4;
    state->visible_tracks = 8;
    state->visible_rows = 32;
    state->step_size = 1;       /* advance 1 row after note entry */
    state->default_octave = 4;  /* middle C octave */
}

void tracker_view_state_cleanup(TrackerViewState* state) {
    if (!state) return;

    free(state->edit_buffer);
    free(state->command_buffer);
    free(state->error_message);
    free(state->status_message);
    free(state->track_widths);

    if (state->owns_theme && state->theme) {
        tracker_theme_free(state->theme);
    }

    memset(state, 0, sizeof(TrackerViewState));
}

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

TrackerView* tracker_view_new(const TrackerViewCallbacks* callbacks) {
    TrackerView* view = calloc(1, sizeof(TrackerView));
    if (!view) return NULL;

    if (callbacks) {
        view->callbacks = *callbacks;
    }

    tracker_view_state_init(&view->state);
    tracker_undo_init(&view->undo, 100);  /* 100 undo levels */

    /* Set default theme */
    view->state.theme = (TrackerTheme*)tracker_theme_get("default");
    view->state.owns_theme = false;

    /* Initialize backend if callback provided */
    if (view->callbacks.init) {
        if (!view->callbacks.init(view)) {
            tracker_view_free(view);
            return NULL;
        }
    }

    return view;
}

void tracker_view_free(TrackerView* view) {
    if (!view) return;

    /* Cleanup backend */
    if (view->callbacks.cleanup) {
        view->callbacks.cleanup(view);
    }

    tracker_view_state_cleanup(&view->state);
    tracker_view_clipboard_clear(view);
    tracker_undo_cleanup(&view->undo);

    free(view);
}

void tracker_view_attach(TrackerView* view, TrackerSong* song, TrackerEngine* engine) {
    if (!view) return;

    view->song = song;
    view->engine = engine;

    /* Reset cursor to valid position */
    view->state.cursor_pattern = 0;
    view->state.cursor_track = 0;
    view->state.cursor_row = 0;
    view->state.scroll_track = 0;
    view->state.scroll_row = 0;

    tracker_view_select_clear(view);
    tracker_view_invalidate(view);
}

void tracker_view_detach(TrackerView* view) {
    if (!view) return;

    view->song = NULL;
    view->engine = NULL;

    tracker_view_invalidate(view);
}

/*============================================================================
 * Theme Management
 *============================================================================*/

void tracker_view_set_theme(TrackerView* view, TrackerTheme* theme, bool owns) {
    if (!view) return;

    /* Free old theme if owned */
    if (view->state.owns_theme && view->state.theme) {
        tracker_theme_free(view->state.theme);
    }

    view->state.theme = theme;
    view->state.owns_theme = owns;

    tracker_view_invalidate(view);
}

bool tracker_view_set_theme_by_name(TrackerView* view, const char* name) {
    if (!view) return false;

    const TrackerTheme* theme = tracker_theme_get(name);
    if (!theme) return false;

    /* Free old theme if owned */
    if (view->state.owns_theme && view->state.theme) {
        tracker_theme_free(view->state.theme);
    }

    view->state.theme = (TrackerTheme*)theme;
    view->state.owns_theme = false;

    tracker_view_invalidate(view);
    return true;
}

const TrackerTheme* tracker_view_get_theme(TrackerView* view) {
    if (!view) return NULL;
    return view->state.theme;
}

/*============================================================================
 * Rendering
 *============================================================================*/

void tracker_view_invalidate(TrackerView* view) {
    if (!view) return;
    view->dirty_flags = TRACKER_DIRTY_ALL;
}

void tracker_view_invalidate_cell(TrackerView* view, int track, int row) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_CELL;
    view->dirty_cell_track = track;
    view->dirty_cell_row = row;
}

void tracker_view_invalidate_row(TrackerView* view, int row) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_ROW;
    view->dirty_row = row;
}

void tracker_view_invalidate_track(TrackerView* view, int track) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_TRACK;
    view->dirty_track = track;
}

void tracker_view_invalidate_cursor(TrackerView* view) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_CURSOR;
}

void tracker_view_invalidate_selection(TrackerView* view) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_SELECTION;
}

void tracker_view_invalidate_status(TrackerView* view) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_STATUS;
}

void tracker_view_render(TrackerView* view) {
    if (!view) return;

    if (view->dirty_flags == TRACKER_DIRTY_NONE) return;

    if (view->callbacks.render_incremental &&
        view->dirty_flags != TRACKER_DIRTY_ALL) {
        view->callbacks.render_incremental(view, view->dirty_flags);
    } else if (view->callbacks.render) {
        view->callbacks.render(view);
    }

    view->dirty_flags = TRACKER_DIRTY_NONE;
}

void tracker_view_update_playback(TrackerView* view, int pattern, int row) {
    if (!view) return;

    bool changed = (view->state.playback_pattern != pattern ||
                    view->state.playback_row != row);

    view->state.playback_pattern = pattern;
    view->state.playback_row = row;

    if (changed) {
        view->dirty_flags |= TRACKER_DIRTY_PLAYBACK;

        /* Auto-scroll if following playback */
        if (view->state.follow_playback && view->engine &&
            tracker_engine_is_playing(view->engine)) {
            if (pattern == view->state.cursor_pattern) {
                tracker_view_scroll_to_row(view, row);
            }
        }
    }
}

/*============================================================================
 * Input Handling
 *============================================================================*/

bool tracker_view_poll_input(TrackerView* view, int timeout_ms) {
    if (!view || !view->callbacks.poll_input) return false;

    TrackerInputEvent event;
    if (view->callbacks.poll_input(view, timeout_ms, &event)) {
        return tracker_view_handle_input(view, &event);
    }

    return false;
}

bool tracker_view_handle_input(TrackerView* view, const TrackerInputEvent* event) {
    if (!view || !event) return false;

    bool handled = true;
    bool shift = (event->modifiers & TRACKER_MOD_SHIFT) != 0;

    /* Handle help mode - any key returns to pattern view */
    if (view->state.view_mode == TRACKER_VIEW_MODE_HELP) {
        tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
        tracker_view_invalidate(view);
        return true;
    }

    /* Handle arrange mode input */
    if (view->state.view_mode == TRACKER_VIEW_MODE_ARRANGE) {
        switch (event->type) {
            case TRACKER_INPUT_CURSOR_UP:
            case TRACKER_INPUT_CURSOR_LEFT:
                if (view->song && view->state.sequence_cursor > 0) {
                    view->state.sequence_cursor--;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_DOWN:
            case TRACKER_INPUT_CURSOR_RIGHT:
                if (view->song &&
                    view->state.sequence_cursor < view->song->sequence_length - 1) {
                    view->state.sequence_cursor++;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_HOME:
            case TRACKER_INPUT_PATTERN_START:
                view->state.sequence_cursor = 0;
                tracker_view_invalidate(view);
                return true;
            case TRACKER_INPUT_END:
            case TRACKER_INPUT_PATTERN_END:
                if (view->song && view->song->sequence_length > 0) {
                    view->state.sequence_cursor = view->song->sequence_length - 1;
                }
                tracker_view_invalidate(view);
                return true;
            case TRACKER_INPUT_ENTER_EDIT:
            case TRACKER_INPUT_SEQ_GOTO:
                /* Jump to pattern and switch to pattern view */
                if (view->song && view->song->sequence_length > 0) {
                    int idx = view->state.sequence_cursor;
                    if (idx >= 0 && idx < view->song->sequence_length) {
                        int pattern_idx = view->song->sequence[idx].pattern_index;
                        if (pattern_idx >= 0 && pattern_idx < view->song->num_patterns) {
                            view->state.cursor_pattern = pattern_idx;
                            tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
                            tracker_view_show_status(view, "Pattern %d", pattern_idx + 1);
                            tracker_view_invalidate(view);
                        }
                    }
                }
                return true;
            case TRACKER_INPUT_CANCEL:
            case TRACKER_INPUT_MODE_PATTERN:
                tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
                tracker_view_invalidate(view);
                return true;

            /* In arrange mode, 'a' (ADD_TRACK) adds to sequence */
            case TRACKER_INPUT_ADD_TRACK:
                if (view->song) {
                    /* Add the pattern currently selected in the sequence, or current pattern */
                    int pattern_idx;
                    if (view->song->sequence_length > 0 &&
                        view->state.sequence_cursor < view->song->sequence_length) {
                        pattern_idx = view->song->sequence[view->state.sequence_cursor].pattern_index;
                    } else {
                        pattern_idx = view->state.cursor_pattern;
                    }
                    if (tracker_song_append_to_sequence(view->song, pattern_idx, 1)) {
                        view->state.sequence_cursor = view->song->sequence_length - 1;
                        view->modified = true;
                        tracker_view_show_status(view, "Added pattern %d", pattern_idx + 1);
                        tracker_view_invalidate(view);
                    }
                }
                return true;

            /* In arrange mode, 'x' (CLEAR_CELL) removes from sequence */
            case TRACKER_INPUT_CLEAR_CELL:
                if (view->song && view->song->sequence_length > 0) {
                    int idx = view->state.sequence_cursor;
                    if (idx >= 0 && idx < view->song->sequence_length) {
                        /* Shift entries down */
                        for (int i = idx; i < view->song->sequence_length - 1; i++) {
                            view->song->sequence[i] = view->song->sequence[i + 1];
                        }
                        view->song->sequence_length--;
                        /* Adjust cursor */
                        if (view->state.sequence_cursor >= view->song->sequence_length &&
                            view->song->sequence_length > 0) {
                            view->state.sequence_cursor = view->song->sequence_length - 1;
                        }
                        view->modified = true;
                        tracker_view_show_status(view, "Removed entry %d", idx + 1);
                        tracker_view_invalidate(view);
                    }
                }
                return true;

            /* In arrange mode, +/- adjusts repeat count */
            case TRACKER_INPUT_STEP_INC:
                if (view->song && view->song->sequence_length > 0) {
                    int idx = view->state.sequence_cursor;
                    if (idx >= 0 && idx < view->song->sequence_length) {
                        int count = view->song->sequence[idx].repeat_count;
                        if (count < 99) {
                            view->song->sequence[idx].repeat_count = count + 1;
                            view->modified = true;
                            tracker_view_show_status(view, "Repeat: x%d", count + 1);
                            tracker_view_invalidate(view);
                        }
                    }
                }
                return true;
            case TRACKER_INPUT_STEP_DEC:
                if (view->song && view->song->sequence_length > 0) {
                    int idx = view->state.sequence_cursor;
                    if (idx >= 0 && idx < view->song->sequence_length) {
                        int count = view->song->sequence[idx].repeat_count;
                        if (count > 1) {
                            view->song->sequence[idx].repeat_count = count - 1;
                            view->modified = true;
                            tracker_view_show_status(view, "Repeat: x%d", count - 1);
                            tracker_view_invalidate(view);
                        }
                    }
                }
                return true;

            case TRACKER_INPUT_SEQ_ADD:
            case TRACKER_INPUT_SEQ_REMOVE:
            case TRACKER_INPUT_SEQ_MOVE_UP:
            case TRACKER_INPUT_SEQ_MOVE_DOWN:
            case TRACKER_INPUT_QUIT:
            case TRACKER_INPUT_MODE_ARRANGE:
            case TRACKER_INPUT_MODE_MIXER:
            case TRACKER_INPUT_MODE_HELP:
            case TRACKER_INPUT_SAVE:
            case TRACKER_INPUT_PLAY_TOGGLE:
                /* Fall through to normal handler */
                break;
            default:
                /* Ignore other inputs in arrange mode */
                return true;
        }
    }

    /* Handle FX edit mode input */
    if (view->state.view_mode == TRACKER_VIEW_MODE_FX) {
        /* Available FX types for cycling */
        static const char* fx_types[] = {
            "transpose", "velocity", "arpeggio", "delay", "ratchet",
            "octave", "humanize", "chance", "reverse", "stutter"
        };
        static const int num_fx_types = 10;

        TrackerFxChain* chain = NULL;
        if (view->song) {
            switch (view->state.fx_target) {
                case TRACKER_FX_TARGET_CELL: {
                    TrackerCell* cell = tracker_view_get_cursor_cell(view);
                    if (cell) chain = &cell->fx_chain;
                    break;
                }
                case TRACKER_FX_TARGET_TRACK: {
                    TrackerPattern* p = tracker_view_get_current_pattern(view);
                    if (p && view->state.cursor_track < p->num_tracks) {
                        chain = &p->tracks[view->state.cursor_track].fx_chain;
                    }
                    break;
                }
                case TRACKER_FX_TARGET_MASTER:
                    chain = &view->song->master_fx;
                    break;
            }
        }

        /* Handle FX parameter editing mode */
        if (view->state.fx_editing) {
            TrackerFxEntry* entry = chain ? tracker_fx_chain_get(chain, view->state.fx_cursor) : NULL;

            switch (event->type) {
                case TRACKER_INPUT_CHAR: {
                    /* Add character to edit buffer */
                    int len = (int)strlen(view->state.fx_edit_buffer);
                    if (len < 62 && event->character >= 32 && event->character < 127) {
                        /* Insert at cursor */
                        memmove(&view->state.fx_edit_buffer[view->state.fx_edit_cursor + 1],
                                &view->state.fx_edit_buffer[view->state.fx_edit_cursor],
                                len - view->state.fx_edit_cursor + 1);
                        view->state.fx_edit_buffer[view->state.fx_edit_cursor] = (char)event->character;
                        view->state.fx_edit_cursor++;
                        tracker_view_invalidate(view);
                    }
                    return true;
                }

                case TRACKER_INPUT_BACKSPACE:
                    if (view->state.fx_edit_cursor > 0) {
                        int len = (int)strlen(view->state.fx_edit_buffer);
                        memmove(&view->state.fx_edit_buffer[view->state.fx_edit_cursor - 1],
                                &view->state.fx_edit_buffer[view->state.fx_edit_cursor],
                                len - view->state.fx_edit_cursor + 1);
                        view->state.fx_edit_cursor--;
                        tracker_view_invalidate(view);
                    }
                    return true;

                case TRACKER_INPUT_DELETE:
                    {
                        int len = (int)strlen(view->state.fx_edit_buffer);
                        if (view->state.fx_edit_cursor < len) {
                            memmove(&view->state.fx_edit_buffer[view->state.fx_edit_cursor],
                                    &view->state.fx_edit_buffer[view->state.fx_edit_cursor + 1],
                                    len - view->state.fx_edit_cursor);
                            tracker_view_invalidate(view);
                        }
                    }
                    return true;

                case TRACKER_INPUT_CURSOR_LEFT:
                    if (view->state.fx_edit_cursor > 0) {
                        view->state.fx_edit_cursor--;
                        tracker_view_invalidate(view);
                    }
                    return true;

                case TRACKER_INPUT_CURSOR_RIGHT:
                    if (view->state.fx_edit_cursor < (int)strlen(view->state.fx_edit_buffer)) {
                        view->state.fx_edit_cursor++;
                        tracker_view_invalidate(view);
                    }
                    return true;

                case TRACKER_INPUT_HOME:
                    view->state.fx_edit_cursor = 0;
                    tracker_view_invalidate(view);
                    return true;

                case TRACKER_INPUT_END:
                    view->state.fx_edit_cursor = (int)strlen(view->state.fx_edit_buffer);
                    tracker_view_invalidate(view);
                    return true;

                case TRACKER_INPUT_CURSOR_UP:
                    /* Cycle to previous FX type when editing name field */
                    if (view->state.fx_edit_field == 0 && entry) {
                        int current = -1;
                        for (int i = 0; i < num_fx_types; i++) {
                            if (strcmp(view->state.fx_edit_buffer, fx_types[i]) == 0) {
                                current = i;
                                break;
                            }
                        }
                        int next = (current <= 0) ? num_fx_types - 1 : current - 1;
                        strncpy(view->state.fx_edit_buffer, fx_types[next], 63);
                        view->state.fx_edit_buffer[63] = '\0';
                        view->state.fx_edit_cursor = (int)strlen(view->state.fx_edit_buffer);
                        tracker_view_invalidate(view);
                    }
                    return true;

                case TRACKER_INPUT_CURSOR_DOWN:
                    /* Cycle to next FX type when editing name field */
                    if (view->state.fx_edit_field == 0 && entry) {
                        int current = -1;
                        for (int i = 0; i < num_fx_types; i++) {
                            if (strcmp(view->state.fx_edit_buffer, fx_types[i]) == 0) {
                                current = i;
                                break;
                            }
                        }
                        int next = (current < 0 || current >= num_fx_types - 1) ? 0 : current + 1;
                        strncpy(view->state.fx_edit_buffer, fx_types[next], 63);
                        view->state.fx_edit_buffer[63] = '\0';
                        view->state.fx_edit_cursor = (int)strlen(view->state.fx_edit_buffer);
                        tracker_view_invalidate(view);
                    }
                    return true;

                case TRACKER_INPUT_TAB:
                    /* Switch between name and params fields */
                    if (entry) {
                        /* Save current field */
                        if (view->state.fx_edit_field == 0) {
                            free(entry->name);
                            entry->name = str_dup(view->state.fx_edit_buffer);
                        } else {
                            free(entry->params);
                            entry->params = str_dup(view->state.fx_edit_buffer);
                        }
                        /* Switch to other field */
                        view->state.fx_edit_field = 1 - view->state.fx_edit_field;
                        /* Load new field into buffer */
                        const char* src = (view->state.fx_edit_field == 0) ?
                            (entry->name ? entry->name : "") :
                            (entry->params ? entry->params : "");
                        strncpy(view->state.fx_edit_buffer, src, 63);
                        view->state.fx_edit_buffer[63] = '\0';
                        view->state.fx_edit_cursor = (int)strlen(view->state.fx_edit_buffer);
                        view->modified = true;
                        tracker_view_invalidate(view);
                    }
                    return true;

                case TRACKER_INPUT_ENTER_EDIT:
                    /* Save and exit edit mode */
                    if (entry) {
                        if (view->state.fx_edit_field == 0) {
                            free(entry->name);
                            entry->name = str_dup(view->state.fx_edit_buffer);
                        } else {
                            free(entry->params);
                            entry->params = str_dup(view->state.fx_edit_buffer);
                        }
                        view->modified = true;
                        tracker_view_show_status(view, "FX updated");
                    }
                    view->state.fx_editing = false;
                    tracker_view_invalidate(view);
                    return true;

                case TRACKER_INPUT_CANCEL:
                    /* Cancel edit mode without saving */
                    view->state.fx_editing = false;
                    tracker_view_show_status(view, "Edit cancelled");
                    tracker_view_invalidate(view);
                    return true;

                default:
                    return true;
            }
        }

        /* Normal FX navigation mode */
        switch (event->type) {
            case TRACKER_INPUT_CURSOR_UP:
                if (view->state.fx_cursor > 0) {
                    view->state.fx_cursor--;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_DOWN:
                if (chain && view->state.fx_cursor < chain->count - 1) {
                    view->state.fx_cursor++;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_LEFT:
                if (view->state.fx_edit_field > 0) {
                    view->state.fx_edit_field--;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_RIGHT:
                if (view->state.fx_edit_field < 1) {
                    view->state.fx_edit_field++;
                    tracker_view_invalidate(view);
                }
                return true;

            /* Enter edit mode with Enter, 'i', or 'e' */
            case TRACKER_INPUT_ENTER_EDIT:  /* Enter or 'i' key */
            case TRACKER_INPUT_FX_EDIT:
                if (chain && view->state.fx_cursor < chain->count) {
                    TrackerFxEntry* entry = tracker_fx_chain_get(chain, view->state.fx_cursor);
                    if (entry) {
                        view->state.fx_editing = true;
                        view->state.fx_edit_field = 0;  /* Start with name */
                        const char* src = entry->name ? entry->name : "";
                        strncpy(view->state.fx_edit_buffer, src, 63);
                        view->state.fx_edit_buffer[63] = '\0';
                        view->state.fx_edit_cursor = (int)strlen(view->state.fx_edit_buffer);
                        tracker_view_show_status(view, "Editing FX (Tab=switch field, Up/Down=cycle type)");
                        tracker_view_invalidate(view);
                    }
                }
                return true;

            /* FX target selection - accept both FX-specific and generic inputs */
            case TRACKER_INPUT_FX_CELL:
            case TRACKER_INPUT_CLONE_PATTERN:  /* 'c' key */
                view->state.fx_target = TRACKER_FX_TARGET_CELL;
                view->state.fx_cursor = 0;
                tracker_view_show_status(view, "Cell FX");
                tracker_view_invalidate(view);
                return true;
            case TRACKER_INPUT_FX_TRACK:
                view->state.fx_target = TRACKER_FX_TARGET_TRACK;
                view->state.fx_cursor = 0;
                tracker_view_show_status(view, "Track FX");
                tracker_view_invalidate(view);
                return true;
            case TRACKER_INPUT_FX_MASTER:
            case TRACKER_INPUT_MUTE_TRACK:  /* 'm' key */
                view->state.fx_target = TRACKER_FX_TARGET_MASTER;
                view->state.fx_cursor = 0;
                tracker_view_show_status(view, "Master FX");
                tracker_view_invalidate(view);
                return true;

            /* FX operations - accept both FX-specific and generic inputs */
            case TRACKER_INPUT_FX_ADD:
            case TRACKER_INPUT_ADD_TRACK:  /* 'a' key */
                if (chain) {
                    if (tracker_fx_chain_append(chain, "transpose", "0", NULL)) {
                        view->state.fx_cursor = chain->count - 1;
                        view->modified = true;
                        tracker_view_show_status(view, "Added FX");
                        tracker_view_invalidate(view);
                    }
                }
                return true;

            case TRACKER_INPUT_FX_REMOVE:
            case TRACKER_INPUT_CLEAR_CELL:  /* 'x' key */
                if (chain && chain->count > 0 && view->state.fx_cursor < chain->count) {
                    tracker_fx_chain_remove(chain, view->state.fx_cursor);
                    if (view->state.fx_cursor >= chain->count && chain->count > 0) {
                        view->state.fx_cursor = chain->count - 1;
                    }
                    view->modified = true;
                    tracker_view_show_status(view, "Removed FX");
                    tracker_view_invalidate(view);
                }
                return true;

            case TRACKER_INPUT_FX_MOVE_UP:
            case TRACKER_INPUT_SEQ_MOVE_UP:  /* 'K' key */
                if (chain && view->state.fx_cursor > 0) {
                    tracker_fx_chain_move(chain, view->state.fx_cursor, view->state.fx_cursor - 1);
                    view->state.fx_cursor--;
                    view->modified = true;
                    tracker_view_show_status(view, "Moved FX up");
                    tracker_view_invalidate(view);
                }
                return true;

            case TRACKER_INPUT_FX_MOVE_DOWN:
            case TRACKER_INPUT_SEQ_MOVE_DOWN:  /* 'J' key */
                if (chain && view->state.fx_cursor < chain->count - 1) {
                    tracker_fx_chain_move(chain, view->state.fx_cursor, view->state.fx_cursor + 1);
                    view->state.fx_cursor++;
                    view->modified = true;
                    tracker_view_show_status(view, "Moved FX down");
                    tracker_view_invalidate(view);
                }
                return true;

            case TRACKER_INPUT_FX_TOGGLE:
            case TRACKER_INPUT_PLAY_TOGGLE:  /* space key */
                if (chain && view->state.fx_cursor < chain->count) {
                    TrackerFxEntry* entry = tracker_fx_chain_get(chain, view->state.fx_cursor);
                    if (entry) {
                        entry->enabled = !entry->enabled;
                        view->modified = true;
                        tracker_view_show_status(view, "FX %s",
                            entry->enabled ? "enabled" : "disabled");
                        tracker_view_invalidate(view);
                    }
                }
                return true;

            case TRACKER_INPUT_CANCEL:
            case TRACKER_INPUT_MODE_PATTERN:
                tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
                tracker_view_invalidate(view);
                return true;

            case TRACKER_INPUT_QUIT:
            case TRACKER_INPUT_MODE_HELP:
            case TRACKER_INPUT_SAVE:
                /* Fall through to normal handler */
                break;

            default:
                return true;
        }
    }

    /* Handle mixer mode input */
    if (view->state.view_mode == TRACKER_VIEW_MODE_MIXER) {
        TrackerPattern* pattern = tracker_view_get_current_pattern(view);
        TrackerTrack* track = NULL;
        if (pattern && view->state.mixer_cursor < pattern->num_tracks) {
            track = &pattern->tracks[view->state.mixer_cursor];
        }

        switch (event->type) {
            case TRACKER_INPUT_CURSOR_LEFT:
                if (view->state.mixer_cursor > 0) {
                    view->state.mixer_cursor--;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_RIGHT:
                if (pattern && view->state.mixer_cursor < pattern->num_tracks - 1) {
                    view->state.mixer_cursor++;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_UP:
                if (view->state.mixer_field > 0) {
                    view->state.mixer_field--;
                    tracker_view_invalidate(view);
                }
                return true;
            case TRACKER_INPUT_CURSOR_DOWN:
                if (view->state.mixer_field < 3) {
                    view->state.mixer_field++;
                    tracker_view_invalidate(view);
                }
                return true;

            /* Volume control */
            case TRACKER_INPUT_VOLUME_UP:
            case TRACKER_INPUT_STEP_INC:  /* + key */
                if (track && view->state.mixer_field == 0) {
                    int new_vol = track->volume + 5;
                    if (new_vol > 127) new_vol = 127;
                    track->volume = (uint8_t)new_vol;
                    view->modified = true;
                    tracker_view_show_status(view, "Volume: %d", track->volume);
                    tracker_view_invalidate(view);
                } else if (track && view->state.mixer_field == 1) {
                    /* Pan right */
                    int new_pan = track->pan + 8;
                    if (new_pan > 63) new_pan = 63;
                    track->pan = (int8_t)new_pan;
                    view->modified = true;
                    tracker_view_show_status(view, "Pan: %d", track->pan);
                    tracker_view_invalidate(view);
                }
                return true;

            case TRACKER_INPUT_VOLUME_DOWN:
            case TRACKER_INPUT_STEP_DEC:  /* - key */
                if (track && view->state.mixer_field == 0) {
                    int new_vol = track->volume - 5;
                    if (new_vol < 0) new_vol = 0;
                    track->volume = (uint8_t)new_vol;
                    view->modified = true;
                    tracker_view_show_status(view, "Volume: %d", track->volume);
                    tracker_view_invalidate(view);
                } else if (track && view->state.mixer_field == 1) {
                    /* Pan left */
                    int new_pan = track->pan - 8;
                    if (new_pan < -64) new_pan = -64;
                    track->pan = (int8_t)new_pan;
                    view->modified = true;
                    tracker_view_show_status(view, "Pan: %d", track->pan);
                    tracker_view_invalidate(view);
                }
                return true;

            /* Mute/Solo toggle */
            case TRACKER_INPUT_MUTE_TRACK:
                if (track) {
                    track->muted = !track->muted;
                    view->modified = true;
                    tracker_view_show_status(view, "Track %s",
                        track->muted ? "muted" : "unmuted");
                    tracker_view_invalidate(view);
                }
                return true;

            case TRACKER_INPUT_SOLO_TRACK:
                if (track) {
                    track->solo = !track->solo;
                    view->modified = true;
                    tracker_view_show_status(view, "Track %s",
                        track->solo ? "soloed" : "unsoloed");
                    tracker_view_invalidate(view);
                }
                return true;

            /* Reset controls */
            case TRACKER_INPUT_VOLUME_RESET:
            case TRACKER_INPUT_CLEAR_CELL:  /* x key - reset selected param */
                if (track) {
                    if (view->state.mixer_field == 0) {
                        track->volume = 100;
                        tracker_view_show_status(view, "Volume reset");
                    } else if (view->state.mixer_field == 1) {
                        track->pan = 0;
                        tracker_view_show_status(view, "Pan reset");
                    } else if (view->state.mixer_field == 2) {
                        track->muted = false;
                        tracker_view_show_status(view, "Unmuted");
                    } else if (view->state.mixer_field == 3) {
                        track->solo = false;
                        tracker_view_show_status(view, "Unsolo");
                    }
                    view->modified = true;
                    tracker_view_invalidate(view);
                }
                return true;

            /* Enter/space toggles mute/solo when on those fields */
            case TRACKER_INPUT_ENTER_EDIT:
            case TRACKER_INPUT_PLAY_TOGGLE:
                if (track) {
                    if (view->state.mixer_field == 2) {
                        track->muted = !track->muted;
                        view->modified = true;
                        tracker_view_show_status(view, "Track %s",
                            track->muted ? "muted" : "unmuted");
                    } else if (view->state.mixer_field == 3) {
                        track->solo = !track->solo;
                        view->modified = true;
                        tracker_view_show_status(view, "Track %s",
                            track->solo ? "soloed" : "unsoloed");
                    }
                    tracker_view_invalidate(view);
                }
                return true;

            case TRACKER_INPUT_CANCEL:
            case TRACKER_INPUT_MODE_PATTERN:
                tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
                tracker_view_invalidate(view);
                return true;

            case TRACKER_INPUT_QUIT:
            case TRACKER_INPUT_MODE_HELP:
            case TRACKER_INPUT_SAVE:
                /* Fall through to normal handler */
                break;

            default:
                return true;
        }
    }

    /* Handle edit mode input */
    if (view->state.edit_mode == TRACKER_EDIT_MODE_EDIT) {
        switch (event->type) {
            case TRACKER_INPUT_CHAR:
                tracker_view_edit_char(view, event->character);
                break;
            case TRACKER_INPUT_BACKSPACE:
                /* Delete char before cursor */
                if (view->state.edit_cursor_pos > 0 && view->state.edit_buffer) {
                    int len = view->state.edit_buffer_len;
                    int pos = view->state.edit_cursor_pos;
                    memmove(&view->state.edit_buffer[pos - 1],
                            &view->state.edit_buffer[pos], len - pos + 1);
                    view->state.edit_buffer_len--;
                    view->state.edit_cursor_pos--;
                    tracker_view_invalidate_cursor(view);
                }
                break;
            case TRACKER_INPUT_DELETE:
                /* Delete char at cursor */
                if (view->state.edit_buffer &&
                    view->state.edit_cursor_pos < view->state.edit_buffer_len) {
                    int len = view->state.edit_buffer_len;
                    int pos = view->state.edit_cursor_pos;
                    memmove(&view->state.edit_buffer[pos],
                            &view->state.edit_buffer[pos + 1], len - pos);
                    view->state.edit_buffer_len--;
                    tracker_view_invalidate_cursor(view);
                }
                break;
            case TRACKER_INPUT_EXIT_EDIT:
                tracker_view_edit_confirm(view);
                break;
            case TRACKER_INPUT_CANCEL:
                tracker_view_edit_cancel(view);
                break;
            case TRACKER_INPUT_CURSOR_LEFT:
                if (view->state.edit_cursor_pos > 0) {
                    view->state.edit_cursor_pos--;
                    tracker_view_invalidate_cursor(view);
                }
                break;
            case TRACKER_INPUT_CURSOR_RIGHT:
                if (view->state.edit_cursor_pos < view->state.edit_buffer_len) {
                    view->state.edit_cursor_pos++;
                    tracker_view_invalidate_cursor(view);
                }
                break;
            default:
                handled = false;
                break;
        }
        return handled;
    }

    /* Handle command mode input */
    if (view->state.edit_mode == TRACKER_EDIT_MODE_COMMAND) {
        switch (event->type) {
            case TRACKER_INPUT_CHAR: {
                /* Add char to command buffer */
                if (!view->state.command_buffer) {
                    view->state.command_buffer = calloc(256, 1);
                    view->state.command_buffer_capacity = 256;
                }
                if (view->state.command_buffer_len + 1 < view->state.command_buffer_capacity) {
                    int pos = view->state.command_cursor_pos;
                    int len = view->state.command_buffer_len;
                    /* Shift characters to make room */
                    memmove(&view->state.command_buffer[pos + 1],
                            &view->state.command_buffer[pos], len - pos + 1);
                    view->state.command_buffer[pos] = (char)event->character;
                    view->state.command_buffer_len++;
                    view->state.command_cursor_pos++;
                    tracker_view_invalidate_status(view);
                }
                break;
            }
            case TRACKER_INPUT_BACKSPACE:
                if (view->state.command_buffer && view->state.command_cursor_pos > 0) {
                    int pos = view->state.command_cursor_pos;
                    int len = view->state.command_buffer_len;
                    memmove(&view->state.command_buffer[pos - 1],
                            &view->state.command_buffer[pos], len - pos + 1);
                    view->state.command_buffer_len--;
                    view->state.command_cursor_pos--;
                    tracker_view_invalidate_status(view);
                }
                break;
            case TRACKER_INPUT_DELETE:
                if (view->state.command_buffer &&
                    view->state.command_cursor_pos < view->state.command_buffer_len) {
                    int pos = view->state.command_cursor_pos;
                    int len = view->state.command_buffer_len;
                    memmove(&view->state.command_buffer[pos],
                            &view->state.command_buffer[pos + 1], len - pos);
                    view->state.command_buffer_len--;
                    tracker_view_invalidate_status(view);
                }
                break;
            case TRACKER_INPUT_CURSOR_LEFT:
                if (view->state.command_cursor_pos > 0) {
                    view->state.command_cursor_pos--;
                    tracker_view_invalidate_status(view);
                }
                break;
            case TRACKER_INPUT_CURSOR_RIGHT:
                if (view->state.command_cursor_pos < view->state.command_buffer_len) {
                    view->state.command_cursor_pos++;
                    tracker_view_invalidate_status(view);
                }
                break;
            case TRACKER_INPUT_HOME:
                view->state.command_cursor_pos = 0;
                tracker_view_invalidate_status(view);
                break;
            case TRACKER_INPUT_END:
                view->state.command_cursor_pos = view->state.command_buffer_len;
                tracker_view_invalidate_status(view);
                break;
            case TRACKER_INPUT_ENTER_EDIT:
            case TRACKER_INPUT_EXIT_EDIT:
                tracker_view_exit_command(view, true);
                break;
            case TRACKER_INPUT_CANCEL:
                tracker_view_exit_command(view, false);
                break;
            default:
                handled = false;
                break;
        }
        return handled;
    }

    /* Normal mode input handling */
    switch (event->type) {
        /* Navigation */
        case TRACKER_INPUT_CURSOR_UP:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_up(view, event->repeat_count ? event->repeat_count : 1);
            if (shift || view->state.selecting) tracker_view_select_extend(view);
            break;
        case TRACKER_INPUT_CURSOR_DOWN:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_down(view, event->repeat_count ? event->repeat_count : 1);
            if (shift || view->state.selecting) tracker_view_select_extend(view);
            break;
        case TRACKER_INPUT_CURSOR_LEFT:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_left(view, event->repeat_count ? event->repeat_count : 1);
            if (shift || view->state.selecting) tracker_view_select_extend(view);
            break;
        case TRACKER_INPUT_CURSOR_RIGHT:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_right(view, event->repeat_count ? event->repeat_count : 1);
            if (shift || view->state.selecting) tracker_view_select_extend(view);
            break;
        case TRACKER_INPUT_PAGE_UP:
            tracker_view_cursor_page_up(view);
            break;
        case TRACKER_INPUT_PAGE_DOWN:
            tracker_view_cursor_page_down(view);
            break;
        case TRACKER_INPUT_HOME:
            tracker_view_cursor_home(view);
            break;
        case TRACKER_INPUT_END:
            tracker_view_cursor_end(view);
            break;
        case TRACKER_INPUT_PATTERN_START:
            tracker_view_cursor_pattern_start(view);
            break;
        case TRACKER_INPUT_PATTERN_END:
            tracker_view_cursor_pattern_end(view);
            break;
        case TRACKER_INPUT_NEXT_PATTERN:
            tracker_view_next_pattern(view);
            break;
        case TRACKER_INPUT_PREV_PATTERN:
            tracker_view_prev_pattern(view);
            break;
        case TRACKER_INPUT_NEW_PATTERN:
            tracker_view_new_pattern(view);
            break;
        case TRACKER_INPUT_CLONE_PATTERN:
            tracker_view_clone_pattern(view);
            break;
        case TRACKER_INPUT_DELETE_PATTERN:
            tracker_view_delete_pattern(view);
            break;
        case TRACKER_INPUT_ADD_TRACK:
            tracker_view_add_track(view);
            break;
        case TRACKER_INPUT_DELETE_TRACK:
            tracker_view_remove_track(view);
            break;

        /* Editing */
        case TRACKER_INPUT_ENTER_EDIT:
            tracker_view_enter_edit(view);
            break;
        case TRACKER_INPUT_CLEAR_CELL:
            tracker_view_clear_cell(view);
            break;
        case TRACKER_INPUT_DELETE:
            tracker_view_clear_selection(view);
            break;
        case TRACKER_INPUT_INSERT_ROW:
            tracker_view_insert_row(view);
            break;
        case TRACKER_INPUT_DELETE_ROW:
            tracker_view_delete_row(view);
            break;
        case TRACKER_INPUT_DUPLICATE_ROW:
            tracker_view_duplicate_row(view);
            break;

        /* Selection */
        case TRACKER_INPUT_SELECT_START:
            tracker_view_select_start(view);
            tracker_view_show_status(view, "-- VISUAL --");
            break;
        case TRACKER_INPUT_SELECT_ALL:
            tracker_view_select_all(view);
            break;
        case TRACKER_INPUT_SELECT_TRACK:
            tracker_view_select_track(view);
            break;
        case TRACKER_INPUT_SELECT_ROW:
            tracker_view_select_row(view);
            break;
        case TRACKER_INPUT_SELECT_PATTERN:
            tracker_view_select_pattern(view);
            break;

        /* Clipboard */
        case TRACKER_INPUT_CUT:
            if (tracker_view_cut(view)) {
                int count = view->clipboard.width * view->clipboard.height;
                tracker_view_show_status(view, "Cut %d cell%s", count, count == 1 ? "" : "s");
            }
            break;
        case TRACKER_INPUT_COPY:
            if (tracker_view_copy(view)) {
                int count = view->clipboard.width * view->clipboard.height;
                tracker_view_show_status(view, "Copied %d cell%s", count, count == 1 ? "" : "s");
            }
            break;
        case TRACKER_INPUT_PASTE:
            if (tracker_view_paste(view)) {
                tracker_view_show_status(view, "Pasted");
            }
            break;
        case TRACKER_INPUT_PASTE_INSERT:
            if (tracker_view_paste_insert(view)) {
                tracker_view_show_status(view, "Pasted (insert)");
            }
            break;

        /* Transport */
        case TRACKER_INPUT_PLAY:
        case TRACKER_INPUT_PLAY_TOGGLE:
            if (view->engine) tracker_engine_toggle(view->engine);
            break;
        case TRACKER_INPUT_STOP:
            if (view->engine) tracker_engine_stop(view->engine);
            break;
        case TRACKER_INPUT_PAUSE:
            if (view->engine) tracker_engine_pause(view->engine);
            break;
        case TRACKER_INPUT_PLAY_FROM_START:
            if (view->engine) {
                tracker_engine_seek(view->engine, view->state.cursor_pattern, 0);
                tracker_engine_play(view->engine);
            }
            break;
        case TRACKER_INPUT_PLAY_FROM_CURSOR:
            if (view->engine) {
                tracker_engine_seek(view->engine, view->state.cursor_pattern,
                                    view->state.cursor_row);
                tracker_engine_play(view->engine);
            }
            break;
        case TRACKER_INPUT_PLAY_ROW:
            if (view->engine) {
                tracker_engine_trigger_cell(view->engine, view->state.cursor_pattern,
                    view->state.cursor_track, view->state.cursor_row);
            }
            break;
        case TRACKER_INPUT_RECORD_TOGGLE:
            view->state.is_recording = !view->state.is_recording;
            tracker_view_show_status(view, "Record: %s",
                view->state.is_recording ? "ON" : "OFF");
            tracker_view_invalidate_status(view);
            break;

        /* Track control */
        case TRACKER_INPUT_MUTE_TRACK:
            if (view->engine && view->song) {
                TrackerPattern* p = tracker_view_get_current_pattern(view);
                if (p && view->state.cursor_track < p->num_tracks) {
                    bool muted = p->tracks[view->state.cursor_track].muted;
                    tracker_engine_mute_track(view->engine, view->state.cursor_track, !muted);
                    tracker_view_invalidate(view);
                    tracker_view_show_status(view, "Track %d: %s",
                        view->state.cursor_track + 1, !muted ? "Muted" : "Unmuted");
                }
            }
            break;
        case TRACKER_INPUT_SOLO_TRACK:
            if (view->engine && view->song) {
                TrackerPattern* p = tracker_view_get_current_pattern(view);
                if (p && view->state.cursor_track < p->num_tracks) {
                    bool solo = p->tracks[view->state.cursor_track].solo;
                    tracker_engine_solo_track(view->engine, view->state.cursor_track, !solo);
                    tracker_view_invalidate(view);
                    tracker_view_show_status(view, "Track %d: %s",
                        view->state.cursor_track + 1, !solo ? "Solo" : "Solo off");
                }
            }
            break;

        /* View modes */
        case TRACKER_INPUT_MODE_PATTERN:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
            break;
        case TRACKER_INPUT_MODE_ARRANGE:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_ARRANGE);
            break;
        case TRACKER_INPUT_MODE_MIXER:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_MIXER);
            break;
        case TRACKER_INPUT_MODE_HELP:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_HELP);
            break;
        case TRACKER_INPUT_MODE_FX:
            view->state.fx_cursor = 0;
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_FX);
            break;
        case TRACKER_INPUT_FOLLOW_TOGGLE:
            view->state.follow_playback = !view->state.follow_playback;
            tracker_view_show_status(view, "Follow: %s",
                view->state.follow_playback ? "ON" : "OFF");
            break;

        /* Undo/Redo */
        case TRACKER_INPUT_UNDO:
            if (tracker_undo_can_undo(&view->undo)) {
                const char* desc = tracker_undo_get_undo_description(&view->undo);
                tracker_view_undo(view);
                tracker_view_show_status(view, "Undo: %s", desc ? desc : "action");
                view->modified = true;
            } else {
                tracker_view_show_status(view, "Nothing to undo");
            }
            break;
        case TRACKER_INPUT_REDO:
            if (tracker_undo_can_redo(&view->undo)) {
                const char* desc = tracker_undo_get_redo_description(&view->undo);
                tracker_view_redo(view);
                tracker_view_show_status(view, "Redo: %s", desc ? desc : "action");
                view->modified = true;
            } else {
                tracker_view_show_status(view, "Nothing to redo");
            }
            break;

        /* Misc */
        case TRACKER_INPUT_COMMAND_MODE:
            tracker_view_enter_command(view);
            break;
        case TRACKER_INPUT_QUIT:
            tracker_view_request_quit(view);
            break;
        case TRACKER_INPUT_CANCEL:  /* Escape clears selection first, then quits */
            if (view->state.selecting) {
                tracker_view_select_clear(view);
            } else {
                tracker_view_request_quit(view);
            }
            break;
        case TRACKER_INPUT_PANIC:
            if (view->engine) tracker_engine_all_notes_off(view->engine);
            break;

        case TRACKER_INPUT_CYCLE_THEME: {
            /* Cycle through available themes */
            int count = 0;
            const char** themes = tracker_theme_list(&count);
            if (count > 0 && view->state.theme) {
                /* Find current theme index */
                int current = 0;
                for (int i = 0; i < count; i++) {
                    if (view->state.theme->name && strcmp(view->state.theme->name, themes[i]) == 0) {
                        current = i;
                        break;
                    }
                }
                /* Switch to next theme */
                int next = (current + 1) % count;
                tracker_view_set_theme_by_name(view, themes[next]);
                tracker_view_show_status(view, "Theme: %s", themes[next]);
                tracker_view_invalidate(view);
            }
            break;
        }

        case TRACKER_INPUT_SAVE:
            if (tracker_view_save(view, NULL)) {
                tracker_view_show_status(view, "Saved: %s",
                    view->file_path ? view->file_path : "song.trk");
            } else {
                tracker_view_show_error(view, "Save failed");
            }
            break;

        case TRACKER_INPUT_OPEN:
            /* TODO: implement file picker or command mode for file path */
            tracker_view_show_status(view, "Open: use command line to load files");
            break;

        case TRACKER_INPUT_STEP_INC:
            if (view->state.step_size < 16) {
                view->state.step_size++;
            }
            tracker_view_show_status(view, "Step: %d", view->state.step_size);
            break;
        case TRACKER_INPUT_STEP_DEC:
            if (view->state.step_size > 0) {
                view->state.step_size--;
            }
            tracker_view_show_status(view, "Step: %d", view->state.step_size);
            break;
        case TRACKER_INPUT_OCTAVE_INC:
            if (view->state.default_octave < 9) {
                view->state.default_octave++;
            }
            tracker_view_show_status(view, "Octave: %d", view->state.default_octave);
            break;
        case TRACKER_INPUT_OCTAVE_DEC:
            if (view->state.default_octave > 0) {
                view->state.default_octave--;
            }
            tracker_view_show_status(view, "Octave: %d", view->state.default_octave);
            break;

        case TRACKER_INPUT_BPM_INC:
            if (view->song && view->engine) {
                int new_bpm = view->song->bpm + 5;
                if (new_bpm > 300) new_bpm = 300;
                view->song->bpm = new_bpm;
                tracker_engine_set_bpm(view->engine, new_bpm);
                view->modified = true;
                tracker_view_show_status(view, "BPM: %d", new_bpm);
                tracker_view_invalidate_status(view);
            }
            break;
        case TRACKER_INPUT_BPM_DEC:
            if (view->song && view->engine) {
                int new_bpm = view->song->bpm - 5;
                if (new_bpm < 20) new_bpm = 20;
                view->song->bpm = new_bpm;
                tracker_engine_set_bpm(view->engine, new_bpm);
                view->modified = true;
                tracker_view_show_status(view, "BPM: %d", new_bpm);
                tracker_view_invalidate_status(view);
            }
            break;
        case TRACKER_INPUT_LOOP_TOGGLE:
            if (view->engine) {
                bool loop_enabled = !view->engine->loop_enabled;
                tracker_engine_set_loop(view->engine, loop_enabled);
                if (loop_enabled) {
                    /* Set loop to current pattern boundaries */
                    tracker_engine_set_loop_points(view->engine, -1, -1);
                    tracker_view_show_status(view, "Loop: ON (pattern)");
                } else {
                    tracker_view_show_status(view, "Loop: OFF");
                }
                tracker_view_invalidate_status(view);
            }
            break;
        case TRACKER_INPUT_LOOP_SELECTION:
            if (view->engine && view->state.selection.type != TRACKER_SEL_NONE) {
                tracker_engine_set_loop(view->engine, true);
                tracker_engine_set_loop_points(view->engine,
                    view->state.selection.start_row,
                    view->state.selection.end_row);
                tracker_view_show_status(view, "Loop: rows %d-%d",
                    view->state.selection.start_row + 1,
                    view->state.selection.end_row + 1);
                tracker_view_invalidate_status(view);
            }
            break;

        case TRACKER_INPUT_PLAY_MODE_TOGGLE:
            if (view->engine) {
                TrackerPlayMode mode = view->engine->play_mode;
                if (mode == TRACKER_PLAY_MODE_PATTERN) {
                    /* Switch to song mode */
                    if (view->song && view->song->sequence_length > 0) {
                        tracker_engine_set_play_mode(view->engine, TRACKER_PLAY_MODE_SONG);
                        /* If not playing, reset to start of sequence */
                        if (view->engine->state != TRACKER_ENGINE_PLAYING) {
                            view->engine->current_pattern = 0;
                        }
                        tracker_view_show_status(view, "Play mode: SONG (%d patterns)",
                            view->song->sequence_length);
                    } else {
                        tracker_view_show_status(view, "No sequence - add patterns with 'r' then 'a'");
                    }
                } else {
                    /* Switch to pattern mode */
                    tracker_engine_set_play_mode(view->engine, TRACKER_PLAY_MODE_PATTERN);
                    tracker_view_show_status(view, "Play mode: PATTERN");
                }
                tracker_view_invalidate_status(view);
            }
            break;

        case TRACKER_INPUT_EXPORT_MIDI: {
            /* Generate default filename based on song name or file path */
            char filename[256];
            if (view->file_path) {
                /* Replace extension with .mid */
                if (!derive_midi_path(view->file_path, filename, sizeof(filename))) {
                    tracker_view_show_error(view, "File path too long to export");
                    break;
                }
            } else if (view->song && view->song->name) {
                snprintf(filename, sizeof(filename), "%s.mid", view->song->name);
            } else {
                snprintf(filename, sizeof(filename), "song.mid");
            }

            if (tracker_export_midi(view, filename)) {
                tracker_view_show_status(view, "Exported: %s", filename);
            } else {
                const char* err = loki_midi_export_error();
                tracker_view_show_error(view, "Export failed: %s",
                    err ? err : "unknown error");
            }
            break;
        }

        /* Sequence/Arrange operations */
        case TRACKER_INPUT_SEQ_ADD:
            if (view->song) {
                int pattern_idx = view->state.cursor_pattern;
                if (tracker_song_append_to_sequence(view->song, pattern_idx, 1)) {
                    view->modified = true;
                    tracker_view_show_status(view, "Added pattern %d to sequence",
                        pattern_idx + 1);
                    tracker_view_invalidate(view);
                }
            }
            break;

        case TRACKER_INPUT_SEQ_REMOVE:
            if (view->song && view->song->sequence_length > 0) {
                int idx = view->state.sequence_cursor;
                if (idx >= 0 && idx < view->song->sequence_length) {
                    /* Shift entries down */
                    for (int i = idx; i < view->song->sequence_length - 1; i++) {
                        view->song->sequence[i] = view->song->sequence[i + 1];
                    }
                    view->song->sequence_length--;
                    /* Adjust cursor */
                    if (view->state.sequence_cursor >= view->song->sequence_length &&
                        view->song->sequence_length > 0) {
                        view->state.sequence_cursor = view->song->sequence_length - 1;
                    }
                    view->modified = true;
                    tracker_view_show_status(view, "Removed sequence entry %d", idx + 1);
                    tracker_view_invalidate(view);
                }
            }
            break;

        case TRACKER_INPUT_SEQ_MOVE_UP:
            if (view->song && view->state.sequence_cursor > 0) {
                int idx = view->state.sequence_cursor;
                TrackerSequenceEntry tmp = view->song->sequence[idx];
                view->song->sequence[idx] = view->song->sequence[idx - 1];
                view->song->sequence[idx - 1] = tmp;
                view->state.sequence_cursor--;
                view->modified = true;
                tracker_view_show_status(view, "Moved entry up");
                tracker_view_invalidate(view);
            }
            break;

        case TRACKER_INPUT_SEQ_MOVE_DOWN:
            if (view->song &&
                view->state.sequence_cursor < view->song->sequence_length - 1) {
                int idx = view->state.sequence_cursor;
                TrackerSequenceEntry tmp = view->song->sequence[idx];
                view->song->sequence[idx] = view->song->sequence[idx + 1];
                view->song->sequence[idx + 1] = tmp;
                view->state.sequence_cursor++;
                view->modified = true;
                tracker_view_show_status(view, "Moved entry down");
                tracker_view_invalidate(view);
            }
            break;

        case TRACKER_INPUT_SEQ_GOTO:
            if (view->song && view->song->sequence_length > 0) {
                int idx = view->state.sequence_cursor;
                if (idx >= 0 && idx < view->song->sequence_length) {
                    int pattern_idx = view->song->sequence[idx].pattern_index;
                    if (pattern_idx >= 0 && pattern_idx < view->song->num_patterns) {
                        view->state.cursor_pattern = pattern_idx;
                        tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
                        tracker_view_show_status(view, "Pattern %d", pattern_idx + 1);
                        tracker_view_invalidate(view);
                    }
                }
            }
            break;

        default:
            handled = false;
            break;
    }

    return handled;
}

void tracker_view_edit_char(TrackerView* view, uint32_t character) {
    if (!view) return;
    if (view->state.edit_mode != TRACKER_EDIT_MODE_EDIT) return;

    /* Ensure buffer exists */
    if (!view->state.edit_buffer) {
        view->state.edit_buffer_capacity = 256;
        view->state.edit_buffer = calloc(view->state.edit_buffer_capacity, 1);
        if (!view->state.edit_buffer) return;
    }

    /* Grow buffer if needed */
    if (view->state.edit_buffer_len + 4 >= view->state.edit_buffer_capacity) {
        int new_cap = view->state.edit_buffer_capacity * 2;
        char* new_buf = realloc(view->state.edit_buffer, new_cap);
        if (!new_buf) return;
        view->state.edit_buffer = new_buf;
        view->state.edit_buffer_capacity = new_cap;
    }

    /* Insert character at cursor (simplified: ASCII only for now) */
    if (character < 128) {
        int pos = view->state.edit_cursor_pos;
        int len = view->state.edit_buffer_len;

        memmove(&view->state.edit_buffer[pos + 1],
                &view->state.edit_buffer[pos], len - pos + 1);

        view->state.edit_buffer[pos] = (char)character;
        view->state.edit_buffer_len++;
        view->state.edit_cursor_pos++;
    }

    tracker_view_invalidate_cursor(view);
}

void tracker_view_edit_confirm(TrackerView* view) {
    if (!view || !view->song) return;
    if (view->state.edit_mode != TRACKER_EDIT_MODE_EDIT) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    TrackerCell* cell = tracker_pattern_get_cell(pattern,
        view->state.cursor_row, view->state.cursor_track);
    if (!cell) return;

    /* Record undo */
    TrackerCell old_cell;
    tracker_cell_clone(&old_cell, cell);

    /* Update cell */
    tracker_cell_set_expression(cell, view->state.edit_buffer,
                                cell->language_id);

    /* Record undo */
    tracker_undo_record_cell_edit(&view->undo, view,
        view->state.cursor_pattern, view->state.cursor_track, view->state.cursor_row,
        &old_cell, cell);

    tracker_cell_clear(&old_cell);

    view->modified = true;

    /* Exit edit mode */
    view->state.edit_mode = TRACKER_EDIT_MODE_NAVIGATE;

    /* Advance by step size */
    if (view->state.step_size > 0) {
        tracker_view_cursor_down(view, view->state.step_size);
    }

    tracker_view_invalidate(view);
}

void tracker_view_edit_cancel(TrackerView* view) {
    if (!view) return;

    view->state.edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    tracker_view_invalidate_cursor(view);
}

/*============================================================================
 * Cursor Movement
 *============================================================================*/

void tracker_view_cursor_up(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_row -= count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_down(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_row += count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_left(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_track -= count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_right(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_track += count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_page_up(TrackerView* view) {
    if (!view) return;
    tracker_view_cursor_up(view, view->state.visible_rows);
}

void tracker_view_cursor_page_down(TrackerView* view) {
    if (!view) return;
    tracker_view_cursor_down(view, view->state.visible_rows);
}

void tracker_view_cursor_home(TrackerView* view) {
    if (!view) return;
    view->state.cursor_track = 0;
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_end(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (pattern) {
        view->state.cursor_track = pattern->num_tracks - 1;
    }
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_pattern_start(TrackerView* view) {
    if (!view) return;
    view->state.cursor_row = 0;
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_pattern_end(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (pattern) {
        view->state.cursor_row = pattern->num_rows - 1;
    }
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_goto(TrackerView* view, int pattern, int track, int row) {
    if (!view) return;

    view->state.cursor_pattern = pattern;
    view->state.cursor_track = track;
    view->state.cursor_row = row;

    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

/*============================================================================
 * Pattern Management
 *============================================================================*/

void tracker_view_next_pattern(TrackerView* view) {
    if (!view || !view->song) return;
    if (view->song->num_patterns <= 1) return;

    int next = view->state.cursor_pattern + 1;
    if (next >= view->song->num_patterns) {
        next = 0;  /* Wrap around */
    }

    view->state.cursor_pattern = next;
    view->state.cursor_row = 0;  /* Reset row on pattern change */
    view->state.scroll_row = 0;

    /* Sync engine if attached */
    if (view->engine) {
        tracker_engine_seek(view->engine, next, 0);
    }

    tracker_view_clamp_cursor(view);
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Pattern %d/%d",
        view->state.cursor_pattern + 1, view->song->num_patterns);
}

void tracker_view_prev_pattern(TrackerView* view) {
    if (!view || !view->song) return;
    if (view->song->num_patterns <= 1) return;

    int prev = view->state.cursor_pattern - 1;
    if (prev < 0) {
        prev = view->song->num_patterns - 1;  /* Wrap around */
    }

    view->state.cursor_pattern = prev;
    view->state.cursor_row = 0;  /* Reset row on pattern change */
    view->state.scroll_row = 0;

    /* Sync engine if attached */
    if (view->engine) {
        tracker_engine_seek(view->engine, prev, 0);
    }

    tracker_view_clamp_cursor(view);
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Pattern %d/%d",
        view->state.cursor_pattern + 1, view->song->num_patterns);
}

void tracker_view_new_pattern(TrackerView* view) {
    if (!view || !view->song) return;

    /* Get current pattern for reference */
    TrackerPattern* current = tracker_view_get_current_pattern(view);
    int rows = current ? current->num_rows : TRACKER_DEFAULT_ROWS;
    int tracks = current ? current->num_tracks : 4;

    /* Generate pattern name */
    char name[64];
    snprintf(name, sizeof(name), "Pattern %d", view->song->num_patterns + 1);

    /* Create new pattern */
    TrackerPattern* pattern = tracker_pattern_new(rows, tracks, name);
    if (!pattern) {
        tracker_view_show_status(view, "Failed to create pattern");
        return;
    }

    /* Copy track names and channels from current pattern */
    if (current) {
        for (int t = 0; t < tracks && t < current->num_tracks; t++) {
            if (current->tracks[t].name) {
                free(pattern->tracks[t].name);
                pattern->tracks[t].name = strdup(current->tracks[t].name);
            }
            pattern->tracks[t].default_channel = current->tracks[t].default_channel;
        }
    }

    /* Add to song */
    int new_index = tracker_song_add_pattern(view->song, pattern);
    if (new_index < 0) {
        tracker_pattern_free(pattern);
        tracker_view_show_status(view, "Failed to add pattern");
        return;
    }

    /* Navigate to new pattern */
    view->state.cursor_pattern = new_index;
    view->state.cursor_row = 0;
    view->state.scroll_row = 0;

    if (view->engine) {
        tracker_engine_seek(view->engine, new_index, 0);
    }

    view->modified = true;
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Created pattern %d", new_index + 1);
}

void tracker_view_clone_pattern(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* current = tracker_view_get_current_pattern(view);
    if (!current) return;

    /* Generate pattern name */
    char name[64];
    if (current->name) {
        snprintf(name, sizeof(name), "%s (copy)", current->name);
    } else {
        snprintf(name, sizeof(name), "Pattern %d (copy)", view->state.cursor_pattern + 1);
    }

    /* Create new pattern with same dimensions */
    TrackerPattern* pattern = tracker_pattern_new(current->num_rows, current->num_tracks, name);
    if (!pattern) {
        tracker_view_show_status(view, "Failed to clone pattern");
        return;
    }

    /* Copy track data */
    for (int t = 0; t < current->num_tracks; t++) {
        TrackerTrack* src_track = &current->tracks[t];
        TrackerTrack* dst_track = &pattern->tracks[t];

        /* Copy track properties */
        if (src_track->name) {
            free(dst_track->name);
            dst_track->name = strdup(src_track->name);
        }
        dst_track->default_channel = src_track->default_channel;
        dst_track->muted = src_track->muted;
        dst_track->solo = src_track->solo;

        /* Copy cells */
        for (int r = 0; r < current->num_rows; r++) {
            TrackerCell* src = &src_track->cells[r];
            TrackerCell* dst = &dst_track->cells[r];
            tracker_cell_clone(dst, src);
        }
    }

    /* Add to song */
    int new_index = tracker_song_add_pattern(view->song, pattern);
    if (new_index < 0) {
        tracker_pattern_free(pattern);
        tracker_view_show_status(view, "Failed to add cloned pattern");
        return;
    }

    /* Navigate to new pattern */
    view->state.cursor_pattern = new_index;
    view->state.cursor_row = 0;
    view->state.scroll_row = 0;

    if (view->engine) {
        tracker_engine_seek(view->engine, new_index, 0);
    }

    view->modified = true;
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Cloned to pattern %d", new_index + 1);
}

void tracker_view_delete_pattern(TrackerView* view) {
    if (!view || !view->song) return;

    /* Don't allow deleting the last pattern */
    if (view->song->num_patterns <= 1) {
        tracker_view_show_status(view, "Cannot delete last pattern");
        return;
    }

    int delete_index = view->state.cursor_pattern;

    /* Remove pattern from song */
    if (!tracker_song_remove_pattern(view->song, delete_index)) {
        tracker_view_show_status(view, "Failed to delete pattern");
        return;
    }

    /* Adjust cursor if needed */
    if (view->state.cursor_pattern >= view->song->num_patterns) {
        view->state.cursor_pattern = view->song->num_patterns - 1;
    }
    view->state.cursor_row = 0;
    view->state.scroll_row = 0;

    if (view->engine) {
        tracker_engine_seek(view->engine, view->state.cursor_pattern, 0);
    }

    view->modified = true;
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Deleted pattern %d (%d remaining)",
        delete_index + 1, view->song->num_patterns);
}

/*============================================================================
 * Track Management
 *============================================================================*/

void tracker_view_add_track(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    /* Check track limit */
    if (pattern->num_tracks >= TRACKER_MAX_TRACKS) {
        tracker_view_show_status(view, "Maximum tracks reached (%d)", TRACKER_MAX_TRACKS);
        return;
    }

    /* Generate track name */
    char name[32];
    snprintf(name, sizeof(name), "Track %d", pattern->num_tracks + 1);

    /* Determine channel - use next available or default to 1 */
    uint8_t channel = (pattern->num_tracks < 16) ? (pattern->num_tracks + 1) : 1;

    /* Add track to pattern */
    if (!tracker_pattern_add_track(pattern, name, channel)) {
        tracker_view_show_status(view, "Failed to add track");
        return;
    }

    /* Move cursor to new track */
    view->state.cursor_track = pattern->num_tracks - 1;

    view->modified = true;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Added track %d (ch %d)",
        pattern->num_tracks, channel);
}

void tracker_view_remove_track(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    /* Don't allow removing the last track */
    if (pattern->num_tracks <= 1) {
        tracker_view_show_status(view, "Cannot remove last track");
        return;
    }

    int track_index = view->state.cursor_track;
    int track_num = track_index + 1;

    /* Remove track from pattern */
    if (!tracker_pattern_remove_track(pattern, track_index)) {
        tracker_view_show_status(view, "Failed to remove track");
        return;
    }

    /* Adjust cursor if needed */
    if (view->state.cursor_track >= pattern->num_tracks) {
        view->state.cursor_track = pattern->num_tracks - 1;
    }

    view->modified = true;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Removed track %d (%d remaining)",
        track_num, pattern->num_tracks);
}

void tracker_view_ensure_visible(TrackerView* view) {
    if (!view) return;

    /* Adjust scroll to keep cursor visible */
    if (view->state.cursor_row < view->state.scroll_row) {
        view->state.scroll_row = view->state.cursor_row;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    } else if (view->state.cursor_row >= view->state.scroll_row + view->state.visible_rows) {
        view->state.scroll_row = view->state.cursor_row - view->state.visible_rows + 1;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }

    if (view->state.cursor_track < view->state.scroll_track) {
        view->state.scroll_track = view->state.cursor_track;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    } else if (view->state.cursor_track >= view->state.scroll_track + view->state.visible_tracks) {
        view->state.scroll_track = view->state.cursor_track - view->state.visible_tracks + 1;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }
}

/*============================================================================
 * Mode Switching
 *============================================================================*/

void tracker_view_set_mode(TrackerView* view, TrackerViewMode mode) {
    if (!view) return;

    if (view->state.view_mode != mode) {
        view->state.view_mode = mode;
        tracker_view_invalidate(view);
    }
}

void tracker_view_enter_edit(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerCell* cell = tracker_view_get_cursor_cell(view);
    if (!cell) return;

    /* Initialize edit buffer with current cell content */
    const char* expr = cell->expression ? cell->expression : "";
    int len = (int)strlen(expr);
    free(view->state.edit_buffer);
    /* Allocate the recorded capacity; tracker_view_edit_char grows only past it */
    view->state.edit_buffer = malloc(len + 256);
    if (view->state.edit_buffer) memcpy(view->state.edit_buffer, expr, len + 1);
    view->state.edit_buffer_len = view->state.edit_buffer ? len : 0;
    view->state.edit_buffer_capacity = view->state.edit_buffer ? len + 256 : 0;
    view->state.edit_cursor_pos = 0;  /* Start cursor at beginning of cell */

    view->state.edit_mode = TRACKER_EDIT_MODE_EDIT;
    tracker_view_invalidate_cursor(view);
}

void tracker_view_exit_edit(TrackerView* view, bool confirm) {
    if (!view) return;

    if (confirm) {
        tracker_view_edit_confirm(view);
    } else {
        tracker_view_edit_cancel(view);
    }
}

void tracker_view_enter_command(TrackerView* view) {
    if (!view) return;

    free(view->state.command_buffer);
    view->state.command_buffer = calloc(256, 1);
    view->state.command_buffer_len = 0;
    view->state.command_buffer_capacity = 256;
    view->state.command_cursor_pos = 0;

    view->state.edit_mode = TRACKER_EDIT_MODE_COMMAND;
    tracker_view_invalidate_status(view);
}

/* Parse command and execute */
static void execute_command(TrackerView* view, const char* cmd) {
    if (!view || !cmd || !cmd[0]) return;

    /* Skip leading whitespace */
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    if (!*cmd) return;

    /* Parse command name and arguments */
    char name[64] = {0};
    char arg[256] = {0};
    int i = 0;

    /* Extract command name */
    while (*cmd && *cmd != ' ' && *cmd != '\t' && i < 63) {
        name[i++] = *cmd++;
    }
    name[i] = '\0';

    /* Skip whitespace between name and arg */
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    /* Rest is argument */
    strncpy(arg, cmd, sizeof(arg) - 1);

    /* Execute command */
    if (strcmp(name, "w") == 0 || strcmp(name, "write") == 0) {
        /* :w [filename] - save */
        const char* path = arg[0] ? arg : NULL;
        if (tracker_view_save(view, path)) {
            tracker_view_show_status(view, "Saved: %s",
                view->file_path ? view->file_path : "song.trk");
        } else {
            tracker_view_show_error(view, "Save failed");
        }
    }
    else if (strcmp(name, "q") == 0 || strcmp(name, "quit") == 0) {
        /* :q - quit */
        tracker_view_request_quit(view);
    }
    else if (strcmp(name, "wq") == 0) {
        /* :wq - save and quit */
        if (tracker_view_save(view, NULL)) {
            tracker_view_request_quit(view);
        } else {
            tracker_view_show_error(view, "Save failed");
        }
    }
    else if (strcmp(name, "q!") == 0) {
        /* :q! - force quit */
        view->modified = false;
        tracker_view_request_quit(view);
    }
    else if (strcmp(name, "bpm") == 0) {
        /* :bpm N - set tempo */
        if (arg[0]) {
            int bpm = atoi(arg);
            if (bpm >= 20 && bpm <= 300 && view->song && view->engine) {
                view->song->bpm = bpm;
                tracker_engine_set_bpm(view->engine, bpm);
                view->modified = true;
                tracker_view_show_status(view, "BPM: %d", bpm);
            } else {
                tracker_view_show_error(view, "BPM must be 20-300");
            }
        } else {
            tracker_view_show_status(view, "BPM: %d", view->song ? view->song->bpm : 120);
        }
    }
    else if (strcmp(name, "rows") == 0) {
        /* :rows N - set pattern length */
        if (arg[0]) {
            int rows = atoi(arg);
            if (rows >= 1 && rows <= 256) {
                TrackerPattern* pattern = tracker_view_get_current_pattern(view);
                if (pattern) {
                    tracker_pattern_set_rows(pattern, rows);
                    /* Adjust cursor if needed */
                    if (view->state.cursor_row >= rows) {
                        view->state.cursor_row = rows - 1;
                    }
                    view->modified = true;
                    tracker_view_show_status(view, "Pattern rows: %d", rows);
                    tracker_view_invalidate(view);
                }
            } else {
                tracker_view_show_error(view, "Rows must be 1-256");
            }
        } else {
            TrackerPattern* pattern = tracker_view_get_current_pattern(view);
            tracker_view_show_status(view, "Pattern rows: %d",
                pattern ? pattern->num_rows : 0);
        }
    }
    else if (strcmp(name, "export") == 0) {
        /* :export [filename.mid] - export MIDI */
        char filename[256];
        if (arg[0]) {
            if (snprintf(filename, sizeof(filename), "%s", arg) >= (int)sizeof(filename)) {
                tracker_view_show_error(view, "Filename too long");
                return;
            }
        } else if (view->file_path) {
            if (!derive_midi_path(view->file_path, filename, sizeof(filename))) {
                tracker_view_show_error(view, "File path too long to export");
                return;
            }
        } else {
            snprintf(filename, sizeof(filename), "song.mid");
        }
        if (tracker_export_midi(view, filename)) {
            tracker_view_show_status(view, "Exported: %s", filename);
        } else {
            tracker_view_show_error(view, "Export failed");
        }
    }
    else if (strcmp(name, "import") == 0) {
        /* :import filename.mid - import MIDI file */
        if (!arg[0]) {
            tracker_view_show_error(view, "Usage: :import filename.mid");
        } else {
            TrackerMidiImportOptions opts;
            tracker_midi_import_options_init(&opts);
            opts.rows_per_beat = view->song ? view->song->rows_per_beat : 4;
            opts.ticks_per_row = view->song ? view->song->ticks_per_row : 6;

            TrackerSong* imported = tracker_midi_import(arg, &opts);
            if (imported) {
                /* Replace current song with imported song */
                if (view->engine) {
                    tracker_engine_stop(view->engine);
                    tracker_engine_unload_song(view->engine);
                }

                tracker_song_free(view->song);
                view->song = imported;

                if (view->engine) {
                    tracker_engine_load_song(view->engine, imported);
                }

                /* Reset view state */
                view->state.cursor_row = 0;
                view->state.cursor_track = 0;
                view->state.cursor_pattern = 0;
                view->state.scroll_row = 0;
                view->state.scroll_track = 0;

                /* Update file path */
                free(view->file_path);
                view->file_path = strdup(arg);

                view->modified = true;
                tracker_view_invalidate(view);
                tracker_view_show_status(view, "Imported: %s (%d patterns, %d BPM)",
                    imported->name ? imported->name : arg,
                    imported->num_patterns, imported->bpm);
            } else {
                const char* err = tracker_midi_import_error();
                tracker_view_show_error(view, "Import failed: %s",
                    err ? err : "unknown error");
            }
        }
    }
    else if (strcmp(name, "set") == 0) {
        /* :set option [value] */
        char option[64] = {0};
        char value[64] = {0};
        sscanf(arg, "%63s %63s", option, value);

        if (strcmp(option, "step") == 0) {
            int step = atoi(value);
            if (step >= 0 && step <= 16) {
                view->state.step_size = step;
                tracker_view_show_status(view, "Step: %d", step);
            } else {
                tracker_view_show_error(view, "Step must be 0-16");
            }
        }
        else if (strcmp(option, "octave") == 0 || strcmp(option, "oct") == 0) {
            int oct = atoi(value);
            if (oct >= 0 && oct <= 9) {
                view->state.default_octave = oct;
                tracker_view_show_status(view, "Octave: %d", oct);
            } else {
                tracker_view_show_error(view, "Octave must be 0-9");
            }
        }
        else if (strcmp(option, "follow") == 0) {
            if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0) {
                view->state.follow_playback = true;
                tracker_view_show_status(view, "Follow: ON");
            } else if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0) {
                view->state.follow_playback = false;
                tracker_view_show_status(view, "Follow: OFF");
            } else {
                view->state.follow_playback = !view->state.follow_playback;
                tracker_view_show_status(view, "Follow: %s",
                    view->state.follow_playback ? "ON" : "OFF");
            }
        }
        else if (strcmp(option, "loop") == 0) {
            if (view->engine) {
                if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0) {
                    tracker_engine_set_loop(view->engine, true);
                    tracker_view_show_status(view, "Loop: ON");
                } else if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0) {
                    tracker_engine_set_loop(view->engine, false);
                    tracker_view_show_status(view, "Loop: OFF");
                } else {
                    bool loop = !view->engine->loop_enabled;
                    tracker_engine_set_loop(view->engine, loop);
                    tracker_view_show_status(view, "Loop: %s", loop ? "ON" : "OFF");
                }
            }
        }
        else if (strcmp(option, "swing") == 0) {
            int swing = atoi(value);
            if (swing >= 0 && swing <= 100 && view->engine) {
                view->engine->swing_amount = swing;
                tracker_view_show_status(view, "Swing: %d%%", swing);
            } else {
                tracker_view_show_error(view, "Swing must be 0-100");
            }
        }
        else if (option[0]) {
            tracker_view_show_error(view, "Unknown option: %s", option);
        } else {
            tracker_view_show_status(view, "step=%d octave=%d follow=%s loop=%s",
                view->state.step_size, view->state.default_octave,
                view->state.follow_playback ? "on" : "off",
                view->engine && view->engine->loop_enabled ? "on" : "off");
        }
    }
    else if (strcmp(name, "name") == 0) {
        /* :name [text] - set pattern or song name */
        if (arg[0]) {
            TrackerPattern* pattern = tracker_view_get_current_pattern(view);
            if (pattern) {
                free(pattern->name);
                pattern->name = strdup(arg);
                view->modified = true;
                tracker_view_show_status(view, "Pattern name: %s", arg);
            }
        } else {
            TrackerPattern* pattern = tracker_view_get_current_pattern(view);
            tracker_view_show_status(view, "Pattern name: %s",
                pattern && pattern->name ? pattern->name : "(unnamed)");
        }
    }
    else if (strcmp(name, "help") == 0 || strcmp(name, "h") == 0) {
        /* :help - show help */
        tracker_view_set_mode(view, TRACKER_VIEW_MODE_HELP);
    }
    else if (strcmp(name, "phrase") == 0 || strcmp(name, "ph") == 0) {
        /* :phrase name [expression] - define or show phrase */
        if (!view->song) {
            tracker_view_show_error(view, "No song loaded");
        } else if (!arg[0]) {
            tracker_view_show_error(view, "Usage: :phrase name [expression]");
        } else {
            /* Parse name and optional expression */
            char pname[64] = {0};
            char pexpr[256] = {0};
            char* space = strchr(arg, ' ');

            if (space) {
                /* Has expression - define phrase */
                size_t nlen = (size_t)(space - arg);
                if (nlen > 63) nlen = 63;
                strncpy(pname, arg, nlen);
                strncpy(pexpr, space + 1, sizeof(pexpr) - 1);

                if (tracker_phrase_library_add(&view->song->phrase_library,
                                                pname, pexpr, NULL)) {
                    view->modified = true;
                    tracker_view_show_status(view, "Phrase @%s defined", pname);
                } else {
                    tracker_view_show_error(view, "Failed to add phrase");
                }
            } else {
                /* Just name - show phrase content */
                strncpy(pname, arg, sizeof(pname) - 1);
                TrackerPhraseEntry* entry = tracker_phrase_library_get(
                    &view->song->phrase_library, pname);
                if (entry) {
                    tracker_view_show_status(view, "@%s = %s", pname, entry->expression);
                } else {
                    tracker_view_show_error(view, "Phrase @%s not found", pname);
                }
            }
        }
    }
    else if (strcmp(name, "phrases") == 0) {
        /* :phrases - list all phrases */
        if (!view->song) {
            tracker_view_show_error(view, "No song loaded");
        } else if (view->song->phrase_library.count == 0) {
            tracker_view_show_status(view, "No phrases defined");
        } else {
            /* Show first few phrases in status */
            char buf[256] = "Phrases: ";
            int len = (int)strlen(buf);
            int max_show = 5;
            int count = view->song->phrase_library.count;

            for (int i = 0; i < count && i < max_show; i++) {
                const char* n = view->song->phrase_library.entries[i].name;
                int nlen = (int)strlen(n);
                if (len + nlen + 3 < (int)sizeof(buf)) {
                    if (i > 0) {
                        buf[len++] = ',';
                        buf[len++] = ' ';
                    }
                    buf[len++] = '@';
                    strcpy(buf + len, n);
                    len += nlen;
                }
            }
            if (count > max_show) {
                snprintf(buf + len, sizeof(buf) - len, " ... (%d more)", count - max_show);
            }
            tracker_view_show_status(view, "%s", buf);
        }
    }
    else if (strcmp(name, "delphrase") == 0 || strcmp(name, "dph") == 0) {
        /* :delphrase name - delete a phrase */
        if (!view->song) {
            tracker_view_show_error(view, "No song loaded");
        } else if (!arg[0]) {
            tracker_view_show_error(view, "Usage: :delphrase name");
        } else {
            if (tracker_phrase_library_remove(&view->song->phrase_library, arg)) {
                view->modified = true;
                tracker_view_show_status(view, "Phrase @%s deleted", arg);
            } else {
                tracker_view_show_error(view, "Phrase @%s not found", arg);
            }
        }
    }
    else {
        tracker_view_show_error(view, "Unknown command: %s", name);
    }
}

void tracker_view_exit_command(TrackerView* view, bool execute) {
    if (!view) return;

    if (execute && view->state.command_buffer && view->state.command_buffer_len > 0) {
        execute_command(view, view->state.command_buffer);
    }

    view->state.edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    tracker_view_invalidate_status(view);
}

/*============================================================================
 * Scroll Control
 *============================================================================*/

void tracker_view_scroll_to_row(TrackerView* view, int row) {
    if (!view) return;

    /* Center the row if possible */
    int target = row - view->state.visible_rows / 2;
    if (target < 0) target = 0;

    if (view->state.scroll_row != target) {
        view->state.scroll_row = target;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }
}

void tracker_view_scroll_to_track(TrackerView* view, int track) {
    if (!view) return;

    int target = track - view->state.visible_tracks / 2;
    if (target < 0) target = 0;

    if (view->state.scroll_track != target) {
        view->state.scroll_track = target;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }
}

void tracker_view_scroll(TrackerView* view, int track_delta, int row_delta) {
    if (!view) return;

    view->state.scroll_track += track_delta;
    view->state.scroll_row += row_delta;

    if (view->state.scroll_track < 0) view->state.scroll_track = 0;
    if (view->state.scroll_row < 0) view->state.scroll_row = 0;

    view->dirty_flags |= TRACKER_DIRTY_SCROLL;
}

void tracker_view_set_follow(TrackerView* view, bool follow) {
    if (!view) return;
    view->state.follow_playback = follow;
}

/*============================================================================
 * Undo/Redo (View Integration)
 *============================================================================*/

bool tracker_view_undo(TrackerView* view) {
    if (!view || !view->song) return false;
    bool result = tracker_undo_undo(&view->undo, view, view->song);
    if (result) tracker_view_invalidate(view);
    return result;
}

bool tracker_view_redo(TrackerView* view) {
    if (!view || !view->song) return false;
    bool result = tracker_undo_redo(&view->undo, view, view->song);
    if (result) tracker_view_invalidate(view);
    return result;
}

void tracker_view_begin_undo_group(TrackerView* view, const char* description) {
    if (!view) return;
    tracker_undo_group_begin(&view->undo, description);
}

void tracker_view_end_undo_group(TrackerView* view) {
    if (!view) return;
    tracker_undo_group_end(&view->undo);
}

/*============================================================================
 * Messages and Status
 *============================================================================*/

void tracker_view_show_status(TrackerView* view, const char* format, ...) {
    if (!view) return;

    va_list args;
    va_start(args, format);

    free(view->state.status_message);
    view->state.status_message = malloc(256);
    if (view->state.status_message) {
        vsnprintf(view->state.status_message, 256, format, args);
    }

    va_end(args);

    view->state.status_display_time = 3.0;  /* 3 seconds */
    tracker_view_invalidate_status(view);

    if (view->callbacks.show_message) {
        view->callbacks.show_message(view, view->state.status_message);
    }
}

void tracker_view_show_error(TrackerView* view, const char* format, ...) {
    if (!view) return;

    va_list args;
    va_start(args, format);

    free(view->state.error_message);
    view->state.error_message = malloc(256);
    if (view->state.error_message) {
        vsnprintf(view->state.error_message, 256, format, args);
    }

    va_end(args);

    view->state.error_display_time = 5.0;  /* 5 seconds */
    tracker_view_invalidate_status(view);

    if (view->callbacks.show_error) {
        view->callbacks.show_error(view, view->state.error_message);
    }
}

void tracker_view_clear_messages(TrackerView* view) {
    if (!view) return;

    free(view->state.status_message);
    free(view->state.error_message);
    view->state.status_message = NULL;
    view->state.error_message = NULL;
    view->state.status_display_time = 0;
    view->state.error_display_time = 0;

    tracker_view_invalidate_status(view);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

TrackerCell* tracker_view_get_cursor_cell(TrackerView* view) {
    if (!view || !view->song) return NULL;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return NULL;

    return tracker_pattern_get_cell(pattern,
        view->state.cursor_row, view->state.cursor_track);
}

TrackerPattern* tracker_view_get_current_pattern(TrackerView* view) {
    if (!view || !view->song) return NULL;
    return tracker_song_get_pattern(view->song, view->state.cursor_pattern);
}

bool tracker_view_cursor_valid(TrackerView* view) {
    if (!view || !view->song) return false;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return false;

    return view->state.cursor_track >= 0 &&
           view->state.cursor_track < pattern->num_tracks &&
           view->state.cursor_row >= 0 &&
           view->state.cursor_row < pattern->num_rows;
}

void tracker_view_clamp_cursor(TrackerView* view) {
    if (!view || !view->song) return;

    /* Clamp pattern */
    if (view->state.cursor_pattern < 0) {
        view->state.cursor_pattern = 0;
    }
    if (view->state.cursor_pattern >= view->song->num_patterns) {
        view->state.cursor_pattern = view->song->num_patterns - 1;
    }
    if (view->state.cursor_pattern < 0) {
        view->state.cursor_pattern = 0;
    }

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    /* Clamp track */
    if (view->state.cursor_track < 0) {
        view->state.cursor_track = 0;
    }
    if (view->state.cursor_track >= pattern->num_tracks) {
        view->state.cursor_track = pattern->num_tracks - 1;
    }

    /* Clamp row */
    if (view->state.cursor_row < 0) {
        view->state.cursor_row = 0;
    }
    if (view->state.cursor_row >= pattern->num_rows) {
        view->state.cursor_row = pattern->num_rows - 1;
    }
}

void tracker_view_get_visible_range(TrackerView* view,
                                    int* out_start_track, int* out_end_track,
                                    int* out_start_row, int* out_end_row) {
    if (!view) return;

    if (out_start_track) *out_start_track = view->state.scroll_track;
    if (out_end_track) *out_end_track = view->state.scroll_track + view->state.visible_tracks - 1;
    if (out_start_row) *out_start_row = view->state.scroll_row;
    if (out_end_row) *out_end_row = view->state.scroll_row + view->state.visible_rows - 1;
}

/*============================================================================
 * Main Loop
 *============================================================================*/

void tracker_view_run(TrackerView* view, int frame_rate) {
    if (!view) return;

    double frame_ms = 1000.0 / frame_rate;
    view->quit_requested = false;

    while (!view->quit_requested) {
        /* Poll input with timeout */
        tracker_view_poll_input(view, (int)frame_ms);

        /* Process engine playback (advance timing, trigger events) */
        if (view->engine && tracker_engine_is_playing(view->engine)) {
            tracker_engine_process(view->engine, frame_ms);

            /* Update playback position display */
            int pattern, row, tick;
            tracker_engine_get_position(view->engine, &pattern, &row, &tick);
            tracker_view_update_playback(view, pattern, row);
            view->state.is_playing = true;
        } else {
            view->state.is_playing = false;
        }

        /* Render */
        tracker_view_render(view);

        /* Decay message timers */
        if (view->state.status_display_time > 0) {
            view->state.status_display_time -= frame_ms / 1000.0;
            if (view->state.status_display_time <= 0) {
                free(view->state.status_message);
                view->state.status_message = NULL;
                tracker_view_invalidate_status(view);
            }
        }
        if (view->state.error_display_time > 0) {
            view->state.error_display_time -= frame_ms / 1000.0;
            if (view->state.error_display_time <= 0) {
                free(view->state.error_message);
                view->state.error_message = NULL;
                tracker_view_invalidate_status(view);
            }
        }
    }
}

void tracker_view_request_quit(TrackerView* view) {
    if (!view) return;
    view->quit_requested = true;
}

/*============================================================================
 * File I/O
 *============================================================================*/

void tracker_view_set_file_path(TrackerView* view, const char* path) {
    if (!view) return;
    free(view->file_path);
    view->file_path = path ? str_dup(path) : NULL;
}

const char* tracker_view_get_file_path(TrackerView* view) {
    if (!view) return NULL;
    return view->file_path;
}

bool tracker_view_is_modified(TrackerView* view) {
    if (!view) return false;
    return view->modified;
}

void tracker_view_set_modified(TrackerView* view, bool modified) {
    if (!view) return;
    view->modified = modified;
}

bool tracker_view_save(TrackerView* view, const char* path) {
    if (!view || !view->song) return false;

    /* Use provided path or current file path */
    const char* save_path = path ? path : view->file_path;
    if (!save_path) {
        save_path = "song.trk";  /* Default filename */
    }

    /* Serialize song to JSON */
    char* json = tracker_json_song_to_string(view->song, true);
    if (!json) {
        return false;
    }

    /* Write to file */
    FILE* f = fopen(save_path, "w");
    if (!f) {
        free(json);
        return false;
    }

    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, f);
    fclose(f);
    free(json);

    if (written != len) {
        return false;
    }

    /* Update file path and clear modified flag */
    if (path) {
        tracker_view_set_file_path(view, path);
    } else if (!view->file_path) {
        tracker_view_set_file_path(view, save_path);
    }
    view->modified = false;

    return true;
}

bool tracker_view_load(TrackerView* view, const char* path) {
    if (!view || !path) return false;

    /* Read file contents */
    FILE* f = fopen(path, "r");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {  /* Max 100MB */
        fclose(f);
        return false;
    }

    char* json = malloc(file_size + 1);
    if (!json) {
        fclose(f);
        return false;
    }

    size_t read_size = fread(json, 1, file_size, f);
    fclose(f);

    if ((long)read_size != file_size) {
        free(json);
        return false;
    }
    json[file_size] = '\0';

    /* Parse JSON into song */
    const char* error_msg = NULL;
    TrackerSong* song = tracker_json_parse_song(json, (int)file_size, &error_msg);
    free(json);

    if (!song) {
        /* error_msg contains reason */
        return false;
    }

    /* Replace current song */
    if (view->song) {
        tracker_song_free(view->song);
    }
    view->song = song;

    /* Update engine if attached */
    if (view->engine) {
        tracker_engine_load_song(view->engine, song);
    }

    /* Update file path and reset state */
    tracker_view_set_file_path(view, path);
    view->modified = false;
    view->state.cursor_pattern = 0;
    view->state.cursor_track = 0;
    view->state.cursor_row = 0;

    tracker_view_invalidate(view);

    return true;
}

/*============================================================================
 * MIDI Input Handling
 *============================================================================*/

/* Convert MIDI note number to note name string for the notes plugin */
static const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void tracker_view_handle_midi_note(TrackerView* view, int channel, int note, int velocity, int is_note_on) {
    if (!view || !view->song) return;
    (void)channel;  /* Could be used to select track */

    /* Only handle note-on in record mode */
    if (!view->state.is_recording || !is_note_on || velocity == 0) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    TrackerCell* cell = tracker_pattern_get_cell(pattern,
        view->state.cursor_row, view->state.cursor_track);
    if (!cell) return;

    /* Convert MIDI note to expression (e.g., "C4", "D#5") */
    int octave = (note / 12) - 1;  /* MIDI note 60 = C4 */
    int note_idx = note % 12;
    const char* note_name = note_names[note_idx];

    char expression[32];
    if (strlen(note_name) == 2) {  /* Sharp note */
        snprintf(expression, sizeof(expression), "%c%c%d",
            note_name[0], note_name[1], octave);
    } else {
        snprintf(expression, sizeof(expression), "%c%d", note_name[0], octave);
    }

    /* Record undo */
    TrackerCell old_cell;
    tracker_cell_clone(&old_cell, cell);

    /* Set cell expression */
    tracker_cell_clear(cell);
    cell->type = TRACKER_CELL_EXPRESSION;
    cell->expression = strdup(expression);
    cell->dirty = true;

    /* Record to undo stack */
    tracker_undo_record_cell_edit(&view->undo, view,
        view->state.cursor_pattern, view->state.cursor_track, view->state.cursor_row,
        &old_cell, cell);
    tracker_cell_clear(&old_cell);

    view->modified = true;

    /* Advance cursor by step size */
    if (view->state.step_size > 0) {
        view->state.cursor_row += view->state.step_size;
        if (view->state.cursor_row >= pattern->num_rows) {
            view->state.cursor_row = pattern->num_rows - 1;
        }
    }

    tracker_view_invalidate(view);
}
