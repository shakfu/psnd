/**
 * tracker_plugin_notes.c - Simple note parser plugin implementation
 */

#include "tracker_plugin_notes.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

/*============================================================================
 * Note Name Tables
 *============================================================================*/

/* Note names to semitone offset (C=0, D=2, E=4, F=5, G=7, A=9, B=11) */
static const int note_offsets[7] = { 9, 11, 0, 2, 4, 5, 7 };  /* A, B, C, D, E, F, G */

/* For converting MIDI note to string */
static const char* sharp_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
static const char* flat_names[12] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

/* Transform names */
static const char* transform_names[] = {
    "transpose", "tr",
    "velocity", "vel",
    "octave", "oct",
    "invert", "inv",
    "arpeggio", "arp",
    "delay",
    "ratchet", "rat",
    "humanize", "hum",
    "chance", "prob",
    "reverse", "rev",
    "stutter", "stut",
    NULL
};

/*============================================================================
 * Parsing Helpers
 *============================================================================*/

static void skip_whitespace(const char** str) {
    while (**str && isspace((unsigned char)**str)) {
        (*str)++;
    }
}

static bool parse_int(const char* str, int* out_val, const char** out_end) {
    if (!str || !*str) return false;

    const char* p = str;
    bool negative = false;

    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (!isdigit((unsigned char)*p)) return false;

    /* Saturate rather than overflow: digits are user-authored and a wrapped
       int is undefined behaviour, not a large duration. */
    int val = 0;
    while (isdigit((unsigned char)*p)) {
        int digit = *p - '0';
        val = val > (INT_MAX - digit) / 10 ? INT_MAX : val * 10 + digit;
        p++;
    }

    if (out_val) *out_val = negative ? -val : val;
    if (out_end) *out_end = p;
    return true;
}

/*============================================================================
 * Public Parsing Functions
 *============================================================================*/

bool tracker_notes_parse_note(const char* str, uint8_t* out_note, const char** out_end) {
    if (!str) return false;

    const char* p = str;
    skip_whitespace(&p);

    /* Parse note letter (A-G, case insensitive) */
    char letter = toupper((unsigned char)*p);
    if (letter < 'A' || letter > 'G') return false;
    p++;

    /* Get base semitone offset */
    int semitone = note_offsets[letter - 'A'];

    /* Parse accidentals (# or b, can be multiple) */
    while (*p == '#' || *p == 'b') {
        if (*p == '#') {
            semitone++;
        } else {
            semitone--;
        }
        p++;
    }

    /* Parse octave (default to 4 if not specified) */
    int octave = TRACKER_NOTES_DEFAULT_OCTAVE;
    if (isdigit((unsigned char)*p)) {
        octave = *p - '0';
        p++;
        /* Handle two-digit octave (10) */
        if (isdigit((unsigned char)*p)) {
            octave = octave * 10 + (*p - '0');
            p++;
        }
    }

    /* Calculate MIDI note number */
    /* MIDI: C4 = 60, so octave 4 starts at note 48 (C) + 12 = 60 */
    int midi_note = (octave + 1) * 12 + semitone;

    /* Clamp to valid MIDI range */
    if (midi_note < 0) midi_note = 0;
    if (midi_note > 127) midi_note = 127;

    if (out_note) *out_note = (uint8_t)midi_note;
    if (out_end) *out_end = p;
    return true;
}

bool tracker_notes_parse_velocity(const char* str, uint8_t* out_vel, const char** out_end) {
    if (!str) return false;

    const char* p = str;

    /* Check for @ or v prefix */
    if (*p != '@' && *p != 'v' && *p != 'V') return false;
    p++;

    /* Parse number */
    int vel;
    if (!parse_int(p, &vel, &p)) return false;

    /* Clamp to 0-127 */
    if (vel < 0) vel = 0;
    if (vel > 127) vel = 127;

    if (out_vel) *out_vel = (uint8_t)vel;
    if (out_end) *out_end = p;
    return true;
}

