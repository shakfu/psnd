/**
 * @file treesitter.c
 * @brief Tree-sitter syntax highlighting for editor buffers.
 *
 * This module provides tree-sitter based syntax highlighting for the editor.
 * Supported languages: Lua, Alda, Csound, Joy, Haskell, Scheme, Prolog (Bog).
 */

#include "treesitter.h"

#ifdef LOKI_USE_LINENOISE

#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* External tree-sitter language functions */
extern const TSLanguage *tree_sitter_lua(void);
extern const TSLanguage *tree_sitter_alda(void);
extern const TSLanguage *tree_sitter_csound(void);
extern const TSLanguage *tree_sitter_joy(void);
extern const TSLanguage *tree_sitter_haskell(void);
extern const TSLanguage *tree_sitter_scheme(void);
extern const TSLanguage *tree_sitter_prolog(void);

/* Highlight query for Lua */
static const char *LUA_HIGHLIGHT_QUERY =
    "\"return\" @keyword.return\n"
    "[\"goto\" \"in\" \"local\"] @keyword\n"
    "(break_statement) @keyword\n"
    "(do_statement [\"do\" \"end\"] @keyword)\n"
    "(while_statement [\"while\" \"do\" \"end\"] @keyword.repeat)\n"
    "(repeat_statement [\"repeat\" \"until\"] @keyword.repeat)\n"
    "(if_statement [\"if\" \"elseif\" \"else\" \"then\" \"end\"] @keyword.conditional)\n"
    "(for_statement [\"for\" \"do\" \"end\"] @keyword.repeat)\n"
    "(function_declaration [\"function\" \"end\"] @keyword.function)\n"
    "(function_definition [\"function\" \"end\"] @keyword.function)\n"
    "[\"and\" \"not\" \"or\"] @keyword.operator\n"
    "(identifier) @variable\n"
    "(nil) @constant.builtin\n"
    "[(false) (true)] @boolean\n"
    "(comment) @comment\n"
    "(number) @number\n"
    "(string) @string\n"
;

/* Highlight query for Alda */
static const char *ALDA_HIGHLIGHT_QUERY =
    "(comment) @comment\n"
    "(string) @string\n"
    "(note_length) @number\n"
    "(duration_ms) @number\n"
    "(duration_s) @number\n"
    "(sexp_number) @number\n"
    "(note_letter) @constant\n"
    "(pitch) @constant\n"
    "(accidental) @operator\n"
    "(slur) @operator\n"
    "(rest) @constant.builtin\n"
    "(octave_set) @keyword\n"
    "(octave_up) @operator\n"
    "(octave_down) @operator\n"
    "(chord) @constant\n"
    "(instrument_call) @function\n"
    "(identifier) @variable\n"
    "(marker) @label\n"
    "(at_marker) @label\n"
    "(voice_marker) @keyword\n"
    "(repeat_count) @number\n"
    "(on_repetitions) @number\n"
    "(sexp_symbol) @function.builtin\n"
    "(quoted_list) @constant\n"
    "(quoted_symbol) @constant\n"
    "(barline) @punctuation.delimiter\n"
    "(dot) @operator\n"
    "(tie_duration) @operator\n"
    "\"=\" @operator\n"
    "\"/\" @operator\n"
    "\":\" @punctuation.delimiter\n"
;

/* Highlight query for Csound */
static const char *CSOUND_HIGHLIGHT_QUERY =
    "(comment) @comment\n"
    "(block_comment) @comment\n"
    "(xml_tag) @keyword.directive\n"
    "(header_var) @variable.builtin\n"
    "(instrument_keyword) @keyword.function\n"
    "(opcode_keyword) @keyword.function\n"
    "(block_keyword) @keyword\n"
    "(control_keyword) @keyword.control\n"
    "(type_keyword) @keyword\n"
    "(variable) @variable\n"
    "(pfield) @variable.parameter\n"
    "(number) @number\n"
    "(string) @string\n"
    "(operator) @operator\n"
    "(identifier) @function\n"
