/* loki_indent.c - Auto-indentation implementation
 *
 * This module provides automatic indentation features for a better
 * coding experience. It handles:
 * - Preserving indentation when pressing Enter
 * - Auto-indenting after opening braces/brackets
 * - Electric dedent when typing closing braces/brackets
 * - Smart detection of tabs vs spaces
 */

#include "indent.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Indentation configuration structure (opaque to external code) */
struct indent_config {
    int enabled;
    int style;
    int width;
    int electric_enabled;
};

/* Initialize indentation configuration with sensible defaults */
void indent_init(editor_ctx_t *ctx) {
    struct indent_config *config = malloc(sizeof(struct indent_config));
    if (!config) return; /* Out of memory - indent will be disabled */

    config->enabled = 1;  /* Auto-indent on by default */
    config->style = INDENT_STYLE_SPACES;  /* Spaces by default */
    config->width = 4;    /* 4 spaces per indent level */
    config->electric_enabled = 1;  /* Electric dedent on by default */

    ctx->model.indent_config = config;
}

/* Enable or disable auto-indent */
void indent_set_enabled(editor_ctx_t *ctx, int enabled) {
    if (!ctx->model.indent_config) return;
    ctx->model.indent_config->enabled = enabled;
}

/* Set indentation width */
void indent_set_width(editor_ctx_t *ctx, int width) {
    if (!ctx->model.indent_config) return;
    if (width < 1) width = 1;
    if (width > 8) width = 8;
    ctx->model.indent_config->width = width;
}

/* Test-only accessor: get indentation width */
int indent_get_width(editor_ctx_t *ctx) {
    if (!ctx->model.indent_config) return 0;
    return ctx->model.indent_config->width;
}

/* Test-only accessor: get enabled state */
int indent_get_enabled(editor_ctx_t *ctx) {
    if (!ctx->model.indent_config) return 0;
    return ctx->model.indent_config->enabled;
}

/* Test-only accessor: get electric dedent enabled state */
int indent_get_electric_enabled(editor_ctx_t *ctx) {
    if (!ctx->model.indent_config) return 0;
    return ctx->model.indent_config->electric_enabled;
}

/* Get the indentation level of a row in spaces.
 * Counts leading whitespace, converting tabs to spaces based on width. */
int indent_get_level(editor_ctx_t *ctx, int row) {
    if (row < 0 || row >= ctx->model.numrows) return 0;
    if (!ctx->model.indent_config) return 0;

    t_erow *erow = &ctx->model.row[row];
    int level = 0;
    int width = ctx->model.indent_config->width;

    for (int i = 0; i < erow->size; i++) {
        if (erow->chars[i] == ' ') {
            level++;
        } else if (erow->chars[i] == '\t') {
            level += width;
        } else {
            break;  /* Stop at first non-whitespace */
        }
    }

    return level;
}

/* Detect indentation style from file content.
 * Heuristic: count lines with leading tabs vs spaces.
 * Returns INDENT_STYLE_TABS or INDENT_STYLE_SPACES. */
int indent_detect_style(editor_ctx_t *ctx) {
    int tab_count = 0;
    int space_count = 0;

    /* Sample up to 100 lines to detect style */
    int sample_size = ctx->model.numrows < 100 ? ctx->model.numrows : 100;

    for (int i = 0; i < sample_size; i++) {
        t_erow *row = &ctx->model.row[i];
        if (row->size == 0) continue;

        /* Check first character - if it's whitespace, count it */
        if (row->chars[0] == '\t') {
            tab_count++;
        } else if (row->chars[0] == ' ') {
            /* Count lines with 2+ leading spaces as space-indented */
            if (row->size > 1 && row->chars[1] == ' ') {
                space_count++;
            }
        }
    }

    /* If we found tabs, prefer tabs; otherwise spaces */
    return (tab_count > space_count / 2) ? INDENT_STYLE_TABS : INDENT_STYLE_SPACES;
}

/* Check if a character is an opening bracket/brace */
static int is_opening_char(int c) {
    return c == '{' || c == '[' || c == '(';
}

/* Check if a character is a closing bracket/brace */
static int is_closing_char(int c) {
    return c == '}' || c == ']' || c == ')';
}

/* Get the matching opening character for a closing character */
static int get_matching_open(int c) {
    if (c == '}') return '{';
    if (c == ']') return '[';
    if (c == ')') return '(';
    return 0;
}

/* Check if a line ends with an opening brace/bracket (ignoring trailing whitespace) */
static int line_ends_with_opening(t_erow *row) {
    /* Scan backwards from end, skipping whitespace */
    for (int i = row->size - 1; i >= 0; i--) {
        if (isspace(row->chars[i])) continue;
        return is_opening_char(row->chars[i]);
    }
    return 0;
}

