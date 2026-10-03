/**
 * @file test_repl_commands.c
 * @brief Tests for the shared REPL command processor.
 *
 * Tests the shared_process_command() function and related utilities.
 */

#include "test_framework.h"
#include "repl_commands.h"
#include "context.h"
#include "link/link.h"
#include <string.h>

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

/* Callback tracking for stop_callback */
static int g_stop_callback_count = 0;

static void mock_stop_callback(void) {
    g_stop_callback_count++;
}

/* Reset test state */
static void reset_test_state(void) {
    g_stop_callback_count = 0;
    /* Clean up Link if it was initialized */
    if (shared_link_is_initialized()) {
        shared_link_cleanup();
    }
}

/* ============================================================================
 * Quit Command Tests
 * ============================================================================ */

TEST(repl_cmd_quit_with_colon) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":quit", NULL);
    ASSERT_EQ(result, REPL_CMD_QUIT);
}

TEST(repl_cmd_quit_without_colon) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, "quit", NULL);
    ASSERT_EQ(result, REPL_CMD_QUIT);
}

TEST(repl_cmd_exit) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":exit", NULL);
    ASSERT_EQ(result, REPL_CMD_QUIT);
}

TEST(repl_cmd_q_short) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":q", NULL);
    ASSERT_EQ(result, REPL_CMD_QUIT);
}

/* ============================================================================
 * Help Command Tests (delegated to language)
 * ============================================================================ */

TEST(repl_cmd_help_delegated) {
    reset_test_state();
    SharedContext ctx = {0};
    /* Help is delegated to language REPLs - should return NOT_CMD */
    int result = shared_process_command(&ctx, ":help", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_h_delegated) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":h", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_question_delegated) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":?", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

/* ============================================================================
 * Stop/Panic Command Tests
 * ============================================================================ */

TEST(repl_cmd_stop_calls_callback) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":stop", mock_stop_callback);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(g_stop_callback_count, 1);
}

TEST(repl_cmd_stop_short) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":s", mock_stop_callback);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(g_stop_callback_count, 1);
}

TEST(repl_cmd_panic_calls_callback) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":panic", mock_stop_callback);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(g_stop_callback_count, 1);
}

TEST(repl_cmd_panic_short) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":p", mock_stop_callback);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(g_stop_callback_count, 1);
}

TEST(repl_cmd_stop_null_callback) {
    reset_test_state();
    SharedContext ctx = {0};
    /* Should not crash with NULL callback */
    int result = shared_process_command(&ctx, ":stop", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

/* ============================================================================
 * List Command Tests
 * ============================================================================ */

TEST(repl_cmd_list) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":list", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    shared_context_cleanup(&ctx);
}

TEST(repl_cmd_list_short) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":l", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    shared_context_cleanup(&ctx);
}

/* ============================================================================
 * Language Switching Tests
 * ============================================================================ */

