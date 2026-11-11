/* test_search.c - Unit tests for text search functionality
 *
 * Tests for incremental text search:
 * - Match finding (forward and backward)
 * - Navigation between matches
 * - Wrapping at file boundaries
 * - Case sensitivity
 * - Multiple matches
 * - Edge cases (empty buffer, no match, etc.)
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/event.h"
#include "loki/search.h"
#include <string.h>
#include <stdlib.h>

/* Helper: Create multi-line buffer with content */
static void init_search_buffer(editor_ctx_t *ctx, int num_lines, const char **lines) {
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
}

/* Helper: Free buffer resources */
static void free_search_buffer(editor_ctx_t *ctx) {
    for (int i = 0; i < ctx->model.numrows; i++) {
        free(ctx->model.row[i].chars);
        free(ctx->model.row[i].render);
        if (ctx->model.row[i].hl) free(ctx->model.row[i].hl);
    }
    free(ctx->model.row);
    ctx->model.row = NULL;
    ctx->model.numrows = 0;
}

/* ============================================================================
 * Event-driven search (Ctrl-F through modal_process_event)
 * ============================================================================ */

/* Rows with hl arrays, as syntax_update_row would leave them. */
static void init_hl_buffer(editor_ctx_t *ctx, int n, const char **lines) {
    init_search_buffer(ctx, n, lines);
    for (int i = 0; i < n; i++) ctx->model.row[i].hl = calloc((size_t)ctx->model.row[i].rsize + 1, 1);
}

static void key(editor_ctx_t *ctx, int c) {
    EditorEvent ev = event_from_keycode(c);
    modal_process_event(ctx, &ev);
}

static void keys(editor_ctx_t *ctx, const char *s) {
    while (*s) key(ctx, (unsigned char)*s++);
}

static void done(editor_ctx_t *ctx) {
    free_search_buffer(ctx);
    editor_ctx_free(ctx);
}

TEST(search_event_ctrl_f_starts_search) {
    editor_ctx_t ctx;
    const char *lines[] = { "alpha", "beta" };
    init_hl_buffer(&ctx, 2, lines);
    key(&ctx, CTRL_F);
    ASSERT_TRUE(search_active(&ctx));
    ASSERT_STR_EQ(search_prompt(&ctx), "Search:  (Use ESC/Arrows/Enter)");
    done(&ctx);
}

TEST(search_event_typing_moves_to_match_and_enter_keeps_it) {
    editor_ctx_t ctx;
    const char *lines[] = { "alpha", "beta", "gamma beta" };
    init_hl_buffer(&ctx, 3, lines);
    key(&ctx, CTRL_F);
    keys(&ctx, "beta");
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 1);
    ASSERT_EQ(ctx.model.row[1].hl[0], HL_MATCH);
    key(&ctx, ENTER);
    ASSERT_FALSE(search_active(&ctx));
    ASSERT_NULL(search_prompt(&ctx));
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 1);
    ASSERT_EQ(ctx.model.row[1].hl[0], HL_NORMAL);   /* match highlight removed */
    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);
    done(&ctx);
}

TEST(search_event_arrows_step_between_matches) {
    editor_ctx_t ctx;
    const char *lines[] = { "alpha", "beta", "gamma beta" };
    init_hl_buffer(&ctx, 3, lines);
    key(&ctx, CTRL_F);
    keys(&ctx, "beta");
    key(&ctx, ARROW_DOWN);
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 2);
    ASSERT_EQ(ctx.view.coloff + ctx.view.cx, 6);
    ASSERT_EQ(ctx.model.row[1].hl[0], HL_NORMAL);   /* previous match restored */
    key(&ctx, ARROW_UP);
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 1);
    key(&ctx, ENTER);
    done(&ctx);
}

TEST(search_event_esc_restores_cursor_and_highlight) {
    editor_ctx_t ctx;
    const char *lines[] = { "alpha", "beta" };
    init_hl_buffer(&ctx, 2, lines);
    ctx.view.cx = 3;
    key(&ctx, CTRL_F);
    keys(&ctx, "be");
    ASSERT_EQ(ctx.view.rowoff + ctx.view.cy, 1);
    key(&ctx, ESC);
    ASSERT_FALSE(search_active(&ctx));
    ASSERT_EQ(ctx.view.cx, 3);
    ASSERT_EQ(ctx.view.cy, 0);
    ASSERT_EQ(ctx.view.rowoff, 0);
    ASSERT_EQ(ctx.model.row[1].hl[0], HL_NORMAL);
    ASSERT_EQ(ctx.model.row[1].hl[1], HL_NORMAL);
    done(&ctx);
}

