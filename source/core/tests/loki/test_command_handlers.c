/**
 * @file test_command_handlers.c
 * @brief Tests for ex-command handlers (goto, substitute, etc.)
 *
 * Tests individual command implementations from command/*.c files.
 * These tests focus on the command logic rather than command mode UI.
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/command.h"
#include "loki/buffers.h"
#include <string.h>
#include <stdlib.h>

/* Command handlers (from command/*.c) */
extern int cmd_goto(editor_ctx_t *ctx, const char *args);
extern int cmd_substitute(editor_ctx_t *ctx, const char *args);
extern int cmd_set(editor_ctx_t *ctx, const char *args);
extern int cmd_help(editor_ctx_t *ctx, const char *args);

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

/* Initialize minimal editor context for testing */
static void init_test_ctx(editor_ctx_t *ctx) {
    editor_ctx_init(ctx);
    buffers_init(ctx);
    command_mode_init(ctx);
    ctx->view.screenrows = 24;
    ctx->view.screencols = 80;
}

/* Create context with multiple lines of content */
static void init_test_ctx_with_lines(editor_ctx_t *ctx, const char **lines, int num_lines) {
    init_test_ctx(ctx);

    ctx->model.numrows = num_lines;
    ctx->model.row = calloc(num_lines, sizeof(t_erow));

    for (int i = 0; i < num_lines; i++) {
        ctx->model.row[i].chars = strdup(lines[i]);
        ctx->model.row[i].size = strlen(lines[i]);
        ctx->model.row[i].render = strdup(lines[i]);
        ctx->model.row[i].rsize = strlen(lines[i]);
        ctx->model.row[i].hl = NULL;
        ctx->model.row[i].idx = i;
    }
}

/* Free test context */
static void free_test_ctx(editor_ctx_t *ctx) {
    command_mode_free(ctx);
    buffers_free();
    editor_ctx_free(ctx);
}

/* Get content of a specific row */
static const char* get_row_content(editor_ctx_t *ctx, int row) {
    if (row < 0 || row >= ctx->model.numrows) return NULL;
    return ctx->model.row[row].chars;
}

/* ============================================================================
 * cmd_goto Tests
 * ============================================================================ */

TEST(goto_basic_navigation) {
    const char *lines[] = {"line 1", "line 2", "line 3", "line 4", "line 5"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 5);

    ctx.view.cy = 0;

    int result = cmd_goto(&ctx, "3");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.cy, 2);  /* Line 3 = index 2 */
    ASSERT_EQ(ctx.view.cx, 0);

    free_test_ctx(&ctx);
}

TEST(goto_first_line) {
    const char *lines[] = {"first", "second", "third"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 3);

    ctx.view.cy = 2;

    int result = cmd_goto(&ctx, "1");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.cy, 0);

    free_test_ctx(&ctx);
}

TEST(goto_last_line) {
    const char *lines[] = {"first", "second", "third"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 3);

    ctx.view.cy = 0;

    int result = cmd_goto(&ctx, "3");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.cy, 2);

    free_test_ctx(&ctx);
}

TEST(goto_clamps_to_max) {
    const char *lines[] = {"one", "two", "three"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 3);

    /* Try to go to line 100 when only 3 lines exist */
    int result = cmd_goto(&ctx, "100");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.cy, 2);  /* Clamped to last line */

    free_test_ctx(&ctx);
}

