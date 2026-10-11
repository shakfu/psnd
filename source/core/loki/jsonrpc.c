/* jsonrpc.c - JSON-RPC harness implementation
 *
 * Provides a stdio-based JSON-RPC interface for testing editor abstractions.
 */

#include "jsonrpc.h"
#include "json.h"
#include "event.h"
#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum input line length */
#define MAX_LINE 65536

/* ======================= ViewModel Serialization =========================== */

char *jsonrpc_serialize_viewmodel(const EditorViewModel *vm) {
    if (!vm) return NULL;

    JsonBuilder jb;
    json_builder_init(&jb);

    json_object_start(&jb);

    /* Screen dimensions */
    json_kv_int(&jb, "rows", vm->rows);
    json_kv_int(&jb, "cols", vm->cols);
    json_kv_int(&jb, "gutter_width", vm->gutter_width);

    /* Cursor */
    json_key(&jb, "cursor");
    json_object_start(&jb);
    json_kv_int(&jb, "row", vm->cursor.row);
    json_kv_int(&jb, "col", vm->cursor.col);
    json_kv_int(&jb, "file_row", vm->cursor.file_row);
    json_kv_int(&jb, "file_col", vm->cursor.file_col);
    json_kv_bool(&jb, "visible", vm->cursor.visible);
    json_object_end(&jb);

    /* Status */
    json_key(&jb, "status");
    json_object_start(&jb);
    json_kv_string(&jb, "mode", vm->status.mode);
    json_kv_string(&jb, "filename", vm->status.filename);
    json_kv_string(&jb, "lang", vm->status.lang);
    json_kv_int(&jb, "numrows", vm->status.numrows);
    json_kv_int(&jb, "current_row", vm->status.current_row);
    json_kv_bool(&jb, "dirty", vm->status.dirty);
    json_kv_bool(&jb, "playing", vm->status.playing);
    json_kv_bool(&jb, "link_active", vm->status.link_active);
    json_object_end(&jb);

    /* Message */
    json_kv_string(&jb, "message", vm->message);

    /* Tabs */
    json_key(&jb, "tabs");
    json_object_start(&jb);
    json_kv_int(&jb, "count", vm->tabs.count);
    json_kv_int(&jb, "active", vm->tabs.active);
    json_key(&jb, "labels");
    json_array_start(&jb);
    for (int i = 0; i < vm->tabs.count; i++) {
        json_string(&jb, vm->tabs.labels[i]);
    }
    json_array_end(&jb);
    json_object_end(&jb);

    /* REPL */
    json_key(&jb, "repl");
    json_object_start(&jb);
    json_kv_bool(&jb, "active", vm->repl_active);
    if (vm->repl_active) {
        json_kv_string(&jb, "prompt", vm->repl.prompt);
        json_kv_string(&jb, "input", vm->repl.input);
        json_kv_int(&jb, "input_len", vm->repl.input_len);
        json_key(&jb, "log");
        json_array_start(&jb);
        for (int i = 0; i < vm->repl.log_count; i++) {
            json_string(&jb, vm->repl.log_lines[i]);
        }
        json_array_end(&jb);
    }
    json_object_end(&jb);

    /* Row views */
    json_key(&jb, "rows_content");
    json_array_start(&jb);
    for (int i = 0; i < vm->row_count; i++) {
        const EditorRowView *rv = &vm->row_views[i];
        json_object_start(&jb);
        json_kv_int(&jb, "row_num", rv->row_num);
        json_kv_bool(&jb, "is_empty", rv->is_empty);

        /* Segments */
        json_key(&jb, "segments");
        json_array_start(&jb);
        for (int j = 0; j < rv->segment_count; j++) {
            const RenderSegment *seg = &rv->segments[j];
            json_object_start(&jb);
            json_key(&jb, "text");
            json_string_len(&jb, seg->text, seg->len);
            json_kv_int(&jb, "hl_type", seg->hl_type);
            json_kv_bool(&jb, "selected", seg->selected);
            json_object_end(&jb);
        }
        json_array_end(&jb);

        json_object_end(&jb);
    }
    json_array_end(&jb);

    json_object_end(&jb);

    /* Extract result */
    char *result = strdup(json_builder_get(&jb));
    json_builder_free(&jb);
    return result;
}

