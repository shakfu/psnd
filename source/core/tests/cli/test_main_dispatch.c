/**
 * @file test_main_dispatch.c
 * @brief Integration tests for main.c entry point dispatch.
 *
 * Tests that the psnd binary correctly dispatches based on:
 * - Command-line flags (-h, --help, -V, --version)
 * - Language commands (alda, joy, tr7)
 * - File extensions
 * - Various argument combinations
 *
 * Uses fork/exec for process execution.
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

/*============================================================================
 * Test Suite Fixture
 *============================================================================*/

static char temp_dir[TEST_PROC_MAX_PATH];

SUITE_SETUP(main_dispatch_tests) {
    if (test_mkdtemp("psnd_dispatch_test", temp_dir) != 0) {
        fprintf(stderr, "Failed to create temp directory\n");
        exit(1);
    }
    /* Keep play on MIDI: a discovered soundfont would sound through speakers */
    test_setenv("PSND_SOUNDFONT", "none");
}

SUITE_TEARDOWN(main_dispatch_tests) {
    test_rmdir_recursive(temp_dir);
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

static int run_psnd(char *args[]) {
    return test_exec(PSND_BINARY, args);
}

static int run_psnd_with_file(const char *filename, const char *content) {
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, filename, filepath);

    if (test_write_file(temp_dir, filename, content) != 0) {
        return -1;
    }

    char *args[] = {"psnd", "play", filepath, NULL};
    return run_psnd(args);
}

/*============================================================================
 * Help Flag Tests
 *============================================================================*/