;

/* Highlight query for Joy */
static const char *JOY_HIGHLIGHT_QUERY =
    "(library_keyword) @keyword\n"
    "\"==\" @keyword.operator\n"
    "(boolean) @constant.builtin\n"
    "(null) @constant.builtin\n"
    "(integer) @number\n"
    "(float) @number\n"
    "(character) @string\n"
    "(string) @string\n"
    "(interpolated_string) @string\n"
    "(line_comment) @comment\n"
    "(block_comment) @comment\n"
    "(shell_escape) @comment\n"
    "(operator) @operator\n"
    "(cons_operator) @operator\n"
    "(definition name: (symbol) @function)\n"
    "(symbol) @variable\n"
    "[\"[\" \"]\"] @punctuation.bracket\n"
    "[\"{\" \"}\"] @punctuation.bracket\n"
    "\".\" @punctuation.delimiter\n"
    "(semicolon) @punctuation.delimiter\n"
;

/* Highlight query for Haskell (simplified) */
static const char *HASKELL_HIGHLIGHT_QUERY =
    "(variable) @variable\n"
    "(integer) @number\n"
    "(negation) @number\n"
    "(char) @string\n"
    "(string) @string\n"
    "(comment) @comment\n"
    "[\"(\" \")\" \"{\" \"}\" \"[\" \"]\"] @punctuation.bracket\n"
    "[\",\" \";\"] @punctuation.delimiter\n"
    "[\"if\" \"then\" \"else\" \"case\" \"of\"] @keyword.conditional\n"
    "[\"import\" \"qualified\" \"module\"] @keyword\n"
    "[\"where\" \"let\" \"in\" \"class\" \"instance\" \"data\"\n"
    " \"newtype\" \"type\" \"do\"] @keyword\n"
    "[(operator) \".\" \"=\" \"|\" \"::\" \"=>\" \"->\" \"<-\"] @operator\n"
    "(name) @type\n"
    "(constructor) @constructor\n"
;

/* Highlight query for Scheme */
static const char *SCHEME_HIGHLIGHT_QUERY =
    "[\"(\" \")\" \"[\" \"]\" \"{\" \"}\"] @punctuation.bracket\n"
    "(number) @number\n"
    "(character) @constant.builtin\n"
    "(boolean) @constant.builtin\n"
    "(symbol) @variable\n"
    "(string) @string\n"
    "(list . (symbol) @function)\n"
    "[(comment) (block_comment) (directive)] @comment\n"
;

/* Highlight query for Prolog (used for Bog language) */
static const char *PROLOG_HIGHLIGHT_QUERY =
    "(comment) @comment\n"
    "(atom) @constant\n"
    "(functional_notation function: (atom) @function.call)\n"
    "(integer) @number\n"
    "(float_number) @number\n"
    "(directive_head) @operator\n"
    "(operator_notation operator: _ @operator)\n"
    "[(open) (open_ct) (close) (open_list) \"|\" (close_list)\n"
    " (open_curly) (close_curly)] @punctuation.bracket\n"
    "[(arg_list_separator) (comma) (end) (list_notation_separator)] @punctuation.delimiter\n"
    "(double_quoted_list_notation) @string\n"
    "(variable_term) @variable\n"
;

/**
 * Map capture name to HL_* constant.
 * Supports hierarchical capture names (e.g., "function.builtin", "keyword.control").
 */
