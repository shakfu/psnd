/* test_buffers.c - Unit tests for multiple buffer management
 *
 * Tests buffer creation, switching, closing, and tab rendering.
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/buffers.h"
#include <stdio.h>
#include <string.h>

/* Helper: Initialize a minimal editor context for testing */
static void init_test_context(editor_ctx_t *ctx) {
    memset(ctx, 0, sizeof(editor_ctx_t));
    ctx->view.cx = 0;
    ctx->view.cy = 0;
    ctx->view.rowoff = 0;
    ctx->view.coloff = 0;
    ctx->model.numrows = 0;
    ctx->model.row = NULL;
    ctx->model.dirty = 0;
    ctx->model.filename = NULL;
    ctx->view.syntax = NULL;
    ctx->view.mode = MODE_NORMAL;
    ctx->view.screencols = 80;
    ctx->view.screenrows = 24;
}

/* Test: Buffer initialization */
TEST(buffer_init) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    ASSERT_EQ(buffers_init(&ctx), 0);
    ASSERT_EQ(buffer_count(), 1);
    ASSERT_NOT_NULL(buffer_get_current());
    ASSERT_TRUE(buffer_get_current_id() >= 0);

    buffers_free();
}

/* Test: Buffer creation */
TEST(buffer_create) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);

    ASSERT_TRUE(id2 > 0);
    ASSERT_NEQ(id2, id1);
    ASSERT_EQ(buffer_count(), 2);

    buffers_free();
}

/* Test: Buffer switching */
TEST(buffer_switch) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);

    ASSERT_EQ(buffer_switch(id2), 0);
    ASSERT_EQ(buffer_get_current_id(), id2);

    ASSERT_EQ(buffer_switch(id1), 0);
    ASSERT_EQ(buffer_get_current_id(), id1);

    buffers_free();
}

/* Test: Buffer next/previous navigation */
TEST(buffer_navigation) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);
    int id3 = buffer_create(NULL);

    buffer_switch(id1);  /* Start at buffer 1 */

    int next = buffer_next();
    ASSERT_EQ(next, id2);

    next = buffer_next();
    ASSERT_EQ(next, id3);

    next = buffer_next();
    ASSERT_EQ(next, id1);

    int prev = buffer_prev();
    ASSERT_EQ(prev, id3);

    buffers_free();
}

/* Test: Buffer closing */
TEST(buffer_close) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);
    int id3 = buffer_create(NULL);

    ASSERT_EQ(buffer_count(), 3);

    /* Close buffer 2 */
    buffer_switch(id2);
    ASSERT_EQ(buffer_close(id2, 0), 0);
    ASSERT_EQ(buffer_count(), 2);

    /* Current buffer should have switched */
    int current = buffer_get_current_id();
    ASSERT_NEQ(current, id2);

    /* Cannot close last buffer */
    buffer_switch(id1);
    buffer_close(id3, 0);
    ASSERT_NEQ(buffer_close(id1, 0), 0);

    buffers_free();
}

/* Test: Buffer modified state */
TEST(buffer_modified) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id = buffer_get_current_id();
    editor_ctx_t *buf_ctx = buffer_get_current();

    ASSERT_EQ(buffer_is_modified(id), 0);

    /* Mark buffer as modified */
    buf_ctx->model.dirty = 1;

    ASSERT_EQ(buffer_is_modified(id), 1);

    /* Try to close modified buffer without force */
    ASSERT_NEQ(buffer_close(id, 0), 0);

    buffers_free();
}

/* Test: Buffer display name */
TEST(buffer_display_name) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id = buffer_get_current_id();
    const char *name = buffer_get_display_name(id);

    ASSERT_NOT_NULL(name);
    ASSERT_STR_EQ(name, "[No Name]");

    buffers_free();
}

/* Test: Buffer list */
TEST(buffer_list) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);
    int id3 = buffer_create(NULL);

    int ids[MAX_BUFFERS];
    int count = buffer_get_list(ids);

    ASSERT_EQ(count, 3);
    ASSERT_EQ(ids[0], id1);
    ASSERT_EQ(ids[1], id2);
    ASSERT_EQ(ids[2], id3);

    buffers_free();
}

/* Test: Maximum buffers limit */
TEST(buffer_limit) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    /* Create MAX_BUFFERS - 1 more buffers (already have 1) */
    for (int i = 1; i < MAX_BUFFERS; i++) {
        int id = buffer_create(NULL);
        ASSERT_TRUE(id > 0);
    }

    ASSERT_EQ(buffer_count(), MAX_BUFFERS);

    /* Try to create one more - should fail */
    int overflow_id = buffer_create(NULL);
    ASSERT_EQ(overflow_id, -1);
    ASSERT_EQ(buffer_count(), MAX_BUFFERS);

    buffers_free();
}

