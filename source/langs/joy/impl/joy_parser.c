/**
 * joy_parser.c - Simple Joy tokenizer and parser
 */

#include "joy_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
/* Windows (MSVC and MinGW with -std=c11) doesn't have strndup */
static char* strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* result = (char*)malloc(len + 1);
    if (result) {
        memcpy(result, s, len);
        result[len] = '\0';
    }
    return result;
}
#endif

/* ---------- Symbol Transformer ---------- */

static JoySymbolTransformer g_symbol_transformer = NULL;

void joy_set_symbol_transformer(JoySymbolTransformer transformer) {
    g_symbol_transformer = transformer;
}

/* ---------- Parser Dictionary for DEFINE ---------- */

static JoyDict* g_parser_dict = NULL;

void joy_set_parser_dict(JoyDict* dict) {
    g_parser_dict = dict;
}

/* ---------- Tokenizer ---------- */

typedef enum {
    TOK_EOF,
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_STRING,
    TOK_CHAR,
    TOK_SYMBOL,
    TOK_LBRACKET,   /* [ */
    TOK_RBRACKET,   /* ] */
    TOK_LBRACE,     /* { */
    TOK_RBRACE,     /* } */
    TOK_TRUE,
    TOK_FALSE
} TokenType;

typedef struct {
    TokenType type;
    union {
        int64_t integer;
        double floating;
        char* string;
        char character;
    } value;
#ifdef SHARED_SOURCE_TRACKING
    int source_line;    /* Line number where token starts (1-based) */
#endif
} Token;

typedef struct {
    const char* source;
    size_t pos;
    size_t length;
    Token current;
#ifdef SHARED_SOURCE_TRACKING
    int line;           /* Current line number (1-based) */
    int column;         /* Current column (1-based) */
#endif
} Lexer;

static void lexer_init(Lexer* lex, const char* source) {
    lex->source = source;
    lex->pos = 0;
    lex->length = strlen(source);
    lex->current.type = TOK_EOF;
    lex->current.value.string = NULL;
#ifdef SHARED_SOURCE_TRACKING
    lex->line = 1;
    lex->column = 1;
    lex->current.source_line = 1;
#endif
}

static char lexer_peek(Lexer* lex) {
    if (lex->pos >= lex->length) return '\0';
    return lex->source[lex->pos];
}

static char lexer_advance(Lexer* lex) {
    if (lex->pos >= lex->length) return '\0';
    char c = lex->source[lex->pos++];
#ifdef SHARED_SOURCE_TRACKING
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
#endif
    return c;
}

static void skip_whitespace_and_comments(Lexer* lex) {
    while (lex->pos < lex->length) {
        char c = lexer_peek(lex);

        /* Whitespace */
        if (isspace(c)) {
            lexer_advance(lex);
            continue;
        }

        /* Line comment: \ to end of line */
        if (c == '\\') {
            while (lex->pos < lex->length && lexer_peek(lex) != '\n') {
                lexer_advance(lex);
            }
            continue;
        }

        /* Block comment: (* ... *) */
        if (c == '(' && lex->pos + 1 < lex->length && lex->source[lex->pos + 1] == '*') {
            lexer_advance(lex); /* ( */
            lexer_advance(lex); /* * */
            int depth = 1;
            while (lex->pos < lex->length && depth > 0) {
                c = lexer_advance(lex);
                if (c == '(' && lexer_peek(lex) == '*') {
                    lexer_advance(lex);
                    depth++;
                } else if (c == '*' && lexer_peek(lex) == ')') {
                    lexer_advance(lex);
                    depth--;
                }
            }
            continue;
        }

        break;
    }
}

static char* lexer_read_string(Lexer* lex) {
    lexer_advance(lex); /* skip opening " */

    size_t capacity = 64;
    char* buffer = malloc(capacity);
    if (!buffer) return NULL;
    size_t len = 0;

    while (lex->pos < lex->length) {
        char c = lexer_advance(lex);
        if (c == '"') break;

        if (c == '\\' && lex->pos < lex->length) {
            c = lexer_advance(lex);
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                default: break;
            }
        }

        if (len + 1 >= capacity) {
            /* Keep the original block if realloc fails: assigning the result
             * straight into `buffer` would both leak it and leave a NULL to
             * write through on the next line. */
            char* grown = realloc(buffer, capacity * 2);
            if (!grown) {
                free(buffer);
                return NULL;
            }
            buffer = grown;
            capacity *= 2;
        }
        buffer[len++] = c;
    }

    buffer[len] = '\0';
    return buffer;
}

