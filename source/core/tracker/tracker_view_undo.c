/**
 * tracker_view_undo.c - Undo/redo system implementation
 */

#include "tracker_view.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

static void cell_state_init(TrackerUndoCellState* state) {
    if (!state) return;
    memset(state, 0, sizeof(TrackerUndoCellState));
    state->pattern = -1;
    state->track = -1;
    state->row = -1;
}

static void cell_state_clear(TrackerUndoCellState* state) {
    if (!state) return;
    free(state->expression);
    free(state->language_id);
    tracker_fx_chain_clear(&state->fx_chain);
    cell_state_init(state);
}

static bool cell_state_copy_from_cell(TrackerUndoCellState* state,
                                       int pattern, int track, int row,
                                       const TrackerCell* cell) {
    if (!state || !cell) return false;

    cell_state_clear(state);
    state->pattern = pattern;
    state->track = track;
    state->row = row;
    state->type = cell->type;
    state->expression = str_dup(cell->expression);
    state->language_id = str_dup(cell->language_id);

    if (!tracker_fx_chain_clone(&state->fx_chain, &cell->fx_chain)) {
        cell_state_clear(state);
        return false;
    }

    return true;
}

static void apply_cell_state(TrackerSong* song, const TrackerUndoCellState* state) {
    if (!song || !state || state->pattern < 0) return;

    TrackerPattern* pattern = tracker_song_get_pattern(song, state->pattern);
    if (!pattern) return;

    TrackerCell* cell = tracker_pattern_get_cell(pattern, state->row, state->track);
    if (!cell) return;

    tracker_cell_clear(cell);
    cell->type = state->type;
    cell->expression = str_dup(state->expression);
    cell->language_id = str_dup(state->language_id);
    tracker_fx_chain_clone(&cell->fx_chain, &state->fx_chain);
    cell->dirty = true;
}

/*============================================================================
 * Undo Action Management
 *============================================================================*/

static TrackerUndoAction* action_new(TrackerUndoType type) {
    TrackerUndoAction* action = calloc(1, sizeof(TrackerUndoAction));
    if (action) {
        action->type = type;
        action->cursor_pattern = -1;
        action->cursor_track = -1;
        action->cursor_row = -1;
    }
    return action;
}

void tracker_undo_action_free(TrackerUndoAction* action) {
    if (!action) return;

    switch (action->type) {
        case TRACKER_UNDO_CELL_EDIT:
        case TRACKER_UNDO_CELL_CLEAR:
            cell_state_clear(&action->data.cell.before);
            cell_state_clear(&action->data.cell.after);
            break;

        case TRACKER_UNDO_CELLS_CHANGE:
        case TRACKER_UNDO_PASTE:
        case TRACKER_UNDO_CUT:
            if (action->data.cells.before) {
                for (int i = 0; i < action->data.cells.count; i++) {
                    cell_state_clear(&action->data.cells.before[i]);
                }
                free(action->data.cells.before);
            }
            if (action->data.cells.after) {
                for (int i = 0; i < action->data.cells.count; i++) {
                    cell_state_clear(&action->data.cells.after[i]);
                }
                free(action->data.cells.after);
            }
            break;

        case TRACKER_UNDO_ROW_INSERT:
        case TRACKER_UNDO_ROW_DELETE:
        case TRACKER_UNDO_ROW_DUPLICATE:
            if (action->data.row.cells) {
                for (int i = 0; i < action->data.row.cell_count; i++) {
                    cell_state_clear(&action->data.row.cells[i]);
                }
                free(action->data.row.cells);
            }
            break;

        case TRACKER_UNDO_TRACK_ADD:
        case TRACKER_UNDO_TRACK_DELETE:
            free(action->data.track.name);
            if (action->data.track.cells) {
                for (int i = 0; i < action->data.track.cell_count; i++) {
                    cell_state_clear(&action->data.track.cells[i]);
                }
                free(action->data.track.cells);
            }
            tracker_fx_chain_clear(&action->data.track.fx_chain);
            break;

        case TRACKER_UNDO_PATTERN_ADD:
        case TRACKER_UNDO_PATTERN_DELETE:
            if (action->data.pattern.pattern) {
                tracker_pattern_free(action->data.pattern.pattern);
            }
            break;

        case TRACKER_UNDO_PATTERN_RESIZE:
            if (action->data.resize.truncated) {
                for (int i = 0; i < action->data.resize.truncated_count; i++) {
                    cell_state_clear(&action->data.resize.truncated[i]);
                }
                free(action->data.resize.truncated);
            }
            break;

        case TRACKER_UNDO_FX_CHAIN_CHANGE:
            tracker_fx_chain_clear(&action->data.fx.before);
            tracker_fx_chain_clear(&action->data.fx.after);
            break;

        case TRACKER_UNDO_GROUP_BEGIN:
            free(action->data.group.description);
            break;

        default:
            break;
    }

    free(action);
}

