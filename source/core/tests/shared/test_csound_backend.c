/**
 * @file test_csound_backend.c
 * @brief Tests for Csound audio backend.
 *
 * Tests verify:
 * - Backend availability check
 * - Initialization and cleanup
 * - State queries before/after initialization
 * - CSD/orchestra loading (error cases)
 * - Enable/disable ref counting
 * - MIDI message sending (note on/off, program, CC, pitch bend)
 * - All notes off
 *
 * Note: Full audio output testing requires manual verification.
 * These tests verify the API behaves correctly without crashing.
 *
 * Build with -DBUILD_CSOUND_BACKEND=ON to enable full Csound tests.
 * Without Csound, tests verify stub behavior (all functions return safely).
 */

#include "test_framework.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <uv.h>
#include "audio/audio.h"

/* ============================================================================
 * Availability Test
 * ============================================================================ */

TEST(csound_availability) {
    /* This should return consistent value based on build config */
    int available = shared_csound_is_available();

#ifdef BUILD_CSOUND_BACKEND
    ASSERT_TRUE(available);
#else
    ASSERT_FALSE(available);
#endif
}

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(csound_init_cleanup) {
    /* Init should succeed (or return -1 if not available) */
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);
    }
    /* If not available, result may be 0 (stub) or -1 */

    /* Cleanup should not crash */
    shared_csound_cleanup();

    /* Double cleanup should not crash */
    shared_csound_cleanup();
}

TEST(csound_state_before_init) {
    /* Ensure clean state */
    shared_csound_cleanup();

    /* Should report no instruments */
    ASSERT_FALSE(shared_csound_has_instruments());

    /* Should report not enabled */
    ASSERT_FALSE(shared_csound_is_enabled());

    /* Sample rate should be 0 or default */
    int sr = shared_csound_get_sample_rate();
    ASSERT_TRUE(sr == 0 || sr > 0);  /* Either 0 or valid sample rate */

    /* Channels should be 0 or default */
    int ch = shared_csound_get_channels();
    ASSERT_TRUE(ch == 0 || ch > 0);
}

TEST(csound_state_after_init_no_instruments) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);

        /* Should report no instruments */
        ASSERT_FALSE(shared_csound_has_instruments());

        /* Should report not enabled */
        ASSERT_FALSE(shared_csound_is_enabled());
    }

    shared_csound_cleanup();
}

/* ============================================================================
 * Loading Tests
 * ============================================================================ */

TEST(csound_load_null_path) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);

        /* Loading NULL path should fail */
        result = shared_csound_load(NULL);
        ASSERT_EQ(result, -1);
    }

    shared_csound_cleanup();
}

TEST(csound_load_nonexistent_file) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);

        /* Loading nonexistent file should fail */
        result = shared_csound_load("/nonexistent/path/to/file.csd");
        ASSERT_EQ(result, -1);

        /* Should still report no instruments */
        ASSERT_FALSE(shared_csound_has_instruments());
    }

    shared_csound_cleanup();
}

TEST(csound_load_without_init) {
    /* Ensure clean state */
    shared_csound_cleanup();

    /* Loading without init should fail or be safe */
    int result = shared_csound_load("/some/path.csd");
    /* Result depends on implementation - just shouldn't crash */
    (void)result;
}

TEST(csound_compile_null_orc) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);

        /* Compiling NULL orchestra should fail */
        result = shared_csound_compile_orc(NULL);
        ASSERT_EQ(result, -1);
    }

    shared_csound_cleanup();
}

TEST(csound_compile_invalid_orc) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);

        /* Compiling invalid orchestra should fail */
        result = shared_csound_compile_orc("this is not valid csound code @@##$$");
        ASSERT_EQ(result, -1);
    }

    shared_csound_cleanup();
}

/* ============================================================================
 * Enable/Disable Tests
 * ============================================================================ */

TEST(csound_enable_without_instruments) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);

        /* Enable without instruments should fail */
        result = shared_csound_enable();
        ASSERT_EQ(result, -1);

        /* Should still be disabled */
        ASSERT_FALSE(shared_csound_is_enabled());
    }

    shared_csound_cleanup();
}

TEST(csound_disable_without_enable) {
    int result = shared_csound_init();

    if (shared_csound_is_available()) {
        ASSERT_EQ(result, 0);
    }

    /* Disable without enable should not crash */
    shared_csound_disable();

    /* Should still be disabled */
    ASSERT_FALSE(shared_csound_is_enabled());

    shared_csound_cleanup();
}

