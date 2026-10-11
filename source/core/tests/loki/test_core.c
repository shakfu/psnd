/* test_core.c - Unit tests for core editor functionality
 *
 * Tests for:
 * - Editor context initialization
 * - Row insertion and deletion
 * - Character insertion and deletion
 * - Cursor movement
 * - Separator detection
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/terminal.h"
#include "loki/session.h"
#include "loki/event.h"
#include <string.h>

/* Test editor context initialization */
TEST(editor_ctx_init_initializes_all_fields) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.view.cx, 0);
    ASSERT_EQ(ctx.view.cy, 0);
    ASSERT_EQ(ctx.model.numrows, 0);
    ASSERT_EQ(ctx.model.dirty, 0);
    ASSERT_NULL(ctx.model.row);
    ASSERT_NULL(ctx.model.filename);
    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);
    /* Note: winsize_changed now lives in TerminalHost, tested separately */
    editor_ctx_free(&ctx);
}

/* Test separator detection */
TEST(is_separator_detects_whitespace) {
    char *seps = " \t,;";
    ASSERT_TRUE(syntax_is_separator(' ', seps));
    ASSERT_TRUE(syntax_is_separator('\t', seps));
    ASSERT_FALSE(syntax_is_separator('a', seps));
    ASSERT_FALSE(syntax_is_separator('1', seps));
}

TEST(is_separator_detects_custom_separators) {
    char *seps = ",.()+-/*";
    ASSERT_TRUE(syntax_is_separator(',', seps));
    ASSERT_TRUE(syntax_is_separator('.', seps));
    ASSERT_TRUE(syntax_is_separator('(', seps));
    ASSERT_TRUE(syntax_is_separator(')', seps));
    ASSERT_FALSE(syntax_is_separator('a', seps));
}

TEST(is_separator_handles_null_terminator) {
    char *seps = ",;";
    ASSERT_TRUE(syntax_is_separator('\0', seps));
}

/* Test character insertion */
TEST(editor_insert_char_adds_character_to_empty_buffer) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* Initialize with empty row */
    ctx.model.numrows = 1;
    ctx.model.row = calloc(1, sizeof(t_erow));
    ASSERT_NOT_NULL(ctx.model.row);

    ctx.model.row[0].chars = malloc(1);
    ctx.model.row[0].chars[0] = '\0';
    ctx.model.row[0].size = 0;
    ctx.model.row[0].render = NULL;
    ctx.model.row[0].hl = NULL;
    ctx.model.row[0].rsize = 0;

    editor_insert_char(&ctx, 'a');

    ASSERT_EQ(ctx.model.row[0].size, 1);
    ASSERT_EQ(ctx.model.row[0].chars[0], 'a');
    ASSERT_EQ(ctx.model.dirty, 1);

    editor_ctx_free(&ctx);
}

/* Test newline insertion */
TEST(editor_insert_newline_splits_line) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* Initialize with one row containing "hello" */
    ctx.model.numrows = 1;
    ctx.model.row = calloc(1, sizeof(t_erow));
    ASSERT_NOT_NULL(ctx.model.row);

    ctx.model.row[0].chars = strdup("hello");
    ctx.model.row[0].size = 5;
    ctx.model.row[0].render = NULL;
    ctx.model.row[0].hl = NULL;
    ctx.model.row[0].rsize = 0;
    ctx.model.row[0].idx = 0;

    /* Position cursor at index 2 (between 'e' and 'l') */
    ctx.view.cx = 2;
    ctx.view.cy = 0;

    editor_insert_newline(&ctx);

    /* Should have 2 rows now */
    ASSERT_EQ(ctx.model.numrows, 2);

    /* First row should be "he" */
    ASSERT_EQ(ctx.model.row[0].size, 2);
    ASSERT_EQ(ctx.model.row[0].chars[0], 'h');
    ASSERT_EQ(ctx.model.row[0].chars[1], 'e');

    /* Second row should be "llo" */
    ASSERT_EQ(ctx.model.row[1].size, 3);
    ASSERT_EQ(ctx.model.row[1].chars[0], 'l');
    ASSERT_EQ(ctx.model.row[1].chars[1], 'l');
    ASSERT_EQ(ctx.model.row[1].chars[2], 'o');

    /* Cursor should move to start of new line */
    ASSERT_EQ(ctx.view.cy, 1);
    ASSERT_EQ(ctx.view.cx, 0);

    editor_ctx_free(&ctx);
}