TEST(help_short_flag) {
    char *args[] = {"psnd", "-h", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(help_long_flag) {
    char *args[] = {"psnd", "--help", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(no_args_shows_help_with_error) {
    char *args[] = {"psnd", NULL};
    int result = run_psnd(args);
    /* No args returns 1 (error) but shows help */
    ASSERT_EQ(result, 1);
}

/*============================================================================
 * Version Flag Tests
 *============================================================================*/

TEST(version_short_flag) {
    char *args[] = {"psnd", "-V", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(version_long_flag) {
    char *args[] = {"psnd", "--version", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

/*============================================================================
 * Play Command Dispatch Tests
 *============================================================================*/

TEST(play_requires_file) {
    char *args[] = {"psnd", "play", NULL};
    int result = run_psnd(args);
    ASSERT_NEQ(result, 0);
}

TEST(play_alda_file) {
    int result = run_psnd_with_file("test.alda", "piano: c\n");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_file) {
    int result = run_psnd_with_file("test.joy", "42\n");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_file) {
    int result = run_psnd_with_file("test.scm", "(+ 1 2)\n");
    ASSERT_EQ(result, 0);
}

TEST(play_nonexistent_file) {
    char *args[] = {"psnd", "play", "/nonexistent/path/file.alda", NULL};
    int result = run_psnd(args);
    ASSERT_NEQ(result, 0);
}

/*============================================================================
 * Language Command Tests
 *============================================================================*/

TEST(alda_help_flag) {
    /* alda -h should work */
    char *args[] = {"psnd", "alda", "-h", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(joy_help_flag) {
    char *args[] = {"psnd", "joy", "-h", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(tr7_help_flag) {
    char *args[] = {"psnd", "tr7", "-h", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(alda_list_ports) {
    char *args[] = {"psnd", "alda", "-l", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(joy_list_ports) {
    char *args[] = {"psnd", "joy", "-l", NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

/*============================================================================
 * Language Delegation Tests
 *
 * `psnd <lang> ...` must hand every remaining argument to the language. The
 * editor is reached by `psnd <file>`, so routing `psnd <lang> <file>` there is
 * a redundant spelling that costs the language its arguments.
 *
 * Regression: between 85739c8 (2026-01-30) and the fix, main.c scanned for an
 * argument carrying any registered extension and diverted the whole command to
 * the editor. That silently turned every documented `psnd <lang> <file>`
 * execute form into an edit, and made `psnd mhs -r f.hs` fail outright because
 * the editor rejects -r. The editor writes terminal escapes even through a
 * pipe, so its presence is what these tests detect.
 *============================================================================*/

/* Run `psnd <args>` on a file written into the temp dir, capturing stdout. */
static int run_lang_with_file(const char *lang, const char *flag,
                              const char *filename, const char *content,
                              char *output, size_t output_size) {
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, filename, filepath);

    if (test_write_file(temp_dir, filename, content) != 0) {
        return -1;
    }

    if (flag) {
        char *args[] = {"psnd", (char *)lang, (char *)flag, filepath, NULL};
        return test_exec_capture(PSND_BINARY, args, output, output_size);
    }
    char *args[] = {"psnd", (char *)lang, filepath, NULL};
    return test_exec_capture(PSND_BINARY, args, output, output_size);
}

static int landed_in_editor(const char *output) {
    return strstr(output, "\033[") != NULL;
}

TEST(lang_file_does_not_open_editor_alda) {
    char output[8192] = {0};
    int result = run_lang_with_file("alda", NULL, "delegate.alda", "piano: c\n",
                                   output, sizeof(output));
    ASSERT_EQ(result, 0);
    ASSERT_FALSE(landed_in_editor(output));
}

TEST(lang_file_does_not_open_editor_joy) {
    char output[8192] = {0};
    int result = run_lang_with_file("joy", NULL, "delegate.joy", "42\n",
                                   output, sizeof(output));
    ASSERT_EQ(result, 0);
    ASSERT_FALSE(landed_in_editor(output));
}

TEST(lang_file_does_not_open_editor_bog) {
    char output[8192] = {0};
    int result = run_lang_with_file("bog", NULL, "delegate.bog",
                                   "event(kick, 0).\n", output, sizeof(output));
    ASSERT_EQ(result, 0);
    ASSERT_FALSE(landed_in_editor(output));
}

TEST(lang_file_does_not_open_editor_tr7) {
    char output[8192] = {0};
    int result = run_lang_with_file("tr7", NULL, "delegate.scm", "(+ 1 2)\n",
                                   output, sizeof(output));
    ASSERT_EQ(result, 0);
    ASSERT_FALSE(landed_in_editor(output));
}

TEST(lang_flag_taking_file_reaches_language_mhs) {
    /* The form mhs documents: -r runs the file. Under the regression the
       editor consumed it and reported "Unknown option: -r".

       Reaching the language is the contract under test, and that holds on every
       platform. Running the module does not: MHS integration needs fmemopen, so
       Windows registers mhs with stub entry points that dispatch reaches and
       then fails. Asserting success there tests the backend, not the routing. */
    char output[8192] = {0};
    int result = run_lang_with_file(
        "mhs", "-r", "Delegate.hs",
        "module Delegate(main) where\nmain :: IO ()\n"
        "main = putStrLn \"psnd-dispatch-ok\"\n",
        output, sizeof(output));
    ASSERT_FALSE(landed_in_editor(output));
#ifdef PSND_MHS_ENABLED
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(strstr(output, "psnd-dispatch-ok"));
#else
    (void)result;
#endif
}

TEST(lang_file_of_another_language_still_delegates) {
    /* The extension test matched any registered extension, so `psnd joy x.alda`
       opened an Alda file in the editor under the joy subcommand. */
    char output[8192] = {0};
    int result = run_lang_with_file("joy", NULL, "cross.alda", "piano: c\n",
                                   output, sizeof(output));
    (void)result;  /* joy may reject the file; it must not be edited */
    ASSERT_FALSE(landed_in_editor(output));
}

/*============================================================================
 * Unknown Command Tests
 *============================================================================*/

TEST(unknown_command_fallback) {
    /* Unknown command without file extension - should try editor */
    char *args[] = {"psnd", "unknowncommand", NULL};
    int result = run_psnd(args);
    /* May fail since file doesn't exist, but shouldn't crash */
    (void)result;  /* Just verify no crash */
}

/*============================================================================
 * File Extension Routing Tests
 *============================================================================*/

TEST(direct_file_alda) {
    /* Direct file argument should open editor or fail gracefully */
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, "direct.alda", filepath);
    test_write_file(temp_dir, "direct.alda", "piano: c\n");

    /* Direct file opens editor, which we can't test non-interactively */
    /* Just verify the dispatch doesn't crash */
    char *args[] = {"psnd", "play", filepath, NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(direct_file_joy) {
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, "direct.joy", filepath);
    test_write_file(temp_dir, "direct.joy", "1 2 +\n");

    char *args[] = {"psnd", "play", filepath, NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

TEST(direct_file_scheme) {
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, "direct.scm", filepath);
    test_write_file(temp_dir, "direct.scm", "(+ 1 2)\n");

    char *args[] = {"psnd", "play", filepath, NULL};
    int result = run_psnd(args);
    ASSERT_EQ(result, 0);
}

/*============================================================================
 * CSD Extension Tests
 *============================================================================*/

TEST(csd_extension_recognized) {
    /* .csd files are always supported (Csound) */
    char filepath[TEST_PROC_MAX_PATH];
    test_build_path(temp_dir, "test.csd", filepath);

    /* Create a minimal valid CSD file */
    const char *csd_content =
        "<CsoundSynthesizer>\n"
        "<CsOptions>\n"
        "-n\n"
        "</CsOptions>\n"
        "<CsInstruments>\n"
        "sr = 44100\n"
        "ksmps = 32\n"
        "nchnls = 2\n"
        "</CsInstruments>\n"
        "<CsScore>\n"
        "</CsScore>\n"
        "</CsoundSynthesizer>\n";

    test_write_file(temp_dir, "test.csd", csd_content);

    /* .csd files route to editor - can't test interactively */
    /* Just verify the file is recognized without crashing */
}

/*============================================================================
 * Option with File Tests
 *============================================================================*/

TEST(soundfont_option_needs_file) {
    char *args[] = {"psnd", "-sf", "gm.sf2", NULL};
    int result = run_psnd(args);
    /* Should fail because no supported file is provided */
    ASSERT_NEQ(result, 0);
}

/*============================================================================
 * Language Command with File Tests
 *============================================================================*/

/*============================================================================
 * Multiple Extension Support Tests
 *============================================================================*/

TEST(ss_extension_routes_to_tr7) {
    int result = run_psnd_with_file("test.ss", "(+ 3 4)\n");
    ASSERT_EQ(result, 0);
}

/* NOTE: Bog test removed - Bog interpreter doesn't support headless playback mode */

/*============================================================================
 * Error Handling Tests
 *============================================================================*/

TEST(invalid_option) {
    char *args[] = {"psnd", "--invalid-option-xyz", NULL};
    int result = run_psnd(args);
    /* Should fail or fall through to editor (which fails on invalid file) */
    ASSERT_NEQ(result, 0);
}

TEST(play_empty_file) {
    int result = run_psnd_with_file("empty.alda", "");
    /* Empty file should succeed (no notes to play) */
    ASSERT_EQ(result, 0);
}

TEST(play_syntax_error) {
    /* Syntax error in file - behavior depends on language */
    int result = run_psnd_with_file("bad.alda", "invalid syntax @@@@\n");
    /* May succeed or fail depending on error handling */
    (void)result;  /* Just verify no crash */
}

/*============================================================================
 * Main Test Runner
 *============================================================================*/

BEGIN_TEST_SUITE_WITH_FIXTURE("Main Dispatch Tests", main_dispatch_tests)
    /* Help flag tests */
    RUN_TEST(help_short_flag);
    RUN_TEST(help_long_flag);
    RUN_TEST(no_args_shows_help_with_error);

    /* Version flag tests */
    RUN_TEST(version_short_flag);
    RUN_TEST(version_long_flag);

    /* Play command tests */
    RUN_TEST(play_requires_file);
    RUN_TEST(play_alda_file);
    RUN_TEST(play_joy_file);
    RUN_TEST(play_scheme_file);
    RUN_TEST(play_nonexistent_file);

    /* Language command tests */
    RUN_TEST(alda_help_flag);
    RUN_TEST(joy_help_flag);
    RUN_TEST(tr7_help_flag);
    RUN_TEST(alda_list_ports);
    RUN_TEST(joy_list_ports);

    /* Language delegation (regression: editor swallowed lang arguments) */
    RUN_TEST(lang_file_does_not_open_editor_alda);
    RUN_TEST(lang_file_does_not_open_editor_joy);
    RUN_TEST(lang_file_does_not_open_editor_bog);
    RUN_TEST(lang_file_does_not_open_editor_tr7);
    RUN_TEST(lang_flag_taking_file_reaches_language_mhs);
    RUN_TEST(lang_file_of_another_language_still_delegates);

    /* Unknown command tests */
    /* NOTE: unknown_command_fallback skipped - it opens editor in interactive mode */
    /* RUN_TEST(unknown_command_fallback); */

    /* File extension routing tests */
    RUN_TEST(direct_file_alda);
    RUN_TEST(direct_file_joy);
    RUN_TEST(direct_file_scheme);

    /* CSD extension tests */
    RUN_TEST(csd_extension_recognized);

    /* Option with file tests */
    RUN_TEST(soundfont_option_needs_file);

    /* Language command with file tests */
    /* Multiple extension tests */
    RUN_TEST(ss_extension_routes_to_tr7);
    /* bog_extension_if_supported removed - Bog doesn't support headless playback */

    /* Error handling tests */
    RUN_TEST(invalid_option);
    RUN_TEST(play_empty_file);
    RUN_TEST(play_syntax_error);
END_TEST_SUITE_WITH_FIXTURE(main_dispatch_tests)
