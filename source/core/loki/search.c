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
 */

#include "search.h"
#include "internal.h"
#include "terminal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Helper function to find the next match in a given direction.
 * Returns the row index of the match, or -1 if not found.
 * Sets match_offset to the column position of the match.
 * This function is exposed for testing purposes. */
int editor_find_next_match(editor_ctx_t *ctx, const char *query, int start_row, int direction, int *match_offset) {
    if (!ctx || !query || !match_offset || ctx->model.numrows == 0 || query[0] == '\0') {
        return -1;
    }

    int current = start_row;

    /* Search through all rows */
    for (int i = 0; i < ctx->model.numrows; i++) {
        current += direction;

        /* Wrap around */
        if (current == -1) {
            current = ctx->model.numrows - 1;
        } else if (current == ctx->model.numrows) {
            current = 0;
        }

        /* Search for query in this row */
        char *match = strstr(ctx->model.row[current].render, query);
        if (match) {
            *match_offset = match - ctx->model.row[current].render;
            return current;
        }
    }

    return -1;  /* No match found */
}

/* Incremental text search with arrow keys navigation.
 * Interactive search that updates as you type and allows navigating
 * between matches. ESC cancels and restores cursor position.
 * ENTER accepts and keeps cursor at current match. */
void editor_find(editor_ctx_t *ctx, int fd) {
    char query[KILO_QUERY_LEN+1] = {0};
    int qlen = 0;
    int last_match = -1; /* Last line where a match was found. -1 for none. */
    int find_next = 0; /* if 1 search next, if -1 search prev. */
    int saved_hl_line = -1;  /* No saved HL */
    char *saved_hl = NULL;

#define FIND_RESTORE_HL do { \
    if (saved_hl) { \
        memcpy(ctx->model.row[saved_hl_line].hl,saved_hl, ctx->model.row[saved_hl_line].rsize); \
        free(saved_hl); \
        saved_hl = NULL; \
    } \
} while (0)

    /* Save the cursor position in order to restore it later. */
    int saved_cx = ctx->view.cx, saved_cy = ctx->view.cy;
    int saved_coloff = ctx->view.coloff, saved_rowoff = ctx->view.rowoff;

    while(1) {
        editor_set_status_msg(ctx,
            "Search: %s (Use ESC/Arrows/Enter)", query);
        editor_refresh_screen(ctx);

        int c = terminal_read_key(fd);
        if (c == -1) c = ESC;  /* Input error: abort the search like ESC. */
        if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
            if (qlen != 0) query[--qlen] = '\0';
            last_match = -1;
        } else if (c == ESC || c == ENTER) {
            if (c == ESC) {
                ctx->view.cx = saved_cx; ctx->view.cy = saved_cy;
                ctx->view.coloff = saved_coloff; ctx->view.rowoff = saved_rowoff;
            }
            FIND_RESTORE_HL;
            editor_set_status_msg(ctx, "");
            return;
        } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            find_next = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            find_next = -1;
        } else if (isprint(c)) {
            if (qlen < KILO_QUERY_LEN) {
                query[qlen++] = c;
                query[qlen] = '\0';
                last_match = -1;
            }
        }

        /* Search occurrence. */
        if (last_match == -1) find_next = 1;
        if (find_next) {
            char *match = NULL;
            int match_offset = 0;
            int i, current = last_match;

            for (i = 0; i < ctx->model.numrows; i++) {
                current += find_next;
                if (current == -1) current = ctx->model.numrows-1;
                else if (current == ctx->model.numrows) current = 0;
                match = strstr(ctx->model.row[current].render,query);
                if (match) {
                    match_offset = match-ctx->model.row[current].render;
                    break;
                }
            }
            find_next = 0;

            /* Highlight */
            FIND_RESTORE_HL;

            if (match) {
                t_erow *row = &ctx->model.row[current];
                last_match = current;
                if (row->hl && row->rsize > 0) {
                    saved_hl = malloc(row->rsize);
                    if (saved_hl) {
                        saved_hl_line = current;
                        memcpy(saved_hl,row->hl,row->rsize);
                        /* Only highlight what we can put back afterwards. */
                        int n = qlen;
                        if (match_offset + n > row->rsize)
                            n = row->rsize - match_offset;
                        if (n > 0) memset(row->hl+match_offset,HL_MATCH,n);
                    }
                }
                ctx->view.cy = 0;
                ctx->view.cx = match_offset;
                ctx->view.rowoff = current;
                ctx->view.coloff = 0;
                /* Scroll horizontally as needed. */
                if (ctx->view.cx > ctx->view.screencols) {
                    int diff = ctx->view.cx - ctx->view.screencols;
                    ctx->view.cx -= diff;
                    ctx->view.coloff += diff;
                }
            }
        }
    }
}