bool tracker_notes_parse_gate(const char* str, int16_t* out_rows, const char** out_end) {
    if (!str) return false;

    const char* p = str;

    /* Check for ~ prefix */
    if (*p != '~') return false;
    p++;

    /* Parse number */
    int rows;
    if (!parse_int(p, &rows, &p)) return false;

    /* Clamp to the output type: 0 is instant, INT16_MAX the longest gate */
    if (rows < 0) rows = 0;
    if (rows > INT16_MAX) rows = INT16_MAX;

    if (out_rows) *out_rows = (int16_t)rows;
    if (out_end) *out_end = p;
    return true;
}

void tracker_notes_to_string(uint8_t note, char* buffer, bool use_sharps) {
    if (!buffer) return;

    int octave = (note / 12) - 1;
    int semitone = note % 12;

    const char* name = use_sharps ? sharp_names[semitone] : flat_names[semitone];

    sprintf(buffer, "%s%d", name, octave);
}

/*============================================================================
 * Expression Evaluation
 *============================================================================*/

/* Maximum phrase recursion depth to prevent infinite loops */
#define MAX_PHRASE_RECURSION 16

/**
 * Parse a complete note expression and return a phrase.
 */
static TrackerPhrase* parse_expression(const char* expr, TrackerContext* ctx) {
    if (!expr) return NULL;

    TrackerPhrase* phrase = tracker_phrase_new(8);
    if (!phrase) return NULL;

    const char* p = expr;
    skip_whitespace(&p);

    /* Check for phrase reference (@name) */
    if (*p == '@') {
        p++;  /* skip @ */

        /* Extract phrase name */
        char name[64];
        int i = 0;
        while (*p && (isalnum((unsigned char)*p) || *p == '_') && i < 63) {
            name[i++] = *p++;
        }
        name[i] = '\0';

        if (i == 0) {
            /* Empty name, return empty phrase */
            return phrase;
        }

        /* Look up phrase in library */
        if (ctx && ctx->lookup_phrase) {
            /* Check recursion depth */
            if (ctx->phrase_recursion_depth >= MAX_PHRASE_RECURSION) {
                /* Too deep - return empty to avoid infinite loop */
                return phrase;
            }

            const char* phrase_expr = NULL;
            const char* phrase_lang = NULL;

            if (ctx->lookup_phrase((TrackerContext*)ctx, name, &phrase_expr, &phrase_lang)) {
                /* Found phrase - recursively evaluate it */
                TrackerContext sub_ctx = *ctx;
                sub_ctx.phrase_recursion_depth = ctx->phrase_recursion_depth + 1;

                tracker_phrase_free(phrase);
                phrase = parse_expression(phrase_expr, &sub_ctx);

                /* Handle remaining expression after phrase reference */
                skip_whitespace(&p);
                if (*p && phrase) {
                    /* There's more content - parse it and merge */
                    TrackerPhrase* extra = parse_expression(p, &sub_ctx);
                    if (extra) {
                        for (int j = 0; j < extra->count; j++) {
                            tracker_phrase_add_event(phrase, &extra->events[j]);
                        }
                        tracker_phrase_free(extra);
                    }
                }

                return phrase;
            }
        }

        /* Phrase not found - return empty phrase */
        return phrase;
    }

    /* Check for rest */
    if (*p == 'r' || *p == '-') {
        /* Rest - return empty phrase */
        return phrase;
    }

    /* Check for note-off */
    if (*p == 'x' || *p == 'X' ||
        (strncasecmp(p, "off", 3) == 0 && !isalnum((unsigned char)p[3]))) {
        /* Note-off - add a note-off event for channel */
        TrackerEvent event = {0};
        event.type = TRACKER_EVENT_NOTE_OFF;
        event.channel = ctx ? ctx->channel : 0;
        event.data1 = 255;  /* special: all notes */
        event.data2 = 0;
        tracker_phrase_add_event(phrase, &event);
        return phrase;
    }

    /* Parse notes (possibly multiple for chords) */
    uint8_t default_velocity = TRACKER_NOTES_DEFAULT_VELOCITY;
    int16_t default_gate = TRACKER_NOTES_DEFAULT_GATE;
    uint8_t channel = ctx ? ctx->channel : 0;

    while (*p) {
        skip_whitespace(&p);
        if (!*p) break;

        /* Skip separators */
        if (*p == ',' || *p == '|') {
            p++;
            continue;
        }

        /* Parse note */
        uint8_t note;
        if (!tracker_notes_parse_note(p, &note, &p)) {
            /* Unknown character, skip it */
            p++;
            continue;
        }

        /* Parse optional velocity */
        uint8_t velocity = default_velocity;
        if (*p == '@' || *p == 'v' || *p == 'V') {
            tracker_notes_parse_velocity(p, &velocity, &p);
        }

        /* Parse optional gate */
        int16_t gate_rows = default_gate;
        if (*p == '~') {
            tracker_notes_parse_gate(p, &gate_rows, &p);
        }

        /* Create note-on event */
        TrackerEvent event = {0};
        event.type = TRACKER_EVENT_NOTE_ON;
        event.channel = channel;
        event.data1 = note;
        event.data2 = velocity;
        event.offset_rows = 0;
        event.offset_ticks = 0;
        event.gate_rows = gate_rows;
        event.gate_ticks = 0;

        tracker_phrase_add_event(phrase, &event);
    }

    return phrase;
}

