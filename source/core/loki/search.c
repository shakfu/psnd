/* loki_search.c - Text search functionality
 *
 * This module implements incremental text search within the editor.
 * Search is interactive: as the user types, matches are found and highlighted
 * in real-time. Users can navigate between matches with arrow keys.
 *
 * Features:
 * - Incremental search: updates as you type
 * - Forward/backward navigation: arrow keys move between matches
 * - Visual highlighting: matches shown with HL_MATCH color
 * - Wrapping: search wraps around at beginning/end of file
 * - Restore cursor: ESC returns to original position
 *
 * Keybindings:
 * - ESC: Cancel search, restore original cursor position
 * - ENTER: Accept search, keep cursor at current match
 * - Arrow Up/Left: Search backward (previous match)
 * - Arrow Down/Right: Search forward (next match)
 * - Backspace/Delete: Remove character from query
 * - Printable chars: Add to search query
 *
 * NORMAL mode: `/` starts a search; `n` / `N` repeat the last accepted query.
 */

#include "search.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ======================= Incremental search ============================== */
/* Search is a state on the view, not a loop: the frontend's own loop keeps
 * running (background work, redraws), and modal_process_event() sends keys
 * here while it is active.
 *
 * Matching runs on row->chars, so match columns are chars indexes like the
 * cursor column; only the highlight converts them to render offsets. */

/* Last accepted query, shared by all buffers like vim's last search. */
static char last_query[KILO_QUERY_LEN + 1];

static int find_fwd(const t_erow *r, const char *q, int n, int from) {
    for (int i = from < 0 ? 0 : from; i + n <= r->size; i++)
        if (memcmp(r->chars + i, q, (size_t)n) == 0) return i;
    return -1;
}

/* Last match starting before `before`. */
static int find_back(const t_erow *r, const char *q, int n, int before) {
    int i = before - 1;
    if (i > r->size - n) i = r->size - n;
    for (; i >= 0; i--)
        if (memcmp(r->chars + i, q, (size_t)n) == 0) return i;
    return -1;
}

/* First match strictly after (direction 1) or before (-1) file position
 * (row, col): the rest of that row, the other rows in order, then the whole
 * of that row again (the wrap). Returns the row and sets *out_col, or -1. */
static int find_from(const editor_ctx_t *ctx, const char *q, int row, int col,
                     int direction, int *out_col) {
    int n = (int)strlen(q);
    int nrows = ctx->model.numrows;
    if (n == 0 || nrows == 0) return -1;
    if (row >= nrows) row = nrows - 1;
    if (row < 0) row = 0;
    for (int k = 0; k <= nrows; k++) {
        int r = ((row + direction * k) % nrows + nrows) % nrows;
        const t_erow *er = &ctx->model.row[r];
        int off;
        if (k == 0) off = direction > 0 ? find_fwd(er, q, n, col + 1) : find_back(er, q, n, col);
        else off = direction > 0 ? find_fwd(er, q, n, 0) : find_back(er, q, n, er->size + 1);
        if (off >= 0) { *out_col = off; return r; }
    }
    return -1;
}

/* Render offset of chars index cx, matching the tab expansion in
 * editor_update_row(). */
static int chars_to_render(const t_erow *r, int cx) {
    int rx = 0;
    for (int j = 0; j < cx && j < r->size; j++) {
        if (r->chars[j] == TAB) {
            rx++;
            while ((rx + 1) % 8 != 0) rx++;
        } else {
            rx++;
        }
    }
    return rx;
}

/* Put the cursor on file position (row, col), scrolling only if needed. */
static void search_goto(editor_ctx_t *ctx, int row, int col) {
    if (row < ctx->view.rowoff || row >= ctx->view.rowoff + ctx->view.screenrows) {
        ctx->view.rowoff = row;
        ctx->view.cy = 0;
    } else {
        ctx->view.cy = row - ctx->view.rowoff;
    }
    if (col >= ctx->view.coloff && col < ctx->view.coloff + ctx->view.screencols) {
        ctx->view.cx = col - ctx->view.coloff;
    } else {
        ctx->view.coloff = 0;
        ctx->view.cx = col;
        if (ctx->view.cx >= ctx->view.screencols) {
            ctx->view.coloff = ctx->view.cx - ctx->view.screencols + 1;
            ctx->view.cx = ctx->view.screencols - 1;
        }
    }
}

/* Put back the highlight the current match overwrote. Rows may have changed
 * since (background Lua runs during a search), so copy only what still fits. */
static void search_restore_hl(editor_ctx_t *ctx) {
    int line = ctx->view.search.saved_hl_line;
    if (ctx->view.search.saved_hl && line >= 0 && line < ctx->model.numrows) {
        t_erow *row = &ctx->model.row[line];
        int n = ctx->view.search.saved_hl_len;
        if (n > row->rsize) n = row->rsize;
        if (row->hl && n > 0) memcpy(row->hl, ctx->view.search.saved_hl, (size_t)n);
    }
    free(ctx->view.search.saved_hl);
    ctx->view.search.saved_hl = NULL;
    ctx->view.search.saved_hl_line = -1;
    ctx->view.search.saved_hl_len = 0;
}

/* Highlight query at chars index col of row, saving what it covers. */
static void search_highlight(editor_ctx_t *ctx, int row_idx, int col) {
    t_erow *row = &ctx->model.row[row_idx];
    if (!row->hl || row->rsize <= 0) return;
    char *saved = malloc((size_t)row->rsize);
    if (!saved) return;
    memcpy(saved, row->hl, (size_t)row->rsize);
    ctx->view.search.saved_hl = saved;
    ctx->view.search.saved_hl_line = row_idx;
    ctx->view.search.saved_hl_len = row->rsize;
    int start = chars_to_render(row, col);
    int end = chars_to_render(row, col + ctx->view.search.qlen);
    if (end > row->rsize) end = row->rsize;
    if (end > start) memset(row->hl + start, HL_MATCH, (size_t)(end - start));
}

