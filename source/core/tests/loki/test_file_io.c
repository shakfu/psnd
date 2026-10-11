/* test_file_io.c - Integration tests for file I/O
 *
 * Tests for:
 * - File loading
 * - File saving
 * - Binary file detection
 * - CRLF handling
 * - Large file handling
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
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
static char TEST_FILE_DIR[512] = {0};

static void init_test_dir_path(void) {
    if (TEST_FILE_DIR[0]) return;  /* Already initialized */
#ifdef _WIN32
    const char *temp = getenv("TEMP");
    if (!temp) temp = getenv("TMP");
    if (!temp) temp = "C:\\Windows\\Temp";
    snprintf(TEST_FILE_DIR, sizeof(TEST_FILE_DIR), "%s\\loki_test", temp);
#else
    snprintf(TEST_FILE_DIR, sizeof(TEST_FILE_DIR), "/tmp/loki_test");
#endif
}

/* Setup: Create test directory */
static void setup_test_dir(void) {
    init_test_dir_path();
    mkdir(TEST_FILE_DIR, 0755);
}

/* Teardown: Clean up test files */
static void cleanup_test_files(void) {
    init_test_dir_path();
#ifdef _WIN32
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>NUL", TEST_FILE_DIR);
    system(cmd);
#else
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", TEST_FILE_DIR);
    system(cmd);
#endif
}

/* Helper: Create a test file with given content */
static void create_test_file(const char *filename, const char *content) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", TEST_FILE_DIR, filename);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

/* Helper: Read file content */
static char *read_test_file(const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", TEST_FILE_DIR, filename);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (content) {
        size_t read = fread(content, 1, size, f);
        content[read] = '\0';
    }
    fclose(f);
    return content;
}

/* Test loading a simple text file */
TEST(editor_open_loads_simple_file) {
    setup_test_dir();
    create_test_file("simple.txt", "Hello\nWorld\n");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/simple.txt", TEST_FILE_DIR);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 2);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "Hello");
    ASSERT_STR_EQ(ctx.model.row[1].chars, "World");
    ASSERT_EQ(ctx.model.dirty, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test loading file with CRLF line endings */
TEST(editor_open_handles_crlf) {
    setup_test_dir();
    create_test_file("crlf.txt", "Line1\r\nLine2\r\nLine3\r\n");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/crlf.txt", TEST_FILE_DIR);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 3);
    /* Should strip both \r and \n */
    ASSERT_STR_EQ(ctx.model.row[0].chars, "Line1");
    ASSERT_STR_EQ(ctx.model.row[1].chars, "Line2");
    ASSERT_STR_EQ(ctx.model.row[2].chars, "Line3");

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test binary file detection */
TEST(editor_open_rejects_binary_file) {
    setup_test_dir();

    /* Create binary file with null bytes */
    char path[256];
    snprintf(path, sizeof(path), "%s/binary.bin", TEST_FILE_DIR);
    FILE *f = fopen(path, "wb");
    if (f) {
        char binary_data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE};
        fwrite(binary_data, 1, sizeof(binary_data), f);
        fclose(f);
    }

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, path);

    /* Should reject binary file */
    ASSERT_NEQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test empty file */
TEST(editor_open_handles_empty_file) {
    setup_test_dir();
    create_test_file("empty.txt", "");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/empty.txt", TEST_FILE_DIR);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test saving file */
TEST(editor_save_writes_content) {
    setup_test_dir();

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* Set up context with 2 rows */
    ctx.model.numrows = 2;
    ctx.model.row = calloc(2, sizeof(t_erow));

    ctx.model.row[0].chars = strdup("First line");
    ctx.model.row[0].size = 10;
    ctx.model.row[0].idx = 0;

    ctx.model.row[1].chars = strdup("Second line");
    ctx.model.row[1].size = 11;
    ctx.model.row[1].idx = 1;

    char path[256];
    snprintf(path, sizeof(path), "%s/output.txt", TEST_FILE_DIR);
    ctx.model.filename = strdup(path);
    ctx.model.dirty = 1;

    int result = editor_save(&ctx);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.dirty, 0);

    /* Verify file content */
    char *content = read_test_file("output.txt");
    ASSERT_NOT_NULL(content);
    ASSERT_STR_EQ(content, "First line\nSecond line\n");

    free(content);
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test file with no trailing newline */
TEST(editor_open_handles_no_trailing_newline) {
    setup_test_dir();

    char path[256];
    snprintf(path, sizeof(path), "%s/no_newline.txt", TEST_FILE_DIR);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs("Line without newline", f);  /* No \n at end */
        fclose(f);
    }

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "Line without newline");

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test loading nonexistent file */
TEST(editor_open_handles_nonexistent_file) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, "/nonexistent/path/to/file.txt");

    /* Should fail gracefully */
    ASSERT_NEQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
}

/* Test loading file with long lines */
TEST(editor_open_handles_long_lines) {
    setup_test_dir();

    /* Create file with a very long line */
    char path[256];
    snprintf(path, sizeof(path), "%s/long_line.txt", TEST_FILE_DIR);
    FILE *f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < 1000; i++) {
            fputc('a', f);
        }
        fputc('\n', f);
        fclose(f);
    }

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_EQ(ctx.model.row[0].size, 1000);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Opening a second file must replace the buffer, not append to it.
 * Regression: ':e other' used to leave both files' lines in the buffer and
 * then clear the dirty flag, marking the mixture as saved. */
TEST(editor_open_replaces_existing_buffer) {
    setup_test_dir();
    create_test_file("first.txt", "AAA\nBBB\n");
    create_test_file("second.txt", "CCC\n");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/first.txt", TEST_FILE_DIR);
    ASSERT_EQ(editor_open(&ctx, path), 0);
    ASSERT_EQ(ctx.model.numrows, 2);

    snprintf(path, sizeof(path), "%s/second.txt", TEST_FILE_DIR);
    ASSERT_EQ(editor_open(&ctx, path), 0);

    ASSERT_EQ(ctx.model.numrows, 1);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "CCC");
    ASSERT_EQ(ctx.model.row[0].idx, 0);
    ASSERT_EQ(ctx.model.dirty, 0);

    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* editor_insert_row() copied len + 1 bytes, so a substring argument dragged
 * in the byte after it. */
TEST(editor_insert_row_copies_only_len_bytes) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    editor_insert_row(&ctx, 0, "hello world", 5);
    ASSERT_EQ(ctx.model.row[0].size, 5);
    ASSERT_STR_EQ(ctx.model.row[0].chars, "hello");

    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("File I/O Integration")
    RUN_TEST(editor_open_loads_simple_file);
    RUN_TEST(editor_open_handles_crlf);
    RUN_TEST(editor_open_rejects_binary_file);
    RUN_TEST(editor_open_handles_empty_file);
    RUN_TEST(editor_save_writes_content);
    RUN_TEST(editor_open_handles_no_trailing_newline);
    RUN_TEST(editor_open_handles_nonexistent_file);
    RUN_TEST(editor_open_handles_long_lines);
    RUN_TEST(editor_open_replaces_existing_buffer);
    RUN_TEST(editor_insert_row_copies_only_len_bytes);
END_TEST_SUITE()