/*============================================================================
 * Transform Functions
 *============================================================================*/

static TrackerPhrase* transform_transpose(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int semitones = 0;
    if (params) {
        parse_int(params, &semitones, NULL);
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON || e->type == TRACKER_EVENT_NOTE_OFF) {
            int new_note = (int)e->data1 + semitones;
            if (new_note < 0) new_note = 0;
            if (new_note > 127) new_note = 127;
            e->data1 = (uint8_t)new_note;
        }
    }

    return result;
}

static TrackerPhrase* transform_velocity(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int velocity = TRACKER_NOTES_DEFAULT_VELOCITY;
    if (params) {
        parse_int(params, &velocity, NULL);
        if (velocity < 0) velocity = 0;
        if (velocity > 127) velocity = 127;
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON) {
            e->data2 = (uint8_t)velocity;
        }
    }

    return result;
}

static TrackerPhrase* transform_octave(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int octaves = 0;
    if (params) {
        parse_int(params, &octaves, NULL);
    }
    int semitones = octaves * 12;

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON || e->type == TRACKER_EVENT_NOTE_OFF) {
            int new_note = (int)e->data1 + semitones;
            if (new_note < 0) new_note = 0;
            if (new_note > 127) new_note = 127;
            e->data1 = (uint8_t)new_note;
        }
    }

    return result;
}

static TrackerPhrase* transform_invert(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int pivot = 60;  /* C4 */
    if (params) {
        /* Try parsing as note name first */
        uint8_t note;
        if (tracker_notes_parse_note(params, &note, NULL)) {
            pivot = note;
        } else {
            parse_int(params, &pivot, NULL);
        }
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON || e->type == TRACKER_EVENT_NOTE_OFF) {
            int new_note = pivot - ((int)e->data1 - pivot);
            if (new_note < 0) new_note = 0;
            if (new_note > 127) new_note = 127;
            e->data1 = (uint8_t)new_note;
        }
    }

    return result;
}

/**
 * Arpeggio transform - spread chord notes across time.
 * Params: "speed" - ticks between notes (default 4)
 */
static TrackerPhrase* transform_arpeggio(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input || input->count == 0) return NULL;

    int speed = 4;  /* ticks between arp notes */
    if (params) {
        parse_int(params, &speed, NULL);
        if (speed < 1) speed = 1;
        if (speed > 48) speed = 48;
    }

    /* Count note-on events */
    int note_count = 0;
    for (int i = 0; i < input->count; i++) {
        if (input->events[i].type == TRACKER_EVENT_NOTE_ON) {
            note_count++;
        }
    }

    if (note_count <= 1) {
        /* No arpeggiation needed for single notes */
        return tracker_phrase_clone(input);
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    /* Spread note-ons across time */
    int note_idx = 0;
    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON) {
            e->offset_ticks = note_idx * speed;
            note_idx++;
        }
    }

    return result;
}