static void free_action_chain(TrackerUndoAction* head) {
    while (head) {
        TrackerUndoAction* next = head->next;
        tracker_undo_action_free(head);
        head = next;
    }
}

/*============================================================================
 * Undo Stack Functions
 *============================================================================*/

void tracker_undo_init(TrackerUndoStack* stack, int max_undo) {
    if (!stack) return;
    memset(stack, 0, sizeof(TrackerUndoStack));
    stack->max_undo = max_undo;
}

void tracker_undo_cleanup(TrackerUndoStack* stack) {
    if (!stack) return;
    tracker_undo_clear(stack);
}

void tracker_undo_clear(TrackerUndoStack* stack) {
    if (!stack) return;

    free_action_chain(stack->undo_head);
    free_action_chain(stack->redo_head);

    stack->undo_head = NULL;
    stack->redo_head = NULL;
    stack->undo_count = 0;
    stack->redo_count = 0;
    stack->group_depth = 0;
}

void tracker_undo_record(TrackerUndoStack* stack, TrackerUndoAction* action) {
    if (!stack || !action) return;
    if (stack->in_undo) {
        /* Don't record during undo/redo; the stack owns action either way */
        tracker_undo_action_free(action);
        return;
    }

    /* Clear redo stack when new action is recorded */
    free_action_chain(stack->redo_head);
    stack->redo_head = NULL;
    stack->redo_count = 0;

    /* Add to undo stack */
    action->next = stack->undo_head;
    stack->undo_head = action;
    stack->undo_count++;

    /* Enforce max_undo limit */
    if (stack->max_undo > 0 && stack->undo_count > stack->max_undo) {
        /* Find and remove oldest action */
        TrackerUndoAction* prev = NULL;
        TrackerUndoAction* curr = stack->undo_head;

        while (curr && curr->next) {
            prev = curr;
            curr = curr->next;
        }

        if (prev) {
            prev->next = NULL;
            tracker_undo_action_free(curr);
            stack->undo_count--;
        }
    }
}

void tracker_undo_group_begin(TrackerUndoStack* stack, const char* description) {
    if (!stack) return;

    TrackerUndoAction* action = action_new(TRACKER_UNDO_GROUP_BEGIN);
    if (!action) return;

    action->data.group.description = str_dup(description);
    tracker_undo_record(stack, action);
    stack->group_depth++;
}

void tracker_undo_group_end(TrackerUndoStack* stack) {
    if (!stack || stack->group_depth == 0) return;

    TrackerUndoAction* action = action_new(TRACKER_UNDO_GROUP_END);
    if (!action) return;

    tracker_undo_record(stack, action);
    stack->group_depth--;
}

bool tracker_undo_can_undo(TrackerUndoStack* stack) {
    if (!stack) return false;

    /* Skip group markers to find actual action */
    TrackerUndoAction* action = stack->undo_head;
    while (action && (action->type == TRACKER_UNDO_GROUP_END ||
                      action->type == TRACKER_UNDO_GROUP_BEGIN)) {
        action = action->next;
    }

    return action != NULL;
}

bool tracker_undo_can_redo(TrackerUndoStack* stack) {
    if (!stack) return false;

    TrackerUndoAction* action = stack->redo_head;
    while (action && (action->type == TRACKER_UNDO_GROUP_END ||
                      action->type == TRACKER_UNDO_GROUP_BEGIN)) {
        action = action->next;
    }

    return action != NULL;
}

