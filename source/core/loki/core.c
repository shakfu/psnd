#include "psnd.h"

/* 
loki is based on kilo -- A very simple editor in less than 1000 
lines of code (as counted by "cloc"). Does not depend on libcurses,
directly emits VT100 escapes on the terminal.

Copyright (c) 2016, Salvatore Sanfilippo <antirez at gmail dot com>

see LICENSE.
*/


#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define isatty _isatty
#define fileno _fileno
#define write _write
#define read _read
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define ftruncate _chsize

/* Windows getline implementation */
static ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;

    size_t pos = 0;
    int c;

    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = (char*)malloc(*n);
        if (!*lineptr) return -1;
    }

    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            *n *= 2;
            char *new_ptr = (char*)realloc(*lineptr, *n);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == '\n') break;
    }

    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}
#else
#include <termios.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#endif

#include "loki/editor.h"
#include "internal.h"
#include "selection.h"
#include "search.h"
#include "modal.h"
#include "command.h"
#include "terminal.h"
#include "undo.h"
#include "buffers.h"
#include "syntax.h"
#include "treesitter.h"
#include "indent.h"
#include "lang_bridge.h"
#include "loki/link.h"
#include "picker.h"
#include "command/command_impl.h"
#include "window.h"

void editor_set_status_msg(editor_ctx_t *ctx, const char *fmt, ...) {
    if (!ctx) return;
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(ctx->view.statusmsg,sizeof(ctx->view.statusmsg),fmt,ap);
    va_end(ap);
    ctx->view.statusmsg_time = time(NULL);
}

/* ======================= Context Management =============================== */

/* Initialize an editor context with default values.
 * This allows creating independent editor contexts for split windows
 * and multiple buffer support. */
void editor_ctx_init(editor_ctx_t *ctx) {
    memset(ctx, 0, sizeof(editor_ctx_t));
    ctx->view.cx = 0;
    ctx->view.cy = 0;
    ctx->view.rowoff = 0;
    ctx->view.coloff = 0;
    ctx->view.screenrows = 0;
    ctx->view.screencols = 0;
    ctx->view.screenrows_total = 0;
    ctx->model.numrows = 0;
    /* Note: rawmode now lives in TerminalHost, not per-buffer */
    ctx->model.row = NULL;
    ctx->model.dirty = 0;
    ctx->model.filename = NULL;
    ctx->view.statusmsg[0] = '\0';
    ctx->view.statusmsg_time = 0;
    ctx->view.syntax = NULL;
    ctx->lua_host = NULL;  /* Lua host is shared across buffers, set by editor_main */
    ctx->view.mode = MODE_NORMAL;
    ctx->view.word_wrap = 0;
    ctx->view.sel_active = 0;
    ctx->view.sel_start_x = 0;
    ctx->view.sel_start_y = 0;
    ctx->view.sel_end_x = 0;
    ctx->view.sel_end_y = 0;
    /* Note: winsize_changed now lives in TerminalHost, not per-buffer */
    memset(ctx->view.colors, 0, sizeof(ctx->view.colors));
    /* Command mode state */
    command_mode_init(ctx);
    ctx->view.quit_times = KILO_QUIT_TIMES;
    /* Undo/redo system (1000 operations, 10MB memory limit) */
    undo_init(ctx, 1000, 10 * 1024 * 1024);
    /* Auto-indent system */
    indent_init(ctx);
    /* Window manager for split windows (starts as NULL, single-pane mode) */
    ctx->wm = NULL;
}

/* Free all dynamically allocated memory in a context.
 * This should be called when a context is no longer needed. */
void editor_ctx_free(editor_ctx_t *ctx) {
    /* Free all row data */
    for (int i = 0; i < ctx->model.numrows; i++) {
        free(ctx->model.row[i].chars);
        free(ctx->model.row[i].render);
        free(ctx->model.row[i].hl);
    }
    free(ctx->model.row);

    /* Free filename */
    free(ctx->model.filename);

    /* Note: We don't free ctx->lua_host (LuaHost) as it's shared across contexts
     * and managed separately by editor_cleanup_resources() */

    /* Free renderer if owned by this context */
    if (ctx->renderer) {
        ctx->renderer->destroy(ctx->renderer);
        ctx->renderer = NULL;
    }

    /* Free command mode state */
    command_mode_free(ctx);

    /* Free undo/redo state */
    undo_free(ctx);

    /* Free indent configuration */
    free(ctx->model.indent_config);

#ifdef LOKI_USE_LINENOISE
    if (ctx->model.ts_state) {
        treesitter_free(ctx->model.ts_state);
        ctx->model.ts_state = NULL;
    }
#endif

    /* Free window manager */
    if (ctx->wm) {
        window_manager_destroy(ctx->wm);
        ctx->wm = NULL;
    }

    /* Clear the structure */
    memset(ctx, 0, sizeof(editor_ctx_t));
}

/* Set the renderer for this editor context.
 * If a renderer was previously set, it is destroyed.
 * The context takes ownership of the renderer. */
void editor_ctx_set_renderer(editor_ctx_t *ctx, Renderer *renderer) {
    if (ctx->renderer) {
        ctx->renderer->destroy(ctx->renderer);
    }
    ctx->renderer = renderer;
}

/* =========================== Syntax highlights DB =========================
 *
 * Built-in language definitions are in loki_languages.c.
 * Dynamic languages can be registered via loki.register_language() in Lua.
 */

#include "languages.h"

/* Static pointer to editor context for atexit cleanup.
 * Set by init_editor() before registering atexit handler. */
static editor_ctx_t *editor_for_atexit = NULL;

/* Set the context used for atexit cleanup */
void editor_set_atexit_context(editor_ctx_t *ctx) {
    editor_for_atexit = ctx;
}

/* Called at exit to avoid remaining in raw mode. */
void editor_atexit(void) {
    /* Restore terminal via TerminalHost */
    terminal_host_disable_raw_mode(g_terminal_host);

    /* Cleanup editor resources */
    if (editor_for_atexit) {
        editor_cleanup_resources(editor_for_atexit);
    }
    /* Release every buffer's rows and undo history, not just the active one. */
    buffers_free();
    cleanup_dynamic_languages();
}

/* ======================= Editor rows implementation ======================= */

