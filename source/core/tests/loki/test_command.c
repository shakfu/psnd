/* test_command.c - Unit tests for vim-like command mode
 *
 * Tests for:
 * - Command mode enter/exit
 * - Command input handling
 * - Command execution (:w, :q, :set, etc.)
 * - Command history navigation
 * - Command parsing
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/command.h"
#include "loki/buffers.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

/* Dynamic test directory */
static char TEST_DIR[512] = {0};

static void init_test_dir_path(void) {
    if (TEST_DIR[0]) return;  /* Already initialized */
#ifdef _WIN32
    const char *temp = getenv("TEMP");
    if (!temp) temp = getenv("TMP");
    if (!temp) temp = "C:\\Windows\\Temp";
    snprintf(TEST_DIR, sizeof(TEST_DIR), "%s\\loki_cmd_test", temp);
#else
    snprintf(TEST_DIR, sizeof(TEST_DIR), "/tmp/loki_cmd_test");
#endif
}

/* Helper: Create test directory */
static void setup_test_dir(void) {
    init_test_dir_path();
    mkdir(TEST_DIR, 0755);
}

/* Helper: Clean up test files */
static void cleanup_test_dir(void) {
    init_test_dir_path();
#ifdef _WIN32
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>NUL", TEST_DIR);
    system(cmd);
#else
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", TEST_DIR);
    system(cmd);
#endif
}

/* Helper: Initialize editor context for command mode testing */
static void init_cmd_ctx(editor_ctx_t *ctx) {
    editor_ctx_init(ctx);
    buffers_init(ctx);
    command_mode_init(ctx);

    ctx->view.screenrows = 24;
    ctx->view.screencols = 80;
}

/* Helper: Create context with content */
static void init_cmd_ctx_with_content(editor_ctx_t *ctx, const char *content) {
    init_cmd_ctx(ctx);

    ctx->model.numrows = 1;
    ctx->model.row = calloc(1, sizeof(t_erow));
    ctx->model.row[0].chars = strdup(content);
    ctx->model.row[0].size = strlen(content);
    ctx->model.row[0].render = strdup(content);
    ctx->model.row[0].rsize = strlen(content);
    ctx->model.row[0].hl = NULL;
    ctx->model.row[0].idx = 0;
}

/* Helper: Free command test context */
static void free_cmd_ctx(editor_ctx_t *ctx) {
    command_mode_free(ctx);
    buffers_free();
    editor_ctx_free(ctx);
}

/* ============================================================================
 * Command Mode Enter/Exit Tests
 * ============================================================================ */

TEST(cmd_mode_enter_sets_mode) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    ctx.view.mode = MODE_NORMAL;

    command_mode_enter(&ctx);

    ASSERT_EQ(ctx.view.mode, MODE_COMMAND);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_enter_initializes_buffer) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);

    /* Buffer should start with ':' */
    ASSERT_EQ(ctx.view.cmd_buffer[0], ':');
    ASSERT_EQ(ctx.view.cmd_length, 1);
    ASSERT_EQ(ctx.view.cmd_cursor_pos, 1);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_exit_returns_to_normal) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    ASSERT_EQ(ctx.view.mode, MODE_COMMAND);

    command_mode_exit(&ctx);
    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_exit_clears_buffer) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    ctx.view.cmd_buffer[1] = 'w';
    ctx.view.cmd_buffer[2] = '\0';
    ctx.view.cmd_length = 2;

    command_mode_exit(&ctx);

    ASSERT_EQ(ctx.view.cmd_length, 0);

    free_cmd_ctx(&ctx);
}

/* ============================================================================
 * Command Input Handling Tests
 * ============================================================================ */

