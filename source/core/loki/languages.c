/* loki_languages.c - Language syntax infrastructure
 *
 * This file contains syntax highlighting infrastructure and the HLDB.
 * Language definitions (keywords, extensions) are in languages/ *.h headers.
 *
 * Minimal keyword arrays are kept for markdown code block highlighting.
 * For actual file editing, Lua-defined languages can extend these.
 */

#include "internal.h"
#include "syntax.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#define strncasecmp _strnicmp
#else
#include <strings.h>  /* for strncasecmp */
#endif

/* ======================= Language Definitions ============================= */
/* Language-specific keywords and extensions are defined in separate headers
 * in the syntax/ subdirectory for better organization. */

#include "syntax/lang_c.h"
#include "syntax/lang_python.h"
#include "syntax/lang_lua.h"
#include "syntax/lang_alda.h"
#include "syntax/lang_csound.h"
#include "syntax/lang_joy.h"
#include "syntax/lang_scheme.h"
#include "syntax/lang_scala.h"
#include "syntax/lang_markdown.h"
#include "syntax/lang_haskell.h"
#include "syntax/lang_bog.h"

/* ======================= Language Database (MINIMAL) ======================== */
/* Minimal static definitions kept for backward compatibility with tests.
 * Full language definitions load dynamically from Lua (.psnd/languages/).
 * These entries have minimal keywords suitable for testing and markdown code blocks. */