/* Update the rendered version and the syntax highlight of a row. */
void editor_update_row(editor_ctx_t *ctx, t_erow *row) {
    unsigned int tabs = 0;
    int j, idx;

   /* Create a version of the row we can directly print on the screen,
     * respecting tabs, substituting non printable characters with '?'. */
    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB) tabs++;

    unsigned long long allocsize =
        (unsigned long long) row->size + tabs*8 + 1;
    if (allocsize > UINT32_MAX) {
        printf("Some line of the edited file is too long for loki\n");
        exit(1);
    }

    row->render = malloc(row->size + tabs*8 + 1);
    if (row->render == NULL) {
        perror("Out of memory");
        exit(1);
    }
    idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == TAB) {
            row->render[idx++] = ' ';
            while((idx+1) % 8 != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->rsize = idx;
    row->render[idx] = '\0';

    /* Update the syntax highlighting attributes of the row. */
    syntax_update_row(ctx, row);
}

/* Insert a row at the specified position, shifting the other rows on the bottom
 * if required. */
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len) {
    if (at < 0 || at > ctx->model.numrows) return;
    /* Guard against overflowing the int row counter itself; the byte-size
     * comparison against SIZE_MAX could never trigger on any real platform. */
    if (ctx->model.numrows >= INT_MAX - 1) {
        fprintf(stderr, "Too many rows, cannot allocate more memory\n");
        exit(1);
    }
    t_erow *new_row = realloc(ctx->model.row,sizeof(t_erow)*(ctx->model.numrows+1));
    if (new_row == NULL) {
        perror("Out of memory");
        exit(1);
    }
    ctx->model.row = new_row;
    if (at != ctx->model.numrows) {
        memmove(ctx->model.row+at+1,ctx->model.row+at,sizeof(ctx->model.row[0])*(ctx->model.numrows-at));
        for (int j = at+1; j <= ctx->model.numrows; j++) ctx->model.row[j].idx++;
    }
    ctx->model.row[at].size = len;
    ctx->model.row[at].chars = malloc(len+1);
    if (ctx->model.row[at].chars == NULL) {
        perror("Out of memory");
        exit(1);
    }
    memcpy(ctx->model.row[at].chars,s,len);
    ctx->model.row[at].chars[len] = '\0';
    ctx->model.row[at].hl = NULL;
    ctx->model.row[at].hl_oc = 0;
    ctx->model.row[at].cb_lang = CB_LANG_NONE;
    ctx->model.row[at].render = NULL;
    ctx->model.row[at].rsize = 0;
    ctx->model.row[at].idx = at;
    editor_update_row(ctx, ctx->model.row+at);
    ctx->model.numrows++;
    ctx->model.dirty++;
}

/* Free row's heap allocated stuff. */
void editor_free_row(t_erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

/* Remove the row at the specified position, shifting the remaining on the
 * top. */
void editor_del_row(editor_ctx_t *ctx, int at) {
    t_erow *row;

    if (at < 0 || at >= ctx->model.numrows) return;
    row = ctx->model.row+at;
    editor_free_row(row);
    memmove(ctx->model.row+at,ctx->model.row+at+1,sizeof(ctx->model.row[0])*(ctx->model.numrows-at-1));
    /* Rows after 'at' shifted up by one, so their idx decreases. */
    for (int j = at; j < ctx->model.numrows-1; j++) ctx->model.row[j].idx--;
    ctx->model.numrows--;
    ctx->model.dirty++;
}

/* Turn the editor rows into a single heap-allocated string.
 * Returns the pointer to the heap-allocated string and populate the
 * integer pointed by 'buflen' with the size of the string, excluding
 * the final nulterm. */
char *editor_rows_to_string(editor_ctx_t *ctx, int *buflen) {
    char *buf = NULL, *p;
    int totlen = 0;
    int j;

    /* Compute count of bytes */
    for (j = 0; j < ctx->model.numrows; j++)
        totlen += ctx->model.row[j].size+1; /* +1 is for "\n" at end of every row */
    *buflen = totlen;
    totlen++; /* Also make space for nulterm */

    p = buf = malloc(totlen);
    if (buf == NULL) return NULL;
    for (j = 0; j < ctx->model.numrows; j++) {
        memcpy(p,ctx->model.row[j].chars,ctx->model.row[j].size);
        p += ctx->model.row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

/* Insert a character at the specified position in a row, moving the remaining
 * chars on the right if needed. */
void editor_row_insert_char(editor_ctx_t *ctx, t_erow *row, int at, int c) {
    if (!row || at < 0) return;
    char *new_chars;
    if (at > row->size) {
        /* Pad the string with spaces if the insert location is outside the
         * current length by more than a single character. */
        int padlen = at-row->size;
        /* In the next line +2 means: new char and null term. */
        new_chars = realloc(row->chars,row->size+padlen+2);
        if (new_chars == NULL) {
            perror("Out of memory");
            exit(1);
        }
        row->chars = new_chars;
        memset(row->chars+row->size,' ',padlen);
        row->chars[row->size+padlen+1] = '\0';
        row->size += padlen+1;
    } else {
        /* If we are in the middle of the string just make space for 1 new
         * char plus the (already existing) null term. */
        new_chars = realloc(row->chars,row->size+2);
        if (new_chars == NULL) {
            perror("Out of memory");
            exit(1);
        }
        row->chars = new_chars;
        memmove(row->chars+at+1,row->chars+at,row->size-at+1);
        row->size++;
    }
    row->chars[at] = c;
    editor_update_row(ctx, row);
    ctx->model.dirty++;
}

/* Append the string 's' at the end of a row */
void editor_row_append_string(editor_ctx_t *ctx, t_erow *row, char *s, size_t len) {
    char *new_chars = realloc(row->chars,row->size+len+1);
    if (new_chars == NULL) {
        perror("Out of memory");
        exit(1);
    }
    row->chars = new_chars;
    memcpy(row->chars+row->size,s,len);
    row->size += len;
    row->chars[row->size] = '\0';
    editor_update_row(ctx, row);
    ctx->model.dirty++;
}

/* Delete the character at offset 'at' from the specified row. */
void editor_row_del_char(editor_ctx_t *ctx, t_erow *row, int at) {
    if (!row || at < 0 || at >= row->size) return;
    /* Bytes at+1..size, which includes the null terminator */
    memmove(row->chars+at,row->chars+at+1,row->size-at);
    row->size--;
    editor_update_row(ctx, row);
    ctx->model.dirty++;
}

/* Insert the specified char at the current prompt position. */
void editor_insert_char(editor_ctx_t *ctx, int c) {
    int filerow = ctx->view.rowoff+ctx->view.cy;
    int filecol = ctx->view.coloff+ctx->view.cx;
    t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];

    /* If the row where the cursor is currently located does not exist in our
     * logical representaion of the file, add enough empty rows as needed. */
    if (!row) {
        while(ctx->model.numrows <= filerow)
            editor_insert_row(ctx, ctx->model.numrows,"",0);
    }
    row = &ctx->model.row[filerow];
    editor_row_insert_char(ctx, row,filecol,c);

    /* Record undo operation */
    undo_record_insert_char(ctx, filerow, filecol, c);

    if (ctx->view.cx == ctx->view.screencols-1)
        ctx->view.coloff++;
    else
        ctx->view.cx++;
    /* Note: dirty already incremented by editor_row_insert_char */

    /* Handle electric dedent for closing braces */
    indent_electric_char(ctx, c);
}

/* Inserting a newline is slightly complex as we have to handle inserting a
 * newline in the middle of a line, splitting the line as needed. */
void editor_insert_newline(editor_ctx_t *ctx) {
    int filerow = ctx->view.rowoff+ctx->view.cy;
    int filecol = ctx->view.coloff+ctx->view.cx;
    t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];

    if (!row) {
        if (filerow == ctx->model.numrows) {
            editor_insert_row(ctx, filerow,"",0);
            undo_record_insert_line(ctx, filerow, filecol, "", 0);
            goto fixcursor;
        }
        return;
    }
    /* If the cursor is over the current line size, we want to conceptually
     * think it's just over the last character. */
    if (filecol >= row->size) filecol = row->size;
    if (filecol == 0) {
        editor_insert_row(ctx, filerow,"",0);
        undo_record_insert_line(ctx, filerow, filecol, "", 0);
    } else {
        /* We are in the middle of a line. Split it between two rows. */
        char *split_content = row->chars + filecol;
        int split_length = row->size - filecol;
        editor_insert_row(ctx, filerow+1, split_content, split_length);
        undo_record_insert_line(ctx, filerow, filecol, split_content, split_length);
        row = &ctx->model.row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        editor_update_row(ctx, row);
    }
fixcursor:
    if (ctx->view.cy == ctx->view.screenrows-1) {
        ctx->view.rowoff++;
    } else {
        ctx->view.cy++;
    }
    ctx->view.cx = 0;
    ctx->view.coloff = 0;

    /* Apply auto-indentation */
    indent_apply(ctx);
}

/* Delete the char at the current prompt position. */
void editor_del_char(editor_ctx_t *ctx) {
    int filerow = ctx->view.rowoff+ctx->view.cy;
    int filecol = ctx->view.coloff+ctx->view.cx;
    t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];

    if (!row || (filecol == 0 && filerow == 0)) return;
    if (filecol == 0) {
        /* Handle the case of column 0, we need to move the current line
         * on the right of the previous one. */
        filecol = ctx->model.row[filerow-1].size;
        /* Record deleting the newline (merging lines) */
        undo_record_delete_line(ctx, filerow - 1, filecol, row->chars, row->size);
        editor_row_append_string(ctx, &ctx->model.row[filerow-1],row->chars,row->size);
        editor_del_row(ctx, filerow);
        row = NULL;
        if (ctx->view.cy == 0)
            ctx->view.rowoff--;
        else
            ctx->view.cy--;
        ctx->view.cx = filecol;
        if (ctx->view.cx >= ctx->view.screencols) {
            int shift = (ctx->view.cx-ctx->view.screencols)+1;
            ctx->view.cx -= shift;
            ctx->view.coloff += shift;
        }
    } else {
        /* Record deleting the character */
        char deleted_char = row->chars[filecol-1];
        undo_record_delete_char(ctx, filerow, filecol-1, deleted_char);
        editor_row_del_char(ctx, row,filecol-1);
        if (ctx->view.cx == 0 && ctx->view.coloff)
            ctx->view.coloff--;
        else
            ctx->view.cx--;
    }
    /* Note: the row was already re-rendered by editor_row_del_char (or
     * removed entirely by editor_del_row); updating it again here just
     * repeated the syntax pass. dirty is likewise already incremented. */
}