/**
 * Delay transform - create echo effect.
 * Params: "time,feedback,decay" - delay time in ticks, number of echoes, velocity decay %
 *         e.g., "12,3,70" = 12 tick delay, 3 echoes, 70% velocity each echo
 */
static TrackerPhrase* transform_delay(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input || input->count == 0) return NULL;

    int delay_time = 12;  /* ticks */
    int feedback = 2;     /* number of echoes */
    int decay = 70;       /* velocity decay percentage */

    if (params) {
        const char* p = params;
        parse_int(p, &delay_time, &p);
        if (*p == ',') p++;
        parse_int(p, &feedback, &p);
        if (*p == ',') p++;
        parse_int(p, &decay, NULL);
    }

    if (delay_time < 1) delay_time = 1;
    if (feedback < 0) feedback = 0;
    if (feedback > 8) feedback = 8;
    if (decay < 0) decay = 0;
    if (decay > 100) decay = 100;

    /* Count note events */
    int note_on_count = 0;
    int note_off_count = 0;
    for (int i = 0; i < input->count; i++) {
        if (input->events[i].type == TRACKER_EVENT_NOTE_ON) note_on_count++;
        if (input->events[i].type == TRACKER_EVENT_NOTE_OFF) note_off_count++;
    }

    /* Calculate total events needed */
    int total_note_ons = note_on_count * (feedback + 1);
    int total_note_offs = note_off_count * (feedback + 1);
    int other_events = input->count - note_on_count - note_off_count;
    int total_events = total_note_ons + total_note_offs + other_events;

    TrackerPhrase* result = tracker_phrase_new(total_events);
    if (!result) return NULL;

    /* Copy original events first */
    for (int i = 0; i < input->count; i++) {
        tracker_phrase_add_event(result, &input->events[i]);
    }

    /* Add delayed echoes */
    for (int echo = 1; echo <= feedback; echo++) {
        int echo_delay = delay_time * echo;
        int vel_mult = 100;
        for (int j = 0; j < echo; j++) {
            vel_mult = vel_mult * decay / 100;
        }

        for (int i = 0; i < input->count; i++) {
            const TrackerEvent* orig = &input->events[i];
            if (orig->type == TRACKER_EVENT_NOTE_ON) {
                TrackerEvent e = *orig;
                e.offset_ticks = orig->offset_ticks + echo_delay;
                e.data2 = (uint8_t)(orig->data2 * vel_mult / 100);
                if (e.data2 < 1) e.data2 = 1;
                tracker_phrase_add_event(result, &e);
            } else if (orig->type == TRACKER_EVENT_NOTE_OFF) {
                TrackerEvent e = *orig;
                e.offset_ticks = orig->offset_ticks + echo_delay;
                tracker_phrase_add_event(result, &e);
            }
        }
    }

    return result;
}

/**
 * Ratchet transform - repeat notes rapidly.
 * Params: "count,speed" - number of repeats, ticks between repeats
 *         e.g., "4,3" = 4 repeats, 3 ticks apart
 */
