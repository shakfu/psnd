/**
 * @file test_shared_async.c
 * @brief Tests for shared asynchronous MIDI playback system.
 *
 * Tests verify:
 * - Schedule creation and memory management
 * - Event scheduling helpers (ms and tick modes)
 * - Tick-to-ms conversion
 * - NULL handling
 * - System init/cleanup lifecycle
 * - State queries
 *
 * Note: Actual playback tests are limited since they would require
 * a valid SharedContext with audio/MIDI output configured.
 */

#include "test_framework.h"
#include "async/shared_async.h"
#include "context.h"
#include <uv.h>
#include <string.h>

test_stats_t test_stats;

/* ============================================================================
 * Schedule Creation Tests
 * ============================================================================ */

TEST(async_schedule_new_creates_empty) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    ASSERT_EQ(sched->count, 0);
    ASSERT_EQ(sched->capacity, 0);
    ASSERT_NULL(sched->events);
    ASSERT_EQ(sched->total_duration_ms, 0);
    ASSERT_EQ(sched->use_ticks, 0);
    ASSERT_EQ(sched->initial_tempo, SHARED_ASYNC_DEFAULT_TEMPO);
    ASSERT_EQ(sched->launch_quantize, LAUNCH_QUANT_IMMEDIATE);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_free_null_safe) {
    /* Should not crash */
    shared_async_schedule_free(NULL);
}

TEST(async_schedule_free_empty) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Should not crash when freeing empty schedule */
    shared_async_schedule_free(sched);
}

/* ============================================================================
 * Note Scheduling Tests (millisecond mode)
 * ============================================================================ */

TEST(async_schedule_note_basic) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note(sched, 0, 1, 60, 100, 500);

    ASSERT_EQ(sched->count, 1);
    ASSERT_NOT_NULL(sched->events);
    ASSERT_EQ(sched->events[0].time_ms, 0);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_NOTE);
    ASSERT_EQ(sched->events[0].channel, 1);
    ASSERT_EQ(sched->events[0].data1, 60);  /* pitch */
    ASSERT_EQ(sched->events[0].data2, 100); /* velocity */
    ASSERT_EQ(sched->events[0].duration_ms, 500);
    ASSERT_EQ(sched->total_duration_ms, 500);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_note_on_basic) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note_on(sched, 100, 2, 72, 80);

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].time_ms, 100);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_NOTE_ON);
    ASSERT_EQ(sched->events[0].channel, 2);
    ASSERT_EQ(sched->events[0].data1, 72);
    ASSERT_EQ(sched->events[0].data2, 80);
    ASSERT_EQ(sched->events[0].duration_ms, 0);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_note_off_basic) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note_off(sched, 200, 3, 64);

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].time_ms, 200);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_NOTE_OFF);
    ASSERT_EQ(sched->events[0].channel, 3);
    ASSERT_EQ(sched->events[0].data1, 64);
    ASSERT_EQ(sched->events[0].data2, 0);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_cc_basic) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_cc(sched, 50, 1, 7, 100);  /* CC#7 = volume */

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].time_ms, 50);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_CC);
    ASSERT_EQ(sched->events[0].channel, 1);
    ASSERT_EQ(sched->events[0].data1, 7);   /* CC number */
    ASSERT_EQ(sched->events[0].data2, 100); /* value */

    shared_async_schedule_free(sched);
}

TEST(async_schedule_program_basic) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_program(sched, 0, 1, 25);  /* Acoustic Guitar */

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].time_ms, 0);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_PROGRAM);
    ASSERT_EQ(sched->events[0].channel, 1);
    ASSERT_EQ(sched->events[0].data1, 25);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_multiple_events) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Schedule a simple sequence: program, note, note */
    shared_async_schedule_program(sched, 0, 1, 0);
    shared_async_schedule_note(sched, 0, 1, 60, 100, 500);
    shared_async_schedule_note(sched, 500, 1, 64, 100, 500);
    shared_async_schedule_note(sched, 1000, 1, 67, 100, 500);

    ASSERT_EQ(sched->count, 4);
    ASSERT_EQ(sched->total_duration_ms, 1500);  /* last note ends at 1000+500 */

    shared_async_schedule_free(sched);
}

