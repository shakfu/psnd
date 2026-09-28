/* host_web.c - Mongoose-based web server host implementation
 *
 * Provides HTTP/WebSocket server for browser-based editing.
 * Uses mongoose library for networking.
 *
 * REST API:
 *   POST /api/run   - Execute code {code, lang}
 *   POST /api/repl  - REPL command {command, lang}
 *   POST /api/save  - Save file {filename, content}
 *   POST /api/load  - Load file {filename}
 *
 * WebSocket API:
 *   {"cmd": "load", "filename": "..."}
 *   {"cmd": "save", "filename": "...", "content": "..."}
 *
 * xterm.js is served from host_web_xterm.h, generated at configure time from
 * source/thirdparty/xterm-* by scripts/cmake/psnd_xterm.cmake.
 */

#ifdef LOKI_WEB_HOST

#include "host_web.h"
#include "host.h"
#include "session.h"
#include "event.h"
#include "json.h"
#include "jsonrpc.h"
#include "lang_bridge.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat/thread.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#endif

#include "host_web_xterm.h"

/* Shared embedded HTML UI (also used by host_webview.cpp) */
#include "host_web_ui.h"

/* ======================= Constants ========================================= */

#define WEB_HOST_QUEUE_SIZE 256
#define WEB_HOST_DEFAULT_PORT 8080
#define WEB_HOST_POLL_MS 50
#define MAX_POST_SIZE (1024 * 1024)  /* 1MB max POST body */

/* Default bind address. Loopback only: the editor exposes file access and code
 * evaluation, so it must not be reachable from the network unless the user
 * explicitly opts in with --web-host. */
#define WEB_HOST_DEFAULT_BIND "127.0.0.1"

/* Session token: 32 random alphanumeric chars + NUL. Required on /ws and
 * the /api endpoints to stop other local users and cross-site requests from driving the
 * editor. */
#define WEB_HOST_TOKEN_LEN 33

/* ======================= Web Host Data ===================================== */

typedef struct {
    struct mg_mgr mgr;              /* Mongoose event manager */
    struct mg_connection *listener; /* HTTP listener connection */
    struct mg_connection *ws_conn;  /* Active WebSocket connection (single client) */
    EditorSession *session;         /* Editor session (set by host loop) */

    /* Event queue from WebSocket */
    EditorEvent queue[WEB_HOST_QUEUE_SIZE];
    int queue_head;
    int queue_tail;
    psnd_mutex_t queue_mutex;

    /* Configuration */
    char *web_root;                 /* Static file directory (owned, may be NULL) */
    char bind_addr[64];             /* Bind address (default 127.0.0.1) */
    int port;                       /* Listening port */
    int running;                    /* Continue running flag */

    /* Access control */
    char token[WEB_HOST_TOKEN_LEN]; /* Per-session auth token */

    /* Render state */
    int needs_render;               /* Flag to push update to client */

    /* REPL state */
    char current_lang[32];          /* Current language for REPL (e.g., "alda", "joy") */
} WebHostData;

/* ======================= Event Queue ======================================= */

static int web_host_queue_event(WebHostData *data, const EditorEvent *event) {
    psnd_mutex_lock(&data->queue_mutex);

    int next_tail = (data->queue_tail + 1) % WEB_HOST_QUEUE_SIZE;
    if (next_tail == data->queue_head) {
        psnd_mutex_unlock(&data->queue_mutex);
        return -1; /* Queue full */
    }

    data->queue[data->queue_tail] = *event;
    data->queue_tail = next_tail;

    psnd_mutex_unlock(&data->queue_mutex);
    return 0;
}

static int web_host_dequeue_event(WebHostData *data, EditorEvent *event) {
    psnd_mutex_lock(&data->queue_mutex);

    if (data->queue_head == data->queue_tail) {
        psnd_mutex_unlock(&data->queue_mutex);
        return -1; /* Queue empty */
    }

    *event = data->queue[data->queue_head];
    data->queue_head = (data->queue_head + 1) % WEB_HOST_QUEUE_SIZE;

    psnd_mutex_unlock(&data->queue_mutex);
    return 0;
}