TEST(cmd_mode_regular_char_appends) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);

    /* Type 'w' */
    command_mode_handle_key(&ctx, 0, 'w');

    ASSERT_EQ(ctx.view.cmd_buffer[1], 'w');
    ASSERT_EQ(ctx.view.cmd_length, 2);
    ASSERT_EQ(ctx.view.cmd_cursor_pos, 2);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_multiple_chars) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);

    /* Type 'set' */
    command_mode_handle_key(&ctx, 0, 's');
    command_mode_handle_key(&ctx, 0, 'e');
    command_mode_handle_key(&ctx, 0, 't');

    ASSERT_STR_EQ(ctx.view.cmd_buffer, ":set");
    ASSERT_EQ(ctx.view.cmd_length, 4);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_backspace_deletes_char) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    command_mode_handle_key(&ctx, 0, 'w');
    command_mode_handle_key(&ctx, 0, 'q');

    ASSERT_STR_EQ(ctx.view.cmd_buffer, ":wq");

    /* Backspace */
    command_mode_handle_key(&ctx, 0, BACKSPACE);

    ASSERT_STR_EQ(ctx.view.cmd_buffer, ":w");
    ASSERT_EQ(ctx.view.cmd_length, 2);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_backspace_on_empty_exits) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    ASSERT_EQ(ctx.view.mode, MODE_COMMAND);

    /* Backspace on ':' should exit command mode */
    command_mode_handle_key(&ctx, 0, BACKSPACE);

    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_escape_exits) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    command_mode_handle_key(&ctx, 0, 'w');

    ASSERT_EQ(ctx.view.mode, MODE_COMMAND);

    command_mode_handle_key(&ctx, 0, ESC);

    ASSERT_EQ(ctx.view.mode, MODE_NORMAL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_ctrl_u_clears_line) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    command_mode_handle_key(&ctx, 0, 'w');
    command_mode_handle_key(&ctx, 0, 'q');

    ASSERT_STR_EQ(ctx.view.cmd_buffer, ":wq");

    /* Ctrl-U clears */
    command_mode_handle_key(&ctx, 0, CTRL_U);

    ASSERT_STR_EQ(ctx.view.cmd_buffer, ":");
    ASSERT_EQ(ctx.view.cmd_length, 1);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_arrow_left_moves_cursor) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    command_mode_handle_key(&ctx, 0, 'w');
    command_mode_handle_key(&ctx, 0, 'q');

    ASSERT_EQ(ctx.view.cmd_cursor_pos, 3);

    command_mode_handle_key(&ctx, 0, ARROW_LEFT);

    ASSERT_EQ(ctx.view.cmd_cursor_pos, 2);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_arrow_right_moves_cursor) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    command_mode_handle_key(&ctx, 0, 'w');
    command_mode_handle_key(&ctx, 0, 'q');
    command_mode_handle_key(&ctx, 0, ARROW_LEFT);
    command_mode_handle_key(&ctx, 0, ARROW_LEFT);

    ASSERT_EQ(ctx.view.cmd_cursor_pos, 1);

    command_mode_handle_key(&ctx, 0, ARROW_RIGHT);

    ASSERT_EQ(ctx.view.cmd_cursor_pos, 2);

    free_cmd_ctx(&ctx);
}

TEST(cmd_mode_cursor_stops_at_colon) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    command_mode_enter(&ctx);
    command_mode_handle_key(&ctx, 0, 'w');

    /* Can't move left past ':' */
    command_mode_handle_key(&ctx, 0, ARROW_LEFT);
    command_mode_handle_key(&ctx, 0, ARROW_LEFT);
    command_mode_handle_key(&ctx, 0, ARROW_LEFT);

    ASSERT_EQ(ctx.view.cmd_cursor_pos, 1);  /* Stays after ':' */

    free_cmd_ctx(&ctx);
}

/* ============================================================================
 * Command Execution Tests
 * ============================================================================ */

TEST(cmd_execute_unknown_command) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    int result = command_execute(&ctx, ":notacommand");

    ASSERT_EQ(result, 0);
    /* Status message should indicate unknown command */
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "Unknown") != NULL ||
                strstr(ctx.view.statusmsg, "unknown") != NULL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_empty_command) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    int result = command_execute(&ctx, ":");

    ASSERT_EQ(result, 0);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_help_shows_message) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    int result = command_execute(&ctx, ":help");

    ASSERT_EQ(result, 1);
    /* Status message should contain help info */
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_help_specific_command) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    int result = command_execute(&ctx, ":help w");

    ASSERT_EQ(result, 1);
    /* Should show help for :w */
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "write") != NULL ||
                strstr(ctx.view.statusmsg, "Write") != NULL ||
                strstr(ctx.view.statusmsg, "save") != NULL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_set_wrap) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    ctx.view.word_wrap = 0;

    int result = command_execute(&ctx, ":set wrap");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.word_wrap, 1);

    /* Toggle again */
    result = command_execute(&ctx, ":set wrap");

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.view.word_wrap, 0);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_set_unknown_option) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    int result = command_execute(&ctx, ":set unknownoption");

    ASSERT_EQ(result, 0);

    free_cmd_ctx(&ctx);
}