TEST(repl_cmd_langs) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":langs", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_languages) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":languages", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_lang_empty) {
    reset_test_state();
    SharedContext ctx = {0};
    /* Empty lang name should print usage, not crash */
    int result = shared_process_command(&ctx, ":lang ", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

/* Note: :lang NAME actually calls execlp, so we can't fully test it in unit tests */

/* ============================================================================
 * Synth Command Tests
 * ============================================================================ */

TEST(repl_cmd_synth_no_soundfont) {
    reset_test_state();
    SharedContext ctx = {0};
    /* Should print message about no soundfont */
    int result = shared_process_command(&ctx, ":synth", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_builtin_no_soundfont) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":builtin", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_midi_switch) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.builtin_synth_enabled = 1;
    int result = shared_process_command(&ctx, ":midi", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(ctx.builtin_synth_enabled, 0);
}

TEST(repl_cmd_sf_empty_path) {
    reset_test_state();
    SharedContext ctx = {0};
    /* Empty path should print usage */
    int result = shared_process_command(&ctx, ":sf ", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_sf_load_empty_path) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":sf-load ", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_presets_no_soundfont) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":presets", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_sf_list_no_soundfont) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":sf-list", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

/* ============================================================================
 * Link Command Tests
 * ============================================================================ */

TEST(repl_cmd_link_enable) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.tempo = 120;
    int result = shared_process_command(&ctx, ":link", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_TRUE(shared_link_is_initialized());
    ASSERT_TRUE(shared_link_is_enabled());
    ASSERT_EQ(ctx.link_enabled, 1);
    shared_link_cleanup();
}

TEST(repl_cmd_link_on) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.tempo = 120;
    int result = shared_process_command(&ctx, ":link on", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(ctx.link_enabled, 1);
    shared_link_cleanup();
}

TEST(repl_cmd_link_off) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.tempo = 120;
    /* First enable */
    shared_process_command(&ctx, ":link", NULL);
    /* Then disable */
    int result = shared_process_command(&ctx, ":link off", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(ctx.link_enabled, 0);
    shared_link_cleanup();
}

TEST(repl_cmd_link_tempo_valid) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.tempo = 120;
    int result = shared_process_command(&ctx, ":link-tempo 140", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_TRUE(shared_link_is_initialized());
    double tempo = shared_link_get_tempo();
    ASSERT_TRUE(tempo >= 139.0 && tempo <= 141.0);
    shared_link_cleanup();
}

TEST(repl_cmd_link_tempo_too_low) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":link-tempo 10", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    /* Should print error but not crash */
}

TEST(repl_cmd_link_tempo_too_high) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":link-tempo 1000", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    /* Should print error but not crash */
}

TEST(repl_cmd_link_status_not_initialized) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":link-status", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_link_status_enabled) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.tempo = 120;
    shared_process_command(&ctx, ":link", NULL);
    int result = shared_process_command(&ctx, ":link-status", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    shared_link_cleanup();
}

/* ============================================================================
 * Csound Command Tests
 * ============================================================================ */

TEST(repl_cmd_cs_empty_path) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":cs ", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_cs_load_empty_path) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":cs-load ", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_csound_no_csd) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":csound", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_cs_enable_no_csd) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":cs-enable", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_cs_disable) {
    reset_test_state();
    SharedContext ctx = {0};
    ctx.csound_enabled = 1;
    int result = shared_process_command(&ctx, ":cs-disable", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    ASSERT_EQ(ctx.csound_enabled, 0);
}

TEST(repl_cmd_cs_status) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":cs-status", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

/* ============================================================================
 * Play Command Tests
 * ============================================================================ */

TEST(repl_cmd_play_empty_path) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":play ", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
}

TEST(repl_cmd_play_no_extension) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":play somefile", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    /* Should print error about no extension */
}

TEST(repl_cmd_play_unknown_extension) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":play file.xyz", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    /* Should print error about unknown file type */
}

TEST(repl_cmd_play_alda_delegated) {
    reset_test_state();
    SharedContext ctx = {0};
    /* .alda files are delegated to language REPL */
    int result = shared_process_command(&ctx, ":play song.alda", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_play_joy_delegated) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":play song.joy", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_play_scm_delegated) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":play song.scm", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

/* ============================================================================
 * Virtual Port Command Tests
 * ============================================================================ */

TEST(repl_cmd_virtual_default) {
    reset_test_state();
    SharedContext ctx = {0};
    /* May fail if MIDI not available, but should not crash */
    int result = shared_process_command(&ctx, ":virtual", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    shared_context_cleanup(&ctx);
}

TEST(repl_cmd_virtual_named) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":virtual MyPort", NULL);
    ASSERT_EQ(result, REPL_CMD_HANDLED);
    shared_context_cleanup(&ctx);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST(repl_cmd_null_input) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, NULL, NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_empty_input) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, "", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_whitespace_only) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, "   ", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_unknown_command) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, ":unknown", NULL);
    ASSERT_EQ(result, REPL_CMD_NOT_CMD);
}

TEST(repl_cmd_leading_whitespace) {
    reset_test_state();
    SharedContext ctx = {0};
    int result = shared_process_command(&ctx, "   :quit", NULL);
    ASSERT_EQ(result, REPL_CMD_QUIT);
}