TEST(goto_rejects_zero) {
    const char *lines[] = {"line"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    int result = cmd_goto(&ctx, "0");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(goto_rejects_negative) {
    const char *lines[] = {"line"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    int result = cmd_goto(&ctx, "-5");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(goto_rejects_empty_args) {
    const char *lines[] = {"line"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    int result = cmd_goto(&ctx, NULL);
    ASSERT_EQ(result, 0);

    result = cmd_goto(&ctx, "");
    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(goto_rejects_non_numeric) {
    const char *lines[] = {"line"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    /* atoi("abc") returns 0, which is invalid */
    int result = cmd_goto(&ctx, "abc");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(goto_rejects_trailing_garbage) {
    const char *lines[] = {"one", "two", "three"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 3);

    ASSERT_EQ(cmd_goto(&ctx, "2abc"), 0);
    ASSERT_EQ(cmd_goto(&ctx, "99999999999999999999"), 0);
    ASSERT_EQ(cmd_goto(&ctx, "2 "), 1);

    free_test_ctx(&ctx);
}

TEST(goto_adjusts_scroll_down) {
    const char *lines[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                           "11", "12", "13", "14", "15", "16", "17", "18", "19", "20",
                           "21", "22", "23", "24", "25", "26", "27", "28", "29", "30"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 30);

    ctx.view.rowoff = 0;
    ctx.view.screenrows = 10;

    cmd_goto(&ctx, "25");

    /* view.cy is screen-relative: the cursor's file row is rowoff + cy. */
    ASSERT_TRUE(ctx.view.rowoff > 0);
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 24);
    ASSERT_TRUE(ctx.view.cy >= 0);
    ASSERT_TRUE(ctx.view.cy < ctx.view.screenrows);

    free_test_ctx(&ctx);
}

TEST(goto_adjusts_scroll_up) {
    const char *lines[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                           "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 20);

    ctx.view.rowoff = 15;
    ctx.view.screenrows = 10;
    ctx.view.cy = 18;

    cmd_goto(&ctx, "3");

    /* Scroll should have adjusted to show line 3 */
    ASSERT_TRUE(ctx.view.rowoff <= 2);
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 2);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_substitute Tests
 * ============================================================================ */

TEST(substitute_basic_replacement) {
    const char *lines[] = {"hello world"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/world/universe/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "hello universe");

    free_test_ctx(&ctx);
}

TEST(substitute_first_occurrence_only) {
    const char *lines[] = {"foo foo foo"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/foo/bar/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "bar foo foo");

    free_test_ctx(&ctx);
}

TEST(substitute_global_flag) {
    const char *lines[] = {"foo foo foo"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/foo/bar/g");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "bar bar bar");

    free_test_ctx(&ctx);
}

TEST(substitute_empty_replacement) {
    const char *lines[] = {"remove this word"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    /* Replace "this " (including trailing space) with empty string */
    int result = cmd_substitute(&ctx, "s/this //");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "remove word");

    free_test_ctx(&ctx);
}

TEST(substitute_delete_pattern) {
    const char *lines[] = {"remove---dashes"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/---//");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "removedashes");

    free_test_ctx(&ctx);
}

TEST(substitute_pattern_not_found) {
    const char *lines[] = {"hello world"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/xyz/abc/");

    ASSERT_EQ(result, 0);
    /* Original line unchanged */
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "hello world");

    free_test_ctx(&ctx);
}

TEST(substitute_empty_search_pattern) {
    const char *lines[] = {"test"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s//replacement/");

    ASSERT_EQ(result, 0);  /* Empty search pattern is invalid */

    free_test_ctx(&ctx);
}

TEST(substitute_invalid_format) {
    const char *lines[] = {"test"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    /* Missing second delimiter */
    int result = cmd_substitute(&ctx, "s/old");
    ASSERT_EQ(result, 0);

    /* Wrong prefix */
    result = cmd_substitute(&ctx, "r/old/new/");
    ASSERT_EQ(result, 0);

    /* NULL args */
    result = cmd_substitute(&ctx, NULL);
    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(substitute_escaped_delimiter) {
    const char *lines[] = {"path/to/file"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    /* Replace / with \ using escaped delimiter */
    int result = cmd_substitute(&ctx, "s/\\//\\\\/g");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "path\\to\\file");

    free_test_ctx(&ctx);
}

TEST(substitute_marks_dirty) {
    const char *lines[] = {"change me"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;
    ctx.model.dirty = 0;

    cmd_substitute(&ctx, "s/me/you/");

    ASSERT_TRUE(ctx.model.dirty > 0);

    free_test_ctx(&ctx);
}

TEST(substitute_longer_replacement) {
    const char *lines[] = {"a b c"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/b/much longer text/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "a much longer text c");

    free_test_ctx(&ctx);
}

TEST(substitute_shorter_replacement) {
    const char *lines[] = {"very long text here"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/very long text/x/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "x here");

    free_test_ctx(&ctx);
}

TEST(substitute_at_line_start) {
    const char *lines[] = {"prefix rest of line"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/prefix/NEW/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "NEW rest of line");

    free_test_ctx(&ctx);
}

TEST(substitute_at_line_end) {
    const char *lines[] = {"beginning suffix"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/suffix/END/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "beginning END");

    free_test_ctx(&ctx);
}

TEST(substitute_entire_line) {
    const char *lines[] = {"replace all"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/replace all/completely new/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "completely new");

    free_test_ctx(&ctx);
}

TEST(substitute_on_specific_line) {
    const char *lines[] = {"line 1", "target line", "line 3"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 3);

    ctx.view.cy = 1;  /* Position on second line */

    int result = cmd_substitute(&ctx, "s/target/modified/");

    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 0), "line 1");
    ASSERT_STR_EQ(get_row_content(&ctx, 1), "modified line");
    ASSERT_STR_EQ(get_row_content(&ctx, 2), "line 3");

    free_test_ctx(&ctx);
}

TEST(substitute_no_line_to_substitute) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    ctx.model.numrows = 0;
    ctx.view.cy = 0;

    int result = cmd_substitute(&ctx, "s/a/b/");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_set Tests
 * ============================================================================ */

TEST(set_toggle_wrap) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    ctx.view.word_wrap = 0;

    int result = cmd_set(&ctx, "wrap");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.word_wrap, 1);

    /* Toggle back */
    result = cmd_set(&ctx, "wrap");
    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.word_wrap, 0);

    free_test_ctx(&ctx);
}

TEST(set_unknown_option) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_set(&ctx, "nonexistent");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(set_no_args_shows_options) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_set(&ctx, NULL);

    ASSERT_EQ(result, 1);
    /* Should show available options */
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);

    result = cmd_set(&ctx, "");
    ASSERT_EQ(result, 1);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_help Tests
 * ============================================================================ */

TEST(help_no_args_shows_general) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_help(&ctx, NULL);

    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);
    /* Should contain basic info */
    ASSERT_TRUE(strstr(ctx.view.statusmsg, ":") != NULL);

    free_test_ctx(&ctx);
}

TEST(help_specific_command) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_help(&ctx, "w");

    ASSERT_EQ(result, 1);
    /* Should show write command help */
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);

    free_test_ctx(&ctx);
}

TEST(help_unknown_command) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_help(&ctx, "notarealcommand");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_write / cmd_edit Tests (file.c)
 * ============================================================================ */

/* Command handlers from file.c */
extern int cmd_write(editor_ctx_t *ctx, const char *args);
extern int cmd_edit(editor_ctx_t *ctx, const char *args);

TEST(write_no_filename) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    ctx.model.filename = NULL;

    int result = cmd_write(&ctx, NULL);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "filename") != NULL ||
                strstr(ctx.view.statusmsg, "No") != NULL);

    free_test_ctx(&ctx);
}

TEST(edit_requires_filename) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_edit(&ctx, NULL);
    ASSERT_EQ(result, 0);

    result = cmd_edit(&ctx, "");
    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(edit_rejects_dirty_buffer) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    ctx.model.dirty = 1;

    int result = cmd_edit(&ctx, "somefile.txt");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "Unsaved") != NULL ||
                strstr(ctx.view.statusmsg, "unsaved") != NULL);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_export Tests (export.c)
 * ============================================================================ */

