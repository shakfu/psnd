/* loki_undo.c - Undo/Redo state management
 *
 * Implements undo/redo with operation grouping and memory limits.
 * Operations are grouped by heuristics (time gap, cursor movement, operation type).
 */

#include "undo.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
static char* strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* result = (char*)malloc(len + 1);
    if (result) { memcpy(result, s, len); result[len] = '\0'; }
    return result;
}
#endif

/* Undo grouping heuristics */
#define UNDO_GROUP_TIMEOUT 2         /* 2 seconds gap = new group */
#define UNDO_GROUP_MOVEMENT_GAP 2    /* Cursor moved >2 positions = new group */

/* Undo state (private to this module) */
struct undo_state {
    undo_entry_t *entries;   /* Circular buffer of undo entries */
    int capacity;            /* Max entries (e.g., 1000) */
    int count;               /* Current number of entries */
    int head;                /* Write position (next entry goes here) */
    int current;             /* Current position in undo stack */

    int next_group_id;       /* Next group ID to assign */
    int current_group_id;    /* Current operation group */

    /* Grouping heuristics */
    time_t last_edit_time;   /* Timestamp of last edit */
    int last_edit_row;       /* Row of last edit */
    int last_edit_col;       /* Column of last edit */
    undo_op_type_t last_op;  /* Type of last operation */

    /* Memory tracking */
    size_t memory_used;      /* Bytes used by undo data */
    size_t memory_limit;     /* Max bytes (e.g., 10MB) */
};

/* ======================== Initialization ======================== */

void undo_init(editor_ctx_t *ctx, int capacity, size_t memory_limit) {
    /* editor_ctx_init() installs a default undo_state, so callers that want
     * different limits re-init over one that already exists. Overwriting the
     * pointer leaked it. */
    undo_free(ctx);

    struct undo_state *undo = malloc(sizeof(struct undo_state));
    if (!undo) return;

    undo->entries = calloc(capacity, sizeof(undo_entry_t));
    if (!undo->entries) {
        free(undo);
        return;
    }

    undo->capacity = capacity;
    undo->count = 0;
    undo->head = 0;
    undo->current = 0;
    undo->next_group_id = 1;
    undo->current_group_id = 0;
    undo->last_edit_time = 0;
    undo->last_edit_row = -1;
    undo->last_edit_col = -1;
    undo->last_op = (undo_op_type_t)-1;
    undo->memory_used = 0;
    undo->memory_limit = memory_limit;

    ctx->model.undo_state = undo;
}

/* Does this operation type own a heap-allocated line_op.content? */
static int entry_owns_content(undo_op_type_t type) {
    return type == UNDO_INSERT_LINE || type == UNDO_DELETE_LINE ||
           type == UNDO_DELETE_BLOCK;
}

/* Physical slot of the i'th live entry (0 = oldest) in the circular buffer. */
static int entry_slot(const struct undo_state *undo, int i) {
    return (undo->head - undo->count + i + undo->capacity) % undo->capacity;
}

void undo_free(editor_ctx_t *ctx) {
    if (!ctx->model.undo_state) return;

    struct undo_state *undo = ctx->model.undo_state;

    /* Free all line_op content strings. Live entries are at the circular
     * offsets given by entry_slot(), not at 0..count-1 -- iterating linearly
     * leaked every entry once the ring had wrapped. */
    for (int i = 0; i < undo->count; i++) {
        undo_entry_t *e = &undo->entries[entry_slot(undo, i)];
        if (entry_owns_content(e->type) && e->data.line_op.content) {
            free(e->data.line_op.content);
            e->data.line_op.content = NULL;
        }
    }

    free(undo->entries);
    free(undo);
    ctx->model.undo_state = NULL;
}

/* ======================== Grouping Logic ======================== */