/* Move to the next match after (direction 1) or before (-1) file position
 * (row, col) and highlight it. Returns 0, or -1 with the cursor unmoved. */
static int search_step(editor_ctx_t *ctx, int row, int col, int direction) {
    int off = 0;
    int hit = find_from(ctx, ctx->view.search.query, row, col, direction, &off);
    search_restore_hl(ctx);
    if (hit < 0) return -1;
    ctx->view.search.match_row = hit;
    ctx->view.search.match_col = off;
    search_highlight(ctx, hit, off);
    search_goto(ctx, hit, off);
    return 0;
}

static void search_restore_cursor(editor_ctx_t *ctx) {
    ctx->view.cx = ctx->view.search.saved_cx;
    ctx->view.cy = ctx->view.search.saved_cy;
    ctx->view.coloff = ctx->view.search.saved_coloff;
    ctx->view.rowoff = ctx->view.search.saved_rowoff;
}

static void search_end(editor_ctx_t *ctx, int restore_cursor) {
    if (restore_cursor) search_restore_cursor(ctx);
    search_restore_hl(ctx);
    ctx->view.search.active = 0;
    editor_set_status_msg(ctx, "");
}

void search_begin(editor_ctx_t *ctx) {
    if (!ctx || ctx->view.search.active) return;
    ctx->view.search.active = 1;
    ctx->view.search.query[0] = '\0';
    ctx->view.search.qlen = 0;
    ctx->view.search.match_row = -1;
    ctx->view.search.match_col = -1;
    ctx->view.search.saved_hl = NULL;
    ctx->view.search.saved_hl_line = -1;
    ctx->view.search.saved_hl_len = 0;
    ctx->view.search.saved_cx = ctx->view.cx;
    ctx->view.search.saved_cy = ctx->view.cy;
    ctx->view.search.saved_coloff = ctx->view.coloff;
    ctx->view.search.saved_rowoff = ctx->view.rowoff;
}

int search_active(const editor_ctx_t *ctx) {
    return ctx && ctx->view.search.active;
}

int search_handle_key(editor_ctx_t *ctx, int c) {
    if (!search_active(ctx)) return 0;
    int origin_row = ctx->view.search.saved_rowoff + ctx->view.search.saved_cy;
    int origin_col = ctx->view.search.saved_coloff + ctx->view.search.saved_cx;
    int requery = 0, direction = 0;
    if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
        if (ctx->view.search.qlen != 0) ctx->view.search.query[--ctx->view.search.qlen] = '\0';
        requery = 1;
    } else if (c == ESC || c == ENTER) {
        if (c == ENTER && ctx->view.search.qlen > 0)
            memcpy(last_query, ctx->view.search.query, (size_t)ctx->view.search.qlen + 1);
        search_end(ctx, c == ESC);
        return 1;
    } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
        direction = 1;
    } else if (c == ARROW_LEFT || c == ARROW_UP) {
        direction = -1;
    } else if (c >= 0 && c < 256 && isprint(c)) {
        if (ctx->view.search.qlen < KILO_QUERY_LEN) {
            ctx->view.search.query[ctx->view.search.qlen++] = (char)c;
            ctx->view.search.query[ctx->view.search.qlen] = '\0';
            requery = 1;
        }
    }
    if (requery) {
        /* A changed query searches again from where the search began, and
         * returns there when nothing matches. */
        ctx->view.search.match_row = -1;
        if (search_step(ctx, origin_row, origin_col, 1) != 0) search_restore_cursor(ctx);
    } else if (direction) {
        if (ctx->view.search.match_row < 0) search_step(ctx, origin_row, origin_col, direction);
        else search_step(ctx, ctx->view.search.match_row, ctx->view.search.match_col, direction);
    }
    return 1;
}

const char *search_prompt(const editor_ctx_t *ctx) {
    static char buf[KILO_QUERY_LEN + 48];
    if (!search_active(ctx)) return NULL;
    snprintf(buf, sizeof buf, "Search: %s (Use ESC/Arrows/Enter)", ctx->view.search.query);
    return buf;
}

/* ======================= Repeat search (n / N) ============================ */

int search_repeat(editor_ctx_t *ctx, int direction) {
    if (!ctx) return -1;
    if (!last_query[0]) {
        editor_set_status_msg(ctx, "No previous search");
        return -1;
    }
    int row = ctx->view.rowoff + ctx->view.cy;
    int col = ctx->view.coloff + ctx->view.cx;
    if (row >= ctx->model.numrows) row = ctx->model.numrows - 1;
    if (row < 0) row = 0;
    int off = 0;
    int hit = find_from(ctx, last_query, row, col, direction, &off);
    if (hit < 0) {
        editor_set_status_msg(ctx, "Pattern not found: %s", last_query);
        return -1;
    }
    int wrapped = direction > 0 ? (hit < row || (hit == row && off <= col))
                                : (hit > row || (hit == row && off >= col));
    search_goto(ctx, hit, off);
    editor_set_status_msg(ctx, "%c%s%s", direction > 0 ? '/' : '?', last_query,
                          wrapped ? " (wrapped)" : "");
    return 0;
}

void search_set_last_query(const char *query) {
    snprintf(last_query, sizeof last_query, "%s", query ? query : "");
}

void search_free(editor_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->view.search.saved_hl);
    ctx->view.search.saved_hl = NULL;
    ctx->view.search.active = 0;
}