/* Load the specified program in the editor memory and returns 0 on success
 * or -1 on error. */
int editor_open(editor_ctx_t *ctx, const char *filename) {
    FILE *fp;

    ctx->model.dirty = 0;
    free(ctx->model.filename);
    size_t fnlen = strlen(filename)+1;
    ctx->model.filename = malloc(fnlen);
    if (ctx->model.filename == NULL) {
        perror("Out of memory");
        exit(1);
    }
    memcpy(ctx->model.filename,filename,fnlen);

    fp = fopen(filename,"r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Opening file");
            exit(1);
        }
        return -1;
    }

    /* Check if file appears to be binary by looking for null bytes in first 1KB */
    char probe[1024];
    size_t probe_len = fread(probe, 1, sizeof(probe), fp);
    for (size_t i = 0; i < probe_len; i++) {
        if (probe[i] == '\0') {
            fclose(fp);
            editor_set_status_msg(ctx, "Cannot open binary file");
            return -1;
        }
    }
    rewind(fp);  /* Go back to start of file to read normally */

    /* Replace the buffer contents rather than appending to them. */
    for (int j = 0; j < ctx->model.numrows; j++) {
        editor_free_row(&ctx->model.row[j]);
    }
    free(ctx->model.row);
    ctx->model.row = NULL;
    ctx->model.numrows = 0;
    /* Undo entries reference row indices in the old buffer. */
    undo_clear(ctx);

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line,&linecap,fp)) != -1) {
        while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            line[--linelen] = '\0';
        editor_insert_row(ctx, ctx->model.numrows,line,linelen);
    }
    free(line);
    fclose(fp);
    ctx->model.dirty = 0;

    return 0;
}

/* Save the current file on disk. Return 0 on success, -1 on error. */
int editor_save(editor_ctx_t *ctx) {
    int len;
    char *buf = editor_rows_to_string(ctx, &len);
    if (buf == NULL) {
        editor_set_status_msg(ctx, "Can't save! Out of memory");
        return -1;
    }

    /* Check if buffer has a filename */
    if (ctx->model.filename == NULL) {
        free(buf);
        editor_set_status_msg(ctx, "No file name (use :w <filename> to save)");
        return -1;
    }

    int fd = open(ctx->model.filename,O_RDWR|O_CREAT,0644);
    if (fd == -1) goto writeerr;

    /* Use truncate + a single write(2) call in order to make saving
     * a bit safer, under the limits of what we can do in a small editor. */
    if (ftruncate(fd,len) == -1) goto writeerr;
    if (write(fd,buf,len) != len) goto writeerr;

    close(fd);
    free(buf);
    ctx->model.dirty = 0;
    editor_set_status_msg(ctx, "%d bytes written on disk", len);
    return 0;

writeerr:
    free(buf);
    if (fd != -1) close(fd);
    editor_set_status_msg(ctx, "Can't save! I/O error: %s",strerror(errno));
    return -1;
}

