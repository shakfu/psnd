/* POSIX terminal implementation
 *
 * Platform-specific terminal operations for POSIX systems (macOS, Linux).
 * Uses termios for raw mode, ioctl for window size, and VT100
 * escape sequences for advanced features.
 */

#ifndef _WIN32

#include "terminal.h"
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ======================= Terminal Host State =============================== */

/* Static terminal host instance */
static TerminalHost g_terminal_host_instance = {0};

/* Global pointer for signal handler access */
TerminalHost *g_terminal_host = &g_terminal_host_instance;

/* ======================= Terminal Host Implementation ===================== */

int terminal_host_init(TerminalHost *host, int fd) {
    if (!host) return -1;

    memset(host, 0, sizeof(*host));
    host->fd = fd;
    host->rawmode = 0;
    host->winsize_changed = 0;

    /* Set global pointer for signal handler */
    g_terminal_host = host;

    /* Register SIGWINCH handler */
    signal(SIGWINCH, terminal_sig_winch_handler);

    return 0;
}

void terminal_host_cleanup(TerminalHost *host) {
    if (!host) return;

    /* Disable raw mode if active */
    terminal_host_disable_raw_mode(host);

    /* Clear global pointer */
    if (g_terminal_host == host) {
        g_terminal_host = NULL;
    }
}

int terminal_host_enable_raw_mode(TerminalHost *host) {
    struct termios raw;

    if (!host) return -1;
    if (host->rawmode) return 0;  /* Already enabled */

    if (!isatty(host->fd)) {
        errno = ENOTTY;
        return -1;
    }

    if (tcgetattr(host->fd, &host->orig_termios) == -1) {
        errno = ENOTTY;
        return -1;
    }

    raw = host->orig_termios;
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - echo off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 0;  /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    if (tcsetattr(host->fd, TCSAFLUSH, &raw) < 0) {
        errno = ENOTTY;
        return -1;
    }

    host->rawmode = 1;

    /* Enter alternate screen buffer (saves current screen, gives clean slate)
     * Only if stdout is a terminal (not a pipe or file) */
    if (isatty(STDOUT_FILENO)) {
        write_ignore_result(STDOUT_FILENO, "\x1b[?1049h", 8);
    }

    return 0;
}

void terminal_host_disable_raw_mode(TerminalHost *host) {
    if (!host || !host->rawmode) return;

    /* Exit alternate screen buffer (restores original terminal content)
     * Only if stdout is a terminal (not a pipe or file) */
    if (isatty(STDOUT_FILENO)) {
        write_ignore_result(STDOUT_FILENO, "\x1b[?1049l", 8);
    }

    tcsetattr(host->fd, TCSAFLUSH, &host->orig_termios);
    host->rawmode = 0;
}

int terminal_host_resize_pending(TerminalHost *host) {
    return host && host->winsize_changed;
}

void terminal_host_clear_resize(TerminalHost *host) {
    if (host) host->winsize_changed = 0;
}

/* ======================= Input Reading ==================================== */

/* Read one byte of an escape sequence. Returns 1 on success, 0 on timeout
 * (VTIME expiry, i.e. the sequence ended), -1 on error. Retries on EINTR so
 * a signal mid-sequence does not leave seq[] uninitialized. */
static int read_seq_byte(int fd, char *out) {
    for (;;) {
        int n = read(fd, out, 1);
        if (n >= 0) return n;
        if (errno == EINTR) continue;
        return -1;
    }
}

/* After a 0-byte read: is the input gone (hangup, closed pipe) rather than
 * idle? An idle raw-mode tty returns 0 on every VTIME timeout; a hung-up one
 * returns 0 at once, forever, and reports POLLHUP. */
static int input_closed(int fd) {
    struct pollfd p = { .fd = fd, .events = POLLIN, .revents = 0 };
    return poll(&p, 1, 0) > 0 && (p.revents & (POLLHUP | POLLERR | POLLNVAL));
}

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. Returns -1 on a read error or closed input. */
int terminal_read_key(int fd) {
    int nread;
    char c, seq[6];
    /* Raw mode sets VMIN=0/VTIME=1, so read() returns 0 on every 100 ms
     * timeout. That is the normal idle case on a healthy terminal, not a
     * closed stdin -- just keep waiting. Retry on EINTR. */
    while ((nread = read(fd, &c, 1)) != 1) {
        if (nread == 0) {
            if (input_closed(fd)) return -1;
            continue;
        }
        if (errno == EINTR || errno == EAGAIN) continue;
        return -1;
    }

    switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read_seq_byte(fd, seq) != 1) return ESC;
            if (read_seq_byte(fd, seq+1) != 1) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read_seq_byte(fd, seq+2) != 1) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    } else if (seq[2] == ';') {
                        /* ESC[1;2X for Shift+Arrow */
                        if (read_seq_byte(fd, seq+3) != 1) return ESC;
                        if (read_seq_byte(fd, seq+4) != 1) return ESC;
                        if (seq[1] == '1' && seq[3] == '2') {
                            switch(seq[4]) {
                            case 'A': return SHIFT_ARROW_UP;
                            case 'B': return SHIFT_ARROW_DOWN;
                            case 'C': return SHIFT_ARROW_RIGHT;
                            case 'D': return SHIFT_ARROW_LEFT;
                            }
                        }
                    } else if (seq[1] == '1' && seq[2] == '3') {
                        /* ESC[13;2u - Shift+Return (kitty keyboard protocol) */
                        if (read_seq_byte(fd, seq+3) != 1) return ESC;
                        if (seq[3] == ';') {
                            if (read_seq_byte(fd, seq+4) != 1) return ESC;
                            if (read_seq_byte(fd, seq+5) != 1) return ESC;
                            if (seq[4] == '2' && seq[5] == 'u') {
                                return SHIFT_RETURN;
                            }
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }

            /* Unrecognized sequence. Report a bare ESC rather than looping
             * with c == ESC, which used to swallow the next two keystrokes. */
            return ESC;
        default:
            return c;
    }
}

