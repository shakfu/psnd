/**
 * tracker_view_clipboard.c - Clipboard and selection operations
 */

#include "tracker_view.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Selection Functions
 *============================================================================*/

void tracker_view_select_start(TrackerView* view) {
    if (!view) return;

    view->state.selection.type = TRACKER_SEL_CELL;
    view->state.selection.anchor_track = view->state.cursor_track;
    view->state.selection.anchor_row = view->state.cursor_row;
    view->state.selection.start_track = view->state.cursor_track;
    view->state.selection.end_track = view->state.cursor_track;
    view->state.selection.start_row = view->state.cursor_row;
    view->state.selection.end_row = view->state.cursor_row;
    view->state.selection.start_pattern = view->state.cursor_pattern;
    view->state.selection.end_pattern = view->state.cursor_pattern;
    view->state.selecting = true;

    tracker_view_invalidate_selection(view);
}

void tracker_view_select_extend(TrackerView* view) {
    if (!view || !view->state.selecting) return;

    int anchor_track = view->state.selection.anchor_track;
    int anchor_row = view->state.selection.anchor_row;
    int cursor_track = view->state.cursor_track;
    int cursor_row = view->state.cursor_row;

    /* Calculate selection bounds */
    if (anchor_track <= cursor_track) {
        view->state.selection.start_track = anchor_track;
        view->state.selection.end_track = cursor_track;
    } else {
        view->state.selection.start_track = cursor_track;
        view->state.selection.end_track = anchor_track;
    }

    if (anchor_row <= cursor_row) {
        view->state.selection.start_row = anchor_row;
        view->state.selection.end_row = cursor_row;
    } else {
        view->state.selection.start_row = cursor_row;
        view->state.selection.end_row = anchor_row;
    }

    view->state.selection.type = TRACKER_SEL_RANGE;

    tracker_view_invalidate_selection(view);
}

void tracker_view_select_clear(TrackerView* view) {
    if (!view) return;

    view->state.selection.type = TRACKER_SEL_NONE;
    view->state.selecting = false;

    tracker_view_invalidate_selection(view);
}

void tracker_view_select_track(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    view->state.selection.type = TRACKER_SEL_TRACK;
    view->state.selection.anchor_track = view->state.cursor_track;
    view->state.selection.anchor_row = 0;
    view->state.selection.start_track = view->state.cursor_track;
    view->state.selection.end_track = view->state.cursor_track;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = pattern->num_rows - 1;
    view->state.selecting = false;

    tracker_view_invalidate_selection(view);
}

void tracker_view_select_row(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    view->state.selection.type = TRACKER_SEL_ROW;
    view->state.selection.anchor_track = 0;
    view->state.selection.anchor_row = view->state.cursor_row;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = pattern->num_tracks - 1;
    view->state.selection.start_row = view->state.cursor_row;
    view->state.selection.end_row = view->state.cursor_row;
    view->state.selecting = false;

    tracker_view_invalidate_selection(view);
}

void tracker_view_select_pattern(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    view->state.selection.type = TRACKER_SEL_PATTERN;
    view->state.selection.anchor_track = 0;
    view->state.selection.anchor_row = 0;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = pattern->num_tracks - 1;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = pattern->num_rows - 1;
    view->state.selecting = false;

    tracker_view_invalidate_selection(view);
}

void tracker_view_select_all(TrackerView* view) {
    tracker_view_select_pattern(view);
}

bool tracker_view_is_selected(TrackerView* view, int track, int row) {
    if (!view) return false;
    if (view->state.selection.type == TRACKER_SEL_NONE) return false;

    return track >= view->state.selection.start_track &&
           track <= view->state.selection.end_track &&
           row >= view->state.selection.start_row &&
           row <= view->state.selection.end_row;
}

bool tracker_view_get_selection(TrackerView* view,
                                int* out_start_track, int* out_end_track,
                                int* out_start_row, int* out_end_row) {
    if (!view) return false;
    if (view->state.selection.type == TRACKER_SEL_NONE) return false;

    if (out_start_track) *out_start_track = view->state.selection.start_track;
    if (out_end_track) *out_end_track = view->state.selection.end_track;
    if (out_start_row) *out_start_row = view->state.selection.start_row;
    if (out_end_row) *out_end_row = view->state.selection.end_row;

    return true;
}

/*============================================================================
 * Clipboard Functions
 *============================================================================*/

static void clipboard_clear_internal(TrackerClipboard* clip) {
    if (!clip) return;

    if (clip->owns_cells && clip->cells) {
        int count = clip->width * clip->height;
        for (int i = 0; i < count; i++) {
            tracker_cell_clear(&clip->cells[i]);
        }
        free(clip->cells);
    }

    clip->cells = NULL;
    clip->width = 0;
    clip->height = 0;
    clip->owns_cells = false;
}