/* ============================= Terminal update ============================ */

/* Screen buffer functions are now in loki_terminal.c */

/* Maximum render segments per row - handles syntax changes within a line */
#define MAX_RENDER_SEGMENTS 256

/* Build render segments from a row for the renderer interface.
 * Returns number of segments created, or 0 for empty row.
 * Caller provides segments array (must be at least MAX_RENDER_SEGMENTS). */
static int build_render_segments(editor_ctx_t *ctx, t_erow *row, int row_idx,
                                 int coloff, int max_cols,
                                 RenderSegment *segments) {
    if (!row || row->rsize <= coloff) return 0;

    int len = row->rsize - coloff;
    if (len > max_cols) len = max_cols;
    if (len <= 0) return 0;

    char *c = row->render + coloff;
    unsigned char *hl = row->hl + coloff;
    int seg_count = 0;

    /* Check if this row is the currently playing line (1-based) */
    int is_playing_row = (ctx->view.playing_line > 0 &&
                          ctx->view.playing_line == row_idx + 1);

    int seg_start = 0;
    int current_hl = hl[0];
    int current_selected = is_selected(ctx, row_idx, coloff);

    for (int j = 1; j <= len && seg_count < MAX_RENDER_SEGMENTS - 1; j++) {
        int next_hl = (j < len) ? hl[j] : -1;
        int next_selected = (j < len) ? is_selected(ctx, row_idx, coloff + j) : 0;

        /* End segment if highlight or selection changes */
        if (j == len || next_hl != current_hl || next_selected != current_selected) {
            segments[seg_count].text = c + seg_start;
            segments[seg_count].len = j - seg_start;
            segments[seg_count].hl_type = hl_const_to_type(current_hl);
            segments[seg_count].selected = current_selected;
            segments[seg_count].is_playing = is_playing_row;
            seg_count++;
            seg_start = j;
            current_hl = next_hl;
            current_selected = next_selected;
        }
    }

    /* If the cap was reached mid-line, emit the remaining text as one
     * segment rather than dropping it from the display. */
    if (seg_start < len && seg_count < MAX_RENDER_SEGMENTS) {
        segments[seg_count].text = c + seg_start;
        segments[seg_count].len = len - seg_start;
        segments[seg_count].hl_type = hl_const_to_type(current_hl);
        segments[seg_count].selected = current_selected;
        segments[seg_count].is_playing = is_playing_row;
        seg_count++;
    }

    return seg_count;
}

/* Build render segments using explicit view (for split pane rendering).
 * This version uses is_selected_view for independent selection state per pane. */
static int build_render_segments_view(const EditorView *view, t_erow *row,
                                      int row_idx, int coloff, int max_cols,
                                      RenderSegment *segments) {
    if (!row || row->rsize <= coloff) return 0;

    int len = row->rsize - coloff;
    if (len > max_cols) len = max_cols;
    if (len <= 0) return 0;

    char *c = row->render + coloff;
    unsigned char *hl = row->hl + coloff;
    int seg_count = 0;

    /* Check if this row is the currently playing line (1-based) */
    int is_playing_row = (view && view->playing_line > 0 &&
                          view->playing_line == row_idx + 1);

    int seg_start = 0;
    int current_hl = hl[0];
    int current_selected = is_selected_view(view, row_idx, coloff);

    for (int j = 1; j <= len && seg_count < MAX_RENDER_SEGMENTS - 1; j++) {
        int next_hl = (j < len) ? hl[j] : -1;
        int next_selected = (j < len) ? is_selected_view(view, row_idx, coloff + j) : 0;

        /* End segment if highlight or selection changes */
        if (j == len || next_hl != current_hl || next_selected != current_selected) {
            segments[seg_count].text = c + seg_start;
            segments[seg_count].len = j - seg_start;
            segments[seg_count].hl_type = hl_const_to_type(current_hl);
            segments[seg_count].selected = current_selected;
            segments[seg_count].is_playing = is_playing_row;
            seg_count++;
            seg_start = j;
            current_hl = next_hl;
            current_selected = next_selected;
        }
    }

    /* If the cap was reached mid-line, emit the remaining text as one
     * segment rather than dropping it from the display. */
    if (seg_start < len && seg_count < MAX_RENDER_SEGMENTS) {
        segments[seg_count].text = c + seg_start;
        segments[seg_count].len = len - seg_start;
        segments[seg_count].hl_type = hl_const_to_type(current_hl);
        segments[seg_count].selected = current_selected;
        segments[seg_count].is_playing = is_playing_row;
        seg_count++;
    }

    return seg_count;
}

/* ======================= Split Window Rendering =========================== */

/* Render a single pane's content at its geometry.
 * This renders the editor content within the pane's bounds.
 * Uses the pane's own view state and buffer (model_id) for rendering. */
static void render_pane_content(editor_ctx_t *ctx, Renderer *r, WindowNode *pane,
                                int is_active) {
    /* Use pane's view if available, otherwise fall back to ctx->view */
    EditorView *view = pane->view ? pane->view : &ctx->view;

    /* Get the buffer for this pane's model_id */
    editor_ctx_t *buffer_ctx = buffer_get(pane->model_id);
    EditorModel *model = buffer_ctx ? &buffer_ctx->model : &ctx->model;

    /* Calculate gutter width for this pane */
    int gutter_width = 0;
    if (view->line_numbers && model->numrows > 0) {
        int max_line = model->numrows;
        gutter_width = 1;
        while (max_line >= 10) {
            gutter_width++;
            max_line /= 10;
        }
        gutter_width += 1; /* Space separator */
    }

    int text_cols = pane->width - gutter_width;
    if (text_cols < 1) text_cols = 1;

    /* Pane geometry: 0-based internal, but renderer uses 1-based */
    int pane_x = pane->x + 1;  /* Convert to 1-based */
    int pane_y = pane->y + 1;

    /* Use pane's view scroll offset */
    int rowoff = view->rowoff;
    int coloff = view->coloff;

    /* Render rows within this pane */
    RenderSegment segments[MAX_RENDER_SEGMENTS];
    for (int y = 0; y < pane->height; y++) {
        int filerow = rowoff + y;
        int is_empty = (filerow >= model->numrows);
        int screen_row = pane_y + y;

        if (is_empty) {
            r->render_row_at(r, screen_row, pane_x, pane->width,
                             0, NULL, 0, gutter_width, 1, is_active);
        } else {
            t_erow *row = &model->row[filerow];
            int seg_count = build_render_segments_view(view, row, filerow,
                                                       coloff, text_cols, segments);
            r->render_row_at(r, screen_row, pane_x, pane->width,
                             filerow + 1, segments, seg_count, gutter_width, 0,
                             is_active);
        }
    }
}

