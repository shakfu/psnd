/**
 * test_view_clipboard.c - Tests for tracker clipboard and selection operations
 */

#include "test_framework.h"
#include "tracker_view.h"
#include "tracker_model.h"
#include <stdlib.h>
#include <string.h>

/* Global test stats required by test framework */
test_stats_t test_stats;

/*============================================================================
 * Test Helpers
 *============================================================================*/

/* Create a minimal view for testing */
static TrackerView* create_test_view(void) {
    TrackerView* view = calloc(1, sizeof(TrackerView));
    if (view) {
        tracker_view_state_init(&view->state);
        tracker_undo_init(&view->undo, 0);  /* unlimited */
    }
    return view;
}

/* Create a view with song and pattern attached */
static TrackerView* create_test_view_with_song(int num_rows, int num_tracks) {
    TrackerView* view = create_test_view();
    if (!view) return NULL;

    TrackerSong* song = tracker_song_new("Test Song");
    if (!song) {
        tracker_view_state_cleanup(&view->state);
        tracker_undo_cleanup(&view->undo);
        free(view);
        return NULL;
    }

    TrackerPattern* pattern = tracker_pattern_new(num_rows, num_tracks, "Pattern 1");
    if (!pattern) {
        tracker_song_free(song);
        tracker_view_state_cleanup(&view->state);
        tracker_undo_cleanup(&view->undo);
        free(view);
        return NULL;
    }

    tracker_song_add_pattern(song, pattern);
    view->song = song;
    view->state.cursor_pattern = 0;
    view->state.cursor_track = 0;
    view->state.cursor_row = 0;

    return view;
}

static void free_test_view(TrackerView* view) {
    if (view) {
        if (view->song) {
            tracker_song_free(view->song);
        }
        tracker_view_clipboard_clear(view);
        tracker_view_state_cleanup(&view->state);
        tracker_undo_cleanup(&view->undo);
        free(view);
    }
}

/* Helper to set a cell's expression for testing */
static void set_test_expression(TrackerPattern* pattern, int row, int track, const char* expr) {
    TrackerCell* cell = tracker_pattern_get_cell(pattern, row, track);
    if (cell) {
        cell->type = TRACKER_CELL_EXPRESSION;
        cell->expression = strdup(expr);
    }
}

/*============================================================================
 * Selection: select_start Tests
 *============================================================================*/

TEST(select_start_null_safe) {
    /* Should not crash */
    tracker_view_select_start(NULL);
}

TEST(select_start_sets_selection_type) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.cursor_track = 1;
    view->state.cursor_row = 5;

    tracker_view_select_start(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_CELL);
    ASSERT_TRUE(view->state.selecting);

    free_test_view(view);
}

TEST(select_start_sets_anchor) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.cursor_track = 2;
    view->state.cursor_row = 7;

    tracker_view_select_start(view);

    ASSERT_EQ(view->state.selection.anchor_track, 2);
    ASSERT_EQ(view->state.selection.anchor_row, 7);
    ASSERT_EQ(view->state.selection.start_track, 2);
    ASSERT_EQ(view->state.selection.end_track, 2);
    ASSERT_EQ(view->state.selection.start_row, 7);
    ASSERT_EQ(view->state.selection.end_row, 7);

    free_test_view(view);
}

TEST(select_start_sets_pattern) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.cursor_pattern = 0;

    tracker_view_select_start(view);

    ASSERT_EQ(view->state.selection.start_pattern, 0);
    ASSERT_EQ(view->state.selection.end_pattern, 0);

    free_test_view(view);
}

/*============================================================================
 * Selection: select_extend Tests
 *============================================================================*/

TEST(select_extend_null_safe) {
    tracker_view_select_extend(NULL);
}

TEST(select_extend_without_selection_noop) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    /* Not selecting - should do nothing */
    view->state.selecting = false;
    tracker_view_select_extend(view);

    ASSERT_FALSE(view->state.selecting);

    free_test_view(view);
}

TEST(select_extend_forwards) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    /* Start selection at (1, 2) */
    view->state.cursor_track = 1;
    view->state.cursor_row = 2;
    tracker_view_select_start(view);

    /* Move cursor forward and extend */
    view->state.cursor_track = 3;
    view->state.cursor_row = 8;
    tracker_view_select_extend(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_RANGE);
    ASSERT_EQ(view->state.selection.start_track, 1);
    ASSERT_EQ(view->state.selection.end_track, 3);
    ASSERT_EQ(view->state.selection.start_row, 2);
    ASSERT_EQ(view->state.selection.end_row, 8);

    free_test_view(view);
}