TEST(csound_disable_without_init) {
    /* Ensure clean state */
    shared_csound_cleanup();

    /* Disable without init should not crash */
    shared_csound_disable();
}

/* ============================================================================
 * MIDI Message Tests (without instruments - should not crash)
 * ============================================================================ */

TEST(csound_note_on_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* Note on without instruments should not crash */
    shared_csound_send_note_on(1, 60, 100);

    shared_csound_cleanup();
}

TEST(csound_note_on_freq_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* Note on with frequency without instruments should not crash */
    shared_csound_send_note_on_freq(1, 440.0, 100, 69);

    shared_csound_cleanup();
}

TEST(csound_note_off_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* Note off without instruments should not crash */
    shared_csound_send_note_off(1, 60);

    shared_csound_cleanup();
}

TEST(csound_program_change_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* Program change without instruments should not crash */
    shared_csound_send_program(1, 0);

    shared_csound_cleanup();
}

TEST(csound_cc_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* CC without instruments should not crash */
    shared_csound_send_cc(1, 7, 100);   /* Volume */
    shared_csound_send_cc(1, 10, 64);   /* Pan */
    shared_csound_send_cc(1, 1, 64);    /* Modulation */

    shared_csound_cleanup();
}

TEST(csound_pitch_bend_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* Pitch bend without instruments should not crash */
    shared_csound_send_pitch_bend(1, 0);      /* Center */
    shared_csound_send_pitch_bend(1, 8191);   /* Max up */
    shared_csound_send_pitch_bend(1, -8192);  /* Max down */

    shared_csound_cleanup();
}

TEST(csound_all_notes_off_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    /* All notes off without instruments should not crash */
    shared_csound_all_notes_off();

    shared_csound_cleanup();
}

/* ============================================================================
 * MIDI Message Tests (without init - should not crash)
 * ============================================================================ */

TEST(csound_messages_without_init) {
    /* Ensure clean state */
    shared_csound_cleanup();

    /* All these should not crash even without init */
    shared_csound_send_note_on(1, 60, 100);
    shared_csound_send_note_on_freq(1, 440.0, 100, 69);
    shared_csound_send_note_off(1, 60);
    shared_csound_send_program(1, 0);
    shared_csound_send_cc(1, 7, 100);
    shared_csound_send_pitch_bend(1, 0);
    shared_csound_all_notes_off();
}

/* ============================================================================
 * Channel/Pitch/Velocity Boundary Tests
 * ============================================================================ */

TEST(csound_channel_boundaries) {
    int result = shared_csound_init();
    (void)result;

    /* Test channel boundaries (1-16 are valid MIDI channels) */
    shared_csound_send_note_on(1, 60, 100);   /* Min channel */
    shared_csound_send_note_on(16, 60, 100);  /* Max channel */
    shared_csound_send_note_on(0, 60, 100);   /* Below min */
    shared_csound_send_note_on(17, 60, 100);  /* Above max */

    shared_csound_cleanup();
}

TEST(csound_pitch_boundaries) {
    int result = shared_csound_init();
    (void)result;

    /* Test pitch boundaries (0-127 are valid MIDI notes) */
    shared_csound_send_note_on(1, 0, 100);    /* Min pitch */
    shared_csound_send_note_on(1, 127, 100);  /* Max pitch */
    shared_csound_send_note_on(1, -1, 100);   /* Below min */
    shared_csound_send_note_on(1, 128, 100);  /* Above max */

    shared_csound_cleanup();
}

TEST(csound_velocity_boundaries) {
    int result = shared_csound_init();
    (void)result;

    /* Test velocity boundaries (0-127 are valid) */
    shared_csound_send_note_on(1, 60, 0);     /* Min velocity */
    shared_csound_send_note_on(1, 60, 127);   /* Max velocity */
    shared_csound_send_note_on(1, 60, -1);    /* Below min */
    shared_csound_send_note_on(1, 60, 128);   /* Above max */

    shared_csound_cleanup();
}

TEST(csound_frequency_boundaries) {
    int result = shared_csound_init();
    (void)result;

    /* Test frequency boundaries */
    shared_csound_send_note_on_freq(1, 20.0, 100, 0);      /* Low freq */
    shared_csound_send_note_on_freq(1, 20000.0, 100, 127); /* High freq */
    shared_csound_send_note_on_freq(1, 0.0, 100, 60);      /* Zero freq */
    shared_csound_send_note_on_freq(1, -1.0, 100, 60);     /* Negative freq */

    shared_csound_cleanup();
}