TEST(async_schedule_duration_tracking) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Events at different times */
    shared_async_schedule_note(sched, 0, 1, 60, 100, 100);     /* ends at 100 */
    shared_async_schedule_note(sched, 500, 1, 64, 100, 200);   /* ends at 700 */
    shared_async_schedule_note(sched, 300, 1, 62, 100, 1000);  /* ends at 1300 */

    /* Duration should track the latest end time */
    ASSERT_EQ(sched->total_duration_ms, 1300);

    shared_async_schedule_free(sched);
}

/* ============================================================================
 * NULL Schedule Handling Tests
 * ============================================================================ */

TEST(async_schedule_note_null_safe) {
    /* Should not crash */
    shared_async_schedule_note(NULL, 0, 1, 60, 100, 500);
}

TEST(async_schedule_note_on_null_safe) {
    shared_async_schedule_note_on(NULL, 0, 1, 60, 100);
}

TEST(async_schedule_note_off_null_safe) {
    shared_async_schedule_note_off(NULL, 0, 1, 60);
}

TEST(async_schedule_cc_null_safe) {
    shared_async_schedule_cc(NULL, 0, 1, 7, 100);
}

TEST(async_schedule_program_null_safe) {
    shared_async_schedule_program(NULL, 0, 1, 0);
}

/* ============================================================================
 * Tick Mode Tests
 * ============================================================================ */

TEST(async_schedule_set_tick_mode) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    ASSERT_EQ(sched->use_ticks, 0);
    ASSERT_EQ(sched->initial_tempo, SHARED_ASYNC_DEFAULT_TEMPO);

    shared_async_schedule_set_tick_mode(sched, 140);

    ASSERT_EQ(sched->use_ticks, 1);
    ASSERT_EQ(sched->initial_tempo, 140);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_set_tick_mode_default_tempo) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Invalid tempo should use default */
    shared_async_schedule_set_tick_mode(sched, 0);
    ASSERT_EQ(sched->initial_tempo, SHARED_ASYNC_DEFAULT_TEMPO);

    shared_async_schedule_set_tick_mode(sched, -10);
    ASSERT_EQ(sched->initial_tempo, SHARED_ASYNC_DEFAULT_TEMPO);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_set_tick_mode_null_safe) {
    /* Should not crash */
    shared_async_schedule_set_tick_mode(NULL, 120);
}

TEST(async_schedule_note_on_tick) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_set_tick_mode(sched, 120);
    shared_async_schedule_note_on_tick(sched, 480, 1, 60, 100);

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].tick, 480);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_NOTE_ON);
    ASSERT_EQ(sched->events[0].channel, 1);
    ASSERT_EQ(sched->events[0].data1, 60);
    ASSERT_EQ(sched->events[0].data2, 100);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_note_off_tick) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note_off_tick(sched, 960, 1, 60);

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].tick, 960);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_NOTE_OFF);
    ASSERT_EQ(sched->events[0].data1, 60);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_cc_tick) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_cc_tick(sched, 240, 1, 1, 64);  /* CC#1 = modulation */

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].tick, 240);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_CC);
    ASSERT_EQ(sched->events[0].data1, 1);
    ASSERT_EQ(sched->events[0].data2, 64);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_program_tick) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_program_tick(sched, 0, 1, 40);  /* Violin */

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].tick, 0);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_PROGRAM);
    ASSERT_EQ(sched->events[0].data1, 40);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_tempo) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_tempo(sched, 1920, 140);

    ASSERT_EQ(sched->count, 1);
    ASSERT_EQ(sched->events[0].tick, 1920);
    ASSERT_EQ(sched->events[0].type, SHARED_ASYNC_TEMPO);
    ASSERT_EQ(sched->events[0].data1, 140);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_tempo_default) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Invalid tempo should use default */
    shared_async_schedule_tempo(sched, 0, 0);
    ASSERT_EQ(sched->events[0].data1, SHARED_ASYNC_DEFAULT_TEMPO);

    shared_async_schedule_tempo(sched, 0, -50);
    ASSERT_EQ(sched->events[1].data1, SHARED_ASYNC_DEFAULT_TEMPO);

    shared_async_schedule_free(sched);
}

