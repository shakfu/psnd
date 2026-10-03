/**
 * @file test_midi_events.c
 * @brief Tests for shared MIDI event buffer.
 *
 * Tests verify:
 * - Buffer initialization and cleanup
 * - Event recording (note on/off, program, CC, tempo)
 * - Event retrieval and counting
 * - Buffer clearing and reuse
 * - Event sorting
 */

#include "test_framework.h"
#include "midi/events.h"

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(midi_events_init_cleanup) {
    /* Init should succeed */
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(shared_midi_events_is_initialized());

    /* Ticks per quarter should be set */
    ASSERT_EQ(shared_midi_events_ticks_per_quarter(), 480);

    /* Cleanup should work */
    shared_midi_events_cleanup();
    ASSERT_FALSE(shared_midi_events_is_initialized());
}

TEST(midi_events_double_init) {
    /* First init */
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Second init should succeed (reinitialize) */
    result = shared_midi_events_init(960);
    ASSERT_EQ(result, 0);

    /* Should have new ticks value */
    ASSERT_EQ(shared_midi_events_ticks_per_quarter(), 960);

    shared_midi_events_cleanup();
}

TEST(midi_events_cleanup_without_init) {
    /* Cleanup without init should not crash */
    shared_midi_events_cleanup();
}

TEST(midi_events_not_initialized) {
    /* Operations on uninitialized buffer should fail safely */
    shared_midi_events_cleanup();

    ASSERT_FALSE(shared_midi_events_is_initialized());
    ASSERT_EQ(shared_midi_events_count(), 0);
    ASSERT_EQ(shared_midi_events_ticks_per_quarter(), 0);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NULL(events);
    ASSERT_EQ(count, 0);
}

/* ============================================================================
 * Event Recording Tests
 * ============================================================================ */

TEST(midi_events_note_on) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add a note on event */
    result = shared_midi_events_note_on(0, 0, 60, 100);
    ASSERT_EQ(result, 0);

    ASSERT_EQ(shared_midi_events_count(), 1);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 1);

    ASSERT_EQ(events[0].tick, 0);
    ASSERT_EQ(events[0].type, SHARED_MIDI_NOTE_ON);
    ASSERT_EQ(events[0].channel, 0);
    ASSERT_EQ(events[0].data1, 60);
    ASSERT_EQ(events[0].data2, 100);

    shared_midi_events_cleanup();
}

TEST(midi_events_note_off) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    result = shared_midi_events_note_off(480, 1, 72);
    ASSERT_EQ(result, 0);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 1);

    ASSERT_EQ(events[0].tick, 480);
    ASSERT_EQ(events[0].type, SHARED_MIDI_NOTE_OFF);
    ASSERT_EQ(events[0].channel, 1);
    ASSERT_EQ(events[0].data1, 72);
    ASSERT_EQ(events[0].data2, 0);

    shared_midi_events_cleanup();
}

TEST(midi_events_program_change) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    result = shared_midi_events_program(0, 2, 25);
    ASSERT_EQ(result, 0);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 1);

    ASSERT_EQ(events[0].type, SHARED_MIDI_PROGRAM);
    ASSERT_EQ(events[0].channel, 2);
    ASSERT_EQ(events[0].data1, 25);

    shared_midi_events_cleanup();
}

TEST(midi_events_control_change) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* CC #7 = volume */
    result = shared_midi_events_cc(0, 0, 7, 80);
    ASSERT_EQ(result, 0);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 1);

    ASSERT_EQ(events[0].type, SHARED_MIDI_CC);
    ASSERT_EQ(events[0].channel, 0);
    ASSERT_EQ(events[0].data1, 7);
    ASSERT_EQ(events[0].data2, 80);

    shared_midi_events_cleanup();
}

TEST(midi_events_tempo_change) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    result = shared_midi_events_tempo(0, 120);
    ASSERT_EQ(result, 0);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 1);

    ASSERT_EQ(events[0].type, SHARED_MIDI_TEMPO);
    ASSERT_EQ(events[0].data1, 120);

    shared_midi_events_cleanup();
}

TEST(midi_events_add_via_struct) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    SharedMidiEvent evt = {
        .tick = 960,
        .type = SHARED_MIDI_NOTE_ON,
        .channel = 3,
        .data1 = 48,
        .data2 = 90
    };

    result = shared_midi_events_add(&evt);
    ASSERT_EQ(result, 0);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 1);

    ASSERT_EQ(events[0].tick, 960);
    ASSERT_EQ(events[0].type, SHARED_MIDI_NOTE_ON);
    ASSERT_EQ(events[0].channel, 3);
    ASSERT_EQ(events[0].data1, 48);
    ASSERT_EQ(events[0].data2, 90);

    shared_midi_events_cleanup();
}

/* ============================================================================
 * Multiple Events Tests
 * ============================================================================ */

TEST(midi_events_multiple_notes) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add a simple melody: C4, D4, E4 */
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_off(480, 0, 60);
    shared_midi_events_note_on(480, 0, 62, 100);
    shared_midi_events_note_off(960, 0, 62);
    shared_midi_events_note_on(960, 0, 64, 100);
    shared_midi_events_note_off(1440, 0, 64);

    ASSERT_EQ(shared_midi_events_count(), 6);

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ(count, 6);

    /* Verify all events */
    ASSERT_EQ(events[0].data1, 60);  /* C4 on */
    ASSERT_EQ(events[1].data1, 60);  /* C4 off */
    ASSERT_EQ(events[2].data1, 62);  /* D4 on */
    ASSERT_EQ(events[3].data1, 62);  /* D4 off */
    ASSERT_EQ(events[4].data1, 64);  /* E4 on */
    ASSERT_EQ(events[5].data1, 64);  /* E4 off */

    shared_midi_events_cleanup();
}

