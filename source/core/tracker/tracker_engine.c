/**
 * tracker_engine.c - Playback engine implementation
 */

#include "tracker_engine.h"
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

static void update_timing_cache(TrackerEngine* engine) {
    if (!engine) return;

    /* ms per beat = 60000 / BPM */
    double ms_per_beat = 60000.0 / engine->bpm;

    /* ms per row = ms_per_beat / rows_per_beat */
    engine->row_duration_ms = ms_per_beat / engine->rows_per_beat;

    /* ms per tick = ms_per_row / ticks_per_row */
    engine->tick_duration_ms = engine->row_duration_ms / engine->ticks_per_row;
}

/*============================================================================
 * Event Queue - Internal Operations
 *============================================================================*/

static TrackerPendingEvent* event_pool_alloc(TrackerEngine* engine) {
    if (!engine) return NULL;

    /* Try free list first */
    if (engine->free_list) {
        TrackerPendingEvent* ev = engine->free_list;
        engine->free_list = ev->next;
        memset(ev, 0, sizeof(TrackerPendingEvent));
        return ev;
    }

    /* Fall back to pool if available */
    int max_events = engine->config.max_pending_events ?
        engine->config.max_pending_events : TRACKER_ENGINE_MAX_PENDING_EVENTS;

    if (engine->pending_count >= max_events) {
        return NULL;  /* queue full */
    }

    /* Allocate new event */
    return calloc(1, sizeof(TrackerPendingEvent));
}

static void event_pool_free(TrackerEngine* engine, TrackerPendingEvent* ev) {
    if (!engine || !ev) return;

    /* Free extended params if present */
    if (ev->event.params) {
        tracker_event_params_free(ev->event.params);
        ev->event.params = NULL;
    }

    /* Add to free list */
    ev->next = engine->free_list;
    engine->free_list = ev;
}

static void event_queue_insert(TrackerEngine* engine, TrackerPendingEvent* ev) {
    if (!engine || !ev) return;

    /* Insert sorted by due_tick (ascending) */
    TrackerPendingEvent** pp = &engine->pending_head;

    while (*pp && (*pp)->due_tick <= ev->due_tick) {
        pp = &(*pp)->next;
    }

    ev->next = *pp;
    *pp = ev;
    engine->pending_count++;
    engine->events_scheduled++;
}

static TrackerPendingEvent* event_queue_pop(TrackerEngine* engine) {
    if (!engine || !engine->pending_head) return NULL;

    TrackerPendingEvent* ev = engine->pending_head;
    engine->pending_head = ev->next;
    ev->next = NULL;
    engine->pending_count--;

    return ev;
}

/*============================================================================
 * Active Note Tracking
 *============================================================================*/

static TrackerActiveNote* find_active_note_slot(TrackerEngine* engine) {
    if (!engine) return NULL;

    int max_notes = engine->config.max_active_notes ?
        engine->config.max_active_notes : TRACKER_ENGINE_MAX_ACTIVE_NOTES;

    for (int i = 0; i < max_notes; i++) {
        if (!engine->active_notes[i].active) {
            return &engine->active_notes[i];
        }
    }

    return NULL;  /* no free slots */
}

static TrackerActiveNote* find_active_note(TrackerEngine* engine,
                                           uint8_t channel, uint8_t note) {
    if (!engine) return NULL;

    int max_notes = engine->config.max_active_notes ?
        engine->config.max_active_notes : TRACKER_ENGINE_MAX_ACTIVE_NOTES;

    for (int i = 0; i < max_notes; i++) {
        TrackerActiveNote* an = &engine->active_notes[i];
        if (an->active && an->channel == channel && an->note == note) {
            return an;
        }
    }

    return NULL;
}

static void register_active_note(TrackerEngine* engine,
                                 uint8_t channel, uint8_t note,
                                 int track_index, int phrase_id,
                                 int64_t started_tick, int64_t off_tick) {
    TrackerActiveNote* slot = find_active_note_slot(engine);
    if (!slot) return;  /* no room */

    slot->active = true;
    slot->channel = channel;
    slot->note = note;
    slot->track_index = track_index;
    slot->phrase_id = phrase_id;
    slot->started_tick = started_tick;
    slot->scheduled_off_tick = off_tick;
    engine->active_note_count++;
}

static void unregister_active_note(TrackerEngine* engine,
                                   uint8_t channel, uint8_t note) {
    TrackerActiveNote* an = find_active_note(engine, channel, note);
    if (an) {
        an->active = false;
        engine->active_note_count--;
    }
}

/*============================================================================
 * MIDI Output Dispatch
 *============================================================================*/

static void dispatch_note_on(TrackerEngine* engine, uint8_t channel,
                             uint8_t note, uint8_t velocity) {
    if (!engine) return;

    if (engine->config.output.note_on) {
        engine->config.output.note_on(engine->config.output.user_data,
                                      channel, note, velocity);
    }

    engine->notes_on++;
}

static void dispatch_note_off(TrackerEngine* engine, uint8_t channel,
                              uint8_t note, uint8_t velocity) {
    if (!engine) return;

    if (engine->config.output.note_off) {
        engine->config.output.note_off(engine->config.output.user_data,
                                       channel, note, velocity);
    }

    engine->notes_off++;
}

static void dispatch_cc(TrackerEngine* engine, uint8_t channel,
                        uint8_t cc, uint8_t value) {
    if (!engine) return;

    if (engine->config.output.cc) {
        engine->config.output.cc(engine->config.output.user_data,
                                 channel, cc, value);
    }
}

static void dispatch_program_change(TrackerEngine* engine, uint8_t channel,
                                    uint8_t program) {
    if (!engine) return;

    if (engine->config.output.program_change) {
        engine->config.output.program_change(engine->config.output.user_data,
                                             channel, program);
    }
}

static void dispatch_pitch_bend(TrackerEngine* engine, uint8_t channel,
                                int16_t value) {
    if (!engine) return;

    if (engine->config.output.pitch_bend) {
        engine->config.output.pitch_bend(engine->config.output.user_data,
                                         channel, value);
    }
}

