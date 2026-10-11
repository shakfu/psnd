/* test_undo.c - Unit tests for undo/redo system
 *
 * Tests for:
 * - Undo initialization and cleanup
 * - Character insert/delete undo
 * - Line operations undo
 * - Undo grouping heuristics
 * - Redo after undo
 * - Memory and capacity limits
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/undo.h"
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* Helper: Create simple test context with undo enabled */
static void init_ctx_with_undo(editor_ctx_t *ctx, const char *text) {
    editor_ctx_init(ctx);

    ctx->model.numrows = 1;
    ctx->model.row = calloc(1, sizeof(t_erow));
    ctx->model.row[0].chars = strdup(text);
    ctx->model.row[0].size = strlen(text);
    ctx->model.row[0].render = strdup(text);
    ctx->model.row[0].rsize = strlen(text);
    ctx->model.row[0].hl = NULL;
    ctx->model.row[0].idx = 0;

    ctx->view.screenrows = 24;
    ctx->view.screencols = 80;

    /* Replace the default undo state from editor_ctx_init */
    undo_free(ctx);
    undo_init(ctx, 100, 1024 * 1024);  /* 100 entries, 1MB */
}

/* Helper: Create multiline test context with undo */
static void init_multiline_ctx_with_undo(editor_ctx_t *ctx, int num_lines, const char **lines) {
    editor_ctx_init(ctx);

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

    ctx->view.screenrows = 24;
    ctx->view.screencols = 80;

    undo_free(ctx);
    undo_init(ctx, 100, 1024 * 1024);
}

/* Helper: Cleanup context with undo */
static void cleanup_ctx(editor_ctx_t *ctx) {
    undo_free(ctx);
    editor_ctx_free(ctx);
}

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(undo_init_creates_state) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* editor_ctx_init already calls undo_init, so state should exist */
    ASSERT_NOT_NULL(ctx.model.undo_state);

    /* Clear and reinit to test explicit init */
    undo_free(&ctx);
    ASSERT_NULL(ctx.model.undo_state);

    undo_init(&ctx, 100, 1024 * 1024);
    ASSERT_NOT_NULL(ctx.model.undo_state);

    editor_ctx_free(&ctx);
}

TEST(undo_can_undo_returns_false_initially) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    ASSERT_FALSE(undo_can_undo(&ctx));
    ASSERT_FALSE(undo_can_redo(&ctx));

    cleanup_ctx(&ctx);
}

TEST(undo_get_stats_returns_zeros_initially) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    int undo_levels, redo_levels;
    size_t memory;
    undo_get_stats(&ctx, &undo_levels, &redo_levels, &memory);

    ASSERT_EQ(undo_levels, 0);
    ASSERT_EQ(redo_levels, 0);
    ASSERT_EQ((int)memory, 0);

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Character Insert/Delete Undo Tests
 * ============================================================================ */

TEST(undo_record_insert_char_makes_undoable) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    /* Record an insert */
    undo_record_insert_char(&ctx, 0, 5, '!');

    ASSERT_TRUE(undo_can_undo(&ctx));

    int undo_levels, redo_levels;
    size_t memory;
    undo_get_stats(&ctx, &undo_levels, &redo_levels, &memory);
    ASSERT_EQ(undo_levels, 1);

    cleanup_ctx(&ctx);
}

TEST(undo_record_delete_char_makes_undoable) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    /* Record a delete */
    undo_record_delete_char(&ctx, 0, 4, 'o');

    ASSERT_TRUE(undo_can_undo(&ctx));

    cleanup_ctx(&ctx);
}

TEST(undo_performs_insert_char_undo) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello!");

    /* Record an insert at position 5 (the '!') */
    ctx.view.cx = 5;
    ctx.view.cy = 0;
    undo_record_insert_char(&ctx, 0, 5, '!');

    /* Perform undo - should delete the '!' */
    int result = undo_perform(&ctx);
    ASSERT_EQ(result, 1);

    /* Verify the character was removed */
    ASSERT_EQ(ctx.model.row[0].size, 5);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    cleanup_ctx(&ctx);
}

