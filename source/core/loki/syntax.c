/* loki_syntax.c - Syntax highlighting implementation
 *
 * This module implements syntax highlighting for the loki editor.
 * It uses a simple token-based approach with support for:
 * - Keywords (primary and type keywords)
 * - String literals (with escape sequence handling)
 * - Single-line and multi-line comments
 * - Numeric literals
 * - Non-printable character visualization
 *
 * The highlighting is performed on the "rendered" version of each row
 * (after tab expansion) and stores highlight types in the row->hl array.
 *
 * When built with linenoise support (LOKI_USE_LINENOISE), colors are
 * pulled from the current tree-sitter theme for consistency.
 */

#include "syntax.h"
#include "languages.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#ifdef LOKI_USE_LINENOISE
#include <syntax/theme.h>
#include "treesitter.h"
#endif

/* ====================== Syntax highlight color scheme  ==================== */

int syntax_is_separator(int c, char *separators) {
    return c == '\0' || isspace(c) || strchr(separators, c) != NULL;
}

/* Return true if the specified row last char is part of a multi line comment
 * that starts at this row or at one before, and does not end at the end
 * of the row but spawns to the next row. */
int syntax_row_has_open_comment(t_erow *row) {
    if (row->hl && row->rsize && row->hl[row->rsize-1] == HL_MLCOMMENT &&
        (row->rsize < 2 || (row->render[row->rsize-2] != '*' ||
                            row->render[row->rsize-1] != '/'))) return 1;
    return 0;
}

/* Forward declaration for markdown highlighter */
void editor_update_syntax_markdown(editor_ctx_t *ctx, t_erow *row);

/* Map human-readable style names to HL_* constants */
int syntax_name_to_code(const char *name) {
    if (name == NULL) return -1;
    /* Base types (0-8) */
    if (strcasecmp(name, "normal") == 0) return HL_NORMAL;
    if (strcasecmp(name, "nonprint") == 0) return HL_NONPRINT;
    if (strcasecmp(name, "comment") == 0) return HL_COMMENT;
    if (strcasecmp(name, "mlcomment") == 0) return HL_MLCOMMENT;
    if (strcasecmp(name, "keyword1") == 0) return HL_KEYWORD1;
    if (strcasecmp(name, "keyword2") == 0) return HL_KEYWORD2;
    if (strcasecmp(name, "string") == 0) return HL_STRING;
    if (strcasecmp(name, "number") == 0) return HL_NUMBER;
    if (strcasecmp(name, "match") == 0) return HL_MATCH;
    /* Extended types (9-23) */
    if (strcasecmp(name, "function") == 0) return HL_FUNCTION;
    if (strcasecmp(name, "function_builtin") == 0) return HL_FUNCTION_BUILTIN;
    if (strcasecmp(name, "function_call") == 0) return HL_FUNCTION_CALL;
    if (strcasecmp(name, "variable_builtin") == 0) return HL_VARIABLE_BUILTIN;
    if (strcasecmp(name, "variable_parameter") == 0) return HL_VARIABLE_PARAMETER;
    if (strcasecmp(name, "operator") == 0) return HL_OPERATOR;
    if (strcasecmp(name, "punctuation") == 0) return HL_PUNCTUATION;
    if (strcasecmp(name, "constructor") == 0) return HL_CONSTRUCTOR;
    if (strcasecmp(name, "namespace") == 0) return HL_NAMESPACE;
    if (strcasecmp(name, "label") == 0) return HL_LABEL;
    if (strcasecmp(name, "tag") == 0) return HL_TAG;
    if (strcasecmp(name, "keyword_control") == 0) return HL_KEYWORD_CONTROL;
    if (strcasecmp(name, "keyword_function") == 0) return HL_KEYWORD_FUNCTION;
    if (strcasecmp(name, "keyword_return") == 0) return HL_KEYWORD_RETURN;
    if (strcasecmp(name, "constant_builtin") == 0) return HL_CONSTANT_BUILTIN;
    /* Additional extended types (24-50) */
    if (strcasecmp(name, "variable") == 0) return HL_VARIABLE;
    if (strcasecmp(name, "variable_field") == 0) return HL_VARIABLE_FIELD;
    if (strcasecmp(name, "variable_property") == 0) return HL_VARIABLE_PROPERTY;
    if (strcasecmp(name, "keyword_operator") == 0) return HL_KEYWORD_OPERATOR;
    if (strcasecmp(name, "keyword_import") == 0) return HL_KEYWORD_IMPORT;
    if (strcasecmp(name, "keyword_type") == 0) return HL_KEYWORD_TYPE;
    if (strcasecmp(name, "keyword_modifier") == 0) return HL_KEYWORD_MODIFIER;
    if (strcasecmp(name, "string_escape") == 0) return HL_STRING_ESCAPE;
    if (strcasecmp(name, "string_regex") == 0) return HL_STRING_REGEX;
    if (strcasecmp(name, "string_special") == 0) return HL_STRING_SPECIAL;
    if (strcasecmp(name, "number_float") == 0) return HL_NUMBER_FLOAT;
    if (strcasecmp(name, "boolean") == 0) return HL_BOOLEAN;
    if (strcasecmp(name, "constant") == 0) return HL_CONSTANT;
    if (strcasecmp(name, "comment_doc") == 0) return HL_COMMENT_DOC;
    if (strcasecmp(name, "function_method") == 0) return HL_FUNCTION_METHOD;
    if (strcasecmp(name, "function_macro") == 0) return HL_FUNCTION_MACRO;
    if (strcasecmp(name, "type") == 0) return HL_TYPE;
    if (strcasecmp(name, "type_builtin") == 0) return HL_TYPE_BUILTIN;
    if (strcasecmp(name, "type_parameter") == 0) return HL_TYPE_PARAMETER;
    if (strcasecmp(name, "type_qualifier") == 0) return HL_TYPE_QUALIFIER;
    if (strcasecmp(name, "punctuation_bracket") == 0) return HL_PUNCTUATION_BRACKET;
    if (strcasecmp(name, "punctuation_delimiter") == 0) return HL_PUNCTUATION_DELIMITER;
    if (strcasecmp(name, "module") == 0) return HL_MODULE;
    if (strcasecmp(name, "tag_attribute") == 0) return HL_TAG_ATTRIBUTE;
    if (strcasecmp(name, "preprocessor") == 0) return HL_PREPROCESSOR;
    if (strcasecmp(name, "error") == 0) return HL_ERROR;
    if (strcasecmp(name, "warning") == 0) return HL_WARNING;
    return -1;
}