TEST(select_extend_backwards) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    /* Start selection at (2, 10) */
    view->state.cursor_track = 2;
    view->state.cursor_row = 10;
    tracker_view_select_start(view);

    /* Move cursor backward and extend */
    view->state.cursor_track = 0;
    view->state.cursor_row = 3;
    tracker_view_select_extend(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_RANGE);
    ASSERT_EQ(view->state.selection.start_track, 0);
    ASSERT_EQ(view->state.selection.end_track, 2);
    ASSERT_EQ(view->state.selection.start_row, 3);
    ASSERT_EQ(view->state.selection.end_row, 10);

    free_test_view(view);
}

TEST(select_extend_preserves_anchor) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.cursor_track = 1;
    view->state.cursor_row = 5;
    tracker_view_select_start(view);

    view->state.cursor_track = 2;
    view->state.cursor_row = 8;
    tracker_view_select_extend(view);

    ASSERT_EQ(view->state.selection.anchor_track, 1);
    ASSERT_EQ(view->state.selection.anchor_row, 5);

    free_test_view(view);
}

/*============================================================================
 * Selection: select_clear Tests
 *============================================================================*/

TEST(select_clear_null_safe) {
    tracker_view_select_clear(NULL);
}

TEST(select_clear_resets_selection) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    tracker_view_select_start(view);
    ASSERT_TRUE(view->state.selecting);

    tracker_view_select_clear(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_NONE);
    ASSERT_FALSE(view->state.selecting);

    free_test_view(view);
}

/*============================================================================
 * Selection: select_track Tests
 *============================================================================*/

TEST(select_track_null_safe) {
    tracker_view_select_track(NULL);
}

TEST(select_track_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_select_track(view);
    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_NONE);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(select_track_selects_entire_track) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.cursor_track = 2;
    view->state.cursor_row = 5;

    tracker_view_select_track(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_TRACK);
    ASSERT_EQ(view->state.selection.start_track, 2);
    ASSERT_EQ(view->state.selection.end_track, 2);
    ASSERT_EQ(view->state.selection.start_row, 0);
    ASSERT_EQ(view->state.selection.end_row, 15);  /* 16 rows, 0-15 */
    ASSERT_FALSE(view->state.selecting);

    free_test_view(view);
}

/*============================================================================
 * Selection: select_row Tests
 *============================================================================*/

TEST(select_row_null_safe) {
    tracker_view_select_row(NULL);
}

TEST(select_row_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_select_row(view);
    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_NONE);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(select_row_selects_entire_row) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.cursor_track = 1;
    view->state.cursor_row = 7;

    tracker_view_select_row(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_ROW);
    ASSERT_EQ(view->state.selection.start_track, 0);
    ASSERT_EQ(view->state.selection.end_track, 3);  /* 4 tracks, 0-3 */
    ASSERT_EQ(view->state.selection.start_row, 7);
    ASSERT_EQ(view->state.selection.end_row, 7);
    ASSERT_FALSE(view->state.selecting);

    free_test_view(view);
}

/*============================================================================
 * Selection: select_pattern Tests
 *============================================================================*/

TEST(select_pattern_null_safe) {
    tracker_view_select_pattern(NULL);
}

TEST(select_pattern_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_select_pattern(view);
    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_NONE);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(select_pattern_selects_all) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    tracker_view_select_pattern(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_PATTERN);
    ASSERT_EQ(view->state.selection.start_track, 0);
    ASSERT_EQ(view->state.selection.end_track, 3);
    ASSERT_EQ(view->state.selection.start_row, 0);
    ASSERT_EQ(view->state.selection.end_row, 15);
    ASSERT_FALSE(view->state.selecting);

    free_test_view(view);
}

/*============================================================================
 * Selection: select_all Tests
 *============================================================================*/

TEST(select_all_null_safe) {
    tracker_view_select_all(NULL);
}

TEST(select_all_selects_pattern) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    tracker_view_select_all(view);

    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_PATTERN);
    ASSERT_EQ(view->state.selection.start_track, 0);
    ASSERT_EQ(view->state.selection.end_track, 3);
    ASSERT_EQ(view->state.selection.start_row, 0);
    ASSERT_EQ(view->state.selection.end_row, 15);

    free_test_view(view);
}