/* NULL handling for tick functions */
TEST(async_schedule_note_on_tick_null_safe) {
    shared_async_schedule_note_on_tick(NULL, 0, 1, 60, 100);
}

TEST(async_schedule_note_off_tick_null_safe) {
    shared_async_schedule_note_off_tick(NULL, 0, 1, 60);
}

TEST(async_schedule_cc_tick_null_safe) {
    shared_async_schedule_cc_tick(NULL, 0, 1, 7, 100);
}

TEST(async_schedule_program_tick_null_safe) {
    shared_async_schedule_program_tick(NULL, 0, 1, 0);
}

TEST(async_schedule_tempo_null_safe) {
    shared_async_schedule_tempo(NULL, 0, 120);
}

/* ============================================================================
 * Tick-to-MS Conversion Tests
 * ============================================================================ */

/* Within a tick: note-offs, then settings, then note-ons */
TEST(async_sort_events_same_tick_order) {
    SharedAsyncEvent events[3];
    memset(events, 0, sizeof(events));
    events[0].tick = 480; events[0].type = SHARED_ASYNC_NOTE_ON;  events[0].data1 = 60;
    events[1].tick = 480; events[1].type = SHARED_ASYNC_PROGRAM;  events[1].data1 = 24;
    events[2].tick = 480; events[2].type = SHARED_ASYNC_NOTE_OFF; events[2].data1 = 60;

    shared_async_sort_events(events, 3, 1);

    ASSERT_EQ(events[0].type, SHARED_ASYNC_NOTE_OFF);
    ASSERT_EQ(events[1].type, SHARED_ASYNC_PROGRAM);
    ASSERT_EQ(events[2].type, SHARED_ASYNC_NOTE_ON);
}

TEST(async_ticks_to_ms_120bpm) {
    /* At 120 BPM with 480 ticks per quarter:
     * 1 beat = 500ms, 480 ticks = 500ms
     * So 1 tick = 500/480 = ~1.04ms
     */
    int ms = shared_async_ticks_to_ms(480, 120);
    ASSERT_EQ(ms, 500);

    ms = shared_async_ticks_to_ms(960, 120);  /* 2 beats */
    ASSERT_EQ(ms, 1000);

    ms = shared_async_ticks_to_ms(240, 120);  /* half beat */
    ASSERT_EQ(ms, 250);
}

TEST(async_ticks_to_ms_60bpm) {
    /* At 60 BPM: 1 beat = 1000ms */
    int ms = shared_async_ticks_to_ms(480, 60);
    ASSERT_EQ(ms, 1000);
}

TEST(async_ticks_to_ms_240bpm) {
    /* At 240 BPM: 1 beat = 250ms */
    int ms = shared_async_ticks_to_ms(480, 240);
    ASSERT_EQ(ms, 250);
}

TEST(async_ticks_to_ms_zero_ticks) {
    int ms = shared_async_ticks_to_ms(0, 120);
    ASSERT_EQ(ms, 0);
}

TEST(async_ticks_to_ms_invalid_tempo) {
    /* Invalid tempo should use default (120 BPM) */
    int ms = shared_async_ticks_to_ms(480, 0);
    ASSERT_EQ(ms, 500);

    ms = shared_async_ticks_to_ms(480, -10);
    ASSERT_EQ(ms, 500);
}

/* ============================================================================
 * Launch Quantization Tests
 * ============================================================================ */

TEST(async_schedule_set_launch_quantize) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    ASSERT_EQ(sched->launch_quantize, LAUNCH_QUANT_IMMEDIATE);

    shared_async_schedule_set_launch_quantize(sched, LAUNCH_QUANT_BEAT);
    ASSERT_EQ(sched->launch_quantize, LAUNCH_QUANT_BEAT);

    shared_async_schedule_set_launch_quantize(sched, LAUNCH_QUANT_BAR);
    ASSERT_EQ(sched->launch_quantize, LAUNCH_QUANT_BAR);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_set_launch_quantize_invalid) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Negative values should be clamped to 0 */
    shared_async_schedule_set_launch_quantize(sched, -5);
    ASSERT_EQ(sched->launch_quantize, 0);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_set_launch_quantize_null_safe) {
    shared_async_schedule_set_launch_quantize(NULL, LAUNCH_QUANT_BEAT);
}

