/* goto.c - Navigation commands (:goto, :<number>)
 *
 * Commands for cursor movement and navigation.
 */

#include <errno.h>
#include <limits.h>
#include "command_impl.h"

/* :goto, :<number> - Go to line number */
int cmd_goto(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        editor_set_status_msg(ctx, "Usage: :<line> or :goto <line>");
        return 0;
    }

    /* Parse line number. strtol so trailing garbage is rejected rather than
     * silently ignored, and so overflow is detected. */
    errno = 0;
    char *end = NULL;
    long parsed = strtol(args, &end, 10);
    if (end == args || errno == ERANGE || parsed < 1 || parsed > INT_MAX) {
        editor_set_status_msg(ctx, "Invalid line number: %s", args);
        return 0;
    }
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) {
        editor_set_status_msg(ctx, "Invalid line number: %s", args);
        return 0;
    }
    int line = (int)parsed;

    /* Clamp to valid range (1-indexed for user, 0-indexed internally) */
    if (line > ctx->model.numrows) {
        line = ctx->model.numrows;
    }
    if (line < 1) {
        editor_set_status_msg(ctx, "Buffer is empty");
        return 0;
    }

    /* Target file row, then split it into scroll offset + screen row. */
    int target = line - 1;
    ctx->view.cx = 0;
    ctx->view.coloff = 0;

    int rows = ctx->view.screenrows > 0 ? ctx->view.screenrows : 1;
    if (target < ctx->view.rowoff || target >= ctx->view.rowoff + rows) {
        /* Centre the target line when it is off-screen. */
        ctx->view.rowoff = target - rows / 2;
        if (ctx->view.rowoff > ctx->model.numrows - rows)
            ctx->view.rowoff = ctx->model.numrows - rows;
        if (ctx->view.rowoff < 0) ctx->view.rowoff = 0;
    }
    ctx->view.cy = target - ctx->view.rowoff;

    editor_set_status_msg(ctx, "Line %d", line);
    return 1;
}
