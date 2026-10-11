/**
 * @file repl_line_editor.c
 * @brief Common REPL infrastructure - line editor, terminal handling, history.
 *
 * This file contains the shared infrastructure used by all language REPLs.
 * When LOKI_USE_LINENOISE is defined, delegates to linenoise for line editing
 * with tree-sitter syntax highlighting.
 */

#include "repl.h"
#include "loki/core.h"
#include "terminal.h"
#include "syntax.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef LOKI_USE_LINENOISE
#include <linenoise.h>
#include "repl_linenoise.h"
#endif

#if !defined(LOKI_USE_LINENOISE) && !defined(_WIN32)
#include <termios.h>
#endif

/* ============================================================================
 * Completion Prefix Extraction
 * ============================================================================ */

/* Extract the word ending at buf[pos] into out[out_size].
 *
 * Returns the word length on success, or -1 if the word (plus its terminator)
 * does not fit in out_size. Callers must treat -1 as "no completion possible"
 * rather than truncating: a word longer than REPL_MAX_INPUT_LENGTH would not
 * survive repl_readline()'s truncation into ReplLineEditor.buf anyway.
 *
 * This matters because linenoise hands its completion callback a buffer of
 * LINENOISE_MAX_LINE (4096), which is four times the size of the REPL's own
 * REPL_MAX_INPUT_LENGTH (1024) buffers.
 */
int repl_extract_completion_prefix(const char *buf, int pos, char *out,
                                   size_t out_size, int *word_start) {
    if (!buf || !out || out_size == 0 || pos < 0) return -1;

    int start = pos;
    while (start > 0 && !isspace((unsigned char)buf[start - 1])) {
        start--;
    }

    int word_len = pos - start;
    if ((size_t)word_len >= out_size) return -1;

    if (word_len > 0) {
        memcpy(out, buf + start, (size_t)word_len);
    }
    out[word_len] = '\0';

    if (word_start) *word_start = start;
    return word_len;
}

/* ============================================================================
 * Line Editor State Management
 * ============================================================================ */

#ifdef LOKI_USE_LINENOISE
void repl_editor_init_with_language(ReplLineEditor *ed, ReplLanguage lang) {
    memset(ed, 0, sizeof(*ed));
    ed->history_idx = -1;

    /* Initialize linenoise context with specified language */
    if (repl_linenoise_init(lang) == 0) {
        ed->ln_ctx = repl_linenoise_get_context();
        ed->ln_lang = lang;
    }
}
#endif

void repl_editor_init(ReplLineEditor *ed) {
    memset(ed, 0, sizeof(*ed));
    ed->history_idx = -1;

#ifdef LOKI_USE_LINENOISE
    /* Initialize linenoise context for Lua by default */
    if (repl_linenoise_init(REPL_LANG_LUA) == 0) {
        ed->ln_ctx = repl_linenoise_get_context();
        ed->ln_lang = REPL_LANG_LUA;
    }
#endif
}

#ifdef LOKI_USE_LINENOISE
/* Editor currently registered with linenoise's completion callback. */
static ReplLineEditor *g_completion_editor = NULL;
#endif

void repl_editor_cleanup(ReplLineEditor *ed) {
    for (int i = 0; i < ed->history_len; i++) {
        free(ed->history[i]);
    }
    repl_completion_clear(ed);

#ifdef LOKI_USE_LINENOISE
    /* Drop the global completion pointer if it still refers to this editor,
     * otherwise the next TAB would dereference freed memory. */
    if (g_completion_editor == ed) {
        g_completion_editor = NULL;
        if (ed->ln_ctx) linenoise_set_completion_callback(ed->ln_ctx, NULL);
    }
    /* Don't destroy context here - it's managed by repl_linenoise module */
    ed->ln_ctx = NULL;
#endif

    memset(ed, 0, sizeof(*ed));
}

void repl_set_completion_words(ReplLineEditor *ed, const char **words, int count) {
    ed->completion_words = words;
    ed->completion_word_count = count;
}