/* ============================================================================
 * Schedule Capacity Tests
 * ============================================================================ */

TEST(async_schedule_grows_capacity) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Add many events to trigger growth */
    for (int i = 0; i < 100; i++) {
        shared_async_schedule_note(sched, i * 100, 1, 60, 100, 50);
    }

    ASSERT_EQ(sched->count, 100);
    ASSERT_TRUE(sched->capacity >= 100);
    ASSERT_NOT_NULL(sched->events);

    shared_async_schedule_free(sched);
}

/* ============================================================================
 * System Init/Cleanup Tests
 * ============================================================================ */

TEST(async_active_count_before_init) {
    /* Before init, should return 0 */
    int count = shared_async_active_count();
    ASSERT_EQ(count, 0);
}

TEST(async_is_slot_playing_before_init) {
    int playing = shared_async_is_slot_playing(0);
    ASSERT_EQ(playing, 0);
}

TEST(async_is_slot_playing_invalid_slot) {
    int playing = shared_async_is_slot_playing(-1);
    ASSERT_EQ(playing, 0);

    playing = shared_async_is_slot_playing(SHARED_ASYNC_MAX_SLOTS + 10);
    ASSERT_EQ(playing, 0);
}

TEST(async_stop_before_init) {
    /* Should not crash */
    shared_async_stop(0);
    shared_async_stop(-1);
}

TEST(async_stop_all_before_init) {
    /* Should not crash */
    shared_async_stop_all();
}

TEST(async_wait_before_init) {
    /* Should return immediately */
    int result = shared_async_wait(0, 100);
    ASSERT_EQ(result, 0);
}

TEST(async_wait_all_before_init) {
    /* Should return immediately */
    int result = shared_async_wait_all(100);
    ASSERT_EQ(result, 0);
}

TEST(async_wait_invalid_slot) {
    int result = shared_async_wait(-1, 100);
    ASSERT_EQ(result, 0);

    result = shared_async_wait(SHARED_ASYNC_MAX_SLOTS + 10, 100);
    ASSERT_EQ(result, 0);
}

TEST(async_cleanup_before_init) {
    /* Should not crash */
    shared_async_cleanup();
}

TEST(async_init_cleanup_cycle) {
    /* First cleanup to ensure clean state */
    shared_async_cleanup();

    /* Initialize */
    int result = shared_async_init();
    ASSERT_EQ(result, 0);

    /* Should report no active slots */
    ASSERT_EQ(shared_async_active_count(), 0);

    /* Double init should be safe */
    result = shared_async_init();
    ASSERT_EQ(result, 0);

    /* Cleanup */
    shared_async_cleanup();

    /* After cleanup, active count should be 0 */
    ASSERT_EQ(shared_async_active_count(), 0);

    /* Double cleanup should be safe */
    shared_async_cleanup();
}

/* ============================================================================
 * Play Function Validation Tests
 * ============================================================================ */

TEST(async_play_null_schedule) {
    int slot = shared_async_play(NULL, NULL);
    ASSERT_EQ(slot, -1);
}

TEST(async_play_empty_schedule) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Empty schedule should fail */
    int slot = shared_async_play(sched, NULL);
    ASSERT_EQ(slot, -1);

    shared_async_schedule_free(sched);
}

TEST(async_play_null_context) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note(sched, 0, 1, 60, 100, 100);

    /* NULL context should fail */
    int slot = shared_async_play(sched, NULL);
    ASSERT_EQ(slot, -1);

    shared_async_schedule_free(sched);
}

TEST(async_play_no_output) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note(sched, 0, 1, 60, 100, 100);

    /* Context with no output should fail */
    SharedContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    int slot = shared_async_play(sched, &ctx);
    ASSERT_EQ(slot, -1);

    shared_async_schedule_free(sched);
}