/* ======================= Response Helpers ================================== */

static void send_json_response(struct mg_connection *c, int status, const char *json) {
    mg_http_reply(c, status, "Content-Type: application/json\r\n", "%s", json);
}

static void send_json_ok(struct mg_connection *c, const char *extra) {
    if (extra) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                     "{\"ok\":true,%s}", extra);
    } else {
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                     "{\"ok\":true}");
    }
}

static void send_json_error(struct mg_connection *c, int status, const char *error) {
    JsonBuilder jb;
    json_builder_init(&jb);
    json_object_start(&jb);
    json_kv_bool(&jb, "ok", 0);
    json_kv_string(&jb, "error", error);
    json_object_end(&jb);
    send_json_response(c, status, json_builder_get(&jb));
    json_builder_free(&jb);
}

/* ======================= REST API Handlers ================================= */

/* POST /api/run - Execute code */
static void handle_api_run(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    if (!data->session) {
        send_json_error(c, 503, "Session not ready");
        return;
    }

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *code = json_object_get_string(&req, "code");
    const char *lang = json_object_get_string(&req, "lang");

    if (!code) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'code' parameter");
        return;
    }

    /* Check for first-line language directive: #alda, #joy, etc. */
    const char *code_start = code;
    char detected_lang[32] = {0};
    if (code[0] == '#') {
        /* Extract language name from first line */
        const char *end = code + 1;
        while (*end && *end != '\n' && *end != '\r' && *end != ' ' && (end - code) < 31) {
            end++;
        }
        size_t len = end - (code + 1);
        if (len > 0 && len < sizeof(detected_lang)) {
            memcpy(detected_lang, code + 1, len);
            detected_lang[len] = '\0';
            /* Skip to next line for actual code */
            while (*end && (*end == ' ' || *end == '\t')) end++;
            if (*end == '\n') end++;
            else if (*end == '\r') { end++; if (*end == '\n') end++; }
            code_start = end;
        }
    }

    /* Get filename from session for language detection */
    const char *filename = editor_session_get_filename(data->session);

    /* Try to find language: explicit param > first-line directive > file extension > default */
    const LokiLangOps *ops = NULL;
    if (lang && *lang) {
        ops = loki_lang_by_name(lang);
    }
    if (!ops && detected_lang[0]) {
        ops = loki_lang_by_name(detected_lang);
    }
    if (!ops && filename) {
        ops = loki_lang_for_file(filename);
    }
    /* Fall back to first available language */
    if (!ops) {
        int lang_count = 0;
        const LokiLangOps **langs = loki_lang_all(&lang_count);
        if (lang_count > 0) {
            ops = langs[0];
        }
    }

    /* Get editor context for language evaluation */
    editor_ctx_t *ctx = editor_session_get_ctx(data->session);

    int result = -1;
    const char *error = NULL;

    if (ops && ops->eval && ctx) {
        /* Initialize language if needed */
        if (ops->init && (!ops->is_initialized || !ops->is_initialized(ctx))) {
            ops->init(ctx);
        }
        result = ops->eval(ctx, code_start);  /* Use code_start to skip directive */
        if (result != 0 && ops->get_error) {
            error = ops->get_error(ctx);
        }
    } else {
        error = "No language available for this file type";
    }

    json_value_free(&req);

    if (result == 0) {
        send_json_ok(c, "\"output\":\"\"");
    } else {
        send_json_error(c, 200, error ? error : "Execution failed");
    }
}