/* Should we start a new undo group for this operation? */
static int should_break_group(struct undo_state *undo, undo_op_type_t op,
                               int row, int col) {
    if (undo->current_group_id == 0) return 1;  /* First operation */

    /* Time gap check */
    time_t now = time(NULL);
    if (now - undo->last_edit_time > UNDO_GROUP_TIMEOUT) {
        return 1;
    }

    /* Operation type change (insert→delete or vice versa) */
    if (undo->last_op == UNDO_INSERT_CHAR && op == UNDO_DELETE_CHAR) return 1;
    if (undo->last_op == UNDO_DELETE_CHAR && op == UNDO_INSERT_CHAR) return 1;

    /* Line and block operations always break groups */
    if (op == UNDO_INSERT_LINE || op == UNDO_DELETE_LINE ||
        op == UNDO_DELETE_BLOCK) return 1;

    /* Cursor jumped (user moved cursor manually) */
    if (undo->last_edit_row != row) return 1;

    int col_gap = abs(col - undo->last_edit_col);
    if (col_gap > UNDO_GROUP_MOVEMENT_GAP) return 1;

    return 0;  /* Continue current group */
}

void undo_break_group(editor_ctx_t *ctx) {
    if (!ctx->model.undo_state) return;

    struct undo_state *undo = ctx->model.undo_state;
    if (undo->count > 0) {
        int last_idx = (undo->head - 1 + undo->capacity) % undo->capacity;
        undo->entries[last_idx].group_break = 1;
    }
    undo->current_group_id = 0;  /* Force new group on next operation */
}

/* ======================== Recording Operations ======================== */

static void free_entry_data(undo_entry_t *entry, struct undo_state *undo) {
    if (entry_owns_content(entry->type) && entry->data.line_op.content) {
        undo->memory_used -= entry->data.line_op.length;
        free(entry->data.line_op.content);
        entry->data.line_op.content = NULL;
    }
}

static void record_operation(editor_ctx_t *ctx, undo_entry_t *entry) {
    struct undo_state *undo = ctx->model.undo_state;
    if (!undo) return;

    /* Check if we should start new group */
    if (should_break_group(undo, entry->type, entry->row, entry->col)) {
        undo->current_group_id = undo->next_group_id++;
    }

    entry->group_id = undo->current_group_id;
    entry->group_break = 0;

    /* Update grouping heuristics */
    undo->last_edit_time = time(NULL);
    undo->last_edit_row = entry->row;
    undo->last_edit_col = entry->col;
    undo->last_op = entry->type;

    /* If we've undone operations, discard redo history */
    if (undo->current < undo->count) {
        /* Free any line content in discarded entries */
        for (int i = undo->current; i < undo->count; i++) {
            int idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
            free_entry_data(&undo->entries[idx], undo);
        }
        undo->count = undo->current;
    }

    /* Add entry to circular buffer */
    if (undo->count == undo->capacity) {
        /* Buffer full - evict oldest entry */
        int evict_idx = undo->head;
        free_entry_data(&undo->entries[evict_idx], undo);
    } else {
        undo->count++;
    }

    /* Write entry */
    undo->entries[undo->head] = *entry;
    undo->head = (undo->head + 1) % undo->capacity;
    undo->current = undo->count;

    /* Track memory for content-carrying operations */
    if (entry_owns_content(entry->type)) {
        undo->memory_used += entry->data.line_op.length;
    }

    /* Enforce the memory limit by dropping the oldest history. Keep at least
     * one entry so the most recent edit always stays undoable. */
    while (undo->memory_limit > 0 && undo->memory_used > undo->memory_limit &&
           undo->count > 1) {
        int oldest = entry_slot(undo, 0);
        free_entry_data(&undo->entries[oldest], undo);
        undo->count--;
        if (undo->current > undo->count) undo->current = undo->count;
    }
}