/*============================================================================
 * Selection: is_selected Tests
 *============================================================================*/

TEST(is_selected_null_returns_false) {
    ASSERT_FALSE(tracker_view_is_selected(NULL, 0, 0));
}

TEST(is_selected_no_selection_returns_false) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_is_selected(view, 0, 0));

    free_test_view(view);
}

TEST(is_selected_in_selection_returns_true) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    /* Select track 1, rows 2-5 */
    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 1;
    view->state.selection.end_track = 2;
    view->state.selection.start_row = 2;
    view->state.selection.end_row = 5;

    /* In selection */
    ASSERT_TRUE(tracker_view_is_selected(view, 1, 2));
    ASSERT_TRUE(tracker_view_is_selected(view, 1, 3));
    ASSERT_TRUE(tracker_view_is_selected(view, 2, 5));

    /* Outside selection */
    ASSERT_FALSE(tracker_view_is_selected(view, 0, 0));
    ASSERT_FALSE(tracker_view_is_selected(view, 3, 3));
    ASSERT_FALSE(tracker_view_is_selected(view, 1, 6));

    free_test_view(view);
}

TEST(is_selected_boundary_checks) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 1;
    view->state.selection.end_track = 2;
    view->state.selection.start_row = 4;
    view->state.selection.end_row = 8;

    /* On boundary */
    ASSERT_TRUE(tracker_view_is_selected(view, 1, 4));
    ASSERT_TRUE(tracker_view_is_selected(view, 2, 8));

    /* Just outside */
    ASSERT_FALSE(tracker_view_is_selected(view, 0, 4));
    ASSERT_FALSE(tracker_view_is_selected(view, 1, 3));
    ASSERT_FALSE(tracker_view_is_selected(view, 3, 4));
    ASSERT_FALSE(tracker_view_is_selected(view, 1, 9));

    free_test_view(view);
}

/*============================================================================
 * Selection: get_selection Tests
 *============================================================================*/

TEST(get_selection_null_returns_false) {
    ASSERT_FALSE(tracker_view_get_selection(NULL, NULL, NULL, NULL, NULL));
}

TEST(get_selection_no_selection_returns_false) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    int st, et, sr, er;
    ASSERT_FALSE(tracker_view_get_selection(view, &st, &et, &sr, &er));

    free_test_view(view);
}

TEST(get_selection_returns_bounds) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 1;
    view->state.selection.end_track = 3;
    view->state.selection.start_row = 2;
    view->state.selection.end_row = 10;

    int st, et, sr, er;
    ASSERT_TRUE(tracker_view_get_selection(view, &st, &et, &sr, &er));
    ASSERT_EQ(st, 1);
    ASSERT_EQ(et, 3);
    ASSERT_EQ(sr, 2);
    ASSERT_EQ(er, 10);

    free_test_view(view);
}

TEST(get_selection_partial_output) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 2;
    view->state.selection.start_row = 5;
    view->state.selection.end_row = 7;

    int st, sr;
    ASSERT_TRUE(tracker_view_get_selection(view, &st, NULL, &sr, NULL));
    ASSERT_EQ(st, 0);
    ASSERT_EQ(sr, 5);

    free_test_view(view);
}

/*============================================================================
 * Clipboard: clipboard_clear Tests
 *============================================================================*/

TEST(clipboard_clear_null_safe) {
    tracker_view_clipboard_clear(NULL);
}

TEST(clipboard_clear_resets_state) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    /* Manually set up a clipboard */
    view->clipboard.cells = calloc(4, sizeof(TrackerCell));
    view->clipboard.width = 2;
    view->clipboard.height = 2;
    view->clipboard.owns_cells = true;

    tracker_view_clipboard_clear(view);

    ASSERT_NULL(view->clipboard.cells);
    ASSERT_EQ(view->clipboard.width, 0);
    ASSERT_EQ(view->clipboard.height, 0);

    free_test_view(view);
}

/*============================================================================
 * Clipboard: clipboard_has_content Tests
 *============================================================================*/

TEST(clipboard_has_content_null_returns_false) {
    ASSERT_FALSE(tracker_view_clipboard_has_content(NULL));
}

