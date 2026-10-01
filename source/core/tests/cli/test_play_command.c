/**
 * @file test_play_command.c
 * @brief Integration tests for psnd play command.
 *
 * Tests that `psnd play <file>` correctly routes to the appropriate
 * language handler based on file extension.
 *
 * Uses fork/exec for process execution instead of system() for:
 * - Proper exit code capture
 * - No shell dependency
 * - Better portability
 */

/* Required for nftw() and FTW_DEPTH/FTW_PHYS - must be before any includes */
#define _XOPEN_SOURCE 500

#include "test_framework.h"
#include "test_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Path to psnd binary - set by CMake */
#ifndef PSND_BINARY
#define PSND_BINARY "./psnd"
#endif

/* =============================================================================
 * Test Suite Fixture
 * =============================================================================
 */

static char temp_dir[TEST_PROC_MAX_PATH];

SUITE_SETUP(play_tests) {
    if (test_mkdtemp("psnd_test", temp_dir) != 0) {
        fprintf(stderr, "Failed to create temp directory\n");
        exit(1);
    }
    /* Keep play on MIDI: a discovered soundfont would sound through speakers */
    test_setenv("PSND_SOUNDFONT", "none");
}

SUITE_TEARDOWN(play_tests) {
    test_rmdir_recursive(temp_dir);
}

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

static int run_psnd_play(const char *filename) {
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, filename, filepath);

    char *args[] = {
        "psnd",
        "play",
        filepath,
        NULL
    };

    return test_exec(PSND_BINARY, args);
}

static int run_psnd_play_verbose(const char *filename) {
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, filename, filepath);

    char *args[] = {
        "psnd",
        "play",
        "-v",
        filepath,
        NULL
    };

    return test_exec(PSND_BINARY, args);
}

/* =============================================================================
 * Joy Play Tests
 * =============================================================================
 */

TEST(play_joy_simple_expression) {
    /* Simple Joy expression that just pushes a value */
    ASSERT_EQ(test_write_file(temp_dir, "test.joy", "42\n"), 0);
    int result = run_psnd_play("test.joy");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_arithmetic) {
    /* Joy arithmetic expression */
    ASSERT_EQ(test_write_file(temp_dir, "arith.joy", "2 3 + 4 *\n"), 0);
    int result = run_psnd_play("arith.joy");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_define) {
    /* Joy DEFINE statement */
    ASSERT_EQ(test_write_file(temp_dir, "define.joy", "DEFINE square == dup *;\n5 square\n"), 0);
    int result = run_psnd_play("define.joy");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_nonexistent_file) {
    /* Attempting to play a nonexistent file should fail */
    int result = run_psnd_play("nonexistent.joy");
    ASSERT_NEQ(result, 0);
}

TEST(play_joy_verbose_flag) {
    /* Verbose flag should work */
    ASSERT_EQ(test_write_file(temp_dir, "verbose.joy", "1 2 +\n"), 0);
    int result = run_psnd_play_verbose("verbose.joy");
    ASSERT_EQ(result, 0);
}

/* =============================================================================
 * TR7/Scheme Play Tests
 * =============================================================================
 */

TEST(play_scheme_simple_expression) {
    /* Simple Scheme expression */
    ASSERT_EQ(test_write_file(temp_dir, "test.scm", "(+ 1 2)\n"), 0);
    int result = run_psnd_play("test.scm");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_define) {
    /* Scheme define */
    ASSERT_EQ(test_write_file(temp_dir, "define.scm", "(define x 42)\nx\n"), 0);
    int result = run_psnd_play("define.scm");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_lambda) {
    /* Scheme lambda */
    ASSERT_EQ(test_write_file(temp_dir, "lambda.scm", "((lambda (x) (* x x)) 5)\n"), 0);
    int result = run_psnd_play("lambda.scm");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_nonexistent_file) {
    /* Attempting to play a nonexistent file should fail */
    int result = run_psnd_play("nonexistent.scm");
    ASSERT_NEQ(result, 0);
}