TEST(async_play_ex_null_schedule) {
    int slot = shared_async_play_ex(NULL, NULL, NULL, NULL);
    ASSERT_EQ(slot, -1);
}

/* ============================================================================
 * Source Tracking Tests (if enabled)
 * ============================================================================ */

#ifdef SHARED_SOURCE_TRACKING

TEST(async_schedule_source_line_tracking) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    /* Set source line before adding events */
    SHARED_SET_SOURCE_LINE(sched, 10);
    shared_async_schedule_note(sched, 0, 1, 60, 100, 100);

    ASSERT_EQ(sched->events[0].source_line, 10);

    /* Change source line */
    SHARED_SET_SOURCE_LINE(sched, 20);
    shared_async_schedule_note(sched, 100, 1, 64, 100, 100);

    ASSERT_EQ(sched->events[1].source_line, 20);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_note_ex_source_line) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note_ex(sched, 0, 1, 60, 100, 500, 42);

    ASSERT_EQ(sched->events[0].source_line, 42);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_note_on_tick_ex_source_line) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note_on_tick_ex(sched, 480, 1, 60, 100, 15);

    ASSERT_EQ(sched->events[0].source_line, 15);

    shared_async_schedule_free(sched);
}

TEST(async_schedule_note_off_tick_ex_source_line) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    ASSERT_NOT_NULL(sched);

    shared_async_schedule_note_off_tick_ex(sched, 960, 1, 60, 25);

    ASSERT_EQ(sched->events[0].source_line, 25);

    shared_async_schedule_free(sched);
}

TEST(async_get_current_source_line_before_init) {
    int line = shared_async_get_current_source_line(0);
    ASSERT_EQ(line, 0);
}

TEST(async_get_current_source_line_any_slot) {
    int line = shared_async_get_current_source_line(-1);
    ASSERT_EQ(line, 0);
}

/* NULL handling for _ex functions */
TEST(async_schedule_note_ex_null_safe) {
    shared_async_schedule_note_ex(NULL, 0, 1, 60, 100, 500, 1);
}

TEST(async_schedule_note_on_tick_ex_null_safe) {
    shared_async_schedule_note_on_tick_ex(NULL, 0, 1, 60, 100, 1);
}

TEST(async_schedule_note_off_tick_ex_null_safe) {
    shared_async_schedule_note_off_tick_ex(NULL, 0, 1, 60, 1);
}

#endif /* SHARED_SOURCE_TRACKING */

/* ============================================================================
 * Test Runner
 * ============================================================================ */


/* ==========================================================================
 * Live Playback (exercises the libuv loop thread)
 *
 * The tests above stop at the argument-validation paths and never reach the
 * event loop. These drive real schedules through it.
 *
 * A context with builtin_synth_enabled set but no synth initialized passes
 * shared_async_play_ex()'s output check while every shared_send_*() call
 * short-circuits on its backend's is_enabled() guard, so playback runs with no
 * audio side effects.
 * ========================================================================== */

typedef struct {
    int calls;
    int last_slot;
    int last_stopped;
} CompletionRecord;

static CompletionRecord g_completion;
static uv_mutex_t g_completion_mutex;
static int g_completion_mutex_ready = 0;

static void completion_record_reset(void) {
    if (!g_completion_mutex_ready) {
        uv_mutex_init(&g_completion_mutex);
        g_completion_mutex_ready = 1;
    }
    uv_mutex_lock(&g_completion_mutex);
    g_completion.calls = 0;
    g_completion.last_slot = -1;
    g_completion.last_stopped = -1;
    uv_mutex_unlock(&g_completion_mutex);
}

static void completion_record_cb(int slot_id, int stopped, void *userdata) {
    (void)userdata;
    uv_mutex_lock(&g_completion_mutex);
    g_completion.calls++;
    g_completion.last_slot = slot_id;
    g_completion.last_stopped = stopped;
    uv_mutex_unlock(&g_completion_mutex);
}

static CompletionRecord completion_record_get(void) {
    uv_mutex_lock(&g_completion_mutex);
    CompletionRecord r = g_completion;
    uv_mutex_unlock(&g_completion_mutex);
    return r;
}

