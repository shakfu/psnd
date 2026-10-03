/**
 * @file shared_async.h
 * @brief Shared asynchronous MIDI playback service.
 *
 * Language-agnostic async playback using libuv. Supports multiple concurrent
 * playback slots for polyphonic layering from separate REPL commands.
 *
 * Usage:
 *   1. Call shared_async_init() once at startup
 *   2. Create events with shared_async_event_*() helpers
 *   3. Call shared_async_play() to start non-blocking playback
 *   4. Call shared_async_stop() to halt all playback
 *   5. Call shared_async_cleanup() at shutdown
 */

#ifndef SHARED_ASYNC_H
#define SHARED_ASYNC_H

#include "context.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SHARED_ASYNC_MAX_SLOTS 8           /* Maximum concurrent playback slots */
#define SHARED_ASYNC_TICKS_PER_QUARTER 480 /* Standard MIDI resolution */
#define SHARED_ASYNC_DEFAULT_TEMPO 120     /* Default BPM */

/* ============================================================================
 * Source Line Tracking (for playback visualization)
 * ============================================================================ */

#ifdef SHARED_SOURCE_TRACKING
/**
 * @brief Set the source line for subsequent scheduled events.
 * Call this before scheduling events to record which source line produced them.
 * @param sched SharedAsyncSchedule pointer.
 * @param line Source line number (1-based, or 0 for unknown).
 */
#define SHARED_SET_SOURCE_LINE(sched, line) \
    do { if (sched) (sched)->source_tracking_line = (line); } while(0)
#else
#define SHARED_SET_SOURCE_LINE(sched, line) ((void)0)
#endif

/* ============================================================================
 * Completion Callback
 * ============================================================================ */

/**
 * Callback invoked when a playback slot completes.
 * Called from the async thread, so must be thread-safe.
 *
 * @param slot_id The slot that completed.
 * @param stopped Non-zero if stopped by user, zero if completed naturally.
 * @param userdata User-provided data from shared_async_play_ex().
 */
typedef void (*SharedAsyncCompletionCallback)(int slot_id, int stopped, void *userdata);

/* ============================================================================
 * Event Types
 * ============================================================================ */

typedef enum {
    SHARED_ASYNC_NOTE,      /* Note with duration (auto note-off) */
    SHARED_ASYNC_NOTE_ON,   /* Note on only (manual note-off) */
    SHARED_ASYNC_NOTE_OFF,  /* Note off */
    SHARED_ASYNC_CC,        /* Control change */
    SHARED_ASYNC_PROGRAM,   /* Program change */
    SHARED_ASYNC_TEMPO,     /* Tempo change (data1 = BPM) */
} SharedAsyncEventType;

/**
 * Generic scheduled event for async playback.
 * Supports both tick-based and millisecond-based timing.
 */
typedef struct SharedAsyncEvent {
    int tick;               /* Tick position (used when use_ticks is set) */
    int time_ms;            /* Millisecond position (used when use_ticks is not set) */
    SharedAsyncEventType type;
    int channel;            /* MIDI channel (0-15) */
    int data1;              /* Pitch for notes, CC number, program, or tempo BPM */
    int data2;              /* Velocity for notes, CC value */
    int duration_ticks;     /* Duration in ticks (for tick mode) */
    int duration_ms;        /* Duration in ms (for ms mode) */
#ifdef SHARED_SOURCE_TRACKING
    int source_line;        /* Source line number (1-based, 0=unknown) */
#endif
} SharedAsyncEvent;

/**
 * Launch quantization options for beat-aligned start.
 */
typedef enum {
    LAUNCH_QUANT_IMMEDIATE = 0,  /* Start immediately (no quantization) */
    LAUNCH_QUANT_BEAT = 1,       /* Start on next beat */
    LAUNCH_QUANT_BAR = 4,        /* Start on next bar (4/4 time) */
} LaunchQuantize;

/**
 * Schedule of events for playback.
 */