struct t_editor_syntax HLDB[] = {
    /* C/C++ - minimal definition for tests and markdown */
    {
        C_HL_extensions,
        C_HL_keywords,
        "//","/*","*/",
        ",.()+-/*=~%<>[]{}:;",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Python - minimal definition for tests and markdown */
    {
        Python_HL_extensions,
        Python_HL_keywords,
        "#","","",
        ",.()+-/*=~%<>[]{}:;",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Lua - minimal definition for tests and markdown */
    {
        Lua_HL_extensions,
        Lua_HL_keywords,
        "--","","",
        ",.()+-/*=~%<>[]{}:;",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Markdown - special handling via markdown module */
    {
        MD_HL_extensions,
        NULL,
        "","","",
        ",.()+-/*=~%[]{}:;",
        0,
        HL_TYPE_MARKDOWN
    },
    /* Alda music notation - built-in for REPL syntax highlighting */
    {
        Alda_HL_extensions,
        Alda_HL_keywords,
        "#","","",
        ",.()+-/*=~%[]{}:;<>|",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Csound CSD files - special handling for multi-section format */
    {
        Csound_HL_extensions,
        Csound_HL_keywords,
        ";","/*","*/",
        ",.()+-/*=~%[]{}:;<>|",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_CSOUND
    },
    /* Scala scale files (.scl) - simple format with ! comments */
    {
        Scala_HL_extensions,
        Scala_HL_keywords,
        "!","","",
        " \t/",
        HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Joy concatenative music language (.joy) */
    {
        Joy_HL_extensions,
        Joy_HL_keywords,
        "#","","",
        " \t,.()[]{}+-/*=~%<>:;|",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Scheme R7RS (.scm, .ss, .sld) */
    {
        Scheme_HL_extensions,
        Scheme_HL_keywords,
        ";","","",
        "()[]{}\"'`,@#",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Haskell / MHS (.hs, .mhs, .lhs) */
    {
        Haskell_HL_extensions,
        Haskell_HL_keywords,
        "--","{-","-}",
        " \t,.()[]{}+-/*=~%<>:;|\\@$!?",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Bog Prolog-based music (.bog) */
    {
        Bog_HL_extensions,
        Bog_HL_keywords,
        "%","/*","*/",
        " \t,.()[]{}+-/*=~<>:;|\\",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Terminator */
    {NULL, NULL, "", "", "", NULL, 0, HL_TYPE_C}
};

/* Slots in HLDB, including the {NULL,...} terminator. */
#define HLDB_SLOTS (sizeof(HLDB)/sizeof(HLDB[0]))

/* Get the number of built-in language entries (excluding the terminator). */
unsigned int loki_get_builtin_language_count(void) {
    return (unsigned int)(HLDB_SLOTS - 1);
}

/* ======================= Helper Functions for Markdown ==================== */

/* Helper function to highlight code block content with specified language rules.
 * This is a simplified version of editor_update_syntax for use within markdown. */
void highlight_code_line(t_erow *row, char **keywords, char *scs, char *separators) {
    if (row->rsize == 0) return;

    int i = 0, prev_sep = 1, in_string = 0;
    char *p = row->render;

    while (i < row->rsize) {
        /* Handle // or # comments (if scs is provided) */
        if (scs && scs[0] && prev_sep && i < row->rsize - 1 &&
            p[i] == scs[0] && (scs[1] == '\0' || p[i+1] == scs[1])) {
            memset(row->hl + i, HL_COMMENT, row->rsize - i);
            return;
        }

        /* Handle strings */
        if (in_string) {
            row->hl[i] = HL_STRING;
            if (i < row->rsize - 1 && p[i] == '\\') {
                row->hl[i+1] = HL_STRING;
                i += 2;
                prev_sep = 0;
                continue;
            }
            if (p[i] == in_string) in_string = 0;
            i++;
            prev_sep = 0;
            continue;
        }

        if (p[i] == '"' || p[i] == '\'') {
            in_string = p[i];
            row->hl[i] = HL_STRING;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers */
        if ((isdigit(p[i]) && (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
            (p[i] == '.' && i > 0 && row->hl[i-1] == HL_NUMBER)) {
            row->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle keywords */
        if (prev_sep && keywords) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen-1] == '|';
                if (kw2) klen--;

                if (!memcmp(p+i,keywords[j],klen) &&
                    (i+klen >= row->rsize || syntax_is_separator(p[i+klen], separators))) {
                    memset(row->hl+i, kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    prev_sep = 0;
                    goto next;
                }
            }
        }

        prev_sep = syntax_is_separator(p[i], separators);
        i++;
next:
        continue;
    }
}

/* Detect code block language from fence marker (e.g., ```python) */
int detect_code_block_language(const char *line) {
    /* Skip opening fence characters */
    const char *p = line;
    while (*p && (*p == '`' || *p == '~')) p++;

    /* Check language identifier */
    if (!*p) return CB_LANG_NONE;

    if (strncmp(p, "c", 1) == 0 || strncmp(p, "cpp", 3) == 0 ||
        strncmp(p, "C", 1) == 0 || strncmp(p, "C++", 3) == 0) {
        return CB_LANG_C;
    }
    if (strncmp(p, "python", 6) == 0 || strncmp(p, "py", 2) == 0) {
        return CB_LANG_PYTHON;
    }
    if (strncmp(p, "lua", 3) == 0) {
        return CB_LANG_LUA;
    }
    if (strncmp(p, "cython", 6) == 0 || strncmp(p, "pyx", 3) == 0) {
        return CB_LANG_CYTHON;
    }

    return CB_LANG_NONE;
}

/* Update syntax highlighting for markdown files (proper editor integration).
 * This is the main entry point called by the editor core. */
void editor_update_syntax_markdown(editor_ctx_t *ctx, t_erow *row) {
    /* realloc(ptr, 0) frees ptr and returns NULL; returning here on NULL
     * would leave row->hl dangling. */
    if (row->rsize == 0) {
        free(row->hl);
        row->hl = NULL;
        row->cb_lang = CB_LANG_NONE;
        return;
    }
    unsigned char *new_hl = realloc(row->hl, row->rsize);
    if (new_hl == NULL) return;
    row->hl = new_hl;
    memset(row->hl, HL_NORMAL, row->rsize);

    char *p = row->render;
    int i = 0;
    int prev_cb_lang = (row->idx > 0 && ctx && ctx->model.row) ? ctx->model.row[row->idx - 1].cb_lang : CB_LANG_NONE;

    /* Code blocks: lines starting with ``` */
    if (row->rsize >= 3 && p[0] == '`' && p[1] == '`' && p[2] == '`') {
        /* Opening or closing code fence */
        memset(row->hl, HL_STRING, row->rsize);

        if (prev_cb_lang != CB_LANG_NONE) {
            /* Closing fence */
            row->cb_lang = CB_LANG_NONE;
        } else {
            /* Opening fence - detect language */
            row->cb_lang = CB_LANG_NONE;
            if (row->rsize > 3) {
                char *lang = p + 3;
                /* Skip whitespace */
                while (*lang && isspace(*lang)) lang++;

                if (strncmp(lang, "cython", 6) == 0 ||
                    strncmp(lang, "pyx", 3) == 0 ||
                    strncmp(lang, "pxd", 3) == 0) {
                    row->cb_lang = CB_LANG_CYTHON;
                } else if (strncmp(lang, "c", 1) == 0 &&
                    (lang[1] == '\0' || isspace(lang[1]) || lang[1] == 'p')) {
                    if (lang[1] == 'p' && lang[2] == 'p') {
                        row->cb_lang = CB_LANG_C; /* C++ */
                    } else if (lang[1] == '\0' || isspace(lang[1])) {
                        row->cb_lang = CB_LANG_C; /* C */
                    }
                } else if (strncmp(lang, "python", 6) == 0 || strncmp(lang, "py", 2) == 0) {
                    row->cb_lang = CB_LANG_PYTHON;
                } else if (strncmp(lang, "lua", 3) == 0) {
                    row->cb_lang = CB_LANG_LUA;
                }
            }
        }
        return;
    }

    /* Inside code block - apply language-specific highlighting */
    if (prev_cb_lang != CB_LANG_NONE) {
        row->cb_lang = prev_cb_lang;

        char **keywords = NULL;
        char *scs = NULL;
        char *separators = ",.()+-/*=~%[];";

        switch (prev_cb_lang) {
            case CB_LANG_C:
                keywords = C_HL_keywords;
                scs = "//";
                break;
            case CB_LANG_PYTHON:
                keywords = Python_HL_keywords;
                scs = "#";
                break;
            case CB_LANG_LUA:
                keywords = Lua_HL_keywords;
                scs = "--";
                break;
            case CB_LANG_CYTHON:
                keywords = Cython_HL_keywords;
                scs = "#";
                break;
        }

        highlight_code_line(row, keywords, scs, separators);
        return;
    }

    /* Not in code block - reset */
    row->cb_lang = CB_LANG_NONE;

    /* Headers: # ## ### etc. at start of line */
    if (row->rsize > 0 && p[0] == '#') {
        int header_len = 0;
        while (header_len < row->rsize && p[header_len] == '#')
            header_len++;
        if (header_len < row->rsize && (p[header_len] == ' ' || p[header_len] == '\t')) {
            /* Valid header - highlight entire line */
            memset(row->hl, HL_KEYWORD1, row->rsize);
            return;
        }
    }

    /* Lists: lines starting with *, -, or + followed by space */
    if (row->rsize >= 2 && (p[0] == '*' || p[0] == '-' || p[0] == '+') &&
        (p[1] == ' ' || p[1] == '\t')) {
        row->hl[0] = HL_KEYWORD2;
    }

    /* Inline patterns: bold, italic, code, links */
    i = 0;
    while (i < row->rsize) {
        /* Inline code: `text` */
        if (p[i] == '`') {
            row->hl[i] = HL_STRING;
            i++;
            while (i < row->rsize && p[i] != '`') {
                row->hl[i] = HL_STRING;
                i++;
            }
            if (i < row->rsize) {
                row->hl[i] = HL_STRING; /* Closing ` */
                i++;
            }
            continue;
        }

        /* Bold: **text** */
        if (i < row->rsize - 1 && p[i] == '*' && p[i+1] == '*') {
            int start = i;
            i += 2;
            while (i < row->rsize - 1) {
                if (p[i] == '*' && p[i+1] == '*') {
                    /* Found closing ** */
                    memset(row->hl + start, HL_KEYWORD2, i - start + 2);
                    i += 2;
                    break;
                }
                i++;
            }
            continue;
        }

        /* Italic: *text* or _text_ */
        if (p[i] == '*' || p[i] == '_') {
            char marker = p[i];
            int start = i;
            i++;
            while (i < row->rsize) {
                if (p[i] == marker) {
                    /* Found closing marker */
                    memset(row->hl + start, HL_COMMENT, i - start + 1);
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        /* Links: [text](url) */
        if (p[i] == '[') {
            int start = i;
            i++;
            /* Find closing ] */
            while (i < row->rsize && p[i] != ']') i++;
            if (i < row->rsize && i + 1 < row->rsize && p[i+1] == '(') {
                /* Found ]( - continue to find ) */
                i += 2;
                while (i < row->rsize && p[i] != ')') i++;
                if (i < row->rsize) {
                    /* Complete link found */
                    memset(row->hl + start, HL_NUMBER, i - start + 1);
                    i++;
                    continue;
                }
            }
            i = start + 1; /* Not a link, continue from next char */
            continue;
        }

        i++;
    }
}

/* ======================= Csound CSD Syntax Highlighting =================== */

/* Helper: Check if line contains a CSD section tag and return the section type.
 * Returns -1 if no section change, otherwise CSD_SECTION_* constant.
 * Sets *is_closing to 1 if it's a closing tag. */
static int detect_csd_section_tag(const char *line, int len, int *is_closing) {
    *is_closing = 0;

    /* Skip leading whitespace */
    int i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;

    if (i >= len || line[i] != '<') return -1;
    i++;

    /* Check for closing tag */
    if (i < len && line[i] == '/') {
        *is_closing = 1;
        i++;
    }

    /* Match section names (case-insensitive) */
    int remaining = len - i;

    if (remaining >= 9 && strncasecmp(line + i, "CsOptions", 9) == 0) {
        return CSD_SECTION_OPTIONS;
    }
    if (remaining >= 14 && strncasecmp(line + i, "CsInstruments", 13) == 0) {
        return CSD_SECTION_ORCHESTRA;
    }
    if (remaining >= 7 && strncasecmp(line + i, "CsScore", 7) == 0) {
        return CSD_SECTION_SCORE;
    }
    if (remaining >= 17 && strncasecmp(line + i, "CsoundSynthesizer", 17) == 0) {
        return CSD_SECTION_NONE;  /* Root tag, not a content section */
    }

    return -1;
}

/* Highlight Csound orchestra code (inside <CsInstruments>) */
static void highlight_csound_orchestra(t_erow *row, char **keywords, char *separators) {
    if (row->rsize == 0) return;

    int i = 0, prev_sep = 1, in_string = 0, in_comment = 0;
    char *p = row->render;

    /* Check for multi-line comment continuation from previous row */
    /* (Csound uses C-style block comments in orchestra) */

    while (i < row->rsize) {
        /* Handle block comments */
        if (in_comment) {
            row->hl[i] = HL_MLCOMMENT;
            if (i < row->rsize - 1 && p[i] == '*' && p[i+1] == '/') {
                row->hl[i+1] = HL_MLCOMMENT;
                i += 2;
                in_comment = 0;
                prev_sep = 1;
                continue;
            }
            i++;
            continue;
        }

        /* Start of block comment */
        if (i < row->rsize - 1 && p[i] == '/' && p[i+1] == '*') {
            row->hl[i] = HL_MLCOMMENT;
            row->hl[i+1] = HL_MLCOMMENT;
            i += 2;
            in_comment = 1;
            continue;
        }

        /* Handle ; comments (to end of line) */
        if (p[i] == ';') {
            memset(row->hl + i, HL_COMMENT, row->rsize - i);
            return;
        }

        /* Handle ;; comments (double semicolon, common in Csound) */
        if (i < row->rsize - 1 && p[i] == ';' && p[i+1] == ';') {
            memset(row->hl + i, HL_COMMENT, row->rsize - i);
            return;
        }

        /* Handle strings */
        if (in_string) {
            row->hl[i] = HL_STRING;
            if (i < row->rsize - 1 && p[i] == '\\') {
                row->hl[i+1] = HL_STRING;
                i += 2;
                prev_sep = 0;
                continue;
            }
            if (p[i] == in_string) in_string = 0;
            i++;
            prev_sep = 0;
            continue;
        }

        if (p[i] == '"' || p[i] == '\'') {
            in_string = p[i];
            row->hl[i] = HL_STRING;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers (including negative and float) */
        if ((isdigit(p[i]) && (prev_sep || (i > 0 && row->hl[i-1] == HL_NUMBER))) ||
            (p[i] == '.' && i > 0 && row->hl[i-1] == HL_NUMBER) ||
            (p[i] == '-' && prev_sep && i < row->rsize - 1 && isdigit(p[i+1]))) {
            row->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle keywords */
        if (prev_sep && keywords) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen-1] == '|';
                if (kw2) klen--;

                if (i + klen <= row->rsize &&
                    !memcmp(p + i, keywords[j], klen) &&
                    (i + klen >= row->rsize || syntax_is_separator(p[i + klen], separators))) {
                    memset(row->hl + i, kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    prev_sep = 0;
                    goto next_orc;
                }
            }
        }

        prev_sep = syntax_is_separator(p[i], separators);
        i++;
next_orc:
        continue;
    }
}

/* Highlight Csound score code (inside <CsScore>) */
static void highlight_csound_score(t_erow *row) {
    if (row->rsize == 0) return;

    char *p = row->render;
    int i = 0;

    /* Skip leading whitespace */
    while (i < row->rsize && (p[i] == ' ' || p[i] == '\t')) i++;

    if (i >= row->rsize) return;

    /* Handle ; comments */
    if (p[i] == ';') {
        memset(row->hl + i, HL_COMMENT, row->rsize - i);
        return;
    }

    /* Score statements start with a letter: i, f, e, s, t, a, b, etc. */
    char stmt = p[i];
    if (isalpha(stmt)) {
        /* Highlight statement letter as keyword */
        row->hl[i] = HL_KEYWORD1;
        i++;

        /* Rest of line is parameters - highlight numbers */
        while (i < row->rsize) {
            if (p[i] == ';') {
                /* Comment to end of line */
                memset(row->hl + i, HL_COMMENT, row->rsize - i);
                return;
            }
            if (isdigit(p[i]) || p[i] == '.' ||
                (p[i] == '-' && i + 1 < row->rsize && isdigit(p[i+1]))) {
                row->hl[i] = HL_NUMBER;
            }
            i++;
        }
    }
}

/* Update syntax highlighting for Csound CSD files.
 * This handles the multi-section structure of CSD files. */
void editor_update_syntax_csound(editor_ctx_t *ctx, t_erow *row) {
    /* realloc(ptr, 0) frees ptr and returns NULL; returning here on NULL
     * would leave row->hl dangling. */
    if (row->rsize == 0) {
        free(row->hl);
        row->hl = NULL;
        row->cb_lang = CB_LANG_NONE;
        return;
    }
    unsigned char *new_hl = realloc(row->hl, row->rsize);
    if (new_hl == NULL) return;
    row->hl = new_hl;
    memset(row->hl, HL_NORMAL, row->rsize);

    char *p = row->render;

    /* Get section state from previous row */
    int prev_section = (row->idx > 0 && ctx && ctx->model.row)
        ? ctx->model.row[row->idx - 1].csd_section
        : CSD_SECTION_NONE;

    /* Check if this line changes the section */
    int is_closing = 0;
    int section_tag = detect_csd_section_tag(p, row->rsize, &is_closing);

    if (section_tag >= 0) {
        /* This line is a section tag - highlight as keyword */
        memset(row->hl, HL_KEYWORD1, row->rsize);

        if (is_closing) {
            /* Closing tag - section ends, go back to NONE */
            row->csd_section = CSD_SECTION_NONE;
        } else {
            /* Opening tag - section starts */
            row->csd_section = section_tag;
        }
        return;
    }

    /* No section change - inherit from previous row */
    row->csd_section = prev_section;

    /* Apply section-specific highlighting */
    switch (row->csd_section) {
        case CSD_SECTION_OPTIONS:
            /* Options section: mostly command-line flags, highlight as comments/strings */
            /* Simple approach: highlight -flags as keywords, rest as normal */
            for (int i = 0; i < row->rsize; i++) {
                if (p[i] == '-' && i + 1 < row->rsize && isalpha(p[i+1])) {
                    /* Flag like -d, -n, -m0, etc. */
                    row->hl[i] = HL_KEYWORD2;
                    i++;
                    while (i < row->rsize && (isalnum(p[i]) || p[i] == '-')) {
                        row->hl[i] = HL_KEYWORD2;
                        i++;
                    }
                    i--; /* Adjust for loop increment */
                }
            }
            break;

        case CSD_SECTION_ORCHESTRA:
            /* Orchestra section: full Csound language highlighting */
            highlight_csound_orchestra(row, Csound_HL_keywords,
                ",.()+-/*=~%[]{}:;<>|");
            break;

        case CSD_SECTION_SCORE:
            /* Score section: simpler syntax (i, f, e statements) */
            highlight_csound_score(row);
            break;

        default:
            /* Outside any section (e.g., XML structure) - leave as normal */
            break;
    }
}

/* ======================= Dynamic Language Registration =================== */

/* Dynamic language registry for user-defined languages */
static struct t_editor_syntax **HLDB_dynamic = NULL;
static int HLDB_dynamic_count = 0;

/* Free a single dynamically allocated language definition */
void free_dynamic_language(struct t_editor_syntax *lang) {
    if (!lang) return;

    /* Free filematch array */
    if (lang->filematch) {
        for (int i = 0; lang->filematch[i]; i++) {
            free(lang->filematch[i]);
        }
        free(lang->filematch);
    }

    /* Free keywords array */
    if (lang->keywords) {
        for (int i = 0; lang->keywords[i]; i++) {
            free(lang->keywords[i]);
        }
        free(lang->keywords);
    }

    /* Free separators string */
    if (lang->separators) {
        free(lang->separators);
    }

    free(lang);
}

/* Free all dynamically allocated languages (called at exit) */
void cleanup_dynamic_languages(void) {
    for (int i = 0; i < HLDB_dynamic_count; i++) {
        free_dynamic_language(HLDB_dynamic[i]);
    }
    free(HLDB_dynamic);
    HLDB_dynamic = NULL;
    HLDB_dynamic_count = 0;
}

/* Add a new language definition dynamically
 * Returns 0 on success, -1 on error */
int add_dynamic_language(struct t_editor_syntax *lang) {
    if (!lang) return -1;

    /* Grow the dynamic array */
    struct t_editor_syntax **new_array = realloc(HLDB_dynamic,
        sizeof(struct t_editor_syntax*) * (HLDB_dynamic_count + 1));
    if (!new_array) {
        return -1;  /* Allocation failed */
    }

    HLDB_dynamic = new_array;
    HLDB_dynamic[HLDB_dynamic_count] = lang;
    HLDB_dynamic_count++;

    return 0;
}

/* Get dynamic language by index (for iteration)
 * Returns NULL if index out of bounds */
struct t_editor_syntax *get_dynamic_language(int index) {
    if (index < 0 || index >= HLDB_dynamic_count) {
        return NULL;
    }
    return HLDB_dynamic[index];
}

/* Get count of dynamic languages */
int get_dynamic_language_count(void) {
    return HLDB_dynamic_count;
}

/* ======================= Note ============================================== */
/*
 * Language Definition System:
 *
 * This file maintains MINIMAL static definitions for backward compatibility:
 *   - C/C++ (HLDB[0])    - Minimal keywords for tests and markdown code blocks
 *   - Python (HLDB[1])   - Minimal keywords for tests and markdown code blocks
 *   - Lua (HLDB[2])      - Minimal keywords for tests and markdown code blocks
 *   - Markdown (HLDB[3]) - Special handling via editor_update_syntax_markdown()
 *
 * FULL language definitions are loaded dynamically from Lua:
 *   .psnd/languages/c.lua          - C/C++ (full keyword set, all extensions)
 *   .psnd/languages/python.lua     - Python (full keyword set, all builtins)
 *   .psnd/languages/lua.lua        - Lua (full keyword set, all builtins)
 *   .psnd/languages/cython.lua     - Cython
 *   .psnd/languages/javascript.lua - JavaScript
 *   .psnd/languages/typescript.lua - TypeScript
 *   .psnd/languages/rust.lua       - Rust
 *   .psnd/languages/go.lua         - Go
 *   .psnd/languages/java.lua       - Java
 *   .psnd/languages/swift.lua      - Swift
 *   .psnd/languages/sql.lua        - SQL
 *   .psnd/languages/shell.lua      - Shell scripts
 *   .psnd/languages/markdown.lua   - Markdown
 *
 * When opening a file:
 *   1. Editor checks static HLDB for matching extension
 *   2. If found in HLDB, uses minimal static definition
 *   3. Lua module (.psnd/modules/languages.lua) can override with full definition
 *   4. Languages are loaded on-demand (lazy loading) when needed
 *
 * Benefits of this approach:
 *   - Backward compatibility: Tests and C code can use HLDB directly
 *   - Extensibility: Users can add new languages via Lua without recompiling
 *   - Performance: Only loads language definitions when actually needed
 *   - Simplicity: Minimal C code, maximum flexibility via Lua
 */