TEST(play_scheme_ss_extension) {
    /* .ss extension should also work for Scheme */
    ASSERT_EQ(test_write_file(temp_dir, "test.ss", "(+ 3 4)\n"), 0);
    int result = run_psnd_play("test.ss");
    ASSERT_EQ(result, 0);
}

/* =============================================================================
 * Alda Play Tests
 * =============================================================================
 */

TEST(play_alda_simple_note) {
    /* Simple Alda note - uses no_sleep for fast execution */
    ASSERT_EQ(test_write_file(temp_dir, "test.alda", "piano: c\n"), 0);
    int result = run_psnd_play("test.alda");
    ASSERT_EQ(result, 0);
}

TEST(play_alda_chord) {
    /* Alda chord */
    ASSERT_EQ(test_write_file(temp_dir, "chord.alda", "piano: c/e/g\n"), 0);
    int result = run_psnd_play("chord.alda");
    ASSERT_EQ(result, 0);
}

TEST(play_alda_nonexistent_file) {
    /* Attempting to play a nonexistent file should fail */
    int result = run_psnd_play("nonexistent.alda");
    ASSERT_NEQ(result, 0);
}

/* =============================================================================
 * Error Cases
 * =============================================================================
 */

/* =============================================================================
 * Option Placement
 * =============================================================================
 */

TEST(play_options_before_filename) {
    /* Regression: the play dispatcher located the file argument to select the
     * language, then forwarded argv starting AT the file. Options preceding the
     * filename (-v, -sf) were silently dropped, so `psnd play -sf x.sf2 y.alda`
     * played to no output at all. Exit status stayed 0, so only the verbose
     * banner reveals whether the flag survived dispatch. */
    char filepath[TEST_PROC_MAX_PATH];
    ASSERT_EQ(test_write_file(temp_dir, "optorder.joy", "42\n"), 0);
    test_build_path(temp_dir, "optorder.joy", filepath);

    char *args[] = {"psnd", "play", "-v", filepath, NULL};
    char output[4096];
    int result = test_exec_capture(PSND_BINARY, args, output, sizeof(output));

    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(strstr(output, "Executing:"));
}

TEST(play_no_file_arg) {
    /* psnd play without file should fail */
    char *args[] = {"psnd", "play", NULL};
    int result = test_exec(PSND_BINARY, args);
    ASSERT_NEQ(result, 0);
}

TEST(play_unknown_extension) {
    /* Unknown extension should fail or fallback */
    ASSERT_EQ(test_write_file(temp_dir, "test.xyz", "hello\n"), 0);
    int result = run_psnd_play("test.xyz");
    /* May succeed with fallback or fail - just verify it doesn't crash */
    (void)result;
}

/* =============================================================================
 * Test Suite
 * =============================================================================
 */

BEGIN_TEST_SUITE_WITH_FIXTURE("psnd play command", play_tests)

    /* Joy tests */
    RUN_TEST(play_joy_simple_expression);
    RUN_TEST(play_joy_arithmetic);
    RUN_TEST(play_joy_define);
    RUN_TEST(play_joy_nonexistent_file);
    RUN_TEST(play_joy_verbose_flag);

    /* TR7/Scheme tests */
    RUN_TEST(play_scheme_simple_expression);
    RUN_TEST(play_scheme_define);
    RUN_TEST(play_scheme_lambda);
    RUN_TEST(play_scheme_nonexistent_file);
    RUN_TEST(play_scheme_ss_extension);

    /* Alda tests */
    RUN_TEST(play_alda_simple_note);
    RUN_TEST(play_alda_chord);
    RUN_TEST(play_alda_nonexistent_file);

    /* Option placement */
    RUN_TEST(play_options_before_filename);

    /* Error cases */
    RUN_TEST(play_no_file_arg);
    RUN_TEST(play_unknown_extension);

END_TEST_SUITE_WITH_FIXTURE(play_tests)