#ifdef LOKI_USE_LINENOISE
/* Linenoise completion callback adapter */
static void linenoise_completion_adapter(const char *buf, linenoise_completions_t *lc) {
    if (!g_completion_editor) return;
    ReplLineEditor *ed = g_completion_editor;

    /* Extract word prefix from end of buffer. Bails out on a word too long to
     * fit prefix[] - see repl_extract_completion_prefix(). */
    char prefix[REPL_MAX_INPUT_LENGTH];
    int start = 0;
    int word_len = repl_extract_completion_prefix(buf, (int)strlen(buf), prefix,
                                                  sizeof(prefix), &start);
    if (word_len < 0) return;

    /* Get completions from callback or word list */
    char **completions = NULL;
    int count = 0;

    if (ed->completion_cb) {
        completions = ed->completion_cb(prefix, &count, ed->completion_user_data);
    } else if (ed->completion_words) {
        /* Generate from word list */
        for (int i = 0; i < ed->completion_word_count; i++) {
            if (ed->completion_words[i] &&
                strncmp(ed->completion_words[i], prefix, word_len) == 0) {
                count++;
            }
        }
        if (count > 0) {
            completions = malloc(sizeof(char*) * count);
            if (completions) {
                int idx = 0;
                for (int i = 0; i < ed->completion_word_count && idx < count; i++) {
                    if (ed->completion_words[i] &&
                        strncmp(ed->completion_words[i], prefix, word_len) == 0) {
                        completions[idx] = strdup(ed->completion_words[i]);
                        idx++;
                    }
                }
            }
        }
    }

    /* Add completions to linenoise */
    if (completions) {
        for (int i = 0; i < count; i++) {
            if (completions[i]) {
                /* Build full line with completion */
                char full[REPL_MAX_INPUT_LENGTH];
                if (start + strlen(completions[i]) < REPL_MAX_INPUT_LENGTH) {
                    memcpy(full, buf, start);
                    strcpy(full + start, completions[i]);
                    linenoise_add_completion(lc, full);
                }
                free(completions[i]);
            }
        }
        free(completions);
    }
}
#endif

void repl_set_completion(ReplLineEditor *ed, ReplCompletionCallback cb, void *user_data) {
    ed->completion_cb = cb;
    ed->completion_user_data = user_data;

#ifdef LOKI_USE_LINENOISE
    if (ed->ln_ctx) {
        if (cb) {
            g_completion_editor = ed;
            linenoise_set_completion_callback(ed->ln_ctx, linenoise_completion_adapter);
        } else if (g_completion_editor == ed) {
            /* Unregistering must actually unregister. */
            g_completion_editor = NULL;
            linenoise_set_completion_callback(ed->ln_ctx, NULL);
        }
    }
#endif
}

void repl_completion_clear(ReplLineEditor *ed) {
    if (ed->completion.completions) {
        for (int i = 0; i < ed->completion.count; i++) {
            free(ed->completion.completions[i]);
        }
        free(ed->completion.completions);
    }
    ed->completion.completions = NULL;
    ed->completion.count = 0;
    ed->completion.index = -1;
    ed->completion.word_start = 0;
    ed->completion.word_len = 0;
}

void repl_add_history(ReplLineEditor *ed, const char *line) {
    if (!line || !line[0]) return;

    /* Don't add duplicates of the last entry */
    if (ed->history_len > 0 && strcmp(ed->history[ed->history_len - 1], line) == 0) {
        return;
    }

    /* Remove oldest if full */
    if (ed->history_len >= REPL_HISTORY_MAX) {
        free(ed->history[0]);
        memmove(ed->history, ed->history + 1, (REPL_HISTORY_MAX - 1) * sizeof(char *));
        ed->history_len--;
    }

    ed->history[ed->history_len++] = strdup(line);

#ifdef LOKI_USE_LINENOISE
    /* Also add to linenoise history */
    if (ed->ln_ctx) {
        linenoise_history_add(ed->ln_ctx, line);
    }
#endif
}