TEST(clipboard_has_content_empty_returns_false) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_clipboard_has_content(view));

    free_test_view(view);
}

TEST(clipboard_has_content_with_data_returns_true) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->clipboard.cells = calloc(1, sizeof(TrackerCell));
    view->clipboard.width = 1;
    view->clipboard.height = 1;
    view->clipboard.owns_cells = true;

    ASSERT_TRUE(tracker_view_clipboard_has_content(view));

    free_test_view(view);
}

TEST(clipboard_has_content_zero_size_returns_false) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    view->clipboard.cells = calloc(1, sizeof(TrackerCell));
    view->clipboard.width = 0;
    view->clipboard.height = 1;
    view->clipboard.owns_cells = true;

    ASSERT_FALSE(tracker_view_clipboard_has_content(view));

    free(view->clipboard.cells);
    view->clipboard.cells = NULL;
    free_test_view(view);
}

/*============================================================================
 * Clipboard: copy Tests
 *============================================================================*/

TEST(copy_null_safe) {
    ASSERT_FALSE(tracker_view_copy(NULL));
}

TEST(copy_no_song_returns_false) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_copy(view));

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(copy_single_cell) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    view->state.cursor_track = 0;
    view->state.cursor_row = 0;

    ASSERT_TRUE(tracker_view_copy(view));
    ASSERT_TRUE(tracker_view_clipboard_has_content(view));
    ASSERT_EQ(view->clipboard.width, 1);
    ASSERT_EQ(view->clipboard.height, 1);
    ASSERT_EQ(view->clipboard.cells[0].type, TRACKER_CELL_EXPRESSION);
    ASSERT_STR_EQ(view->clipboard.cells[0].expression, "C4");

    free_test_view(view);
}

TEST(copy_selection) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");
    set_test_expression(pattern, 0, 1, "E4");
    set_test_expression(pattern, 1, 0, "G4");
    set_test_expression(pattern, 1, 1, "C5");

    /* Select 2x2 region */
    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 1;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = 1;

    ASSERT_TRUE(tracker_view_copy(view));
    ASSERT_EQ(view->clipboard.width, 2);
    ASSERT_EQ(view->clipboard.height, 2);

    /* Cells stored row by row, track by track */
    ASSERT_STR_EQ(view->clipboard.cells[0].expression, "C4");  /* row 0, track 0 */
    ASSERT_STR_EQ(view->clipboard.cells[1].expression, "E4");  /* row 0, track 1 */
    ASSERT_STR_EQ(view->clipboard.cells[2].expression, "G4");  /* row 1, track 0 */
    ASSERT_STR_EQ(view->clipboard.cells[3].expression, "C5");  /* row 1, track 1 */

    free_test_view(view);
}

TEST(copy_clears_previous_clipboard) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    /* First copy */
    tracker_view_copy(view);
    TrackerCell* old_cells = view->clipboard.cells;
    ASSERT_NOT_NULL(old_cells);

    /* Second copy - should free old cells */
    tracker_view_copy(view);
    /* Pointer should be different (new allocation) */
    /* Note: can't reliably test pointer difference, just verify it works */
    ASSERT_NOT_NULL(view->clipboard.cells);

    free_test_view(view);
}

TEST(copy_clamps_bounds) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    /* Set selection beyond bounds */
    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = -1;
    view->state.selection.end_track = 100;
    view->state.selection.start_row = -1;
    view->state.selection.end_row = 100;

    ASSERT_TRUE(tracker_view_copy(view));
    /* Should clamp to pattern size (4 tracks, 16 rows) */
    ASSERT_EQ(view->clipboard.width, 4);
    ASSERT_EQ(view->clipboard.height, 16);

    free_test_view(view);
}

/*============================================================================
 * Clipboard: cut Tests
 *============================================================================*/

TEST(cut_null_safe) {
    ASSERT_FALSE(tracker_view_cut(NULL));
}

TEST(cut_no_song_returns_false) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_cut(view));

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(cut_copies_and_clears) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    view->state.cursor_track = 0;
    view->state.cursor_row = 0;

    /* Select the cell */
    view->state.selection.type = TRACKER_SEL_CELL;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 0;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = 0;

    ASSERT_TRUE(tracker_view_cut(view));

    /* Clipboard should have the data */
    ASSERT_TRUE(tracker_view_clipboard_has_content(view));
    ASSERT_STR_EQ(view->clipboard.cells[0].expression, "C4");

    /* Cell in pattern should be cleared */
    TrackerCell* cell = tracker_pattern_get_cell(pattern, 0, 0);
    ASSERT_EQ(cell->type, TRACKER_CELL_EMPTY);

    free_test_view(view);
}