static int capture_to_hl(const char *capture_name, uint32_t len) {
    /* Keywords - check subtypes first for specificity */
    if (len >= 7 && strncmp(capture_name, "keyword", 7) == 0) {
        if (len > 8) {
            const char *subtype = capture_name + 8;  /* Skip "keyword." */
            uint32_t sublen = len - 8;
            /* Control flow keywords */
            if ((sublen >= 7 && strncmp(subtype, "control", 7) == 0) ||
                (sublen >= 11 && strncmp(subtype, "conditional", 11) == 0) ||
                (sublen >= 6 && strncmp(subtype, "repeat", 6) == 0)) {
                return HL_KEYWORD_CONTROL;
            }
            /* Function definition keywords */
            if (sublen >= 8 && strncmp(subtype, "function", 8) == 0) {
                return HL_KEYWORD_FUNCTION;
            }
            /* Return keywords */
            if (sublen >= 6 && strncmp(subtype, "return", 6) == 0) {
                return HL_KEYWORD_RETURN;
            }
            /* Operator keywords (and, or, not) */
            if (sublen >= 8 && strncmp(subtype, "operator", 8) == 0) {
                return HL_KEYWORD_OPERATOR;
            }
            /* Import keywords (import, require, use) */
            if (sublen >= 6 && strncmp(subtype, "import", 6) == 0) {
                return HL_KEYWORD_IMPORT;
            }
            /* Type keywords (type, class, struct) */
            if (sublen >= 4 && strncmp(subtype, "type", 4) == 0) {
                return HL_KEYWORD_TYPE;
            }
            /* Modifier keywords (public, private, static) */
            if (sublen >= 8 && strncmp(subtype, "modifier", 8) == 0) {
                return HL_KEYWORD_MODIFIER;
            }
        }
        return HL_KEYWORD1;  /* Generic keyword */
    }

    /* Functions - check subtypes for builtin/call/method/macro */
    if (len >= 8 && strncmp(capture_name, "function", 8) == 0) {
        if (len > 9) {
            const char *subtype = capture_name + 9;  /* Skip "function." */
            uint32_t sublen = len - 9;
            if (sublen >= 7 && strncmp(subtype, "builtin", 7) == 0) {
                return HL_FUNCTION_BUILTIN;
            }
            if (sublen >= 4 && strncmp(subtype, "call", 4) == 0) {
                return HL_FUNCTION_CALL;
            }
            if (sublen >= 6 && strncmp(subtype, "method", 6) == 0) {
                return HL_FUNCTION_METHOD;
            }
            if (sublen >= 5 && strncmp(subtype, "macro", 5) == 0) {
                return HL_FUNCTION_MACRO;
            }
        }
        return HL_FUNCTION;  /* Function definition */
    }

    /* Variables - check for builtin, parameter, field, property */
    if (len >= 8 && strncmp(capture_name, "variable", 8) == 0) {
        if (len > 9) {
            const char *subtype = capture_name + 9;  /* Skip "variable." */
            uint32_t sublen = len - 9;
            if (sublen >= 7 && strncmp(subtype, "builtin", 7) == 0) {
                return HL_VARIABLE_BUILTIN;
            }
            if (sublen >= 9 && strncmp(subtype, "parameter", 9) == 0) {
                return HL_VARIABLE_PARAMETER;
            }
            if (sublen >= 5 && strncmp(subtype, "field", 5) == 0) {
                return HL_VARIABLE_FIELD;
            }
            if (sublen >= 8 && strncmp(subtype, "property", 8) == 0) {
                return HL_VARIABLE_PROPERTY;
            }
        }
        return HL_VARIABLE;  /* Regular variables */
    }

    /* Constants - check for builtin (nil, null, etc.) */
    if (len >= 8 && strncmp(capture_name, "constant", 8) == 0) {
        if (len > 9) {
            const char *subtype = capture_name + 9;  /* Skip "constant." */
            uint32_t sublen = len - 9;
            if (sublen >= 7 && strncmp(subtype, "builtin", 7) == 0) {
                return HL_CONSTANT_BUILTIN;
            }
        }
        return HL_CONSTANT;  /* General constants */
    }

    /* Comments - check for doc comments */
    if (len >= 7 && strncmp(capture_name, "comment", 7) == 0) {
        if (len > 8) {
            const char *subtype = capture_name + 8;  /* Skip "comment." */
            uint32_t sublen = len - 8;
            if (sublen >= 3 && strncmp(subtype, "doc", 3) == 0) {
                return HL_COMMENT_DOC;
            }
        }
        return HL_COMMENT;
    }

    /* Strings - check for escape, regex, special */
    if (len >= 6 && strncmp(capture_name, "string", 6) == 0) {
        if (len > 7) {
            const char *subtype = capture_name + 7;  /* Skip "string." */
            uint32_t sublen = len - 7;
            if (sublen >= 6 && strncmp(subtype, "escape", 6) == 0) {
                return HL_STRING_ESCAPE;
            }
            if (sublen >= 5 && strncmp(subtype, "regex", 5) == 0) {
                return HL_STRING_REGEX;
            }
            if (sublen >= 7 && strncmp(subtype, "special", 7) == 0) {
                return HL_STRING_SPECIAL;
            }
        }
        return HL_STRING;
    }

    /* Numbers - check for float */
    if (len >= 6 && strncmp(capture_name, "number", 6) == 0) {
        if (len > 7) {
            const char *subtype = capture_name + 7;  /* Skip "number." */
            uint32_t sublen = len - 7;
            if (sublen >= 5 && strncmp(subtype, "float", 5) == 0) {
                return HL_NUMBER_FLOAT;
            }
        }
        return HL_NUMBER;
    }

    /* Booleans */
    if (len >= 7 && strncmp(capture_name, "boolean", 7) == 0) {
        return HL_BOOLEAN;
    }

    /* Types - check for builtin, parameter, qualifier */
    if (len >= 4 && strncmp(capture_name, "type", 4) == 0) {
        if (len > 5) {
            const char *subtype = capture_name + 5;  /* Skip "type." */
            uint32_t sublen = len - 5;
            if (sublen >= 7 && strncmp(subtype, "builtin", 7) == 0) {
                return HL_TYPE_BUILTIN;
            }
            if (sublen >= 9 && strncmp(subtype, "parameter", 9) == 0) {
                return HL_TYPE_PARAMETER;
            }
            if (sublen >= 9 && strncmp(subtype, "qualifier", 9) == 0) {
                return HL_TYPE_QUALIFIER;
            }
        }
        return HL_TYPE;
    }

    /* Operators */
    if (len >= 8 && strncmp(capture_name, "operator", 8) == 0) {
        return HL_OPERATOR;
    }

    /* Punctuation - check for bracket, delimiter */
    if (len >= 11 && strncmp(capture_name, "punctuation", 11) == 0) {
        if (len > 12) {
            const char *subtype = capture_name + 12;  /* Skip "punctuation." */
            uint32_t sublen = len - 12;
            if (sublen >= 7 && strncmp(subtype, "bracket", 7) == 0) {
                return HL_PUNCTUATION_BRACKET;
            }
            if (sublen >= 9 && strncmp(subtype, "delimiter", 9) == 0) {
                return HL_PUNCTUATION_DELIMITER;
            }
        }
        return HL_PUNCTUATION;
    }

    /* Constructors */
    if (len >= 11 && strncmp(capture_name, "constructor", 11) == 0) {
        return HL_CONSTRUCTOR;
    }

    /* Namespaces */
    if (len >= 9 && strncmp(capture_name, "namespace", 9) == 0) {
        return HL_NAMESPACE;
    }

    /* Modules */
    if (len >= 6 && strncmp(capture_name, "module", 6) == 0) {
        return HL_MODULE;
    }

    /* Labels */
    if (len >= 5 && strncmp(capture_name, "label", 5) == 0) {
        return HL_LABEL;
    }

    /* Tags (HTML, XML) - check for attribute */
    if (len >= 3 && strncmp(capture_name, "tag", 3) == 0) {
        if (len > 4) {
            const char *subtype = capture_name + 4;  /* Skip "tag." */
            uint32_t sublen = len - 4;
            if (sublen >= 9 && strncmp(subtype, "attribute", 9) == 0) {
                return HL_TAG_ATTRIBUTE;
            }
        }
        return HL_TAG;
    }

    /* Preprocessor directives */
    if (len >= 12 && strncmp(capture_name, "preprocessor", 12) == 0) {
        return HL_PREPROCESSOR;
    }

    /* Errors */
    if (len >= 5 && strncmp(capture_name, "error", 5) == 0) {
        return HL_ERROR;
    }

    /* Warnings */
    if (len >= 7 && strncmp(capture_name, "warning", 7) == 0) {
        return HL_WARNING;
    }

    return HL_NORMAL;
}