int repl_history_load(ReplLineEditor *ed, const char *filepath) {
    if (!ed || !filepath) return -1;

#ifdef LOKI_USE_LINENOISE
    /* Use linenoise history loading */
    if (ed->ln_ctx) {
        return linenoise_history_load(ed->ln_ctx, filepath);
    }
#endif

    FILE *f = fopen(filepath, "r");
    if (!f) return -1;  /* File doesn't exist yet, not an error */

    char line[REPL_MAX_INPUT_LENGTH];
    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        /* Skip empty lines */
        if (line[0] != '\0') {
            repl_add_history(ed, line);
        }
    }

    fclose(f);
    return 0;
}

int repl_history_save(ReplLineEditor *ed, const char *filepath) {
    if (!ed || !filepath || !filepath[0]) return -1;

#ifdef LOKI_USE_LINENOISE
    /* Use linenoise history saving */
    if (ed->ln_ctx) {
        return linenoise_history_save(ed->ln_ctx, filepath);
    }
#endif

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    for (int i = 0; i < ed->history_len; i++) {
        fprintf(f, "%s\n", ed->history[i]);
    }

    fclose(f);
    return 0;
}

/* ============================================================================
 * Terminal Raw Mode (only needed without linenoise)
 * ============================================================================ */

#ifndef LOKI_USE_LINENOISE
/* Original termios for REPL raw mode (separate from editor) */
static struct termios repl_orig_termios;
static int repl_rawmode = 0;

void repl_disable_raw_mode(void) {
    if (repl_rawmode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &repl_orig_termios);
        repl_rawmode = 0;
    }
}

int repl_enable_raw_mode(void) {
    struct termios raw;

    if (repl_rawmode) return 0;
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &repl_orig_termios) == -1) return -1;

    raw = repl_orig_termios;
    /* Input modes: no break, no CR to NL, no parity, no strip, no flow ctrl */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Control modes - 8 bit chars */
    raw.c_cflag |= (CS8);
    /* Local modes - echo off, canonical off, no extended functions */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* Return each byte immediately */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return -1;
    repl_rawmode = 1;
    return 0;
}
#else
/* With linenoise, raw mode is handled internally */
void repl_disable_raw_mode(void) {
    /* No-op - linenoise handles raw mode */
}

int repl_enable_raw_mode(void) {
    /* No-op - linenoise handles raw mode */
    return 0;
}
#endif

/* ============================================================================
 * Syntax Highlighting (fallback for non-linenoise mode)
 * ============================================================================ */

#ifndef LOKI_USE_LINENOISE
static int repl_is_separator(int c, const char *separators) {
    if (isspace(c)) return 1;
    if (c == '\0') return 1;
    return (separators && strchr(separators, c) != NULL);
}