/*============================================================================
 * Clipboard: paste Tests
 *============================================================================*/

TEST(paste_null_safe) {
    ASSERT_FALSE(tracker_view_paste(NULL));
}

TEST(paste_no_song_returns_false) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_paste(view));

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(paste_no_clipboard_returns_false) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_paste(view));

    free_test_view(view);
}

TEST(paste_single_cell) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    /* Copy from (0, 0) */
    view->state.cursor_track = 0;
    view->state.cursor_row = 0;
    tracker_view_copy(view);

    /* Move cursor and paste */
    view->state.cursor_track = 1;
    view->state.cursor_row = 5;
    ASSERT_TRUE(tracker_view_paste(view));

    /* Check destination */
    TrackerCell* cell = tracker_pattern_get_cell(pattern, 5, 1);
    ASSERT_NOT_NULL(cell);
    ASSERT_EQ(cell->type, TRACKER_CELL_EXPRESSION);
    ASSERT_STR_EQ(cell->expression, "C4");

    free_test_view(view);
}

TEST(paste_over_filled_cell_undo_restores_it) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");
    set_test_expression(pattern, 5, 1, "E4");

    view->state.cursor_track = 0;
    view->state.cursor_row = 0;
    tracker_view_copy(view);

    view->state.cursor_track = 1;
    view->state.cursor_row = 5;
    ASSERT_TRUE(tracker_view_paste(view));

    TrackerCell* cell = tracker_pattern_get_cell(pattern, 5, 1);
    ASSERT_NOT_NULL(cell);
    ASSERT_STR_EQ(cell->expression, "C4");

    /* The undo snapshot must hold the overwritten value, not freed memory */
    ASSERT_TRUE(tracker_view_undo(view));
    ASSERT_STR_EQ(cell->expression, "E4");

    free_test_view(view);
}

TEST(paste_multiple_cells) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");
    set_test_expression(pattern, 0, 1, "E4");
    set_test_expression(pattern, 1, 0, "G4");
    set_test_expression(pattern, 1, 1, "C5");

    /* Select 2x2 region */
    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 1;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = 1;

    tracker_view_copy(view);

    /* Paste at (2, 8) */
    tracker_view_select_clear(view);
    view->state.cursor_track = 2;
    view->state.cursor_row = 8;
    ASSERT_TRUE(tracker_view_paste(view));

    /* Check pasted cells */
    TrackerCell* c1 = tracker_pattern_get_cell(pattern, 8, 2);
    TrackerCell* c2 = tracker_pattern_get_cell(pattern, 8, 3);
    TrackerCell* c3 = tracker_pattern_get_cell(pattern, 9, 2);
    TrackerCell* c4 = tracker_pattern_get_cell(pattern, 9, 3);

    ASSERT_STR_EQ(c1->expression, "C4");
    ASSERT_STR_EQ(c2->expression, "E4");
    ASSERT_STR_EQ(c3->expression, "G4");
    ASSERT_STR_EQ(c4->expression, "C5");

    free_test_view(view);
}

TEST(paste_clips_at_boundary) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");
    set_test_expression(pattern, 0, 1, "E4");
    set_test_expression(pattern, 1, 0, "G4");
    set_test_expression(pattern, 1, 1, "C5");

    /* Copy 2x2 region */
    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 1;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = 1;
    tracker_view_copy(view);

    /* Paste at edge - should clip */
    tracker_view_select_clear(view);
    view->state.cursor_track = 3;  /* Only 1 track left */
    view->state.cursor_row = 15;   /* Only 1 row left */
    ASSERT_TRUE(tracker_view_paste(view));

    /* Only (3, 15) should be pasted */
    TrackerCell* c = tracker_pattern_get_cell(pattern, 15, 3);
    ASSERT_STR_EQ(c->expression, "C4");

    /* Original should be unchanged */
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 0, 0)->expression, "C4");

    free_test_view(view);
}

/*============================================================================
 * Clipboard: paste_insert Tests
 *============================================================================*/