TEST(midi_events_multiple_channels) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add events on multiple channels */
    shared_midi_events_program(0, 0, 0);   /* Piano on ch 0 */
    shared_midi_events_program(0, 1, 24);  /* Guitar on ch 1 */
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_on(0, 1, 48, 80);

    ASSERT_EQ(shared_midi_events_count(), 4);

    shared_midi_events_cleanup();
}

/* ============================================================================
 * Buffer Management Tests
 * ============================================================================ */

TEST(midi_events_clear) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add some events */
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_on(0, 0, 64, 100);
    shared_midi_events_note_on(0, 0, 67, 100);
    ASSERT_EQ(shared_midi_events_count(), 3);

    /* Clear the buffer */
    shared_midi_events_clear();
    ASSERT_EQ(shared_midi_events_count(), 0);

    /* Buffer should still be initialized */
    ASSERT_TRUE(shared_midi_events_is_initialized());

    /* Can add more events */
    shared_midi_events_note_on(0, 0, 72, 100);
    ASSERT_EQ(shared_midi_events_count(), 1);

    shared_midi_events_cleanup();
}

TEST(midi_events_add_when_not_initialized) {
    shared_midi_events_cleanup();

    /* Should fail gracefully */
    int result = shared_midi_events_note_on(0, 0, 60, 100);
    ASSERT_EQ(result, -1);

    SharedMidiEvent evt = {0, SHARED_MIDI_NOTE_ON, 0, 60, 100};
    result = shared_midi_events_add(&evt);
    ASSERT_EQ(result, -1);
}

/* ============================================================================
 * Sorting Tests
 * ============================================================================ */

TEST(midi_events_sort_by_tick) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add events out of order */
    shared_midi_events_note_on(960, 0, 64, 100);   /* tick 960 first */
    shared_midi_events_note_on(0, 0, 60, 100);     /* tick 0 second */
    shared_midi_events_note_on(480, 0, 62, 100);   /* tick 480 third */

    /* Sort events */
    shared_midi_events_sort();

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);

    /* Verify sorted order */
    ASSERT_EQ(events[0].tick, 0);
    ASSERT_EQ(events[0].data1, 60);  /* C4 */
    ASSERT_EQ(events[1].tick, 480);
    ASSERT_EQ(events[1].data1, 62);  /* D4 */
    ASSERT_EQ(events[2].tick, 960);
    ASSERT_EQ(events[2].data1, 64);  /* E4 */

    shared_midi_events_cleanup();
}

/* Within a tick the order is explicit, not left to qsort's stability */
TEST(midi_events_sort_stable) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add multiple events at same tick */
    shared_midi_events_program(0, 0, 0);        /* Program first */
    shared_midi_events_note_on(0, 0, 60, 100);  /* Note second */

    shared_midi_events_sort();

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_NOT_NULL(events);

    /* Order should be preserved for same tick */
    ASSERT_EQ(events[0].type, SHARED_MIDI_PROGRAM);
    ASSERT_EQ(events[1].type, SHARED_MIDI_NOTE_ON);

    shared_midi_events_cleanup();
}

/* A note ending where the same pitch starts again must end first */
TEST(midi_events_sort_note_off_before_note_on) {
    ASSERT_EQ(shared_midi_events_init(480), 0);

    shared_midi_events_note_on(480, 0, 60, 100);
    shared_midi_events_cc(480, 0, 11, 100);
    shared_midi_events_note_off(480, 0, 60);
    shared_midi_events_sort();

    int count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&count);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(events[0].type, SHARED_MIDI_NOTE_OFF);
    ASSERT_EQ(events[1].type, SHARED_MIDI_CC);
    ASSERT_EQ(events[2].type, SHARED_MIDI_NOTE_ON);

    shared_midi_events_cleanup();
}

/* ============================================================================
 * Capacity Tests
 * ============================================================================ */

TEST(midi_events_many_events) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add many events to test buffer growth */
    int tick = 0;
    for (int i = 0; i < 1000; i++) {
        shared_midi_events_note_on(tick, 0, 60, 100);
        shared_midi_events_note_off(tick + 240, 0, 60);
        tick += 480;
    }

    ASSERT_EQ(shared_midi_events_count(), 2000);

    shared_midi_events_cleanup();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

BEGIN_TEST_SUITE("Shared MIDI Events Tests")

    /* Initialization */
    RUN_TEST(midi_events_init_cleanup);
    RUN_TEST(midi_events_double_init);
    RUN_TEST(midi_events_cleanup_without_init);
    RUN_TEST(midi_events_not_initialized);

    /* Event Recording */
    RUN_TEST(midi_events_note_on);
    RUN_TEST(midi_events_note_off);
    RUN_TEST(midi_events_program_change);
    RUN_TEST(midi_events_control_change);
    RUN_TEST(midi_events_tempo_change);
    RUN_TEST(midi_events_add_via_struct);

    /* Multiple Events */
    RUN_TEST(midi_events_multiple_notes);
    RUN_TEST(midi_events_multiple_channels);

    /* Buffer Management */
    RUN_TEST(midi_events_clear);
    RUN_TEST(midi_events_add_when_not_initialized);

    /* Sorting */
    RUN_TEST(midi_events_sort_by_tick);
    RUN_TEST(midi_events_sort_stable);
    RUN_TEST(midi_events_sort_note_off_before_note_on);

    /* Capacity */
    RUN_TEST(midi_events_many_events);

END_TEST_SUITE()