void tracker_view_clipboard_clear(TrackerView* view) {
    if (!view) return;
    clipboard_clear_internal(&view->clipboard);
}

bool tracker_view_clipboard_has_content(TrackerView* view) {
    if (!view) return false;
    return view->clipboard.cells != NULL &&
           view->clipboard.width > 0 &&
           view->clipboard.height > 0;
}

bool tracker_view_copy(TrackerView* view) {
    if (!view || !view->song) return false;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return false;

    int start_track, end_track, start_row, end_row;

    /* Use selection if exists, otherwise copy cursor cell */
    if (view->state.selection.type != TRACKER_SEL_NONE) {
        start_track = view->state.selection.start_track;
        end_track = view->state.selection.end_track;
        start_row = view->state.selection.start_row;
        end_row = view->state.selection.end_row;
    } else {
        start_track = end_track = view->state.cursor_track;
        start_row = end_row = view->state.cursor_row;
    }

    /* Validate bounds */
    if (start_track < 0) start_track = 0;
    if (end_track >= pattern->num_tracks) end_track = pattern->num_tracks - 1;
    if (start_row < 0) start_row = 0;
    if (end_row >= pattern->num_rows) end_row = pattern->num_rows - 1;

    int width = end_track - start_track + 1;
    int height = end_row - start_row + 1;
    int count = width * height;

    /* Clear existing clipboard */
    clipboard_clear_internal(&view->clipboard);

    /* Allocate new clipboard */
    view->clipboard.cells = calloc(count, sizeof(TrackerCell));
    if (!view->clipboard.cells) return false;

    view->clipboard.width = width;
    view->clipboard.height = height;
    view->clipboard.owns_cells = true;

    /* Copy cells */
    int idx = 0;
    for (int r = start_row; r <= end_row; r++) {
        for (int t = start_track; t <= end_track; t++) {
            TrackerCell* src = tracker_pattern_get_cell(pattern, r, t);
            if (src) {
                tracker_cell_clone(&view->clipboard.cells[idx], src);
            } else {
                tracker_cell_init(&view->clipboard.cells[idx]);
            }
            idx++;
        }
    }

    return true;
}

bool tracker_view_cut(TrackerView* view) {
    if (!view || !view->song) return false;

    /* Copy first */
    if (!tracker_view_copy(view)) return false;

    /* Record undo for the clear */
    tracker_view_begin_undo_group(view, "Cut");

    /* Clear the selection */
    tracker_view_clear_selection(view);

    tracker_view_end_undo_group(view);

    return true;
}

bool tracker_view_paste(TrackerView* view) {
    if (!view || !view->song) return false;
    if (!tracker_view_clipboard_has_content(view)) return false;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return false;

    int paste_track = view->state.cursor_track;
    int paste_row = view->state.cursor_row;

    tracker_view_begin_undo_group(view, "Paste");

    /* Paste clipboard content */
    int idx = 0;
    for (int r = 0; r < view->clipboard.height; r++) {
        int target_row = paste_row + r;
        if (target_row >= pattern->num_rows) break;

        for (int t = 0; t < view->clipboard.width; t++) {
            int target_track = paste_track + t;
            if (target_track >= pattern->num_tracks) {
                idx++;
                continue;
            }

            TrackerCell* target = tracker_pattern_get_cell(pattern, target_row, target_track);
            if (!target) {
                idx++;
                continue;
            }

            /* Record undo for this cell */
            /* Clone: clearing target frees what a struct copy would share */
            TrackerCell old_cell;
            tracker_cell_clone(&old_cell, target);
            tracker_cell_clear(target);
            tracker_cell_clone(target, &view->clipboard.cells[idx]);
            target->dirty = true;

            tracker_undo_record_cell_edit(&view->undo, view,
                view->state.cursor_pattern, target_track, target_row,
                &old_cell, target);

            tracker_cell_clear(&old_cell);

            idx++;
        }
    }

    tracker_view_end_undo_group(view);

    tracker_view_invalidate(view);
    return true;
}

bool tracker_view_paste_insert(TrackerView* view) {
    if (!view || !view->song) return false;
    if (!tracker_view_clipboard_has_content(view)) return false;

    /* TODO: implement paste with row insertion */
    /* For now, just do regular paste */
    return tracker_view_paste(view);
}

/*============================================================================
 * Cell Operations
 *============================================================================*/