static char lexer_read_char(Lexer* lex) {
    lexer_advance(lex); /* skip ' */
    char c = lexer_advance(lex);

    if (c == '\\' && lex->pos < lex->length) {
        c = lexer_advance(lex);
        switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            default: break;
        }
    }

    return c;
}

static int is_symbol_char(char c) {
    if (isalnum(c)) return 1;
    /* Note: '.' is NOT a symbol char - it's a statement terminator in Joy */
    if (c == '_' || c == '-' || c == '?' || c == '!' || c == '=' ||
        c == '<' || c == '>' || c == '+' || c == '*' || c == '/' ||
        c == '%' || c == '&' || c == '|' || c == '^' || c == '~' ||
        c == '@' || c == '#' || c == '$' || c == ':') return 1;
    return 0;
}

static void lexer_next(Lexer* lex) {
    /* Free previous string if any */
    if ((lex->current.type == TOK_STRING || lex->current.type == TOK_SYMBOL)
        && lex->current.value.string) {
        free(lex->current.value.string);
        lex->current.value.string = NULL;
    }

    skip_whitespace_and_comments(lex);

    if (lex->pos >= lex->length) {
        lex->current.type = TOK_EOF;
        return;
    }

#ifdef SHARED_SOURCE_TRACKING
    /* Record line number at start of token */
    lex->current.source_line = lex->line;
#endif

    char c = lexer_peek(lex);

    /* Brackets */
    if (c == '[') { lexer_advance(lex); lex->current.type = TOK_LBRACKET; return; }
    if (c == ']') { lexer_advance(lex); lex->current.type = TOK_RBRACKET; return; }
    if (c == '{') { lexer_advance(lex); lex->current.type = TOK_LBRACE; return; }
    if (c == '}') { lexer_advance(lex); lex->current.type = TOK_RBRACE; return; }

    /* String */
    if (c == '"') {
        lex->current.type = TOK_STRING;
        lex->current.value.string = lexer_read_string(lex);
        return;
    }

    /* Character */
    if (c == '\'') {
        lex->current.type = TOK_CHAR;
        lex->current.value.character = lexer_read_char(lex);
        return;
    }

    /* Number or symbol starting with - */
    if (isdigit(c) || (c == '-' && lex->pos + 1 < lex->length && isdigit(lex->source[lex->pos + 1]))) {
        size_t start = lex->pos;
        if (c == '-') lexer_advance(lex);

        while (lex->pos < lex->length && isdigit(lexer_peek(lex))) {
            lexer_advance(lex);
        }

        /* Check for float */
        if (lexer_peek(lex) == '.' && lex->pos + 1 < lex->length && isdigit(lex->source[lex->pos + 1])) {
            lexer_advance(lex); /* . */
            while (lex->pos < lex->length && isdigit(lexer_peek(lex))) {
                lexer_advance(lex);
            }
            /* Exponent */
            if (lexer_peek(lex) == 'e' || lexer_peek(lex) == 'E') {
                lexer_advance(lex);
                if (lexer_peek(lex) == '+' || lexer_peek(lex) == '-') lexer_advance(lex);
                while (lex->pos < lex->length && isdigit(lexer_peek(lex))) {
                    lexer_advance(lex);
                }
            }

            char* num_str = strndup(lex->source + start, lex->pos - start);
            lex->current.type = TOK_FLOAT;
            lex->current.value.floating = strtod(num_str, NULL);
            free(num_str);
            return;
        }

        char* num_str = strndup(lex->source + start, lex->pos - start);
        lex->current.type = TOK_INTEGER;
        lex->current.value.integer = strtoll(num_str, NULL, 10);
        free(num_str);
        return;
    }

    /* Period - Joy statement terminator, also used as print primitive */
    if (c == '.') {
        lexer_advance(lex);
        lex->current.type = TOK_SYMBOL;
        lex->current.value.string = strdup(".");
        return;
    }

    /* Semicolon - DEFINE definition separator */
    if (c == ';') {
        lexer_advance(lex);
        lex->current.type = TOK_SYMBOL;
        lex->current.value.string = strdup(";");
        return;
    }

    /* Symbol */
    if (is_symbol_char(c)) {
        size_t start = lex->pos;
        while (lex->pos < lex->length && is_symbol_char(lexer_peek(lex))) {
            lexer_advance(lex);
        }

        char* sym = strndup(lex->source + start, lex->pos - start);

        /* Check for boolean literals */
        if (strcmp(sym, "true") == 0) {
            free(sym);
            lex->current.type = TOK_TRUE;
            return;
        }
        if (strcmp(sym, "false") == 0) {
            free(sym);
            lex->current.type = TOK_FALSE;
            return;
        }

        lex->current.type = TOK_SYMBOL;
        lex->current.value.string = sym;
        return;
    }

    /* Unknown character - skip it */
    lexer_advance(lex);
    lexer_next(lex);
}

