/* test_indent.c - Unit tests for auto-indentation module
 *
 * Tests all aspects of auto-indentation:
 * - Indentation level detection
 * - Style detection (tabs vs spaces)
 * - Auto-indent on newline
 * - Electric dedent for closing braces
 * - Configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/indent.h"
#include "test_framework.h"

/* Helper: Create editor context with test content */
static editor_ctx_t *create_test_ctx(void) {
    editor_ctx_t *ctx = malloc(sizeof(editor_ctx_t));
    editor_ctx_init(ctx);
    return ctx;
}

/* Helper: Free editor context */
static void free_test_ctx(editor_ctx_t *ctx) {
    editor_ctx_free(ctx);
    free(ctx);
}

/* Helper: Insert a line of text at the end of the buffer */
static void insert_line(editor_ctx_t *ctx, const char *text) {
    editor_insert_row(ctx, ctx->model.numrows, (char *)text, strlen(text));
}

/* Test: indent_get_level() correctly counts leading spaces */
TEST(indent_get_level_spaces) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "no indent");
    insert_line(ctx, "    four spaces");
    insert_line(ctx, "        eight spaces");
    insert_line(ctx, "  two spaces");

    ASSERT_EQ(indent_get_level(ctx, 0), 0);
    ASSERT_EQ(indent_get_level(ctx, 1), 4);
    ASSERT_EQ(indent_get_level(ctx, 2), 8);
    ASSERT_EQ(indent_get_level(ctx, 3), 2);

    free_test_ctx(ctx);
}

/* Test: indent_get_level() correctly counts tabs */
TEST(indent_get_level_tabs) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "\tone tab");
    insert_line(ctx, "\t\ttwo tabs");
    insert_line(ctx, "\t    tab and spaces");

    /* With default width=4, one tab = 4 spaces */
    ASSERT_EQ(indent_get_level(ctx, 0), 4);
    ASSERT_EQ(indent_get_level(ctx, 1), 8);
    ASSERT_EQ(indent_get_level(ctx, 2), 8);  /* 4 (tab) + 4 (spaces) */

    free_test_ctx(ctx);
}

/* Test: indent_get_level() with mixed indentation */
TEST(indent_get_level_mixed) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "  \t  mixed");  /* 2 spaces + tab (4) + 2 spaces = 8 */

    ASSERT_EQ(indent_get_level(ctx, 0), 8);

    free_test_ctx(ctx);
}

/* Test: indent_get_level() stops at first non-whitespace */
TEST(indent_get_level_stops_at_content) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "    code    with    spaces");

    /* Should only count leading whitespace, not internal spaces */
    ASSERT_EQ(indent_get_level(ctx, 0), 4);

    free_test_ctx(ctx);
}

/* Test: indent_get_level() handles empty lines */
TEST(indent_get_level_empty) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "");
    insert_line(ctx, "   ");  /* Only whitespace */

    ASSERT_EQ(indent_get_level(ctx, 0), 0);
    ASSERT_EQ(indent_get_level(ctx, 1), 3);

    free_test_ctx(ctx);
}

/* Test: indent_detect_style() prefers tabs when present */
TEST(indent_detect_style_tabs) {
    editor_ctx_t *ctx = create_test_ctx();

    /* File with mostly tabs */
    insert_line(ctx, "\tline 1");
    insert_line(ctx, "\tline 2");
    insert_line(ctx, "\t\tline 3");
    insert_line(ctx, "  line 4");  /* One space-indented line */

    ASSERT_EQ(indent_detect_style(ctx), INDENT_STYLE_TABS);

    free_test_ctx(ctx);
}

/* Test: indent_detect_style() prefers spaces when tabs absent */
TEST(indent_detect_style_spaces) {
    editor_ctx_t *ctx = create_test_ctx();

    /* File with mostly spaces */
    insert_line(ctx, "  line 1");
    insert_line(ctx, "    line 2");
    insert_line(ctx, "  line 3");
    insert_line(ctx, "    line 4");

    ASSERT_EQ(indent_detect_style(ctx), INDENT_STYLE_SPACES);

    free_test_ctx(ctx);
}