void repl_highlight_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed) {
    struct t_editor_syntax *syn = syntax_ctx->view.syntax;

    memset(ed->hl, HL_NORMAL, ed->len);

    if (!syn || ed->len == 0) return;

    char **keywords = syn->keywords;
    char *scs = syn->singleline_comment_start;
    char *separators = syn->separators;
    int highlight_strings = syn->flags & HL_HIGHLIGHT_STRINGS;
    int highlight_numbers = syn->flags & HL_HIGHLIGHT_NUMBERS;

    int i = 0;
    int prev_sep = 1;
    int in_string = 0;
    char *p = ed->buf;

    /* Skip leading whitespace */
    while (*p && isspace(*p) && i < ed->len) {
        p++;
        i++;
    }

    while (*p && i < ed->len) {
        /* Handle single-line comments */
        if (prev_sep && scs && scs[0] && *p == scs[0] &&
            (scs[1] == '\0' || (i < ed->len - 1 && *(p+1) == scs[1]))) {
            memset(ed->hl + i, HL_COMMENT, ed->len - i);
            break;
        }

        /* Handle strings */
        if (in_string) {
            ed->hl[i] = HL_STRING;
            if (*p == '\\' && i < ed->len - 1) {
                ed->hl[i+1] = HL_STRING;
                p += 2;
                i += 2;
                continue;
            }
            if (*p == in_string) in_string = 0;
            p++;
            i++;
            prev_sep = 1;
            continue;
        } else if (highlight_strings && (*p == '"' || *p == '\'')) {
            in_string = *p;
            ed->hl[i] = HL_STRING;
            p++;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers */
        if (highlight_numbers && (isdigit(*p) || (*p == '.' && i < ed->len - 1 && isdigit(*(p+1)))) && prev_sep) {
            while (i < ed->len && (isdigit(*p) || *p == '.')) {
                ed->hl[i] = HL_NUMBER;
                p++;
                i++;
            }
            prev_sep = 0;
            continue;
        }

        /* Handle keywords */
        if (prev_sep && keywords) {
            int keyword_found = 0;
            for (int j = 0; keywords[j]; j++) {
                size_t klen = strlen(keywords[j]);
                int kw2 = (klen > 0 && keywords[j][klen-1] == '|');
                if (kw2) klen--;

                if (klen == 0) continue;

                if ((int)(i + klen) <= ed->len &&
                    memcmp(p, keywords[j], klen) == 0 &&
                    repl_is_separator(p[klen], separators)) {
                    int hl_type = kw2 ? HL_KEYWORD2 : HL_KEYWORD1;
                    memset(ed->hl + i, hl_type, klen);
                    p += klen;
                    i += (int)klen;
                    prev_sep = 0;
                    keyword_found = 1;
                    break;
                }
            }
            if (keyword_found) continue;
        }

        prev_sep = repl_is_separator(*p, separators);
        p++;
        i++;
    }
}

void repl_render_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    struct abuf ab = ABUF_INIT;

    /* Move to start of line, clear it */
    terminal_buffer_append(&ab, "\r\x1b[K", 4);

    /* Output prompt (no highlighting) */
    terminal_buffer_append(&ab, prompt, strlen(prompt));

    /* Highlight the input */
    repl_highlight_line(syntax_ctx, ed);

    /* Output highlighted text */
    int current_hl = -1;
    for (int i = 0; i < ed->len; i++) {
        if (ed->hl[i] != current_hl) {
            char color[32];
            int clen = syntax_format_color(syntax_ctx, ed->hl[i], color, sizeof(color));
            terminal_buffer_append(&ab, color, clen);
            current_hl = ed->hl[i];
        }
        terminal_buffer_append(&ab, &ed->buf[i], 1);
    }

    /* Reset color */
    terminal_buffer_append(&ab, "\x1b[39m", 5);

    /* Position cursor */
    if (ed->pos < ed->len) {
        char pos[32];
        int plen = snprintf(pos, sizeof(pos), "\x1b[%dD", ed->len - ed->pos);
        terminal_buffer_append(&ab, pos, plen);
    }

    write(STDOUT_FILENO, ab.b, ab.len);
    terminal_buffer_free(&ab);
}
#else
/* With linenoise, these are no-ops since linenoise handles rendering */
void repl_highlight_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed) {
    (void)syntax_ctx;
    (void)ed;
}

void repl_render_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    (void)syntax_ctx;
    (void)ed;
    (void)prompt;
}
#endif

/* ============================================================================
 * Line Reading
 * ============================================================================ */

#ifdef LOKI_USE_LINENOISE

char *repl_readline(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    (void)syntax_ctx;  /* Not needed - linenoise handles highlighting */

    if (!ed->ln_ctx) {
        /* Fallback: initialize linenoise if not done */
        if (repl_linenoise_init(REPL_LANG_LUA) == 0) {
            ed->ln_ctx = repl_linenoise_get_context();
            ed->ln_lang = REPL_LANG_LUA;
        }
    }

    if (!ed->ln_ctx) {
        return NULL;  /* No linenoise context available */
    }

    /* Set up completion adapter */
    if (ed->completion_cb || ed->completion_words) {
        g_completion_editor = ed;
        linenoise_set_completion_callback(ed->ln_ctx, linenoise_completion_adapter);
    }

    /* Read line using linenoise */
    char *line = linenoise_read(ed->ln_ctx, prompt);

    if (line == NULL) {
        /* EOF or error */
        ed->buf[0] = '\0';
        ed->len = 0;
        ed->pos = 0;
        return NULL;
    }

    /* Copy result to editor buffer */
    strncpy(ed->buf, line, REPL_MAX_INPUT_LENGTH - 1);
    ed->buf[REPL_MAX_INPUT_LENGTH - 1] = '\0';
    ed->len = strlen(ed->buf);
    ed->pos = ed->len;

    /* Free linenoise's allocated line */
    linenoise_free(line);

    return ed->buf;
}