/* ---------- Parser ---------- */

static JoyValue parse_value(Lexer* lex);

static JoyValue parse_list(Lexer* lex) {
    lexer_next(lex); /* skip [ */

    JoyList* list = joy_list_new(8);

    while (lex->current.type != TOK_RBRACKET && lex->current.type != TOK_EOF) {
        JoyValue item = parse_value(lex);
        joy_list_push(list, item);
    }

    if (lex->current.type == TOK_RBRACKET) {
        lexer_next(lex); /* skip ] */
    }

    JoyValue v = {.type = JOY_LIST};
    v.data.list = list;
    return v;
}

static JoyValue parse_set(Lexer* lex) {
    lexer_next(lex); /* skip { */

    uint64_t set = 0;

    while (lex->current.type != TOK_RBRACE && lex->current.type != TOK_EOF) {
        if (lex->current.type == TOK_INTEGER) {
            int64_t n = lex->current.value.integer;
            if (n >= 0 && n < 64) {
                set |= (1ULL << n);
            }
            lexer_next(lex);
        } else {
            /* Skip invalid set members */
            lexer_next(lex);
        }
    }

    if (lex->current.type == TOK_RBRACE) {
        lexer_next(lex); /* skip } */
    }

    JoyValue v = {.type = JOY_SET};
    v.data.set = set;
    return v;
}

static JoyValue parse_value(Lexer* lex) {
    JoyValue v;

    switch (lex->current.type) {
        case TOK_INTEGER:
            v = joy_integer(lex->current.value.integer);
            lexer_next(lex);
            return v;

        case TOK_FLOAT:
            v = joy_float(lex->current.value.floating);
            lexer_next(lex);
            return v;

        case TOK_STRING:
            v = joy_string(lex->current.value.string);
            lexer_next(lex);
            return v;

        case TOK_CHAR:
            v = joy_char(lex->current.value.character);
            lexer_next(lex);
            return v;

        case TOK_TRUE:
            v = joy_boolean(true);
            lexer_next(lex);
            return v;

        case TOK_FALSE:
            v = joy_boolean(false);
            lexer_next(lex);
            return v;

        case TOK_SYMBOL:
            /* Check dictionary first - user definitions override transformers */
            if (g_parser_dict && joy_dict_lookup(g_parser_dict, lex->current.value.string)) {
                v = joy_symbol(lex->current.value.string);
                lexer_next(lex);
                return v;
            }
            /* Check if symbol transformer wants to convert this */
            if (g_symbol_transformer &&
                g_symbol_transformer(lex->current.value.string, &v)) {
                lexer_next(lex);
                return v;
            }
            v = joy_symbol(lex->current.value.string);
            lexer_next(lex);
            return v;

        case TOK_LBRACKET:
            return parse_list(lex);

        case TOK_LBRACE:
            return parse_set(lex);

        default:
            /* Return an empty symbol for unknown tokens */
            v = joy_symbol("");
            lexer_next(lex);
            return v;
    }
}

/* ---------- DEFINE Parsing ---------- */

/*
 * Check if symbol is a definition keyword (DEFINE, def, LIBRA, CONST)
 * All have the same semantics: name == body ; or name == body .
 */
static bool is_define_keyword(const char* sym) {
    return strcmp(sym, "DEFINE") == 0 ||
           strcmp(sym, "def") == 0 ||
           strcmp(sym, "LIBRA") == 0 ||
           strcmp(sym, "CONST") == 0;
}

/*
 * Check if symbol is a SEQ keyword (SEQ, seq)
 * Syntax: SEQ name body . (defines a named sequence)
 */
static bool is_seq_keyword(const char* sym) {
    return strcmp(sym, "SEQ") == 0 ||
           strcmp(sym, "seq") == 0;
}

/*
 * Parse a single definition: name == term1 term2 ... (terminated by ; or .)
 * Returns true if more definitions follow (;), false if done (.)
 */