/* POST /api/repl - REPL command */
static void handle_api_repl(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    if (!data->session) {
        send_json_error(c, 503, "Session not ready");
        return;
    }

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *command = json_object_get_string(&req, "command");
    const char *lang = json_object_get_string(&req, "lang");

    if (!command) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'command' parameter");
        return;
    }

    /* Handle ex-commands (starting with :) */
    if (command[0] == ':') {
        const char *cmd = command + 1;  /* Skip the colon */

        /* :help - show available commands */
        if (strcmp(cmd, "help") == 0) {
            int lang_count = 0;
            const LokiLangOps **langs = loki_lang_all(&lang_count);

            /* Build help message with available languages */
            char help[512];
            int offset = snprintf(help, sizeof(help),
                "Commands:\\n"
                "  :help     - Show this help\\n"
                "  :lang     - Show current language\\n"
                "  :langs    - List available languages\\n"
                "  :<name>   - Switch to language (");

            for (int i = 0; i < lang_count && offset < (int)sizeof(help) - 20; i++) {
                offset += snprintf(help + offset, sizeof(help) - offset,
                    "%s%s", i > 0 ? ", " : "", langs[i]->name);
            }
            snprintf(help + offset, sizeof(help) - offset, ")");

            json_value_free(&req);
            /* Escape for JSON */
            char response[600];
            snprintf(response, sizeof(response), "\"output\":\"%s\"", help);
            send_json_ok(c, response);
            return;
        }

        /* :lang - show current language */
        if (strcmp(cmd, "lang") == 0) {
            char response[128];
            snprintf(response, sizeof(response), "\"output\":\"Current language: %s\"",
                data->current_lang[0] ? data->current_lang : "alda (default)");
            json_value_free(&req);
            send_json_ok(c, response);
            return;
        }

        /* :langs - list available languages */
        if (strcmp(cmd, "langs") == 0) {
            int lang_count = 0;
            const LokiLangOps **langs = loki_lang_all(&lang_count);

            char list[256] = "Available languages: ";
            int offset = strlen(list);
            for (int i = 0; i < lang_count && offset < (int)sizeof(list) - 20; i++) {
                offset += snprintf(list + offset, sizeof(list) - offset,
                    "%s%s", i > 0 ? ", " : "", langs[i]->name);
            }

            char response[300];
            snprintf(response, sizeof(response), "\"output\":\"%s\"", list);
            json_value_free(&req);
            send_json_ok(c, response);
            return;
        }

        /* Try to match a language name (e.g., :alda, :joy) */
        const LokiLangOps *lang_ops = loki_lang_by_name(cmd);
        if (lang_ops) {
            strncpy(data->current_lang, cmd, sizeof(data->current_lang) - 1);
            data->current_lang[sizeof(data->current_lang) - 1] = '\0';

            char response[128];
            snprintf(response, sizeof(response), "\"output\":\"Switched to %s\"", cmd);
            json_value_free(&req);
            send_json_ok(c, response);
            return;
        }

        /* Unknown command */
        json_value_free(&req);
        send_json_error(c, 200, "Unknown command. Type :help for available commands.");
        return;
    }

    /* Get filename from session for language detection */
    const char *filename = editor_session_get_filename(data->session);

    /* Determine language: explicit param > stored REPL lang > file extension > default */
    const LokiLangOps *ops = NULL;
    if (lang && *lang) {
        ops = loki_lang_by_name(lang);
    }
    if (!ops && data->current_lang[0]) {
        ops = loki_lang_by_name(data->current_lang);
    }
    if (!ops && filename) {
        ops = loki_lang_for_file(filename);
    }
    /* Fall back to first available language (usually alda) */
    if (!ops) {
        int lang_count = 0;
        const LokiLangOps **langs = loki_lang_all(&lang_count);
        if (lang_count > 0) {
            ops = langs[0];
        }
    }

    /* Get editor context for language evaluation */
    editor_ctx_t *ctx = editor_session_get_ctx(data->session);

    int result = -1;
    const char *error = NULL;

    if (ops && ops->eval && ctx) {
        if (ops->init && (!ops->is_initialized || !ops->is_initialized(ctx))) {
            ops->init(ctx);
        }
        result = ops->eval(ctx, command);
        if (result != 0 && ops->get_error) {
            error = ops->get_error(ctx);
        }
    } else {
        error = "No language available";
    }

    json_value_free(&req);

    if (result == 0) {
        send_json_ok(c, "\"output\":\"\"");
    } else {
        send_json_error(c, 200, error ? error : "Command failed");
    }
}