static TrackerPhrase* transform_ratchet(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input || input->count == 0) return NULL;

    int count = 4;    /* number of repeats */
    int speed = 3;    /* ticks between repeats */

    if (params) {
        const char* p = params;
        parse_int(p, &count, &p);
        if (*p == ',') p++;
        parse_int(p, &speed, NULL);
    }

    if (count < 1) count = 1;
    if (count > 16) count = 16;
    if (speed < 1) speed = 1;
    if (speed > 24) speed = 24;

    /* Count note-on events */
    int note_count = 0;
    for (int i = 0; i < input->count; i++) {
        if (input->events[i].type == TRACKER_EVENT_NOTE_ON) {
            note_count++;
        }
    }

    /* Calculate events needed: original + (count-1) * note pairs */
    int extra_events = note_count * (count - 1) * 2;  /* note-on + note-off per repeat */
    int total_events = input->count + extra_events;

    TrackerPhrase* result = tracker_phrase_new(total_events);
    if (!result) return NULL;

    /* Process each event */
    for (int i = 0; i < input->count; i++) {
        const TrackerEvent* orig = &input->events[i];

        if (orig->type == TRACKER_EVENT_NOTE_ON) {
            /* Find matching note-off to get duration */
            int orig_duration = speed;  /* default duration if no note-off found */
            for (int j = i + 1; j < input->count; j++) {
                if (input->events[j].type == TRACKER_EVENT_NOTE_OFF &&
                    input->events[j].data1 == orig->data1) {
                    orig_duration = input->events[j].offset_ticks - orig->offset_ticks;
                    break;
                }
            }

            /* Calculate per-note duration */
            int note_duration = speed - 1;
            if (note_duration < 1) note_duration = 1;

            /* Add ratcheted notes */
            for (int r = 0; r < count; r++) {
                TrackerEvent note_on = *orig;
                note_on.offset_ticks = orig->offset_ticks + r * speed;
                tracker_phrase_add_event(result, &note_on);

                TrackerEvent note_off = {
                    .type = TRACKER_EVENT_NOTE_OFF,
                    .offset_ticks = note_on.offset_ticks + note_duration,
                    .channel = orig->channel,
                    .data1 = orig->data1,
                    .data2 = 0
                };
                tracker_phrase_add_event(result, &note_off);
            }
        } else if (orig->type != TRACKER_EVENT_NOTE_OFF) {
            /* Copy non-note events as-is */
            tracker_phrase_add_event(result, orig);
        }
        /* Skip original note-offs - we generate our own */
    }

    return result;
}

/**
 * Humanize transform - add random variation.
 * Params: "timing,velocity" - max timing offset, max velocity variation
 *         e.g., "2,10" = +/-2 ticks timing, +/-10 velocity
 */
static TrackerPhrase* transform_humanize(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input || input->count == 0) return NULL;

    int timing_var = 2;    /* max timing variation in ticks */
    int velocity_var = 10; /* max velocity variation */

    if (params) {
        const char* p = params;
        parse_int(p, &timing_var, &p);
        if (*p == ',') p++;
        parse_int(p, &velocity_var, NULL);
    }

    if (timing_var < 0) timing_var = 0;
    if (timing_var > 12) timing_var = 12;
    if (velocity_var < 0) velocity_var = 0;
    if (velocity_var > 64) velocity_var = 64;

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    /* Simple pseudo-random based on note data */
    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON) {
            /* Use note and position for pseudo-random variation */
            int seed = e->data1 * 17 + i * 31 + e->offset_ticks * 7;

            if (timing_var > 0) {
                int t_offset = (seed % (timing_var * 2 + 1)) - timing_var;
                int new_tick = e->offset_ticks + t_offset;
                if (new_tick < 0) new_tick = 0;
                e->offset_ticks = new_tick;
            }

            if (velocity_var > 0) {
                int v_offset = ((seed / 3) % (velocity_var * 2 + 1)) - velocity_var;
                int new_vel = (int)e->data2 + v_offset;
                if (new_vel < 1) new_vel = 1;
                if (new_vel > 127) new_vel = 127;
                e->data2 = (uint8_t)new_vel;
            }
        }
    }

    return result;
}

/**
 * Chance transform - probability-based note triggering.
 * Params: "percent" - probability that note plays (0-100)
 *         e.g., "75" = 75% chance each note plays
 */