const TSLanguage *treesitter_get_language(const char *lang_name) {
    if (!lang_name) return NULL;

    if (strcmp(lang_name, "lua") == 0) {
        return tree_sitter_lua();
    }
    if (strcmp(lang_name, "alda") == 0) {
        return tree_sitter_alda();
    }
    if (strcmp(lang_name, "csound") == 0) {
        return tree_sitter_csound();
    }
    if (strcmp(lang_name, "joy") == 0) {
        return tree_sitter_joy();
    }
    if (strcmp(lang_name, "haskell") == 0) {
        return tree_sitter_haskell();
    }
    if (strcmp(lang_name, "scheme") == 0) {
        return tree_sitter_scheme();
    }
    if (strcmp(lang_name, "prolog") == 0 || strcmp(lang_name, "bog") == 0) {
        return tree_sitter_prolog();
    }

    return NULL;
}

static const char *get_highlight_query(const char *lang_name) {
    if (!lang_name) return NULL;

    if (strcmp(lang_name, "lua") == 0) {
        return LUA_HIGHLIGHT_QUERY;
    }
    if (strcmp(lang_name, "alda") == 0) {
        return ALDA_HIGHLIGHT_QUERY;
    }
    if (strcmp(lang_name, "csound") == 0) {
        return CSOUND_HIGHLIGHT_QUERY;
    }
    if (strcmp(lang_name, "joy") == 0) {
        return JOY_HIGHLIGHT_QUERY;
    }
    if (strcmp(lang_name, "haskell") == 0) {
        return HASKELL_HIGHLIGHT_QUERY;
    }
    if (strcmp(lang_name, "scheme") == 0) {
        return SCHEME_HIGHLIGHT_QUERY;
    }
    if (strcmp(lang_name, "prolog") == 0 || strcmp(lang_name, "bog") == 0) {
        return PROLOG_HIGHLIGHT_QUERY;
    }

    return NULL;
}