TEST(paste_insert_null_safe) {
    ASSERT_FALSE(tracker_view_paste_insert(NULL));
}

TEST(paste_insert_no_clipboard_returns_false) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(tracker_view_paste_insert(view));

    free_test_view(view);
}

TEST(paste_insert_falls_back_to_paste) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    view->state.cursor_track = 0;
    view->state.cursor_row = 0;
    tracker_view_copy(view);

    view->state.cursor_track = 1;
    view->state.cursor_row = 5;
    ASSERT_TRUE(tracker_view_paste_insert(view));

    /* Should paste like normal */
    TrackerCell* cell = tracker_pattern_get_cell(pattern, 5, 1);
    ASSERT_STR_EQ(cell->expression, "C4");

    free_test_view(view);
}

/*============================================================================
 * Cell Operations: clear_cell Tests
 *============================================================================*/

TEST(clear_cell_null_safe) {
    tracker_view_clear_cell(NULL);
}

TEST(clear_cell_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_clear_cell(view);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(clear_cell_clears_cursor_cell) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 3, 1, "C4");

    view->state.cursor_track = 1;
    view->state.cursor_row = 3;

    TrackerCell* cell = tracker_pattern_get_cell(pattern, 3, 1);
    ASSERT_EQ(cell->type, TRACKER_CELL_EXPRESSION);

    tracker_view_clear_cell(view);

    ASSERT_EQ(cell->type, TRACKER_CELL_EMPTY);
    ASSERT_TRUE(view->modified);

    free_test_view(view);
}

TEST(clear_cell_creates_undo) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    view->state.cursor_track = 0;
    view->state.cursor_row = 0;

    tracker_view_clear_cell(view);

    ASSERT_TRUE(tracker_undo_can_undo(&view->undo));

    free_test_view(view);
}

/*============================================================================
 * Cell Operations: clear_selection Tests
 *============================================================================*/

TEST(clear_selection_null_safe) {
    tracker_view_clear_selection(NULL);
}

TEST(clear_selection_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_clear_selection(view);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(clear_selection_clears_cursor_when_no_selection) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");

    view->state.cursor_track = 0;
    view->state.cursor_row = 0;
    /* No selection */
    view->state.selection.type = TRACKER_SEL_NONE;

    tracker_view_clear_selection(view);

    TrackerCell* cell = tracker_pattern_get_cell(pattern, 0, 0);
    ASSERT_EQ(cell->type, TRACKER_CELL_EMPTY);

    free_test_view(view);
}

TEST(clear_selection_clears_selected_cells) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");
    set_test_expression(pattern, 0, 1, "E4");
    set_test_expression(pattern, 1, 0, "G4");
    set_test_expression(pattern, 1, 1, "C5");

    /* Select 2x2 region */
    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 1;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = 1;

    tracker_view_clear_selection(view);

    /* All cells should be empty */
    ASSERT_EQ(tracker_pattern_get_cell(pattern, 0, 0)->type, TRACKER_CELL_EMPTY);
    ASSERT_EQ(tracker_pattern_get_cell(pattern, 0, 1)->type, TRACKER_CELL_EMPTY);
    ASSERT_EQ(tracker_pattern_get_cell(pattern, 1, 0)->type, TRACKER_CELL_EMPTY);
    ASSERT_EQ(tracker_pattern_get_cell(pattern, 1, 1)->type, TRACKER_CELL_EMPTY);

    /* Selection should be cleared */
    ASSERT_EQ(view->state.selection.type, TRACKER_SEL_NONE);

    free_test_view(view);
}

TEST(clear_selection_creates_undo_group) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 0, 0, "C4");
    set_test_expression(pattern, 0, 1, "E4");

    view->state.selection.type = TRACKER_SEL_RANGE;
    view->state.selection.start_track = 0;
    view->state.selection.end_track = 1;
    view->state.selection.start_row = 0;
    view->state.selection.end_row = 0;

    tracker_view_clear_selection(view);

    ASSERT_TRUE(tracker_undo_can_undo(&view->undo));

    free_test_view(view);
}

/*============================================================================
 * Cell Operations: insert_row Tests
 *============================================================================*/

TEST(insert_row_null_safe) {
    tracker_view_insert_row(NULL);
}