/* POST /api/save - Save file */
static void handle_api_save(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    (void)data; /* May be used later for session access */

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *filename = json_object_get_string(&req, "filename");
    const char *content = json_object_get_string(&req, "content");

    if (!filename || !content) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'filename' or 'content' parameter");
        return;
    }

    /* Write file */
    FILE *f = fopen(filename, "w");
    if (!f) {
        json_value_free(&req);
        send_json_error(c, 500, "Failed to open file for writing");
        return;
    }

    size_t written = fwrite(content, 1, strlen(content), f);
    fclose(f);

    json_value_free(&req);

    if (written > 0) {
        send_json_ok(c, NULL);
    } else {
        send_json_error(c, 500, "Failed to write file");
    }
}

/* POST /api/load - Load file */
static void handle_api_load(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    (void)data;

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *filename = json_object_get_string(&req, "filename");
    if (!filename) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'filename' parameter");
        return;
    }

    /* Read file */
    FILE *f = fopen(filename, "r");
    if (!f) {
        json_value_free(&req);
        send_json_error(c, 404, "File not found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* ftell returns -1 on a non-seekable stream (fifo, /proc, device). Without
     * this check malloc() would undersize the buffer and the fread() below would
     * read until EOF into it. */
    if (size < 0) {
        fclose(f);
        json_value_free(&req);
        send_json_error(c, 400, "Not a regular file");
        return;
    }

    char *content = malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        json_value_free(&req);
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }

    size_t read = fread(content, 1, (size_t)size, f);
    content[read] = '\0';
    fclose(f);

    /* Build response */
    JsonBuilder jb;
    json_builder_init(&jb);
    json_object_start(&jb);
    json_kv_bool(&jb, "ok", 1);
    json_kv_string(&jb, "filename", filename);
    json_kv_string(&jb, "content", content);
    json_object_end(&jb);

    send_json_response(c, 200, json_builder_get(&jb));

    json_builder_free(&jb);
    free(content);
    json_value_free(&req);
}

/* ======================= WebSocket Message Processing ====================== */