static bool parse_single_definition(Lexer* lex) {
    /* Expect name */
    if (lex->current.type != TOK_SYMBOL) {
        fprintf(stderr, "DEFINE: expected name, got token type %d\n", lex->current.type);
        return false;
    }

    char* name = strdup(lex->current.value.string);
    lexer_next(lex);

    /* Expect == */
    if (lex->current.type != TOK_SYMBOL ||
        strcmp(lex->current.value.string, "==") != 0) {
        fprintf(stderr, "DEFINE: expected '==' after '%s'\n", name);
        free(name);
        return false;
    }
    lexer_next(lex);

    /* Parse terms until ; or . */
    JoyQuotation* body = joy_quotation_new(8);
    bool more_defs = false;

    while (lex->current.type != TOK_EOF) {
        /* Check for terminators */
        if (lex->current.type == TOK_SYMBOL) {
            if (strcmp(lex->current.value.string, ";") == 0) {
                lexer_next(lex);
                more_defs = true;
                break;
            }
            if (strcmp(lex->current.value.string, ".") == 0) {
                lexer_next(lex);
                more_defs = false;
                break;
            }
        }

        /* Parse term */
        JoyValue term = parse_value(lex);
        joy_quotation_push(body, term);
    }

    /* Register definition if we have a dictionary */
    if (g_parser_dict) {
        joy_dict_define_quotation(g_parser_dict, name, body);
    } else {
        /* No dictionary - just free the body */
        joy_quotation_free(body);
    }

    free(name);
    return more_defs;
}

/*
 * Parse DEFINE block: DEFINE name == prog . or DEFINE n1 == p1 ; n2 == p2 .
 */
static void parse_define_block(Lexer* lex) {
    /* Skip past DEFINE/def keyword */
    lexer_next(lex);

    /* Parse definitions until . terminator */
    while (lex->current.type != TOK_EOF) {
        if (!parse_single_definition(lex)) {
            break;  /* . terminator or error */
        }
        /* ; terminator - continue with next definition */
    }
}

/*
 * Check if current position has a channel prefix: N: (digit followed by colon)
 * Returns the channel number (1-16) if found, 0 otherwise.
 * Does not consume tokens if no channel prefix found.
 */
static int peek_channel_prefix(Lexer* lex) {
    if (lex->current.type != TOK_INTEGER) return 0;

    int channel = (int)lex->current.value.integer;
    if (channel < 1 || channel > 16) return 0;

    /* Save position to check for colon */
    size_t saved_pos = lex->pos;

    /* Skip whitespace to find colon */
    while (lex->pos < lex->length &&
           (lex->source[lex->pos] == ' ' || lex->source[lex->pos] == '\t')) {
        lex->pos++;
    }

    /* Check for colon */
    if (lex->pos < lex->length && lex->source[lex->pos] == ':') {
        /* Restore position - caller will consume the tokens */
        lex->pos = saved_pos;
        return channel;
    }

    /* No colon - restore and return 0 */
    lex->pos = saved_pos;
    return 0;
}

/*
 * Consume channel prefix: N: (already verified by peek_channel_prefix)
 */
static void consume_channel_prefix(Lexer* lex) {
    lexer_next(lex);  /* consume the number */

    /* Skip whitespace and consume colon */
    while (lex->pos < lex->length &&
           (lex->source[lex->pos] == ' ' || lex->source[lex->pos] == '\t')) {
        lex->pos++;
    }
    if (lex->pos < lex->length && lex->source[lex->pos] == ':') {
        lex->pos++;  /* consume colon */
    }

    /* Re-scan next token */
    lexer_next(lex);
}

/*
 * Parse a SEQ block: SEQ name parts .
 * Parts have format: N: code; (channel prefix with code until ; or .)
 * Multiple parts execute in parallel when the sequence is played.
 *
 * Example:
 *   SEQ melody
 *       1: [c4 e4 g4] arp;
 *       2: [c3 e3 g3] .
 */