static void dispatch_aftertouch(TrackerEngine* engine, uint8_t channel,
                                uint8_t pressure) {
    if (!engine) return;

    if (engine->config.output.aftertouch) {
        engine->config.output.aftertouch(engine->config.output.user_data,
                                         channel, pressure);
    }
}

static void dispatch_poly_aftertouch(TrackerEngine* engine, uint8_t channel,
                                     uint8_t note, uint8_t pressure) {
    if (!engine) return;

    if (engine->config.output.poly_aftertouch) {
        engine->config.output.poly_aftertouch(engine->config.output.user_data,
                                              channel, note, pressure);
    }
}

static void dispatch_all_notes_off(TrackerEngine* engine, uint8_t channel) {
    if (!engine) return;

    if (engine->config.output.all_notes_off) {
        engine->config.output.all_notes_off(engine->config.output.user_data, channel);
    }
}

/*============================================================================
 * Event Firing
 *============================================================================*/

static void fire_event(TrackerEngine* engine, TrackerPendingEvent* ev) {
    if (!engine || !ev) return;

    TrackerEvent* event = &ev->event;

    switch (event->type) {
        case TRACKER_EVENT_NOTE_ON: {
            /* Check if this note is already active - auto-cut if so */
            TrackerActiveNote* existing = find_active_note(engine,
                event->channel, event->data1);
            if (existing) {
                dispatch_note_off(engine, event->channel, event->data1, 0);
                unregister_active_note(engine, event->channel, event->data1);
            }

            /* Apply track volume scaling */
            uint8_t velocity = event->data2;
            if (engine->song && ev->source.track_index >= 0) {
                TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                    ev->source.pattern_index);
                if (pattern && ev->source.track_index < pattern->num_tracks) {
                    TrackerTrack* track = &pattern->tracks[ev->source.track_index];
                    /* Scale velocity by track volume (0-127, default 100) */
                    velocity = (uint8_t)((velocity * track->volume) / 100);
                    if (velocity > 127) velocity = 127;
                }
            }

            dispatch_note_on(engine, event->channel, event->data1, velocity);

            /* Calculate note-off tick if gate is specified */
            int64_t off_tick = -1;
            if (event->gate_rows > 0 || event->gate_ticks > 0) {
                off_tick = ev->due_tick +
                    (int64_t)event->gate_rows * engine->ticks_per_row +
                    event->gate_ticks;

                /* Schedule note-off */
                TrackerPendingEvent* off_ev = event_pool_alloc(engine);
                if (off_ev) {
                    off_ev->due_tick = off_tick;
                    off_ev->event.type = TRACKER_EVENT_NOTE_OFF;
                    off_ev->event.channel = event->channel;
                    off_ev->event.data1 = event->data1;
                    off_ev->event.data2 = 0;
                    off_ev->source = ev->source;
                    event_queue_insert(engine, off_ev);
                }
            }

            register_active_note(engine, event->channel, event->data1,
                                ev->source.track_index, ev->source.phrase_id,
                                ev->due_tick, off_tick);
            break;
        }

        case TRACKER_EVENT_NOTE_OFF:
            dispatch_note_off(engine, event->channel, event->data1, event->data2);
            unregister_active_note(engine, event->channel, event->data1);
            break;

        case TRACKER_EVENT_CC:
            dispatch_cc(engine, event->channel, event->data1, event->data2);
            break;

        case TRACKER_EVENT_PROGRAM_CHANGE:
            dispatch_program_change(engine, event->channel, event->data1);
            break;

        case TRACKER_EVENT_PITCH_BEND: {
            int16_t bend = (int16_t)((event->data2 << 7) | event->data1) - 8192;
            dispatch_pitch_bend(engine, event->channel, bend);
            break;
        }

        case TRACKER_EVENT_AFTERTOUCH:
            dispatch_aftertouch(engine, event->channel, event->data1);
            break;

        case TRACKER_EVENT_POLY_AFTERTOUCH:
            dispatch_poly_aftertouch(engine, event->channel,
                                     event->data1, event->data2);
            break;
    }

    engine->events_fired++;
}

/*============================================================================
 * Row Triggering
 *============================================================================*/

static bool is_track_audible(TrackerEngine* engine, int track_index) {
    if (!engine || !engine->song) return false;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern || track_index < 0 || track_index >= pattern->num_tracks) {
        return false;
    }

    TrackerTrack* track = &pattern->tracks[track_index];

    if (track->muted) return false;

    /* If any track is soloed, only soloed tracks are audible */
    if (tracker_engine_has_solo(engine) && !track->solo) {
        return false;
    }

    return true;
}