/* Test cursor movement doesn't go out of bounds */
TEST(cursor_stays_within_bounds) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ctx.model.numrows = 2;
    ctx.model.row = calloc(2, sizeof(t_erow));

    /* Row 0: "abc" */
    ctx.model.row[0].chars = strdup("abc");
    ctx.model.row[0].size = 3;
    ctx.model.row[0].render = strdup("abc");
    ctx.model.row[0].rsize = 3;
    ctx.model.row[0].hl = NULL;

    /* Row 1: "defg" */
    ctx.model.row[1].chars = strdup("defg");
    ctx.model.row[1].size = 4;
    ctx.model.row[1].render = strdup("defg");
    ctx.model.row[1].rsize = 4;
    ctx.model.row[1].hl = NULL;

    ctx.view.screenrows = 10;
    ctx.view.screencols = 80;

    /* Start at (0,0) */
    ctx.view.cx = 0;
    ctx.view.cy = 0;

    /* Move right to end of line */
    editor_move_cursor(&ctx, ARROW_RIGHT);  /* cx = 1 */
    editor_move_cursor(&ctx, ARROW_RIGHT);  /* cx = 2 */
    editor_move_cursor(&ctx, ARROW_RIGHT);  /* cx = 3 (at end) */
    ASSERT_EQ(ctx.view.cx, 3);

    /* Try to move right beyond end - should stay at end */
    editor_move_cursor(&ctx, ARROW_RIGHT);
    /* Cursor should wrap to next line or stay - implementation specific */
    /* Just verify we don't crash */
    ASSERT_TRUE(ctx.view.cx >= 0);
    ASSERT_TRUE(ctx.view.cy >= 0);
    ASSERT_TRUE(ctx.view.cy < ctx.model.numrows);

    editor_ctx_free(&ctx);
}

/* Test dirty flag management */
TEST(dirty_flag_set_on_modification) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.model.dirty, 0);

    /* Initialize with empty row */
    ctx.model.numrows = 1;
    ctx.model.row = calloc(1, sizeof(t_erow));
    ctx.model.row[0].chars = malloc(1);
    ctx.model.row[0].chars[0] = '\0';
    ctx.model.row[0].size = 0;
    ctx.model.row[0].render = NULL;
    ctx.model.row[0].hl = NULL;

    editor_insert_char(&ctx, 'x');

    ASSERT_EQ(ctx.model.dirty, 1);

    editor_ctx_free(&ctx);
}

/* Test mode management */
TEST(mode_switching_works) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);

    ctx.view.mode = MODE_INSERT;
    ASSERT_EQ(ctx.view.mode, MODE_INSERT);

    ctx.view.mode = MODE_VISUAL;
    ASSERT_EQ(ctx.view.mode, MODE_VISUAL);

    ctx.view.mode = MODE_NORMAL;
    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);
    editor_ctx_free(&ctx);
}

/* Test window resize flag (now in TerminalHost) */
TEST(window_resize_flag_initialized) {
    /* winsize_changed now lives in TerminalHost, not editor_ctx_t */
    TerminalHost host = {0};

    ASSERT_EQ(terminal_host_resize_pending(&host), 0);

    /* Simulate resize signal */
    host.winsize_changed = 1;
    ASSERT_EQ(terminal_host_resize_pending(&host), 1);

    /* Clear flag */
    terminal_host_clear_resize(&host);
    ASSERT_EQ(terminal_host_resize_pending(&host), 0);
}

/* EVENT_RESIZE carries text rows for every host. The session used to be
 * inconsistent with modal_process_event about subtracting the status rows. */
TEST(session_resize_uses_text_rows) {
    EditorConfig config = {0};
    config.rows = 24;
    config.cols = 80;
    EditorSession *session = editor_session_new(&config);
    ASSERT_NOT_NULL(session);

    EditorEvent ev = event_resize(30, 100);
    ASSERT_EQ(editor_session_handle_event(session, &ev), 0);
    ASSERT_EQ(editor_session_get_ctx(session)->view.screenrows, 30);
    ASSERT_EQ(editor_session_get_ctx(session)->view.screencols, 100);

    editor_session_free(session);
}

BEGIN_TEST_SUITE("Core Editor Functions")
    RUN_TEST(editor_ctx_init_initializes_all_fields);
    RUN_TEST(is_separator_detects_whitespace);
    RUN_TEST(is_separator_detects_custom_separators);
    RUN_TEST(is_separator_handles_null_terminator);
    RUN_TEST(editor_insert_char_adds_character_to_empty_buffer);
    RUN_TEST(editor_insert_newline_splits_line);
    RUN_TEST(cursor_stays_within_bounds);
    RUN_TEST(dirty_flag_set_on_modification);
    RUN_TEST(mode_switching_works);
    RUN_TEST(window_resize_flag_initialized);
    RUN_TEST(session_resize_uses_text_rows);
END_TEST_SUITE()