/* ============================================================================
 * Render Test (without instruments)
 * ============================================================================ */

TEST(csound_render_without_instruments) {
    int result = shared_csound_init();
    (void)result;

    float buffer[1024] = {0};

    /* Render without instruments should produce silence or not crash */
    shared_csound_render(buffer, 512);

    shared_csound_cleanup();
}

/* ============================================================================
 * Error Message Test
 * ============================================================================ */

TEST(csound_error_message) {
    /* Error message should return NULL or a string, never crash */
    const char* err = shared_csound_get_error();
    (void)err;  /* May be NULL or a message */
}

/* ============================================================================
 * Playback Control Tests
 * ============================================================================ */

TEST(csound_playback_state) {
    /* Playback active should return false when nothing is playing */
    ASSERT_FALSE(shared_csound_playback_active());

    /* Stop playback should not crash even when nothing is playing */
    shared_csound_stop_playback();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */


/* ============================================================================
 * Realtime Render Contention
 *
 * shared_csound_render() runs on the miniaudio device thread. The engine mutex
 * is also taken by control-thread compiles, which are slow: a 600-instrument
 * orchestra parses in roughly 30ms against a 23.2ms audio period. Blocking on
 * that mutex in the audio callback misses the deadline and underruns the
 * device, so the render path try-locks and emits silence instead.
 *
 * These tests need a working playback device to put the engine in its enabled,
 * actively-rendering state. Where none is available (headless CI) they report
 * that and return rather than failing.
 * ============================================================================ */

/* Build a large orchestra: one long parse while holding the engine mutex. */
static char *build_big_orc(int generation, size_t *out_len) {
    size_t cap = 256u * 1024u;
    char *orc = malloc(cap);
    if (!orc) return NULL;

    size_t off = 0;
    for (int i = 0; i < 1200; i++) {
        off += (size_t)snprintf(orc + off, cap - off,
            "instr %d\n"
            "k1 line 1, p3, 0\n"
            "a1 oscil 0.05*k1, %d\n"
            "a2 oscil 0.05*k1, %d\n"
            "out (a1+a2)*0.5\n"
            "endin\n",
            1000 + generation * 10000 + i, 100 + i, 300 + i);
    }
    if (out_len) *out_len = off;
    return orc;
}

/* Bring the engine up to the enabled, rendering state.
 * Returns 1 if it is live, 0 if no audio device is available. */
static int csound_engine_start(void) {
    if (shared_csound_init() != 0) return 0;
    if (shared_csound_compile_orc(
            "instr 1\na1 oscil 0.2, 440\nout a1\nendin\n") != 0) return 0;
    shared_csound_enable();
    return shared_csound_is_enabled() ? 1 : 0;
}

TEST(csound_render_skip_count_safe_before_init) {
    shared_csound_cleanup();
    /* Must be callable in any state without crashing. */
    long skips = shared_csound_render_skip_count();
    ASSERT_TRUE(skips >= 0);
}

TEST(csound_render_skips_rather_than_blocking_on_compile) {
    if (!csound_engine_start()) {
        printf("    (no audio device: skipping contention check)\n");
        shared_csound_cleanup();
        return;
    }

    /* The device thread is now calling shared_csound_render continuously.
     * Hold the engine mutex with slow compiles and confirm the render path
     * takes the try-lock miss branch instead of stalling. */
    long before = shared_csound_render_skip_count();

    for (int gen = 0; gen < 3; gen++) {
        size_t len = 0;
        char *orc = build_big_orc(gen, &len);
        ASSERT_NOT_NULL(orc);
        shared_csound_compile_orc(orc);
        free(orc);
    }

    long after = shared_csound_render_skip_count();

    /* Each compile outlasts an audio period, so the device thread must have hit
     * the busy mutex at least once. Under the previous cs_mutex_lock() it would
     * have blocked here instead of counting a skip. */
    ASSERT_TRUE(after > before);

    /* And the engine recovers: still enabled, still rendering. */
    ASSERT_EQ(shared_csound_is_enabled(), 1);

    shared_csound_cleanup();
}

/* Background compiler: holds the engine mutex in long bursts. */
static volatile int g_compiler_done = 0;
static double g_min_hold_ms = 0.0;

static void compile_thread_fn(void *arg) {
    (void)arg;
    g_min_hold_ms = 1e9;
    for (int gen = 0; gen < 3; gen++) {
        size_t len = 0;
        char *orc = build_big_orc(100 + gen, &len);
        if (orc) {
            /* compile_orc holds the mutex for its whole duration. */
            uint64_t t0 = uv_hrtime();
            shared_csound_compile_orc(orc);
            double ms = (double)(uv_hrtime() - t0) / 1e6;
            if (ms < g_min_hold_ms) g_min_hold_ms = ms;
            free(orc);
        }
    }
    g_compiler_done = 1;
}

TEST(csound_render_returns_promptly_while_compiling) {
    if (!csound_engine_start()) {
        printf("    (no audio device: skipping latency check)\n");
        shared_csound_cleanup();
        return;
    }

    /* Compile on another thread so the render calls below genuinely race a held
     * mutex. Doing the compile inline would never contend with itself, and the
     * test would pass even with a blocking lock. */
    g_compiler_done = 0;
    uv_thread_t compiler;
    ASSERT_EQ(uv_thread_create(&compiler, compile_thread_fn, NULL), 0);

    float buffer[1024];
    double worst_ms = 0.0;
    long samples = 0;

    while (!g_compiler_done) {
        /* uv_hrtime rather than clock_gettime: monotonic on every platform we
         * build for, and MSVC has no CLOCK_MONOTONIC. */
        uint64_t t0 = uv_hrtime();
        shared_csound_render(buffer, 512);
        uint64_t t1 = uv_hrtime();

        double ms = (double)(t1 - t0) / 1e6;
        if (ms > worst_ms) worst_ms = ms;
        samples++;
    }

    uv_thread_join(&compiler);

    /* A blocking lock stalls one render for nearly a full compile, so bound
     * against the measured hold, not a fixed time. A fixed 15ms bound failed on
     * Windows CI: one preemption costs a 15.6ms scheduler quantum. */
    printf("    (%ld render calls, worst %.2f ms, shortest hold %.2f ms)\n",
           samples, worst_ms, g_min_hold_ms);
    ASSERT_TRUE(samples > 0);
    ASSERT_TRUE(worst_ms < 0.75 * g_min_hold_ms);

    shared_csound_cleanup();
}


BEGIN_TEST_SUITE("Csound Backend Tests")

    /* Availability */
    RUN_TEST(csound_availability);

    /* Initialization */
    RUN_TEST(csound_init_cleanup);
    RUN_TEST(csound_state_before_init);
    RUN_TEST(csound_state_after_init_no_instruments);

    /* Loading */
    RUN_TEST(csound_load_null_path);
    RUN_TEST(csound_load_nonexistent_file);
    RUN_TEST(csound_load_without_init);
    RUN_TEST(csound_compile_null_orc);
    RUN_TEST(csound_compile_invalid_orc);

    /* Enable/disable */
    RUN_TEST(csound_enable_without_instruments);
    RUN_TEST(csound_disable_without_enable);
    RUN_TEST(csound_disable_without_init);

    /* MIDI messages without instruments */
    RUN_TEST(csound_note_on_without_instruments);
    RUN_TEST(csound_note_on_freq_without_instruments);
    RUN_TEST(csound_note_off_without_instruments);
    RUN_TEST(csound_program_change_without_instruments);
    RUN_TEST(csound_cc_without_instruments);
    RUN_TEST(csound_pitch_bend_without_instruments);
    RUN_TEST(csound_all_notes_off_without_instruments);

    /* MIDI messages without init */
    RUN_TEST(csound_messages_without_init);

    /* Boundary tests */
    RUN_TEST(csound_channel_boundaries);
    RUN_TEST(csound_pitch_boundaries);
    RUN_TEST(csound_velocity_boundaries);
    RUN_TEST(csound_frequency_boundaries);

    /* Render */
    RUN_TEST(csound_render_without_instruments);

    /* Error handling */
    RUN_TEST(csound_error_message);

    /* Playback control */
    RUN_TEST(csound_playback_state);

    /* Realtime render contention */
    RUN_TEST(csound_render_skip_count_safe_before_init);
    RUN_TEST(csound_render_skips_rather_than_blocking_on_compile);
    RUN_TEST(csound_render_returns_promptly_while_compiling);
END_TEST_SUITE()