static void trigger_cell(TrackerEngine* engine, int pattern_index,
                         int track_index, int row_index) {
    if (!engine || !engine->song) return;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song, pattern_index);
    if (!pattern) return;

    TrackerCell* cell = tracker_pattern_get_cell(pattern, row_index, track_index);
    if (!cell || cell->type == TRACKER_CELL_EMPTY ||
        cell->type == TRACKER_CELL_CONTINUATION) {
        return;
    }

    if (!is_track_audible(engine, track_index)) return;

    TrackerTrack* track = &pattern->tracks[track_index];

    /* Handle spillover for previous phrase on this track */
    if (engine->song->spillover_mode == TRACKER_SPILLOVER_TRUNCATE) {
        tracker_engine_cancel_track(engine, track_index);
        tracker_engine_track_notes_off(engine, track_index);
    }

    /* Handle explicit note-off */
    if (cell->type == TRACKER_CELL_NOTE_OFF) {
        tracker_engine_track_notes_off(engine, track_index);
        return;
    }

    /* Compile cell if needed */
    if (!cell->compiled || cell->dirty) {
        const char* error = NULL;
        cell->compiled = tracker_compile_cell(cell,
            engine->song->default_language_id, &error);
        cell->dirty = false;

        if (!cell->compiled && error) {
            /* Store error for reporting */
            free(engine->last_error);
            engine->last_error = str_dup(error);
            engine->error_pattern = pattern_index;
            engine->error_track = track_index;
            engine->error_row = row_index;
            return;
        }
    }

    if (!cell->compiled) return;

    /* Setup context */
    TrackerContext ctx;
    tracker_context_from_song(&ctx, engine->song, pattern_index,
                              row_index, track_index);
    ctx.absolute_tick = engine->current_tick;
    ctx.absolute_time_ms = engine->current_time_ms;
    ctx.random_seed = engine->current_tick;  /* deterministic per position */
    ctx.engine_data = engine;

    /* Evaluate cell */
    TrackerPhrase* phrase = tracker_evaluate_cell(cell->compiled, &ctx);
    if (!phrase) return;

    /* Apply cell FX chain */
    if (cell->compiled->fx_chain.count > 0) {
        phrase = tracker_apply_fx_chain(&cell->compiled->fx_chain, phrase, &ctx);
        if (!phrase) return;
    }

    /* Apply track FX chain */
    if (track->compiled_fx && track->compiled_fx->count > 0) {
        phrase = tracker_apply_fx_chain(track->compiled_fx, phrase, &ctx);
        if (!phrase) return;
    }

    /* Apply master FX chain */
    if (engine->song->compiled_master_fx &&
        engine->song->compiled_master_fx->count > 0) {
        phrase = tracker_apply_fx_chain(engine->song->compiled_master_fx, phrase, &ctx);
        if (!phrase) return;
    }

    /* Schedule events from phrase */
    int phrase_id = engine->next_phrase_id++;

    TrackerEventSource source = {
        .pattern_index = pattern_index,
        .track_index = track_index,
        .row_index = row_index,
        .phrase_id = phrase_id
    };

    for (int i = 0; i < phrase->count; i++) {
        TrackerEvent* ev = &phrase->events[i];

        /* Calculate due tick */
        int64_t due_tick = engine->current_tick +
            (int64_t)ev->offset_rows * engine->ticks_per_row +
            ev->offset_ticks;

        /* Use track's default channel if event channel is 0 */
        uint8_t channel = ev->channel ? ev->channel : track->default_channel;

        TrackerPendingEvent* pending = event_pool_alloc(engine);
        if (!pending) {
            engine->underruns++;
            break;
        }

        pending->due_tick = due_tick;
        pending->event = *ev;
        pending->event.channel = channel;
        pending->source = source;

        /* Clone params if present */
        if (ev->params) {
            pending->event.params = tracker_event_params_clone(ev->params);
        }

        event_queue_insert(engine, pending);
    }

    tracker_phrase_free(phrase);
}

static void trigger_row(TrackerEngine* engine, int row) {
    if (!engine || !engine->song) return;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern) return;

    for (int t = 0; t < pattern->num_tracks; t++) {
        trigger_cell(engine, engine->current_pattern, t, row);
    }
}

/*============================================================================
 * Configuration
 *============================================================================*/

void tracker_engine_config_init(TrackerEngineConfig* config) {
    if (!config) return;
    memset(config, 0, sizeof(TrackerEngineConfig));

    config->sync_mode = TRACKER_SYNC_INTERNAL;
    config->default_play_mode = TRACKER_PLAY_MODE_PATTERN;
    config->send_midi_clock = false;
    config->auto_recompile = true;
    config->chase_notes = false;
    config->send_all_notes_off_on_stop = true;
    config->lookahead_ms = 10;
    config->max_pending_events = TRACKER_ENGINE_MAX_PENDING_EVENTS;
    config->max_active_notes = TRACKER_ENGINE_MAX_ACTIVE_NOTES;
    config->recent_events_rows = TRACKER_ENGINE_RECENT_ROWS;
}

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

TrackerEngine* tracker_engine_new(void) {
    TrackerEngineConfig config;
    tracker_engine_config_init(&config);
    return tracker_engine_new_with_config(&config);
}

TrackerEngine* tracker_engine_new_with_config(const TrackerEngineConfig* config) {
    TrackerEngine* engine = calloc(1, sizeof(TrackerEngine));
    if (!engine) return NULL;

    if (config) {
        engine->config = *config;
    } else {
        tracker_engine_config_init(&engine->config);
    }

    /* Allocate active notes array */
    int max_notes = engine->config.max_active_notes ?
        engine->config.max_active_notes : TRACKER_ENGINE_MAX_ACTIVE_NOTES;
    engine->active_notes = calloc(max_notes, sizeof(TrackerActiveNote));
    if (!engine->active_notes) {
        free(engine);
        return NULL;
    }

    /* Set defaults */
    engine->state = TRACKER_ENGINE_STOPPED;
    engine->play_mode = engine->config.default_play_mode;
    engine->bpm = TRACKER_DEFAULT_BPM;
    engine->rows_per_beat = TRACKER_DEFAULT_RPB;
    engine->ticks_per_row = TRACKER_DEFAULT_TPR;
    engine->loop_enabled = true;
    engine->loop_start_row = -1;
    engine->loop_end_row = -1;

    update_timing_cache(engine);

    return engine;
}

void tracker_engine_free(TrackerEngine* engine) {
    if (!engine) return;

    tracker_engine_stop(engine);
    tracker_engine_unload_song(engine);

    /* Free event pool */
    TrackerPendingEvent* ev = engine->pending_head;
    while (ev) {
        TrackerPendingEvent* next = ev->next;
        if (ev->event.params) tracker_event_params_free(ev->event.params);
        free(ev);
        ev = next;
    }

    ev = engine->free_list;
    while (ev) {
        TrackerPendingEvent* next = ev->next;
        free(ev);
        ev = next;
    }

    free(engine->active_notes);
    free(engine->last_error);
    free(engine);
}

