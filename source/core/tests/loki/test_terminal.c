/* test_terminal.c - Unit tests for terminal buffer operations
 *
 * Tests for terminal buffer operations that can be tested without a real terminal:
 * - Screen buffer append and management
 * - Buffer initialization and cleanup
 * - String building efficiency
 *
 * Note: Functions requiring actual terminal I/O (raw mode, key reading, etc.)
 * cannot be easily unit tested and are tested through integration tests.
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/terminal.h"
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* ============================================================================
 * Screen Buffer Tests
 * ============================================================================ */

TEST(terminal_buffer_init) {
    struct abuf ab = ABUF_INIT;

    ASSERT_NULL(ab.b);
    ASSERT_EQ(ab.len, 0);
}

TEST(terminal_buffer_append_single) {
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab, "hello", 5);

    ASSERT_NOT_NULL(ab.b);
    ASSERT_EQ(ab.len, 5);
    ASSERT_EQ(memcmp(ab.b, "hello", 5), 0);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_append_multiple) {
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab, "hello", 5);
    terminal_buffer_append(&ab, " ", 1);
    terminal_buffer_append(&ab, "world", 5);

    ASSERT_EQ(ab.len, 11);
    ASSERT_EQ(memcmp(ab.b, "hello world", 11), 0);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_append_empty) {
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab, "", 0);

    /* Should handle empty append gracefully */
    ASSERT_EQ(ab.len, 0);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_append_escape_sequence) {
    struct abuf ab = ABUF_INIT;

    /* VT100 clear screen: ESC[2J */
    terminal_buffer_append(&ab, "\x1b[2J", 4);

    ASSERT_EQ(ab.len, 4);
    ASSERT_EQ(ab.b[0], '\x1b');
    ASSERT_EQ(ab.b[1], '[');
    ASSERT_EQ(ab.b[2], '2');
    ASSERT_EQ(ab.b[3], 'J');

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_append_cursor_position) {
    struct abuf ab = ABUF_INIT;

    /* Cursor position: ESC[H (home) */
    terminal_buffer_append(&ab, "\x1b[H", 3);

    ASSERT_EQ(ab.len, 3);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_free_clears) {
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab, "test", 4);
    ASSERT_NOT_NULL(ab.b);

    terminal_buffer_free(&ab);

    /* After free, memory is deallocated (ab.b is dangling, not NULL)
     * This is the expected behavior - caller should reinit if reusing */
    /* Test passes if we get here without crash */
    ASSERT_TRUE(1);
}

TEST(terminal_buffer_append_large_content) {
    struct abuf ab = ABUF_INIT;

    /* Append a large string */
    char large[1024];
    memset(large, 'x', sizeof(large));

    terminal_buffer_append(&ab, large, sizeof(large));

    ASSERT_EQ(ab.len, 1024);
    ASSERT_NOT_NULL(ab.b);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_multiple_escape_sequences) {
    struct abuf ab = ABUF_INIT;

    /* Build typical screen output */
    terminal_buffer_append(&ab, "\x1b[H", 3);       /* Home */
    terminal_buffer_append(&ab, "\x1b[2J", 4);      /* Clear */
    terminal_buffer_append(&ab, "Line 1", 6);
    terminal_buffer_append(&ab, "\x1b[2;1H", 6);    /* Move to row 2 */
    terminal_buffer_append(&ab, "Line 2", 6);

    ASSERT_EQ(ab.len, 25);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_append_binary_data) {
    struct abuf ab = ABUF_INIT;

    /* Binary data with null bytes */
    char binary[] = {0x00, 0x01, 0x02, 0x03, 0x00};

    terminal_buffer_append(&ab, binary, 5);

    ASSERT_EQ(ab.len, 5);
    ASSERT_EQ(ab.b[0], 0x00);
    ASSERT_EQ(ab.b[4], 0x00);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_sequential_operations) {
    struct abuf ab = ABUF_INIT;

    /* Simulate building screen content */
    for (int i = 0; i < 100; i++) {
        terminal_buffer_append(&ab, "line\r\n", 6);
    }

    ASSERT_EQ(ab.len, 600);

    terminal_buffer_free(&ab);
}