TEST(undo_performs_delete_char_undo) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hell");

    /* Record that 'o' was deleted from position 4 */
    ctx.view.cx = 4;
    ctx.view.cy = 0;
    undo_record_delete_char(&ctx, 0, 4, 'o');

    /* Perform undo - should re-insert 'o' */
    int result = undo_perform(&ctx);
    ASSERT_EQ(result, 1);

    /* Verify the character was restored */
    ASSERT_EQ(ctx.model.row[0].size, 5);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Redo Tests
 * ============================================================================ */

TEST(redo_after_undo_restores_change) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello!");

    /* Record an insert */
    ctx.view.cx = 5;
    ctx.view.cy = 0;
    undo_record_insert_char(&ctx, 0, 5, '!');

    /* Undo - removes the '!' */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    /* Redo - re-inserts the '!' */
    ASSERT_TRUE(undo_can_redo(&ctx));
    int result = redo_perform(&ctx);
    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello!");

    cleanup_ctx(&ctx);
}

TEST(redo_unavailable_after_new_edit) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello!");

    /* Record insert, undo it */
    undo_record_insert_char(&ctx, 0, 5, '!');
    undo_perform(&ctx);

    ASSERT_TRUE(undo_can_redo(&ctx));

    /* Make a new edit - should clear redo history */
    undo_record_insert_char(&ctx, 0, 5, '?');

    ASSERT_FALSE(undo_can_redo(&ctx));

    cleanup_ctx(&ctx);
}

TEST(undo_returns_zero_when_nothing_to_undo) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    int result = undo_perform(&ctx);
    ASSERT_EQ(result, 0);

    cleanup_ctx(&ctx);
}

TEST(redo_returns_zero_when_nothing_to_redo) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    int result = redo_perform(&ctx);
    ASSERT_EQ(result, 0);

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Undo Grouping Tests
 * ============================================================================ */

TEST(undo_groups_consecutive_inserts) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "h");

    /* Insert 'e', 'l', 'l', 'o' consecutively */
    undo_record_insert_char(&ctx, 0, 1, 'e');
    undo_record_insert_char(&ctx, 0, 2, 'l');
    undo_record_insert_char(&ctx, 0, 3, 'l');
    undo_record_insert_char(&ctx, 0, 4, 'o');

    /* All should be in one group, so one undo should remove all */
    int undo_levels, redo_levels;
    size_t memory;
    undo_get_stats(&ctx, &undo_levels, &redo_levels, &memory);
    ASSERT_EQ(undo_levels, 4);  /* 4 operations recorded */

    /* But they should all be in the same group */
    /* Single undo should undo the entire group */
    undo_perform(&ctx);

    /* After undo, row should just be "h" */
    ASSERT_STR_EQ(ctx.model.row[0].chars, "h");

    cleanup_ctx(&ctx);
}

TEST(undo_break_group_forces_new_group) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "he");

    /* Position cursor at end of "he" */
    ctx.view.cx = 2;

    /* Actually insert 'l', 'l' into the buffer */
    editor_insert_char(&ctx, 'l');  /* "hel" */
    editor_insert_char(&ctx, 'l');  /* "hell" */

    /* Force break */
    undo_break_group(&ctx);

    /* Insert 'o' in new group */
    editor_insert_char(&ctx, 'o');  /* "hello" */

    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    /* First undo should only remove 'o' */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hell");

    /* Second undo should remove 'l', 'l' */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "he");

    cleanup_ctx(&ctx);
}

TEST(undo_breaks_on_operation_type_change) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    /* Move cursor to end and insert 'X' */
    ctx.view.cx = 5;
    editor_insert_char(&ctx, 'X');  /* "helloX" */
    ASSERT_STR_EQ(ctx.model.row[0].chars, "helloX");

    /* Now delete (operation type change) - move cursor back and delete */
    ctx.view.cx = 6;  /* After 'X' */
    editor_del_char(&ctx);  /* "hello" */
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    /* Operation type change should break group */
    /* First undo should only undo the delete (restore 'X') */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "helloX");

    /* Second undo should undo the insert (remove 'X') */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    cleanup_ctx(&ctx);
}