/* Test: indent_detect_style() on empty file */
TEST(indent_detect_style_empty) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Empty file should default to spaces */
    ASSERT_EQ(indent_detect_style(ctx), INDENT_STYLE_SPACES);

    free_test_ctx(ctx);
}

/* Test: indent_set_width() changes indentation width */
TEST(indent_set_width) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Default width is 4 */
    ASSERT_EQ(indent_get_width(ctx), 4);

    /* Set to 2 */
    indent_set_width(ctx, 2);
    ASSERT_EQ(indent_get_width(ctx), 2);

    /* Set to 8 */
    indent_set_width(ctx, 8);
    ASSERT_EQ(indent_get_width(ctx), 8);

    /* Clamp to valid range (1-8) */
    indent_set_width(ctx, 0);
    ASSERT_EQ(indent_get_width(ctx), 1);

    indent_set_width(ctx, 10);
    ASSERT_EQ(indent_get_width(ctx), 8);

    free_test_ctx(ctx);
}

/* Test: indent_set_enabled() toggles auto-indent */
TEST(indent_set_enabled) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Enabled by default */
    ASSERT_EQ(indent_get_enabled(ctx), 1);

    /* Disable */
    indent_set_enabled(ctx, 0);
    ASSERT_EQ(indent_get_enabled(ctx), 0);

    /* Re-enable */
    indent_set_enabled(ctx, 1);
    ASSERT_EQ(indent_get_enabled(ctx), 1);

    free_test_ctx(ctx);
}

/* Test: indent_apply() copies indentation from previous line */
TEST(indent_apply_basic) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Insert a line with 4 spaces indent */
    insert_line(ctx, "    code");

    /* Create a new empty line and apply indent */
    editor_insert_row(ctx, 1, "", 0);
    ctx->view.cy = 1;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* New line should have 4 spaces */
    ASSERT_EQ(indent_get_level(ctx, 1), 4);

    free_test_ctx(ctx);
}

/* Test: indent_apply() adds extra indent after opening brace */
TEST(indent_apply_after_brace) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Insert a line ending with { */
    insert_line(ctx, "    if (condition) {");

    /* Create a new line and apply indent */
    editor_insert_row(ctx, 1, "", 0);
    ctx->view.cy = 1;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* New line should have 4 (base) + 4 (extra) = 8 spaces */
    ASSERT_EQ(indent_get_level(ctx, 1), 8);

    free_test_ctx(ctx);
}

/* Test: indent_apply() handles opening bracket [ */
TEST(indent_apply_after_bracket) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "  array = [");

    editor_insert_row(ctx, 1, "", 0);
    ctx->view.cy = 1;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* New line should have 2 (base) + 4 (extra) = 6 spaces */
    ASSERT_EQ(indent_get_level(ctx, 1), 6);

    free_test_ctx(ctx);
}

/* Test: indent_apply() handles opening paren ( */
TEST(indent_apply_after_paren) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "  function(");

    editor_insert_row(ctx, 1, "", 0);
    ctx->view.cy = 1;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* New line should have 2 (base) + 4 (extra) = 6 spaces */
    ASSERT_EQ(indent_get_level(ctx, 1), 6);

    free_test_ctx(ctx);
}

/* Test: indent_apply() ignores trailing whitespace before brace */
TEST(indent_apply_brace_with_trailing_space) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "    if (condition) {  ");  /* Trailing spaces after { */

    editor_insert_row(ctx, 1, "", 0);
    ctx->view.cy = 1;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* Should still detect the { and add extra indent */
    ASSERT_EQ(indent_get_level(ctx, 1), 8);

    free_test_ctx(ctx);
}

/* Test: indent_apply() does nothing on first line */
TEST(indent_apply_first_line) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Create first line */
    editor_insert_row(ctx, 0, "", 0);
    ctx->view.cy = 0;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* No indentation should be added */
    ASSERT_EQ(indent_get_level(ctx, 0), 0);

    free_test_ctx(ctx);
}