TEST(insert_row_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_insert_row(view);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(insert_row_shifts_cells_down) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 5, 0, "C4");
    set_test_expression(pattern, 6, 0, "E4");

    view->state.cursor_row = 5;

    tracker_view_insert_row(view);

    /* Row 5 should now be empty, old row 5 moved to row 6 */
    ASSERT_EQ(tracker_pattern_get_cell(pattern, 5, 0)->type, TRACKER_CELL_EMPTY);
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 6, 0)->expression, "C4");
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 7, 0)->expression, "E4");

    free_test_view(view);
}

TEST(insert_row_sets_modified) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(view->modified);
    tracker_view_insert_row(view);
    ASSERT_TRUE(view->modified);

    free_test_view(view);
}

TEST(insert_row_creates_undo) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    tracker_view_insert_row(view);
    ASSERT_TRUE(tracker_undo_can_undo(&view->undo));

    free_test_view(view);
}

/*============================================================================
 * Cell Operations: delete_row Tests
 *============================================================================*/

TEST(delete_row_null_safe) {
    tracker_view_delete_row(NULL);
}

TEST(delete_row_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_delete_row(view);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(delete_row_shifts_cells_up) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 5, 0, "C4");
    set_test_expression(pattern, 6, 0, "E4");
    set_test_expression(pattern, 7, 0, "G4");

    view->state.cursor_row = 5;

    tracker_view_delete_row(view);

    /* Row 5 should now have what was in row 6 */
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 5, 0)->expression, "E4");
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 6, 0)->expression, "G4");
    ASSERT_EQ(tracker_pattern_get_cell(pattern, 7, 0)->type, TRACKER_CELL_EMPTY);

    free_test_view(view);
}

TEST(delete_row_sets_modified) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(view->modified);
    tracker_view_delete_row(view);
    ASSERT_TRUE(view->modified);

    free_test_view(view);
}

TEST(delete_row_creates_undo) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    tracker_view_delete_row(view);
    ASSERT_TRUE(tracker_undo_can_undo(&view->undo));

    free_test_view(view);
}

/*============================================================================
 * Cell Operations: duplicate_row Tests
 *============================================================================*/

TEST(duplicate_row_null_safe) {
    tracker_view_duplicate_row(NULL);
}

TEST(duplicate_row_no_song_noop) {
    TrackerView* view = create_test_view();
    ASSERT_NOT_NULL(view);

    tracker_view_duplicate_row(view);

    tracker_view_state_cleanup(&view->state);
    tracker_undo_cleanup(&view->undo);
    free(view);
}

TEST(duplicate_row_copies_row) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    set_test_expression(pattern, 5, 0, "C4");
    set_test_expression(pattern, 5, 1, "E4");
    set_test_expression(pattern, 6, 0, "G4");

    view->state.cursor_row = 5;

    tracker_view_duplicate_row(view);

    /* Row 5 should be duplicated, original now at row 6 */
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 5, 0)->expression, "C4");
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 5, 1)->expression, "E4");
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 6, 0)->expression, "C4");
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 6, 1)->expression, "E4");
    /* Old row 6 (G4) should be at row 7 */
    ASSERT_STR_EQ(tracker_pattern_get_cell(pattern, 7, 0)->expression, "G4");

    free_test_view(view);
}

TEST(duplicate_row_sets_modified) {
    TrackerView* view = create_test_view_with_song(16, 4);
    ASSERT_NOT_NULL(view);

    ASSERT_FALSE(view->modified);
    tracker_view_duplicate_row(view);
    ASSERT_TRUE(view->modified);

    free_test_view(view);
}

/*============================================================================
 * Main
 *============================================================================*/