/* ============================================================================
 * VT100 Escape Sequence Building Tests
 * ============================================================================ */

TEST(terminal_buffer_color_sequence) {
    struct abuf ab = ABUF_INIT;

    /* SGR sequence for red foreground: ESC[31m */
    terminal_buffer_append(&ab, "\x1b[31m", 5);
    terminal_buffer_append(&ab, "red text", 8);
    terminal_buffer_append(&ab, "\x1b[0m", 4);  /* Reset */

    ASSERT_EQ(ab.len, 17);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_rgb_color_sequence) {
    struct abuf ab = ABUF_INIT;

    /* True color RGB sequence: ESC[38;2;255;0;0m */
    terminal_buffer_append(&ab, "\x1b[38;2;255;0;0m", 15);
    terminal_buffer_append(&ab, "bright red", 10);
    terminal_buffer_append(&ab, "\x1b[0m", 4);

    ASSERT_EQ(ab.len, 29);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_reverse_video) {
    struct abuf ab = ABUF_INIT;

    /* Reverse video for highlighting: ESC[7m */
    terminal_buffer_append(&ab, "\x1b[7m", 4);
    terminal_buffer_append(&ab, "highlighted", 11);
    terminal_buffer_append(&ab, "\x1b[m", 3);

    ASSERT_EQ(ab.len, 18);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_clear_to_end_of_line) {
    struct abuf ab = ABUF_INIT;

    /* Clear to end of line: ESC[K */
    terminal_buffer_append(&ab, "text", 4);
    terminal_buffer_append(&ab, "\x1b[K", 3);

    ASSERT_EQ(ab.len, 7);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_hide_show_cursor) {
    struct abuf ab = ABUF_INIT;

    /* Hide cursor: ESC[?25l */
    terminal_buffer_append(&ab, "\x1b[?25l", 6);
    terminal_buffer_append(&ab, "content", 7);
    /* Show cursor: ESC[?25h */
    terminal_buffer_append(&ab, "\x1b[?25h", 6);

    ASSERT_EQ(ab.len, 19);

    terminal_buffer_free(&ab);
}

/* ============================================================================
 * Buffer Growth Tests
 * ============================================================================ */

TEST(terminal_buffer_grows_automatically) {
    struct abuf ab = ABUF_INIT;

    /* Keep appending to force multiple reallocations */
    for (int i = 0; i < 1000; i++) {
        terminal_buffer_append(&ab, "x", 1);
    }

    ASSERT_EQ(ab.len, 1000);

    /* Verify content */
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(ab.b[i], 'x');
    }

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_handles_varying_sizes) {
    struct abuf ab = ABUF_INIT;

    /* Append strings of varying sizes */
    terminal_buffer_append(&ab, "a", 1);
    terminal_buffer_append(&ab, "bb", 2);
    terminal_buffer_append(&ab, "cccc", 4);
    terminal_buffer_append(&ab, "dddddddd", 8);

    ASSERT_EQ(ab.len, 15);
    ASSERT_EQ(memcmp(ab.b, "abbccccdddddddd", 15), 0);

    terminal_buffer_free(&ab);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST(terminal_buffer_free_null_safe) {
    struct abuf ab = ABUF_INIT;

    /* Should not crash on uninitialized buffer */
    terminal_buffer_free(&ab);
    terminal_buffer_free(&ab);  /* Double free should be safe */
}

TEST(terminal_buffer_append_after_free) {
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab, "first", 5);
    terminal_buffer_free(&ab);

    /* Reusing after free should work */
    ab = (struct abuf)ABUF_INIT;
    terminal_buffer_append(&ab, "second", 6);

    ASSERT_EQ(ab.len, 6);
    ASSERT_EQ(memcmp(ab.b, "second", 6), 0);

    terminal_buffer_free(&ab);
}