static TrackerPhrase* transform_chance(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input || input->count == 0) return NULL;

    int percent = 75;  /* probability percentage */

    if (params) {
        parse_int(params, &percent, NULL);
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    TrackerPhrase* result = tracker_phrase_new(input->count);
    if (!result) return NULL;

    /* Determine which notes to keep using pseudo-random */
    for (int i = 0; i < input->count; i++) {
        const TrackerEvent* e = &input->events[i];

        if (e->type == TRACKER_EVENT_NOTE_ON) {
            /* Pseudo-random based on note data */
            int seed = e->data1 * 23 + i * 47 + e->offset_ticks * 13;
            int roll = seed % 100;

            if (roll < percent) {
                /* Note plays - add it and its note-off */
                tracker_phrase_add_event(result, e);
            }
            /* Otherwise skip the note */
        } else if (e->type == TRACKER_EVENT_NOTE_OFF) {
            /* Only add note-off if note-on was added */
            /* Check if we have a matching note-on in result */
            bool found = false;
            for (int j = 0; j < result->count; j++) {
                if (result->events[j].type == TRACKER_EVENT_NOTE_ON &&
                    result->events[j].data1 == e->data1 &&
                    result->events[j].channel == e->channel) {
                    found = true;
                    break;
                }
            }
            if (found) {
                tracker_phrase_add_event(result, e);
            }
        } else {
            /* Copy other events */
            tracker_phrase_add_event(result, e);
        }
    }

    return result;
}

/**
 * Reverse transform - reverse note order.
 * Params: none
 */
static TrackerPhrase* transform_reverse(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    (void)params;
    if (!input || input->count == 0) return NULL;

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    /* Find time range */
    int min_tick = 0x7FFFFFFF;
    int max_tick = 0;
    for (int i = 0; i < result->count; i++) {
        int t = result->events[i].offset_ticks;
        if (t < min_tick) min_tick = t;
        if (t > max_tick) max_tick = t;
    }

    /* Reverse timing */
    for (int i = 0; i < result->count; i++) {
        result->events[i].offset_ticks = max_tick - (result->events[i].offset_ticks - min_tick);
    }

    return result;
}

/**
 * Stutter transform - repeat phrase with variations.
 * Params: "count,decay" - number of repeats, velocity decay %
 *         e.g., "3,80" = 3 repeats, 80% velocity each time
 */
static TrackerPhrase* transform_stutter(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input || input->count == 0) return NULL;

    int count = 2;    /* number of total plays */
    int decay = 80;   /* velocity decay percentage */

    if (params) {
        const char* p = params;
        parse_int(p, &count, &p);
        if (*p == ',') p++;
        parse_int(p, &decay, NULL);
    }

    if (count < 1) count = 1;
    if (count > 8) count = 8;
    if (decay < 0) decay = 0;
    if (decay > 100) decay = 100;

    /* Find phrase duration */
    int max_tick = 0;
    for (int i = 0; i < input->count; i++) {
        if (input->events[i].offset_ticks > max_tick) {
            max_tick = input->events[i].offset_ticks;
        }
    }
    int phrase_len = max_tick + 1;

    /* Create result with repeated events */
    int total_events = input->count * count;
    TrackerPhrase* result = tracker_phrase_new(total_events);
    if (!result) return NULL;

    for (int rep = 0; rep < count; rep++) {
        int time_offset = rep * phrase_len;
        int vel_mult = 100;
        for (int j = 0; j < rep; j++) {
            vel_mult = vel_mult * decay / 100;
        }

        for (int i = 0; i < input->count; i++) {
            TrackerEvent e = input->events[i];
            e.offset_ticks += time_offset;
            if (e.type == TRACKER_EVENT_NOTE_ON) {
                e.data2 = (uint8_t)(e.data2 * vel_mult / 100);
                if (e.data2 < 1) e.data2 = 1;
            }
            tracker_phrase_add_event(result, &e);
        }
    }

    return result;
}

/*============================================================================
 * Plugin Callbacks
 *============================================================================*/

static bool plugin_init(void) {
    return true;
}

static void plugin_cleanup(void) {
    /* Nothing to clean up */
}

static void plugin_reset(void) {
    /* Nothing to reset */
}

static const char* validation_error = NULL;

