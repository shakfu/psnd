/* host.c - Editor host implementations
 *
 * This module provides:
 * - Terminal host for interactive editing
 * - Headless host for scripted/automated editing
 * - Common host loop logic
 */

#include "host.h"
#include "terminal.h"
#include "event.h"
#include "buffers.h"
#include "lang_bridge.h"
#include "live_loop.h"
#include "async_queue.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define STDOUT_FILENO 1
#else
#include <unistd.h>
#endif

/* ======================= Common Host Logic ================================= */

int editor_host_run(EditorHost *host, const EditorConfig *config) {
    if (!host) return -1;

    /* Create session */
    EditorSession *session = editor_session_new(config);
    if (!session) {
        if (host->callbacks.on_error) {
            host->callbacks.on_error(host, "Failed to create session");
        }
        return -1;
    }

    /* Notify start */
    if (host->callbacks.on_start) {
        host->callbacks.on_start(host, session);
    }

    /* Run main loop */
    int result = editor_host_loop(host, session);

    /* Notify quit */
    if (host->callbacks.on_quit) {
        host->callbacks.on_quit(host, session);
    }

    /* Cleanup */
    editor_session_free(session);

    return result;
}

int editor_host_loop(EditorHost *host, EditorSession *session) {
    if (!host || !session) return -1;

    EditorEvent event;

    while (host->should_continue && host->should_continue(host)) {
        /* Render current state */
        if (host->render) {
            host->render(host, session);
        }

        /* Read next event */
        int read_result = host->read_event(host, &event, 100); /* 100ms timeout */
        if (read_result < 0) {
            return -1;  /* Input is gone; looping would spin */
        }

        if (read_result == 0) {
            /* Process event */
            int handle_result = editor_session_handle_event(session, &event);

            if (handle_result == 1) {
                /* Quit requested */
                return 0;
            }

            /* Notify tick */
            if (host->callbacks.on_tick) {
                host->callbacks.on_tick(host, session);
            }
        }
        /* Timeout - continue loop for render/resize handling */
    }

    return 0;
}

/* ======================= Terminal Host ===================================== */

typedef struct {
    TerminalHost terminal;
    int input_fd;
    int running;
} TerminalHostData;

static int terminal_host_read_event(EditorHost *host, EditorEvent *event, int timeout_ms) {
    TerminalHostData *data = (TerminalHostData *)host->data;
    (void)timeout_ms; /* terminal_read_key has its own timeout */

    /* Check for resize */
    if (terminal_host_resize_pending(&data->terminal)) {
        terminal_host_clear_resize(&data->terminal);
        event->type = EVENT_RESIZE;
        /* Actual dimensions will be fetched by session */
        int rows, cols;
        terminal_get_window_size(data->input_fd, STDOUT_FILENO, &rows, &cols);
        /* EVENT_RESIZE carries text rows, as the web hosts send them; the
         * terminal draws the status and message lines itself. */
        rows -= STATUS_ROWS;
        if (rows < 1) rows = 1;
        event->data.resize.rows = rows;
        event->data.resize.cols = cols;
        return 0;
    }

    /* Read key */
    int key = terminal_read_key(data->input_fd);
    if (key == -1) {
        return -1; /* Input closed or failed */
    }

    *event = event_from_keycode(key);
    return 0;
}

static void terminal_host_render(EditorHost *host, EditorSession *session) {
    (void)host;

    /* Get view model and render */
    EditorViewModel *vm = editor_session_snapshot(session);
    if (!vm) return;

    /* For now, we still use the legacy rendering path through
     * buffer_get_current() and editor_refresh_screen().
     * TODO: Use vm directly with a renderer */
    editor_viewmodel_free(vm);

    /* Use buffer system's refresh */
    editor_ctx_t *ctx = buffer_get_current();
    if (ctx) {
        editor_refresh_screen(ctx);
    }
}

static int terminal_host_should_continue(EditorHost *host) {
    TerminalHostData *data = (TerminalHostData *)host->data;
    return data->running;
}

static void terminal_host_destroy(EditorHost *host) {
    if (!host) return;

    TerminalHostData *data = (TerminalHostData *)host->data;
    if (data) {
        terminal_host_disable_raw_mode(&data->terminal);
        terminal_host_cleanup(&data->terminal);
        free(data);
    }
    free(host);
}

EditorHost *editor_host_terminal_create(int input_fd) {
    EditorHost *host = calloc(1, sizeof(EditorHost));
    if (!host) return NULL;

    TerminalHostData *data = calloc(1, sizeof(TerminalHostData));
    if (!data) {
        free(host);
        return NULL;
    }

    data->input_fd = input_fd;
    data->running = 1;

    /* Initialize terminal */
    if (terminal_host_init(&data->terminal, input_fd) != 0) {
        free(data);
        free(host);
        return NULL;
    }

    if (terminal_host_enable_raw_mode(&data->terminal) != 0) {
        terminal_host_cleanup(&data->terminal);
        free(data);
        free(host);
        return NULL;
    }

    host->read_event = terminal_host_read_event;
    host->render = terminal_host_render;
    host->should_continue = terminal_host_should_continue;
    host->destroy = terminal_host_destroy;
    host->data = data;

    return host;
}

/* ======================= Headless Host ===================================== */

#define HEADLESS_QUEUE_SIZE 256

typedef struct {
    EditorEvent queue[HEADLESS_QUEUE_SIZE];
    int queue_head;
    int queue_tail;
    int running;
} HeadlessHostData;

static int headless_host_read_event(EditorHost *host, EditorEvent *event, int timeout_ms) {
    (void)timeout_ms;
    HeadlessHostData *data = (HeadlessHostData *)host->data;

    if (data->queue_head == data->queue_tail) {
        return 1; /* Empty queue - timeout */
    }

    *event = data->queue[data->queue_head];
    data->queue_head = (data->queue_head + 1) % HEADLESS_QUEUE_SIZE;
    return 0;
}

static void headless_host_render(EditorHost *host, EditorSession *session) {
    (void)host;
    (void)session;
    /* Headless host doesn't render */
}

static int headless_host_should_continue(EditorHost *host) {
    HeadlessHostData *data = (HeadlessHostData *)host->data;
    return data->running;
}

static void headless_host_destroy(EditorHost *host) {
    if (!host) return;
    free(host->data);
    free(host);
}

EditorHost *editor_host_headless_create(void) {
    EditorHost *host = calloc(1, sizeof(EditorHost));
    if (!host) return NULL;

    HeadlessHostData *data = calloc(1, sizeof(HeadlessHostData));
    if (!data) {
        free(host);
        return NULL;
    }

    data->running = 1;

    host->read_event = headless_host_read_event;
    host->render = headless_host_render;
    host->should_continue = headless_host_should_continue;
    host->destroy = headless_host_destroy;
    host->data = data;

    return host;
}

int editor_host_headless_queue_event(EditorHost *host, const EditorEvent *event) {
    if (!host || !event) return -1;

    HeadlessHostData *data = (HeadlessHostData *)host->data;
    int next_tail = (data->queue_tail + 1) % HEADLESS_QUEUE_SIZE;

    if (next_tail == data->queue_head) {
        return -1; /* Queue full */
    }

    data->queue[data->queue_tail] = *event;
    data->queue_tail = next_tail;
    return 0;
}

void editor_host_headless_quit(EditorHost *host) {
    if (!host) return;
    HeadlessHostData *data = (HeadlessHostData *)host->data;
    data->running = 0;
}