TEST(repl_cmd_colon_stripped) {
    reset_test_state();
    SharedContext ctx = {0};
    /* Both :quit and quit should work */
    int result1 = shared_process_command(&ctx, ":quit", NULL);
    int result2 = shared_process_command(&ctx, "quit", NULL);
    ASSERT_EQ(result1, REPL_CMD_QUIT);
    ASSERT_EQ(result2, REPL_CMD_QUIT);
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

BEGIN_TEST_SUITE("Shared REPL Commands Tests")

    /* Quit commands */
    RUN_TEST(repl_cmd_quit_with_colon);
    RUN_TEST(repl_cmd_quit_without_colon);
    RUN_TEST(repl_cmd_exit);
    RUN_TEST(repl_cmd_q_short);

    /* Help (delegated) */
    RUN_TEST(repl_cmd_help_delegated);
    RUN_TEST(repl_cmd_h_delegated);
    RUN_TEST(repl_cmd_question_delegated);

    /* Stop/Panic */
    RUN_TEST(repl_cmd_stop_calls_callback);
    RUN_TEST(repl_cmd_stop_short);
    RUN_TEST(repl_cmd_panic_calls_callback);
    RUN_TEST(repl_cmd_panic_short);
    RUN_TEST(repl_cmd_stop_null_callback);

    /* List */
    RUN_TEST(repl_cmd_list);
    RUN_TEST(repl_cmd_list_short);

    /* Language switching */
    RUN_TEST(repl_cmd_langs);
    RUN_TEST(repl_cmd_languages);
    RUN_TEST(repl_cmd_lang_empty);

    /* Synth */
    RUN_TEST(repl_cmd_synth_no_soundfont);
    RUN_TEST(repl_cmd_builtin_no_soundfont);
    RUN_TEST(repl_cmd_midi_switch);
    RUN_TEST(repl_cmd_sf_empty_path);
    RUN_TEST(repl_cmd_sf_load_empty_path);
    RUN_TEST(repl_cmd_presets_no_soundfont);
    RUN_TEST(repl_cmd_sf_list_no_soundfont);

    /* Link */
    RUN_TEST(repl_cmd_link_enable);
    RUN_TEST(repl_cmd_link_on);
    RUN_TEST(repl_cmd_link_off);
    RUN_TEST(repl_cmd_link_tempo_valid);
    RUN_TEST(repl_cmd_link_tempo_too_low);
    RUN_TEST(repl_cmd_link_tempo_too_high);
    RUN_TEST(repl_cmd_link_status_not_initialized);
    RUN_TEST(repl_cmd_link_status_enabled);

    /* Csound */
    RUN_TEST(repl_cmd_cs_empty_path);
    RUN_TEST(repl_cmd_cs_load_empty_path);
    RUN_TEST(repl_cmd_csound_no_csd);
    RUN_TEST(repl_cmd_cs_enable_no_csd);
    RUN_TEST(repl_cmd_cs_disable);
    RUN_TEST(repl_cmd_cs_status);

    /* Play */
    RUN_TEST(repl_cmd_play_empty_path);
    RUN_TEST(repl_cmd_play_no_extension);
    RUN_TEST(repl_cmd_play_unknown_extension);
    RUN_TEST(repl_cmd_play_alda_delegated);
    RUN_TEST(repl_cmd_play_joy_delegated);
    RUN_TEST(repl_cmd_play_scm_delegated);

    /* Virtual port */
    RUN_TEST(repl_cmd_virtual_default);
    RUN_TEST(repl_cmd_virtual_named);

    /* Edge cases */
    RUN_TEST(repl_cmd_null_input);
    RUN_TEST(repl_cmd_empty_input);
    RUN_TEST(repl_cmd_whitespace_only);
    RUN_TEST(repl_cmd_unknown_command);
    RUN_TEST(repl_cmd_leading_whitespace);
    RUN_TEST(repl_cmd_colon_stripped);

END_TEST_SUITE()