void tracker_engine_reset(TrackerEngine* engine) {
    if (!engine) return;

    tracker_engine_stop(engine);
    tracker_engine_cancel_all(engine);
    tracker_engine_all_notes_off(engine);

    engine->current_pattern = 0;
    engine->current_row = 0;
    engine->current_tick = 0;
    engine->current_time_ms = 0.0;
    engine->loop_count = 0;
    engine->next_phrase_id = 0;

    tracker_engine_clear_error(engine);
    tracker_engine_reset_stats(engine);
}

/*============================================================================
 * Song Management
 *============================================================================*/

bool tracker_engine_load_song(TrackerEngine* engine, TrackerSong* song) {
    if (!engine) return false;

    tracker_engine_unload_song(engine);

    engine->song = song;

    if (song) {
        engine->bpm = song->bpm;
        engine->rows_per_beat = song->rows_per_beat;
        engine->ticks_per_row = song->ticks_per_row;
        update_timing_cache(engine);

        if (engine->config.auto_recompile) {
            return tracker_engine_compile_all(engine);
        }
    }

    return true;
}

void tracker_engine_unload_song(TrackerEngine* engine) {
    if (!engine) return;

    tracker_engine_stop(engine);
    tracker_engine_cancel_all(engine);
    tracker_engine_all_notes_off(engine);

    /* Free compiled caches; cells and tracks own their compiled forms */
    free(engine->compiled_patterns);
    engine->compiled_patterns = NULL;

    free(engine->compiled_track_fx);
    engine->compiled_track_fx = NULL;

    free(engine->compiled_master_fx);
    engine->compiled_master_fx = NULL;

    free(engine->pattern_compiled);
    engine->pattern_compiled = NULL;

    /* Free recent events */
    if (engine->recent_by_track) {
        /* TODO: free phrase arrays */
        free(engine->recent_by_track);
        engine->recent_by_track = NULL;
    }

    engine->song = NULL;
    engine->current_pattern = 0;
    engine->current_row = 0;
    engine->current_tick = 0;
    engine->current_time_ms = 0.0;
}

bool tracker_engine_compile_pattern(TrackerEngine* engine, int pattern_index) {
    if (!engine || !engine->song) return false;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song, pattern_index);
    if (!pattern) return false;

    const char* error = NULL;

    /* Compile all cells */
    for (int t = 0; t < pattern->num_tracks; t++) {
        TrackerTrack* track = &pattern->tracks[t];

        /* Compile track FX chain */
        if (track->fx_chain.count > 0 && !track->compiled_fx) {
            track->compiled_fx = tracker_compile_fx_chain(&track->fx_chain,
                engine->song->default_language_id, &error);
            if (!track->compiled_fx && error) {
                free(engine->last_error);
                engine->last_error = str_dup(error);
                return false;
            }
        }

        for (int r = 0; r < pattern->num_rows; r++) {
            TrackerCell* cell = &track->cells[r];

            if (cell->type == TRACKER_CELL_EXPRESSION && !cell->compiled) {
                cell->compiled = tracker_compile_cell(cell,
                    engine->song->default_language_id, &error);
                cell->dirty = false;

                if (!cell->compiled && error) {
                    free(engine->last_error);
                    engine->last_error = str_dup(error);
                    engine->error_pattern = pattern_index;
                    engine->error_track = t;
                    engine->error_row = r;
                    /* Continue compiling other cells */
                }
            }
        }
    }

    return true;
}

bool tracker_engine_compile_all(TrackerEngine* engine) {
    if (!engine || !engine->song) return false;

    /* Compile master FX */
    if (engine->song->master_fx.count > 0 && !engine->song->compiled_master_fx) {
        const char* error = NULL;
        engine->song->compiled_master_fx = tracker_compile_fx_chain(
            &engine->song->master_fx,
            engine->song->default_language_id, &error);
    }

    /* Compile all patterns */
    bool success = true;
    for (int i = 0; i < engine->song->num_patterns; i++) {
        if (!tracker_engine_compile_pattern(engine, i)) {
            success = false;
        }
    }

    return success;
}

void tracker_engine_mark_dirty(TrackerEngine* engine, int pattern, int track, int row) {
    if (!engine || !engine->song) return;

    TrackerPattern* p = tracker_song_get_pattern(engine->song, pattern);
    if (!p) return;

    TrackerCell* cell = tracker_pattern_get_cell(p, row, track);
    if (cell) {
        cell->dirty = true;
    }
}

/*============================================================================
 * Transport Controls
 *============================================================================*/

bool tracker_engine_play(TrackerEngine* engine) {
    if (!engine || !engine->song) return false;

    if (engine->state == TRACKER_ENGINE_PLAYING) return true;

    /* When starting from stopped state, initialize MIDI channels */
    if (engine->state == TRACKER_ENGINE_STOPPED) {
        /* Send program changes for each track's channel to initialize instruments.
         * This is required for synthesizers like TSF that need a preset selected
         * before notes will sound. */
        TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                            engine->current_pattern);
        if (pattern) {
            for (int t = 0; t < pattern->num_tracks; t++) {
                TrackerTrack* track = &pattern->tracks[t];
                uint8_t channel = track->default_channel;
                /* Use program 0 (piano) as default, or bank 128 for drums on ch 10 */
                uint8_t program = (channel == 9 || channel == 10) ? 0 : 0;
                dispatch_program_change(engine, channel, program);

                /* Send pan CC (CC 10) - convert from -64..+63 to 0..127 */
                uint8_t pan_cc = (uint8_t)(track->pan + 64);
                if (pan_cc > 127) pan_cc = 127;
                dispatch_cc(engine, channel, 10, pan_cc);
            }
        }
    }

    /* Send MIDI start if configured */
    if (engine->config.output.start && engine->state == TRACKER_ENGINE_STOPPED) {
        engine->config.output.start(engine->config.output.user_data);
    } else if (engine->config.output.cont && engine->state == TRACKER_ENGINE_PAUSED) {
        engine->config.output.cont(engine->config.output.user_data);
    }

    engine->state = TRACKER_ENGINE_PLAYING;

    /* Trigger current row if starting fresh */
    if (engine->current_tick == 0 ||
        (engine->current_tick % engine->ticks_per_row) == 0) {
        trigger_row(engine, engine->current_row);
    }

    return true;
}

