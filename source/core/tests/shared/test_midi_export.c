/**
 * @file test_midi_export.c
 * @brief Tests for MIDI file export.
 *
 * Tests verify:
 * - Export with empty buffer fails gracefully
 * - Export with events creates a file
 * - Single-channel uses Type 0 format
 * - Multi-channel uses Type 1 format
 */

#include "test_framework.h"
#include "midi/events.h"
#include "loki/midi_export.h"
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define access _access
#define F_OK 0
#define unlink _unlink
#else
#include <unistd.h>
#endif

/* Dynamic test directory */
static char TEST_DIR_BUF[512] = {0};
static const char *TEST_DIR = NULL;

static void init_test_dir(void) {
    if (TEST_DIR) return;
#ifdef _WIN32
    const char *temp = getenv("TEMP");
    if (!temp) temp = getenv("TMP");
    if (!temp) temp = "C:\\Windows\\Temp";
    snprintf(TEST_DIR_BUF, sizeof(TEST_DIR_BUF), "%s", temp);
#else
    snprintf(TEST_DIR_BUF, sizeof(TEST_DIR_BUF), "/tmp");
#endif
    TEST_DIR = TEST_DIR_BUF;
}

/* Helper to build test file path */
static void build_test_path(char *buf, size_t size, const char *name) {
    init_test_dir();
#ifdef _WIN32
    snprintf(buf, size, "%s\\psnd_test_%s.mid", TEST_DIR, name);
#else
    snprintf(buf, size, "%s/psnd_test_%s.mid", TEST_DIR, name);
#endif
}

/* Helper to check if file exists */
static int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

/* Helper to delete test file */
static void delete_test_file(const char *path) {
    unlink(path);
}

/* Helper to validate MIDI file header */
static int is_valid_midi_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    /* Check for "MThd" magic header */
    char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        fclose(f);
        return 0;
    }
    fclose(f);

    return (magic[0] == 'M' && magic[1] == 'T' &&
            magic[2] == 'h' && magic[3] == 'd');
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST(export_empty_buffer) {
    /* Initialize buffer but add no events */
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);
    shared_midi_events_clear();

    char path[256];
    build_test_path(path, sizeof(path), "empty");

    /* Export should fail with empty buffer */
    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, -1);

    /* Check error message */
    const char *err = loki_midi_export_error();
    ASSERT_NOT_NULL(err);

    /* File should not exist */
    ASSERT_FALSE(file_exists(path));

    shared_midi_events_cleanup();
}

TEST(export_null_filename) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add an event so buffer isn't empty */
    shared_midi_events_note_on(0, 0, 60, 100);

    /* Export with NULL filename should fail */
    result = loki_midi_export_shared(NULL);
    ASSERT_EQ(result, -1);

    const char *err = loki_midi_export_error();
    ASSERT_NOT_NULL(err);

    shared_midi_events_cleanup();
}

TEST(export_empty_filename) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    shared_midi_events_note_on(0, 0, 60, 100);

    /* Export with empty filename should fail */
    result = loki_midi_export_shared("");
    ASSERT_EQ(result, -1);

    shared_midi_events_cleanup();
}

/* ============================================================================
 * Single Channel (Type 0) Tests
 * ============================================================================ */