/* Check if the active pane is in a subtree */
static int subtree_contains_active(WindowNode *node, WindowNode *active) {
    if (!node || !active) return 0;
    if (node == active) return 1;
    if (node->split == SPLIT_NONE) return 0;
    return subtree_contains_active(node->first, active) ||
           subtree_contains_active(node->second, active);
}

/* Recursively render the window tree, including separators */
static void render_window_tree(editor_ctx_t *ctx, Renderer *r, WindowNode *node) {
    if (!node) return;

    if (node->split == SPLIT_NONE) {
        /* Leaf node: render pane content */
        int is_active = (ctx->wm && ctx->wm->active == node);
        render_pane_content(ctx, r, node, is_active);
    } else {
        /* Container node: render children and separator */
        render_window_tree(ctx, r, node->first);
        render_window_tree(ctx, r, node->second);

        /* Determine which side of the separator has the active pane */
        WindowNode *active = ctx->wm ? ctx->wm->active : NULL;
        int active_side = 0;
        if (subtree_contains_active(node->first, active)) {
            active_side = 1;  /* First child (left/top) */
        } else if (subtree_contains_active(node->second, active)) {
            active_side = 2;  /* Second child (right/bottom) */
        }

        /* Render separator between children */
        if (node->split == SPLIT_VERTICAL) {
            /* Vertical split: separator is a vertical line between left and right */
            int sep_x = node->first->x + node->first->width + 1;  /* 1-based */
            int sep_y = node->y + 1;
            r->render_separator(r, sep_x, sep_y, node->height, 1, active_side);
        } else {
            /* Horizontal split: separator is a horizontal line between top and bottom */
            int sep_x = node->x + 1;
            int sep_y = node->first->y + node->first->height + 1;
            r->render_separator(r, sep_x, sep_y, node->width, 0, active_side);
        }
    }
}

/* Refresh screen using renderer interface.
 * This is the abstract rendering path that doesn't emit VT100 directly. */