/* Parse WebSocket JSON and queue appropriate event or handle command */
static void web_host_process_message(WebHostData *data, struct mg_connection *c,
                                     const char *msg, size_t len) {
    /* Parse JSON */
    char *json_str = malloc(len + 1);
    if (!json_str) return;
    memcpy(json_str, msg, len);
    json_str[len] = '\0';

    JsonValue cmd = json_parse(json_str);
    free(json_str);

    if (cmd.type == JSON_ERROR) {
        return;
    }

    const char *cmd_type = json_object_get_string(&cmd, "cmd");
    if (!cmd_type) {
        json_value_free(&cmd);
        return;
    }

    EditorEvent event = {0};

    if (strcmp(cmd_type, "event") == 0) {
        /* Key event: {"cmd": "event", "type": "key", "code": 105, "modifiers": 0} */
        const char *type = json_object_get_string(&cmd, "type");
        if (type && strcmp(type, "key") == 0) {
            int code = json_object_get_int(&cmd, "code", 0);
            int mods = json_object_get_int(&cmd, "modifiers", 0);

            event.type = EVENT_KEY;
            event.data.key.keycode = code;
            event.data.key.modifiers = (uint8_t)mods;
            web_host_queue_event(data, &event);
        } else if (type && strcmp(type, "mouse") == 0) {
            /* Mouse event: {"cmd": "event", "type": "mouse", "x": 10, "y": 5, "button": 0, "pressed": 1} */
            int x = json_object_get_int(&cmd, "x", 0);
            int y = json_object_get_int(&cmd, "y", 0);
            int button = json_object_get_int(&cmd, "button", 0);
            int pressed = json_object_get_int(&cmd, "pressed", 1);
            int mods = json_object_get_int(&cmd, "modifiers", 0);

            event.type = EVENT_MOUSE;
            event.data.mouse.x = x;
            event.data.mouse.y = y;
            event.data.mouse.button = button;
            event.data.mouse.pressed = pressed;
            event.data.mouse.modifiers = (uint8_t)mods;
            web_host_queue_event(data, &event);
        }
    } else if (strcmp(cmd_type, "resize") == 0) {
        /* Resize event: {"cmd": "resize", "rows": 24, "cols": 80} */
        int rows = json_object_get_int(&cmd, "rows", 24);
        int cols = json_object_get_int(&cmd, "cols", 80);

        event.type = EVENT_RESIZE;
        event.data.resize.rows = rows;
        event.data.resize.cols = cols;
        web_host_queue_event(data, &event);
    } else if (strcmp(cmd_type, "snapshot") == 0) {
        /* Snapshot request - mark for render */
        data->needs_render = 1;
    } else if (strcmp(cmd_type, "quit") == 0) {
        /* Quit event */
        event.type = EVENT_QUIT;
        web_host_queue_event(data, &event);
    } else if (strcmp(cmd_type, "load") == 0) {
        /* Load file via WebSocket */
        const char *filename = json_object_get_string(&cmd, "filename");
        if (filename) {
            FILE *f = fopen(filename, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);

                /* See handle_api_load: a negative ftell would undersize the
                 * buffer and let fread overrun it. */
                char *content = size < 0 ? NULL : malloc((size_t)size + 1);
                if (content) {
                    size_t read = fread(content, 1, (size_t)size, f);
                    content[read] = '\0';

                    /* Build response */
                    JsonBuilder jb;
                    json_builder_init(&jb);
                    json_object_start(&jb);
                    json_kv_string(&jb, "type", "file");
                    json_kv_string(&jb, "filename", filename);
                    json_kv_string(&jb, "content", content);
                    json_object_end(&jb);

                    mg_ws_send(c, json_builder_get(&jb), strlen(json_builder_get(&jb)),
                              WEBSOCKET_OP_TEXT);
                    json_builder_free(&jb);
                    free(content);
                }
                fclose(f);
            } else {
                /* Send error */
                mg_ws_send(c, "{\"type\":\"error\",\"text\":\"File not found\"}",
                          40, WEBSOCKET_OP_TEXT);
            }
        }
    }

    json_value_free(&cmd);
}

/* Send ViewModel snapshot to WebSocket client */
static void web_host_send_snapshot(WebHostData *data) {
    if (!data->ws_conn || !data->session) return;

    EditorViewModel *vm = editor_session_snapshot(data->session);
    if (!vm) return;

    char *vm_json = jsonrpc_serialize_viewmodel(vm);
    editor_viewmodel_free(vm);

    if (!vm_json) return;

    /* Build response: {"type": "update", "viewmodel": ...} */
    size_t vm_len = strlen(vm_json);
    size_t total_len = vm_len + 32; /* Extra for wrapper */
    char *response = malloc(total_len);
    if (response) {
        snprintf(response, total_len, "{\"type\":\"update\",\"viewmodel\":%s}", vm_json);
        mg_ws_send(data->ws_conn, response, strlen(response), WEBSOCKET_OP_TEXT);
        free(response);
    }

    free(vm_json);
}

/* ======================= Access Control ==================================== */

/* Constant-time string compare, so a caller cannot recover the token by
 * timing successive guesses. */
static int web_host_secret_eq(const char *a, const char *b, size_t len) {
    unsigned char diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return diff == 0;
}

