/* test_lua_api.c - Integration tests for Lua API
 *
 * Tests for:
 * - Lua state initialization
 * - loki.status() function
 * - loki.get_lines() function
 * - loki.get_line() function
 * - loki.get_cursor() function
 * - loki.insert_text() function
 * - loki.get_filename() function
 * - loki.set_color() function
 * - loki.register_language() function
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/lua.h"
#include "loki/internal.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* Helper to initialize context with Lua */
static void init_ctx_with_lua(editor_ctx_t *ctx) {
    editor_ctx_init(ctx);
    LuaHost *lua_host = lua_host_create();
    if (lua_host) {
        ctx->lua_host = lua_host;
        lua_host->L = loki_lua_bootstrap(ctx, NULL);
    }
}

/* Helper to cleanup context with Lua */
static void free_ctx_with_lua(editor_ctx_t *ctx) {
    if (ctx->lua_host) {
        lua_host_free(ctx->lua_host);
        ctx->lua_host = NULL;
    }
    editor_ctx_free(ctx);
}

/* Test Lua state initialization */
TEST(lua_state_initializes) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    ASSERT_NOT_NULL(ctx_L(&ctx));

    /* Verify loki table exists */
    lua_getglobal(ctx_L(&ctx), "loki");
    ASSERT_TRUE(lua_istable(ctx_L(&ctx), -1));
    lua_pop(ctx_L(&ctx), 1);

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.status() function */
TEST(lua_status_sets_message) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Call loki.status("Test message") */
    const char *code = "loki.status('Test message')";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_STR_EQ(ctx.view.statusmsg, "Test message");

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.get_lines() function */
TEST(lua_get_lines_returns_count) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Add 3 rows */
    ctx.model.numrows = 3;
    ctx.model.row = calloc(3, sizeof(t_erow));
    for (int i = 0; i < 3; i++) {
        ctx.model.row[i].chars = strdup("test");
        ctx.model.row[i].size = 4;
        ctx.model.row[i].idx = i;
    }

    /* Call loki.get_lines() */
    const char *code = "return loki.get_lines()";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isnumber(ctx_L(&ctx), -1));
    ASSERT_EQ((int)lua_tonumber(ctx_L(&ctx), -1), 3);

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.get_line() function */
TEST(lua_get_line_returns_content) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Add 2 rows */
    ctx.model.numrows = 2;
    ctx.model.row = calloc(2, sizeof(t_erow));

    ctx.model.row[0].chars = strdup("First line");
    ctx.model.row[0].size = 10;
    ctx.model.row[0].idx = 0;

    ctx.model.row[1].chars = strdup("Second line");
    ctx.model.row[1].size = 11;
    ctx.model.row[1].idx = 1;

    /* Call loki.get_line(0) */
    const char *code1 = "return loki.get_line(0)";
    int result1 = luaL_dostring(ctx_L(&ctx), code1);

    ASSERT_EQ(result1, 0);
    ASSERT_TRUE(lua_isstring(ctx_L(&ctx), -1));
    ASSERT_STR_EQ(lua_tostring(ctx_L(&ctx), -1), "First line");
    lua_pop(ctx_L(&ctx), 1);

    /* Call loki.get_line(1) */
    const char *code2 = "return loki.get_line(1)";
    int result2 = luaL_dostring(ctx_L(&ctx), code2);

    ASSERT_EQ(result2, 0);
    ASSERT_TRUE(lua_isstring(ctx_L(&ctx), -1));
    ASSERT_STR_EQ(lua_tostring(ctx_L(&ctx), -1), "Second line");

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.get_line() with out of bounds */
TEST(lua_get_line_handles_out_of_bounds) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* No rows */
    ctx.model.numrows = 0;

    /* Call loki.get_line(0) should return nil */
    const char *code = "return loki.get_line(0)";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isnil(ctx_L(&ctx), -1));

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.get_cursor() function */
TEST(lua_get_cursor_returns_position) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Set cursor position */
    ctx.view.cx = 5;
    ctx.view.cy = 10;

    /* Call loki.get_cursor() */
    const char *code = "local row, col = loki.get_cursor(); return row, col";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isnumber(ctx_L(&ctx), -2));
    ASSERT_TRUE(lua_isnumber(ctx_L(&ctx), -1));
    ASSERT_EQ((int)lua_tonumber(ctx_L(&ctx), -2), 10);  /* row */
    ASSERT_EQ((int)lua_tonumber(ctx_L(&ctx), -1), 5);   /* col */

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.insert_text() function */
TEST(lua_insert_text_adds_content) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Create empty row */
    ctx.model.numrows = 1;
    ctx.model.row = calloc(1, sizeof(t_erow));
    ctx.model.row[0].chars = malloc(1);
    ctx.model.row[0].chars[0] = '\0';
    ctx.model.row[0].size = 0;
    ctx.model.row[0].render = NULL;
    ctx.model.row[0].hl = NULL;
    ctx.model.row[0].rsize = 0;
    ctx.model.row[0].idx = 0;

    ctx.view.cx = 0;
    ctx.view.cy = 0;

    /* Call loki.insert_text("Hello") */
    const char *code = "loki.insert_text('Hello')";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.row[0].size, 5);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "Hello");
    ASSERT_EQ(ctx.model.dirty, 5);  /* dirty increments per character */

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.get_filename() function */
TEST(lua_get_filename_returns_name) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Set filename */
    ctx.model.filename = strdup("/tmp/test.txt");

    /* Call loki.get_filename() */
    const char *code = "return loki.get_filename()";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isstring(ctx_L(&ctx), -1));
    ASSERT_STR_EQ(lua_tostring(ctx_L(&ctx), -1), "/tmp/test.txt");

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.get_filename() with no file */
TEST(lua_get_filename_returns_nil_when_no_file) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* No filename set */
    ctx.model.filename = NULL;

    /* Call loki.get_filename() */
    const char *code = "return loki.get_filename()";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isnil(ctx_L(&ctx), -1));

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.set_color() function */
TEST(lua_set_color_updates_colors) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Call loki.set_color("keyword1", {r=255, g=0, b=0}) */
    const char *code = "loki.set_color('keyword1', {r=255, g=0, b=0})";
    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);

    /* Verify color was set (would need to check internal state) */
    /* For now, just verify no Lua error occurred */

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test loki.register_language() function */
TEST(lua_register_language_adds_syntax) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Register a simple language */
    const char *code =
        "loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  keywords = {'if', 'then', 'else'},\n"
        "  types = {'int', 'string'},\n"
        "  line_comment = '#',\n"
        "  separators = ',.()+-',\n"
        "  highlight_strings = true,\n"
        "  highlight_numbers = true\n"
        "})";

    int result = luaL_dostring(ctx_L(&ctx), code);

    ASSERT_EQ(result, 0);

    /* Verify language was registered */
    /* Would need to check HLDB or internal state */
    /* For now, just verify no Lua error */

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Test Lua error handling */
TEST(lua_handles_syntax_errors) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);

    /* Invalid Lua code */
    const char *code = "this is not valid lua syntax !!!";
    int result = luaL_dostring(ctx_L(&ctx), code);

    /* Should return error */
    ASSERT_NEQ(result, 0);

    /* Cleanup */
    free_ctx_with_lua(&ctx);
}

