#include "bog.h"
#include "builtins.h"
#include "internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- Memory arena ---------------- */

typedef struct ArenaBlock {
    struct ArenaBlock* next;
    size_t capacity;
    size_t used;
    unsigned char data[];
} ArenaBlock;

struct BogArena {
    ArenaBlock* head;
    size_t default_block;
};

static ArenaBlock* arena_block_create(size_t capacity)
{
    ArenaBlock* block = (ArenaBlock*)malloc(sizeof(ArenaBlock) + capacity);
    if (!block)
        return NULL;
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

BogArena* bog_arena_create(void)
{
    BogArena* arena = (BogArena*)calloc(1, sizeof(BogArena));
    if (!arena)
        return NULL;
    arena->default_block = 4096;
    arena->head = arena_block_create(arena->default_block);
    return arena;
}

void bog_arena_destroy(BogArena* arena)
{
    if (!arena)
        return;
    ArenaBlock* block = arena->head;
    while (block) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    free(arena);
}

void* bog_arena_alloc(BogArena* arena, size_t size)
{
    if (!arena || !size)
        return NULL;
    size = (size + 7) & ~(size_t)7;
    if (!arena->head || arena->head->used + size > arena->head->capacity) {
        size_t cap = size > arena->default_block ? size : arena->default_block;
        ArenaBlock* block = arena_block_create(cap);
        if (!block)
            return NULL; /* allocation failed: propagate NULL to caller */
        block->next = arena->head;
        arena->head = block;
    }
    unsigned char* ptr = arena->head->data + arena->head->used;
    arena->head->used += size;
    memset(ptr, 0, size);
    return ptr;
}

const char* bog_arena_strdup(BogArena* arena, const char* src)
{
    size_t len = strlen(src) + 1;
    char* dst = (char*)bog_arena_alloc(arena, len);
    memcpy(dst, src, len);
    return dst;
}

/* ---------------- Environment helpers ---------------- */

void bog_env_init(BogEnv* env)
{
    env->items = NULL;
    env->count = 0;
    env->capacity = 0;
}

void bog_env_free(BogEnv* env)
{
    if (!env)
        return;
    free(env->items);
    env->items = NULL;
    env->count = 0;
    env->capacity = 0;
}

static void env_reserve(BogEnv* env, size_t needed)
{
    if (env->capacity >= needed)
        return;
    size_t cap = env->capacity ? env->capacity * 2 : 8;
    if (cap < needed)
        cap = needed;
    env->items = (BogBinding*)realloc(env->items,
                                          cap * sizeof(BogBinding));
    env->capacity = cap;
}

BogEnv bog_env_clone(const BogEnv* env)
{
    BogEnv copy;
    copy.count = env->count;
    copy.capacity = env->count;
    copy.items = NULL;
    if (env->count) {
        copy.items = (BogBinding*)malloc(env->count
                                             * sizeof(BogBinding));
        memcpy(copy.items, env->items, env->count * sizeof(BogBinding));
    }
    return copy;
}

BogTerm* bog_env_get(const BogEnv* env, const char* name)
{
    for (size_t i = 0; i < env->count; ++i) {
        if (strcmp(env->items[i].name, name) == 0)
            return env->items[i].value;
    }
    return NULL;
}

void bog_env_set(BogEnv* env, const char* name, BogTerm* value)
{
    for (size_t i = 0; i < env->count; ++i) {
        if (strcmp(env->items[i].name, name) == 0) {
            env->items[i].value = value;
            return;
        }
    }
    env_reserve(env, env->count + 1);
    env->items[env->count].name = name;
    env->items[env->count].value = value;
    env->count++;
}

/* ---------------- Tokenizer ---------------- */

typedef enum { TOKEN_SYM, TOKEN_IDENT, TOKEN_NUMBER, TOKEN_EOF } TokenType;

typedef struct {
    TokenType type;
    char* text;
    double number;
    int line;  /* 1-based source line number */
} Token;

typedef struct {
    Token* data;
    size_t count;
    size_t capacity;
} TokenVec;

static void token_vec_init(TokenVec* vec)
{
    vec->data = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

static void token_vec_push(TokenVec* vec, Token tok)
{
    if (vec->count == vec->capacity) {
        size_t cap = vec->capacity ? vec->capacity * 2 : 64;
        vec->data = (Token*)realloc(vec->data, cap * sizeof(Token));
        vec->capacity = cap;
    }
    vec->data[vec->count++] = tok;
}

static void token_vec_free(TokenVec* vec)
{
    if (!vec)
        return;
    for (size_t i = 0; i < vec->count; ++i) {
        free(vec->data[i].text);
    }
    free(vec->data);
    vec->data = NULL;
    vec->count = vec->capacity = 0;
}

static bool is_ident_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_ident_part(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static bool match_seq(const char* src, size_t pos, size_t len,
                      const char* pattern)
{
    size_t plen = strlen(pattern);
    if (pos + plen > len)
        return false;
    return strncmp(src + pos, pattern, plen) == 0;
}

static char* substr_copy(const char* src, size_t start, size_t len)
{
    char* out = (char*)malloc(len + 1);
    memcpy(out, src + start, len);
    out[len] = '\0';
    return out;
}

static void tokenize_source(const char* src, TokenVec* out,
                            char** error_message)
{
    size_t len = strlen(src);
    size_t i = 0;
    int line = 1;  /* Track current line (1-based) */
    while (i < len) {
        char c = src[i];
        if (isspace((unsigned char)c)) {
            if (c == '\n')
                line++;
            i++;
            continue;
        }
        if (c == '%') {
            while (i < len && src[i] != '\n' && src[i] != '\r')
                i++;
            continue;
        }
        if (is_ident_start(c)) {
            size_t start = i++;
            while (i < len && is_ident_part(src[i]))
                i++;
            Token tok = { .type = TOKEN_IDENT,
                          .text = substr_copy(src, start, i - start),
                          .line = line };
            token_vec_push(out, tok);
            continue;
        }
        if (isdigit((unsigned char)c)) {
            size_t start = i++;
            while (i < len && isdigit((unsigned char)src[i]))
                i++;
            /* consume decimal point only if followed by a digit */
            if (i < len && src[i] == '.' && i + 1 < len
                && isdigit((unsigned char)src[i + 1])) {
                i++;
                while (i < len && isdigit((unsigned char)src[i]))
                    i++;
            }
            char buf[64];
            size_t seg = i - start;
            if (seg >= sizeof(buf)) {
                *error_message = strdup("Numeric literal too long");
                return;
            }
            memcpy(buf, src + start, seg);
            buf[seg] = '\0';
            Token tok = { .type = TOKEN_NUMBER, .number = strtod(buf, NULL),
                          .line = line };
            token_vec_push(out, tok);
            continue;
        }
        const char* multi[] = { "=:=", "=\\=", "=<", ">=", ":-", "\\+", NULL };
        bool matched = false;
        for (const char** m = multi; *m; ++m) {
            if (match_seq(src, i, len, *m)) {
                Token tok = { .type = TOKEN_SYM,
                              .text = substr_copy(src, i, strlen(*m)),
                              .line = line };
                token_vec_push(out, tok);
                i += strlen(*m);
                matched = true;
                break;
            }
        }
        if (matched)
            continue;
        const char singles[] = "()[],.;|+-*/<>= ";
        if (strchr(singles, c)) {
            if (c == ' ' || c == '\t') {
                i++;
                continue;
            }
            Token tok = { .type = TOKEN_SYM, .text = substr_copy(src, i, 1),
                          .line = line };
            token_vec_push(out, tok);
            i++;
            continue;
        }
        *error_message = strdup("Invalid character in source");
        return;
    }
    Token eof = { .type = TOKEN_EOF, .text = NULL, .line = line };
    token_vec_push(out, eof);
}

/* ---------------- Parser ---------------- */

typedef struct {
    TokenVec tokens;
    size_t index;
    BogArena* arena;
    char* error;
} Parser;

typedef enum {
    GOAL_NODE_TERM,
    GOAL_NODE_NOT,
    GOAL_NODE_AND,
    GOAL_NODE_OR,
    GOAL_NODE_COMPARISON
} GoalNodeType;

typedef struct GoalNode GoalNode;

struct GoalNode {
    GoalNodeType type;
    BogTerm* term;
    const char* op;
    GoalNode* left;
    GoalNode* right;
};

typedef struct {
    BogGoalList* lists;
    size_t count;
} GoalBranchList;

BogTerm* bog_make_num(BogArena* arena, double value)
{
    BogTerm* t = (BogTerm*)bog_arena_alloc(arena,
                                                       sizeof(BogTerm));
    t->type = CPROLOG_TERM_NUM;
    t->value.number = value;
    return t;
}

BogTerm* bog_make_atom(BogArena* arena, const char* name)
{
    BogTerm* t = (BogTerm*)bog_arena_alloc(arena,
                                                       sizeof(BogTerm));
    t->type = CPROLOG_TERM_ATOM;
    t->value.atom = bog_arena_strdup(arena, name);
    return t;
}

BogTerm* bog_make_var(BogArena* arena, const char* name)
{
    BogTerm* t = (BogTerm*)bog_arena_alloc(arena,
                                                       sizeof(BogTerm));
    t->type = CPROLOG_TERM_VAR;
    t->value.atom = bog_arena_strdup(arena, name);
    return t;
}

BogTerm* bog_make_compound(BogArena* arena, const char* functor,
                                   BogTerm** args, size_t arity)
{
    BogTerm* t = (BogTerm*)bog_arena_alloc(arena,
                                                       sizeof(BogTerm));
    t->type = CPROLOG_TERM_COMPOUND;
    t->value.compound.functor = bog_arena_strdup(arena, functor);
    if (arity) {
        t->value.compound.args = (BogTerm**)bog_arena_alloc(
            arena, sizeof(BogTerm*) * arity);
        memcpy(t->value.compound.args, args, sizeof(BogTerm*) * arity);
    } else {
        t->value.compound.args = NULL;
    }
    t->value.compound.arity = arity;
    return t;
}

BogTerm* bog_make_list(BogArena* arena, BogTerm** items,
                               size_t length, BogTerm* tail)
{
    BogTerm* t = (BogTerm*)bog_arena_alloc(arena,
                                                       sizeof(BogTerm));
    t->type = CPROLOG_TERM_LIST;
    if (length) {
        t->value.list.items = (BogTerm**)bog_arena_alloc(
            arena, sizeof(BogTerm*) * length);
        memcpy(t->value.list.items, items, sizeof(BogTerm*) * length);
    } else {
        t->value.list.items = NULL;
    }
    t->value.list.length = length;
    t->value.list.tail = tail;
    return t;
}

BogTerm* bog_make_expr(BogArena* arena, char op, BogTerm* left,
                               BogTerm* right)
{
    BogTerm* t = (BogTerm*)bog_arena_alloc(arena,
                                                       sizeof(BogTerm));
    t->type = CPROLOG_TERM_EXPR;
    t->value.expr.op = op;
    t->value.expr.left = left;
    t->value.expr.right = right;
    return t;
}

/* Keep the first error: parsing continues after one, and later errors are consequences */
static void parser_set_error(Parser* parser, const char* msg)
{
    if (!parser->error)
        parser->error = strdup(msg);
}

static GoalNode* make_goal_node(Parser* parser, GoalNodeType type)
{
    GoalNode* node = (GoalNode*)bog_arena_alloc(parser->arena,
                                                    sizeof(GoalNode));
    memset(node, 0, sizeof(GoalNode));
    node->type = type;
    return node;
}

static Token* parser_peek(Parser* parser)
{
    if (parser->index >= parser->tokens.count)
        return NULL;
    return &parser->tokens.data[parser->index];
}

static Token* parser_eat(Parser* parser, TokenType type, const char* value)
{
    Token* tok = parser_peek(parser);
    if (!tok || tok->type != type) {
        parser_set_error(parser, "Unexpected token");
        return NULL;
    }
    if (value && (!tok->text || strcmp(tok->text, value) != 0)) {
        parser_set_error(parser, "Unexpected token value");
        return NULL;
    }
    parser->index++;
    return tok;
}

static BogTerm* parse_expression(Parser* parser);
static GoalNode* parse_goal_or(Parser* parser);

static BogTerm* parse_primary(Parser* parser)
{
    Token* tok = parser_peek(parser);
    if (!tok) {
        parser_set_error(parser, "Unexpected end of input");
        return NULL;
    }
    if (tok->type == TOKEN_NUMBER) {
        parser_eat(parser, TOKEN_NUMBER, NULL);
        return bog_make_num(parser->arena, tok->number);
    }
    if (tok->type == TOKEN_IDENT) {
        parser_eat(parser, TOKEN_IDENT, NULL);
        const char* name = tok->text;
        Token* next = parser_peek(parser);
        if (next && next->type == TOKEN_SYM && next->text
            && strcmp(next->text, "(") == 0) {
            parser_eat(parser, TOKEN_SYM, "(");
            size_t arity = 0;
            size_t cap = 0;
            BogTerm** args = NULL;
            Token* peek = parser_peek(parser);
            if (!(peek->type == TOKEN_SYM && peek->text
                  && strcmp(peek->text, ")") == 0)) {
                while (true) {
                    if (arity == cap) {
                        cap = cap ? cap * 2 : 4;
                        args = (BogTerm**)realloc(
                            args, sizeof(BogTerm*) * cap);
                    }
                    args[arity++] = parse_expression(parser);
                    if (parser->error) {
                        free(args);
                        return NULL;
                    }
                    Token* sep = parser_peek(parser);
                    if (sep->type == TOKEN_SYM && sep->text
                        && strcmp(sep->text, ",") == 0) {
                        parser_eat(parser, TOKEN_SYM, ",");
                        continue;
                    }
                    break;
                }
            }
            parser_eat(parser, TOKEN_SYM, ")");
            BogTerm* term = bog_make_compound(parser->arena, name,
                                                      args, arity);
            free(args);
            return term;
        }
        if (isupper((unsigned char)name[0]) || name[0] == '_') {
            return bog_make_var(parser->arena, name);
        }
        return bog_make_atom(parser->arena, name);
    }
    if (tok->type == TOKEN_SYM && tok->text && strcmp(tok->text, "[") == 0) {
        parser_eat(parser, TOKEN_SYM, "[");
        size_t length = 0;
        size_t cap = 0;
        BogTerm** items = NULL;
        BogTerm* tail = NULL;
        Token* peek = parser_peek(parser);
        if (!(peek->type == TOKEN_SYM && peek->text
              && strcmp(peek->text, "]") == 0)) {
            while (true) {
                if (length == cap) {
                    cap = cap ? cap * 2 : 4;
                    items = (BogTerm**)realloc(items,
                                                   sizeof(BogTerm*) * cap);
                }
                items[length++] = parse_expression(parser);
                if (parser->error) {
                    free(items);
                    return NULL;
                }
                Token* sep = parser_peek(parser);
                if (sep->type == TOKEN_SYM && sep->text
                    && strcmp(sep->text, ",") == 0) {
                    parser_eat(parser, TOKEN_SYM, ",");
                    continue;
                }
                if (sep->type == TOKEN_SYM && sep->text
                    && strcmp(sep->text, "|") == 0) {
                    parser_eat(parser, TOKEN_SYM, "|");
                    tail = parse_expression(parser);
                    break;
                }
                break;
            }
        }
        parser_eat(parser, TOKEN_SYM, "]");
        BogTerm* list = bog_make_list(parser->arena, items, length,
                                              tail);
        free(items);
        return list;
    }
    if (tok->type == TOKEN_SYM && tok->text && strcmp(tok->text, "(") == 0) {
        parser_eat(parser, TOKEN_SYM, "(");
        BogTerm* inner = parse_expression(parser);
        parser_eat(parser, TOKEN_SYM, ")");
        return inner;
    }
    parser_set_error(parser, "Bad term");
    return NULL;
}

static BogTerm* parse_multiplicative(Parser* parser)
{
    BogTerm* node = parse_primary(parser);
    if (!node)
        return NULL;
    while (true) {
        Token* tok = parser_peek(parser);
        if (tok->type == TOKEN_SYM && tok->text
            && (strcmp(tok->text, "*") == 0 || strcmp(tok->text, "/") == 0)) {
            char op = tok->text[0];
            parser_eat(parser, TOKEN_SYM, tok->text);
            BogTerm* right = parse_primary(parser);
            node = bog_make_expr(parser->arena, op, node, right);
            continue;
        }
        break;
    }
    return node;
}

static BogTerm* parse_additive(Parser* parser)
{
    BogTerm* node = parse_multiplicative(parser);
    if (!node)
        return NULL;
    while (true) {
        Token* tok = parser_peek(parser);
        if (tok->type == TOKEN_SYM && tok->text
            && (strcmp(tok->text, "+") == 0 || strcmp(tok->text, "-") == 0)) {
            char op = tok->text[0];
            parser_eat(parser, TOKEN_SYM, tok->text);
            BogTerm* right = parse_multiplicative(parser);
            node = bog_make_expr(parser->arena, op, node, right);
            continue;
        }
        break;
    }
    return node;
}

static BogTerm* parse_expression(Parser* parser)
{
    return parse_additive(parser);
}

static bool is_comparison_token(Token* tok)
{
    if (!tok)
        return false;
    if (tok->type == TOKEN_SYM && tok->text) {
        const char* v = tok->text;
        return strcmp(v, "=") == 0 || strcmp(v, "=:=") == 0
            || strcmp(v, "=\\=") == 0 || strcmp(v, "<") == 0
            || strcmp(v, ">") == 0 || strcmp(v, "=<") == 0
            || strcmp(v, ">=") == 0;
    }
    if (tok->type == TOKEN_IDENT && tok->text && strcmp(tok->text, "is") == 0)
        return true;
    return false;
}

static GoalNode* parse_goal_term(Parser* parser)
{
    BogTerm* left = parse_expression(parser);
    if (!left)
        return NULL;
    Token* tok = parser_peek(parser);
    if (is_comparison_token(tok)) {
        GoalNode* node = make_goal_node(parser, GOAL_NODE_COMPARISON);
        node->term = left;
        node->op = tok->text ? bog_arena_strdup(parser->arena, tok->text) : NULL;
        parser_eat(parser, tok->type, tok->text);
        node->right = make_goal_node(parser, GOAL_NODE_TERM);
        node->right->term = parse_expression(parser);
        return node;
    }
    GoalNode* node = make_goal_node(parser, GOAL_NODE_TERM);
    node->term = left;
    return node;
}

static GoalNode* parse_goal_unary(Parser* parser)
{
    Token* tok = parser_peek(parser);
    if (tok->type == TOKEN_SYM && tok->text && strcmp(tok->text, "\\+") == 0) {
        parser_eat(parser, TOKEN_SYM, "\\+");
        GoalNode* node = make_goal_node(parser, GOAL_NODE_NOT);
        node->left = parse_goal_unary(parser);
        return node;
    }
    if (tok->type == TOKEN_SYM && tok->text && strcmp(tok->text, "(") == 0) {
        parser_eat(parser, TOKEN_SYM, "(");
        GoalNode* inner = parse_goal_or(parser);
        parser_eat(parser, TOKEN_SYM, ")");
        return inner;
    }
    return parse_goal_term(parser);
}

static GoalNode* parse_goal_and(Parser* parser)
{
    GoalNode* left = parse_goal_unary(parser);
    if (!left)
        return NULL;
    Token* tok = parser_peek(parser);
    while (tok->type == TOKEN_SYM && tok->text
           && strcmp(tok->text, ",") == 0) {
        parser_eat(parser, TOKEN_SYM, ",");
        GoalNode* right = parse_goal_unary(parser);
        GoalNode* node = make_goal_node(parser, GOAL_NODE_AND);
        node->left = left;
        node->right = right;
        left = node;
        tok = parser_peek(parser);
    }
    return left;
}

static GoalNode* parse_goal_or(Parser* parser)
{
    GoalNode* left = parse_goal_and(parser);
    if (!left)
        return NULL;
    Token* tok = parser_peek(parser);
    while (tok->type == TOKEN_SYM && tok->text
           && strcmp(tok->text, ";") == 0) {
        parser_eat(parser, TOKEN_SYM, ";");
        GoalNode* right = parse_goal_and(parser);
        GoalNode* node = make_goal_node(parser, GOAL_NODE_OR);
        node->left = left;
        node->right = right;
        left = node;
        tok = parser_peek(parser);
    }
    return left;
}

static BogTerm* convert_comparison(Parser* parser, GoalNode* node)
{
    const char* op = node->op ? node->op : "=";
    const char* functor = NULL;
    if (strcmp(op, "=") == 0)
        functor = "=";
    else if (strcmp(op, "=:=") == 0)
        functor = "=:=";
    else if (strcmp(op, "=\\=") == 0)
        functor = "=\\=";
    else if (strcmp(op, "<") == 0)
        functor = "<";
    else if (strcmp(op, ">") == 0)
        functor = ">";
    else if (strcmp(op, "=<") == 0)
        functor = "=<";
    else if (strcmp(op, ">=") == 0)
        functor = ">=";
    else if (strcmp(op, "is") == 0)
        functor = "is";
    if (!functor) {
        parser_set_error(parser, "Unsupported operator");
        return NULL;
    }
    BogTerm* args[2];
    args[0] = node->term;
    args[1] = node->right ? node->right->term : NULL;
    return bog_make_compound(parser->arena, functor, args, 2);
}

static GoalBranchList branch_single(Parser* parser, BogGoal goal)
{
    GoalBranchList out;
    out.count = 1;
    out.lists = (BogGoalList*)bog_arena_alloc(parser->arena,
                                                      sizeof(BogGoalList));
    out.lists[0].count = 1;
    out.lists[0].items = (BogGoal*)bog_arena_alloc(
        parser->arena, sizeof(BogGoal));
    out.lists[0].items[0] = goal;
    return out;
}

static GoalBranchList branch_concat(Parser* parser, GoalBranchList left,
                                    GoalBranchList right)
{
    GoalBranchList out;
    out.count = left.count * right.count;
    out.lists = (BogGoalList*)bog_arena_alloc(
        parser->arena, sizeof(BogGoalList) * out.count);
    size_t idx = 0;
    for (size_t i = 0; i < left.count; ++i) {
        for (size_t j = 0; j < right.count; ++j) {
            size_t len = left.lists[i].count + right.lists[j].count;
            BogGoal* goals = (BogGoal*)bog_arena_alloc(
                parser->arena, sizeof(BogGoal) * len);
            memcpy(goals, left.lists[i].items,
                   sizeof(BogGoal) * left.lists[i].count);
            memcpy(goals + left.lists[i].count, right.lists[j].items,
                   sizeof(BogGoal) * right.lists[j].count);
            out.lists[idx].items = goals;
            out.lists[idx].count = len;
            idx++;
        }
    }
    return out;
}

static GoalBranchList branch_merge(Parser* parser, GoalBranchList left,
                                   GoalBranchList right)
{
    GoalBranchList out;
    out.count = left.count + right.count;
    out.lists = (BogGoalList*)bog_arena_alloc(
        parser->arena, sizeof(BogGoalList) * out.count);
    memcpy(out.lists, left.lists, sizeof(BogGoalList) * left.count);
    memcpy(out.lists + left.count, right.lists,
           sizeof(BogGoalList) * right.count);
    return out;
}

static GoalBranchList expand_goals(Parser* parser, GoalNode* node)
{
    GoalBranchList out = { 0 };
    if (!node) {
        out.count = 1;
        out.lists = (BogGoalList*)bog_arena_alloc(
            parser->arena, sizeof(BogGoalList));
        out.lists[0].count = 0;
        out.lists[0].items = NULL;
        return out;
    }
    if (node->type == GOAL_NODE_TERM) {
        BogGoal goal;
        goal.kind = CPROLOG_GOAL_TERM;
        goal.data.term = node->term;
        return branch_single(parser, goal);
    }
    if (node->type == GOAL_NODE_COMPARISON) {
        BogTerm* term = convert_comparison(parser, node);
        if (!term)
            return out;
        BogGoal goal;
        goal.kind = CPROLOG_GOAL_TERM;
        goal.data.term = term;
        return branch_single(parser, goal);
    }
    if (node->type == GOAL_NODE_NOT) {
        GoalBranchList inner = expand_goals(parser, node->left);
        BogGoal goal;
        goal.kind = CPROLOG_GOAL_NOT;
        goal.data.neg.count = inner.count;
        goal.data.neg.branches = inner.lists;
        return branch_single(parser, goal);
    }
    if (node->type == GOAL_NODE_AND) {
        GoalBranchList left = expand_goals(parser, node->left);
        GoalBranchList right = expand_goals(parser, node->right);
        return branch_concat(parser, left, right);
    }
    if (node->type == GOAL_NODE_OR) {
        GoalBranchList left = expand_goals(parser, node->left);
        GoalBranchList right = expand_goals(parser, node->right);
        return branch_merge(parser, left, right);
    }
    return out;
}

static BogClause parse_clause(Parser* parser)
{
    BogClause clause;
    memset(&clause, 0, sizeof(clause));
    /* Record source line from first token of clause */
    Token* first_tok = parser_peek(parser);
    clause.source_line = first_tok ? first_tok->line : 0;
    clause.head = parse_expression(parser);
    GoalNode* body_ast = NULL;
    Token* tok = parser_peek(parser);
    if (tok->type == TOKEN_SYM && tok->text && strcmp(tok->text, ":-") == 0) {
        parser_eat(parser, TOKEN_SYM, ":-");
        body_ast = parse_goal_or(parser);
    }
    parser_eat(parser, TOKEN_SYM, ".");
    if (body_ast) {
        GoalBranchList branches = expand_goals(parser, body_ast);
        clause.body = branches.lists[0];
    } else {
        clause.body.count = 0;
        clause.body.items = NULL;
    }
    return clause;
}

BogProgram* bog_parse_program(const char* src, BogArena* arena,
                                      char** error_message)
{
    Parser parser;
    token_vec_init(&parser.tokens);
    parser.index = 0;
    parser.arena = arena;
    parser.error = NULL;

    tokenize_source(src, &parser.tokens, &parser.error);
    if (parser.error) {
        if (error_message)
            *error_message = parser.error;
        else
            free(parser.error);
        token_vec_free(&parser.tokens);
        return NULL;
    }

    BogProgram* program = (BogProgram*)bog_arena_alloc(
        arena, sizeof(BogProgram));
    program->clauses = NULL;
    program->count = 0;

    while (parser.tokens.data[parser.index].type != TOKEN_EOF) {
        BogClause clause = parse_clause(&parser);
        if (parser.error) {
            if (error_message)
                *error_message = parser.error;
            else
                free(parser.error);
            token_vec_free(&parser.tokens);
            return NULL;
        }
        size_t new_count = program->count + 1;
        BogClause* clauses = (BogClause*)bog_arena_alloc(
            arena, sizeof(BogClause) * new_count);
        if (program->count) {
            memcpy(clauses, program->clauses,
                   sizeof(BogClause) * program->count);
        }
        clauses[program->count] = clause;
        program->clauses = clauses;
        program->count = new_count;
    }

    token_vec_free(&parser.tokens);
    return program;
}

void bog_free_program(BogProgram* program)
{
    (void)program; /* program memory is arena-managed */
}

/* ---------------- Term utilities ---------------- */

BogTerm* bog_subst_term(BogTerm* term, const BogEnv* env,
                                BogArena* arena)
{
    if (!term)
        return NULL;
    switch (term->type) {
    case CPROLOG_TERM_VAR: {
        BogTerm* value = bog_env_get(env, term->value.atom);
        if (value)
            return bog_subst_term(value, env, arena);
        return term;
    }
    case CPROLOG_TERM_COMPOUND: {
        BogTerm** args = NULL;
        if (term->value.compound.arity) {
            args = (BogTerm**)bog_arena_alloc(
                arena, sizeof(BogTerm*) * term->value.compound.arity);
            for (size_t i = 0; i < term->value.compound.arity; ++i) {
                args[i] = bog_subst_term(term->value.compound.args[i], env,
                                             arena);
            }
        }
        return bog_make_compound(arena, term->value.compound.functor, args,
                                     term->value.compound.arity);
    }
    case CPROLOG_TERM_LIST: {
        BogTerm** items = NULL;
        if (term->value.list.length) {
            items = (BogTerm**)bog_arena_alloc(
                arena, sizeof(BogTerm*) * term->value.list.length);
            for (size_t i = 0; i < term->value.list.length; ++i) {
                items[i] = bog_subst_term(term->value.list.items[i], env,
                                              arena);
            }
        }
        BogTerm* tail = term->value.list.tail
            ? bog_subst_term(term->value.list.tail, env, arena)
            : NULL;
        return bog_make_list(arena, items, term->value.list.length, tail);
    }
    case CPROLOG_TERM_EXPR: {
        BogTerm* left = bog_subst_term(term->value.expr.left, env,
                                               arena);
        BogTerm* right = bog_subst_term(term->value.expr.right, env,
                                                arena);
        return bog_make_expr(arena, term->value.expr.op, left, right);
    }
    default:
        return term;
    }
}

static void term_to_string_rec(const BogTerm* term, BogArena* arena,
                               char** buffer, size_t* size)
{
    if (!term) {
        const char* nil = "∅";
        char* out = (char*)bog_arena_alloc(arena, strlen(nil) + 1);
        if (!out) { *buffer = NULL; *size = 0; return; }
        strcpy(out, nil);
        *buffer = out;
        *size = strlen(nil);
        return;
    }
    switch (term->type) {
    case CPROLOG_TERM_NUM: {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%g", term->value.number);
        size_t len = strlen(tmp);
        char* out = (char*)bog_arena_alloc(arena, len + 1);
        if (!out) { *buffer = NULL; *size = 0; return; }
        memcpy(out, tmp, len + 1);
        *buffer = out;
        *size = len;
        return;
    }
    case CPROLOG_TERM_ATOM:
    case CPROLOG_TERM_VAR: {
        size_t len = strlen(term->value.atom);
        char* out = (char*)bog_arena_alloc(arena, len + 1);
        if (!out) { *buffer = NULL; *size = 0; return; }
        memcpy(out, term->value.atom, len + 1);
        *buffer = out;
        *size = len;
        return;
    }
    case CPROLOG_TERM_LIST: {
        size_t total = 2;
        char** parts = NULL;
        size_t* lengths = NULL;
        if (term->value.list.length) {
            parts = (char**)bog_arena_alloc(
                arena, sizeof(char*) * term->value.list.length);
            lengths = (size_t*)bog_arena_alloc(
                arena, sizeof(size_t) * term->value.list.length);
            if (!parts || !lengths) { *buffer = NULL; *size = 0; return; }
            for (size_t i = 0; i < term->value.list.length; ++i) {
                term_to_string_rec(term->value.list.items[i], arena, &parts[i],
                                   &lengths[i]);
                total += lengths[i];
                if (i > 0)
                    total += 2;
            }
        }
        char* tail_str = NULL;
        size_t tail_len = 0;
        if (term->value.list.tail) {
            term_to_string_rec(term->value.list.tail, arena, &tail_str,
                               &tail_len);
            total += tail_len + 3;
        }
        char* out = (char*)bog_arena_alloc(arena, total + 1);
        if (!out) { *buffer = NULL; *size = 0; return; }
        char* ptr = out;
        *ptr++ = '[';
        for (size_t i = 0; i < term->value.list.length; ++i) {
            if (i > 0) {
                *ptr++ = ',';
                *ptr++ = ' ';
            }
            memcpy(ptr, parts[i], lengths[i]);
            ptr += lengths[i];
        }
        if (term->value.list.tail) {
            *ptr++ = ' ';
            *ptr++ = '|';
            *ptr++ = ' ';
            memcpy(ptr, tail_str, tail_len);
            ptr += tail_len;
        }
        *ptr++ = ']';
        *ptr = '\0';
        *buffer = out;
        *size = ptr - out;
        return;
    }
    case CPROLOG_TERM_EXPR: {
        char* left_str;
        size_t left_len;
        char* right_str;
        size_t right_len;
        term_to_string_rec(term->value.expr.left, arena, &left_str, &left_len);
        term_to_string_rec(term->value.expr.right, arena, &right_str,
                           &right_len);
        size_t total = left_len + right_len + 5;
        char* out = (char*)bog_arena_alloc(arena, total + 1);
        if (!out) { *buffer = NULL; *size = 0; return; }
        char* ptr = out;
        *ptr++ = '(';
        memcpy(ptr, left_str, left_len);
        ptr += left_len;
        *ptr++ = ' ';
        *ptr++ = term->value.expr.op;
        *ptr++ = ' ';
        memcpy(ptr, right_str, right_len);
        ptr += right_len;
        *ptr++ = ')';
        *ptr = '\0';
        *buffer = out;
        *size = ptr - out;
        return;
    }
    case CPROLOG_TERM_COMPOUND: {
        size_t name_len = strlen(term->value.compound.functor);
        size_t total = name_len + 2;
        char** parts = NULL;
        size_t* lengths = NULL;
        if (term->value.compound.arity) {
            parts = (char**)bog_arena_alloc(
                arena, sizeof(char*) * term->value.compound.arity);
            lengths = (size_t*)bog_arena_alloc(
                arena, sizeof(size_t) * term->value.compound.arity);
            if (!parts || !lengths) { *buffer = NULL; *size = 0; return; }
            for (size_t i = 0; i < term->value.compound.arity; ++i) {
                term_to_string_rec(term->value.compound.args[i], arena,
                                   &parts[i], &lengths[i]);
                total += lengths[i];
                if (i > 0)
                    total += 2;
            }
        }
        char* out = (char*)bog_arena_alloc(arena, total + 1);
        if (!out) { *buffer = NULL; *size = 0; return; }
        char* ptr = out;
        memcpy(ptr, term->value.compound.functor, name_len);
        ptr += name_len;
        *ptr++ = '(';
        for (size_t i = 0; i < term->value.compound.arity; ++i) {
            if (i > 0) {
                *ptr++ = ',';
                *ptr++ = ' ';
            }
            memcpy(ptr, parts[i], lengths[i]);
            ptr += lengths[i];
        }
        *ptr++ = ')';
        *ptr = '\0';
        *buffer = out;
        *size = ptr - out;
        return;
    }
    }
}

char* bog_term_to_string(const BogTerm* term, BogArena* arena)
{
    char* buffer = NULL;
    size_t len = 0;
    term_to_string_rec(term, arena, &buffer, &len);
    if (!buffer) return NULL;
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buffer, len);
    out[len] = '\0';
    return out;
}

/* ---------------- Variable renaming ---------------- */

typedef struct {
    const char* name;
    BogTerm* value;
} RenameEntry;

typedef struct {
    RenameEntry* items;
    size_t count;
    size_t capacity;
} RenameMap;

static void rename_map_init(RenameMap* map)
{
    map->items = NULL;
    map->count = 0;
    map->capacity = 0;
}

static void rename_map_free(RenameMap* map)
{
    free(map->items);
    map->items = NULL;
    map->count = map->capacity = 0;
}

static BogTerm* rename_term(BogTerm* term, RenameMap* map,
                                size_t* counter, BogArena* arena);

static BogTerm* rename_var(RenameMap* map, size_t* counter,
                               const char* name, BogArena* arena)
{
    for (size_t i = 0; i < map->count; ++i) {
        if (strcmp(map->items[i].name, name) == 0)
            return map->items[i].value;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s$%zu", name, (*counter)++);
    BogTerm* var = bog_make_var(arena, buf);
    if (map->count == map->capacity) {
        size_t cap = map->capacity ? map->capacity * 2 : 8;
        map->items = (RenameEntry*)realloc(map->items,
                                           cap * sizeof(RenameEntry));
        map->capacity = cap;
    }
    map->items[map->count].name = name;
    map->items[map->count].value = var;
    map->count++;
    return var;
}

static BogTerm* rename_term(BogTerm* term, RenameMap* map,
                                size_t* counter, BogArena* arena)
{
    if (!term)
        return NULL;
    switch (term->type) {
    case CPROLOG_TERM_VAR:
        return rename_var(map, counter, term->value.atom, arena);
    case CPROLOG_TERM_COMPOUND: {
        BogTerm** args = NULL;
        if (term->value.compound.arity) {
            args = (BogTerm**)bog_arena_alloc(
                arena, sizeof(BogTerm*) * term->value.compound.arity);
            for (size_t i = 0; i < term->value.compound.arity; ++i) {
                args[i] = rename_term(term->value.compound.args[i], map,
                                      counter, arena);
            }
        }
        return bog_make_compound(arena, term->value.compound.functor, args,
                                     term->value.compound.arity);
    }
    case CPROLOG_TERM_LIST: {
        BogTerm** items = NULL;
        if (term->value.list.length) {
            items = (BogTerm**)bog_arena_alloc(
                arena, sizeof(BogTerm*) * term->value.list.length);
            for (size_t i = 0; i < term->value.list.length; ++i) {
                items[i] = rename_term(term->value.list.items[i], map, counter,
                                       arena);
            }
        }
        BogTerm* tail = term->value.list.tail
            ? rename_term(term->value.list.tail, map, counter, arena)
            : NULL;
        return bog_make_list(arena, items, term->value.list.length, tail);
    }
    case CPROLOG_TERM_EXPR: {
        BogTerm* left = rename_term(term->value.expr.left, map, counter,
                                        arena);
        BogTerm* right = rename_term(term->value.expr.right, map, counter,
                                         arena);
        return bog_make_expr(arena, term->value.expr.op, left, right);
    }
    default:
        return term;
    }
}

static BogGoalList rename_goal_list(const BogGoalList* list,
                                        RenameMap* map, size_t* counter,
                                        BogArena* arena);

static BogGoal rename_goal(const BogGoal* goal, RenameMap* map,
                               size_t* counter, BogArena* arena)
{
    BogGoal out;
    out.kind = goal->kind;
    if (goal->kind == CPROLOG_GOAL_TERM) {
        out.data.term = rename_term(goal->data.term, map, counter, arena);
    } else {
        out.data.neg.count = goal->data.neg.count;
        out.data.neg.branches = (BogGoalList*)bog_arena_alloc(
            arena, sizeof(BogGoalList) * goal->data.neg.count);
        for (size_t i = 0; i < goal->data.neg.count; ++i) {
            out.data.neg.branches[i] = rename_goal_list(
                &goal->data.neg.branches[i], map, counter, arena);
        }
    }
    return out;
}

static BogGoalList rename_goal_list(const BogGoalList* list,
                                        RenameMap* map, size_t* counter,
                                        BogArena* arena)
{
    BogGoalList out;
    out.count = list->count;
    if (!list->count) {
        out.items = NULL;
        return out;
    }
    out.items = (BogGoal*)bog_arena_alloc(
        arena, sizeof(BogGoal) * list->count);
    for (size_t i = 0; i < list->count; ++i) {
        out.items[i] = rename_goal(&list->items[i], map, counter, arena);
    }
    return out;
}

void bog_rename_clause(const BogClause* src, BogClause* dst,
                           size_t* counter, BogArena* arena)
{
    RenameMap map;
    rename_map_init(&map);
    dst->head = rename_term(src->head, &map, counter, arena);
    dst->body = rename_goal_list(&src->body, &map, counter, arena);
    dst->source_line = src->source_line;  /* Preserve source line */
    rename_map_free(&map);
}

/* ---------------- Unification ---------------- */

static bool unify_lists(BogTerm* left, BogTerm* right, BogEnv* env,
                        BogArena* arena);

bool bog_unify(BogTerm* a, BogTerm* b, BogEnv* env,
                   BogArena* arena)
{
    BogTerm* left = bog_subst_term(a, env, arena);
    BogTerm* right = bog_subst_term(b, env, arena);
    if (left->type == CPROLOG_TERM_VAR) {
        bog_env_set(env, left->value.atom, right);
        return true;
    }
    if (right->type == CPROLOG_TERM_VAR) {
        bog_env_set(env, right->value.atom, left);
        return true;
    }
    if (left->type == CPROLOG_TERM_NUM && right->type == CPROLOG_TERM_NUM) {
        return fabs(left->value.number - right->value.number) < 1e-9;
    }
    if (left->type == CPROLOG_TERM_ATOM && right->type == CPROLOG_TERM_ATOM) {
        return strcmp(left->value.atom, right->value.atom) == 0;
    }
    if (left->type == CPROLOG_TERM_LIST && right->type == CPROLOG_TERM_LIST) {
        return unify_lists(left, right, env, arena);
    }
    if (left->type == CPROLOG_TERM_EXPR && right->type == CPROLOG_TERM_EXPR
        && left->value.expr.op == right->value.expr.op) {
        if (!bog_unify(left->value.expr.left, right->value.expr.left, env,
                           arena))
            return false;
        return bog_unify(left->value.expr.right, right->value.expr.right,
                             env, arena);
    }
    if (left->type == CPROLOG_TERM_COMPOUND
        && right->type == CPROLOG_TERM_COMPOUND) {
        if (strcmp(left->value.compound.functor, right->value.compound.functor)
            != 0)
            return false;
        if (left->value.compound.arity != right->value.compound.arity)
            return false;
        for (size_t i = 0; i < left->value.compound.arity; ++i) {
            if (!bog_unify(left->value.compound.args[i],
                               right->value.compound.args[i], env, arena))
                return false;
        }
        return true;
    }
    return false;
}

static bool unify_lists(BogTerm* left, BogTerm* right, BogEnv* env,
                        BogArena* arena)
{
    size_t min = left->value.list.length < right->value.list.length
        ? left->value.list.length
        : right->value.list.length;
    for (size_t i = 0; i < min; ++i) {
        if (!bog_unify(left->value.list.items[i],
                           right->value.list.items[i], env, arena))
            return false;
    }
    if (left->value.list.length == right->value.list.length) {
        if (!left->value.list.tail && !right->value.list.tail)
            return true;
        if (left->value.list.tail && right->value.list.tail)
            return bog_unify(left->value.list.tail, right->value.list.tail,
                                 env, arena);
        if (left->value.list.tail) {
            BogTerm* empty = bog_make_list(arena, NULL, 0,
                                                   right->value.list.tail);
            return bog_unify(left->value.list.tail, empty, env, arena);
        }
        if (right->value.list.tail) {
            BogTerm* empty = bog_make_list(arena, NULL, 0,
                                                   left->value.list.tail);
            return bog_unify(empty, right->value.list.tail, env, arena);
        }
        return true;
    }
    if (left->value.list.length < right->value.list.length) {
        if (!left->value.list.tail)
            return false;
        BogTerm** items = right->value.list.items
            + left->value.list.length;
        size_t length = right->value.list.length - left->value.list.length;
        BogTerm* remaining = bog_make_list(arena, items, length,
                                                   right->value.list.tail);
        return bog_unify(left->value.list.tail, remaining, env, arena);
    }
    if (right->value.list.length < left->value.list.length) {
        if (!right->value.list.tail)
            return false;
        BogTerm** items = left->value.list.items
            + right->value.list.length;
        size_t length = left->value.list.length - right->value.list.length;
        BogTerm* remaining = bog_make_list(arena, items, length,
                                                   left->value.list.tail);
        return bog_unify(remaining, right->value.list.tail, env, arena);
    }
    return false;
}

/* ---------------- Resolution ---------------- */

static void resolve_internal(const BogGoalList* goals, size_t index,
                             BogEnv* env, const BogProgram* program,
                             const BogContext* ctx,
                             const BogBuiltins* builtins,
                             BogSolutions* solutions, BogArena* arena,
                             int source_line);

static bool goal_succeeds(const BogGoalList* goals, BogEnv* env,
                          const BogProgram* program,
                          const BogContext* ctx,
                          const BogBuiltins* builtins, BogArena* arena)
{
    BogSolutions temp = { 0 };
    resolve_internal(goals, 0, env, program, ctx, builtins, &temp, arena, 0);
    for (size_t i = 0; i < temp.count; ++i) {
        bog_env_free(&temp.envs[i]);
    }
    free(temp.envs);
    free(temp.source_lines);
    return temp.count > 0;
}

static void push_solution(BogSolutions* solutions, const BogEnv* env,
                          int source_line)
{
    BogEnv copy = bog_env_clone(env);
    solutions->envs = (BogEnv*)realloc(
        solutions->envs, sizeof(BogEnv) * (solutions->count + 1));
    solutions->source_lines = (int*)realloc(
        solutions->source_lines, sizeof(int) * (solutions->count + 1));
    solutions->envs[solutions->count] = copy;
    solutions->source_lines[solutions->count] = source_line;
    solutions->count++;
}

static void resolve_builtin(const BogGoal* goal, size_t next_index,
                            BogEnv* env, const BogGoalList* goals,
                            const BogProgram* program,
                            const BogContext* ctx,
                            const BogBuiltins* builtins,
                            BogSolutions* solutions, BogArena* arena,
                            int source_line)
{
    BogTerm* term = goal->data.term;
    const BogBuiltin* builtin = bog_find_builtin(
        builtins, term->value.compound.functor);
    if (!builtin)
        return;
    BogBuiltinResult result = { 0 };
    builtin->fn(term->value.compound.args, term->value.compound.arity, env,
                ctx, &result, arena);
    for (size_t i = 0; i < result.count; ++i) {
        resolve_internal(goals, next_index, &result.envs[i], program, ctx,
                         builtins, solutions, arena, source_line);
        bog_env_free(&result.envs[i]);
    }
    free(result.envs);
}

static void resolve_internal(const BogGoalList* goals, size_t index,
                             BogEnv* env, const BogProgram* program,
                             const BogContext* ctx,
                             const BogBuiltins* builtins,
                             BogSolutions* solutions, BogArena* arena,
                             int source_line)
{
    if (index >= goals->count) {
        push_solution(solutions, env, source_line);
        return;
    }
    BogGoal goal = goals->items[index];
    if (goal.kind == CPROLOG_GOAL_NOT) {
        bool succeeded = false;
        for (size_t b = 0; b < goal.data.neg.count; ++b) {
            BogEnv tmp = bog_env_clone(env);
            if (goal_succeeds(&goal.data.neg.branches[b], &tmp, program, ctx,
                              builtins, arena)) {
                succeeded = true;
            }
            bog_env_free(&tmp);
            if (succeeded)
                break;
        }
        if (!succeeded) {
            resolve_internal(goals, index + 1, env, program, ctx, builtins,
                             solutions, arena, source_line);
        }
        return;
    }
    if (goal.data.term->type == CPROLOG_TERM_COMPOUND) {
        const BogBuiltin* builtin = bog_find_builtin(
            builtins, goal.data.term->value.compound.functor);
        if (builtin) {
            resolve_builtin(&goal, index + 1, env, goals, program, ctx,
                            builtins, solutions, arena, source_line);
            return;
        }
    }
    size_t counter = 0;
    for (size_t i = 0; i < program->count; ++i) {
        BogClause renamed;
        bog_rename_clause(&program->clauses[i], &renamed, &counter, arena);
        BogEnv tmp = bog_env_clone(env);
        if (!bog_unify(goal.data.term, renamed.head, &tmp, arena)) {
            bog_env_free(&tmp);
            continue;
        }
        BogGoalList combined;
        combined.count = renamed.body.count + (goals->count - index - 1);
        combined.items = (BogGoal*)bog_arena_alloc(
            arena, sizeof(BogGoal) * combined.count);
        if (renamed.body.count) {
            memcpy(combined.items, renamed.body.items,
                   sizeof(BogGoal) * renamed.body.count);
        }
        if (goals->count - index - 1) {
            memcpy(combined.items + renamed.body.count,
                   goals->items + index + 1,
                   sizeof(BogGoal) * (goals->count - index - 1));
        }
        /* Use the matched clause's source line if this is the first goal */
        int new_source_line = (source_line == 0) ? renamed.source_line : source_line;
        resolve_internal(&combined, 0, &tmp, program, ctx, builtins, solutions,
                         arena, new_source_line);
        bog_env_free(&tmp);
    }
}

void bog_resolve(const BogGoalList* goals, BogEnv* env,
                     const BogProgram* program, const BogContext* ctx,
                     const BogBuiltins* builtins,
                     BogSolutions* solutions, BogArena* arena)
{
    solutions->envs = NULL;
    solutions->source_lines = NULL;
    solutions->count = 0;
    resolve_internal(goals, 0, env, program, ctx, builtins, solutions, arena, 0);
}