const char* tracker_undo_get_undo_description(TrackerUndoStack* stack) {
    if (!stack || !stack->undo_head) return NULL;

    TrackerUndoAction* action = stack->undo_head;

    /* If top is GROUP_END, find corresponding GROUP_BEGIN */
    if (action->type == TRACKER_UNDO_GROUP_END) {
        int depth = 1;
        action = action->next;
        while (action && depth > 0) {
            if (action->type == TRACKER_UNDO_GROUP_END) depth++;
            else if (action->type == TRACKER_UNDO_GROUP_BEGIN) depth--;
            if (depth > 0) action = action->next;
        }
        if (action && action->type == TRACKER_UNDO_GROUP_BEGIN) {
            return action->data.group.description;
        }
    }

    /* Return type-based description */
    switch (action->type) {
        case TRACKER_UNDO_CELL_EDIT: return "Edit cell";
        case TRACKER_UNDO_CELL_CLEAR: return "Clear cell";
        case TRACKER_UNDO_CELLS_CHANGE: return "Edit cells";
        case TRACKER_UNDO_ROW_INSERT: return "Insert row";
        case TRACKER_UNDO_ROW_DELETE: return "Delete row";
        case TRACKER_UNDO_ROW_DUPLICATE: return "Duplicate row";
        case TRACKER_UNDO_ROWS_MOVE: return "Move rows";
        case TRACKER_UNDO_TRACK_ADD: return "Add track";
        case TRACKER_UNDO_TRACK_DELETE: return "Delete track";
        case TRACKER_UNDO_TRACK_MOVE: return "Move track";
        case TRACKER_UNDO_PATTERN_ADD: return "Add pattern";
        case TRACKER_UNDO_PATTERN_DELETE: return "Delete pattern";
        case TRACKER_UNDO_PATTERN_RESIZE: return "Resize pattern";
        case TRACKER_UNDO_FX_CHAIN_CHANGE: return "Edit FX";
        case TRACKER_UNDO_SONG_SETTINGS: return "Change settings";
        case TRACKER_UNDO_PASTE: return "Paste";
        case TRACKER_UNDO_CUT: return "Cut";
        case TRACKER_UNDO_GROUP_BEGIN: return action->data.group.description;
        default: return "Unknown";
    }
}

const char* tracker_undo_get_redo_description(TrackerUndoStack* stack) {
    if (!stack || !stack->redo_head) return NULL;

    TrackerUndoAction* action = stack->redo_head;

    if (action->type == TRACKER_UNDO_GROUP_BEGIN) {
        return action->data.group.description;
    }

    /* Use same logic as undo description */
    switch (action->type) {
        case TRACKER_UNDO_CELL_EDIT: return "Edit cell";
        case TRACKER_UNDO_CELL_CLEAR: return "Clear cell";
        case TRACKER_UNDO_CELLS_CHANGE: return "Edit cells";
        case TRACKER_UNDO_ROW_INSERT: return "Insert row";
        case TRACKER_UNDO_ROW_DELETE: return "Delete row";
        case TRACKER_UNDO_ROW_DUPLICATE: return "Duplicate row";
        case TRACKER_UNDO_ROWS_MOVE: return "Move rows";
        case TRACKER_UNDO_TRACK_ADD: return "Add track";
        case TRACKER_UNDO_TRACK_DELETE: return "Delete track";
        case TRACKER_UNDO_TRACK_MOVE: return "Move track";
        case TRACKER_UNDO_PATTERN_ADD: return "Add pattern";
        case TRACKER_UNDO_PATTERN_DELETE: return "Delete pattern";
        case TRACKER_UNDO_PATTERN_RESIZE: return "Resize pattern";
        case TRACKER_UNDO_FX_CHAIN_CHANGE: return "Edit FX";
        case TRACKER_UNDO_SONG_SETTINGS: return "Change settings";
        case TRACKER_UNDO_PASTE: return "Paste";
        case TRACKER_UNDO_CUT: return "Cut";
        default: return "Unknown";
    }
}

/*============================================================================
 * Undo/Redo Execution
 *============================================================================*/