void undo_record_insert_char(editor_ctx_t *ctx, int row, int col, char ch) {
    if (!ctx->model.undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_INSERT_CHAR,
        .row = row,
        .col = col,
        .data.char_op.ch = ch,
        .cursor_row = ctx->view.cy,
        .cursor_col = ctx->view.cx,
        .cursor_rowoff = ctx->view.rowoff,
        .cursor_coloff = ctx->view.coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_delete_char(editor_ctx_t *ctx, int row, int col, char ch) {
    if (!ctx->model.undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_DELETE_CHAR,
        .row = row,
        .col = col,
        .data.char_op.ch = ch,
        .cursor_row = ctx->view.cy,
        .cursor_col = ctx->view.cx,
        .cursor_rowoff = ctx->view.rowoff,
        .cursor_coloff = ctx->view.coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_delete_block(editor_ctx_t *ctx, int row, int col,
                              const char *content, int length) {
    if (!ctx->model.undo_state || length <= 0) return;

    undo_entry_t entry = {
        .type = UNDO_DELETE_BLOCK,
        .row = row,
        .col = col,
        .data.line_op.content = strndup(content, length),
        .data.line_op.length = length,
        .cursor_row = ctx->view.cy,
        .cursor_col = ctx->view.cx,
        .cursor_rowoff = ctx->view.rowoff,
        .cursor_coloff = ctx->view.coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_insert_line(editor_ctx_t *ctx, int row, int col,
                              const char *content, int length) {
    if (!ctx->model.undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_INSERT_LINE,
        .row = row,
        .col = col,
        .data.line_op.content = strndup(content, length),
        .data.line_op.length = length,
        .cursor_row = ctx->view.cy,
        .cursor_col = ctx->view.cx,
        .cursor_rowoff = ctx->view.rowoff,
        .cursor_coloff = ctx->view.coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_delete_line(editor_ctx_t *ctx, int row, int col,
                              const char *content, int length) {
    if (!ctx->model.undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_DELETE_LINE,
        .row = row,
        .col = col,
        .data.line_op.content = strndup(content, length),
        .data.line_op.length = length,
        .cursor_row = ctx->view.cy,
        .cursor_col = ctx->view.cx,
        .cursor_rowoff = ctx->view.rowoff,
        .cursor_coloff = ctx->view.coloff
    };

    record_operation(ctx, &entry);
}

/* ======================== Undo/Redo Operations ======================== */


/* ---------------- Multi-line range helpers (for UNDO_DELETE_BLOCK) --------- */

/* Insert 'text' (which may contain '\n') into the buffer at (row, col).
 * Rows are split/joined as needed. Used to undo a block deletion. */
static void insert_text_at(editor_ctx_t *ctx, int row, int col,
                           const char *text, int length) {
    if (row < 0 || row > ctx->model.numrows) return;

    /* An insertion point past the last row means appending new rows. */
    if (row == ctx->model.numrows) {
        editor_insert_row(ctx, row, "", 0);
    }

    t_erow *target = &ctx->model.row[row];
    if (col < 0) col = 0;
    if (col > target->size) col = target->size;

    /* Detach the part of the row that follows the insertion point; it has to
     * end up after the last inserted line. */
    int tail_len = target->size - col;
    char *tail = malloc(tail_len + 1);
    if (!tail) return;
    memcpy(tail, target->chars + col, tail_len);
    tail[tail_len] = '\0';

    target->chars[col] = '\0';
    target->size = col;
    editor_update_row(ctx, target);

    /* Walk the text one '\n'-separated segment at a time. */
    int cur_row = row;
    int seg_start = 0;
    for (int i = 0; i <= length; i++) {
        if (i == length || text[i] == '\n') {
            int seg_len = i - seg_start;
            if (seg_start == 0) {
                /* First segment continues the row we split. */
                editor_row_append_string(ctx, &ctx->model.row[cur_row],
                                         (char *)text + seg_start, seg_len);
            } else {
                cur_row++;
                editor_insert_row(ctx, cur_row, (char *)text + seg_start, seg_len);
            }
            seg_start = i + 1;
        }
    }

    /* Re-attach the tail to the final row. */
    if (tail_len > 0) {
        editor_row_append_string(ctx, &ctx->model.row[cur_row], tail, tail_len);
    }
    free(tail);
}

/* Delete 'length' bytes starting at (row, col), where a row boundary counts
 * as one byte ('\n'). Used to redo a block deletion. */
static void delete_range_at(editor_ctx_t *ctx, int row, int col, int length) {
    if (row < 0 || row >= ctx->model.numrows || length <= 0) return;

    t_erow *cur = &ctx->model.row[row];
    if (col < 0) col = 0;
    if (col > cur->size) col = cur->size;

    int remaining = length;
    while (remaining > 0 && row < ctx->model.numrows) {
        cur = &ctx->model.row[row];
        int avail = cur->size - col;   /* bytes left on this row */

        if (remaining <= avail) {
            memmove(cur->chars + col, cur->chars + col + remaining,
                    cur->size - col - remaining);
            cur->size -= remaining;
            cur->chars[cur->size] = '\0';
            editor_update_row(ctx, cur);
            remaining = 0;
        } else {
            /* Drop the rest of this row plus its newline, then merge the
             * following row up. */
            cur->chars[col] = '\0';
            cur->size = col;
            editor_update_row(ctx, cur);
            remaining -= avail + 1;
            if (row + 1 < ctx->model.numrows) {
                t_erow *next = &ctx->model.row[row + 1];
                editor_row_append_string(ctx, cur, next->chars, next->size);
                editor_del_row(ctx, row + 1);
            } else {
                break;
            }
        }
    }
}

/* Apply single undo operation (reverse the operation) */
static void apply_undo(editor_ctx_t *ctx, undo_entry_t *entry) {
    /* Suppress undo recording while applying undo */
    struct undo_state *saved_state = ctx->model.undo_state;
    ctx->model.undo_state = NULL;

    t_erow *row;

    switch (entry->type) {
        case UNDO_INSERT_CHAR:
            /* Undo insert = delete the character */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                row = &ctx->model.row[entry->row];
                if (entry->col >= 0 && entry->col < row->size) {
                    editor_row_del_char(ctx, row, entry->col);
                }
            }
            break;

        case UNDO_DELETE_CHAR:
            /* Undo delete = re-insert the character */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                row = &ctx->model.row[entry->row];
                editor_row_insert_char(ctx, row, entry->col, entry->data.char_op.ch);
            }
            break;

        case UNDO_INSERT_LINE:
            /* Undo a line split at (row, col) = join row+1 back onto row.
             * This covers all three record sites in editor_insert_newline:
             * a split mid-line, an empty row inserted at column 0 (the
             * original text sits at row+1), and an empty row appended at
             * EOF (no row+1 exists, so the row itself is removed). */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                if (entry->row + 1 < ctx->model.numrows) {
                    t_erow *tail = &ctx->model.row[entry->row + 1];
                    editor_row_append_string(ctx, &ctx->model.row[entry->row],
                                             tail->chars, tail->size);
                    editor_del_row(ctx, entry->row + 1);
                } else {
                    editor_del_row(ctx, entry->row);
                }
            }
            break;

        case UNDO_DELETE_LINE:
            /* Undo a line merge = split row back at the recorded join column.
             * Re-inserting the tail is not enough: the text merged into
             * row must also be truncated away, or it ends up duplicated. */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                editor_insert_row(ctx, entry->row + 1,
                                 entry->data.line_op.content,
                                 entry->data.line_op.length);
                row = &ctx->model.row[entry->row];
                if (entry->col >= 0 && entry->col < row->size) {
                    row->chars[entry->col] = '\0';
                    row->size = entry->col;
                    editor_update_row(ctx, row);
                }
            }
            break;

        case UNDO_DELETE_BLOCK:
            /* Undo a range delete = put the text back where it was. */
            insert_text_at(ctx, entry->row, entry->col,
                           entry->data.line_op.content,
                           entry->data.line_op.length);
            break;
    }

    /* Restore cursor position from before the operation */
    ctx->view.cy = entry->cursor_row;
    ctx->view.cx = entry->cursor_col;
    ctx->view.rowoff = entry->cursor_rowoff;
    ctx->view.coloff = entry->cursor_coloff;

    /* Restore undo state */
    ctx->model.undo_state = saved_state;
}

/* Apply single redo operation (replay the operation) */
static void apply_redo(editor_ctx_t *ctx, undo_entry_t *entry) {
    /* Suppress undo recording while applying redo */
    struct undo_state *saved_state = ctx->model.undo_state;
    ctx->model.undo_state = NULL;

    t_erow *row;

    switch (entry->type) {
        case UNDO_INSERT_CHAR:
            /* Redo insert = insert the character again */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                row = &ctx->model.row[entry->row];
                editor_row_insert_char(ctx, row, entry->col, entry->data.char_op.ch);
            }
            break;

        case UNDO_DELETE_CHAR:
            /* Redo delete = delete the character again */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                row = &ctx->model.row[entry->row];
                if (entry->col >= 0 && entry->col < row->size) {
                    editor_row_del_char(ctx, row, entry->col);
                }
            }
            break;

        case UNDO_INSERT_LINE:
            /* Redo a line split: move the tail past 'col' onto a new row and
             * truncate the head, mirroring editor_insert_newline. */
            if (entry->row >= 0 && entry->row < ctx->model.numrows) {
                row = &ctx->model.row[entry->row];
                int col = entry->col;
                if (col < 0) col = 0;
                if (col > row->size) col = row->size;
                editor_insert_row(ctx, entry->row + 1, row->chars + col,
                                 row->size - col);
                row = &ctx->model.row[entry->row];  /* insert may realloc */
                row->chars[col] = '\0';
                row->size = col;
                editor_update_row(ctx, row);
            } else if (entry->row == ctx->model.numrows) {
                /* Newline appended past the last row. */
                editor_insert_row(ctx, entry->row, "", 0);
            }
            break;

        case UNDO_DELETE_LINE:
            /* Redo a line merge: append row+1 onto row before removing it,
             * otherwise the merged text is dropped. */
            if (entry->row >= 0 && entry->row + 1 < ctx->model.numrows) {
                t_erow *tail = &ctx->model.row[entry->row + 1];
                editor_row_append_string(ctx, &ctx->model.row[entry->row],
                                         tail->chars, tail->size);
                editor_del_row(ctx, entry->row + 1);
            }
            break;

        case UNDO_DELETE_BLOCK:
            delete_range_at(ctx, entry->row, entry->col,
                            entry->data.line_op.length);
            break;
    }

    /* Restore undo state */
    ctx->model.undo_state = saved_state;
}

int undo_perform(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->model.undo_state;
    if (!undo || undo->current == 0) return 0;  /* Nothing to undo */

    /* Find start of current group */
    int target_group = -1;
    int undo_idx = undo->current - 1;

    for (int i = undo->current - 1; i >= 0; i--) {
        int idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        undo_entry_t *e = &undo->entries[idx];

        if (target_group == -1) {
            target_group = e->group_id;
        } else if (e->group_id != target_group || e->group_break) {
            break;  /* Different group or explicit break, stop here */
        }
        undo_idx = i;
    }

    /* Undo all operations in this group (in reverse order) */
    for (int i = undo->current - 1; i >= undo_idx; i--) {
        int idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        apply_undo(ctx, &undo->entries[idx]);
    }

    undo->current = undo_idx;
    ctx->model.dirty++;
    return 1;
}

int redo_perform(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->model.undo_state;
    if (!undo || undo->current >= undo->count) return 0;  /* Nothing to redo */

    /* Find end of next group */
    int idx = (undo->head - undo->count + undo->current + undo->capacity) % undo->capacity;
    int target_group = undo->entries[idx].group_id;
    int redo_end = undo->current;

    for (int i = undo->current; i < undo->count; i++) {
        idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        undo_entry_t *e = &undo->entries[idx];

        redo_end = i + 1;

        if (e->group_break || (i + 1 < undo->count &&
            undo->entries[(undo->head - undo->count + i + 1 + undo->capacity) % undo->capacity].group_id != target_group)) {
            break;  /* End of group */
        }
    }

    /* Redo all operations in this group (in forward order) */
    for (int i = undo->current; i < redo_end; i++) {
        idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        apply_redo(ctx, &undo->entries[idx]);
    }

    undo->current = redo_end;
    ctx->model.dirty++;
    return 1;
}

/* ======================== Query Functions ======================== */

int undo_can_undo(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->model.undo_state;
    return undo && undo->current > 0;
}

int undo_can_redo(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->model.undo_state;
    return undo && undo->current < undo->count;
}

void undo_clear(editor_ctx_t *ctx) {
    if (!ctx->model.undo_state) return;

    struct undo_state *undo = ctx->model.undo_state;

    /* Free all line content */
    for (int i = 0; i < undo->count; i++) {
        free_entry_data(&undo->entries[i], undo);
    }

    undo->count = 0;
    undo->head = 0;
    undo->current = 0;
    undo->memory_used = 0;
    undo->current_group_id = 0;
}

void undo_get_stats(editor_ctx_t *ctx, int *undo_levels,
                     int *redo_levels, size_t *memory) {
    struct undo_state *undo = ctx->model.undo_state;
    if (!undo) {
        if (undo_levels) *undo_levels = 0;
        if (redo_levels) *redo_levels = 0;
        if (memory) *memory = 0;
        return;
    }

    if (undo_levels) *undo_levels = undo->current;
    if (redo_levels) *redo_levels = undo->count - undo->current;
    if (memory) *memory = undo->memory_used;
}