const char *treesitter_lang_from_filename(const char *filename) {
    if (!filename) return NULL;

    const char *ext = strrchr(filename, '.');
    if (!ext) return NULL;
    ext++; /* Skip the dot */

    if (strcmp(ext, "lua") == 0) return "lua";
    if (strcmp(ext, "alda") == 0) return "alda";
    if (strcmp(ext, "csd") == 0) return "csound";
    if (strcmp(ext, "orc") == 0) return "csound";
    if (strcmp(ext, "sco") == 0) return "csound";
    if (strcmp(ext, "joy") == 0) return "joy";
    if (strcmp(ext, "hs") == 0) return "haskell";
    if (strcmp(ext, "lhs") == 0) return "haskell";
    if (strcmp(ext, "scm") == 0) return "scheme";
    if (strcmp(ext, "ss") == 0) return "scheme";
    if (strcmp(ext, "rkt") == 0) return "scheme";
    if (strcmp(ext, "bog") == 0) return "prolog";
    if (strcmp(ext, "pl") == 0) return "prolog";

    return NULL;
}

TreeSitterState *treesitter_init(const char *lang_name) {
    uint32_t error_offset;
    TSQueryError error_type;

    const TSLanguage *language = treesitter_get_language(lang_name);
    if (!language) {
        return NULL;
    }

    const char *query_str = get_highlight_query(lang_name);
    if (!query_str) {
        return NULL;
    }

    TreeSitterState *ts = calloc(1, sizeof(TreeSitterState));
    if (!ts) {
        return NULL;
    }

    ts->language = language;

    /* Create parser */
    ts->parser = ts_parser_new();
    if (!ts->parser) {
        free(ts);
        return NULL;
    }

    if (!ts_parser_set_language(ts->parser, language)) {
        ts_parser_delete(ts->parser);
        free(ts);
        return NULL;
    }

    /* Create query */
    ts->query = ts_query_new(
        language,
        query_str,
        (uint32_t)strlen(query_str),
        &error_offset,
        &error_type
    );
    if (!ts->query) {
        ts_parser_delete(ts->parser);
        free(ts);
        return NULL;
    }

    /* Create cursor */
    ts->cursor = ts_query_cursor_new();
    if (!ts->cursor) {
        ts_query_delete(ts->query);
        ts_parser_delete(ts->parser);
        free(ts);
        return NULL;
    }

    return ts;
}