typedef struct SharedAsyncSchedule {
    SharedAsyncEvent* events;
    size_t count;
    size_t capacity;
    int total_duration_ms;
    int use_ticks;          /* Non-zero to use tick-based timing */
    int initial_tempo;      /* Starting tempo in BPM (for tick mode) */
    int launch_quantize;    /* Beat quantization (0=immediate, 1=beat, 4=bar, etc.) */
#ifdef SHARED_SOURCE_TRACKING
    int source_tracking_line; /* Current source line for new events (set via SHARED_SET_SOURCE_LINE) */
#endif
} SharedAsyncSchedule;

/* ============================================================================
 * Schedule Management
 * ============================================================================ */

/**
 * Create a new empty schedule.
 * @return New schedule, or NULL on allocation failure.
 */
SharedAsyncSchedule* shared_async_schedule_new(void);

/**
 * Free a schedule and its events.
 */
void shared_async_schedule_free(SharedAsyncSchedule* sched);

/**
 * Add a note event (with automatic note-off after duration).
 */
void shared_async_schedule_note(SharedAsyncSchedule* sched, int time_ms,
                                 int channel, int pitch, int velocity,
                                 int duration_ms);

/**
 * Add a note-on event (no automatic note-off).
 */
void shared_async_schedule_note_on(SharedAsyncSchedule* sched, int time_ms,
                                    int channel, int pitch, int velocity);

/**
 * Add a note-off event.
 */
void shared_async_schedule_note_off(SharedAsyncSchedule* sched, int time_ms,
                                     int channel, int pitch);

/**
 * Add a control change event.
 */
void shared_async_schedule_cc(SharedAsyncSchedule* sched, int time_ms,
                               int channel, int cc, int value);

/**
 * Add a program change event.
 */
void shared_async_schedule_program(SharedAsyncSchedule* sched, int time_ms,
                                    int channel, int program);

/* ============================================================================
 * Tick-Based Schedule Helpers (for Alda and similar tick-based languages)
 * ============================================================================ */

/**
 * Set schedule to use tick-based timing.
 * @param sched Schedule to configure.
 * @param initial_tempo Starting tempo in BPM.
 */
void shared_async_schedule_set_tick_mode(SharedAsyncSchedule* sched, int initial_tempo);

/**
 * Add a note-on event at tick position.
 */
void shared_async_schedule_note_on_tick(SharedAsyncSchedule* sched, int tick,
                                         int channel, int pitch, int velocity);

/**
 * Add a note-off event at tick position.
 */
void shared_async_schedule_note_off_tick(SharedAsyncSchedule* sched, int tick,
                                          int channel, int pitch);

/**
 * Add a control change event at tick position.
 */
void shared_async_schedule_cc_tick(SharedAsyncSchedule* sched, int tick,
                                    int channel, int cc, int value);

/**
 * Add a program change event at tick position.
 */
void shared_async_schedule_program_tick(SharedAsyncSchedule* sched, int tick,
                                         int channel, int program);

/**
 * Add a tempo change event at tick position.
 * @param sched Schedule.
 * @param tick Tick position.
 * @param tempo New tempo in BPM.
 */
void shared_async_schedule_tempo(SharedAsyncSchedule* sched, int tick, int tempo);

/* ============================================================================
 * Source-Tracking Schedule Helpers (explicit source line parameter)
 * ============================================================================ */

#ifdef SHARED_SOURCE_TRACKING
/**
 * Add a note event with explicit source line.
 */
void shared_async_schedule_note_ex(SharedAsyncSchedule* sched, int time_ms,
                                    int channel, int pitch, int velocity,
                                    int duration_ms, int source_line);

/**
 * Add a note-on event at tick position with explicit source line.
 */
void shared_async_schedule_note_on_tick_ex(SharedAsyncSchedule* sched, int tick,
                                            int channel, int pitch, int velocity,
                                            int source_line);

/**
 * Add a note-off event at tick position with explicit source line.
 */