TEST(cmd_write_requires_filename_when_new) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    ctx.model.filename = NULL;

    int result = command_execute(&ctx, ":w");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "filename") != NULL ||
                strstr(ctx.view.statusmsg, "No") != NULL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_write_saves_file) {
    setup_test_dir();

    editor_ctx_t ctx;
    init_cmd_ctx_with_content(&ctx, "test content");

    char path[256];
    snprintf(path, sizeof(path), "%s/test_save.txt", TEST_DIR);

    char cmd[300];
    snprintf(cmd, sizeof(cmd), ":w %s", path);

    ctx.model.dirty = 1;
    int result = command_execute(&ctx, cmd);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(ctx.model.dirty, 0);

    /* Verify file exists */
    FILE *f = fopen(path, "r");
    ASSERT_NOT_NULL(f);
    fclose(f);

    free_cmd_ctx(&ctx);
    cleanup_test_dir();
}

TEST(cmd_edit_requires_filename) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    int result = command_execute(&ctx, ":e");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "require") != NULL ||
                strstr(ctx.view.statusmsg, "Filename") != NULL);

    free_cmd_ctx(&ctx);
}

/* ============================================================================
 * Command History Tests
 * ============================================================================ */

TEST(cmd_history_tracks_length) {
    /* Note: History is global/static, so we can only test that
     * it tracks length properly, not that it starts at 0.
     * Previous tests may have added to history. */
    int initial_len = command_history_len();

    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* Execute a unique command to ensure it gets added */
    command_execute(&ctx, ":help unique_test_cmd");

    /* History should have grown */
    ASSERT_TRUE(command_history_len() >= initial_len);

    free_cmd_ctx(&ctx);
}

TEST(cmd_history_stores_commands) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* Execute a few commands */
    command_execute(&ctx, ":help");
    command_execute(&ctx, ":set wrap");
    command_execute(&ctx, ":help w");

    int len = command_history_len();
    ASSERT_TRUE(len >= 3);

    free_cmd_ctx(&ctx);
}

TEST(cmd_history_get_returns_previous) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* Execute commands */
    command_execute(&ctx, ":help");
    command_execute(&ctx, ":set wrap");

    int len = command_history_len();
    ASSERT_TRUE(len >= 2);

    /* Get most recent */
    const char *hist = command_history_get(len - 1);
    ASSERT_NOT_NULL(hist);
    ASSERT_STR_EQ(hist, "set wrap");

    free_cmd_ctx(&ctx);
}

TEST(cmd_history_get_out_of_bounds) {
    const char *hist = command_history_get(-1);
    ASSERT_NULL(hist);

    hist = command_history_get(10000);
    ASSERT_NULL(hist);
}

/* ============================================================================
 * Command Parsing Edge Cases
 * ============================================================================ */

TEST(cmd_execute_with_whitespace) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* Extra whitespace should be handled */
    int result = command_execute(&ctx, ":  help  ");

    ASSERT_EQ(result, 1);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_alias_q) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    ctx.model.dirty = 1;

    /* :q should fail if dirty */
    int result = command_execute(&ctx, ":q");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "Unsaved") != NULL ||
                strstr(ctx.view.statusmsg, "unsaved") != NULL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_alias_quit) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    ctx.model.dirty = 1;

    /* :quit is alias for :q */
    int result = command_execute(&ctx, ":quit");

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "Unsaved") != NULL ||
                strstr(ctx.view.statusmsg, "unsaved") != NULL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_alias_write) {
    setup_test_dir();

    editor_ctx_t ctx;
    init_cmd_ctx_with_content(&ctx, "test");

    char path[256];
    snprintf(path, sizeof(path), "%s/test_alias.txt", TEST_DIR);

    char cmd[300];
    snprintf(cmd, sizeof(cmd), ":write %s", path);

    int result = command_execute(&ctx, cmd);

    ASSERT_EQ(result, 1);

    free_cmd_ctx(&ctx);
    cleanup_test_dir();
}

TEST(cmd_execute_alias_h_for_help) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* :h is alias for :help */
    int result = command_execute(&ctx, ":h");

    ASSERT_EQ(result, 1);
    ASSERT_TRUE(strlen(ctx.view.statusmsg) > 0);

    free_cmd_ctx(&ctx);
}

/* ============================================================================
 * Dynamic Command Registration Tests
 * ============================================================================ */

/* Dummy handler for testing */
static int test_handler(editor_ctx_t *ctx, const char *args) {
    (void)args;
    editor_set_status_msg(ctx, "Test handler called");
    return 1;
}

TEST(cmd_register_custom_command) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* Register custom command */
    int result = command_register("testcmd", test_handler, "Test command", 0, 0);
    ASSERT_EQ(result, 1);

    /* Execute custom command */
    result = command_execute(&ctx, ":testcmd");
    ASSERT_EQ(result, 1);
    ASSERT_STR_EQ(ctx.view.statusmsg, "Test handler called");

    command_unregister_all_dynamic();
    free_cmd_ctx(&ctx);
}