extern int cmd_export(editor_ctx_t *ctx, const char *args);

TEST(export_requires_filename) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_export(&ctx, NULL);
    ASSERT_EQ(result, 0);

    result = cmd_export(&ctx, "");
    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_tap / cmd_metronome Tests (metronome.c)
 * ============================================================================ */

extern int cmd_tap(editor_ctx_t *ctx, const char *args);
extern int cmd_metronome(editor_ctx_t *ctx, const char *args);

TEST(tap_reset) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_tap(&ctx, "reset");

    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "reset") != NULL);

    free_test_ctx(&ctx);
}

TEST(tap_first_tap) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    /* Reset first */
    cmd_tap(&ctx, "reset");

    /* First tap should just record */
    int result = cmd_tap(&ctx, NULL);

    ASSERT_EQ(result, 1);
    /* Should show "Tap... (1)" or similar */
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "1") != NULL);

    free_test_ctx(&ctx);
}

TEST(metronome_toggle) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    /* Toggle on */
    int result = cmd_metronome(&ctx, NULL);
    ASSERT_EQ(result, 1);

    /* Toggle off */
    result = cmd_metronome(&ctx, NULL);
    ASSERT_EQ(result, 1);

    free_test_ctx(&ctx);
}

TEST(metronome_on_off) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_metronome(&ctx, "on");
    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "ON") != NULL);

    result = cmd_metronome(&ctx, "off");
    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "OFF") != NULL);

    free_test_ctx(&ctx);
}