/* Context that satisfies the output check without producing output. */
static void playable_context(SharedContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->builtin_synth_enabled = 1;
}

/* Short schedule: a handful of notes inside ~60ms. */
static SharedAsyncSchedule *short_schedule(void) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    if (!sched) return NULL;
    for (int i = 0; i < 4; i++) {
        shared_async_schedule_note(sched, i * 15, 1, 60 + i, 100, 10);
    }
    return sched;
}

/* Long schedule: runs for several seconds so a stop always lands mid-flight. */
static SharedAsyncSchedule *long_schedule(void) {
    SharedAsyncSchedule *sched = shared_async_schedule_new();
    if (!sched) return NULL;
    for (int i = 0; i < 40; i++) {
        shared_async_schedule_note(sched, i * 200, 1, 60 + (i % 12), 100, 100);
    }
    return sched;
}

TEST(async_play_runs_to_completion) {
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = short_schedule();
    ASSERT_NOT_NULL(sched);

    int slot = shared_async_play_ex(sched, &ctx, completion_record_cb, NULL);
    ASSERT_TRUE(slot >= 0);

    /* The timer is started on the loop thread now, so completion is what proves
     * the handoff actually happened. */
    ASSERT_EQ(shared_async_wait(slot, 5000), 0);
    ASSERT_EQ(shared_async_is_slot_playing(slot), 0);

    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, 1);
    ASSERT_EQ(r.last_slot, slot);
    ASSERT_EQ(r.last_stopped, 0);

    shared_async_schedule_free(sched);
}

TEST(async_stop_finalizes_and_reports_stopped) {
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = long_schedule();
    ASSERT_NOT_NULL(sched);

    int slot = shared_async_play_ex(sched, &ctx, completion_record_cb, NULL);
    ASSERT_TRUE(slot >= 0);

    shared_async_stop(slot);
    ASSERT_EQ(shared_async_wait(slot, 5000), 0);

    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, 1);
    ASSERT_EQ(r.last_slot, slot);
    ASSERT_EQ(r.last_stopped, 1);

    shared_async_schedule_free(sched);
}

TEST(async_repeated_stop_finalizes_once) {
    /* uv_async_send coalesces, and a duplicate stop must not double-invoke the
     * completion callback or tear down a later activation. */
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = long_schedule();
    ASSERT_NOT_NULL(sched);

    int slot = shared_async_play_ex(sched, &ctx, completion_record_cb, NULL);
    ASSERT_TRUE(slot >= 0);

    shared_async_stop(slot);
    shared_async_stop(slot);
    shared_async_stop(slot);

    ASSERT_EQ(shared_async_wait(slot, 5000), 0);
    uv_sleep(50);  /* Let any straggling signal be processed */

    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, 1);

    shared_async_schedule_free(sched);
}

TEST(async_stop_on_idle_slot_is_noop) {
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    /* Slot never started: stop must not invoke a callback or crash. */
    shared_async_stop(0);
    uv_sleep(30);

    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, 0);
    ASSERT_EQ(shared_async_is_slot_playing(0), 0);
}

TEST(async_slot_reuse_after_stop) {
    /* The stop signal for the first activation must not kill the second one
     * that reuses the same slot. */
    ASSERT_EQ(shared_async_init(), 0);

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *lng = long_schedule();
    SharedAsyncSchedule *shrt = short_schedule();
    ASSERT_NOT_NULL(lng);
    ASSERT_NOT_NULL(shrt);

    int first = shared_async_play(lng, &ctx);
    ASSERT_TRUE(first >= 0);
    shared_async_stop(first);
    ASSERT_EQ(shared_async_wait(first, 5000), 0);

    completion_record_reset();

    int second = shared_async_play_ex(shrt, &ctx, completion_record_cb, NULL);
    ASSERT_TRUE(second >= 0);
    ASSERT_EQ(shared_async_wait(second, 5000), 0);

    /* Ran to completion rather than being stopped by a stale signal. */
    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, 1);
    ASSERT_EQ(r.last_stopped, 0);

    shared_async_schedule_free(lng);
    shared_async_schedule_free(shrt);
}