/* Insert indentation at the current cursor position.
 * Respects the configured indentation style (tabs or spaces). */
static void insert_indentation(editor_ctx_t *ctx, int level) {
    if (!ctx->model.indent_config) return;
    if (level <= 0) return;

    int width = ctx->model.indent_config->width;
    int style = ctx->model.indent_config->style;

    if (style == INDENT_STYLE_TABS) {
        /* Use tabs: convert level to tab count */
        int tabs = level / width;
        for (int i = 0; i < tabs; i++) {
            editor_insert_char(ctx, '\t');
        }
        /* Insert remaining spaces */
        int spaces = level % width;
        for (int i = 0; i < spaces; i++) {
            editor_insert_char(ctx, ' ');
        }
    } else {
        /* Use spaces */
        for (int i = 0; i < level; i++) {
            editor_insert_char(ctx, ' ');
        }
    }
}

/* Apply indentation to current line based on previous line.
 * Called after editor_insert_newline() creates a new line.
 * ctx->view.cy should point to the new (empty) line. */
void indent_apply(editor_ctx_t *ctx) {
    if (!ctx->model.indent_config) return;
    if (!ctx->model.indent_config->enabled) return;
    int filerow = ctx->view.rowoff + ctx->view.cy;
    if (filerow <= 0 || filerow > ctx->model.numrows) return;  /* No previous line */

    /* Get indentation from previous line */
    int prev_row = filerow - 1;
    if (prev_row >= ctx->model.numrows) return;
    int base_indent = indent_get_level(ctx, prev_row);

    /* Check if previous line ends with opening brace/bracket */
    int extra_indent = 0;
    if (line_ends_with_opening(&ctx->model.row[prev_row])) {
        extra_indent = ctx->model.indent_config->width;
    }

    /* Insert the indentation */
    int total_indent = base_indent + extra_indent;
    insert_indentation(ctx, total_indent);
}

/* Handle electric dedent when typing closing braces.
 * If the current line has only whitespace + the closing char, dedent by one level.
 * Returns 1 if dedent was applied, 0 otherwise. */
int indent_electric_char(editor_ctx_t *ctx, int c) {
    if (!ctx->model.indent_config) return 0;
    if (!ctx->model.indent_config->enabled) return 0;  /* Respect general enabled flag */
    if (!ctx->model.indent_config->electric_enabled) return 0;
    if (!is_closing_char(c)) return 0;
    int filerow = ctx->view.rowoff + ctx->view.cy;
    if (filerow < 0 || filerow >= ctx->model.numrows) return 0;

    t_erow *row = &ctx->model.row[filerow];

    int filecol = ctx->view.coloff + ctx->view.cx;
    if (filecol > row->size) filecol = row->size;

    /* Check if line contains only whitespace before cursor */
    for (int i = 0; i < filecol; i++) {
        if (!isspace((unsigned char)row->chars[i])) {
            return 0;  /* Non-whitespace before cursor - no dedent */
        }
    }

    /* Find matching opening bracket on previous lines to determine target indent */
    int target_indent = -1;
    int match_char = get_matching_open(c);
    int depth = 1;  /* We're looking for the matching opening */

    /* Scan backwards from previous line */
    for (int r = filerow - 1; r >= 0; r--) {
        t_erow *scan_row = &ctx->model.row[r];
        for (int i = scan_row->size - 1; i >= 0; i--) {
            char ch = scan_row->chars[i];
            if (ch == c) {
                depth++;
            } else if (ch == match_char) {
                depth--;
                if (depth == 0) {
                    /* Found the matching opening bracket */
                    target_indent = indent_get_level(ctx, r);
                    goto found_match;
                }
            }
        }
    }

found_match:
    /* If we didn't find a match, just dedent by one level */
    if (target_indent < 0) {
        int current_indent = indent_get_level(ctx, filerow);
        target_indent = current_indent - ctx->model.indent_config->width;
        if (target_indent < 0) target_indent = 0;
    }

    /* Calculate how much to dedent */
    int current_indent = indent_get_level(ctx, filerow);
    int dedent_amount = current_indent - target_indent;

    if (dedent_amount <= 0) return 0;  /* Already at or past target */

    /* Delete leading whitespace characters */
    int deleted = 0;
    while (deleted < dedent_amount &&
           (ctx->view.coloff + ctx->view.cx) > 0) {
        /* Delete character before cursor (backspace) */
        editor_del_char(ctx);
        deleted++;
    }

    return 1;  /* Dedent was applied */
}
