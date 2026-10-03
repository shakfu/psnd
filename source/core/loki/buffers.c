/* loki_buffers.c - Multiple buffer management implementation
 *
 * Manages multiple editor contexts (buffers) allowing users to edit
 * multiple files simultaneously with tab-like interface.
 */

#include "buffers.h"
#include "terminal.h"
#include "undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations */
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len);

/* Global buffer manager instance */
static buffer_manager_t default_buffer_manager = {0};
buffer_manager_t *g_buffer_manager = &default_buffer_manager;

/* ======================== Helper Functions ======================== */

/* Find buffer entry by ID in a specific manager
 * Returns: Pointer to buffer entry, or NULL if not found */
static buffer_entry_t *find_buffer_in(buffer_manager_t *mgr, int buffer_id) {
    if (!mgr) return NULL;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (mgr->buffers[i].active && mgr->buffers[i].id == buffer_id) {
            return &mgr->buffers[i];
        }
    }
    return NULL;
}

/* Find first free buffer slot in a specific manager
 * Returns: Pointer to free slot, or NULL if all slots full */
static buffer_entry_t *find_free_slot_in(buffer_manager_t *mgr) {
    if (!mgr) return NULL;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!mgr->buffers[i].active) {
            return &mgr->buffers[i];
        }
    }
    return NULL;
}

/* Update display name for buffer */
static void update_display_name(buffer_entry_t *buf) {
    if (!buf) return;

    if (buf->ctx.model.filename && buf->ctx.model.filename[0] != '\0') {
        /* Extract basename from path */
        const char *basename = strrchr(buf->ctx.model.filename, '/');
        basename = basename ? basename + 1 : buf->ctx.model.filename;

        /* Truncate if too long */
        if (strlen(basename) > 50) {
            snprintf(buf->display_name, sizeof(buf->display_name), "...%s", basename + strlen(basename) - 47);
        } else {
            snprintf(buf->display_name, sizeof(buf->display_name), "%s", basename);
        }
    } else {
        snprintf(buf->display_name, sizeof(buf->display_name), "[No Name]");
    }
}

/* ======================== Buffer Manager API ======================== */

buffer_manager_t *buffer_manager_create(void) {
    buffer_manager_t *mgr = calloc(1, sizeof(buffer_manager_t));
    if (!mgr) return NULL;
    mgr->current_buffer_id = -1;
    mgr->next_id = 1;
    return mgr;
}

void buffer_manager_destroy(buffer_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Free all active buffers */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (mgr->buffers[i].active) {
            /* Note: Don't free lua_host here - it's shared and freed separately */
            mgr->buffers[i].ctx.lua_host = NULL;
            editor_ctx_free(&mgr->buffers[i].ctx);
            mgr->buffers[i].active = 0;
        }
    }

    /* Reset buffer manager state completely */
    mgr->initialized = 0;
    mgr->current_buffer_id = 0;
    mgr->next_id = 1;
    mgr->shared_lua_host = NULL;
    mgr->shared_ctx = NULL;
}

int buffer_manager_init(buffer_manager_t *mgr, editor_ctx_t *initial_ctx) {
    if (!mgr || mgr->initialized) {
        return -1;  /* Already initialized */
    }

    /* Initialize buffer array */
    memset(mgr->buffers, 0, sizeof(mgr->buffers));
    mgr->current_buffer_id = -1;
    mgr->next_id = 1;
    mgr->initialized = 1;

    /* Create first buffer using the provided context */
    buffer_entry_t *first = &mgr->buffers[0];
    first->active = 1;
    first->id = mgr->next_id++;

    /* Initialize fresh context and copy only essential state from initial_ctx */
    editor_ctx_init(&first->ctx);

    /* Copy display state (rawmode now lives in TerminalHost, not per-buffer) */
    first->ctx.view.screencols = initial_ctx->view.screencols;
    first->ctx.view.screenrows = initial_ctx->view.screenrows;
    first->ctx.view.screenrows_total = initial_ctx->view.screenrows_total;
    first->ctx.lua_host = initial_ctx->lua_host;  /* Share Lua host across buffers */
    mgr->shared_lua_host = initial_ctx->lua_host;
    memcpy(first->ctx.view.colors, initial_ctx->view.colors, sizeof(first->ctx.view.colors));

    /* Copy display settings */
    first->ctx.view.line_numbers = initial_ctx->view.line_numbers;

    /* Transfer ownership of buffer content from initial_ctx to first buffer.
     * We take ownership of the pointers and NULL them in initial_ctx to prevent
     * double-free when initial_ctx is cleaned up. */
    first->ctx.model.numrows = initial_ctx->model.numrows;
    first->ctx.model.row = initial_ctx->model.row;
    initial_ctx->model.row = NULL;  /* Transfer ownership */
    initial_ctx->model.numrows = 0;

    first->ctx.model.filename = initial_ctx->model.filename;
    initial_ctx->model.filename = NULL;  /* Transfer ownership */

    first->ctx.view.syntax = initial_ctx->view.syntax;
    first->ctx.model.dirty = initial_ctx->model.dirty;

    /* Copy cursor state */
    first->ctx.view.cx = initial_ctx->view.cx;
    first->ctx.view.cy = initial_ctx->view.cy;
    first->ctx.view.rowoff = initial_ctx->view.rowoff;
    first->ctx.view.coloff = initial_ctx->view.coloff;

    update_display_name(first);
    mgr->current_buffer_id = first->id;

    return 0;
}