static bool apply_action_undo(TrackerUndoAction* action, TrackerView* view,
                              TrackerSong* song) {
    if (!action || !song) return false;

    switch (action->type) {
        case TRACKER_UNDO_CELL_EDIT:
        case TRACKER_UNDO_CELL_CLEAR:
            apply_cell_state(song, &action->data.cell.before);
            break;

        case TRACKER_UNDO_CELLS_CHANGE:
        case TRACKER_UNDO_PASTE:
        case TRACKER_UNDO_CUT:
            for (int i = 0; i < action->data.cells.count; i++) {
                apply_cell_state(song, &action->data.cells.before[i]);
            }
            break;

        case TRACKER_UNDO_ROW_INSERT: {
            TrackerPattern* pattern = tracker_song_get_pattern(song,
                action->data.row.pattern);
            if (pattern) {
                /* Delete the inserted row */
                /* Note: simplified - should shift cells up */
                for (int t = 0; t < pattern->num_tracks; t++) {
                    TrackerCell* cell = tracker_pattern_get_cell(pattern,
                        action->data.row.row, t);
                    if (cell) tracker_cell_clear(cell);
                }
            }
            break;
        }

        case TRACKER_UNDO_ROW_DELETE: {
            /* Restore deleted cells */
            for (int i = 0; i < action->data.row.cell_count; i++) {
                apply_cell_state(song, &action->data.row.cells[i]);
            }
            break;
        }

        case TRACKER_UNDO_SONG_SETTINGS: {
            song->bpm = action->data.settings.old_bpm;
            song->rows_per_beat = action->data.settings.old_rpb;
            song->ticks_per_row = action->data.settings.old_tpr;
            song->spillover_mode = action->data.settings.old_spillover;
            break;
        }

        default:
            /* TODO: implement other action types */
            break;
    }

    /* Restore cursor position */
    if (view && action->cursor_pattern >= 0) {
        view->state.cursor_pattern = action->cursor_pattern;
        view->state.cursor_track = action->cursor_track;
        view->state.cursor_row = action->cursor_row;
    }

    return true;
}

static bool apply_action_redo(TrackerUndoAction* action, TrackerView* view,
                              TrackerSong* song) {
    (void)view;  /* reserved for cursor restoration */
    if (!action || !song) return false;

    switch (action->type) {
        case TRACKER_UNDO_CELL_EDIT:
        case TRACKER_UNDO_CELL_CLEAR:
            apply_cell_state(song, &action->data.cell.after);
            break;

        case TRACKER_UNDO_CELLS_CHANGE:
        case TRACKER_UNDO_PASTE:
        case TRACKER_UNDO_CUT:
            for (int i = 0; i < action->data.cells.count; i++) {
                apply_cell_state(song, &action->data.cells.after[i]);
            }
            break;

        case TRACKER_UNDO_SONG_SETTINGS: {
            song->bpm = action->data.settings.new_bpm;
            song->rows_per_beat = action->data.settings.new_rpb;
            song->ticks_per_row = action->data.settings.new_tpr;
            song->spillover_mode = action->data.settings.new_spillover;
            break;
        }

        default:
            /* TODO: implement other action types */
            break;
    }

    return true;
}

bool tracker_undo_undo(TrackerUndoStack* stack, TrackerView* view,
                       TrackerSong* song) {
    if (!stack || !song || !stack->undo_head) return false;

    stack->in_undo = true;

    /* Handle grouped actions */
    bool in_group = false;
    int group_depth = 0;

    do {
        TrackerUndoAction* action = stack->undo_head;
        stack->undo_head = action->next;
        stack->undo_count--;

        if (action->type == TRACKER_UNDO_GROUP_END) {
            in_group = true;
            group_depth++;
        } else if (action->type == TRACKER_UNDO_GROUP_BEGIN) {
            group_depth--;
            if (group_depth == 0) in_group = false;
        } else {
            apply_action_undo(action, view, song);
        }

        /* Move to redo stack */
        action->next = stack->redo_head;
        stack->redo_head = action;
        stack->redo_count++;

    } while (in_group && stack->undo_head);

    stack->in_undo = false;
    return true;
}

bool tracker_undo_redo(TrackerUndoStack* stack, TrackerView* view,
                       TrackerSong* song) {
    if (!stack || !song || !stack->redo_head) return false;

    stack->in_undo = true;

    /* Handle grouped actions */
    bool in_group = false;
    int group_depth = 0;

    do {
        TrackerUndoAction* action = stack->redo_head;
        stack->redo_head = action->next;
        stack->redo_count--;

        if (action->type == TRACKER_UNDO_GROUP_BEGIN) {
            in_group = true;
            group_depth++;
        } else if (action->type == TRACKER_UNDO_GROUP_END) {
            group_depth--;
            if (group_depth == 0) in_group = false;
        } else {
            apply_action_redo(action, view, song);
        }

        /* Move to undo stack */
        action->next = stack->undo_head;
        stack->undo_head = action;
        stack->undo_count++;

    } while (in_group && stack->redo_head);

    stack->in_undo = false;
    return true;
}