static void editor_refresh_screen_via_renderer(editor_ctx_t *ctx) {
    Renderer *r = ctx->renderer;

    /* Handle picker mode separately */
    if (ctx->view.mode == MODE_PICKER && r->render_picker) {
        r->begin_frame(r, ctx->view.screencols, ctx->view.screenrows);
        PickerInfo picker_info = {
            .title = ctx->view.picker.title,
            .items = ctx->view.picker.items,
            .item_count = ctx->view.picker.item_count,
            .selected_index = ctx->view.picker.selected_index,
            .scroll_offset = ctx->view.picker.scroll_offset,
            .visible_rows = ctx->view.screenrows - 3,
        };
        r->render_picker(r, &picker_info, ctx->view.screencols, ctx->view.screenrows);
        r->hide_cursor(r);
        r->end_frame(r);
        return;
    }

    int tabs_showing = (buffer_count() > 1) ? 1 : 0;
    int available_rows = ctx->view.screenrows - tabs_showing;

    /* Calculate gutter width for line numbers */
    int gutter_width = 0;
    if (ctx->view.line_numbers && ctx->model.numrows > 0) {
        int max_line = ctx->model.numrows;
        gutter_width = 1;
        while (max_line >= 10) {
            gutter_width++;
            max_line /= 10;
        }
        gutter_width += 1; /* Space separator */
    }

    int text_cols = ctx->view.screencols - gutter_width;
    if (text_cols < 1) text_cols = 1;

    /* Begin frame */
    r->begin_frame(r, ctx->view.screencols, ctx->view.screenrows);

    /* Render buffer tabs if multiple buffers */
    if (tabs_showing) {
        char **tabs = NULL;
        int tab_count = 0, active_tab = 0;
        if (buffers_get_tab_info(&tabs, &tab_count, &active_tab) == 0 && tab_count > 0) {
            r->render_tabs(r, (const char **)tabs, tab_count, active_tab, ctx->view.screencols);
            buffers_free_tab_info(tabs, tab_count);
        }
    }

    /* Check if we have split windows */
    int has_splits = (ctx->wm && window_count_panes(ctx->wm) > 1);

    if (has_splits) {
        /* Split window mode: compute layout and render window tree */
        int content_start_y = tabs_showing;  /* 0 if no tabs, 1 if tabs */
        int content_height = available_rows;

        /* Recompute layout with current screen dimensions
         * Account for status bar rows at bottom */
        window_layout(ctx->wm, 0, content_start_y,
                      ctx->view.screencols, content_height);

        /* Render all panes and separators */
        render_window_tree(ctx, r, ctx->wm->root);
    } else {
        /* Single pane mode: use traditional row-by-row rendering */
        RenderSegment segments[MAX_RENDER_SEGMENTS];
        for (int y = 0; y < available_rows; y++) {
            int filerow = ctx->view.rowoff + y;
            int is_empty = (filerow >= ctx->model.numrows);

            if (is_empty) {
                r->render_row(r, 0, NULL, 0, gutter_width, 1);
            } else {
                t_erow *row = &ctx->model.row[filerow];
                int seg_count = build_render_segments(ctx, row, filerow,
                                                      ctx->view.coloff, text_cols, segments);
                r->render_row(r, filerow + 1, segments, seg_count, gutter_width, 0);
            }
        }
    }

    /* Build status info */
    const char *mode_str = "";
    int link_active = loki_link_is_initialized(ctx) && loki_link_is_enabled(ctx);
    switch(ctx->view.mode) {
        case MODE_NORMAL: mode_str = link_active ? "LINK" : "NORMAL"; break;
        case MODE_INSERT: mode_str = "INSERT"; break;
        case MODE_VISUAL: mode_str = "VISUAL"; break;
        case MODE_COMMAND: mode_str = "COMMAND"; break;
        case MODE_PICKER: mode_str = "PICKER"; break;
    }

    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    static char lang_buf[16] = "";
    if (lang && lang->is_initialized && lang->is_initialized(ctx)) {
        snprintf(lang_buf, sizeof(lang_buf), "%s ", lang->name);
        for (int i = 0; lang_buf[i] && i < 15; i++) {
            if (lang_buf[i] >= 'a' && lang_buf[i] <= 'z') {
                lang_buf[i] -= 32;
            }
        }
    } else {
        lang_buf[0] = '\0';
    }

    /* Get playback timing info from Link */
    double tempo = 0.0;
    double beat = 0.0;
    int bar = 0;
    int beat_in_bar = 0;
    int link_peers = 0;

    if (link_active) {
        tempo = loki_link_get_tempo(ctx);
        beat = loki_link_get_beat(ctx, 4.0);  /* 4/4 time */
        double phase = loki_link_get_phase(ctx, 4.0);
        bar = (int)(beat / 4.0) + 1;
        beat_in_bar = (int)phase + 1;
        link_peers = (int)loki_link_num_peers(ctx);
    }

    /* Check if transport sync is armed (enabled but not playing) */
    int transport_armed = link_active &&
                          loki_link_is_transport_sync_enabled(ctx) &&
                          !loki_lang_is_playing(ctx);

    StatusInfo status_info = {
        .mode = mode_str,
        .filename = ctx->model.filename,
        .lang = lang_buf,
        .numrows = ctx->model.numrows,
        .current_row = ctx->view.rowoff + ctx->view.cy + 1,
        .dirty = ctx->model.dirty,
        .playing = loki_lang_is_playing(ctx),
        .link_active = link_active,
        .metronome_active = metronome_is_enabled(),
        .transport_armed = transport_armed,
        .tempo = tempo,
        .beat = beat,
        .bar = bar,
        .beat_in_bar = beat_in_bar,
        .link_peers = link_peers,
    };
    r->render_status(r, &status_info, ctx->view.screencols);

    /* Render message line */
    const char *msg = NULL;
    if (ctx->view.statusmsg[0] && time(NULL) - ctx->view.statusmsg_time < 5) {
        msg = ctx->view.statusmsg;
    }
    r->render_message(r, msg, ctx->view.screencols);

    /* Render REPL if active */
    t_lua_repl *repl = ctx_repl(ctx);
    if (repl && repl->active) {
        ReplInfo repl_info = {
            .prompt = LUA_REPL_PROMPT,
            .input = repl->input,
            .input_len = repl->input_len,
            .log_lines = (const char **)repl->log,
            .log_count = repl->log_len,
            .max_display_lines = LUA_REPL_OUTPUT_ROWS,
        };
        r->render_repl(r, &repl_info, ctx->view.screencols);
    }

    /* Calculate and set cursor position */
    int cursor_row = 1, cursor_col = 1;
    if (repl && repl->active) {
        int prompt_len = (int)strlen(LUA_REPL_PROMPT);
        int visible = repl->input_len;
        if (prompt_len + visible >= ctx->view.screencols) {
            visible = ctx->view.screencols > prompt_len ? ctx->view.screencols - prompt_len : 0;
        }
        cursor_row = ctx->view.screenrows + STATUS_ROWS + LUA_REPL_OUTPUT_ROWS + 1;
        cursor_col = prompt_len + visible + 1;
        if (cursor_col < 1) cursor_col = 1;
        if (cursor_col > ctx->view.screencols) cursor_col = ctx->view.screencols;
    } else {
        /* Calculate cursor x position within content */
        int cx = 1;
        int filerow = ctx->view.rowoff + ctx->view.cy;
        t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];
        if (row) {
            for (int j = ctx->view.coloff; j < (ctx->view.cx + ctx->view.coloff); j++) {
                if (j < row->size && row->chars[j] == TAB)
                    cx += 7 - ((cx) % 8);
                cx++;
            }
        }

        /* Add gutter width */
        if (ctx->view.line_numbers && ctx->model.numrows > 0) {
            int gw = 1, max_ln = ctx->model.numrows;
            while (max_ln >= 10) { gw++; max_ln /= 10; }
            gw += 1;
            cx += gw;
        }

        int tab_offset = tabs_showing ? 1 : 0;

        if (has_splits && ctx->wm && ctx->wm->active) {
            /* Split window mode: position cursor within active pane */
            WindowNode *active_pane = ctx->wm->active;
            cursor_row = active_pane->y + ctx->view.cy + 1;  /* +1 for 1-based */
            cursor_col = active_pane->x + cx;
            /* Clamp to pane bounds */
            if (cursor_col > active_pane->x + active_pane->width) {
                cursor_col = active_pane->x + active_pane->width;
            }
            if (cursor_row > active_pane->y + active_pane->height) {
                cursor_row = active_pane->y + active_pane->height;
            }
        } else {
            /* Single pane mode */
            cursor_row = ctx->view.cy + 1 + tab_offset;
            cursor_col = cx;
            if (cursor_col > ctx->view.screencols) cursor_col = ctx->view.screencols;
        }
    }

    r->set_cursor(r, cursor_row, cursor_col);

    /* End frame */
    r->end_frame(r);
}

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editor_refresh_screen(editor_ctx_t *ctx) {
    /* Use renderer if available */
    if (ctx->renderer) {
        editor_refresh_screen_via_renderer(ctx);
        return;
    }
    int y;
    t_erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab,"\x1b[?25l",6); /* Hide cursor. */
    terminal_buffer_append(&ab,"\x1b[H",3); /* Go home. */

    /* Render buffer tabs at top if multiple buffers open */
    int tabs_showing = (buffer_count() > 1) ? 1 : 0;
    buffers_render_tabs(&ab, ctx->view.screencols);

    /* Reduce available rows if tabs are showing */
    int available_rows = ctx->view.screenrows - tabs_showing;

    /* Calculate gutter width for line numbers */
    int gutter_width = 0;
    if (ctx->view.line_numbers && ctx->model.numrows > 0) {
        /* Width = digits needed for max line number + 1 for separator */
        int max_line = ctx->model.numrows;
        gutter_width = 1; /* At least 1 digit */
        while (max_line >= 10) {
            gutter_width++;
            max_line /= 10;
        }
        gutter_width += 1; /* Space separator after number */
    }

    /* Available cols for text after gutter */
    int text_cols = ctx->view.screencols - gutter_width;
    if (text_cols < 1) text_cols = 1;

    for (y = 0; y < available_rows; y++) {
        int filerow = ctx->view.rowoff+y;

        if (filerow >= ctx->model.numrows) {
            if (ctx->model.numrows == 0 && y == available_rows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                    "Loki editor -- version %s\x1b[0K\r\n", PSND_VERSION);
                int padding = (ctx->view.screencols-welcomelen)/2;
                if (padding > 0) {
                    terminal_buffer_append(&ab,"~",1);
                    padding--;
                }
                while(padding-- > 0) terminal_buffer_append(&ab," ",1);
                terminal_buffer_append(&ab,welcome,welcomelen);
            } else {
                /* Empty lines: show gutter filler if line numbers enabled */
                if (ctx->view.line_numbers && gutter_width > 0) {
                    terminal_buffer_append(&ab,"\x1b[90m",5); /* Dark gray */
                    for (int i = 0; i < gutter_width - 1; i++)
                        terminal_buffer_append(&ab," ",1);
                    terminal_buffer_append(&ab,"~",1);
                    terminal_buffer_append(&ab,"\x1b[39m",5); /* Reset */
                } else {
                    terminal_buffer_append(&ab,"~",1);
                }
                terminal_buffer_append(&ab,"\x1b[0K\r\n",6);
            }
            continue;
        }

        /* Render line number gutter */
        if (ctx->view.line_numbers && gutter_width > 0) {
            char line_num_buf[16];
            int line_num_len = snprintf(line_num_buf, sizeof(line_num_buf),
                "%*d ", gutter_width - 1, filerow + 1);
            terminal_buffer_append(&ab,"\x1b[90m",5); /* Dark gray for line numbers */
            terminal_buffer_append(&ab, line_num_buf, line_num_len);
            terminal_buffer_append(&ab,"\x1b[39m",5); /* Reset foreground */
        }

        r = &ctx->model.row[filerow];

        int len = r->rsize - ctx->view.coloff;
        int current_color = -1;

        /* Word wrap: clamp to screen width and find word boundary */
        if (ctx->view.word_wrap && len > text_cols && r->cb_lang == CB_LANG_NONE) {
            len = text_cols;
            /* Find last space/separator to break at word boundary */
            int last_space = -1;
            for (int k = 0; k < len; k++) {
                if (isspace(r->render[ctx->view.coloff + k])) {
                    last_space = k;
                }
            }
            if (last_space > 0 && last_space > len / 2) {
                len = last_space + 1; /* Include the space */
            }
        }

        if (len > 0) {
            if (len > text_cols) len = text_cols;
            char *c = r->render+ctx->view.coloff;
            unsigned char *hl = r->hl+ctx->view.coloff;
            int j;
            for (j = 0; j < len; j++) {
                int selected = is_selected(ctx, filerow, ctx->view.coloff + j);

                /* Apply selection background */
                if (selected) {
                    terminal_buffer_append(&ab,"\x1b[7m",4); /* Reverse video */
                }

                if (hl[j] == HL_NONPRINT) {
                    char sym;
                    if (!selected) terminal_buffer_append(&ab,"\x1b[7m",4);
                    if (c[j] <= 26)
                        sym = '@'+c[j];
                    else
                        sym = '?';
                    terminal_buffer_append(&ab,&sym,1);
                    terminal_buffer_append(&ab,"\x1b[0m",4);
                    if (current_color != -1) {
                        char buf[32];
                        int clen = syntax_format_color(ctx, current_color, buf, sizeof(buf));
                        terminal_buffer_append(&ab,buf,clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        terminal_buffer_append(&ab,"\x1b[39m",5);
                        current_color = -1;
                    }
                    terminal_buffer_append(&ab,c+j,1);
                    if (selected) {
                        terminal_buffer_append(&ab,"\x1b[0m",4); /* Reset */
                    }
                } else {
                    int color = hl[j];
                    if (color != current_color) {
                        char buf[32];
                        int clen = syntax_format_color(ctx, color, buf, sizeof(buf));
                        current_color = color;
                        terminal_buffer_append(&ab,buf,clen);
                    }
                    terminal_buffer_append(&ab,c+j,1);
                    if (selected) {
                        terminal_buffer_append(&ab,"\x1b[0m",4); /* Reset */
                        if (current_color != -1) {
                            char buf[32];
                            int clen = syntax_format_color(ctx, current_color, buf, sizeof(buf));
                            terminal_buffer_append(&ab,buf,clen);
                        }
                    }
                }
            }
        }
        terminal_buffer_append(&ab,"\x1b[39m",5);
        terminal_buffer_append(&ab,"\x1b[0K",4);
        terminal_buffer_append(&ab,"\r\n",2);
    }

    /* Create a two rows status. First row: */
    terminal_buffer_append(&ab,"\x1b[0K",4);
    terminal_buffer_append(&ab,"\x1b[7m",4);
    char status[80], rstatus[80];

    /* Get mode indicator */
    const char *mode_str = "";
    int link_active = loki_link_is_initialized(ctx) && loki_link_is_enabled(ctx);

    switch(ctx->view.mode) {
        case MODE_NORMAL: mode_str = link_active ? "LINK" : "NORMAL"; break;
        case MODE_INSERT: mode_str = "INSERT"; break;
        case MODE_VISUAL: mode_str = "VISUAL"; break;
        case MODE_COMMAND: mode_str = "COMMAND"; break;
        case MODE_PICKER: mode_str = "PICKER"; break;
    }

    /* Show language indicator if a language is active for this file */
    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    const char *lang_str = "";
    char lang_buf[16] = "";
    if (lang && lang->is_initialized && lang->is_initialized(ctx)) {
        snprintf(lang_buf, sizeof(lang_buf), "%s ", lang->name);
        /* Uppercase the language name */
        for (int i = 0; lang_buf[i] && i < 15; i++) {
            if (lang_buf[i] >= 'a' && lang_buf[i] <= 'z') {
                lang_buf[i] -= 32;
            }
        }
        lang_str = lang_buf;
    }

    int len = snprintf(status, sizeof(status), " %s%s  %.20s - %d lines %s",
        lang_str, mode_str,
        ctx->model.filename ? ctx->model.filename : "[No Name]",
        ctx->model.numrows, ctx->model.dirty ? "(modified)" : "");

    /* Show playing indicator if any language is playing */
    const char *playing = loki_lang_is_playing(ctx) ? "[PLAYING] " : "";
    int rlen = snprintf(rstatus, sizeof(rstatus),
        "%s%d/%d", playing, ctx->view.rowoff+ctx->view.cy+1, ctx->model.numrows);
    if (len > ctx->view.screencols) len = ctx->view.screencols;
    terminal_buffer_append(&ab,status,len);
    while(len < ctx->view.screencols) {
        if (ctx->view.screencols - len == rlen) {
            terminal_buffer_append(&ab,rstatus,rlen);
            break;
        } else {
            terminal_buffer_append(&ab," ",1);
            len++;
        }
    }
    terminal_buffer_append(&ab,"\x1b[0m\r\n",6);

    /* Render buffer tabs if multiple buffers are open */
    /* Note: buffer tabs are rendered in a separate line above the status bar */
    /* This is done by the buffers module */

    /* Second row depends on ctx->view.statusmsg and the status message update time. */
    terminal_buffer_append(&ab,"\x1b[0K",4);
    int msglen = strlen(ctx->view.statusmsg);
    if (msglen && time(NULL)-ctx->view.statusmsg_time < 5)
        terminal_buffer_append(&ab,ctx->view.statusmsg,msglen <= ctx->view.screencols ? msglen : ctx->view.screencols);

    /* Render REPL if active */
    t_lua_repl *repl = ctx_repl(ctx);
    if (repl && repl->active) lua_repl_render(ctx, &ab);

    /* Put cursor at its current position. Note that the horizontal position
     * at which the cursor is displayed may be different compared to 'ctx->view.cx'
     * because of TABs. */
    int cursor_row = 1;
    int cursor_col = 1;

    /* Calculate cursor position - different for REPL vs editor mode */
    if (repl && repl->active) {
        /* REPL mode: cursor is on the REPL prompt line */
        int prompt_len = (int)strlen(LUA_REPL_PROMPT);
        int visible = repl->input_len;
        if (prompt_len + visible >= ctx->view.screencols) {
            visible = ctx->view.screencols > prompt_len ? ctx->view.screencols - prompt_len : 0;
        }
        cursor_row = ctx->view.screenrows + STATUS_ROWS + LUA_REPL_OUTPUT_ROWS + 1;
        cursor_col = prompt_len + visible + 1;
        if (cursor_col < 1) cursor_col = 1;
        if (cursor_col > ctx->view.screencols) cursor_col = ctx->view.screencols;
    } else {
        /* Editor mode: cursor is in the text area */
        int cx = 1;
        int filerow = ctx->view.rowoff+ctx->view.cy;
        t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];
        if (row) {
            for (int j = ctx->view.coloff; j < (ctx->view.cx+ctx->view.coloff); j++) {
                if (j < row->size && row->chars[j] == TAB)
                    cx += 7-((cx)%8);
                cx++;
            }
        }
        /* Account for line numbers gutter */
        if (ctx->view.line_numbers && ctx->model.numrows > 0) {
            int gw = 1;
            int max_ln = ctx->model.numrows;
            while (max_ln >= 10) { gw++; max_ln /= 10; }
            gw += 1; /* Space separator */
            cx += gw;
        }
        /* Account for tab bar at top if multiple buffers are open */
        int tab_offset = (buffer_count() > 1) ? 1 : 0;
        cursor_row = ctx->view.cy + 1 + tab_offset;
        cursor_col = cx;
        if (cursor_col > ctx->view.screencols) cursor_col = ctx->view.screencols;
    }

    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",cursor_row,cursor_col);
    terminal_buffer_append(&ab,buf,strlen(buf));
    terminal_buffer_append(&ab,"\x1b[?25h",6); /* Show cursor. */
    write_ignore_result(STDOUT_FILENO,ab.b,ab.len);
    terminal_buffer_free(&ab);
}