/* ======================== Initialization (Global Wrappers) ======================== */

int buffers_init(editor_ctx_t *initial_ctx) {
    return buffer_manager_init(g_buffer_manager, initial_ctx);
}

void buffers_free(void) {
    buffer_manager_destroy(g_buffer_manager);
}

/* ======================== Buffer Operations ======================== */

int buffer_create_in(buffer_manager_t *mgr, const char *filename) {
    if (!mgr || !mgr->initialized) return -1;

    /* Get current buffer context to copy terminal state BEFORE creating new one */
    editor_ctx_t *template_ctx = buffer_get_current_in(mgr);

    /* Find free slot */
    buffer_entry_t *buf = find_free_slot_in(mgr);
    if (!buf) {
        return -1;  /* No free slots */
    }

    /* Initialize new buffer */
    buf->active = 1;
    buf->id = mgr->next_id++;

    /* Initialize editor context */
    editor_ctx_init(&buf->ctx);

    /* Copy display state from template buffer (rawmode now in TerminalHost) */
    if (template_ctx) {
        buf->ctx.view.screencols = template_ctx->view.screencols;
        buf->ctx.view.screenrows = template_ctx->view.screenrows;
        buf->ctx.view.screenrows_total = template_ctx->view.screenrows_total;
        buf->ctx.lua_host = template_ctx->lua_host;  /* Share Lua host */
        /* Copy color scheme */
        memcpy(buf->ctx.view.colors, template_ctx->view.colors, sizeof(buf->ctx.view.colors));
        /* Copy display settings */
        buf->ctx.view.line_numbers = template_ctx->view.line_numbers;
    } else if (mgr->shared_lua_host) {
        buf->ctx.lua_host = mgr->shared_lua_host;
    }

    /* Open file if provided */
    if (filename) {
        if (editor_open(&buf->ctx, (char *)filename) != 0) {
            /* Failed to open file - clean up */
            editor_ctx_free(&buf->ctx);
            buf->active = 0;
            return -1;
        }
    } else {
        /* Empty buffer - insert one empty row so it displays properly */
        editor_insert_row(&buf->ctx, 0, "", 0);
        /* Reset dirty flag - empty buffer shouldn't be marked as modified */
        buf->ctx.model.dirty = 0;
    }

    update_display_name(buf);

    return buf->id;
}

int buffer_create(const char *filename) {
    return buffer_create_in(g_buffer_manager, filename);
}

int buffer_close(int buffer_id, int force) {
    if (!g_buffer_manager || !g_buffer_manager->initialized) return -1;

    buffer_entry_t *buf = find_buffer_in(g_buffer_manager, buffer_id);
    if (!buf) return -1;

    /* Check for unsaved changes */
    if (!force && buf->ctx.model.dirty) {
        return 1;  /* Has unsaved changes */
    }

    /* Can't close last buffer */
    if (buffer_count() <= 1) {
        return -1;
    }

    /* If closing current buffer, switch to another first */
    if (buffer_id == g_buffer_manager->current_buffer_id) {
        /* Try to switch to next buffer */
        int next_id = buffer_next();
        if (next_id == buffer_id) {
            /* Only one buffer left - can't close */
            return -1;
        }
    }

    /* Free buffer resources */
    editor_ctx_free(&buf->ctx);
    buf->active = 0;

    return 0;
}