/* ======================= Response Helpers ================================== */

static void respond_ok(void) {
    printf("{\"ok\":true}\n");
    fflush(stdout);
}

static void respond_error(const char *msg) {
    JsonBuilder jb;
    json_builder_init(&jb);
    json_object_start(&jb);
    json_kv_bool(&jb, "ok", 0);
    json_kv_string(&jb, "error", msg);
    json_object_end(&jb);
    printf("%s\n", json_builder_get(&jb));
    json_builder_free(&jb);
    fflush(stdout);
}

static void respond_viewmodel(const EditorViewModel *vm) {
    char *vm_json = jsonrpc_serialize_viewmodel(vm);
    if (vm_json) {
        printf("{\"ok\":true,\"viewmodel\":%s}\n", vm_json);
        free(vm_json);
    } else {
        respond_error("Failed to serialize viewmodel");
    }
    fflush(stdout);
}

static void respond_status(EditorSession *session) {
    JsonBuilder jb;
    json_builder_init(&jb);
    json_object_start(&jb);
    json_kv_bool(&jb, "ok", 1);
    json_kv_string(&jb, "mode",
        editor_session_get_mode(session) == MODE_NORMAL ? "normal" :
        editor_session_get_mode(session) == MODE_INSERT ? "insert" :
        editor_session_get_mode(session) == MODE_VISUAL ? "visual" :
        editor_session_get_mode(session) == MODE_COMMAND ? "command" : "unknown");
    json_kv_string(&jb, "filename", editor_session_get_filename(session));
    json_kv_bool(&jb, "dirty", editor_session_is_dirty(session));
    json_object_end(&jb);
    printf("%s\n", json_builder_get(&jb));
    json_builder_free(&jb);
    fflush(stdout);
}

/* ======================= Command Processing ================================ */

typedef enum {
    CMD_UNKNOWN,
    CMD_LOAD,
    CMD_SAVE,
    CMD_EVENT,
    CMD_SNAPSHOT,
    CMD_STATUS,
    CMD_QUIT,
    CMD_RESIZE,
    CMD_INSERT_TEXT
} CommandType;

static CommandType parse_command_type(const char *cmd) {
    if (!cmd) return CMD_UNKNOWN;
    if (strcmp(cmd, "load") == 0) return CMD_LOAD;
    if (strcmp(cmd, "save") == 0) return CMD_SAVE;
    if (strcmp(cmd, "event") == 0) return CMD_EVENT;
    if (strcmp(cmd, "snapshot") == 0) return CMD_SNAPSHOT;
    if (strcmp(cmd, "status") == 0) return CMD_STATUS;
    if (strcmp(cmd, "quit") == 0) return CMD_QUIT;
    if (strcmp(cmd, "resize") == 0) return CMD_RESIZE;
    if (strcmp(cmd, "insert") == 0) return CMD_INSERT_TEXT;
    return CMD_UNKNOWN;
}