void shared_async_schedule_note_off_tick_ex(SharedAsyncSchedule* sched, int tick,
                                             int channel, int pitch,
                                             int source_line);
#endif

/**
 * Convert ticks to milliseconds.
 * @param ticks Tick count.
 * @param tempo Tempo in BPM.
 * @return Duration in milliseconds.
 */
int shared_async_ticks_to_ms(int ticks, int tempo);

/**
 * @brief Sort events into playing order.
 *
 * By time, then within one time: note-offs, then program, controller and
 * tempo changes, then note-ons.
 *
 * @param events Events to sort in place.
 * @param count Number of events.
 * @param use_ticks Non-zero to sort by tick, zero by milliseconds.
 */
void shared_async_sort_events(SharedAsyncEvent* events, int count, int use_ticks);

/**
 * Set launch quantization for beat-aligned start.
 * When Link is enabled, playback will wait until the next beat boundary.
 *
 * @param sched Schedule to configure.
 * @param quantize Beat quantum (0=immediate, 1=next beat, 4=next bar in 4/4).
 */
void shared_async_schedule_set_launch_quantize(SharedAsyncSchedule* sched, int quantize);

/* ============================================================================
 * Async Playback System
 * ============================================================================ */

/**
 * Initialize the async playback system.
 * Creates a libuv event loop in a background thread.
 *
 * @return 0 on success, -1 on error.
 */
int shared_async_init(void);

/**
 * Cleanup the async playback system.
 * Stops all playback and shuts down the background thread.
 */
void shared_async_cleanup(void);

/**
 * Play a schedule asynchronously.
 * The schedule is deep-copied; caller retains ownership and can free it.
 *
 * @param sched Schedule to play.
 * @param ctx SharedContext for MIDI output.
 * @return Slot ID (0 to MAX_SLOTS-1) on success, -1 on error.
 */
int shared_async_play(SharedAsyncSchedule* sched, SharedContext* ctx);

/**
 * Play a schedule asynchronously with completion callback.
 * Extended version that allows notification when playback completes.
 *
 * @param sched Schedule to play.
 * @param ctx SharedContext for MIDI output.
 * @param callback Function to call when playback completes (can be NULL).
 * @param userdata User data passed to callback.
 * @return Slot ID (0 to MAX_SLOTS-1) on success, -1 on error.
 */
int shared_async_play_ex(SharedAsyncSchedule* sched, SharedContext* ctx,
                         SharedAsyncCompletionCallback callback, void *userdata);

/**
 * Stop a specific playback slot.
 * @param slot_id Slot to stop (-1 to stop all slots).
 */
void shared_async_stop(int slot_id);

/**
 * Stop all async playback.
 */
void shared_async_stop_all(void);

/**
 * Check if any async playback is active.
 * @return Number of active slots.
 */
int shared_async_active_count(void);

/**
 * Check if a specific slot is playing.
 * @param slot_id Slot to check.
 * @return Non-zero if playing.
 */
int shared_async_is_slot_playing(int slot_id);

/**
 * Wait for all async playback to complete.
 * @param timeout_ms Maximum wait time (0 = infinite).
 * @return 0 if completed, -1 if timed out.
 */
int shared_async_wait_all(int timeout_ms);

/**
 * Wait for a specific slot to complete.
 * @param slot_id Slot to wait for.
 * @param timeout_ms Maximum wait time (0 = infinite).
 * @return 0 if completed, -1 if timed out.
 */
int shared_async_wait(int slot_id, int timeout_ms);

/* ============================================================================
 * Playback State Query (for visualization)
 * ============================================================================ */

#ifdef SHARED_SOURCE_TRACKING
/**
 * Get the source line of the most recently played event in a slot.
 * @param slot_id Slot to query (-1 for any active slot).
 * @return Source line number (1-based), or 0 if unknown/not playing.
 */
int shared_async_get_current_source_line(int slot_id);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SHARED_ASYNC_H */