TEST(async_concurrent_slots_all_complete) {
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = short_schedule();
    ASSERT_NOT_NULL(sched);

    const int n = 4;
    int slots[4];
    for (int i = 0; i < n; i++) {
        slots[i] = shared_async_play_ex(sched, &ctx, completion_record_cb, NULL);
        ASSERT_TRUE(slots[i] >= 0);
    }

    /* Slot ids must be distinct. */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_TRUE(slots[i] != slots[j]);
        }
    }

    ASSERT_EQ(shared_async_wait_all(5000), 0);
    ASSERT_EQ(shared_async_active_count(), 0);

    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, n);

    shared_async_schedule_free(sched);
}

TEST(async_stop_all_stops_every_slot) {
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = long_schedule();
    ASSERT_NOT_NULL(sched);

    const int n = 3;
    for (int i = 0; i < n; i++) {
        ASSERT_TRUE(shared_async_play_ex(sched, &ctx, completion_record_cb, NULL) >= 0);
    }

    shared_async_stop_all();
    ASSERT_EQ(shared_async_wait_all(5000), 0);
    ASSERT_EQ(shared_async_active_count(), 0);

    CompletionRecord r = completion_record_get();
    ASSERT_EQ(r.calls, n);
    ASSERT_EQ(r.last_stopped, 1);

    shared_async_schedule_free(sched);
}

TEST(async_rapid_play_stop_cycles) {
    /* Stress the control-thread/loop-thread handshake. Before the timer start
     * was moved onto the loop thread this hammered uv_timer_start from the
     * wrong thread; it is the case most likely to expose a regression under a
     * thread sanitizer. */
    ASSERT_EQ(shared_async_init(), 0);
    completion_record_reset();

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = long_schedule();
    ASSERT_NOT_NULL(sched);

    const int cycles = 40;
    for (int i = 0; i < cycles; i++) {
        int slot = shared_async_play(sched, &ctx);
        ASSERT_TRUE(slot >= 0);
        shared_async_stop(slot);
        ASSERT_EQ(shared_async_wait(slot, 5000), 0);
    }

    ASSERT_EQ(shared_async_wait_all(5000), 0);
    ASSERT_EQ(shared_async_active_count(), 0);

    shared_async_schedule_free(sched);
}

TEST(async_immediate_stop_after_play) {
    /* Stop issued before the loop thread has run on_start_signal: the start
     * must be cancelled rather than starting a timer on a finalized slot. */
    ASSERT_EQ(shared_async_init(), 0);

    SharedContext ctx;
    playable_context(&ctx);

    SharedAsyncSchedule *sched = long_schedule();
    ASSERT_NOT_NULL(sched);

    for (int i = 0; i < 20; i++) {
        completion_record_reset();

        int slot = shared_async_play_ex(sched, &ctx, completion_record_cb, NULL);
        ASSERT_TRUE(slot >= 0);
        shared_async_stop(slot);  /* No sleep: races the start signal */

        ASSERT_EQ(shared_async_wait(slot, 5000), 0);
        uv_sleep(5);

        CompletionRecord r = completion_record_get();
        ASSERT_EQ(r.calls, 1);
    }

    ASSERT_EQ(shared_async_active_count(), 0);
    shared_async_schedule_free(sched);
}

