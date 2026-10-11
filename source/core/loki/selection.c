/* loki_selection.c - Text selection and clipboard functionality
 *
 * This module handles visual text selection and clipboard operations using
 * OSC 52 escape sequences. OSC 52 allows terminal-based clipboard access
 * that works over SSH and doesn't require X11 or platform-specific APIs.
 *
 * Features:
 * - Visual selection checking (is position within selection?)
 * - Base64 encoding for OSC 52 clipboard protocol
 * - Copy selection to clipboard using terminal escape sequences
 *
 * OSC 52 Protocol:
 * - Sequence: ESC]52;c;<base64_text>BEL
 * - Supported by: xterm, iTerm2, tmux, screen, kitty, alacritty
 * - Works over SSH without X11 forwarding
 */

#include "selection.h"
#include "internal.h"
#include "undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Base64 encoding table for OSC 52 clipboard protocol */
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Check if a position (row, col) is within the current selection.
 * Returns 1 if selected, 0 otherwise.
 * Handles both single-line and multi-line selections. */
int is_selected(editor_ctx_t *ctx, int row, int col) {
    if (!ctx->view.sel_active) return 0;

    int start_y = ctx->view.sel_start_y;
    int start_x = ctx->view.sel_start_x;
    int end_y = ctx->view.sel_end_y;
    int end_x = ctx->view.sel_end_x;

    /* Ensure start comes before end */
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Check if row is in range */
    if (row < start_y || row > end_y) return 0;

    /* Single line selection */
    if (start_y == end_y) {
        return col >= start_x && col < end_x;
    }

    /* Multi-line selection */
    if (row == start_y) {
        return col >= start_x;
    } else if (row == end_y) {
        return col < end_x;
    } else {
        return 1; /* Entire line selected */
    }
}

/* Check if a position is within the selection defined by a view.
 * Used for rendering split panes with independent selection state. */
int is_selected_view(const EditorView *view, int row, int col) {
    if (!view || !view->sel_active) return 0;

    int start_y = view->sel_start_y;
    int start_x = view->sel_start_x;
    int end_y = view->sel_end_y;
    int end_x = view->sel_end_x;

    /* Ensure start comes before end */
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Check if row is in range */
    if (row < start_y || row > end_y) return 0;

    /* Single line selection */
    if (start_y == end_y) {
        return col >= start_x && col < end_x;
    }

    /* Multi-line selection */
    if (row == start_y) {
        return col >= start_x;
    } else if (row == end_y) {
        return col < end_x;
    } else {
        return 1; /* Entire line selected */
    }
}

/* Base64 encode a string for OSC 52 clipboard protocol.
 * Caller must free the returned string.
 * Returns NULL on allocation failure. */
char *base64_encode(const char *input, size_t len) {
    size_t output_len = 4 * ((len + 2) / 3);
    char *output = malloc(output_len + 1);
    if (!output) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? (unsigned char)input[i++] : 0;
        uint32_t octet_b = i < len ? (unsigned char)input[i++] : 0;
        uint32_t octet_c = i < len ? (unsigned char)input[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = base64_table[(triple >> 6) & 0x3F];
        output[j++] = base64_table[triple & 0x3F];
    }

    /* Add padding */
    for (i = 0; i < (3 - len % 3) % 3; i++)
        output[output_len - 1 - i] = '=';

    output[output_len] = '\0';
    return output;
}

/* Copy text to clipboard using renderer abstraction or fallback to OSC-52.
 * This allows different clipboard implementations for different frontends. */
static int clipboard_copy_text(editor_ctx_t *ctx, const char *text, size_t len) {
    /* Use renderer if available */
    if (ctx->renderer && ctx->renderer->clipboard_copy) {
        return ctx->renderer->clipboard_copy(ctx->renderer, text, len);
    }

    /* Fallback: Direct OSC-52 sequence (terminal-specific)
     * This works over SSH and doesn't require X11 or platform-specific APIs. */
    char *encoded = base64_encode(text, len);
    if (!encoded) return -1;

    printf("\033]52;c;%s\007", encoded);
    fflush(stdout);
    free(encoded);
    return 0;
}

/* Copy selected text to clipboard.
 * Uses renderer abstraction if available, otherwise falls back to OSC-52.
 * Clears the selection after successful copy. */
void copy_selection_to_clipboard(editor_ctx_t *ctx) {
    if (!ctx->view.sel_active) {
        editor_set_status_msg(ctx, "No selection");
        return;
    }

    char *text = get_selection_text(ctx);
    if (!text) return;
    size_t text_len = strlen(text);

    /* Copy to clipboard using abstraction */
    int result = clipboard_copy_text(ctx, text, text_len);
    free(text);

    if (result == 0) {
        editor_set_status_msg(ctx, "Copied %d bytes to clipboard", (int)text_len);
        ctx->view.sel_active = 0;  /* Clear selection after copy */
    } else {
        editor_set_status_msg(ctx, "Failed to copy to clipboard");
    }
}


/* Get selected text as a newly allocated string.
 * Caller must free the returned string.
 * Returns NULL if no selection or allocation failure. */
