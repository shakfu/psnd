/* file.c - File operation commands (:w, :e)
 *
 * Commands for saving and opening files.
 */

#include "command_impl.h"

/* :w, :write - Save file */
int cmd_write(editor_ctx_t *ctx, const char *args) {
    char *previous_name = NULL;
    int renamed = 0;

    /* Use provided filename or current filename */
    if (args && args[0]) {
        char *new_name = strdup(args);
        if (!new_name) {
            editor_set_status_msg(ctx, "Out of memory");
            return 0;
        }
        /* Keep the old name so it can be restored if the write fails. */
        previous_name = ctx->model.filename;
        ctx->model.filename = new_name;
        renamed = 1;
    }

    if (!ctx->model.filename) {
        editor_set_status_msg(ctx, "No filename");
        return 0;
    }

    /* Save file using existing editor_save() */
    int len = editor_save(ctx);
    if (len < 0 && renamed) {
        /* Roll the rename back: the buffer still belongs to the old file. */
        free(ctx->model.filename);
        ctx->model.filename = previous_name;
        previous_name = NULL;
    }
    if (renamed && len >= 0) {
        free(previous_name);
        previous_name = NULL;
        buffer_update_display_name(buffer_get_current_id());
    }
    if (len >= 0) {
        editor_set_status_msg(ctx, "\"%s\" %dL written",
                             ctx->model.filename, ctx->model.numrows);
        ctx->model.dirty = 0;
        return 1;
    } else {
        editor_set_status_msg(ctx, "Error writing file");
        return 0;
    }
}

/* :e, :edit - Open file */
int cmd_edit(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        editor_set_status_msg(ctx, "Filename required");
        return 0;
    }

    if (ctx->model.dirty) {
        editor_set_status_msg(ctx, "Unsaved changes! Save first or use :q!");
        return 0;
    }

    /* Load new file */
    editor_open(ctx, (char*)args);  /* Cast away const - editor_open doesn't modify */
    editor_set_status_msg(ctx, "\"%s\" loaded", args);
    return 1;
}