#else

/* ============================================================================
 * Tab Completion (non-linenoise fallback)
 * ============================================================================ */

static int repl_find_word_start(const char *buf, int pos) {
    int start = pos;
    while (start > 0 && !isspace((unsigned char)buf[start - 1])) {
        start--;
    }
    return start;
}

static char **repl_completions_from_words(const char **words, int word_count,
                                           const char *prefix, int *out_count) {
    if (!words || word_count == 0) {
        *out_count = 0;
        return NULL;
    }

    size_t prefix_len = prefix ? strlen(prefix) : 0;

    int matches = 0;
    for (int i = 0; i < word_count; i++) {
        if (words[i] && strncmp(words[i], prefix, prefix_len) == 0) {
            matches++;
        }
    }

    if (matches == 0) {
        *out_count = 0;
        return NULL;
    }

    char **result = malloc(sizeof(char*) * matches);
    if (!result) {
        *out_count = 0;
        return NULL;
    }

    int idx = 0;
    for (int i = 0; i < word_count && idx < matches; i++) {
        if (words[i] && strncmp(words[i], prefix, prefix_len) == 0) {
            result[idx] = strdup(words[i]);
            if (!result[idx]) {
                for (int j = 0; j < idx; j++) free(result[j]);
                free(result);
                *out_count = 0;
                return NULL;
            }
            idx++;
        }
    }

    *out_count = matches;
    return result;
}

static void repl_handle_tab(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    if (!ed->completion_cb && !ed->completion_words) return;

    ReplCompletionState *cs = &ed->completion;

    if (cs->completions && cs->count > 0) {
        cs->index = (cs->index + 1) % cs->count;

        int new_len = strlen(cs->completions[cs->index]);
        int old_completion_len = ed->pos - cs->word_start;
        int delta = new_len - old_completion_len;

        if (ed->len + delta >= REPL_MAX_INPUT_LENGTH) return;

        memmove(&ed->buf[ed->pos + delta],
                &ed->buf[ed->pos],
                ed->len - ed->pos + 1);

        memcpy(&ed->buf[cs->word_start], cs->completions[cs->index], new_len);

        ed->len += delta;
        ed->pos = cs->word_start + new_len;
        ed->buf[ed->len] = '\0';

        repl_render_line(syntax_ctx, ed, prompt);
        return;
    }

    int word_start = repl_find_word_start(ed->buf, ed->pos);
    int word_len = ed->pos - word_start;

    char prefix[REPL_MAX_INPUT_LENGTH];
    if (word_len > 0) {
        memcpy(prefix, &ed->buf[word_start], word_len);
    }
    prefix[word_len] = '\0';

    int count = 0;
    char **completions;
    if (ed->completion_cb) {
        completions = ed->completion_cb(prefix, &count, ed->completion_user_data);
    } else {
        completions = repl_completions_from_words(ed->completion_words,
                                                   ed->completion_word_count,
                                                   prefix, &count);
    }

    if (!completions || count == 0) {
        return;
    }

    cs->completions = completions;
    cs->count = count;
    cs->index = 0;
    cs->word_start = word_start;
    cs->word_len = word_len;

    if (count == 1 && strcmp(completions[0], prefix) == 0) {
        repl_completion_clear(ed);
        return;
    }

    int new_len = strlen(completions[0]);
    int delta = new_len - word_len;

    if (ed->len + delta >= REPL_MAX_INPUT_LENGTH) {
        repl_completion_clear(ed);
        return;
    }

    memmove(&ed->buf[ed->pos + delta],
            &ed->buf[ed->pos],
            ed->len - ed->pos + 1);

    memcpy(&ed->buf[word_start], completions[0], new_len);

    ed->len += delta;
    ed->pos = word_start + new_len;
    ed->buf[ed->len] = '\0';

    repl_render_line(syntax_ctx, ed, prompt);
}