int buffer_switch_in(buffer_manager_t *mgr, int buffer_id) {
    if (!mgr || !mgr->initialized) return -1;

    buffer_entry_t *buf = find_buffer_in(mgr, buffer_id);
    if (!buf) return -1;

    /* Save current state before switching */
    buffers_save_current_state();

    /* Switch to new buffer */
    mgr->current_buffer_id = buffer_id;

    /* Restore new buffer state */
    buffers_restore_state(&buf->ctx);

    return 0;
}

int buffer_switch(int buffer_id) {
    return buffer_switch_in(g_buffer_manager, buffer_id);
}

int buffer_next(void) {
    if (!g_buffer_manager || !g_buffer_manager->initialized || buffer_count() == 0) return -1;

    int found_current = 0;

    /* Find next active buffer after current */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!g_buffer_manager->buffers[i].active) continue;

        if (found_current) {
            /* Found next buffer */
            buffer_switch(g_buffer_manager->buffers[i].id);
            return g_buffer_manager->buffers[i].id;
        }

        if (g_buffer_manager->buffers[i].id == g_buffer_manager->current_buffer_id) {
            found_current = 1;
        }
    }

    /* Wrap around to first buffer */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (g_buffer_manager->buffers[i].active) {
            buffer_switch(g_buffer_manager->buffers[i].id);
            return g_buffer_manager->buffers[i].id;
        }
    }

    return -1;
}

int buffer_prev(void) {
    if (!g_buffer_manager || !g_buffer_manager->initialized || buffer_count() == 0) return -1;

    int prev_id = -1;

    /* Find previous active buffer before current */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!g_buffer_manager->buffers[i].active) continue;

        if (g_buffer_manager->buffers[i].id == g_buffer_manager->current_buffer_id) {
            /* Found current, prev_id has previous */
            if (prev_id != -1) {
                buffer_switch(prev_id);
                return prev_id;
            }
            break;
        }

        prev_id = g_buffer_manager->buffers[i].id;
    }

    /* Wrap around to last buffer */
    for (int i = MAX_BUFFERS - 1; i >= 0; i--) {
        if (g_buffer_manager->buffers[i].active) {
            buffer_switch(g_buffer_manager->buffers[i].id);
            return g_buffer_manager->buffers[i].id;
        }
    }

    return -1;
}

/* ======================== Query Functions ======================== */

editor_ctx_t *buffer_get_current_in(buffer_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return NULL;

    buffer_entry_t *buf = find_buffer_in(mgr, mgr->current_buffer_id);
    return buf ? &buf->ctx : NULL;
}

editor_ctx_t *buffer_get_current(void) {
    return buffer_get_current_in(g_buffer_manager);
}

editor_ctx_t *buffer_get_in(buffer_manager_t *mgr, int buffer_id) {
    if (!mgr || !mgr->initialized) return NULL;

    buffer_entry_t *buf = find_buffer_in(mgr, buffer_id);
    return buf ? &buf->ctx : NULL;
}

editor_ctx_t *buffer_get(int buffer_id) {
    return buffer_get_in(g_buffer_manager, buffer_id);
}

int buffer_get_current_id_in(buffer_manager_t *mgr) {
    return (mgr && mgr->initialized) ? mgr->current_buffer_id : -1;
}

int buffer_get_current_id(void) {
    return buffer_get_current_id_in(g_buffer_manager);
}

int buffer_count_in(buffer_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    int count = 0;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (mgr->buffers[i].active) {
            count++;
        }
    }
    return count;
}

int buffer_count(void) {
    return buffer_count_in(g_buffer_manager);
}

int buffer_get_list(int *ids) {
    if (!g_buffer_manager || !g_buffer_manager->initialized || !ids) return 0;

    int count = 0;
    for (int i = 0; i < MAX_BUFFERS && count < MAX_BUFFERS; i++) {
        if (g_buffer_manager->buffers[i].active) {
            ids[count++] = g_buffer_manager->buffers[i].id;
        }
    }
    return count;
}