static bool plugin_validate(const char* expression, const char** error_msg, int* error_pos) {
    if (!expression || !*expression) {
        validation_error = "Empty expression";
        if (error_msg) *error_msg = validation_error;
        if (error_pos) *error_pos = 0;
        return false;
    }

    const char* p = expression;
    skip_whitespace(&p);

    /* Check for rest or note-off */
    if (*p == 'r' || *p == '-' || *p == 'x' || *p == 'X') {
        return true;
    }
    if (strncasecmp(p, "off", 3) == 0) {
        return true;
    }

    /* Try to parse at least one note */
    uint8_t note;
    if (!tracker_notes_parse_note(p, &note, NULL)) {
        validation_error = "Invalid note: expected A-G";
        if (error_msg) *error_msg = validation_error;
        if (error_pos) *error_pos = (int)(p - expression);
        return false;
    }

    return true;
}

static bool plugin_is_generator(const char* expression) {
    (void)expression;
    /* Notes are not generators - they produce fixed output */
    return false;
}

static TrackerPhrase* plugin_evaluate(const char* expression, TrackerContext* ctx) {
    return parse_expression(expression, ctx);
}

static TrackerTransformFn plugin_get_transform(const char* fx_name) {
    if (!fx_name) return NULL;

    if (strcmp(fx_name, "transpose") == 0 || strcmp(fx_name, "tr") == 0) {
        return transform_transpose;
    }
    if (strcmp(fx_name, "velocity") == 0 || strcmp(fx_name, "vel") == 0) {
        return transform_velocity;
    }
    if (strcmp(fx_name, "octave") == 0 || strcmp(fx_name, "oct") == 0) {
        return transform_octave;
    }
    if (strcmp(fx_name, "invert") == 0 || strcmp(fx_name, "inv") == 0) {
        return transform_invert;
    }
    if (strcmp(fx_name, "arpeggio") == 0 || strcmp(fx_name, "arp") == 0) {
        return transform_arpeggio;
    }
    if (strcmp(fx_name, "delay") == 0) {
        return transform_delay;
    }
    if (strcmp(fx_name, "ratchet") == 0 || strcmp(fx_name, "rat") == 0) {
        return transform_ratchet;
    }
    if (strcmp(fx_name, "humanize") == 0 || strcmp(fx_name, "hum") == 0) {
        return transform_humanize;
    }
    if (strcmp(fx_name, "chance") == 0 || strcmp(fx_name, "prob") == 0) {
        return transform_chance;
    }
    if (strcmp(fx_name, "reverse") == 0 || strcmp(fx_name, "rev") == 0) {
        return transform_reverse;
    }
    if (strcmp(fx_name, "stutter") == 0 || strcmp(fx_name, "stut") == 0) {
        return transform_stutter;
    }

    return NULL;
}

static const char** plugin_list_transforms(int* count) {
    if (count) {
        /* Count non-NULL entries */
        int n = 0;
        for (int i = 0; transform_names[i] != NULL; i++) {
            n++;
        }
        *count = n;
    }
    return transform_names;
}

static const char* plugin_describe_transform(const char* fx_name) {
    if (!fx_name) return NULL;

    if (strcmp(fx_name, "transpose") == 0 || strcmp(fx_name, "tr") == 0) {
        return "Transpose notes by semitones";
    }
    if (strcmp(fx_name, "velocity") == 0 || strcmp(fx_name, "vel") == 0) {
        return "Set note velocity (0-127)";
    }
    if (strcmp(fx_name, "octave") == 0 || strcmp(fx_name, "oct") == 0) {
        return "Shift notes by octaves";
    }
    if (strcmp(fx_name, "invert") == 0 || strcmp(fx_name, "inv") == 0) {
        return "Invert notes around a pivot";
    }
    if (strcmp(fx_name, "arpeggio") == 0 || strcmp(fx_name, "arp") == 0) {
        return "Spread chord notes across time";
    }
    if (strcmp(fx_name, "delay") == 0) {
        return "Create echo/delay effect";
    }
    if (strcmp(fx_name, "ratchet") == 0 || strcmp(fx_name, "rat") == 0) {
        return "Repeat notes rapidly";
    }
    if (strcmp(fx_name, "humanize") == 0 || strcmp(fx_name, "hum") == 0) {
        return "Add random timing/velocity variation";
    }
    if (strcmp(fx_name, "chance") == 0 || strcmp(fx_name, "prob") == 0) {
        return "Probability-based note triggering";
    }
    if (strcmp(fx_name, "reverse") == 0 || strcmp(fx_name, "rev") == 0) {
        return "Reverse note order";
    }
    if (strcmp(fx_name, "stutter") == 0 || strcmp(fx_name, "stut") == 0) {
        return "Repeat phrase with velocity decay";
    }

    return NULL;
}