/* ======================= Window Size Detection ============================ */

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor is stored at *rows and *cols and 0 is returned. */
int terminal_get_cursor_position(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd, buf+i, 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2, "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
int terminal_get_window_size(int ifd, int ofd, int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(ofd, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int orig_row, orig_col, retval;

        /* Get the initial position so we can restore it later. */
        retval = terminal_get_cursor_position(ifd, ofd, &orig_row, &orig_col);
        if (retval == -1) goto failed;

        /* Go to right/bottom margin and get position. */
        if (write(ofd, "\x1b[999C\x1b[999B", 12) != 12) goto failed;
        retval = terminal_get_cursor_position(ifd, ofd, rows, cols);
        if (retval == -1) goto failed;

        /* Restore position. */
        char seq[32];
        snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
        if (write(ofd, seq, strlen(seq)) == -1) {
            /* Can't recover... */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
}

/* Update window size and adjust screen layout */
void terminal_update_window_size(editor_ctx_t *ctx) {
    int rows, cols;
    if (terminal_get_window_size(STDIN_FILENO, STDOUT_FILENO,
                      &rows, &cols) == -1) {
        /* If we can't get terminal size (e.g., non-interactive mode), use defaults */
        rows = 24;
        cols = 80;
    }
    ctx->view.screencols = cols;
    rows -= STATUS_ROWS;
    if (rows < 1) rows = 1;
    ctx->view.screenrows_total = rows;
    /* REPL layout update (editor_update_repl_layout) is in loki_editor.c */
    ctx->view.screenrows = ctx->view.screenrows_total; /* Without REPL, use all available rows */
}

/* ======================= Signal Handling ================================== */

/* Signal handler for window size changes */
void terminal_sig_winch_handler(int unused __attribute__((unused))) {
    /* Signal handler must be async-signal-safe.
     * Just set a flag in the terminal host for main loop to handle. */
    if (g_terminal_host) {
        g_terminal_host->winsize_changed = 1;
    }
}

/* Check and handle window resize */
void terminal_handle_resize(editor_ctx_t *ctx) {
    if (!ctx) return;

    /* Check terminal host for resize flag (set by signal handler) */
    if (terminal_host_resize_pending(g_terminal_host)) {
        terminal_host_clear_resize(g_terminal_host);
        terminal_update_window_size(ctx);
        if (ctx->view.screenrows > 0 && ctx->view.cy >= ctx->view.screenrows)
            ctx->view.cy = ctx->view.screenrows - 1;
        if (ctx->view.screencols > 0 && ctx->view.cx >= ctx->view.screencols)
            ctx->view.cx = ctx->view.screencols - 1;
        if (ctx->view.cy < 0) ctx->view.cy = 0;
        if (ctx->view.cx < 0) ctx->view.cx = 0;
        /* Re-clamp the scroll offsets: shrinking the window could otherwise
         * leave the view scrolled past the end of the buffer. */
        if (ctx->view.rowoff > ctx->model.numrows - 1)
            ctx->view.rowoff = ctx->model.numrows - 1;
        if (ctx->view.rowoff < 0) ctx->view.rowoff = 0;
        if (ctx->view.coloff < 0) ctx->view.coloff = 0;
    }
}

/* ======================= Screen Buffer ==================================== */

void terminal_buffer_append(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) {
        /* Out of memory - attempt to restore terminal and exit cleanly */
        write_ignore_result(STDOUT_FILENO, "\x1b[2J", 4);  /* Clear screen */
        write_ignore_result(STDOUT_FILENO, "\x1b[H", 3);   /* Go home */
        perror("Out of memory during screen refresh");
        exit(1);
    }
    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

void terminal_buffer_free(struct abuf *ab) {
    free(ab->b);
}

#endif /* !_WIN32 */