void tracker_engine_stop(TrackerEngine* engine) {
    if (!engine) return;

    if (engine->state == TRACKER_ENGINE_STOPPED) return;

    engine->state = TRACKER_ENGINE_STOPPED;

    /* Send all notes off */
    if (engine->config.send_all_notes_off_on_stop) {
        tracker_engine_all_notes_off(engine);
    }

    /* Send MIDI stop */
    if (engine->config.output.stop) {
        engine->config.output.stop(engine->config.output.user_data);
    }

    /* Reset position */
    engine->current_pattern = 0;
    engine->current_row = 0;
    engine->current_tick = 0;
    engine->current_time_ms = 0.0;
    engine->loop_count = 0;

    /* Clear pending events */
    tracker_engine_cancel_all(engine);
}

void tracker_engine_pause(TrackerEngine* engine) {
    if (!engine) return;

    if (engine->state != TRACKER_ENGINE_PLAYING) return;

    engine->state = TRACKER_ENGINE_PAUSED;

    /* Optionally send all notes off on pause */
    if (engine->config.send_all_notes_off_on_stop) {
        tracker_engine_all_notes_off(engine);
    }
}

void tracker_engine_toggle(TrackerEngine* engine) {
    if (!engine) return;

    if (engine->state == TRACKER_ENGINE_PLAYING) {
        tracker_engine_pause(engine);
    } else {
        tracker_engine_play(engine);
    }
}

void tracker_engine_seek(TrackerEngine* engine, int pattern, int row) {
    if (!engine || !engine->song) return;

    bool was_playing = (engine->state == TRACKER_ENGINE_PLAYING);

    /* Cancel pending events and notes */
    tracker_engine_cancel_all(engine);
    tracker_engine_all_notes_off(engine);

    /* Validate pattern */
    if (engine->play_mode == TRACKER_PLAY_MODE_PATTERN) {
        /* In pattern mode, pattern parameter is the pattern index */
        if (pattern < 0 || pattern >= engine->song->num_patterns) {
            pattern = 0;
        }
    } else {
        /* In song mode, pattern is sequence position */
        if (pattern < 0 || pattern >= engine->song->sequence_length) {
            pattern = 0;
        }
    }

    engine->current_pattern = pattern;

    /* Get actual pattern for row validation */
    int actual_pattern_idx = pattern;
    if (engine->play_mode == TRACKER_PLAY_MODE_SONG &&
        pattern < engine->song->sequence_length) {
        actual_pattern_idx = engine->song->sequence[pattern].pattern_index;
    }

    TrackerPattern* p = tracker_song_get_pattern(engine->song, actual_pattern_idx);
    if (p) {
        if (row < 0) row = 0;
        if (row >= p->num_rows) row = p->num_rows - 1;
    } else {
        row = 0;
    }

    engine->current_row = row;
    engine->current_tick = (int64_t)row * engine->ticks_per_row;
    engine->current_time_ms = tracker_tick_to_ms(engine->current_tick,
                                                  engine->bpm,
                                                  engine->rows_per_beat,
                                                  engine->ticks_per_row);

    /* Trigger row if playing */
    if (was_playing) {
        trigger_row(engine, row);
    }
}

void tracker_engine_next_pattern(TrackerEngine* engine) {
    if (!engine || !engine->song) return;

    int next = engine->current_pattern + 1;

    if (engine->play_mode == TRACKER_PLAY_MODE_PATTERN) {
        if (next >= engine->song->num_patterns) {
            next = 0;
        }
    } else {
        if (next >= engine->song->sequence_length) {
            next = 0;
        }
    }

    tracker_engine_seek(engine, next, 0);
}

void tracker_engine_prev_pattern(TrackerEngine* engine) {
    if (!engine || !engine->song) return;

    int prev = engine->current_pattern - 1;

    if (prev < 0) {
        if (engine->play_mode == TRACKER_PLAY_MODE_PATTERN) {
            prev = engine->song->num_patterns - 1;
        } else {
            prev = engine->song->sequence_length - 1;
        }
    }

    tracker_engine_seek(engine, prev, 0);
}

/*============================================================================
 * Timing and Advance
 *============================================================================*/

int tracker_engine_process(TrackerEngine* engine, double delta_ms) {
    if (!engine || engine->state != TRACKER_ENGINE_PLAYING) return 0;

    double target_ms = engine->current_time_ms + delta_ms;
    return tracker_engine_process_until(engine, target_ms);
}