/* Test: Buffer tab rendering */
TEST(buffer_tabs_rendering) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    /* Single buffer - should not render tabs */
    struct abuf ab = {NULL, 0};
    buffers_render_tabs(&ab, 80);
    ASSERT_EQ(ab.len, 0);

    /* Multiple buffers - should render tabs */
    buffer_create(NULL);
    buffer_create(NULL);

    ab.b = NULL;
    ab.len = 0;
    buffers_render_tabs(&ab, 80);
    ASSERT_TRUE(ab.len > 0);

    if (ab.b) free(ab.b);

    buffers_free();
}

/* Test: Command-style buffer creation and population
 * This simulates what :plugin presets does:
 * 1. Create new buffer
 * 2. Switch to it
 * 3. Add content
 * 4. Verify buffer is current with correct content */
TEST(buffer_command_workflow) {
    editor_ctx_t ctx;
    init_test_context(&ctx);
    ctx.view.screencols = 80;
    ctx.view.screenrows = 24;
    ctx.view.screenrows_total = 24;

    buffers_init(&ctx);

    int original_id = buffer_get_current_id();
    editor_ctx_t *original_ctx = buffer_get_current();
    ASSERT_NOT_NULL(original_ctx);

    /* Simulate :plugin presets command workflow */

    /* 1. Create new buffer */
    int new_id = buffer_create(NULL);
    ASSERT_TRUE(new_id >= 0);
    ASSERT_NEQ(new_id, original_id);

    /* 2. Switch to new buffer */
    int switch_result = buffer_switch(new_id);
    ASSERT_EQ(switch_result, 0);
    ASSERT_EQ(buffer_get_current_id(), new_id);

    /* 3. Get new buffer context */
    editor_ctx_t *new_ctx = buffer_get_current();
    ASSERT_NOT_NULL(new_ctx);
    ASSERT_TRUE(new_ctx != original_ctx);

    /* Verify screen dimensions were copied */
    ASSERT_EQ(new_ctx->view.screencols, 80);
    ASSERT_EQ(new_ctx->view.screenrows, 24);

    /* 4. Clear initial empty row and add content (like plugin.c does) */
    if (new_ctx->model.numrows > 0) {
        editor_del_row(new_ctx, 0);
    }
    ASSERT_EQ(new_ctx->model.numrows, 0);

    /* Add test content */
    editor_insert_row(new_ctx, 0, "Header line", 11);
    editor_insert_row(new_ctx, 1, "Content line 1", 14);
    editor_insert_row(new_ctx, 2, "Content line 2", 14);

    ASSERT_EQ(new_ctx->model.numrows, 3);

    /* Verify content */
    ASSERT_NOT_NULL(new_ctx->model.row);
    ASSERT_NOT_NULL(new_ctx->model.row[0].chars);
    ASSERT_STR_EQ(new_ctx->model.row[0].chars, "Header line");

    /* 5. Set mode and verify buffer is still current */
    new_ctx->view.mode = MODE_NORMAL;
    ASSERT_EQ(buffer_get_current_id(), new_id);

    /* 6. Verify buffer_get_current returns correct context with content */
    editor_ctx_t *verify_ctx = buffer_get_current();
    ASSERT_TRUE(verify_ctx == new_ctx);
    ASSERT_EQ(verify_ctx->model.numrows, 3);

    buffers_free();
}

/* Test suite */
/* Test: a path that does not exist yet opens as an empty, named buffer.
 * It used to fail, so ':e newfile' could not create a file. */
TEST(buffer_create_nonexistent_file) {
    editor_ctx_t ctx;
    init_test_context(&ctx);
    buffers_init(&ctx);

    const char *path = "psnd_test_buffer_does_not_exist.txt";
    remove(path);
    int id = buffer_create(path);
    ASSERT_TRUE(id > 0);

    editor_ctx_t *b = buffer_get(id);
    ASSERT_NOT_NULL(b);
    ASSERT_EQ(b->model.numrows, 1);
    ASSERT_EQ(b->model.dirty, 0);
    ASSERT_STR_EQ(b->model.filename, path);

    buffers_free();
}

/* Test: new buffers inherit word_wrap from the current one */
TEST(buffer_create_inherits_word_wrap) {
    editor_ctx_t ctx;
    init_test_context(&ctx);
    ctx.view.word_wrap = 1;
    buffers_init(&ctx);
    ASSERT_EQ(buffer_get_current()->view.word_wrap, 1);

    int id = buffer_create(NULL);
    ASSERT_EQ(buffer_get(id)->view.word_wrap, 1);

    buffers_free();
}

BEGIN_TEST_SUITE("Buffer Management")
    RUN_TEST(buffer_init);
    RUN_TEST(buffer_create);
    RUN_TEST(buffer_switch);
    RUN_TEST(buffer_navigation);
    RUN_TEST(buffer_close);
    RUN_TEST(buffer_modified);
    RUN_TEST(buffer_display_name);
    RUN_TEST(buffer_list);
    RUN_TEST(buffer_limit);
    RUN_TEST(buffer_tabs_rendering);
    RUN_TEST(buffer_command_workflow);
    RUN_TEST(buffer_create_nonexistent_file);
    RUN_TEST(buffer_create_inherits_word_wrap);
END_TEST_SUITE()