/* While searching, editing keys extend the query instead of editing. */
TEST(search_event_consumes_editing_keys) {
    editor_ctx_t ctx;
    const char *lines[] = { "xdd", "keep" };
    init_hl_buffer(&ctx, 2, lines);
    key(&ctx, CTRL_F);
    keys(&ctx, "xdd");
    ASSERT_STR_EQ(ctx.view.search.query, "xdd");
    ASSERT_STR_EQ(ctx.model.row[0].chars, "xdd");
    ASSERT_EQ(ctx.model.numrows, 2);
    key(&ctx, BACKSPACE);
    ASSERT_STR_EQ(ctx.view.search.query, "xd");
    key(&ctx, ESC);
    done(&ctx);
}

TEST(search_event_returns_to_insert_mode) {
    editor_ctx_t ctx;
    const char *lines[] = { "alpha", "beta" };
    init_hl_buffer(&ctx, 2, lines);
    ctx.view.mode = MODE_INSERT;
    key(&ctx, CTRL_F);
    keys(&ctx, "beta");
    key(&ctx, ENTER);
    ASSERT_EQ(ctx.view.mode, MODE_INSERT);
    keys(&ctx, "x");   /* typing inserts again */
    ASSERT_STR_EQ(ctx.model.row[1].chars, "xbeta");
    done(&ctx);
}

/* Background work runs during a search now, so the matched row can shrink
 * before ESC. The restore must copy only what fits the new row. */
TEST(search_event_restore_fits_shrunk_row) {
    editor_ctx_t ctx;
    const char *lines[] = { "aa", "long match here" };
    init_hl_buffer(&ctx, 2, lines);
    memset(ctx.model.row[1].hl, HL_STRING, (size_t)ctx.model.row[1].rsize);
    key(&ctx, CTRL_F);
    keys(&ctx, "match");
    ASSERT_EQ(ctx.view.search.saved_hl_line, 1);

    t_erow *row = &ctx.model.row[1];   /* simulate a Lua edit */
    free(row->render); free(row->chars); free(row->hl);
    row->chars = strdup("ab"); row->render = strdup("ab");
    row->size = row->rsize = 2;
    row->hl = calloc(3, 1);
    row->hl[2] = 99;                   /* guard byte past rsize */

    key(&ctx, ESC);
    ASSERT_EQ(row->hl[0], HL_STRING);
    ASSERT_EQ(row->hl[1], HL_STRING);
    ASSERT_EQ(row->hl[2], 99);
    done(&ctx);
}

/* ============================================================================
 * / and n/N in NORMAL mode
 * ============================================================================ */

static int cur_row(const editor_ctx_t *c) { return c->view.rowoff + c->view.cy; }
static int cur_col(const editor_ctx_t *c) { return c->view.coloff + c->view.cx; }