int tracker_engine_process_until(TrackerEngine* engine, double target_ms) {
    if (!engine || !engine->song) return 0;
    if (engine->state != TRACKER_ENGINE_PLAYING) return 0;

    int events_fired = 0;

    /* Convert target time to ticks */
    int64_t target_tick = (int64_t)(target_ms / engine->tick_duration_ms);

    /* Get current pattern info */
    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern) return 0;

    /* Process tick by tick */
    while (engine->current_tick < target_tick) {
        int64_t next_tick = engine->current_tick + 1;

        /* Check for row boundary */
        int current_row = (int)(engine->current_tick / engine->ticks_per_row);
        int next_row = (int)(next_tick / engine->ticks_per_row);

        if (next_row > current_row) {
            /* Row boundary crossed */
            engine->current_row = next_row;

            /* Check for pattern end */
            int loop_end = (engine->loop_end_row >= 0) ?
                engine->loop_end_row : pattern->num_rows;

            if (engine->current_row >= loop_end) {
                /* Handle loop or advance */
                if (engine->loop_enabled) {
                    int loop_start = (engine->loop_start_row >= 0) ?
                        engine->loop_start_row : 0;
                    engine->current_row = loop_start;
                    engine->loop_count++;

                    /* Reset tick and time for loop start */
                    engine->current_tick = (int64_t)engine->current_row * engine->ticks_per_row;
                    engine->current_time_ms = engine->current_tick * engine->tick_duration_ms;

                    /* Trigger the first row of the loop, then return.
                     * We must return here because target_tick is based on
                     * cumulative time, but current_tick just reset to 0.
                     * The next call to process() will continue normally. */
                    trigger_row(engine, engine->current_row);
                    return events_fired;
                } else if (engine->play_mode == TRACKER_PLAY_MODE_SONG) {
                    /* Advance to next pattern in sequence */
                    engine->current_pattern++;
                    if (engine->current_pattern >= engine->song->sequence_length) {
                        /* End of song */
                        tracker_engine_stop(engine);
                        return events_fired;
                    }
                    engine->current_row = 0;
                    engine->current_tick = 0;
                    next_tick = 0;
                    engine->current_time_ms = 0.0;

                    /* Update pattern reference */
                    int pat_idx = engine->song->sequence[engine->current_pattern].pattern_index;
                    pattern = tracker_song_get_pattern(engine->song, pat_idx);
                    if (!pattern) {
                        tracker_engine_stop(engine);
                        return events_fired;
                    }
                } else {
                    /* Pattern mode, no loop - stop */
                    tracker_engine_stop(engine);
                    return events_fired;
                }
            }

            /* Trigger new row */
            trigger_row(engine, engine->current_row);
        }

        /* Fire all events due at or before next_tick */
        while (engine->pending_head && engine->pending_head->due_tick <= next_tick) {
            TrackerPendingEvent* ev = event_queue_pop(engine);
            fire_event(engine, ev);
            event_pool_free(engine, ev);
            events_fired++;
        }

        engine->current_tick = next_tick;
    }

    engine->current_time_ms = target_ms;

    return events_fired;
}

void tracker_engine_step_row(TrackerEngine* engine) {
    if (!engine || !engine->song) return;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern) return;

    /* Trigger current row */
    trigger_row(engine, engine->current_row);

    /* Fire all events for this row immediately */
    int64_t row_end_tick = (int64_t)(engine->current_row + 1) * engine->ticks_per_row;

    while (engine->pending_head && engine->pending_head->due_tick < row_end_tick) {
        TrackerPendingEvent* ev = event_queue_pop(engine);
        fire_event(engine, ev);
        event_pool_free(engine, ev);
    }

    /* Advance to next row */
    engine->current_row++;
    if (engine->current_row >= pattern->num_rows) {
        engine->current_row = 0;
    }

    engine->current_tick = (int64_t)engine->current_row * engine->ticks_per_row;
    engine->current_time_ms = tracker_tick_to_ms(engine->current_tick,
                                                  engine->bpm,
                                                  engine->rows_per_beat,
                                                  engine->ticks_per_row);
}

void tracker_engine_step_tick(TrackerEngine* engine) {
    if (!engine || !engine->song) return;

    double tick_ms = engine->tick_duration_ms;
    tracker_engine_process(engine, tick_ms);
}

void tracker_engine_trigger_cell(TrackerEngine* engine, int pattern,
                                 int track, int row) {
    if (!engine || !engine->song) return;

    /* Save current state */
    int64_t saved_tick = engine->current_tick;
    double saved_time = engine->current_time_ms;

    /* Set temporary position */
    engine->current_tick = (int64_t)row * engine->ticks_per_row;
    engine->current_time_ms = tracker_tick_to_ms(engine->current_tick,
                                                  engine->bpm,
                                                  engine->rows_per_beat,
                                                  engine->ticks_per_row);

    /* Trigger the cell */
    trigger_cell(engine, pattern, track, row);

    /* Fire immediate events */
    while (engine->pending_head &&
           engine->pending_head->due_tick <= engine->current_tick) {
        TrackerPendingEvent* ev = event_queue_pop(engine);
        fire_event(engine, ev);
        event_pool_free(engine, ev);
    }

    /* Restore state */
    engine->current_tick = saved_tick;
    engine->current_time_ms = saved_time;
}

bool tracker_engine_eval_immediate(TrackerEngine* engine,
                                   const char* expression,
                                   const char* language_id,
                                   uint8_t channel) {
    if (!engine || !expression) return false;

    /* Create temporary cell */
    TrackerCell cell;
    tracker_cell_init(&cell);
    tracker_cell_set_expression(&cell, expression, language_id);

    /* Compile */
    const char* error = NULL;
    CompiledCell* compiled = tracker_compile_cell(&cell,
        engine->song ? engine->song->default_language_id : NULL, &error);

    if (!compiled) {
        tracker_cell_clear(&cell);
        return false;
    }

    /* Setup context */
    TrackerContext ctx;
    tracker_context_init(&ctx);
    if (engine->song) {
        ctx.bpm = engine->bpm;
        ctx.rows_per_beat = engine->rows_per_beat;
        ctx.ticks_per_row = engine->ticks_per_row;
    }
    ctx.channel = channel;
    ctx.engine_data = engine;

    /* Evaluate */
    TrackerPhrase* phrase = tracker_evaluate_cell(compiled, &ctx);

    if (phrase) {
        /* Fire events immediately */
        for (int i = 0; i < phrase->count; i++) {
            TrackerEvent* ev = &phrase->events[i];

            TrackerPendingEvent pending = {0};
            pending.event = *ev;
            pending.event.channel = ev->channel ? ev->channel : channel;

            fire_event(engine, &pending);
        }

        tracker_phrase_free(phrase);
    }

    tracker_compiled_cell_free(compiled);
    tracker_cell_clear(&cell);

    return true;
}

/*============================================================================
 * Playback Settings
 *============================================================================*/

void tracker_engine_set_play_mode(TrackerEngine* engine, TrackerPlayMode mode) {
    if (!engine) return;
    engine->play_mode = mode;
}

void tracker_engine_set_bpm(TrackerEngine* engine, int bpm) {
    if (!engine || bpm < 1) return;
    engine->bpm = bpm;
    update_timing_cache(engine);
}

