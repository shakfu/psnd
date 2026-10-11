/* picker.c - Modal picker implementation
 *
 * Provides a full-screen selection UI integrated with the modal editor.
 */

#include "picker.h"
#include "internal.h"
#include "terminal.h"
#include <string.h>

/* Helper: Clamp selected index to valid range */
static void clamp_selection(editor_ctx_t *ctx) {
    if (ctx->view.picker.selected_index < 0) {
        ctx->view.picker.selected_index = 0;
    }
    if (ctx->view.picker.selected_index >= ctx->view.picker.item_count) {
        ctx->view.picker.selected_index = ctx->view.picker.item_count - 1;
    }
    if (ctx->view.picker.selected_index < 0) {
        ctx->view.picker.selected_index = 0;
    }
}

/* Helper: Ensure selected item is visible by adjusting scroll */
static void ensure_visible(editor_ctx_t *ctx) {
    /* Calculate visible rows (leave room for title and status) */
    int visible_rows = ctx->view.screenrows - 3;
    if (visible_rows < 1) visible_rows = 1;

    int sel = ctx->view.picker.selected_index;
    int off = ctx->view.picker.scroll_offset;

    /* Scroll down if selection is below visible area */
    if (sel >= off + visible_rows) {
        ctx->view.picker.scroll_offset = sel - visible_rows + 1;
    }
    /* Scroll up if selection is above visible area */
    if (sel < off) {
        ctx->view.picker.scroll_offset = sel;
    }
    /* Clamp scroll offset */
    if (ctx->view.picker.scroll_offset < 0) {
        ctx->view.picker.scroll_offset = 0;
    }
}

int picker_open(editor_ctx_t *ctx, const char *title,
                const char **items, int count,
                picker_callback_t on_select,
                void *user_data) {
    if (!ctx || !items || count <= 0) {
        return -1;
    }

    /* Store picker state */
    ctx->view.picker.items = items;
    ctx->view.picker.item_count = count;
    ctx->view.picker.selected_index = 0;
    ctx->view.picker.scroll_offset = 0;
    ctx->view.picker.title = title;
    ctx->view.picker.on_select = on_select;
    ctx->view.picker.on_cancel = NULL;
    ctx->view.picker.user_data = user_data;
    ctx->view.picker.prev_mode = ctx->view.mode;

    /* Enter picker mode */
    ctx->view.mode = MODE_PICKER;

    return 0;
}

void picker_close(editor_ctx_t *ctx) {
    if (!ctx || ctx->view.mode != MODE_PICKER) {
        return;
    }

    /* Restore previous mode */
    ctx->view.mode = ctx->view.picker.prev_mode;

    /* Clear picker state */
    ctx->view.picker.items = NULL;
    ctx->view.picker.item_count = 0;
    ctx->view.picker.title = NULL;
    ctx->view.picker.on_select = NULL;
    ctx->view.picker.on_cancel = NULL;
    ctx->view.picker.user_data = NULL;
}

int picker_handle_key(editor_ctx_t *ctx, int key) {
    if (!ctx || ctx->view.mode != MODE_PICKER) {
        return 0;
    }

    int visible_rows = ctx->view.screenrows - 3;
    if (visible_rows < 1) visible_rows = 1;

    switch (key) {
        case 'j':
        case ARROW_DOWN:
            ctx->view.picker.selected_index++;
            clamp_selection(ctx);
            ensure_visible(ctx);
            break;

        case 'k':
        case ARROW_UP:
            ctx->view.picker.selected_index--;
            clamp_selection(ctx);
            ensure_visible(ctx);
            break;

        case CTRL_D:
        case PAGE_DOWN:
            /* Page down */
            ctx->view.picker.selected_index += visible_rows;
            clamp_selection(ctx);
            ensure_visible(ctx);
            break;

        case CTRL_U:
        case PAGE_UP:
            /* Page up */
            ctx->view.picker.selected_index -= visible_rows;
            clamp_selection(ctx);
            ensure_visible(ctx);
            break;

        case 'g':
            /* Go to top */
            ctx->view.picker.selected_index = 0;
            ctx->view.picker.scroll_offset = 0;
            break;

        case 'G':
            /* Go to bottom */
            ctx->view.picker.selected_index = ctx->view.picker.item_count - 1;
            ensure_visible(ctx);
            break;

        case ENTER:
            /* Select current item */
            {
                int selected = ctx->view.picker.selected_index;
                picker_callback_t callback = ctx->view.picker.on_select;
                void *data = ctx->view.picker.user_data;

                picker_close(ctx);

                if (callback) {
                    callback(ctx, selected, data);
                }
            }
            return 0;  /* Picker closed */

        case ESC:
        case 'q':
            /* Cancel */
            {
                picker_callback_t callback = ctx->view.picker.on_select;
                void *data = ctx->view.picker.user_data;

                picker_close(ctx);

                if (callback) {
                    callback(ctx, -1, data);  /* -1 indicates cancel */
                }
            }
            return 0;  /* Picker closed */

        default:
            /* Ignore unknown keys */
            break;
    }

    return 1;  /* Picker still active */
}

/* Blocking picker state (for picker_select_blocking) */
static struct {
    int result;
    int done;
} blocking_state;

static void blocking_callback(editor_ctx_t *ctx, int index, void *data) {
    (void)ctx;
    (void)data;
    blocking_state.result = index;
    blocking_state.done = 1;
}

int picker_select_blocking(editor_ctx_t *ctx, int fd,
                           const char *title, const char **items, int count) {
    if (!ctx || !items || count <= 0) {
        return -1;
    }

    blocking_state.result = -1;
    blocking_state.done = 0;

    if (picker_open(ctx, title, items, count, blocking_callback, NULL) != 0) {
        return -1;
    }

    /* Event loop until picker closes */
    while (!blocking_state.done) {
        editor_refresh_screen(ctx);
        int key = terminal_read_key(fd);
        if (key == -1) key = ESC;  /* Input error: close the picker. */
        picker_handle_key(ctx, key);
    }

    return blocking_state.result;
}

int picker_get_selected(editor_ctx_t *ctx) {
    if (!ctx || ctx->view.mode != MODE_PICKER) {
        return -1;
    }
    return ctx->view.picker.selected_index;
}

void picker_set_selected(editor_ctx_t *ctx, int index) {
    if (!ctx || ctx->view.mode != MODE_PICKER) {
        return;
    }
    ctx->view.picker.selected_index = index;
    clamp_selection(ctx);
    ensure_visible(ctx);
}