BEGIN_TEST_SUITE("Tracker View Clipboard Tests")

    /* Selection: select_start */
    RUN_TEST(select_start_null_safe);
    RUN_TEST(select_start_sets_selection_type);
    RUN_TEST(select_start_sets_anchor);
    RUN_TEST(select_start_sets_pattern);

    /* Selection: select_extend */
    RUN_TEST(select_extend_null_safe);
    RUN_TEST(select_extend_without_selection_noop);
    RUN_TEST(select_extend_forwards);
    RUN_TEST(select_extend_backwards);
    RUN_TEST(select_extend_preserves_anchor);

    /* Selection: select_clear */
    RUN_TEST(select_clear_null_safe);
    RUN_TEST(select_clear_resets_selection);

    /* Selection: select_track */
    RUN_TEST(select_track_null_safe);
    RUN_TEST(select_track_no_song_noop);
    RUN_TEST(select_track_selects_entire_track);

    /* Selection: select_row */
    RUN_TEST(select_row_null_safe);
    RUN_TEST(select_row_no_song_noop);
    RUN_TEST(select_row_selects_entire_row);

    /* Selection: select_pattern */
    RUN_TEST(select_pattern_null_safe);
    RUN_TEST(select_pattern_no_song_noop);
    RUN_TEST(select_pattern_selects_all);

    /* Selection: select_all */
    RUN_TEST(select_all_null_safe);
    RUN_TEST(select_all_selects_pattern);

    /* Selection: is_selected */
    RUN_TEST(is_selected_null_returns_false);
    RUN_TEST(is_selected_no_selection_returns_false);
    RUN_TEST(is_selected_in_selection_returns_true);
    RUN_TEST(is_selected_boundary_checks);

    /* Selection: get_selection */
    RUN_TEST(get_selection_null_returns_false);
    RUN_TEST(get_selection_no_selection_returns_false);
    RUN_TEST(get_selection_returns_bounds);
    RUN_TEST(get_selection_partial_output);

    /* Clipboard: clipboard_clear */
    RUN_TEST(clipboard_clear_null_safe);
    RUN_TEST(clipboard_clear_resets_state);

    /* Clipboard: clipboard_has_content */
    RUN_TEST(clipboard_has_content_null_returns_false);
    RUN_TEST(clipboard_has_content_empty_returns_false);
    RUN_TEST(clipboard_has_content_with_data_returns_true);
    RUN_TEST(clipboard_has_content_zero_size_returns_false);

    /* Clipboard: copy */
    RUN_TEST(copy_null_safe);
    RUN_TEST(copy_no_song_returns_false);
    RUN_TEST(copy_single_cell);
    RUN_TEST(copy_selection);
    RUN_TEST(copy_clears_previous_clipboard);
    RUN_TEST(copy_clamps_bounds);

    /* Clipboard: cut */
    RUN_TEST(cut_null_safe);
    RUN_TEST(cut_no_song_returns_false);
    RUN_TEST(cut_copies_and_clears);

    /* Clipboard: paste */
    RUN_TEST(paste_null_safe);
    RUN_TEST(paste_no_song_returns_false);
    RUN_TEST(paste_no_clipboard_returns_false);
    RUN_TEST(paste_single_cell);
    RUN_TEST(paste_over_filled_cell_undo_restores_it);
    RUN_TEST(paste_multiple_cells);
    RUN_TEST(paste_clips_at_boundary);

    /* Clipboard: paste_insert */
    RUN_TEST(paste_insert_null_safe);
    RUN_TEST(paste_insert_no_clipboard_returns_false);
    RUN_TEST(paste_insert_falls_back_to_paste);

    /* Cell Operations: clear_cell */
    RUN_TEST(clear_cell_null_safe);
    RUN_TEST(clear_cell_no_song_noop);
    RUN_TEST(clear_cell_clears_cursor_cell);
    RUN_TEST(clear_cell_creates_undo);

    /* Cell Operations: clear_selection */
    RUN_TEST(clear_selection_null_safe);
    RUN_TEST(clear_selection_no_song_noop);
    RUN_TEST(clear_selection_clears_cursor_when_no_selection);
    RUN_TEST(clear_selection_clears_selected_cells);
    RUN_TEST(clear_selection_creates_undo_group);

    /* Cell Operations: insert_row */
    RUN_TEST(insert_row_null_safe);
    RUN_TEST(insert_row_no_song_noop);
    RUN_TEST(insert_row_shifts_cells_down);
    RUN_TEST(insert_row_sets_modified);
    RUN_TEST(insert_row_creates_undo);

    /* Cell Operations: delete_row */
    RUN_TEST(delete_row_null_safe);
    RUN_TEST(delete_row_no_song_noop);
    RUN_TEST(delete_row_shifts_cells_up);
    RUN_TEST(delete_row_sets_modified);
    RUN_TEST(delete_row_creates_undo);

    /* Cell Operations: duplicate_row */
    RUN_TEST(duplicate_row_null_safe);
    RUN_TEST(duplicate_row_no_song_noop);
    RUN_TEST(duplicate_row_copies_row);
    RUN_TEST(duplicate_row_sets_modified);

END_TEST_SUITE()