TEST(metronome_subdivisions) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    /* Set to eighth notes */
    int result = cmd_metronome(&ctx, "2");
    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "eighth") != NULL);

    /* Set to sixteenth notes */
    result = cmd_metronome(&ctx, "4");
    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "sixteenth") != NULL);

    free_test_ctx(&ctx);
}

TEST(metronome_invalid_subdivision) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_metronome(&ctx, "99");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_link Tests (link.c)
 * ============================================================================ */

extern int cmd_link(editor_ctx_t *ctx, const char *args);

TEST(link_invalid_args) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_link(&ctx, "invalid");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "Usage") != NULL);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_loop / cmd_unloop Tests (loop.c)
 * ============================================================================ */

extern int cmd_loop(editor_ctx_t *ctx, const char *args);
extern int cmd_unloop(editor_ctx_t *ctx, const char *args);

TEST(loop_invalid_beats) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_loop(&ctx, "0");
    ASSERT_EQ(result, 0);

    result = cmd_loop(&ctx, "-5");
    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

TEST(unloop_no_active_loop) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    /* Should succeed even if no loop is active */
    int result = cmd_unloop(&ctx, NULL);

    ASSERT_EQ(result, 1);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_csd Tests (csd.c)
 * ============================================================================ */

extern int cmd_csd(editor_ctx_t *ctx, const char *args);

TEST(csd_invalid_args) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_csd(&ctx, "invalid");

    ASSERT_EQ(result, 0);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_theme Tests (theme.c)
 * ============================================================================ */

extern int cmd_theme(editor_ctx_t *ctx, const char *args);

TEST(theme_list_available) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    /* No args should list themes */
    int result = cmd_theme(&ctx, NULL);

    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);

    free_test_ctx(&ctx);
}

TEST(theme_unknown) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_theme(&ctx, "nonexistent_theme_12345");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "Unknown") != NULL ||
                strstr(ctx.view.statusmsg, "unknown") != NULL ||
                strstr(ctx.view.statusmsg, "not") != NULL);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * cmd_plugin Tests (plugin.c)
 * ============================================================================ */

extern int cmd_plugin(editor_ctx_t *ctx, const char *args);

TEST(plugin_no_plugin_loaded) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    /* Without a plugin, should fail gracefully */
    int result = cmd_plugin(&ctx, NULL);

    /* Either plugin support not compiled, or no plugin loaded */
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);

    free_test_ctx(&ctx);
}

TEST(plugin_invalid_subcommand) {
    editor_ctx_t ctx;
    init_test_ctx(&ctx);

    int result = cmd_plugin(&ctx, "invalid_subcommand");

    /* Should fail with usage message */
    ASSERT_TRUE(result == 0 || strlen(ctx.view.statusmsg) > 0);

    free_test_ctx(&ctx);
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

test_stats_t test_stats;

TEST(substitute_long_line_not_truncated) {
    /* A fixed 4096-byte buffer used to cut the tail off longer lines. */
    char *line = malloc(5004);
    memset(line, 'a', 5000);
    memcpy(line + 5000, "foo", 4);
    const char *lines[] = {line};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 1);

    ASSERT_EQ(cmd_substitute(&ctx, "s/foo/barbaz/"), 1);
    ASSERT_EQ(ctx.model.row[0].size, 5006);
    ASSERT_STR_EQ(get_row_content(&ctx, 0) + 5000, "barbaz");

    free_test_ctx(&ctx);
    free(line);
}