/* Check the session token, supplied either as an "X-Psnd-Token" header or as a
 * "token" query parameter (browsers cannot set headers on a WebSocket
 * handshake, so the query form is required for /ws). */
static int web_host_token_ok(WebHostData *data, struct mg_http_message *hm) {
    char supplied[WEB_HOST_TOKEN_LEN];
    size_t want = strlen(data->token);

    struct mg_str *hdr = mg_http_get_header(hm, "X-Psnd-Token");
    if (hdr && hdr->len == want &&
        web_host_secret_eq(hdr->buf, data->token, want)) {
        return 1;
    }

    int n = mg_http_get_var(&hm->query, "token", supplied, sizeof(supplied));
    if (n == (int)want && web_host_secret_eq(supplied, data->token, want)) {
        return 1;
    }

    return 0;
}

/* Reject cross-origin WebSocket handshakes.
 *
 * A browser always sends Origin on a WebSocket handshake, so any request
 * carrying an Origin that is not our own is a page on another site trying to
 * drive this editor (cross-site WebSocket hijacking). Requests with no Origin
 * are non-browser clients, which cannot be used for that attack and are still
 * gated by the token. */
static int web_host_origin_ok(struct mg_http_message *hm) {
    struct mg_str *origin = mg_http_get_header(hm, "Origin");
    if (!origin) return 1; /* Non-browser client; token check still applies */

    struct mg_str *host = mg_http_get_header(hm, "Host");
    if (!host) return 0;

    /* Compare the authority portion of Origin ("scheme://authority") against
     * the Host header. */
    size_t prefix = 0;
    for (size_t i = 0; i + 3 <= origin->len; i++) {
        if (memcmp(origin->buf + i, "://", 3) == 0) {
            prefix = i + 3;
            break;
        }
    }
    if (prefix == 0) return 0; /* Malformed or opaque origin ("null") */

    struct mg_str authority = mg_str_n(origin->buf + prefix, origin->len - prefix);
    return mg_strcasecmp(authority, *host) == 0;
}

static void send_forbidden(struct mg_connection *c) {
    mg_http_reply(c, 403, "Content-Type: application/json\r\n",
                  "{\"error\":\"forbidden\"}");
}

/* ======================= Mongoose Event Handler ============================ */

