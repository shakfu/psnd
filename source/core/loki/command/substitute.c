/* substitute.c - Search and replace command (:s/old/new/)
 *
 * Vim-style substitution on current line.
 */

#include "command_impl.h"

/* :s/old/new/[g] - Search and replace on current line */
int cmd_substitute(editor_ctx_t *ctx, const char *pattern) {
    if (!pattern || pattern[0] != 's' || pattern[1] != '/') {
        editor_set_status_msg(ctx, "Usage: :s/old/new/[g]");
        return 0;
    }

    /* Parse s/old/new/[g] pattern */
    const char *p = pattern + 2;  /* Skip "s/" */

    /* Find the "old" string (up to next unescaped /) */
    char old_str[256] = {0};
    int old_len = 0;
    while (*p && *p != '/' && old_len < 255) {
        if (*p == '\\' && *(p + 1)) {
            /* Escaped character */
            p++;
            old_str[old_len++] = *p++;
        } else {
            old_str[old_len++] = *p++;
        }
    }
    old_str[old_len] = '\0';

    if (*p != '/') {
        editor_set_status_msg(ctx, "Invalid substitute pattern");
        return 0;
    }
    p++;  /* Skip middle '/' */

    /* Find the "new" string (up to next unescaped / or end) */
    char new_str[256] = {0};
    int new_len = 0;
    while (*p && *p != '/' && new_len < 255) {
        if (*p == '\\' && *(p + 1)) {
            /* Escaped character */
            p++;
            new_str[new_len++] = *p++;
        } else {
            new_str[new_len++] = *p++;
        }
    }
    new_str[new_len] = '\0';

    /* Check for global flag */
    int global = 0;
    if (*p == '/') {
        p++;
        while (*p) {
            if (*p == 'g') global = 1;
            p++;
        }
    }

    if (old_len == 0) {
        editor_set_status_msg(ctx, "Empty search pattern");
        return 0;
    }

    /* Perform substitution on current line */
    int filerow = ctx->view.rowoff + ctx->view.cy;
    if (filerow < 0 || filerow >= ctx->model.numrows) {
        editor_set_status_msg(ctx, "No line to substitute");
        return 0;
    }

    t_erow *row = &ctx->model.row[filerow];
    char *line = row->chars;
    int line_len = row->size;

    /* First pass: count matches so the output can be sized exactly. A fixed
     * buffer here used to truncate long lines and lose their tails. */
    int count = 0;
    for (int i = 0; i + old_len <= line_len; ) {
        if (strncmp(line + i, old_str, old_len) == 0) {
            count++;
            i += old_len;
            if (!global) break;
        } else {
            i++;
        }
    }

    if (count == 0) {
        editor_set_status_msg(ctx, "Pattern not found: %s", old_str);
        return 0;
    }

    size_t out_cap = (size_t)line_len + (size_t)count * (size_t)new_len + 1;
    char *new_line = malloc(out_cap);
    if (!new_line) {
        editor_set_status_msg(ctx, "Out of memory");
        return 0;
    }

    /* Second pass: build the result. */
    int new_line_len = 0;
    int done = 0;
    int i = 0;
    while (i < line_len) {
        if (!done && i + old_len <= line_len &&
            strncmp(line + i, old_str, old_len) == 0) {
            memcpy(new_line + new_line_len, new_str, new_len);
            new_line_len += new_len;
            i += old_len;
            if (!global) done = 1;
        } else {
            new_line[new_line_len++] = line[i++];
        }
    }
    new_line[new_line_len] = '\0';

    /* Update the row. Hand over the buffer we already own rather than
     * freeing the old text before a strdup that might fail. */
    free(row->chars);
    row->chars = new_line;
    row->size = new_line_len;

    /* Update render */
    editor_update_row(ctx, row);

    ctx->model.dirty++;
    editor_set_status_msg(ctx, "%d substitution%s", count, count > 1 ? "s" : "");
    return 1;
}