/* Test: indent_apply() respects disabled state */
TEST(indent_apply_disabled) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "    code");

    /* Disable auto-indent */
    indent_set_enabled(ctx, 0);

    editor_insert_row(ctx, 1, "", 0);
    ctx->view.cy = 1;
    ctx->view.cx = 0;

    indent_apply(ctx);

    /* No indentation should be added when disabled */
    ASSERT_EQ(indent_get_level(ctx, 1), 0);

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() dedents closing brace */
TEST(indent_electric_dedent_brace) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Setup: line with opening brace and indented line */
    insert_line(ctx, "if (condition) {");
    insert_line(ctx, "        code");  /* 8 spaces */

    /* User is on a new line with 8 spaces, about to type } */
    editor_insert_row(ctx, 2, "        ", 8);
    ctx->view.cy = 2;
    ctx->view.cx = 8;

    /* Typing } should trigger dedent */
    int dedented = indent_electric_char(ctx, '}');

    /* Should have dedented */
    ASSERT_EQ(dedented, 1);

    /* Line should now have 0 spaces (matching line 0) */
    ASSERT_EQ(indent_get_level(ctx, 2), 0);

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() dedents closing bracket */
TEST(indent_electric_dedent_bracket) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "  array = [");
    insert_line(ctx, "      1, 2, 3");
    insert_line(ctx, "      ");  /* 6 spaces, about to type ] */

    ctx->view.cy = 2;
    ctx->view.cx = 6;

    int dedented = indent_electric_char(ctx, ']');

    ASSERT_EQ(dedented, 1);
    ASSERT_EQ(indent_get_level(ctx, 2), 2);  /* Match line 0 */

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() dedents closing paren */
TEST(indent_electric_dedent_paren) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "function(");
    insert_line(ctx, "    arg1,");
    insert_line(ctx, "    ");  /* About to type ) */

    ctx->view.cy = 2;
    ctx->view.cx = 4;

    int dedented = indent_electric_char(ctx, ')');

    ASSERT_EQ(dedented, 1);
    ASSERT_EQ(indent_get_level(ctx, 2), 0);  /* Match line 0 */

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() doesn't dedent if non-whitespace before cursor */
TEST(indent_electric_no_dedent_with_content) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "{");
    insert_line(ctx, "    code}");  /* Has content before } */

    ctx->view.cy = 1;
    ctx->view.cx = 9;

    int dedented = indent_electric_char(ctx, '}');

    /* Should NOT dedent because there's content before cursor */
    ASSERT_EQ(dedented, 0);
    ASSERT_EQ(indent_get_level(ctx, 1), 4);

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() respects disabled state */
TEST(indent_electric_disabled) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "{");
    insert_line(ctx, "    ");

    ctx->view.cy = 1;
    ctx->view.cx = 4;

    /* Disable electric dedent - need to access via helper */
    /* Note: electric_enabled is part of indent_config opaque structure,
     * so we need to add a setter. For now, test the enabled flag instead */
    indent_set_enabled(ctx, 0);

    int dedented = indent_electric_char(ctx, '}');

    /* Should not dedent when auto-indent is disabled */
    ASSERT_EQ(dedented, 0);
    ASSERT_EQ(indent_get_level(ctx, 1), 4);

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() ignores non-closing characters */
TEST(indent_electric_only_closing_chars) {
    editor_ctx_t *ctx = create_test_ctx();

    insert_line(ctx, "{");
    insert_line(ctx, "    ");

    ctx->view.cy = 1;
    ctx->view.cx = 4;

    /* Typing regular character should not trigger dedent */
    int dedented = indent_electric_char(ctx, 'a');

    ASSERT_EQ(dedented, 0);
    ASSERT_EQ(indent_get_level(ctx, 1), 4);

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() handles nested braces */
TEST(indent_electric_nested_braces) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Nested structure */
    insert_line(ctx, "outer {");
    insert_line(ctx, "    inner {");
    insert_line(ctx, "        code");
    insert_line(ctx, "        ");  /* About to type first } */

    ctx->view.cy = 3;
    ctx->view.cx = 8;

    /* First closing brace should dedent to match "inner {" */
    int dedented = indent_electric_char(ctx, '}');

    ASSERT_EQ(dedented, 1);
    ASSERT_EQ(indent_get_level(ctx, 3), 4);  /* Match line 1 */

    free_test_ctx(ctx);
}