void tracker_view_clear_cell(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    TrackerCell* cell = tracker_pattern_get_cell(pattern,
        view->state.cursor_row, view->state.cursor_track);
    if (!cell) return;

    /* Record undo */
    TrackerCell old_cell;
    tracker_cell_clone(&old_cell, cell);

    /* Clear the cell */
    tracker_cell_clear(cell);
    cell->dirty = true;

    /* Create new empty cell state for undo */
    TrackerCell new_cell;
    tracker_cell_init(&new_cell);

    tracker_undo_record_cell_edit(&view->undo, view,
        view->state.cursor_pattern, view->state.cursor_track, view->state.cursor_row,
        &old_cell, &new_cell);

    /* Clean up temporary cell */
    tracker_cell_clear(&old_cell);

    view->modified = true;
    tracker_view_invalidate_cell(view, view->state.cursor_track, view->state.cursor_row);
    tracker_view_show_status(view, "Cleared cell");
}

void tracker_view_clear_selection(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    int start_track, end_track, start_row, end_row;

    if (view->state.selection.type != TRACKER_SEL_NONE) {
        start_track = view->state.selection.start_track;
        end_track = view->state.selection.end_track;
        start_row = view->state.selection.start_row;
        end_row = view->state.selection.end_row;
    } else {
        /* Just clear cursor cell */
        tracker_view_clear_cell(view);
        return;
    }

    tracker_view_begin_undo_group(view, "Clear");

    for (int r = start_row; r <= end_row; r++) {
        for (int t = start_track; t <= end_track; t++) {
            TrackerCell* cell = tracker_pattern_get_cell(pattern, r, t);
            if (!cell) continue;
            if (cell->type == TRACKER_CELL_EMPTY) continue;

            TrackerCell old_cell;
            tracker_cell_clone(&old_cell, cell);

            tracker_cell_clear(cell);

            TrackerCell new_cell;
            tracker_cell_init(&new_cell);

            tracker_undo_record_cell_edit(&view->undo, view,
                view->state.cursor_pattern, t, r, &old_cell, &new_cell);

            tracker_cell_clear(&old_cell);
        }
    }

    tracker_view_end_undo_group(view);
    tracker_view_select_clear(view);
    tracker_view_invalidate(view);
}

void tracker_view_insert_row(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    int row = view->state.cursor_row;

    /* Record undo */
    tracker_undo_record_row_insert(&view->undo, view,
        view->state.cursor_pattern, row);

    /* Shift rows down */
    for (int t = 0; t < pattern->num_tracks; t++) {
        TrackerTrack* track = &pattern->tracks[t];

        /* Shift cells from end to insert point */
        for (int r = pattern->num_rows - 1; r > row; r--) {
            TrackerCell* src = &track->cells[r - 1];
            TrackerCell* dst = &track->cells[r];

            tracker_cell_clear(dst);
            *dst = *src;
            memset(src, 0, sizeof(TrackerCell));
            tracker_cell_init(src);
        }

        /* Clear the inserted row */
        tracker_cell_clear(&track->cells[row]);
    }

    view->modified = true;
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Inserted row at %d", row + 1);
}

void tracker_view_delete_row(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    int row = view->state.cursor_row;

    /* Collect cells for undo */
    TrackerCell* deleted_cells = calloc(pattern->num_tracks, sizeof(TrackerCell));
    if (deleted_cells) {
        for (int t = 0; t < pattern->num_tracks; t++) {
            TrackerCell* cell = tracker_pattern_get_cell(pattern, row, t);
            if (cell) {
                tracker_cell_clone(&deleted_cells[t], cell);
            }
        }

        tracker_undo_record_row_delete(&view->undo, view,
            view->state.cursor_pattern, row, deleted_cells, pattern->num_tracks);

        for (int t = 0; t < pattern->num_tracks; t++) {
            tracker_cell_clear(&deleted_cells[t]);
        }
        free(deleted_cells);
    }

    /* Shift rows up */
    for (int t = 0; t < pattern->num_tracks; t++) {
        TrackerTrack* track = &pattern->tracks[t];

        /* Clear the row being deleted */
        tracker_cell_clear(&track->cells[row]);

        /* Shift cells up */
        for (int r = row; r < pattern->num_rows - 1; r++) {
            TrackerCell* src = &track->cells[r + 1];
            TrackerCell* dst = &track->cells[r];

            *dst = *src;
            memset(src, 0, sizeof(TrackerCell));
            tracker_cell_init(src);
        }
    }

    view->modified = true;
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Deleted row %d", row + 1);
}

void tracker_view_duplicate_row(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    int row = view->state.cursor_row;

    /* Insert a new row first */
    tracker_view_insert_row(view);

    /* Copy the current row (now at row+1) to the new row */
    for (int t = 0; t < pattern->num_tracks; t++) {
        TrackerCell* src = tracker_pattern_get_cell(pattern, row + 1, t);
        TrackerCell* dst = tracker_pattern_get_cell(pattern, row, t);
        if (src && dst) {
            tracker_cell_clone(dst, src);
            dst->dirty = true;
        }
    }

    view->modified = true;
    tracker_view_invalidate(view);
    tracker_view_show_status(view, "Duplicated row %d", row + 1);
}