char *repl_readline(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    ed->buf[0] = '\0';
    ed->len = 0;
    ed->pos = 0;
    ed->history_idx = -1;

    repl_render_line(syntax_ctx, ed, prompt);

    while (1) {
        fflush(stdout);
        int c = terminal_read_key(STDIN_FILENO);
        if (c == -1) return NULL;  /* Input error: treat as EOF. */

        if (c == ENTER) {
            write(STDOUT_FILENO, "\r\n", 2);
            ed->buf[ed->len] = '\0';
            return ed->buf;
        }

        if (c == CTRL_C) {
            ed->buf[0] = '\0';
            ed->len = 0;
            ed->pos = 0;
            write(STDOUT_FILENO, "^C\r\n", 4);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_D) {
            if (ed->len == 0) {
                write(STDOUT_FILENO, "\r\n", 2);
                return NULL;
            }
            if (ed->pos < ed->len) {
                memmove(&ed->buf[ed->pos], &ed->buf[ed->pos + 1], ed->len - ed->pos);
                ed->len--;
            }
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == TAB) {
            repl_handle_tab(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == BACKSPACE || c == CTRL_H) {
            if (ed->pos > 0) {
                memmove(&ed->buf[ed->pos - 1], &ed->buf[ed->pos], ed->len - ed->pos + 1);
                ed->pos--;
                ed->len--;
            }
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == DEL_KEY) {
            if (ed->pos < ed->len) {
                memmove(&ed->buf[ed->pos], &ed->buf[ed->pos + 1], ed->len - ed->pos);
                ed->len--;
            }
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_LEFT) {
            if (ed->pos > 0) ed->pos--;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_RIGHT) {
            if (ed->pos < ed->len) ed->pos++;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == HOME_KEY || c == CTRL_A) {
            ed->pos = 0;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == END_KEY || c == CTRL_E) {
            ed->pos = ed->len;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_UP) {
            if (ed->history_len == 0) continue;

            if (ed->history_idx == -1) {
                memcpy(ed->saved_buf, ed->buf, ed->len + 1);
                ed->saved_len = ed->len;
                ed->history_idx = ed->history_len - 1;
            } else if (ed->history_idx > 0) {
                ed->history_idx--;
            } else {
                continue;
            }

            strncpy(ed->buf, ed->history[ed->history_idx], REPL_MAX_INPUT_LENGTH - 1);
            ed->buf[REPL_MAX_INPUT_LENGTH - 1] = '\0';
            ed->len = strlen(ed->buf);
            ed->pos = ed->len;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_DOWN) {
            if (ed->history_idx == -1) continue;

            if (ed->history_idx < ed->history_len - 1) {
                ed->history_idx++;
                strncpy(ed->buf, ed->history[ed->history_idx], REPL_MAX_INPUT_LENGTH - 1);
                ed->buf[REPL_MAX_INPUT_LENGTH - 1] = '\0';
                ed->len = strlen(ed->buf);
                ed->pos = ed->len;
            } else {
                ed->history_idx = -1;
                memcpy(ed->buf, ed->saved_buf, ed->saved_len + 1);
                ed->len = ed->saved_len;
                ed->pos = ed->len;
            }
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_U) {
            ed->buf[0] = '\0';
            ed->len = 0;
            ed->pos = 0;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_K) {
            ed->len = ed->pos;
            ed->buf[ed->len] = '\0';
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c >= 32 && c < 127 && ed->len < REPL_MAX_INPUT_LENGTH - 1) {
            memmove(&ed->buf[ed->pos + 1], &ed->buf[ed->pos], ed->len - ed->pos + 1);
            ed->buf[ed->pos] = c;
            ed->pos++;
            ed->len++;
            repl_completion_clear(ed);
            repl_render_line(syntax_ctx, ed, prompt);
        }
    }
}

#endif /* LOKI_USE_LINENOISE */
