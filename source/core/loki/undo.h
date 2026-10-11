/* loki_undo.h - Undo/Redo system interface
 *
 * This module implements undo/redo with operation grouping and memory limits.
 * Operations are grouped by heuristics (time gap, cursor movement, operation type).
 */

#ifndef LOKI_UNDO_H
#define LOKI_UNDO_H

#include "loki/core.h"
#include <stddef.h>
#include <time.h>

/* Operation types that can be undone */
typedef enum {
    UNDO_INSERT_CHAR,    /* Insert single character */
    UNDO_DELETE_CHAR,    /* Delete single character */
    UNDO_INSERT_LINE,    /* Insert newline (split line) */
    UNDO_DELETE_LINE,    /* Delete newline (merge lines) */
    UNDO_DELETE_BLOCK,   /* Delete a multi-character/multi-line range */
} undo_op_type_t;

/* Single undo operation */
typedef struct {
    undo_op_type_t type;     /* Operation type */
    int row;                 /* Row where operation occurred (file coordinates) */
    int col;                 /* Column where operation occurred */

    /* Operation-specific data */
    union {
        struct {
            char ch;         /* Character inserted/deleted */
        } char_op;

        struct {
            char *content;   /* Line content (for line ops) */
            int length;      /* Content length */
        } line_op;
    } data;

    /* Cursor position before operation (for undo restoration) */
    int cursor_row;
    int cursor_col;
    int cursor_rowoff;
    int cursor_coloff;

    /* Grouping information */
    int group_id;            /* Operations with same group_id undo together */
    int group_break;         /* 1 = explicit break after this operation */
} undo_entry_t;

/* Undo state (opaque - definition in loki_undo.c) */
struct undo_state;

/* ======================== Public API ======================== */

/* Initialize undo system
 * capacity: Max number of undo operations (e.g., 1000)
 * memory_limit: Max bytes for line content (e.g., 10MB) */
void undo_init(editor_ctx_t *ctx, int capacity, size_t memory_limit);

/* Free undo system resources */
void undo_free(editor_ctx_t *ctx);

/* Record an edit operation (called by editor_insert_char, etc.)
 * These save cursor position BEFORE the operation for proper undo restoration */
void undo_record_insert_char(editor_ctx_t *ctx, int row, int col, char ch);
void undo_record_delete_char(editor_ctx_t *ctx, int row, int col, char ch);
void undo_record_insert_line(editor_ctx_t *ctx, int row, int col, const char *content, int length);
void undo_record_delete_line(editor_ctx_t *ctx, int row, int col, const char *content, int length);

/* Record deletion of a text range starting at (row, col). 'content' is the
 * deleted text, with embedded '\n' for row boundaries. Recorded as one entry
 * so a large selection delete does not flood the undo ring. */
void undo_record_delete_block(editor_ctx_t *ctx, int row, int col, const char *content, int length);

/* Force start of new undo group (e.g., after mode change, after delay) */
void undo_break_group(editor_ctx_t *ctx);

/* Undo last operation/group
 * Returns: 1 if undo performed, 0 if nothing to undo */
int undo_perform(editor_ctx_t *ctx);

/* Redo previously undone operation/group
 * Returns: 1 if redo performed, 0 if nothing to redo */
int redo_perform(editor_ctx_t *ctx);

/* Check if undo/redo available */
int undo_can_undo(editor_ctx_t *ctx);
int undo_can_redo(editor_ctx_t *ctx);

/* Clear all undo history (e.g., after file save or major change) */
void undo_clear(editor_ctx_t *ctx);

/* Get undo statistics (for debugging/status display) */
void undo_get_stats(editor_ctx_t *ctx, int *undo_levels, int *redo_levels, size_t *memory);

#endif /* LOKI_UNDO_H */
