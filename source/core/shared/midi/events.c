/**
 * @file events.c
 * @brief Shared MIDI event buffer implementation.
 */

#include "events.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define INITIAL_CAPACITY 1024
#define GROWTH_FACTOR 2

/* ============================================================================
 * Global State
 * ============================================================================ */

static struct {
    SharedMidiEvent *events;
    int count;
    int capacity;
    int ticks_per_quarter;
    int initialized;
} g_buffer = {0};

/* ============================================================================
 * Buffer Management
 * ============================================================================ */

int shared_midi_events_init(int ticks_per_quarter) {
    if (g_buffer.initialized) {
        /* Already initialized, just clear and update ticks */
        shared_midi_events_clear();
        g_buffer.ticks_per_quarter = ticks_per_quarter;
        return 0;
    }

    g_buffer.events = malloc(INITIAL_CAPACITY * sizeof(SharedMidiEvent));
    if (!g_buffer.events) {
        return -1;
    }

    g_buffer.count = 0;
    g_buffer.capacity = INITIAL_CAPACITY;
    g_buffer.ticks_per_quarter = ticks_per_quarter;
    g_buffer.initialized = 1;

    return 0;
}

void shared_midi_events_cleanup(void) {
    if (g_buffer.events) {
        free(g_buffer.events);
        g_buffer.events = NULL;
    }
    g_buffer.count = 0;
    g_buffer.capacity = 0;
    g_buffer.ticks_per_quarter = 0;
    g_buffer.initialized = 0;
}

void shared_midi_events_clear(void) {
    g_buffer.count = 0;
}

int shared_midi_events_is_initialized(void) {
    return g_buffer.initialized;
}

/* ============================================================================
 * Internal: Grow Buffer
 * ============================================================================ */

static int grow_buffer(void) {
    int new_capacity = g_buffer.capacity * GROWTH_FACTOR;
    SharedMidiEvent *new_events = realloc(g_buffer.events,
                                          new_capacity * sizeof(SharedMidiEvent));
    if (!new_events) {
        return -1;
    }
    g_buffer.events = new_events;
    g_buffer.capacity = new_capacity;
    return 0;
}

/* ============================================================================
 * Event Recording
 * ============================================================================ */

int shared_midi_events_add(const SharedMidiEvent *event) {
    if (!g_buffer.initialized || !event) {
        return -1;
    }

    if (g_buffer.count >= g_buffer.capacity) {
        if (grow_buffer() != 0) {
            return -1;
        }
    }

    g_buffer.events[g_buffer.count++] = *event;
    return 0;
}

int shared_midi_events_note_on(int tick, int channel, int pitch, int velocity) {
    SharedMidiEvent evt = {
        .tick = tick,
        .type = SHARED_MIDI_NOTE_ON,
        .channel = channel,
        .data1 = pitch,
        .data2 = velocity
    };
    return shared_midi_events_add(&evt);
}

int shared_midi_events_note_off(int tick, int channel, int pitch) {
    SharedMidiEvent evt = {
        .tick = tick,
        .type = SHARED_MIDI_NOTE_OFF,
        .channel = channel,
        .data1 = pitch,
        .data2 = 0
    };
    return shared_midi_events_add(&evt);
}

int shared_midi_events_program(int tick, int channel, int program) {
    SharedMidiEvent evt = {
        .tick = tick,
        .type = SHARED_MIDI_PROGRAM,
        .channel = channel,
        .data1 = program,
        .data2 = 0
    };
    return shared_midi_events_add(&evt);
}

int shared_midi_events_cc(int tick, int channel, int cc, int value) {
    SharedMidiEvent evt = {
        .tick = tick,
        .type = SHARED_MIDI_CC,
        .channel = channel,
        .data1 = cc,
        .data2 = value
    };
    return shared_midi_events_add(&evt);
}

int shared_midi_events_tempo(int tick, int bpm) {
    SharedMidiEvent evt = {
        .tick = tick,
        .type = SHARED_MIDI_TEMPO,
        .channel = 0,
        .data1 = bpm,
        .data2 = 0
    };
    return shared_midi_events_add(&evt);
}

/* ============================================================================
 * Event Access
 * ============================================================================ */

const SharedMidiEvent* shared_midi_events_get(int *count) {
    if (count) {
        *count = g_buffer.initialized ? g_buffer.count : 0;
    }
    return g_buffer.initialized ? g_buffer.events : NULL;
}

int shared_midi_events_count(void) {
    return g_buffer.initialized ? g_buffer.count : 0;
}

int shared_midi_events_ticks_per_quarter(void) {
    return g_buffer.initialized ? g_buffer.ticks_per_quarter : 0;
}

/* ============================================================================
 * Sorting (stable sort by tick)
 * ============================================================================ */

/* Order within a tick: note-offs, then tempo, program and controller
 * changes, then note-ons. A note ending where the same pitch starts again
 * must end before the new one starts, and a note must find its channel's
 * settings in place. qsort is not stable, so ties are broken explicitly. */
static int type_rank(SharedMidiEventType type) {
    switch (type) {
        case SHARED_MIDI_NOTE_OFF: return 0;
        case SHARED_MIDI_TEMPO:    return 1;
        case SHARED_MIDI_PROGRAM:  return 2;
        case SHARED_MIDI_CC:       return 3;
        default:                   return 4;  /* note-on */
    }
}

static int compare_events(const void *a, const void *b) {
    const SharedMidiEvent *ea = (const SharedMidiEvent *)a;
    const SharedMidiEvent *eb = (const SharedMidiEvent *)b;
    if (ea->tick != eb->tick) return ea->tick - eb->tick;
    int ra = type_rank(ea->type), rb = type_rank(eb->type);
    if (ra != rb) return ra - rb;
    if (ea->channel != eb->channel) return ea->channel - eb->channel;
    return ea->data1 - eb->data1;
}

void shared_midi_events_sort(void) {
    if (g_buffer.initialized && g_buffer.count > 1) {
        qsort(g_buffer.events, g_buffer.count, sizeof(SharedMidiEvent), compare_events);
    }
}