TEST(undo_breaks_on_row_change) {
    editor_ctx_t ctx;
    const char *lines[] = {"line1", "line2"};
    init_multiline_ctx_with_undo(&ctx, 2, lines);

    /* Insert on row 0 */
    undo_record_insert_char(&ctx, 0, 5, 'X');

    /* Insert on row 1 (row change should break group) */
    undo_record_insert_char(&ctx, 1, 5, 'Y');

    /* First undo should only undo row 1 change */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[1].chars, "line2");

    /* Second undo should undo row 0 change */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "line1");

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Line Operations Tests
 * ============================================================================ */

TEST(undo_record_insert_line_makes_undoable) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    /* Record line insert */
    undo_record_insert_line(&ctx, 0, 5, "world", 5);

    ASSERT_TRUE(undo_can_undo(&ctx));

    cleanup_ctx(&ctx);
}

TEST(undo_record_delete_line_makes_undoable) {
    editor_ctx_t ctx;
    const char *lines[] = {"hello", "world"};
    init_multiline_ctx_with_undo(&ctx, 2, lines);

    /* Record line delete */
    undo_record_delete_line(&ctx, 0, 5, "world", 5);

    ASSERT_TRUE(undo_can_undo(&ctx));

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Memory and Capacity Tests
 * ============================================================================ */

TEST(undo_clear_removes_all_history) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    /* Record some operations */
    undo_record_insert_char(&ctx, 0, 5, '!');
    undo_record_insert_char(&ctx, 0, 6, '?');

    ASSERT_TRUE(undo_can_undo(&ctx));

    /* Clear history */
    undo_clear(&ctx);

    ASSERT_FALSE(undo_can_undo(&ctx));

    int undo_levels, redo_levels;
    size_t memory;
    undo_get_stats(&ctx, &undo_levels, &redo_levels, &memory);
    ASSERT_EQ(undo_levels, 0);
    ASSERT_EQ(redo_levels, 0);

    cleanup_ctx(&ctx);
}

TEST(undo_respects_capacity_limit) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ctx.model.numrows = 1;
    ctx.model.row = calloc(1, sizeof(t_erow));
    ctx.model.row[0].chars = strdup("test");
    ctx.model.row[0].size = 4;
    ctx.model.row[0].render = strdup("test");
    ctx.model.row[0].rsize = 4;
    ctx.model.row[0].hl = NULL;

    ctx.view.screenrows = 24;
    ctx.view.screencols = 80;

    /* Replace the default undo state with a small one */
    undo_free(&ctx);
    undo_init(&ctx, 5, 1024 * 1024);  /* Only 5 entries */

    /* Record more operations than capacity */
    for (int i = 0; i < 10; i++) {
        undo_record_insert_char(&ctx, 0, i, 'a' + i);
        undo_break_group(&ctx);  /* Force each to be its own group */
    }

    /* Should still work, oldest entries evicted */
    int undo_levels, redo_levels;
    size_t memory;
    undo_get_stats(&ctx, &undo_levels, &redo_levels, &memory);

    /* Should have at most 5 entries */
    ASSERT_TRUE(undo_levels <= 5);

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST(undo_handles_null_undo_state) {
    editor_ctx_t ctx;
    /* Manually initialize context without undo */
    memset(&ctx, 0, sizeof(editor_ctx_t));
    ctx.view.mode = MODE_NORMAL;
    ctx.view.screenrows = 24;
    ctx.view.screencols = 80;

    /* undo_state should be NULL since we skipped normal init */
    ASSERT_NULL(ctx.model.undo_state);

    /* These should not crash */
    ASSERT_FALSE(undo_can_undo(&ctx));
    ASSERT_FALSE(undo_can_redo(&ctx));
    ASSERT_EQ(undo_perform(&ctx), 0);
    ASSERT_EQ(redo_perform(&ctx), 0);

    /* Recording should also not crash */
    undo_record_insert_char(&ctx, 0, 0, 'x');
    undo_record_delete_char(&ctx, 0, 0, 'x');
    undo_break_group(&ctx);
    undo_clear(&ctx);

    /* No cleanup needed - nothing was allocated */
}