const char *buffer_get_display_name(int buffer_id) {
    buffer_entry_t *buf = find_buffer_in(g_buffer_manager, buffer_id);
    return buf ? buf->display_name : NULL;
}

int buffer_is_modified(int buffer_id) {
    buffer_entry_t *buf = find_buffer_in(g_buffer_manager, buffer_id);
    if (!buf) return -1;
    return buf->ctx.model.dirty ? 1 : 0;
}

/* ======================== State Management ======================== */

void buffers_save_current_state(void) {
    /* Current state is already in the buffer context - nothing to do.
     * The editor context contains all necessary state (cursor position, etc.)
     * This function is here for future extensions if needed. */
}

void buffers_restore_state(editor_ctx_t *ctx) {
    /* State is restored by switching the active context pointer.
     * This function is here for future extensions if needed. */
    (void)ctx;  /* Unused for now */
}

/* ======================== Utility Functions ======================== */

void buffer_update_display_name(int buffer_id) {
    buffer_entry_t *buf = find_buffer_in(g_buffer_manager, buffer_id);
    if (buf) {
        update_display_name(buf);
    }
}

/* ======================== Rendering ======================== */

void buffers_render_tabs(struct abuf *ab, int max_width) {
    if (!g_buffer_manager || !g_buffer_manager->initialized || !ab) return;

    int count = buffer_count();
    if (count <= 1) {
        /* Single buffer - don't show tabs */
        return;
    }

    (void)max_width;  /* Not needed for simple numeric tabs */

    /* Render each active buffer as a simple numbered tab: [1] [2] [3] */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!g_buffer_manager->buffers[i].active) continue;

        buffer_entry_t *buf = &g_buffer_manager->buffers[i];
        int is_current = (buf->id == g_buffer_manager->current_buffer_id);

        /* Highlight current tab with reverse video */
        if (is_current) {
            terminal_buffer_append(ab, "\x1b[7m", 4);  /* Reverse video */
        }

        /* Simple tab format: [N] where N is the buffer ID */
        char tab_str[8];
        snprintf(tab_str, sizeof(tab_str), "[%d]", buf->id);
        terminal_buffer_append(ab, tab_str, strlen(tab_str));

        if (is_current) {
            terminal_buffer_append(ab, "\x1b[m", 3);   /* Reset */
        }

        terminal_buffer_append(ab, " ", 1);  /* Space between tabs */
    }

    /* Clear to end of line and add newline to separate tabs from status bar */
    terminal_buffer_append(ab, "\x1b[0K", 4);  /* Clear to end of line */
    terminal_buffer_append(ab, "\r\n", 2);
}

/* Get tab info for renderer abstraction */
int buffers_get_tab_info(char ***tabs, int *tab_count, int *active_tab) {
    if (!g_buffer_manager || !g_buffer_manager->initialized || !tabs || !tab_count || !active_tab) {
        return -1;
    }

    int count = buffer_count();
    if (count <= 1) {
        *tabs = NULL;
        *tab_count = 0;
        *active_tab = 0;
        return 0;
    }

    /* Allocate array of tab labels */
    char **labels = malloc(count * sizeof(char *));
    if (!labels) return -1;

    int idx = 0;
    int active_idx = 0;
    for (int i = 0; i < MAX_BUFFERS && idx < count; i++) {
        if (!g_buffer_manager->buffers[i].active) continue;

        buffer_entry_t *buf = &g_buffer_manager->buffers[i];

        /* Create label: "[N]" format */
        labels[idx] = malloc(8);
        if (!labels[idx]) {
            /* Cleanup on failure */
            for (int j = 0; j < idx; j++) free(labels[j]);
            free(labels);
            return -1;
        }
        snprintf(labels[idx], 8, "[%d]", buf->id);

        if (buf->id == g_buffer_manager->current_buffer_id) {
            active_idx = idx;
        }
        idx++;
    }

    *tabs = labels;
    *tab_count = count;
    *active_tab = active_idx;
    return 0;
}

/* Free tab info allocated by buffers_get_tab_info */
void buffers_free_tab_info(char **tabs, int tab_count) {
    if (tabs) {
        for (int i = 0; i < tab_count; i++) {
            free(tabs[i]);
        }
        free(tabs);
    }
}