char *get_selection_text(editor_ctx_t *ctx) {
    if (!ctx->view.sel_active) {
        return NULL;
    }

    /* Ensure start comes before end */
    int start_y = ctx->view.sel_start_y;
    int start_x = ctx->view.sel_start_x;
    int end_y = ctx->view.sel_end_y;
    int end_x = ctx->view.sel_end_x;

    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Build selected text */
    char *text = NULL;
    size_t text_len = 0;
    size_t text_capacity = 1024;
    text = malloc(text_capacity);
    if (!text) return NULL;

    for (int y = start_y; y <= end_y && y < ctx->model.numrows; y++) {
        int x_start = (y == start_y) ? start_x : 0;
        int x_end = (y == end_y) ? end_x : ctx->model.row[y].size;
        if (x_end > ctx->model.row[y].size) x_end = ctx->model.row[y].size;

        int len = x_end - x_start;
        int need = len + (y < end_y ? 1 : 0) + 1;  /* + separator + NUL */
        while (text_len + need > text_capacity) {
            text_capacity *= 2;
            char *new_text = realloc(text, text_capacity);
            if (!new_text) { free(text); return NULL; }
            text = new_text;
        }
        if (len > 0) {
            memcpy(text + text_len, ctx->model.row[y].chars + x_start, len);
            text_len += len;
        }
        if (y < end_y) {
            text[text_len++] = '\n';
        }
    }
    text[text_len] = '\0';

    return text;
}

/* Delete selected text from the buffer.
 * Records the deleted range as a single undo entry.
 * Clears selection and positions cursor at selection start.
 * Returns number of characters deleted, or 0 if no selection. */
int delete_selection(editor_ctx_t *ctx) {
    if (!ctx->view.sel_active || ctx->model.numrows == 0) {
        return 0;
    }

    /* Normalize selection: ensure start comes before end */
    int start_y = ctx->view.sel_start_y;
    int start_x = ctx->view.sel_start_x;
    int end_y = ctx->view.sel_end_y;
    int end_x = ctx->view.sel_end_x;

    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Bounds checking */
    if (start_y >= ctx->model.numrows) start_y = ctx->model.numrows - 1;
    if (end_y >= ctx->model.numrows) end_y = ctx->model.numrows - 1;
    if (start_y < 0) start_y = 0;
    if (end_y < 0) end_y = 0;

    t_erow *start_row = &ctx->model.row[start_y];
    t_erow *end_row = &ctx->model.row[end_y];

    if (start_x > start_row->size) start_x = start_row->size;
    if (end_x > end_row->size) end_x = end_row->size;
    if (start_x < 0) start_x = 0;
    if (end_x < 0) end_x = 0;

    /* Snapshot the region before touching the buffer so the delete can be
     * undone. get_selection_text() reads the selection fields, which are
     * still intact at this point. */
    ctx->view.sel_start_y = start_y;
    ctx->view.sel_start_x = start_x;
    ctx->view.sel_end_y = end_y;
    ctx->view.sel_end_x = end_x;
    char *deleted_text = get_selection_text(ctx);
    int deleted_chars = 0;
    if (deleted_text) {
        deleted_chars = (int)strlen(deleted_text);
        undo_record_delete_block(ctx, start_y, start_x,
                                 deleted_text, deleted_chars);
    }

    /* Clear selection before modifying buffer */
    ctx->view.sel_active = 0;

    if (start_y == end_y) {
        /* Single line selection: delete characters from start_x to end_x */
        t_erow *row = &ctx->model.row[start_y];

        /* Delete characters from end to start to avoid index shifting issues */
        for (int i = end_x - 1; i >= start_x; i--) {
            if (i < row->size) {
                editor_row_del_char(ctx, row, i);
            }
        }
    } else {
        /* Multi-line selection */

        /* 1. Save text after end_x on end row (will be appended to start row) */
        char *remaining = NULL;
        int remaining_len = 0;
        if (end_x < end_row->size) {
            remaining_len = end_row->size - end_x;
            remaining = malloc(remaining_len);
            if (remaining) {
                memcpy(remaining, end_row->chars + end_x, remaining_len);
            }
        }

        /* 2. Truncate start row to start_x */
        start_row->chars[start_x] = '\0';
        start_row->size = start_x;
        editor_update_row(ctx, start_row);

        /* 3. Delete middle rows and end row (from end to start to avoid index shifting) */
        for (int y = end_y; y > start_y; y--) {
            editor_del_row(ctx, y);
        }

        /* 4. Append remaining text from end row to start row */
        if (remaining && remaining_len > 0) {
            /* Refresh start_row pointer after possible reallocation */
            start_row = &ctx->model.row[start_y];
            editor_row_append_string(ctx, start_row, remaining, remaining_len);
        }
        free(remaining);
    }

    /* Position cursor at start of deleted region */
    ctx->view.cy = start_y - ctx->view.rowoff;
    if (ctx->view.cy < 0) {
        ctx->view.rowoff = start_y;
        ctx->view.cy = 0;
    } else if (ctx->view.cy >= ctx->view.screenrows) {
        ctx->view.rowoff = start_y - ctx->view.screenrows + 1;
        ctx->view.cy = ctx->view.screenrows - 1;
    }

    ctx->view.cx = start_x - ctx->view.coloff;
    if (ctx->view.cx < 0) {
        ctx->view.coloff = start_x;
        ctx->view.cx = 0;
    }

    ctx->model.dirty++;

    free(deleted_text);
    return deleted_chars;
}
