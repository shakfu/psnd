#ifndef CPROLOG_H
#define CPROLOG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BogArena BogArena;

typedef enum {
    CPROLOG_TERM_NUM,
    CPROLOG_TERM_ATOM,
    CPROLOG_TERM_VAR,
    CPROLOG_TERM_COMPOUND,
    CPROLOG_TERM_LIST,
    CPROLOG_TERM_EXPR
} BogTermType;

typedef struct BogTerm BogTerm;

struct BogTerm {
    BogTermType type;
    union {
        double number;
        const char* atom; /* atoms and variable names */
        struct {
            const char* functor;
            BogTerm** args;
            size_t arity;
        } compound;
        struct {
            BogTerm** items;
            size_t length;
            BogTerm* tail;
        } list;
        struct {
            char op;
            BogTerm* left;
            BogTerm* right;
        } expr;
    } value;
};

typedef struct {
    const char* name;
    BogTerm* value;
} BogBinding;

typedef struct {
    BogBinding* items;
    size_t count;
    size_t capacity;
} BogEnv;

typedef enum { CPROLOG_GOAL_TERM, CPROLOG_GOAL_NOT } BogGoalKind;

typedef struct BogGoalList BogGoalList;

typedef struct {
    BogGoalKind kind;
    union {
        BogTerm* term;
        struct {
            BogGoalList* branches;
            size_t count;
        } neg;
    } data;
} BogGoal;

struct BogGoalList {
    BogGoal* items;
    size_t count;
};

typedef struct {
    BogTerm* head;
    BogGoalList body;
    int source_line;  /* 1-based line number where clause was defined */
} BogClause;

typedef struct {
    BogClause* clauses;
    size_t count;
} BogProgram;

typedef struct BogStateManager BogStateManager;

typedef struct {
    double bpm;
    BogStateManager* state_manager;
} BogContext;

typedef struct BogBuiltinResult {
    BogEnv* envs;
    size_t count;
} BogBuiltinResult;

typedef bool (*BogBuiltinFn)(BogTerm** args, size_t arity,
                                 BogEnv* env, const BogContext* ctx,
                                 BogBuiltinResult* out,
                                 BogArena* arena);

typedef struct {
    const char* name;
    BogBuiltinFn fn;
} BogBuiltin;

typedef struct {
    BogBuiltin* items;
    size_t count;
} BogBuiltins;

/* Arena helpers */
BogArena* bog_arena_create(void);
void bog_arena_destroy(BogArena* arena);
void* bog_arena_alloc(BogArena* arena, size_t size);
const char* bog_arena_strdup(BogArena* arena, const char* src);

/* Environment helpers */
void bog_env_init(BogEnv* env);
void bog_env_free(BogEnv* env);
BogEnv bog_env_clone(const BogEnv* env);
BogTerm* bog_env_get(const BogEnv* env, const char* name);
void bog_env_set(BogEnv* env, const char* name, BogTerm* value);

/* Core API */
BogProgram* bog_parse_program(const char* src, BogArena* arena,
                                      char** error_message);
void bog_free_program(BogProgram* program);

void bog_rename_clause(const BogClause* src, BogClause* dst,
                           size_t* counter, BogArena* arena);

BogBuiltins* bog_create_builtins(BogArena* arena);
const BogBuiltin* bog_find_builtin(const BogBuiltins* builtins,
                                           const char* name);

/* Term construction */
BogTerm* bog_make_num(BogArena* arena, double value);
BogTerm* bog_make_atom(BogArena* arena, const char* name);
BogTerm* bog_make_var(BogArena* arena, const char* name);
BogTerm* bog_make_compound(BogArena* arena, const char* functor,
                           BogTerm** args, size_t arity);
BogTerm* bog_make_list(BogArena* arena, BogTerm** items,
                       size_t length, BogTerm* tail);
BogTerm* bog_make_expr(BogArena* arena, char op, BogTerm* left,
                       BogTerm* right);

/* Term helpers */
BogTerm* bog_subst_term(BogTerm* term, const BogEnv* env,
                                BogArena* arena);
char* bog_term_to_string(const BogTerm* term, BogArena* arena);
bool bog_unify(BogTerm* a, BogTerm* b, BogEnv* env,
                   BogArena* arena);

/* Resolution limits. A query that hits one stops and sets truncated. */
#define BOG_MAX_DEPTH 512        /* nested resolution calls (C stack) */
#define BOG_MAX_STEPS 20000      /* goal resolutions per query (time) */
#define BOG_MAX_SOLUTIONS 1024   /* solutions per query (memory) */

/* Resolution */
typedef struct {
    BogEnv* envs;
    int* source_lines;  /* Source line of matched clause for each solution */
    size_t count;
    bool truncated;     /* a resolution limit stopped the query */
} BogSolutions;

void bog_resolve(const BogGoalList* goals, BogEnv* env,
                     const BogProgram* program, const BogContext* ctx,
                     const BogBuiltins* builtins,
                     BogSolutions* solutions, BogArena* arena);

#ifdef __cplusplus
}
#endif

#endif /* CPROLOG_H */