BEGIN_TEST_SUITE("Shared Async Playback Tests")

    /* Schedule creation */
    RUN_TEST(async_schedule_new_creates_empty);
    RUN_TEST(async_schedule_free_null_safe);
    RUN_TEST(async_schedule_free_empty);

    /* Note scheduling (ms mode) */
    RUN_TEST(async_schedule_note_basic);
    RUN_TEST(async_schedule_note_on_basic);
    RUN_TEST(async_schedule_note_off_basic);
    RUN_TEST(async_schedule_cc_basic);
    RUN_TEST(async_schedule_program_basic);
    RUN_TEST(async_schedule_multiple_events);
    RUN_TEST(async_schedule_duration_tracking);

    /* NULL handling (ms mode) */
    RUN_TEST(async_schedule_note_null_safe);
    RUN_TEST(async_schedule_note_on_null_safe);
    RUN_TEST(async_schedule_note_off_null_safe);
    RUN_TEST(async_schedule_cc_null_safe);
    RUN_TEST(async_schedule_program_null_safe);

    /* Tick mode */
    RUN_TEST(async_schedule_set_tick_mode);
    RUN_TEST(async_schedule_set_tick_mode_default_tempo);
    RUN_TEST(async_schedule_set_tick_mode_null_safe);
    RUN_TEST(async_schedule_note_on_tick);
    RUN_TEST(async_schedule_note_off_tick);
    RUN_TEST(async_schedule_cc_tick);
    RUN_TEST(async_schedule_program_tick);
    RUN_TEST(async_schedule_tempo);
    RUN_TEST(async_schedule_tempo_default);

    /* NULL handling (tick mode) */
    RUN_TEST(async_schedule_note_on_tick_null_safe);
    RUN_TEST(async_schedule_note_off_tick_null_safe);
    RUN_TEST(async_schedule_cc_tick_null_safe);
    RUN_TEST(async_schedule_program_tick_null_safe);
    RUN_TEST(async_schedule_tempo_null_safe);

    /* Tick-to-ms conversion */
    RUN_TEST(async_sort_events_same_tick_order);
    RUN_TEST(async_ticks_to_ms_120bpm);
    RUN_TEST(async_ticks_to_ms_60bpm);
    RUN_TEST(async_ticks_to_ms_240bpm);
    RUN_TEST(async_ticks_to_ms_zero_ticks);
    RUN_TEST(async_ticks_to_ms_invalid_tempo);

    /* Launch quantization */
    RUN_TEST(async_schedule_set_launch_quantize);
    RUN_TEST(async_schedule_set_launch_quantize_invalid);
    RUN_TEST(async_schedule_set_launch_quantize_null_safe);

    /* Capacity */
    RUN_TEST(async_schedule_grows_capacity);

    /* System state queries (before init) */
    RUN_TEST(async_active_count_before_init);
    RUN_TEST(async_is_slot_playing_before_init);
    RUN_TEST(async_is_slot_playing_invalid_slot);
    RUN_TEST(async_stop_before_init);
    RUN_TEST(async_stop_all_before_init);
    RUN_TEST(async_wait_before_init);
    RUN_TEST(async_wait_all_before_init);
    RUN_TEST(async_wait_invalid_slot);
    RUN_TEST(async_cleanup_before_init);

    /* Init/cleanup lifecycle */
    RUN_TEST(async_init_cleanup_cycle);

    /* Play validation */
    RUN_TEST(async_play_null_schedule);
    RUN_TEST(async_play_empty_schedule);
    RUN_TEST(async_play_null_context);
    RUN_TEST(async_play_no_output);
    RUN_TEST(async_play_ex_null_schedule);

    /* Live playback through the loop thread */
    RUN_TEST(async_play_runs_to_completion);
    RUN_TEST(async_stop_finalizes_and_reports_stopped);
    RUN_TEST(async_repeated_stop_finalizes_once);
    RUN_TEST(async_stop_on_idle_slot_is_noop);
    RUN_TEST(async_slot_reuse_after_stop);
    RUN_TEST(async_concurrent_slots_all_complete);
    RUN_TEST(async_stop_all_stops_every_slot);
    RUN_TEST(async_rapid_play_stop_cycles);
    RUN_TEST(async_immediate_stop_after_play);

#ifdef SHARED_SOURCE_TRACKING
    /* Source tracking */
    RUN_TEST(async_schedule_source_line_tracking);
    RUN_TEST(async_schedule_note_ex_source_line);
    RUN_TEST(async_schedule_note_on_tick_ex_source_line);
    RUN_TEST(async_schedule_note_off_tick_ex_source_line);
    RUN_TEST(async_get_current_source_line_before_init);
    RUN_TEST(async_get_current_source_line_any_slot);
    RUN_TEST(async_schedule_note_ex_null_safe);
    RUN_TEST(async_schedule_note_on_tick_ex_null_safe);
    RUN_TEST(async_schedule_note_off_tick_ex_null_safe);
#endif

END_TEST_SUITE()