TEST(undo_multiple_undo_redo_cycles) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "abc");

    /* Insert 'd' */
    ctx.model.row[0].chars = realloc(ctx.model.row[0].chars, 5);
    ctx.model.row[0].chars[3] = 'd';
    ctx.model.row[0].chars[4] = '\0';
    ctx.model.row[0].size = 4;
    undo_record_insert_char(&ctx, 0, 3, 'd');
    undo_break_group(&ctx);

    /* Insert 'e' */
    ctx.model.row[0].chars = realloc(ctx.model.row[0].chars, 6);
    ctx.model.row[0].chars[4] = 'e';
    ctx.model.row[0].chars[5] = '\0';
    ctx.model.row[0].size = 5;
    undo_record_insert_char(&ctx, 0, 4, 'e');
    undo_break_group(&ctx);

    ASSERT_STR_EQ(ctx.model.row[0].chars, "abcde");

    /* Undo 'e' */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "abcd");

    /* Undo 'd' */
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "abc");

    /* Redo 'd' */
    redo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "abcd");

    /* Redo 'e' */
    redo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "abcde");

    /* Undo both again */
    undo_perform(&ctx);
    undo_perform(&ctx);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "abc");

    cleanup_ctx(&ctx);
}

/* ============================================================================
 * Line-operation round-trips (regressions for the undo/redo line bugs)
 *
 * These drive the real editing entry points (editor_insert_newline /
 * editor_del_char) rather than undo_record_* directly, so they cover the
 * record-then-apply path end to end.
 * ============================================================================ */

/* Enter in the middle of a line, then undo, must restore the whole line.
 * Previously the tail row was deleted and never re-joined -> "hello world"
 * came back as "hello". */
TEST(undo_split_midline_restores_full_line) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello world");

    ctx.view.cy = 0;
    ctx.view.cx = 5;
    editor_insert_newline(&ctx);

    ASSERT_EQ(ctx.model.numrows, 2);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");
    ASSERT_STR_EQ(ctx.model.row[1].chars, " world");

    ASSERT_EQ(undo_perform(&ctx), 1);
    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello world");
    ASSERT_EQ(ctx.model.row[0].size, 11);

    cleanup_ctx(&ctx);
}

/* Enter at column 0 inserts a blank line above; undo must remove the blank
 * line, not the line that holds the text. */
TEST(undo_split_at_col0_keeps_text) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello");

    ctx.view.cy = 0;
    ctx.view.cx = 0;
    editor_insert_newline(&ctx);

    ASSERT_EQ(ctx.model.numrows, 2);

    ASSERT_EQ(undo_perform(&ctx), 1);
    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    cleanup_ctx(&ctx);
}

/* Redo of a split must reproduce the split exactly. */
TEST(redo_split_midline_reproduces_split) {
    editor_ctx_t ctx;
    init_ctx_with_undo(&ctx, "hello world");

    ctx.view.cy = 0;
    ctx.view.cx = 5;
    editor_insert_newline(&ctx);
    ASSERT_EQ(undo_perform(&ctx), 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello world");

    ASSERT_EQ(redo_perform(&ctx), 1);
    ASSERT_EQ(ctx.model.numrows, 2);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");
    ASSERT_STR_EQ(ctx.model.row[1].chars, " world");

    cleanup_ctx(&ctx);
}

/* Backspace at column 0 merges two lines; undo must split them back without
 * duplicating the merged text. Previously undo produced "helloworldworld". */
TEST(undo_line_merge_does_not_duplicate) {
    editor_ctx_t ctx;
    const char *lines[] = {"hello", "world"};
    init_multiline_ctx_with_undo(&ctx, 2, lines);

    ctx.view.cy = 1;
    ctx.view.cx = 0;
    editor_del_char(&ctx);

    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "helloworld");

    ASSERT_EQ(undo_perform(&ctx), 1);
    ASSERT_EQ(ctx.model.numrows, 2);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");
    ASSERT_STR_EQ(ctx.model.row[1].chars, "world");

    cleanup_ctx(&ctx);
}