static const char* plugin_get_transform_params_doc(const char* fx_name) {
    if (!fx_name) return NULL;

    if (strcmp(fx_name, "transpose") == 0 || strcmp(fx_name, "tr") == 0) {
        return "semitones: integer (positive = up, negative = down)";
    }
    if (strcmp(fx_name, "velocity") == 0 || strcmp(fx_name, "vel") == 0) {
        return "velocity: 0-127";
    }
    if (strcmp(fx_name, "octave") == 0 || strcmp(fx_name, "oct") == 0) {
        return "octaves: integer (positive = up, negative = down)";
    }
    if (strcmp(fx_name, "invert") == 0 || strcmp(fx_name, "inv") == 0) {
        return "pivot: note name (e.g., C4) or MIDI number (default: 60)";
    }
    if (strcmp(fx_name, "arpeggio") == 0 || strcmp(fx_name, "arp") == 0) {
        return "speed: ticks between notes (default: 4)";
    }
    if (strcmp(fx_name, "delay") == 0) {
        return "time,feedback,decay: delay ticks, echo count, velocity % (e.g., 12,3,70)";
    }
    if (strcmp(fx_name, "ratchet") == 0 || strcmp(fx_name, "rat") == 0) {
        return "count,speed: repeats, ticks between (e.g., 4,3)";
    }
    if (strcmp(fx_name, "humanize") == 0 || strcmp(fx_name, "hum") == 0) {
        return "timing,velocity: max variation (e.g., 2,10)";
    }
    if (strcmp(fx_name, "chance") == 0 || strcmp(fx_name, "prob") == 0) {
        return "percent: probability 0-100 (default: 75)";
    }
    if (strcmp(fx_name, "reverse") == 0 || strcmp(fx_name, "rev") == 0) {
        return "(no parameters)";
    }
    if (strcmp(fx_name, "stutter") == 0 || strcmp(fx_name, "stut") == 0) {
        return "count,decay: repeats, velocity % (e.g., 3,80)";
    }

    return NULL;
}

/*============================================================================
 * Plugin Definition
 *============================================================================*/

static TrackerPlugin notes_plugin = {
    /* Identity */
    .name = "Notes",
    .language_id = "notes",
    .version = "1.0",
    .description = "Simple note notation parser (C4, D#5, Bb3)",

    /* Capabilities & Priority */
    .capabilities = TRACKER_CAP_EVALUATE | TRACKER_CAP_VALIDATION | TRACKER_CAP_TRANSFORMS,
    .priority = 0,

    /* Lifecycle */
    .init = plugin_init,
    .cleanup = plugin_cleanup,
    .reset = plugin_reset,

    /* Expression Handling */
    .validate = plugin_validate,
    .is_generator = plugin_is_generator,
    .evaluate = plugin_evaluate,
    .compile = NULL,
    .evaluate_compiled = NULL,
    .free_compiled = NULL,

    /* Transform Handling */
    .get_transform = plugin_get_transform,
    .list_transforms = plugin_list_transforms,
    .describe_transform = plugin_describe_transform,
    .get_transform_params_doc = plugin_get_transform_params_doc,
    .validate_transform_params = NULL,
    .parse_transform_params = NULL,
    .free_transform_params = NULL,
    .get_last_error = NULL,
    .get_last_error_message = NULL,
    .clear_error = NULL,
};

const TrackerPlugin* tracker_plugin_notes_get(void) {
    return &notes_plugin;
}

bool tracker_plugin_notes_register(void) {
    return tracker_plugin_register(&notes_plugin);
}