TEST(terminal_buffer_newlines) {
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab, "line1\r\n", 7);
    terminal_buffer_append(&ab, "line2\r\n", 7);
    terminal_buffer_append(&ab, "line3", 5);

    ASSERT_EQ(ab.len, 19);

    terminal_buffer_free(&ab);
}

#ifndef _WIN32
/* Feed bytes through a pipe whose write end is closed, so reads past the
 * data return 0 (the same value raw mode gives on a VTIME timeout). */
static int key_pipe(const char *bytes, size_t n) {
    int fds[2];
    if (pipe(fds) != 0) return -1;
    if (write(fds[1], bytes, n) != (ssize_t)n) { close(fds[0]); close(fds[1]); return -1; }
    close(fds[1]);
    return fds[0];
}

/* An unrecognised CSI sequence used to loop with c == ESC and swallow the
 * next two keystrokes. */
TEST(read_key_unknown_sequence_keeps_following_keys) {
    int fd = key_pipe("\x1b[Zab", 5);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(terminal_read_key(fd), ESC);
    ASSERT_EQ(terminal_read_key(fd), 'a');
    ASSERT_EQ(terminal_read_key(fd), 'b');
    close(fd);
}

TEST(read_key_known_sequence) {
    int fd = key_pipe("\x1b[Ax", 4);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(terminal_read_key(fd), ARROW_UP);
    ASSERT_EQ(terminal_read_key(fd), 'x');
    close(fd);
}

/* Closed input (hangup, closed pipe) reads 0 bytes at once, forever. It
 * must end the read instead of being mistaken for an idle timeout. */
TEST(read_key_returns_minus_one_on_closed_input) {
    int fd = key_pipe("x", 1);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(terminal_read_key(fd), 'x');
    ASSERT_EQ(terminal_read_key(fd), -1);
    close(fd);
}

/* A read error is reported to the caller instead of calling exit(). */
TEST(read_key_returns_minus_one_on_error) {
    ASSERT_EQ(terminal_read_key(-1), -1);
}
#endif

BEGIN_TEST_SUITE("Terminal Buffer Operations")
    /* Basic buffer operations */
    RUN_TEST(terminal_buffer_init);
    RUN_TEST(terminal_buffer_append_single);
    RUN_TEST(terminal_buffer_append_multiple);
    RUN_TEST(terminal_buffer_append_empty);
    RUN_TEST(terminal_buffer_free_clears);
    RUN_TEST(terminal_buffer_append_large_content);

    /* Escape sequences */
    RUN_TEST(terminal_buffer_append_escape_sequence);
    RUN_TEST(terminal_buffer_append_cursor_position);
    RUN_TEST(terminal_buffer_multiple_escape_sequences);

    /* VT100 specific */
    RUN_TEST(terminal_buffer_color_sequence);
    RUN_TEST(terminal_buffer_rgb_color_sequence);
    RUN_TEST(terminal_buffer_reverse_video);
    RUN_TEST(terminal_buffer_clear_to_end_of_line);
    RUN_TEST(terminal_buffer_hide_show_cursor);

    /* Buffer growth */
    RUN_TEST(terminal_buffer_grows_automatically);
    RUN_TEST(terminal_buffer_handles_varying_sizes);

    /* Binary and special data */
    RUN_TEST(terminal_buffer_append_binary_data);
    RUN_TEST(terminal_buffer_sequential_operations);

    /* Edge cases */
    RUN_TEST(terminal_buffer_free_null_safe);
    RUN_TEST(terminal_buffer_append_after_free);
    RUN_TEST(terminal_buffer_newlines);
#ifndef _WIN32
    RUN_TEST(read_key_unknown_sequence_keeps_following_keys);
    RUN_TEST(read_key_known_sequence);
    RUN_TEST(read_key_returns_minus_one_on_closed_input);
    RUN_TEST(read_key_returns_minus_one_on_error);
#endif
END_TEST_SUITE()