/*============================================================================
 * Convenience Recording Functions
 *============================================================================*/

void tracker_undo_record_cell_edit(TrackerUndoStack* stack,
                                   TrackerView* view,
                                   int pattern, int track, int row,
                                   const TrackerCell* old_cell,
                                   const TrackerCell* new_cell) {
    if (!stack || !old_cell || !new_cell) return;

    TrackerUndoAction* action = action_new(TRACKER_UNDO_CELL_EDIT);
    if (!action) return;

    if (view) {
        action->cursor_pattern = view->state.cursor_pattern;
        action->cursor_track = view->state.cursor_track;
        action->cursor_row = view->state.cursor_row;
    }

    cell_state_copy_from_cell(&action->data.cell.before, pattern, track, row, old_cell);
    cell_state_copy_from_cell(&action->data.cell.after, pattern, track, row, new_cell);

    tracker_undo_record(stack, action);
}

void tracker_undo_record_cells_change(TrackerUndoStack* stack,
                                       TrackerView* view,
                                       int pattern,
                                       int start_track, int end_track,
                                       int start_row, int end_row,
                                       TrackerCell** old_cells,
                                       TrackerCell** new_cells) {
    if (!stack) return;

    int width = end_track - start_track + 1;
    int height = end_row - start_row + 1;
    int count = width * height;

    TrackerUndoAction* action = action_new(TRACKER_UNDO_CELLS_CHANGE);
    if (!action) return;

    if (view) {
        action->cursor_pattern = view->state.cursor_pattern;
        action->cursor_track = view->state.cursor_track;
        action->cursor_row = view->state.cursor_row;
    }

    action->data.cells.start_track = start_track;
    action->data.cells.end_track = end_track;
    action->data.cells.start_row = start_row;
    action->data.cells.end_row = end_row;
    action->data.cells.count = count;

    action->data.cells.before = calloc(count, sizeof(TrackerUndoCellState));
    action->data.cells.after = calloc(count, sizeof(TrackerUndoCellState));

    if (!action->data.cells.before || !action->data.cells.after) {
        tracker_undo_action_free(action);
        return;
    }

    int idx = 0;
    for (int r = start_row; r <= end_row; r++) {
        for (int t = start_track; t <= end_track; t++) {
            if (old_cells && old_cells[idx]) {
                cell_state_copy_from_cell(&action->data.cells.before[idx],
                                          pattern, t, r, old_cells[idx]);
            }
            if (new_cells && new_cells[idx]) {
                cell_state_copy_from_cell(&action->data.cells.after[idx],
                                          pattern, t, r, new_cells[idx]);
            }
            idx++;
        }
    }

    tracker_undo_record(stack, action);
}

void tracker_undo_record_row_insert(TrackerUndoStack* stack,
                                    TrackerView* view,
                                    int pattern, int row) {
    if (!stack) return;

    TrackerUndoAction* action = action_new(TRACKER_UNDO_ROW_INSERT);
    if (!action) return;

    if (view) {
        action->cursor_pattern = view->state.cursor_pattern;
        action->cursor_track = view->state.cursor_track;
        action->cursor_row = view->state.cursor_row;
    }

    action->data.row.pattern = pattern;
    action->data.row.row = row;

    tracker_undo_record(stack, action);
}

void tracker_undo_record_row_delete(TrackerUndoStack* stack,
                                    TrackerView* view,
                                    int pattern, int row,
                                    const TrackerCell* deleted_cells,
                                    int cell_count) {
    if (!stack) return;

    TrackerUndoAction* action = action_new(TRACKER_UNDO_ROW_DELETE);
    if (!action) return;

    if (view) {
        action->cursor_pattern = view->state.cursor_pattern;
        action->cursor_track = view->state.cursor_track;
        action->cursor_row = view->state.cursor_row;
    }

    action->data.row.pattern = pattern;
    action->data.row.row = row;
    action->data.row.cell_count = cell_count;

    if (deleted_cells && cell_count > 0) {
        action->data.row.cells = calloc(cell_count, sizeof(TrackerUndoCellState));
        if (action->data.row.cells) {
            for (int i = 0; i < cell_count; i++) {
                cell_state_copy_from_cell(&action->data.row.cells[i],
                                          pattern, i, row, &deleted_cells[i]);
            }
        }
    }

    tracker_undo_record(stack, action);
}