/* Note: Test for context storage in Lua registry removed as it tests
 * internal implementation details. The editor_ctx_registry_key is a
 * static variable in loki_lua.c and not exposed to tests. The fact that
 * Lua API functions work correctly (tested above) is sufficient proof
 * that context storage works. */

#ifdef LUA_SANDBOX
/* The sandbox must not be able to load native code: package.loadlib and the
 * C searchers would bypass the removed os/io libraries. */
TEST(lua_sandbox_blocks_native_loaders) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);
    lua_State *L = ctx_L(&ctx);
    ASSERT_NOT_NULL(L);

    ASSERT_EQ(luaL_dostring(L,
        "return package.loadlib == nil and package.cpath == '' "
        "and #package.searchers == 2 and os == nil and io == nil"), LUA_OK);
    ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);

    free_ctx_with_lua(&ctx);
}
#endif

/* loki.register_command() callbacks are dispatched in normal mode. They
 * used to be stored and never called. */
TEST(lua_register_command_dispatches_in_normal_mode) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);
    lua_State *L = ctx_L(&ctx);
    ASSERT_NOT_NULL(L);
    ctx.view.mode = MODE_NORMAL;

    ASSERT_EQ(luaL_dostring(L,
        "loki.register_command('Z', function() HITS = (HITS or 0) + 1 end)"), LUA_OK);
    modal_process_normal_mode_key(&ctx, 0, 'Z');

    lua_getglobal(L, "HITS");
    ASSERT_EQ((int)lua_tointeger(L, -1), 1);
    lua_pop(L, 1);

    free_ctx_with_lua(&ctx);
}

TEST(lua_register_command_reports_errors) {
    editor_ctx_t ctx;
    init_ctx_with_lua(&ctx);
    lua_State *L = ctx_L(&ctx);
    ASSERT_NOT_NULL(L);
    ctx.view.mode = MODE_NORMAL;

    ASSERT_EQ(luaL_dostring(L,
        "loki.register_command('Z', function() error('boom') end)"), LUA_OK);
    modal_process_normal_mode_key(&ctx, 0, 'Z');
    ASSERT_TRUE(strncmp(ctx.view.statusmsg, "Command error:", 14) == 0);
    ASSERT_EQ(lua_gettop(L), 0);   /* stack left balanced */

    free_ctx_with_lua(&ctx);
}

BEGIN_TEST_SUITE("Lua API Integration")
    RUN_TEST(lua_state_initializes);
    RUN_TEST(lua_status_sets_message);
    RUN_TEST(lua_get_lines_returns_count);
    RUN_TEST(lua_get_line_returns_content);
    RUN_TEST(lua_get_line_handles_out_of_bounds);
    RUN_TEST(lua_get_cursor_returns_position);
    RUN_TEST(lua_insert_text_adds_content);
    RUN_TEST(lua_get_filename_returns_name);
    RUN_TEST(lua_get_filename_returns_nil_when_no_file);
    RUN_TEST(lua_set_color_updates_colors);
    RUN_TEST(lua_register_language_adds_syntax);
    RUN_TEST(lua_handles_syntax_errors);
#ifdef LUA_SANDBOX
    RUN_TEST(lua_sandbox_blocks_native_loaders);
#endif
    RUN_TEST(lua_register_command_dispatches_in_normal_mode);
    RUN_TEST(lua_register_command_reports_errors);
END_TEST_SUITE()