void treesitter_free(TreeSitterState *ts) {
    if (!ts) return;

    if (ts->cursor) {
        ts_query_cursor_delete(ts->cursor);
    }
    if (ts->query) {
        ts_query_delete(ts->query);
    }
    if (ts->tree) {
        ts_tree_delete(ts->tree);
    }
    if (ts->parser) {
        ts_parser_delete(ts->parser);
    }
    if (ts->source) {
        free(ts->source);
    }

    free(ts);
}

void treesitter_reparse(TreeSitterState *ts, const char *source, size_t len) {
    if (!ts || !ts->parser) return;

    /* Store source for later use */
    if (ts->source_cap < len + 1) {
        char *new_source = realloc(ts->source, len + 1);
        if (!new_source) return;
        ts->source = new_source;
        ts->source_cap = len + 1;
    }
    memcpy(ts->source, source, len);
    ts->source[len] = '\0';
    ts->source_len = len;

    /* Delete old tree if exists */
    if (ts->tree) {
        ts_tree_delete(ts->tree);
    }

    /* Parse the source */
    ts->tree = ts_parser_parse_string(ts->parser, NULL, source, (uint32_t)len);
}

void treesitter_edit(TreeSitterState *ts, TSInputEdit *edit) {
    if (!ts || !ts->tree || !edit) return;

    ts_tree_edit(ts->tree, edit);
}

void treesitter_update_row(editor_ctx_t *ctx, t_erow *row, TreeSitterState *ts) {
    if (!ctx || !row || !ts || !ts->parser || !ts->query || !ts->cursor) {
        return;
    }

    /* Parse just this row for simple highlighting */
    TSTree *tree = ts_parser_parse_string(ts->parser, NULL, row->render, (uint32_t)row->rsize);
    if (!tree) {
        return;
    }

    TSNode root = ts_tree_root_node(tree);

    /* Execute query */
    ts_query_cursor_exec(ts->cursor, ts->query, root);

    /* Apply captures to highlight array */
    TSQueryMatch match;
    uint32_t capture_index;

    while (ts_query_cursor_next_capture(ts->cursor, &match, &capture_index)) {
        TSQueryCapture capture = match.captures[capture_index];
        uint32_t start = ts_node_start_byte(capture.node);
        uint32_t end = ts_node_end_byte(capture.node);
        uint32_t name_len;
        const char *capture_name;
        int hl_type;

        capture_name = ts_query_capture_name_for_id(ts->query, capture.index, &name_len);
        hl_type = capture_to_hl(capture_name, name_len);

        if (start < (uint32_t)row->rsize && hl_type != HL_NORMAL) {
            if (end > (uint32_t)row->rsize) {
                end = (uint32_t)row->rsize;
            }
            for (uint32_t i = start; i < end; i++) {
                /* Only set if not already set (first match wins) */
                if (row->hl[i] == HL_NORMAL) {
                    row->hl[i] = (unsigned char)hl_type;
                }
            }
        }
    }

    ts_tree_delete(tree);
}

#endif /* LOKI_USE_LINENOISE */