static void web_host_handler(struct mg_connection *c, int ev, void *ev_data) {
    WebHostData *data = (WebHostData *)c->fn_data;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        /* WebSocket upgrade. Requires both a same-origin handshake and the
         * session token: this connection can drive the whole editor. */
        if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            if (!web_host_origin_ok(hm) || !web_host_token_ok(data, hm)) {
                send_forbidden(c);
                return;
            }
            mg_ws_upgrade(c, hm, NULL);
            data->ws_conn = c;
            data->needs_render = 1; /* Send initial state */
            return;
        }

        /* Everything under /api/ evaluates code or touches the filesystem, so
         * it is token-gated too. */
        if (mg_match(hm->uri, mg_str("/api/#"), NULL) &&
            !web_host_token_ok(data, hm)) {
            send_forbidden(c);
            return;
        }

        /* REST API endpoints */
        if (mg_match(hm->uri, mg_str("/api/run"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_run(c, hm, data);
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/repl"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_repl(c, hm, data);
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/save"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_save(c, hm, data);
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/load"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_load(c, hm, data);
            return;
        }

        /* Serve embedded xterm.js files */
        if (mg_match(hm->uri, mg_str("/xterm.css"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: text/css\r\nCache-Control: max-age=86400\r\n",
                         "%s", XTERM_CSS);
            return;
        }
        if (mg_match(hm->uri, mg_str("/xterm.js"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/javascript\r\nCache-Control: max-age=86400\r\n",
                         "%s", XTERM_JS);
            return;
        }
        if (mg_match(hm->uri, mg_str("/xterm-fit.js"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/javascript\r\nCache-Control: max-age=86400\r\n",
                         "%s", XTERM_FIT_JS);
            return;
        }

        /* Serve static files from web_root. Token-gated: web_root is an
         * arbitrary user-chosen directory. */
        if (data->web_root && !web_host_token_ok(data, hm)) {
            send_forbidden(c);
            return;
        }
        if (data->web_root) {
            struct mg_http_serve_opts opts = {
                .root_dir = data->web_root,
                .ssi_pattern = NULL
            };
            mg_http_serve_dir(c, hm, &opts);
        } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            /* Serve embedded HTML for root path if no web_root */
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                         "%s", EMBEDDED_HTML);
        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        web_host_process_message(data, c, wm->data.buf, wm->data.len);

    } else if (ev == MG_EV_CLOSE) {
        if (c == data->ws_conn) {
            data->ws_conn = NULL;
        }
    }
}

/* ======================= EditorHost Interface ============================== */

static int web_host_read_event(EditorHost *host, EditorEvent *event, int timeout_ms) {
    WebHostData *data = (WebHostData *)host->data;

    /* Poll mongoose for network events */
    mg_mgr_poll(&data->mgr, timeout_ms > 0 ? timeout_ms : WEB_HOST_POLL_MS);

    /* Check if WebSocket delivered an event */
    if (web_host_dequeue_event(data, event) == 0) {
        /* Mark for render after event is processed */
        data->needs_render = 1;
        return 0; /* Got event */
    }

    return 1; /* Timeout */
}

static void web_host_render(EditorHost *host, EditorSession *session) {
    WebHostData *data = (WebHostData *)host->data;

    /* Store session reference for snapshot requests */
    data->session = session;

    /* Send snapshot only when flagged (after event processed) */
    if (data->needs_render && data->ws_conn) {
        web_host_send_snapshot(data);
        data->needs_render = 0;
    }
}

static int web_host_should_continue(EditorHost *host) {
    WebHostData *data = (WebHostData *)host->data;
    return data->running;
}

static void web_host_destroy(EditorHost *host) {
    if (!host) return;

    WebHostData *data = (WebHostData *)host->data;
    if (data) {
        data->running = 0;
        mg_mgr_free(&data->mgr);
        psnd_mutex_destroy(&data->queue_mutex);
        free(data->web_root);
        free(data);
    }
    free(host);
}

/* ======================= Public API ======================================== */

EditorHost *editor_host_web_create(const char *bind_addr, int port,
                                   const char *web_root) {
    EditorHost *host = calloc(1, sizeof(EditorHost));
    if (!host) return NULL;

    WebHostData *data = calloc(1, sizeof(WebHostData));
    if (!data) {
        free(host);
        return NULL;
    }

    /* Initialize mutex */
    if (psnd_mutex_init(&data->queue_mutex) != 0) {
        free(data);
        free(host);
        return NULL;
    }

    data->port = port > 0 ? port : WEB_HOST_DEFAULT_PORT;
    data->running = 1;

    snprintf(data->bind_addr, sizeof(data->bind_addr), "%s",
             (bind_addr && bind_addr[0]) ? bind_addr : WEB_HOST_DEFAULT_BIND);

    /* Generate the per-session token before the listener starts accepting.
     * mg_random_str() returns its own buffer and so reports nothing about
     * entropy; mg_random() is the call that does, and it falls back to rand()
     * when /dev/urandom cannot be read. Probe it first and refuse to start,
     * rather than serving on a predictable token. */
    if (!mg_random(data->token, sizeof(data->token))) {
        fprintf(stderr, "Error: no secure RNG for the session token\n");
        psnd_mutex_destroy(&data->queue_mutex);
        free(data);
        free(host);
        return NULL;
    }
    mg_random_str(data->token, sizeof(data->token));

    if (web_root) {
        data->web_root = strdup(web_root);
        if (!data->web_root) {
            psnd_mutex_destroy(&data->queue_mutex);
            free(data);
            free(host);
            return NULL;
        }
    }

    /* Initialize mongoose */
    mg_mgr_init(&data->mgr);

    /* Build listen URL */
    char url[96];
    snprintf(url, sizeof(url), "http://%s:%d", data->bind_addr, data->port);

    /* Start HTTP listener */
    data->listener = mg_http_listen(&data->mgr, url, web_host_handler, data);
    if (!data->listener) {
        fprintf(stderr, "Error: Failed to bind to port %d\n", data->port);
        mg_mgr_free(&data->mgr);
        psnd_mutex_destroy(&data->queue_mutex);
        free(data->web_root);
        free(data);
        free(host);
        return NULL;
    }

    /* Set up host interface */
    host->read_event = web_host_read_event;
    host->render = web_host_render;
    host->should_continue = web_host_should_continue;
    host->destroy = web_host_destroy;
    host->data = data;

    return host;
}

int editor_host_web_get_port(EditorHost *host) {
    if (!host || !host->data) return -1;
    WebHostData *data = (WebHostData *)host->data;
    return data->port;
}

/* Hand `url` to the platform's default browser, without a shell: the URL
 * carries the session token and the bind address comes from the command line,
 * so nothing here is interpolated into a command string. Failure is silent
 * beyond a note - the URL has already been printed for the user to open. */
static void web_host_open_url(const char *url) {
#ifdef _WIN32
    if ((INT_PTR)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL) <= 32) {
        fprintf(stderr, "Note: could not open a browser; open the URL above.\n");
    }
#else
#ifdef __APPLE__
    char *const argv[] = { (char *)"/usr/bin/open", (char *)url, NULL };
#else
    char *const argv[] = { (char *)"xdg-open", (char *)url, NULL };
#endif
    /* Double fork: the grandchild is reparented to init, so the opener is
     * never a zombie in a server that runs for hours and never waits. */
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Note: could not open a browser; open the URL above.\n");
        return;
    }
    if (pid == 0) {
        if (fork() == 0) {
            /* xdg-open is chatty and would corrupt the terminal UI. */
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                if (devnull > STDERR_FILENO) close(devnull);
            }
            setsid();
            execvp(argv[0], argv);
            _exit(127);
        }
        _exit(0);
    }
    /* Returns as soon as the intermediate exits, not when the browser does. */
    waitpid(pid, NULL, 0);