void tracker_engine_reset_bpm(TrackerEngine* engine) {
    if (!engine || !engine->song) return;
    engine->bpm = engine->song->bpm;
    update_timing_cache(engine);
}

void tracker_engine_set_loop(TrackerEngine* engine, bool enabled) {
    if (!engine) return;
    engine->loop_enabled = enabled;
}

void tracker_engine_set_loop_points(TrackerEngine* engine, int start_row, int end_row) {
    if (!engine) return;
    engine->loop_start_row = start_row;
    engine->loop_end_row = end_row;
}

void tracker_engine_set_sync_mode(TrackerEngine* engine, TrackerSyncMode mode) {
    if (!engine) return;
    engine->config.sync_mode = mode;
}

/*============================================================================
 * Track Control
 *============================================================================*/

void tracker_engine_mute_track(TrackerEngine* engine, int track, bool muted) {
    if (!engine || !engine->song) return;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern || track < 0 || track >= pattern->num_tracks) return;

    pattern->tracks[track].muted = muted;

    /* If muting, stop notes on this track */
    if (muted) {
        tracker_engine_track_notes_off(engine, track);
    }
}

void tracker_engine_solo_track(TrackerEngine* engine, int track, bool solo) {
    if (!engine || !engine->song) return;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern || track < 0 || track >= pattern->num_tracks) return;

    pattern->tracks[track].solo = solo;

    /* If soloing, stop notes on non-soloed tracks */
    if (solo) {
        for (int t = 0; t < pattern->num_tracks; t++) {
            if (!pattern->tracks[t].solo) {
                tracker_engine_track_notes_off(engine, t);
            }
        }
    }
}

bool tracker_engine_has_solo(TrackerEngine* engine) {
    if (!engine || !engine->song) return false;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern) return false;

    for (int t = 0; t < pattern->num_tracks; t++) {
        if (pattern->tracks[t].solo) return true;
    }

    return false;
}

void tracker_engine_clear_solo(TrackerEngine* engine) {
    if (!engine || !engine->song) return;

    TrackerPattern* pattern = tracker_song_get_pattern(engine->song,
                                                        engine->current_pattern);
    if (!pattern) return;

    for (int t = 0; t < pattern->num_tracks; t++) {
        pattern->tracks[t].solo = false;
    }
}

/*============================================================================
 * Event Queue Management
 *============================================================================*/

bool tracker_engine_schedule_event(TrackerEngine* engine,
                                   int64_t due_tick,
                                   const TrackerEvent* event,
                                   const TrackerEventSource* source) {
    if (!engine || !event) return false;

    TrackerPendingEvent* ev = event_pool_alloc(engine);
    if (!ev) return false;

    ev->due_tick = due_tick;
    ev->event = *event;
    if (source) ev->source = *source;

    if (event->params) {
        ev->event.params = tracker_event_params_clone(event->params);
    }

    event_queue_insert(engine, ev);
    return true;
}

void tracker_engine_cancel_phrase(TrackerEngine* engine, int phrase_id) {
    if (!engine) return;

    TrackerPendingEvent** pp = &engine->pending_head;

    while (*pp) {
        if ((*pp)->source.phrase_id == phrase_id) {
            TrackerPendingEvent* ev = *pp;
            *pp = ev->next;
            event_pool_free(engine, ev);
            engine->pending_count--;
        } else {
            pp = &(*pp)->next;
        }
    }
}

void tracker_engine_cancel_track(TrackerEngine* engine, int track_index) {
    if (!engine) return;

    TrackerPendingEvent** pp = &engine->pending_head;

    while (*pp) {
        if ((*pp)->source.track_index == track_index) {
            TrackerPendingEvent* ev = *pp;
            *pp = ev->next;
            event_pool_free(engine, ev);
            engine->pending_count--;
        } else {
            pp = &(*pp)->next;
        }
    }
}

void tracker_engine_cancel_all(TrackerEngine* engine) {
    if (!engine) return;

    while (engine->pending_head) {
        TrackerPendingEvent* ev = engine->pending_head;
        engine->pending_head = ev->next;
        event_pool_free(engine, ev);
    }

    engine->pending_count = 0;
}

int tracker_engine_pending_count(TrackerEngine* engine) {
    return engine ? engine->pending_count : 0;
}

/*============================================================================
 * Active Note Management
 *============================================================================*/

void tracker_engine_all_notes_off(TrackerEngine* engine) {
    if (!engine) return;

    int max_notes = engine->config.max_active_notes ?
        engine->config.max_active_notes : TRACKER_ENGINE_MAX_ACTIVE_NOTES;

    for (int i = 0; i < max_notes; i++) {
        TrackerActiveNote* an = &engine->active_notes[i];
        if (an->active) {
            dispatch_note_off(engine, an->channel, an->note, 0);
            an->active = false;
        }
    }

    engine->active_note_count = 0;

    /* Also send CC 123 (all notes off) on all channels */
    for (int ch = 0; ch < 16; ch++) {
        dispatch_all_notes_off(engine, ch);
    }
}

void tracker_engine_channel_notes_off(TrackerEngine* engine, uint8_t channel) {
    if (!engine) return;

    int max_notes = engine->config.max_active_notes ?
        engine->config.max_active_notes : TRACKER_ENGINE_MAX_ACTIVE_NOTES;

    for (int i = 0; i < max_notes; i++) {
        TrackerActiveNote* an = &engine->active_notes[i];
        if (an->active && an->channel == channel) {
            dispatch_note_off(engine, an->channel, an->note, 0);
            an->active = false;
            engine->active_note_count--;
        }
    }

    dispatch_all_notes_off(engine, channel);
}