/* Redo of a merge must keep the merged text (it used to drop the tail). */
TEST(redo_line_merge_keeps_text) {
    editor_ctx_t ctx;
    const char *lines[] = {"hello", "world"};
    init_multiline_ctx_with_undo(&ctx, 2, lines);

    ctx.view.cy = 1;
    ctx.view.cx = 0;
    editor_del_char(&ctx);
    ASSERT_EQ(undo_perform(&ctx), 1);

    ASSERT_EQ(redo_perform(&ctx), 1);
    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "helloworld");

    cleanup_ctx(&ctx);
}

/* Splitting a line must leave row.idx consistent, and deleting a row must
 * renumber the rows after it downward (they used to be incremented). */
TEST(row_idx_stays_consistent_across_delete) {
    editor_ctx_t ctx;
    const char *lines[] = {"aaa", "bbb", "ccc"};
    init_multiline_ctx_with_undo(&ctx, 3, lines);

    editor_del_row(&ctx, 0);

    ASSERT_EQ(ctx.model.numrows, 2);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "bbb");
    ASSERT_EQ(ctx.model.row[0].idx, 0);
    ASSERT_STR_EQ(ctx.model.row[1].chars, "ccc");
    ASSERT_EQ(ctx.model.row[1].idx, 1);

    cleanup_ctx(&ctx);
}

BEGIN_TEST_SUITE("Undo/Redo System")
    /* Initialization */
    RUN_TEST(undo_init_creates_state);
    RUN_TEST(undo_can_undo_returns_false_initially);
    RUN_TEST(undo_get_stats_returns_zeros_initially);

    /* Character insert/delete */
    RUN_TEST(undo_record_insert_char_makes_undoable);
    RUN_TEST(undo_record_delete_char_makes_undoable);
    RUN_TEST(undo_performs_insert_char_undo);
    RUN_TEST(undo_performs_delete_char_undo);

    /* Redo */
    RUN_TEST(redo_after_undo_restores_change);
    RUN_TEST(redo_unavailable_after_new_edit);
    RUN_TEST(undo_returns_zero_when_nothing_to_undo);
    RUN_TEST(redo_returns_zero_when_nothing_to_redo);

    /* Grouping */
    RUN_TEST(undo_groups_consecutive_inserts);
    RUN_TEST(undo_break_group_forces_new_group);
    RUN_TEST(undo_breaks_on_operation_type_change);
    RUN_TEST(undo_breaks_on_row_change);

    /* Line operations */
    RUN_TEST(undo_record_insert_line_makes_undoable);
    RUN_TEST(undo_record_delete_line_makes_undoable);
    RUN_TEST(undo_split_midline_restores_full_line);
    RUN_TEST(undo_split_at_col0_keeps_text);
    RUN_TEST(redo_split_midline_reproduces_split);
    RUN_TEST(undo_line_merge_does_not_duplicate);
    RUN_TEST(redo_line_merge_keeps_text);
    RUN_TEST(row_idx_stays_consistent_across_delete);

    /* Memory and capacity */
    RUN_TEST(undo_clear_removes_all_history);
    RUN_TEST(undo_respects_capacity_limit);

    /* Edge cases */
    RUN_TEST(undo_handles_null_undo_state);
    RUN_TEST(undo_multiple_undo_redo_cycles);
END_TEST_SUITE()