TEST(export_single_note) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Single note: middle C, quarter note */
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_off(480, 0, 60);

    char path[256];
    build_test_path(path, sizeof(path), "single_note");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);

    /* File should exist and be valid MIDI */
    ASSERT_TRUE(file_exists(path));
    ASSERT_TRUE(is_valid_midi_file(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

/* Index of a two-byte MIDI message in a file's bytes, from `from`; -1 if absent */
static long find_message(const unsigned char *data, long size, long from,
                         unsigned char status, unsigned char key) {
    for (long i = from; i + 1 < size; i++) {
        if (data[i] == status && data[i + 1] == key) return i;
    }
    return -1;
}

/* A note ending where the same pitch starts again must end first in the file,
 * or a reader ends the new note instead */
TEST(export_note_off_before_note_on_at_same_tick) {
    ASSERT_EQ(shared_midi_events_init(480), 0);
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_on(480, 0, 60, 100);
    shared_midi_events_note_off(480, 0, 60);
    shared_midi_events_note_off(960, 0, 60);
    shared_midi_events_sort();

    char path[256];
    build_test_path(path, sizeof(path), "same_tick");
    delete_test_file(path);
    ASSERT_EQ(loki_midi_export_shared(path), 0);

    unsigned char data[1024];
    FILE *f = fopen(path, "rb");
    ASSERT_NOT_NULL(f);
    long size = (long)fread(data, 1, sizeof(data), f);
    fclose(f);

    long first_on = find_message(data, size, 0, 0x90, 60);
    long second_on = find_message(data, size, first_on + 2, 0x90, 60);
    long first_off = find_message(data, size, 0, 0x80, 60);
    ASSERT_TRUE(first_on >= 0 && second_on >= 0 && first_off >= 0);
    ASSERT_LT(first_off, second_on);

    delete_test_file(path);
    shared_midi_events_cleanup();
}

TEST(export_melody) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* C major scale */
    int notes[] = {60, 62, 64, 65, 67, 69, 71, 72};
    int tick = 0;

    for (int i = 0; i < 8; i++) {
        shared_midi_events_note_on(tick, 0, notes[i], 100);
        shared_midi_events_note_off(tick + 240, 0, notes[i]);
        tick += 480;
    }

    char path[256];
    build_test_path(path, sizeof(path), "melody");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

TEST(export_with_program_change) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Set program to acoustic guitar (25) */
    shared_midi_events_program(0, 0, 25);
    shared_midi_events_note_on(0, 0, 60, 80);
    shared_midi_events_note_off(480, 0, 60);

    char path[256];
    build_test_path(path, sizeof(path), "program");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

TEST(export_with_tempo) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Tempo change then notes */
    shared_midi_events_tempo(0, 140);
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_off(480, 0, 60);

    char path[256];
    build_test_path(path, sizeof(path), "tempo");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

TEST(export_with_cc) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Volume CC before note */
    shared_midi_events_cc(0, 0, 7, 100);  /* CC7 = volume */
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_off(480, 0, 60);

    char path[256];
    build_test_path(path, sizeof(path), "cc");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

/* ============================================================================
 * Multi Channel (Type 1) Tests
 * ============================================================================ */

TEST(export_two_channels) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Piano on channel 0, bass on channel 1 */
    shared_midi_events_program(0, 0, 0);   /* Piano */
    shared_midi_events_program(0, 1, 32);  /* Bass */

    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_on(0, 1, 36, 80);
    shared_midi_events_note_off(480, 0, 60);
    shared_midi_events_note_off(480, 1, 36);

    char path[256];
    build_test_path(path, sizeof(path), "two_channels");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);

    /* File should exist and be valid MIDI (Type 1 multi-track) */
    ASSERT_TRUE(file_exists(path));
    ASSERT_TRUE(is_valid_midi_file(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

TEST(export_multi_channel) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Three channels: piano, guitar, bass */
    shared_midi_events_program(0, 0, 0);
    shared_midi_events_program(0, 1, 25);
    shared_midi_events_program(0, 2, 32);

    /* Chord: C E G with different instruments */
    shared_midi_events_note_on(0, 0, 60, 100);
    shared_midi_events_note_on(0, 1, 64, 90);
    shared_midi_events_note_on(0, 2, 48, 80);

    shared_midi_events_note_off(960, 0, 60);
    shared_midi_events_note_off(960, 1, 64);
    shared_midi_events_note_off(960, 2, 48);

    char path[256];
    build_test_path(path, sizeof(path), "multi_channel");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

/* ============================================================================
 * Complex Composition Tests
 * ============================================================================ */

TEST(export_unsorted_events) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Add events out of order (as might happen in real use) */
    shared_midi_events_note_off(480, 0, 60);  /* Off at tick 480 */
    shared_midi_events_note_on(0, 0, 60, 100); /* On at tick 0 */

    /* Sort before export */
    shared_midi_events_sort();

    char path[256];
    build_test_path(path, sizeof(path), "unsorted");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    delete_test_file(path);
    shared_midi_events_cleanup();
}

TEST(export_many_events) {
    int result = shared_midi_events_init(480);
    ASSERT_EQ(result, 0);

    /* Generate a longer piece */
    int tick = 0;
    for (int bar = 0; bar < 16; bar++) {
        /* Four quarter notes per bar */
        for (int beat = 0; beat < 4; beat++) {
            int note = 48 + (bar % 12);  /* Walk up the scale */
            shared_midi_events_note_on(tick, 0, note, 100);
            shared_midi_events_note_off(tick + 240, 0, note);
            tick += 480;
        }
    }

    char path[256];
    build_test_path(path, sizeof(path), "many_events");
    delete_test_file(path);

    result = loki_midi_export_shared(path);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(file_exists(path));

    /* Verify we exported 128 events (64 notes * 2) */
    ASSERT_EQ(shared_midi_events_count(), 128);

    delete_test_file(path);
    shared_midi_events_cleanup();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

BEGIN_TEST_SUITE("MIDI Export Tests")

    /* Error Handling */
    RUN_TEST(export_empty_buffer);
    RUN_TEST(export_null_filename);
    RUN_TEST(export_empty_filename);

    /* Single Channel (Type 0) */
    RUN_TEST(export_single_note);
    RUN_TEST(export_note_off_before_note_on_at_same_tick);
    RUN_TEST(export_melody);
    RUN_TEST(export_with_program_change);
    RUN_TEST(export_with_tempo);
    RUN_TEST(export_with_cc);

    /* Multi Channel (Type 1) */
    RUN_TEST(export_two_channels);
    RUN_TEST(export_multi_channel);

    /* Complex */
    RUN_TEST(export_unsorted_events);
    RUN_TEST(export_many_events);

END_TEST_SUITE()