void tracker_engine_track_notes_off(TrackerEngine* engine, int track_index) {
    if (!engine) return;

    int max_notes = engine->config.max_active_notes ?
        engine->config.max_active_notes : TRACKER_ENGINE_MAX_ACTIVE_NOTES;

    for (int i = 0; i < max_notes; i++) {
        TrackerActiveNote* an = &engine->active_notes[i];
        if (an->active && an->track_index == track_index) {
            dispatch_note_off(engine, an->channel, an->note, 0);
            an->active = false;
            engine->active_note_count--;
        }
    }
}

int tracker_engine_active_note_count(TrackerEngine* engine) {
    return engine ? engine->active_note_count : 0;
}

/*============================================================================
 * Query Functions
 *============================================================================*/

void tracker_engine_get_position(TrackerEngine* engine,
                                 int* out_pattern, int* out_row, int* out_tick) {
    if (!engine) return;
    if (out_pattern) *out_pattern = engine->current_pattern;
    if (out_row) *out_row = engine->current_row;
    if (out_tick) *out_tick = (int)(engine->current_tick % engine->ticks_per_row);
}

double tracker_engine_get_time_ms(TrackerEngine* engine) {
    return engine ? engine->current_time_ms : 0.0;
}

int tracker_engine_get_bpm(TrackerEngine* engine) {
    return engine ? engine->bpm : 0;
}

const char* tracker_engine_get_error(TrackerEngine* engine) {
    return engine ? engine->last_error : NULL;
}

void tracker_engine_get_error_location(TrackerEngine* engine,
                                       int* out_pattern, int* out_track, int* out_row) {
    if (!engine) return;
    if (out_pattern) *out_pattern = engine->error_pattern;
    if (out_track) *out_track = engine->error_track;
    if (out_row) *out_row = engine->error_row;
}

void tracker_engine_clear_error(TrackerEngine* engine) {
    if (!engine) return;
    free(engine->last_error);
    engine->last_error = NULL;
    engine->error_pattern = -1;
    engine->error_track = -1;
    engine->error_row = -1;
}

/*============================================================================
 * Output Configuration
 *============================================================================*/

void tracker_engine_set_output(TrackerEngine* engine, const TrackerOutput* output) {
    if (!engine || !output) return;
    engine->config.output = *output;
}

const TrackerOutput* tracker_engine_get_output(TrackerEngine* engine) {
    return engine ? &engine->config.output : NULL;
}

/*============================================================================
 * Statistics
 *============================================================================*/

void tracker_engine_get_stats(TrackerEngine* engine, TrackerEngineStats* stats) {
    if (!engine || !stats) return;

    stats->events_fired = engine->events_fired;
    stats->events_scheduled = engine->events_scheduled;
    stats->notes_on = engine->notes_on;
    stats->notes_off = engine->notes_off;
    stats->underruns = engine->underruns;
    stats->pending_events = engine->pending_count;
    stats->active_notes = engine->active_note_count;
    stats->cpu_usage = 0.0;  /* TODO: implement CPU usage tracking */
}

void tracker_engine_reset_stats(TrackerEngine* engine) {
    if (!engine) return;
    engine->events_fired = 0;
    engine->events_scheduled = 0;
    engine->notes_on = 0;
    engine->notes_off = 0;
    engine->underruns = 0;
}

/*============================================================================
 * External Sync
 *============================================================================*/

void tracker_engine_external_clock(TrackerEngine* engine) {
    if (!engine) return;
    if (engine->config.sync_mode != TRACKER_SYNC_EXTERNAL_MIDI) return;

    /* MIDI clock is 24 PPQ - advance accordingly */
    /* This is a simplified implementation */
    double tick_ms = engine->tick_duration_ms;
    tracker_engine_process(engine, tick_ms);
}

void tracker_engine_external_start(TrackerEngine* engine) {
    if (!engine) return;
    if (engine->config.sync_mode != TRACKER_SYNC_EXTERNAL_MIDI) return;

    tracker_engine_seek(engine, 0, 0);
    engine->state = TRACKER_ENGINE_PLAYING;
}

void tracker_engine_external_stop(TrackerEngine* engine) {
    if (!engine) return;
    if (engine->config.sync_mode != TRACKER_SYNC_EXTERNAL_MIDI) return;

    tracker_engine_stop(engine);
}

void tracker_engine_external_continue(TrackerEngine* engine) {
    if (!engine) return;
    if (engine->config.sync_mode != TRACKER_SYNC_EXTERNAL_MIDI) return;

    engine->state = TRACKER_ENGINE_PLAYING;
}

void tracker_engine_external_position(TrackerEngine* engine, int position) {
    if (!engine) return;
    if (engine->config.sync_mode != TRACKER_SYNC_EXTERNAL_MIDI) return;

    /* SPP is in MIDI beats (1 beat = 6 MIDI clocks) */
    /* Convert to rows */
    int row = (position * 6) / (engine->rows_per_beat * engine->ticks_per_row);
    tracker_engine_seek(engine, engine->current_pattern, row);
}

void tracker_engine_link_update(TrackerEngine* engine,
                                double beat, double bpm, bool is_playing) {
    if (!engine) return;
    if (engine->config.sync_mode != TRACKER_SYNC_EXTERNAL_LINK) return;

    /* Update tempo if changed */
    if ((int)bpm != engine->bpm) {
        engine->bpm = (int)bpm;
        update_timing_cache(engine);
    }

    /* Update position */
    double rows_per_beat_f = (double)engine->rows_per_beat;
    int row = (int)(beat * rows_per_beat_f) %
        (engine->song ? engine->song->patterns[engine->current_pattern]->num_rows : 64);

    if (row != engine->current_row) {
        engine->current_row = row;
        engine->current_tick = (int64_t)row * engine->ticks_per_row;

        if (is_playing) {
            trigger_row(engine, row);
        }
    }

    /* Update play state */
    if (is_playing && engine->state != TRACKER_ENGINE_PLAYING) {
        engine->state = TRACKER_ENGINE_PLAYING;
    } else if (!is_playing && engine->state == TRACKER_ENGINE_PLAYING) {
        tracker_engine_pause(engine);
    }
}