/* Test: indent_electric_char() handles mismatched brackets gracefully */
TEST(indent_electric_mismatched_brackets) {
    editor_ctx_t *ctx = create_test_ctx();

    /* Opening bracket [ but closing brace } */
    insert_line(ctx, "array = [");
    insert_line(ctx, "    1, 2, 3");
    insert_line(ctx, "    ");

    ctx->view.cy = 2;
    ctx->view.cx = 4;

    /* Should still dedent even if brackets don't match perfectly */
    int dedented = indent_electric_char(ctx, '}');

    /* Dedent should occur (falls back to one level) */
    ASSERT_EQ(dedented, 1);
    ASSERT_EQ(indent_get_level(ctx, 2), 0);

    free_test_ctx(ctx);
}

/* Test suite runner */
/* Test: indent_apply() uses the file row, not the screen row, when scrolled */
TEST(indent_apply_when_scrolled) {
    editor_ctx_t *ctx = create_test_ctx();
    ctx->view.screenrows = 24;
    ctx->view.screencols = 80;

    for (int i = 0; i < 19; i++) insert_line(ctx, "x");
    insert_line(ctx, "    if (condition) {");   /* file row 19 */
    editor_insert_row(ctx, 20, "", 0);
    ctx->view.rowoff = 15;
    ctx->view.cy = 5;                           /* file row 20 */
    ctx->view.cx = 0;

    indent_apply(ctx);

    ASSERT_EQ(indent_get_level(ctx, 20), 8);
    free_test_ctx(ctx);
}

/* Test: indent_electric_char() uses the file row when scrolled */
TEST(indent_electric_dedent_when_scrolled) {
    editor_ctx_t *ctx = create_test_ctx();
    ctx->view.screenrows = 24;
    ctx->view.screencols = 80;

    for (int i = 0; i < 20; i++) insert_line(ctx, "x");
    insert_line(ctx, "if (condition) {");      /* file row 20 */
    insert_line(ctx, "        code");
    editor_insert_row(ctx, 22, "        ", 8);
    ctx->view.rowoff = 20;
    ctx->view.cy = 2;                           /* file row 22 */
    ctx->view.cx = 8;

    ASSERT_EQ(indent_electric_char(ctx, '}'), 1);
    ASSERT_EQ(indent_get_level(ctx, 22), 0);
    free_test_ctx(ctx);
}

BEGIN_TEST_SUITE("Auto-Indent Module")

    /* indent_get_level() tests */
    RUN_TEST(indent_get_level_spaces);
    RUN_TEST(indent_get_level_tabs);
    RUN_TEST(indent_get_level_mixed);
    RUN_TEST(indent_get_level_stops_at_content);
    RUN_TEST(indent_get_level_empty);

    /* indent_detect_style() tests */
    RUN_TEST(indent_detect_style_tabs);
    RUN_TEST(indent_detect_style_spaces);
    RUN_TEST(indent_detect_style_empty);

    /* Configuration tests */
    RUN_TEST(indent_set_width);
    RUN_TEST(indent_set_enabled);

    /* indent_apply() tests */
    RUN_TEST(indent_apply_basic);
    RUN_TEST(indent_apply_after_brace);
    RUN_TEST(indent_apply_after_bracket);
    RUN_TEST(indent_apply_after_paren);
    RUN_TEST(indent_apply_brace_with_trailing_space);
    RUN_TEST(indent_apply_first_line);
    RUN_TEST(indent_apply_disabled);

    /* indent_electric_char() tests */
    RUN_TEST(indent_electric_dedent_brace);
    RUN_TEST(indent_electric_dedent_bracket);
    RUN_TEST(indent_electric_dedent_paren);
    RUN_TEST(indent_electric_no_dedent_with_content);
    RUN_TEST(indent_electric_disabled);
    RUN_TEST(indent_electric_only_closing_chars);
    RUN_TEST(indent_electric_nested_braces);
    RUN_TEST(indent_electric_mismatched_brackets);

    RUN_TEST(indent_apply_when_scrolled);
    RUN_TEST(indent_electric_dedent_when_scrolled);
END_TEST_SUITE()