#endif
}

int editor_host_web_run(const char *bind_addr, int port, const char *web_root,
                        int open_browser, const EditorConfig *config) {
    /* Register all languages before creating session */
    loki_lang_init();

    EditorHost *host = editor_host_web_create(bind_addr, port, web_root);
    if (!host) {
        return 1;
    }

    WebHostData *data = (WebHostData *)host->data;

    /* One URL for both the message and the browser: the token is only useful
     * to the client when it arrives with the address. */
    char url[160];
    snprintf(url, sizeof(url), "http://%s:%d/?token=%s",
             data->bind_addr, data->port, data->token);

    fprintf(stderr, "Web editor running at %s\n", url);
    if (strcmp(data->bind_addr, "127.0.0.1") != 0 &&
        strcmp(data->bind_addr, "localhost") != 0) {
        fprintf(stderr,
                "WARNING: bound to %s - reachable from the network. Anyone who\n"
                "         obtains the token above can run code as you.\n",
                data->bind_addr);
    }
    if (data->web_root) {
        fprintf(stderr, "Serving static files from: %s\n", data->web_root);
    }
    fprintf(stderr, "Press Ctrl-C to stop\n");
    fflush(stderr);

    /* After the listener is bound, so the browser cannot beat the server to
     * the port. */
    if (open_browser) web_host_open_url(url);

    int result = editor_host_run(host, config);

    host->destroy(host);
    return result;
}

#endif /* LOKI_WEB_HOST */