/* Set every byte of row->hl (that corresponds to every character in the line)
 * to the right syntax highlight type (HL_* defines). */
void syntax_update_row(editor_ctx_t *ctx, t_erow *row) {
    if (row->rsize == 0) {
        free(row->hl);
        row->hl = NULL;
        return;
    }
    unsigned char *new_hl = realloc(row->hl,row->rsize);
    if (new_hl == NULL) return; /* Out of memory, keep old highlighting */
    row->hl = new_hl;
    memset(row->hl,HL_NORMAL,row->rsize);

#ifdef LOKI_USE_LINENOISE
    /* Use tree-sitter highlighting when available */
    if (ctx->model.ts_state != NULL) {
        treesitter_update_row(ctx, row, ctx->model.ts_state);
        return;
    }
#endif

    int default_ran = 0;
    int prev_cb_lang = row->cb_lang;   /* to detect code-block state changes */

    if (ctx->view.syntax != NULL) {
        if (ctx->view.syntax->type == HL_TYPE_MARKDOWN) {
            editor_update_syntax_markdown(ctx, row);
            default_ran = 1;
        } else if (ctx->view.syntax->type == HL_TYPE_CSOUND) {
            editor_update_syntax_csound(ctx, row);
            default_ran = 1;
        } else {
            int i, prev_sep, in_string, in_comment;
            char *p;
            char **keywords = ctx->view.syntax->keywords;
            char *scs = ctx->view.syntax->singleline_comment_start;
            char *mcs = ctx->view.syntax->multiline_comment_start;
            char *mce = ctx->view.syntax->multiline_comment_end;
            char *separators = ctx->view.syntax->separators;

            /* Point to the first non-space char. */
            p = row->render;
            i = 0; /* Current char offset */
            while(*p && isspace(*p)) {
                p++;
                i++;
            }
            prev_sep = 1; /* Tell the parser if 'i' points to start of word. */
            in_string = 0; /* Are we inside "" or '' ? */
            in_comment = 0; /* Are we inside multi-line comment? */

            /* If the previous line has an open comment, this line starts
             * with an open comment state. */
            if (row->idx > 0 && syntax_row_has_open_comment(&ctx->model.row[row->idx-1]))
                in_comment = 1;

            while(*p) {
                /* Handle single-line comments (e.g., //, #, --) */
                if (prev_sep && scs[0] && *p == scs[0] &&
                    (scs[1] == '\0' || (i < row->rsize - 1 && *(p+1) == scs[1]))) {
                    /* From here to end is a comment */
                    memset(row->hl+i,HL_COMMENT,row->rsize-i);
                    break;
                }

                /* Handle multi line comments. */
                if (in_comment) {
                    row->hl[i] = HL_MLCOMMENT;
                    if (i < row->rsize - 1 && *p == mce[0] && *(p+1) == mce[1]) {
                        row->hl[i+1] = HL_MLCOMMENT;
                        p += 2; i += 2;
                        in_comment = 0;
                        prev_sep = 1;
                        continue;
                    } else {
                        prev_sep = 0;
                        p++; i++;
                        continue;
                    }
                } else if (i < row->rsize - 1 && *p == mcs[0] && *(p+1) == mcs[1]) {
                    row->hl[i] = HL_MLCOMMENT;
                    row->hl[i+1] = HL_MLCOMMENT;
                    p += 2; i += 2;
                    in_comment = 1;
                    prev_sep = 0;
                    continue;
                }

                /* Handle "" and '' */
                if (in_string) {
                    row->hl[i] = HL_STRING;
                    if (i < row->rsize - 1 && *p == '\\') {
                        row->hl[i+1] = HL_STRING;
                        p += 2; i += 2;
                        prev_sep = 0;
                        continue;
                    }
                    if (*p == in_string) in_string = 0;
                    p++; i++;
                    continue;
                } else {
                    if (*p == '"' || *p == '\'') {
                        in_string = *p;
                        row->hl[i] = HL_STRING;
                        p++; i++;
                        prev_sep = 0;
                        continue;
                    }
                }

                /* Handle non printable chars. */
                if (!isprint(*p)) {
                    row->hl[i] = HL_NONPRINT;
                    p++; i++;
                    prev_sep = 0;
                    continue;
                }

                /* Handle numbers */
                if ((isdigit(*p) && (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
                    (*p == '.' && i > 0 && row->hl[i-1] == HL_NUMBER &&
                     i < row->rsize - 1 && isdigit(*(p+1)))) {
                    row->hl[i] = HL_NUMBER;
                    p++; i++;
                    prev_sep = 0;
                    continue;
                }

                /* Handle keywords and lib calls */
                if (prev_sep) {
                    int j;
                    for (j = 0; keywords[j]; j++) {
                        int klen = strlen(keywords[j]);
                        int kw2 = keywords[j][klen-1] == '|';
                        if (kw2) klen--;

                        if (i + klen <= row->rsize &&
                            !memcmp(p,keywords[j],klen) &&
                            (i + klen == row->rsize || syntax_is_separator(*(p+klen), separators)))
                        {
                            /* Keyword */
                            memset(row->hl+i,kw2 ? HL_KEYWORD2 : HL_KEYWORD1,klen);
                            p += klen;
                            i += klen;
                            break;
                        }
                    }
                    if (keywords[j] != NULL) {
                        prev_sep = 0;
                        continue; /* We had a keyword match */
                    }
                }

                /* Not special chars */
                prev_sep = syntax_is_separator(*p, separators);
                p++; i++;
            }

            default_ran = 1;
        }
    }

    /* Let the Lua `loki.highlight_row` hook post-process this row. */
    lua_apply_highlight_row(ctx, row, default_ran);

    /* Propagate syntax change to the next row if the open comment
     * state changed. This may recursively affect all the following rows
     * in the file. */
    int oc = syntax_row_has_open_comment(row);
    int cb_changed = (row->cb_lang != prev_cb_lang);
    if ((row->hl_oc != oc || cb_changed) &&
        row->idx >= 0 && row->idx+1 < ctx->model.numrows)
        syntax_update_row(ctx, &ctx->model.row[row->idx+1]);
    row->hl_oc = oc;
}

/* Format RGB color escape sequence for syntax highlighting.
 * Uses true color (24-bit) escape codes: ESC[38;2;R;G;Bm
 * Returns the length of the formatted string. */
int syntax_format_color(editor_ctx_t *ctx, int hl, char *buf, size_t bufsize) {
    if (hl < 0 || hl >= HL_TYPE_COUNT) hl = 0;  /* Default to HL_NORMAL */
    t_hlcolor *color = &ctx->view.colors[hl];
    return snprintf(buf, bufsize, "\x1b[38;2;%d;%d;%dm",
                    color->r, color->g, color->b);
}

/* Select the syntax highlight scheme depending on the filename. */
void syntax_select_for_filename(editor_ctx_t *ctx, const char *filename) {
    if (!filename) return;   /* Unnamed buffer: no syntax to select. */
#ifdef LOKI_USE_LINENOISE
    /* Free any existing tree-sitter state */
    if (ctx->model.ts_state != NULL) {
        treesitter_free(ctx->model.ts_state);
        ctx->model.ts_state = NULL;
    }

    /* Try to initialize tree-sitter for this file */
    const char *ts_lang = treesitter_lang_from_filename(filename);
    if (ts_lang != NULL) {
        ctx->model.ts_state = treesitter_init(ts_lang);
        /* If tree-sitter succeeded, we still set view.syntax for fallback */
    }
#endif

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct t_editor_syntax *s = HLDB+j;
        if (!s->filematch) continue;
        unsigned int i = 0;
        while(s->filematch[i]) {
            char *p;
            int patlen = strlen(s->filematch[i]);
            if ((p = strstr(filename,s->filematch[i])) != NULL) {
                if (s->filematch[i][0] != '.' || p[patlen] == '\0') {
                    ctx->view.syntax = s;
                    return;
                }
            }
            i++;
        }
    }

    /* Also check dynamic language registry */
    int dynamic_count = get_dynamic_language_count();
    for (int j = 0; j < dynamic_count; j++) {
        struct t_editor_syntax *s = get_dynamic_language(j);
        if (!s || !s->filematch) continue;
        unsigned int i = 0;
        while(s->filematch[i]) {
            char *p;
            int patlen = strlen(s->filematch[i]);
            if ((p = strstr(filename,s->filematch[i])) != NULL) {
                if (s->filematch[i][0] != '.' || p[patlen] == '\0') {
                    ctx->view.syntax = s;
                    return;
                }
            }
            i++;
        }
    }
}

#ifdef LOKI_USE_LINENOISE
/* Convert 256-color palette index to RGB.
 * Handles: 0-15 (standard), 16-231 (6x6x6 cube), 232-255 (grayscale) */
static void color256_to_rgb(unsigned char c, int *r, int *g, int *b) {
    if (c == 0) {
        /* Color 0 means "default" - use light gray */
        *r = 200; *g = 200; *b = 200;
    } else if (c < 16) {
        /* Standard ANSI colors */
        static const int ansi[16][3] = {
            {0, 0, 0},       /* 0: black */
            {205, 49, 49},   /* 1: red */
            {13, 188, 121},  /* 2: green */
            {229, 229, 16},  /* 3: yellow */
            {36, 114, 200},  /* 4: blue */
            {188, 63, 188},  /* 5: magenta */
            {17, 168, 205},  /* 6: cyan */
            {229, 229, 229}, /* 7: white */
            {102, 102, 102}, /* 8: bright black */
            {241, 76, 76},   /* 9: bright red */
            {35, 209, 139},  /* 10: bright green */
            {245, 245, 67},  /* 11: bright yellow */
            {59, 142, 234},  /* 12: bright blue */
            {214, 112, 214}, /* 13: bright magenta */
            {41, 184, 219},  /* 14: bright cyan */
            {255, 255, 255}, /* 15: bright white */
        };
        *r = ansi[c][0]; *g = ansi[c][1]; *b = ansi[c][2];
    } else if (c < 232) {
        /* 6x6x6 color cube (indices 16-231) */
        int idx = c - 16;
        int ri = idx / 36;
        int gi = (idx % 36) / 6;
        int bi = idx % 6;
        *r = ri ? (ri * 40 + 55) : 0;
        *g = gi ? (gi * 40 + 55) : 0;
        *b = bi ? (bi * 40 + 55) : 0;
    } else {
        /* Grayscale (indices 232-255) */
        int gray = (c - 232) * 10 + 8;
        *r = gray; *g = gray; *b = gray;
    }
}

/* Helper to set a color entry from a TOK_* value */
static void set_color_from_tok(editor_ctx_t *ctx, int hl_type, syntax_token_t tok) {
    int r, g, b;
    color256_to_rgb(theme_color(tok), &r, &g, &b);
    ctx->view.colors[hl_type].r = r;
    ctx->view.colors[hl_type].g = g;
    ctx->view.colors[hl_type].b = b;
}

/* Apply current theme colors to the editor's color array */
void syntax_apply_theme_colors(editor_ctx_t *ctx) {
    int r, g, b;

    /* Base types (0-8) */

    /* HL_NORMAL - default foreground */
    set_color_from_tok(ctx, HL_NORMAL, TOK_DEFAULT);

    /* HL_NONPRINT - use muted color */
    color256_to_rgb(theme_color(TOK_COMMENT), &r, &g, &b);
    ctx->view.colors[HL_NONPRINT].r = r / 2;
    ctx->view.colors[HL_NONPRINT].g = g / 2;
    ctx->view.colors[HL_NONPRINT].b = b / 2;

    /* HL_COMMENT */
    set_color_from_tok(ctx, HL_COMMENT, TOK_COMMENT);

    /* HL_MLCOMMENT - same as comment */
    set_color_from_tok(ctx, HL_MLCOMMENT, TOK_COMMENT);

    /* HL_KEYWORD1 - primary keywords */
    set_color_from_tok(ctx, HL_KEYWORD1, TOK_KEYWORD);

    /* HL_KEYWORD2 - type keywords */
    set_color_from_tok(ctx, HL_KEYWORD2, TOK_TYPE);

    /* HL_STRING */
    set_color_from_tok(ctx, HL_STRING, TOK_STRING);

    /* HL_NUMBER */
    set_color_from_tok(ctx, HL_NUMBER, TOK_NUMBER);

    /* HL_MATCH - search match highlight (use function color for visibility) */
    set_color_from_tok(ctx, HL_MATCH, TOK_FUNCTION);

    /* Extended types (9-23) */

    /* Functions */
    set_color_from_tok(ctx, HL_FUNCTION, TOK_FUNCTION);
    set_color_from_tok(ctx, HL_FUNCTION_BUILTIN, TOK_FUNCTION_BUILTIN);
    set_color_from_tok(ctx, HL_FUNCTION_CALL, TOK_FUNCTION_CALL);

    /* Variables */
    set_color_from_tok(ctx, HL_VARIABLE_BUILTIN, TOK_VARIABLE_BUILTIN);
    set_color_from_tok(ctx, HL_VARIABLE_PARAMETER, TOK_VARIABLE_PARAMETER);

    /* Operators and punctuation */
    set_color_from_tok(ctx, HL_OPERATOR, TOK_OPERATOR);
    set_color_from_tok(ctx, HL_PUNCTUATION, TOK_PUNCTUATION);

    /* Special types */
    set_color_from_tok(ctx, HL_CONSTRUCTOR, TOK_CONSTRUCTOR);
    set_color_from_tok(ctx, HL_NAMESPACE, TOK_NAMESPACE);
    set_color_from_tok(ctx, HL_LABEL, TOK_LABEL);
    set_color_from_tok(ctx, HL_TAG, TOK_TAG);

    /* Keyword subtypes */
    set_color_from_tok(ctx, HL_KEYWORD_CONTROL, TOK_KEYWORD_CONTROL);
    set_color_from_tok(ctx, HL_KEYWORD_FUNCTION, TOK_KEYWORD_FUNCTION);
    set_color_from_tok(ctx, HL_KEYWORD_RETURN, TOK_KEYWORD_RETURN);

    /* Constants */
    set_color_from_tok(ctx, HL_CONSTANT_BUILTIN, TOK_CONSTANT_BUILTIN);

    /* Additional extended types (24-50) */

    /* Variables */
    set_color_from_tok(ctx, HL_VARIABLE, TOK_VARIABLE);
    set_color_from_tok(ctx, HL_VARIABLE_FIELD, TOK_VARIABLE_FIELD);
    set_color_from_tok(ctx, HL_VARIABLE_PROPERTY, TOK_VARIABLE_PROPERTY);

    /* Additional keyword subtypes */
    set_color_from_tok(ctx, HL_KEYWORD_OPERATOR, TOK_KEYWORD_OPERATOR);
    set_color_from_tok(ctx, HL_KEYWORD_IMPORT, TOK_KEYWORD_IMPORT);
    set_color_from_tok(ctx, HL_KEYWORD_TYPE, TOK_KEYWORD_TYPE);
    set_color_from_tok(ctx, HL_KEYWORD_MODIFIER, TOK_KEYWORD_MODIFIER);

    /* String subtypes */
    set_color_from_tok(ctx, HL_STRING_ESCAPE, TOK_STRING_ESCAPE);
    set_color_from_tok(ctx, HL_STRING_REGEX, TOK_STRING_REGEX);
    set_color_from_tok(ctx, HL_STRING_SPECIAL, TOK_STRING_SPECIAL);

    /* Number subtypes */
    set_color_from_tok(ctx, HL_NUMBER_FLOAT, TOK_NUMBER_FLOAT);

    /* Literals */
    set_color_from_tok(ctx, HL_BOOLEAN, TOK_BOOLEAN);
    set_color_from_tok(ctx, HL_CONSTANT, TOK_CONSTANT);

    /* Comment subtypes */
    set_color_from_tok(ctx, HL_COMMENT_DOC, TOK_COMMENT_DOC);

    /* Function subtypes */
    set_color_from_tok(ctx, HL_FUNCTION_METHOD, TOK_FUNCTION_METHOD);
    set_color_from_tok(ctx, HL_FUNCTION_MACRO, TOK_FUNCTION_MACRO);

    /* Types */
    set_color_from_tok(ctx, HL_TYPE, TOK_TYPE);
    set_color_from_tok(ctx, HL_TYPE_BUILTIN, TOK_TYPE_BUILTIN);
    set_color_from_tok(ctx, HL_TYPE_PARAMETER, TOK_TYPE_PARAMETER);
    set_color_from_tok(ctx, HL_TYPE_QUALIFIER, TOK_TYPE_QUALIFIER);

    /* Punctuation subtypes */
    set_color_from_tok(ctx, HL_PUNCTUATION_BRACKET, TOK_PUNCTUATION_BRACKET);
    set_color_from_tok(ctx, HL_PUNCTUATION_DELIMITER, TOK_PUNCTUATION_DELIMITER);

    /* Additional special types */
    set_color_from_tok(ctx, HL_MODULE, TOK_MODULE);
    set_color_from_tok(ctx, HL_TAG_ATTRIBUTE, TOK_TAG_ATTRIBUTE);
    set_color_from_tok(ctx, HL_PREPROCESSOR, TOK_PREPROCESSOR);

    /* Errors and warnings */
    set_color_from_tok(ctx, HL_ERROR, TOK_ERROR);
    set_color_from_tok(ctx, HL_WARNING, TOK_WARNING);
}
#endif

/* Fallback RGB colors for non-linenoise builds.
 * Indexed by HL_* constants from hl_types.h.
 * Format: {R, G, B} */
#ifndef LOKI_USE_LINENOISE
static const int hl_fallback_colors[HL_TYPE_COUNT][3] = {
    /* Base types (0-8) */
    [HL_NORMAL]             = {200, 200, 200},  /* Light gray */
    [HL_NONPRINT]           = {100, 100, 100},  /* Dark gray */
    [HL_COMMENT]            = {100, 100, 100},  /* Dark gray */
    [HL_MLCOMMENT]          = {100, 100, 100},  /* Dark gray */
    [HL_KEYWORD1]           = {220, 100, 220},  /* Magenta */
    [HL_KEYWORD2]           = {100, 220, 220},  /* Cyan */
    [HL_STRING]             = {220, 220, 100},  /* Yellow */
    [HL_NUMBER]             = {200, 100, 200},  /* Purple */
    [HL_MATCH]              = {100, 150, 220},  /* Blue */

    /* Extended types (9-23) */
    [HL_FUNCTION]           = { 80, 180, 220},  /* Blue/cyan */
    [HL_FUNCTION_BUILTIN]   = {100, 200, 240},  /* Bright cyan */
    [HL_FUNCTION_CALL]      = {120, 180, 200},  /* Muted cyan */
    [HL_VARIABLE_BUILTIN]   = {220, 120, 100},  /* Orange/red */
    [HL_VARIABLE_PARAMETER] = {220, 160, 100},  /* Light orange */
    [HL_OPERATOR]           = {220, 220, 220},  /* White/light gray */
    [HL_PUNCTUATION]        = {150, 150, 150},  /* Dimmer gray */
    [HL_CONSTRUCTOR]        = {100, 220, 180},  /* Green/teal */
    [HL_NAMESPACE]          = {180, 120, 220},  /* Purple */
    [HL_LABEL]              = {220, 200, 100},  /* Yellow/gold */
    [HL_TAG]                = {100, 150, 220},  /* Blue */
    [HL_KEYWORD_CONTROL]    = {220, 100, 220},  /* Magenta */
    [HL_KEYWORD_FUNCTION]   = {200, 100, 200},  /* Magenta variant */
    [HL_KEYWORD_RETURN]     = {240, 120, 240},  /* Bright magenta */
    [HL_CONSTANT_BUILTIN]   = {100, 220, 220},  /* Cyan */

    /* Additional extended types (24-50) */
    [HL_VARIABLE]           = {180, 180, 200},  /* Light foreground */
    [HL_VARIABLE_FIELD]     = {200, 140, 100},  /* Orange variant */
    [HL_VARIABLE_PROPERTY]  = {200, 150, 110},  /* Similar to field */
    [HL_KEYWORD_OPERATOR]   = {220, 100, 200},  /* Magenta */
    [HL_KEYWORD_IMPORT]     = {100, 200, 180},  /* Cyan/teal */
    [HL_KEYWORD_TYPE]       = {100, 220, 220},  /* Cyan */
    [HL_KEYWORD_MODIFIER]   = {180, 100, 200},  /* Magenta variant */
    [HL_STRING_ESCAPE]      = {240, 140, 100},  /* Bright orange/red */
    [HL_STRING_REGEX]       = {220, 160,  80},  /* Orange */
    [HL_STRING_SPECIAL]     = {200, 220, 120},  /* Yellow/green */
    [HL_NUMBER_FLOAT]       = {200, 100, 200},  /* Same as number */
    [HL_BOOLEAN]            = {140, 180, 220},  /* Cyan/purple */
    [HL_CONSTANT]           = {100, 200, 200},  /* Cyan */
    [HL_COMMENT_DOC]        = {120, 140, 120},  /* Brighter comment */
    [HL_FUNCTION_METHOD]    = {100, 180, 220},  /* Cyan variant */
    [HL_FUNCTION_MACRO]     = {120, 200, 240},  /* Brighter cyan */
    [HL_TYPE]               = {100, 220, 220},  /* Cyan */
    [HL_TYPE_BUILTIN]       = {120, 240, 240},  /* Brighter cyan */
    [HL_TYPE_PARAMETER]     = {100, 200, 180},  /* Teal */
    [HL_TYPE_QUALIFIER]     = {180, 120, 200},  /* Magenta variant */
    [HL_PUNCTUATION_BRACKET]   = {170, 170, 170},  /* Brighter gray */
    [HL_PUNCTUATION_DELIMITER] = {150, 150, 150},  /* Gray */
    [HL_MODULE]             = {180, 120, 220},  /* Purple */
    [HL_TAG_ATTRIBUTE]      = {220, 200, 100},  /* Yellow/gold */
    [HL_PREPROCESSOR]       = {200, 100, 180},  /* Magenta */
    [HL_ERROR]              = {240,  80,  80},  /* Red */
    [HL_WARNING]            = {240, 200,  80},  /* Yellow/orange */
};
#endif

/* Initialize default syntax highlighting colors.
 * Colors are stored as RGB values and rendered using true color escape codes.
 * When built with linenoise, colors come from the current theme. */
void syntax_init_default_colors(editor_ctx_t *ctx) {
#ifdef LOKI_USE_LINENOISE
    /* Use colors from the current theme */
    syntax_apply_theme_colors(ctx);
#else
    /* Apply fallback colors from static array */
    for (int i = 0; i < HL_TYPE_COUNT; i++) {
        ctx->view.colors[i].r = hl_fallback_colors[i][0];
        ctx->view.colors[i].g = hl_fallback_colors[i][1];
        ctx->view.colors[i].b = hl_fallback_colors[i][2];
    }
#endif
}