TEST(search_slash_starts_search_and_enter_saves_query) {
    editor_ctx_t ctx;
    const char *lines[] = { "one two", "two" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query(NULL);
    key(&ctx, '/');
    ASSERT_TRUE(search_active(&ctx));
    keys(&ctx, "two");
    key(&ctx, ENTER);
    ASSERT_EQ(cur_row(&ctx), 0);
    ASSERT_EQ(cur_col(&ctx), 4);
    key(&ctx, 'n');                       /* last query is "two" */
    ASSERT_EQ(cur_row(&ctx), 1);
    ASSERT_EQ(cur_col(&ctx), 0);
    done(&ctx);
}

TEST(search_esc_does_not_save_query) {
    editor_ctx_t ctx;
    const char *lines[] = { "abc" };
    init_hl_buffer(&ctx, 1, lines);
    search_set_last_query(NULL);
    key(&ctx, '/');
    keys(&ctx, "b");
    key(&ctx, ESC);
    ASSERT_EQ(search_repeat(&ctx, 1), -1);
    ASSERT_STR_EQ(ctx.view.statusmsg, "No previous search");
    done(&ctx);
}

TEST(search_n_finds_later_match_on_same_line) {
    editor_ctx_t ctx;
    const char *lines[] = { "ab ab ab" };
    init_hl_buffer(&ctx, 1, lines);
    search_set_last_query("ab");
    key(&ctx, 'n');
    ASSERT_EQ(cur_col(&ctx), 3);
    key(&ctx, 'n');
    ASSERT_EQ(cur_col(&ctx), 6);
    ASSERT_STR_EQ(ctx.view.statusmsg, "/ab");
    done(&ctx);
}

TEST(search_n_wraps_to_top) {
    editor_ctx_t ctx;
    const char *lines[] = { "x hit", "none", "hit" };
    init_hl_buffer(&ctx, 3, lines);
    search_set_last_query("hit");
    ctx.view.cy = 2;
    key(&ctx, 'n');
    ASSERT_EQ(cur_row(&ctx), 0);
    ASSERT_EQ(cur_col(&ctx), 2);
    ASSERT_STR_EQ(ctx.view.statusmsg, "/hit (wrapped)");
    done(&ctx);
}

/* N lands on the last match of an earlier row, not its first. */
TEST(search_N_goes_back_to_last_match_of_previous_row) {
    editor_ctx_t ctx;
    const char *lines[] = { "ab ab", "zz ab" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query("ab");
    ctx.view.cy = 1;
    ctx.view.cx = 3;                      /* on the "ab" of row 1 */
    key(&ctx, 'N');
    ASSERT_EQ(cur_row(&ctx), 0);
    ASSERT_EQ(cur_col(&ctx), 3);
    ASSERT_STR_EQ(ctx.view.statusmsg, "?ab");
    key(&ctx, 'N');
    ASSERT_EQ(cur_col(&ctx), 0);
    key(&ctx, 'N');                       /* wraps to the bottom */
    ASSERT_EQ(cur_row(&ctx), 1);
    ASSERT_EQ(cur_col(&ctx), 3);
    ASSERT_STR_EQ(ctx.view.statusmsg, "?ab (wrapped)");
    done(&ctx);
}

/* The only match is the one under the cursor: n wraps back onto it. */
TEST(search_n_single_match_stays) {
    editor_ctx_t ctx;
    const char *lines[] = { "only", "other" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query("only");
    ASSERT_EQ(search_repeat(&ctx, 1), 0);
    ASSERT_EQ(cur_row(&ctx), 0);
    ASSERT_EQ(cur_col(&ctx), 0);
    ASSERT_STR_EQ(ctx.view.statusmsg, "/only (wrapped)");
    done(&ctx);
}

TEST(search_n_reports_missing_pattern) {
    editor_ctx_t ctx;
    const char *lines[] = { "abc" };
    init_hl_buffer(&ctx, 1, lines);
    search_set_last_query("zzz");
    ctx.view.cx = 1;
    ASSERT_EQ(search_repeat(&ctx, 1), -1);
    ASSERT_EQ(cur_col(&ctx), 1);
    ASSERT_STR_EQ(ctx.view.statusmsg, "Pattern not found: zzz");
    done(&ctx);
}

/* The cursor column is a chars index: after a tab, "foo" is at 1, not 8. */
TEST(search_n_uses_chars_column_with_tabs) {
    editor_ctx_t ctx;
    const char *lines[] = { "x", "\tfoo" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query("foo");
    key(&ctx, 'n');
    ASSERT_EQ(cur_row(&ctx), 1);
    ASSERT_EQ(cur_col(&ctx), 1);
    done(&ctx);
}

/* A far match scrolls; a visible one only moves the cursor. */
TEST(search_n_scrolls_only_when_needed) {
    editor_ctx_t ctx;
    const char *lines[] = { "a", "hit", "b", "c", "hit" };
    init_hl_buffer(&ctx, 5, lines);
    ctx.view.screenrows = 3;
    search_set_last_query("hit");
    key(&ctx, 'n');
    ASSERT_EQ(ctx.view.rowoff, 0);
    ASSERT_EQ(ctx.view.cy, 1);
    key(&ctx, 'n');
    ASSERT_EQ(cur_row(&ctx), 4);
    ASSERT_TRUE(ctx.view.cy < ctx.view.screenrows);
    done(&ctx);
}

/* ============================================================================
 * Incremental search starts at the cursor; columns are chars indexes
 * ============================================================================ */

TEST(search_starts_from_cursor) {
    editor_ctx_t ctx;
    const char *lines[] = { "beta", "x", "beta y" };
    init_hl_buffer(&ctx, 3, lines);
    ctx.view.cy = 1;
    key(&ctx, CTRL_F);
    keys(&ctx, "beta");
    ASSERT_EQ(cur_row(&ctx), 2);           /* not the first match in the file */
    ASSERT_EQ(cur_col(&ctx), 0);
    key(&ctx, ENTER);
    done(&ctx);
}

TEST(search_wraps_past_end_from_cursor) {
    editor_ctx_t ctx;
    const char *lines[] = { "x beta", "y", "beta" };
    init_hl_buffer(&ctx, 3, lines);
    ctx.view.cy = 2;
    ctx.view.cx = 2;                       /* past the only match on row 2 */
    key(&ctx, '/');
    keys(&ctx, "beta");
    ASSERT_EQ(cur_row(&ctx), 0);
    ASSERT_EQ(cur_col(&ctx), 2);
    key(&ctx, ENTER);
    done(&ctx);
}

/* Like vim, a match under the cursor is skipped in favour of the next one. */
TEST(search_skips_match_under_cursor) {
    editor_ctx_t ctx;
    const char *lines[] = { "beta", "x", "beta" };
    init_hl_buffer(&ctx, 3, lines);
    key(&ctx, '/');
    keys(&ctx, "beta");
    ASSERT_EQ(cur_row(&ctx), 2);
    key(&ctx, ESC);
    done(&ctx);
}

/* Arrows step match by match, including matches on the same row. */
TEST(search_arrows_step_within_row) {
    editor_ctx_t ctx;
    const char *lines[] = { "ab ab ab" };
    init_hl_buffer(&ctx, 1, lines);
    key(&ctx, '/');
    keys(&ctx, "ab");
    ASSERT_EQ(cur_col(&ctx), 3);
    key(&ctx, ARROW_DOWN);
    ASSERT_EQ(cur_col(&ctx), 6);
    key(&ctx, ARROW_UP);
    ASSERT_EQ(cur_col(&ctx), 3);
    key(&ctx, ARROW_UP);
    ASSERT_EQ(cur_col(&ctx), 0);
    key(&ctx, ESC);
    done(&ctx);
}

/* A query with no match returns the cursor to where the search began. */
TEST(search_no_match_returns_to_origin) {
    editor_ctx_t ctx;
    const char *lines[] = { "alpha", "beta" };
    init_hl_buffer(&ctx, 2, lines);
    ctx.view.cx = 2;
    key(&ctx, '/');
    keys(&ctx, "b");
    ASSERT_EQ(cur_row(&ctx), 1);
    keys(&ctx, "z");
    ASSERT_EQ(cur_row(&ctx), 0);
    ASSERT_EQ(cur_col(&ctx), 2);
    ASSERT_EQ(ctx.model.row[1].hl[0], HL_NORMAL);
    key(&ctx, ESC);
    done(&ctx);
}

/* After a tab the cursor lands on the chars index of the match, and the
 * highlight covers its render columns (the tab renders as 7 spaces). */
TEST(search_tab_line_cursor_and_highlight) {
    editor_ctx_t ctx;
    const char *lines[] = { "x", "\tfoo" };
    init_hl_buffer(&ctx, 2, lines);
    t_erow *row = &ctx.model.row[1];
    free(row->render); free(row->hl);
    row->render = strdup("       foo");
    row->rsize = 10;
    row->hl = calloc(11, 1);
    key(&ctx, '/');
    keys(&ctx, "foo");
    ASSERT_EQ(cur_row(&ctx), 1);
    ASSERT_EQ(cur_col(&ctx), 1);
    ASSERT_EQ(row->hl[6], HL_NORMAL);
    ASSERT_EQ(row->hl[7], HL_MATCH);
    ASSERT_EQ(row->hl[9], HL_MATCH);
    key(&ctx, ENTER);
    ASSERT_EQ(cur_col(&ctx), 1);
    ASSERT_EQ(row->hl[7], HL_NORMAL);
    done(&ctx);
}

/* A visible match moves the cursor without scrolling. */
TEST(search_scrolls_only_when_needed) {
    editor_ctx_t ctx;
    const char *lines[] = { "a", "hit", "b", "c", "d", "hit" };
    init_hl_buffer(&ctx, 6, lines);
    ctx.view.screenrows = 3;
    key(&ctx, '/');
    keys(&ctx, "hit");
    ASSERT_EQ(ctx.view.rowoff, 0);
    ASSERT_EQ(ctx.view.cy, 1);
    key(&ctx, ARROW_DOWN);
    ASSERT_EQ(cur_row(&ctx), 5);
    ASSERT_TRUE(ctx.view.cy < ctx.view.screenrows);
    key(&ctx, ESC);
    done(&ctx);
}

/* ============================================================================
 * Matching properties (through n, the path every search uses)
 * ============================================================================ */

TEST(search_match_is_case_sensitive) {
    editor_ctx_t ctx;
    const char *lines[] = { "Hello", "hello" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query("hello");
    ASSERT_EQ(search_repeat(&ctx, 1), 0);
    ASSERT_EQ(cur_row(&ctx), 1);
    search_set_last_query("HELLO");
    ASSERT_EQ(search_repeat(&ctx, 1), -1);
    done(&ctx);
}

/* No pattern syntax: brackets match themselves. */
TEST(search_match_is_literal) {
    editor_ctx_t ctx;
    const char *lines[] = { "a1b", "x[123]" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query("[123]");
    ASSERT_EQ(search_repeat(&ctx, 1), 0);
    ASSERT_EQ(cur_row(&ctx), 1);
    ASSERT_EQ(cur_col(&ctx), 1);
    done(&ctx);
}

TEST(search_empty_buffer_finds_nothing) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.view.screenrows = 24;
    ctx.view.screencols = 80;
    search_set_last_query("x");
    ASSERT_EQ(search_repeat(&ctx, 1), -1);
    ASSERT_EQ(search_repeat(&ctx, -1), -1);
    key(&ctx, '/');
    keys(&ctx, "x");
    ASSERT_EQ(cur_row(&ctx), 0);
    key(&ctx, ESC);
    editor_ctx_free(&ctx);
}

TEST(search_query_longer_than_rows) {
    editor_ctx_t ctx;
    const char *lines[] = { "ab", "c" };
    init_hl_buffer(&ctx, 2, lines);
    search_set_last_query("abcdef");
    ASSERT_EQ(search_repeat(&ctx, 1), -1);
    ASSERT_EQ(search_repeat(&ctx, -1), -1);
    done(&ctx);
}

BEGIN_TEST_SUITE("Search")
    /* Search from cursor, chars columns */
    RUN_TEST(search_starts_from_cursor);
    RUN_TEST(search_wraps_past_end_from_cursor);
    RUN_TEST(search_skips_match_under_cursor);
    RUN_TEST(search_arrows_step_within_row);
    RUN_TEST(search_no_match_returns_to_origin);
    RUN_TEST(search_tab_line_cursor_and_highlight);
    RUN_TEST(search_scrolls_only_when_needed);
    /* / and n/N */
    RUN_TEST(search_slash_starts_search_and_enter_saves_query);
    RUN_TEST(search_esc_does_not_save_query);
    RUN_TEST(search_n_finds_later_match_on_same_line);
    RUN_TEST(search_n_wraps_to_top);
    RUN_TEST(search_N_goes_back_to_last_match_of_previous_row);
    RUN_TEST(search_n_single_match_stays);
    RUN_TEST(search_n_reports_missing_pattern);
    RUN_TEST(search_n_uses_chars_column_with_tabs);
    RUN_TEST(search_n_scrolls_only_when_needed);
    /* Event-driven search */
    RUN_TEST(search_event_ctrl_f_starts_search);
    RUN_TEST(search_event_typing_moves_to_match_and_enter_keeps_it);
    RUN_TEST(search_event_arrows_step_between_matches);
    RUN_TEST(search_event_esc_restores_cursor_and_highlight);
    RUN_TEST(search_event_consumes_editing_keys);
    RUN_TEST(search_event_returns_to_insert_mode);
    RUN_TEST(search_event_restore_fits_shrunk_row);
    /* Matching properties */
    RUN_TEST(search_match_is_case_sensitive);
    RUN_TEST(search_match_is_literal);
    RUN_TEST(search_empty_buffer_finds_nothing);
    RUN_TEST(search_query_longer_than_rows);
END_TEST_SUITE()