TEST(substitute_uses_file_row_when_scrolled) {
    const char *lines[] = {"zero", "one", "two", "three"};
    editor_ctx_t ctx;
    init_test_ctx_with_lines(&ctx, lines, 4);

    ctx.view.rowoff = 2;
    ctx.view.cy = 1;   /* file row 3 */

    ASSERT_EQ(cmd_substitute(&ctx, "s/three/THREE/"), 1);
    ASSERT_STR_EQ(get_row_content(&ctx, 3), "THREE");
    ASSERT_STR_EQ(get_row_content(&ctx, 1), "one");

    free_test_ctx(&ctx);
}

BEGIN_TEST_SUITE("Command Handlers")

    /* Goto tests */
    RUN_TEST(goto_basic_navigation);
    RUN_TEST(goto_first_line);
    RUN_TEST(goto_last_line);
    RUN_TEST(goto_clamps_to_max);
    RUN_TEST(goto_rejects_zero);
    RUN_TEST(goto_rejects_negative);
    RUN_TEST(goto_rejects_empty_args);
    RUN_TEST(goto_rejects_non_numeric);
    RUN_TEST(goto_rejects_trailing_garbage);
    RUN_TEST(goto_adjusts_scroll_down);
    RUN_TEST(goto_adjusts_scroll_up);

    /* Substitute tests */
    RUN_TEST(substitute_basic_replacement);
    RUN_TEST(substitute_first_occurrence_only);
    RUN_TEST(substitute_global_flag);
    RUN_TEST(substitute_empty_replacement);
    RUN_TEST(substitute_delete_pattern);
    RUN_TEST(substitute_pattern_not_found);
    RUN_TEST(substitute_empty_search_pattern);
    RUN_TEST(substitute_invalid_format);
    RUN_TEST(substitute_escaped_delimiter);
    RUN_TEST(substitute_marks_dirty);
    RUN_TEST(substitute_longer_replacement);
    RUN_TEST(substitute_shorter_replacement);
    RUN_TEST(substitute_at_line_start);
    RUN_TEST(substitute_at_line_end);
    RUN_TEST(substitute_entire_line);
    RUN_TEST(substitute_on_specific_line);
    RUN_TEST(substitute_no_line_to_substitute);
    RUN_TEST(substitute_long_line_not_truncated);
    RUN_TEST(substitute_uses_file_row_when_scrolled);

    /* Set tests */
    RUN_TEST(set_toggle_wrap);
    RUN_TEST(set_unknown_option);
    RUN_TEST(set_no_args_shows_options);

    /* Help tests */
    RUN_TEST(help_no_args_shows_general);
    RUN_TEST(help_specific_command);
    RUN_TEST(help_unknown_command);

    /* File command tests (file.c) */
    RUN_TEST(write_no_filename);
    RUN_TEST(edit_requires_filename);
    RUN_TEST(edit_rejects_dirty_buffer);

    /* Export tests (export.c) */
    RUN_TEST(export_requires_filename);

    /* Tap/Metronome tests (metronome.c) */
    RUN_TEST(tap_reset);
    RUN_TEST(tap_first_tap);
    RUN_TEST(metronome_toggle);
    RUN_TEST(metronome_on_off);
    RUN_TEST(metronome_subdivisions);
    RUN_TEST(metronome_invalid_subdivision);

    /* Link tests (link.c) */
    RUN_TEST(link_invalid_args);

    /* Loop tests (loop.c) */
    RUN_TEST(loop_invalid_beats);
    RUN_TEST(unloop_no_active_loop);

    /* Csound tests (csd.c) */
    RUN_TEST(csd_invalid_args);

    /* Theme tests (theme.c) */
    RUN_TEST(theme_list_available);
    RUN_TEST(theme_unknown);

    /* Plugin tests (plugin.c) */
    RUN_TEST(plugin_no_plugin_loaded);
    RUN_TEST(plugin_invalid_subcommand);

END_TEST_SUITE()