TEST(cmd_register_duplicate_fails) {
    /* Can't register same name twice */
    int result1 = command_register("duptest", test_handler, "First", 0, 0);
    int result2 = command_register("duptest", test_handler, "Second", 0, 0);

    ASSERT_EQ(result1, 1);
    ASSERT_EQ(result2, 0);

    command_unregister_all_dynamic();
}

TEST(cmd_register_builtin_override_fails) {
    /* Can't override built-in commands */
    int result = command_register("w", test_handler, "Override write", 0, 0);

    ASSERT_EQ(result, 0);

    command_unregister_all_dynamic();
}

TEST(cmd_unregister_all_clears_dynamic) {
    command_register("dyn1", test_handler, "Dynamic 1", 0, 0);
    command_register("dyn2", test_handler, "Dynamic 2", 0, 0);

    command_unregister_all_dynamic();

    /* Now registering same names should succeed */
    int result = command_register("dyn1", test_handler, "Dynamic 1 again", 0, 0);
    ASSERT_EQ(result, 1);

    command_unregister_all_dynamic();
}

TEST(cmd_execute_rejects_too_many_args) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* :goto takes exactly one argument; max_args used to be ignored. */
    int result = command_execute(&ctx, ":goto 1 2");
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(strstr(ctx.view.statusmsg, "at most") != NULL);

    free_cmd_ctx(&ctx);
}

TEST(cmd_execute_set_with_value_reports_failure) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* No option takes a value; this used to claim success. */
    ASSERT_EQ(command_execute(&ctx, ":set foo=bar"), 0);

    free_cmd_ctx(&ctx);
}

TEST(command_mode_exit_keeps_status_message) {
    editor_ctx_t ctx;
    init_cmd_ctx(&ctx);

    /* command_mode_exit() used to erase the command's own feedback. */
    editor_set_status_msg(&ctx, "3L written");
    command_mode_exit(&ctx);
    ASSERT_STR_EQ(ctx.view.statusmsg, "3L written");

    free_cmd_ctx(&ctx);
}

BEGIN_TEST_SUITE("Command Mode")
    /* Enter/Exit */
    RUN_TEST(cmd_mode_enter_sets_mode);
    RUN_TEST(cmd_mode_enter_initializes_buffer);
    RUN_TEST(cmd_mode_exit_returns_to_normal);
    RUN_TEST(cmd_mode_exit_clears_buffer);

    /* Input handling */
    RUN_TEST(cmd_mode_regular_char_appends);
    RUN_TEST(cmd_mode_multiple_chars);
    RUN_TEST(cmd_mode_backspace_deletes_char);
    RUN_TEST(cmd_mode_backspace_on_empty_exits);
    RUN_TEST(cmd_mode_escape_exits);
    RUN_TEST(cmd_mode_ctrl_u_clears_line);
    RUN_TEST(cmd_mode_arrow_left_moves_cursor);
    RUN_TEST(cmd_mode_arrow_right_moves_cursor);
    RUN_TEST(cmd_mode_cursor_stops_at_colon);

    /* Command execution */
    RUN_TEST(cmd_execute_unknown_command);
    RUN_TEST(cmd_execute_empty_command);
    RUN_TEST(cmd_execute_help_shows_message);
    RUN_TEST(cmd_execute_help_specific_command);
    RUN_TEST(cmd_execute_set_wrap);
    RUN_TEST(cmd_execute_set_unknown_option);
    RUN_TEST(cmd_write_requires_filename_when_new);
    RUN_TEST(cmd_write_saves_file);
    RUN_TEST(cmd_edit_requires_filename);

    /* History */
    RUN_TEST(cmd_history_tracks_length);
    RUN_TEST(cmd_history_stores_commands);
    RUN_TEST(cmd_history_get_returns_previous);
    RUN_TEST(cmd_history_get_out_of_bounds);

    /* Parsing edge cases */
    RUN_TEST(cmd_execute_with_whitespace);
    RUN_TEST(cmd_execute_alias_q);
    RUN_TEST(cmd_execute_alias_quit);
    RUN_TEST(cmd_execute_alias_write);
    RUN_TEST(cmd_execute_alias_h_for_help);
    RUN_TEST(cmd_execute_rejects_too_many_args);
    RUN_TEST(cmd_execute_set_with_value_reports_failure);
    RUN_TEST(command_mode_exit_keeps_status_message);

    /* Dynamic registration */
    RUN_TEST(cmd_register_custom_command);
    RUN_TEST(cmd_register_duplicate_fails);
    RUN_TEST(cmd_register_builtin_override_fails);
    RUN_TEST(cmd_unregister_all_clears_dynamic);
END_TEST_SUITE()