/* REPL layout management, toggle function, and status reporter are in loki_editor.c */

/* Window size functions are now in loki_terminal.c */

/* ========================= Editor events handling  ======================== */

/* Handle cursor position change because arrow keys were pressed. */
void editor_move_cursor(editor_ctx_t *ctx, int key) {
    int filerow = ctx->view.rowoff+ctx->view.cy;
    int filecol = ctx->view.coloff+ctx->view.cx;
    int rowlen;
    t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];

    switch(key) {
    case ARROW_LEFT:
        if (ctx->view.cx == 0) {
            if (ctx->view.coloff) {
                ctx->view.coloff--;
            } else {
                if (filerow > 0) {
                    ctx->view.cy--;
                    ctx->view.cx = ctx->model.row[filerow-1].size;
                    if (ctx->view.cx > ctx->view.screencols-1) {
                        ctx->view.coloff = ctx->view.cx-ctx->view.screencols+1;
                        ctx->view.cx = ctx->view.screencols-1;
                    }
                }
            }
        } else {
            ctx->view.cx -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (row && filecol < row->size) {
            if (ctx->view.cx == ctx->view.screencols-1) {
                ctx->view.coloff++;
            } else {
                ctx->view.cx += 1;
            }
        } else if (row && filecol == row->size) {
            ctx->view.cx = 0;
            ctx->view.coloff = 0;
            if (ctx->view.cy == ctx->view.screenrows-1) {
                ctx->view.rowoff++;
            } else {
                ctx->view.cy += 1;
            }
        }
        break;
    case ARROW_UP:
        if (ctx->view.cy == 0) {
            if (ctx->view.rowoff) ctx->view.rowoff--;
        } else {
            ctx->view.cy -= 1;
        }
        break;
    case ARROW_DOWN:
        if (filerow < ctx->model.numrows) {
            if (ctx->view.cy == ctx->view.screenrows-1) {
                ctx->view.rowoff++;
            } else {
                ctx->view.cy += 1;
            }
        }
        break;
    }
    /* Fix cx if the current line has not enough chars. */
    filerow = ctx->view.rowoff+ctx->view.cy;
    filecol = ctx->view.coloff+ctx->view.cx;
    row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        ctx->view.cx -= filecol-rowlen;
        if (ctx->view.cx < 0) {
            ctx->view.coloff += ctx->view.cx;
            ctx->view.cx = 0;
        }
    }
}

/* ========================= Modal Key Processing ============================ */

/* Process a single keypress - delegates to modal editing module */
int editor_process_keypress(editor_ctx_t *ctx, int fd) {
    /* All modal editing logic is now in loki_modal.c */
    return modal_process_keypress(ctx, fd);
}

void init_editor(editor_ctx_t *ctx) {
    /* Thin wrapper over editor_ctx_init(), which is the single complete
     * initialiser (undo state, indent config, command state, colors). This
     * used to be a partial second contract that left those fields untouched,
     * so it was only safe on an already-zeroed struct. */
    editor_ctx_init(ctx);

    ctx->view.word_wrap = 1;  /* Word wrap on by default in the terminal editor */
    syntax_init_default_colors(ctx);
    terminal_update_window_size(ctx);
    /* Note: SIGWINCH registration now happens in terminal_host_init() */
    /* Note: editor_for_atexit is set explicitly after buffers_init() */
}

/* Main editor entry point moved to loki_editor.c */