static void parse_seq_block(Lexer* lex) {
    /* Skip past SEQ/seq keyword */
    lexer_next(lex);

    /* Expect name */
    if (lex->current.type != TOK_SYMBOL) {
        fprintf(stderr, "SEQ: expected name\n");
        return;
    }

    char* name = strdup(lex->current.value.string);
    lexer_next(lex);

    /* Create sequence definition */
    SeqDefinition* seq = seq_definition_new();

    /* Parse parts until . terminator */
    while (lex->current.type != TOK_EOF) {
        /* Check for . terminator */
        if (lex->current.type == TOK_SYMBOL &&
            strcmp(lex->current.value.string, ".") == 0) {
            lexer_next(lex);  /* consume . */
            break;
        }

        /* Skip ; separators */
        if (lex->current.type == TOK_SYMBOL &&
            strcmp(lex->current.value.string, ";") == 0) {
            lexer_next(lex);
            continue;
        }

        /* Check for channel prefix N: */
        int channel = peek_channel_prefix(lex);
        if (channel > 0) {
            consume_channel_prefix(lex);

            /* Parse code for this channel until ; or . */
            JoyQuotation* part_body = joy_quotation_new(16);

            while (lex->current.type != TOK_EOF) {
                /* Check for ; or . */
                if (lex->current.type == TOK_SYMBOL &&
                    (strcmp(lex->current.value.string, ";") == 0 ||
                     strcmp(lex->current.value.string, ".") == 0)) {
                    break;
                }

                /* Parse value and add to part body */
                JoyValue v = parse_value(lex);
                joy_quotation_push(part_body, v);
            }

            /* Add part to sequence */
            seq_definition_add_part(seq, channel, part_body);
        } else {
            /* No channel prefix - error or skip */
            fprintf(stderr, "SEQ %s: expected channel prefix (N:), got ", name);
            if (lex->current.type == TOK_SYMBOL) {
                fprintf(stderr, "'%s'\n", lex->current.value.string);
            } else if (lex->current.type == TOK_INTEGER) {
                fprintf(stderr, "%lld\n", lex->current.value.integer);
            } else {
                fprintf(stderr, "token type %d\n", lex->current.type);
            }
            lexer_next(lex);  /* skip problematic token */
        }
    }

    /* Register the sequence */
    if (g_parser_dict && seq->part_count > 0) {
        joy_dict_define_seq(g_parser_dict, name, seq);
    } else {
        seq_definition_free(seq);
    }
    free(name);
}

/* ---------- Public API ---------- */

/*
 * Check if current position starts a bare definition: name == body .
 * Returns true if current token is a symbol and next token is ==
 */
static bool is_bare_definition(Lexer* lex) {
    if (lex->current.type != TOK_SYMBOL) return false;

    /* Save current position to peek ahead */
    size_t saved_pos = lex->pos;

    /* Skip whitespace and comments to find next token */
    skip_whitespace_and_comments(lex);

    /* Check if next chars are == */
    bool is_def = (lex->pos + 1 < lex->length &&
                   lex->source[lex->pos] == '=' &&
                   lex->source[lex->pos + 1] == '=');

    /* Restore position */
    lex->pos = saved_pos;

    return is_def;
}

JoyQuotation* joy_parse(const char* source) {
    Lexer lex;
    lexer_init(&lex, source);
    lexer_next(&lex);

    JoyQuotation* quot = joy_quotation_new(16);

    while (lex.current.type != TOK_EOF) {
        /* Check for DEFINE/def/LIBRA/CONST keyword */
        if (lex.current.type == TOK_SYMBOL && is_define_keyword(lex.current.value.string)) {
            parse_define_block(&lex);
            continue;
        }

        /* Check for SEQ/seq keyword */
        if (lex.current.type == TOK_SYMBOL && is_seq_keyword(lex.current.value.string)) {
            parse_seq_block(&lex);
            continue;
        }

        /* Check for bare definition: name == body . */
        if (is_bare_definition(&lex)) {
            /* parse_single_definition expects to be AT the name token */
            parse_single_definition(&lex);
            continue;
        }

        JoyValue v = parse_value(&lex);
        joy_quotation_push(quot, v);
    }

    /* Cleanup lexer */
    if ((lex.current.type == TOK_STRING || lex.current.type == TOK_SYMBOL)
        && lex.current.value.string) {
        free(lex.current.value.string);
    }

    return quot;
}

void joy_eval_line(JoyContext* ctx, const char* line) {
    JoyQuotation* quot = joy_parse(line);
    jmp_buf* outer = ctx->error_jmp;
    jmp_buf local;
    if (outer) {
        /* A Joy error longjmps to the caller; free quot on the way */
        ctx->error_jmp = &local;
        if (setjmp(local) != 0) {
            ctx->error_jmp = outer;
            joy_quotation_free(quot);
            longjmp(*outer, 1);
        }
    }
    joy_execute_quotation(ctx, quot);
    ctx->error_jmp = outer;
    joy_quotation_free(quot);
}

int joy_load_file(JoyContext* ctx, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        return -1;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return -1;
    }

    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);

    /* Parse and execute */
    joy_eval_line(ctx, content);
    free(content);

    /* Call post-eval hook (e.g., for MIDI schedule playback) */
    if (ctx->post_eval_hook) {
        ctx->post_eval_hook();
    }

    return 0;
}