/* Process a single command. Returns 1 if should quit, 0 to continue. */
static int process_command(EditorSession *session, const JsonValue *cmd_obj) {
    if (cmd_obj->type != JSON_OBJECT) {
        respond_error("Expected JSON object");
        return 0;
    }

    const char *cmd_str = json_object_get_string(cmd_obj, "cmd");
    CommandType cmd_type = parse_command_type(cmd_str);

    switch (cmd_type) {
        case CMD_LOAD: {
            const char *file = json_object_get_string(cmd_obj, "file");
            if (!file) {
                respond_error("load: missing 'file' parameter");
                return 0;
            }
            int result = editor_session_open(session, file);
            if (result == 0) {
                respond_ok();
            } else {
                respond_error("Failed to load file");
            }
            return 0;
        }

        case CMD_SAVE: {
            int result = editor_session_save(session);
            if (result == 0) {
                respond_ok();
            } else {
                respond_error("Failed to save file");
            }
            return 0;
        }

        case CMD_EVENT: {
            const char *type = json_object_get_string(cmd_obj, "type");
            if (!type) {
                respond_error("event: missing 'type' parameter");
                return 0;
            }

            EditorEvent event = {0};

            if (strcmp(type, "key") == 0) {
                int code = json_object_get_int(cmd_obj, "code", 0);
                int mods = json_object_get_int(cmd_obj, "modifiers", 0);

                event.type = EVENT_KEY;
                event.data.key.keycode = code;
                event.data.key.modifiers = (uint8_t)mods;
            } else if (strcmp(type, "quit") == 0) {
                event.type = EVENT_QUIT;
            } else if (strcmp(type, "resize") == 0) {
                event.type = EVENT_RESIZE;
                event.data.resize.rows = json_object_get_int(cmd_obj, "rows", 24);
                event.data.resize.cols = json_object_get_int(cmd_obj, "cols", 80);
            } else {
                respond_error("event: unknown type");
                return 0;
            }

            int result = editor_session_handle_event(session, &event);
            if (result == 1) {
                respond_ok();
                return 1; /* Quit */
            }
            respond_ok();
            return 0;
        }

        case CMD_SNAPSHOT: {
            EditorViewModel *vm = editor_session_snapshot(session);
            if (vm) {
                respond_viewmodel(vm);
                editor_viewmodel_free(vm);
            } else {
                respond_error("Failed to create snapshot");
            }
            return 0;
        }

        case CMD_STATUS: {
            respond_status(session);
            return 0;
        }

        case CMD_RESIZE: {
            int rows = json_object_get_int(cmd_obj, "rows", 24);
            int cols = json_object_get_int(cmd_obj, "cols", 80);
            editor_session_resize(session, rows, cols);
            respond_ok();
            return 0;
        }

        case CMD_INSERT_TEXT: {
            const char *text = json_object_get_string(cmd_obj, "text");
            if (!text) {
                respond_error("insert: missing 'text' parameter");
                return 0;
            }
            /* Send each character as a key event */
            for (const char *p = text; *p; p++) {
                EditorEvent event = {0};
                event.type = EVENT_KEY;
                event.data.key.keycode = (unsigned char)*p;
                editor_session_handle_event(session, &event);
            }
            respond_ok();
            return 0;
        }

        case CMD_QUIT:
            respond_ok();
            return 1;

        case CMD_UNKNOWN:
        default:
            respond_error("Unknown command");
            return 0;
    }
}

/* ======================= Main Entry Points ================================= */

int jsonrpc_run_single(const EditorConfig *config) {
    /* Read single line from stdin */
    char line[MAX_LINE];
    if (!fgets(line, sizeof(line), stdin)) {
        respond_error("No input");
        return 1;
    }

    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    /* Parse JSON */
    JsonValue cmd = json_parse(line);
    if (cmd.type == JSON_ERROR) {
        respond_error("Invalid JSON");
        return 1;
    }

    /* Create session */
    EditorSession *session = editor_session_new(config);
    if (!session) {
        json_value_free(&cmd);
        respond_error("Failed to create session");
        return 1;
    }

    /* Process command */
    process_command(session, &cmd);

    /* Cleanup */
    json_value_free(&cmd);
    editor_session_free(session);

    return 0;
}

int jsonrpc_run_interactive(const EditorConfig *config) {
    /* Create session */
    EditorSession *session = editor_session_new(config);
    if (!session) {
        respond_error("Failed to create session");
        return 1;
    }

    /* Read and process commands until quit or EOF */
    char line[MAX_LINE];
    int should_quit = 0;

    while (!should_quit && fgets(line, sizeof(line), stdin)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* Skip empty lines */
        if (len == 0 || (len == 1 && line[0] == '\0')) {
            continue;
        }

        /* Parse JSON */
        JsonValue cmd = json_parse(line);
        if (cmd.type == JSON_ERROR) {
            respond_error("Invalid JSON");
            continue;
        }

        /* Process command */
        should_quit = process_command(session, &cmd);
        json_value_free(&cmd);
    }

    /* Cleanup */
    editor_session_free(session);

    return 0;
}
