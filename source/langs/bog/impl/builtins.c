#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "builtins.h"
#include "internal.h"
#include "scheduler.h"

/* Evaluation helpers */
static BogTerm* eval_term(BogTerm* term, BogEnv* env,
                              BogArena* arena)
{
    return bog_subst_term(term, env, arena);
}

static double eval_expression(BogTerm* term, BogEnv* env,
                              BogArena* arena);

static double eval_number(BogTerm* term, BogEnv* env,
                          BogArena* arena)
{
    BogTerm* t = eval_term(term, env, arena);
    if (t->type == CPROLOG_TERM_NUM)
        return t->value.number;
    if (t->type == CPROLOG_TERM_EXPR)
        return eval_expression(t, env, arena);
    fprintf(stderr, "Expected number in builtin\n");
    return 0.0;
}

static double eval_expression(BogTerm* term, BogEnv* env,
                              BogArena* arena)
{
    BogTerm* t = eval_term(term, env, arena);
    if (t->type == CPROLOG_TERM_NUM)
        return t->value.number;
    if (t->type == CPROLOG_TERM_EXPR) {
        double left = eval_expression(t->value.expr.left, env, arena);
        double right = eval_expression(t->value.expr.right, env, arena);
        switch (t->value.expr.op) {
        case '+':
            return left + right;
        case '-':
            return left - right;
        case '*':
            return left * right;
        case '/':
            return right != 0 ? left / right : 0.0;
        default:
            fprintf(stderr, "Unsupported operator %c\n", t->value.expr.op);
            return 0.0;
        }
    }
    fprintf(stderr, "Expected numeric expression\n");
    return 0.0;
}

static BogTerm** eval_list(BogTerm* term, BogEnv* env,
                               BogArena* arena, size_t* length)
{
    BogTerm* t = eval_term(term, env, arena);
    if (t->type == CPROLOG_TERM_LIST && !t->value.list.tail) {
        *length = t->value.list.length;
        return t->value.list.items;
    }
    fprintf(stderr, "Expected list in builtin\n");
    *length = 0;
    return NULL;
}

/* Deep equality */
static bool deep_equal(const BogTerm* a, const BogTerm* b);

static bool term_equal(const BogTerm* a, const BogTerm* b)
{
    if (a->type != b->type)
        return false;
    switch (a->type) {
    case CPROLOG_TERM_NUM:
        return fabs(a->value.number - b->value.number) < 1e-9;
    case CPROLOG_TERM_ATOM:
    case CPROLOG_TERM_VAR:
        return strcmp(a->value.atom, b->value.atom) == 0;
    case CPROLOG_TERM_EXPR:
        return a->value.expr.op == b->value.expr.op
            && deep_equal(a->value.expr.left, b->value.expr.left)
            && deep_equal(a->value.expr.right, b->value.expr.right);
    case CPROLOG_TERM_COMPOUND:
        if (strcmp(a->value.compound.functor, b->value.compound.functor) != 0)
            return false;
        if (a->value.compound.arity != b->value.compound.arity)
            return false;
        for (size_t i = 0; i < a->value.compound.arity; ++i) {
            if (!deep_equal(a->value.compound.args[i],
                            b->value.compound.args[i]))
                return false;
        }
        return true;
    case CPROLOG_TERM_LIST:
        if (a->value.list.length != b->value.list.length)
            return false;
        for (size_t i = 0; i < a->value.list.length; ++i) {
            if (!deep_equal(a->value.list.items[i], b->value.list.items[i]))
                return false;
        }
        if (a->value.list.tail && b->value.list.tail)
            return deep_equal(a->value.list.tail, b->value.list.tail);
        return !a->value.list.tail && !b->value.list.tail;
    }
    return false;
}

static bool deep_equal(const BogTerm* a, const BogTerm* b)
{
    return term_equal(a, b);
}

static size_t step_index_at_time(double T, double N, double B, double bpm)
{
    double beats = T * bpm / 60.0;
    double bars = beats / B;
    double s_real = round(bars * N);
    long idx = (long)fmod(s_real, N);
    if (idx < 0)
        idx += (long)N;
    return (size_t)idx;
}

/* Builtin helpers */
static bool builtin_record_env(BogBuiltinResult* out,
                               const BogEnv* env)
{
    BogEnv copy = bog_env_clone(env);
    out->envs = (BogEnv*)realloc(out->envs,
                                     sizeof(BogEnv) * (out->count + 1));
    out->envs[out->count++] = copy;
    return true;
}

/* Builtin predicates */
static bool builtin_eq(BogTerm** args, size_t arity, BogEnv* env,
                       const BogContext* ctx, BogBuiltinResult* out,
                       BogArena* arena)
{
    (void)arity;
    (void)ctx;
    BogTerm* left = eval_term(args[0], env, arena);
    BogTerm* right = eval_term(args[1], env, arena);
    if (deep_equal(left, right))
        builtin_record_env(out, env);
    return true;
}

static bool builtin_eq_numeric(BogTerm** args, size_t arity,
                               BogEnv* env, const BogContext* ctx,
                               BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    double a = eval_number(args[0], env, arena);
    double b = eval_number(args[1], env, arena);
    if (fabs(a - b) < 1e-9)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_neq_numeric(BogTerm** args, size_t arity,
                                BogEnv* env, const BogContext* ctx,
                                BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    double a = eval_number(args[0], env, arena);
    double b = eval_number(args[1], env, arena);
    if (fabs(a - b) >= 1e-9)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_unify_goal(BogTerm** args, size_t arity,
                               BogEnv* env, const BogContext* ctx,
                               BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    BogEnv tmp = bog_env_clone(env);
    if (bog_unify(args[0], args[1], &tmp, arena))
        builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_is(BogTerm** args, size_t arity, BogEnv* env,
                       const BogContext* ctx, BogBuiltinResult* out,
                       BogArena* arena)
{
    (void)arity;
    (void)ctx;
    double value = eval_expression(args[1], env, arena);
    BogTerm* target = bog_make_num(arena, value);
    BogEnv tmp = bog_env_clone(env);
    if (bog_unify(args[0], target, &tmp, arena))
        builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_lt(BogTerm** args, size_t arity, BogEnv* env,
                       const BogContext* ctx, BogBuiltinResult* out,
                       BogArena* arena)
{
    (void)arity;
    (void)ctx;
    (void)arena;
    double a = eval_number(args[0], env, arena);
    double b = eval_number(args[1], env, arena);
    if (a < b)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_gt(BogTerm** args, size_t arity, BogEnv* env,
                       const BogContext* ctx, BogBuiltinResult* out,
                       BogArena* arena)
{
    (void)arity;
    (void)ctx;
    (void)arena;
    double a = eval_number(args[0], env, arena);
    double b = eval_number(args[1], env, arena);
    if (a > b)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_lte(BogTerm** args, size_t arity, BogEnv* env,
                        const BogContext* ctx, BogBuiltinResult* out,
                        BogArena* arena)
{
    (void)arity;
    (void)ctx;
    (void)arena;
    double a = eval_number(args[0], env, arena);
    double b = eval_number(args[1], env, arena);
    if (a <= b)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_gte(BogTerm** args, size_t arity, BogEnv* env,
                        const BogContext* ctx, BogBuiltinResult* out,
                        BogArena* arena)
{
    (void)arity;
    (void)ctx;
    (void)arena;
    double a = eval_number(args[0], env, arena);
    double b = eval_number(args[1], env, arena);
    if (a >= b)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_within(BogTerm** args, size_t arity, BogEnv* env,
                           const BogContext* ctx,
                           BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    double t = eval_number(args[0], env, arena);
    double start = eval_number(args[1], env, arena);
    double end = eval_number(args[2], env, arena);
    if (t >= start && t <= end)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_distinct(BogTerm** args, size_t arity, BogEnv* env,
                             const BogContext* ctx,
                             BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    size_t length = 0;
    BogTerm** list = eval_list(args[0], env, arena, &length);
    if (!list)
        return true;
    for (size_t i = 0; i < length; ++i) {
        for (size_t j = i + 1; j < length; ++j) {
            if (deep_equal(list[i], list[j]))
                return true;
        }
    }
    builtin_record_env(out, env);
    return true;
}

static bool builtin_cooldown(BogTerm** args, size_t arity, BogEnv* env,
                             const BogContext* ctx,
                             BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    double now = eval_number(args[0], env, arena);
    double last = eval_number(args[1], env, arena);
    double gap = eval_number(args[2], env, arena);
    if (now - last >= gap)
        builtin_record_env(out, env);
    return true;
}

uint64_t bog_rng_seed_from_clock(const void* salt)
{
    struct timespec ts = { 0, 0 };
    timespec_get(&ts, TIME_UTC);
    uint64_t state = ((uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec)
        ^ (uint64_t)(uintptr_t)salt;
    return state;
}

double bog_rng_unit(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (double)(z >> 11) * (1.0 / 9007199254740992.0);
}

/* Draw from the instance's stream. Callers without a state manager share one
 * fallback stream, which is not thread-safe. */
static double next_random(const BogContext* ctx)
{
    static uint64_t fallback_state;
    static bool fallback_seeded;
    if (ctx && ctx->state_manager)
        return bog_state_manager_random(ctx->state_manager);
    if (!fallback_seeded) {
        fallback_state = bog_rng_seed_from_clock(&fallback_state);
        fallback_seeded = true;
    }
    return bog_rng_unit(&fallback_state);
}

static bool builtin_prob(BogTerm** args, size_t arity, BogEnv* env,
                         const BogContext* ctx, BogBuiltinResult* out,
                         BogArena* arena)
{
    (void)arity;
    double p = eval_number(args[0], env, arena);
    double r = next_random(ctx);
    if (r < p)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_choose(BogTerm** args, size_t arity, BogEnv* env,
                           const BogContext* ctx,
                           BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    size_t length = 0;
    BogTerm** list = eval_list(args[0], env, arena, &length);
    if (!list)
        return true;
    for (size_t i = 0; i < length; ++i) {
        BogEnv tmp = bog_env_clone(env);
        if (bog_unify(args[1], list[i], &tmp, arena))
            builtin_record_env(out, &tmp);
        bog_env_free(&tmp);
    }
    return true;
}

static bool builtin_pick(BogTerm** args, size_t arity, BogEnv* env,
                         const BogContext* ctx, BogBuiltinResult* out,
                         BogArena* arena)
{
    (void)arity;
    size_t length = 0;
    BogTerm** list = eval_list(args[0], env, arena, &length);
    if (!list || length == 0 || args[1]->type != CPROLOG_TERM_VAR)
        return true;
    size_t idx = (size_t)(next_random(ctx) * (double)length);
    BogEnv tmp = bog_env_clone(env);
    if (bog_unify(args[1], list[idx], &tmp, arena))
        builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_cycle(BogTerm** args, size_t arity, BogEnv* env,
                          const BogContext* ctx, BogBuiltinResult* out,
                          BogArena* arena)
{
    (void)arity;
    size_t length = 0;
    BogTerm** list = eval_list(args[0], env, arena, &length);
    if (!list || length == 0 || args[1]->type != CPROLOG_TERM_VAR)
        return true;
    size_t idx = 0;
    if (ctx && ctx->state_manager) {
        BogArena* tmp_arena = bog_arena_create();
        char* key = bog_term_to_string(args[0], tmp_arena);
        idx = bog_state_manager_increment_cycle(ctx->state_manager, key,
                                                    length);
        bog_arena_destroy(tmp_arena);
        free(key);
    }
    BogEnv tmp = bog_env_clone(env);
    if (bog_unify(args[1], list[idx % length], &tmp, arena))
        builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_rand(BogTerm** args, size_t arity, BogEnv* env,
                         const BogContext* ctx, BogBuiltinResult* out,
                         BogArena* arena)
{
    (void)arity;
    if (args[2]->type != CPROLOG_TERM_VAR)
        return true;
    double min = eval_number(args[0], env, arena);
    double max = eval_number(args[1], env, arena);
    double value = min + next_random(ctx) * (max - min);
    BogTerm* num = bog_make_num(arena, value);
    BogEnv tmp = bog_env_clone(env);
    bog_env_set(&tmp, args[2]->value.atom, num);
    builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_randint(BogTerm** args, size_t arity, BogEnv* env,
                            const BogContext* ctx,
                            BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    if (args[2]->type != CPROLOG_TERM_VAR)
        return true;
    int min = (int)floor(eval_number(args[0], env, arena));
    int max = (int)floor(eval_number(args[1], env, arena));
    int span = max - min;
    if (span <= 0)
        span = 1;
    int value = min + (int)(next_random(ctx) * (double)span);
    BogTerm* num = bog_make_num(arena, (double)value);
    BogEnv tmp = bog_env_clone(env);
    bog_env_set(&tmp, args[2]->value.atom, num);
    builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_every(BogTerm** args, size_t arity, BogEnv* env,
                          const BogContext* ctx, BogBuiltinResult* out,
                          BogArena* arena)
{
    (void)arity;
    (void)arena;
    if (!ctx)
        return true;
    double T = eval_number(args[0], env, arena);
    double step = eval_number(args[1], env, arena);
    double beat = T * ctx->bpm / 60.0;
    double ratio = beat / step;
    if (fabs(ratio - round(ratio)) < 1e-4)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_beat(BogTerm** args, size_t arity, BogEnv* env,
                         const BogContext* ctx, BogBuiltinResult* out,
                         BogArena* arena)
{
    (void)arity;
    (void)arena;
    if (!ctx)
        return true;
    double T = eval_number(args[0], env, arena);
    double N = eval_number(args[1], env, arena);
    double beat = T * ctx->bpm / 60.0;
    double value = beat * N;
    if (fabs(value - round(value)) < 1e-4)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_phase(BogTerm** args, size_t arity, BogEnv* env,
                          const BogContext* ctx, BogBuiltinResult* out,
                          BogArena* arena)
{
    (void)arity;
    (void)arena;
    if (!ctx)
        return true;
    double T = eval_number(args[0], env, arena);
    double N = eval_number(args[1], env, arena);
    double K = eval_number(args[2], env, arena);
    double beat = T * ctx->bpm / 60.0;
    long pos = (long)round(beat * N);
    long modN = (long)N;
    if (((pos % modN) + modN) % modN == ((long)K % modN + modN) % modN)
        builtin_record_env(out, env);
    return true;
}

static bool builtin_euc(BogTerm** args, size_t arity, BogEnv* env,
                        const BogContext* ctx, BogBuiltinResult* out,
                        BogArena* arena)
{
    (void)arity;
    (void)arena;
    if (!ctx)
        return true;
    double T = eval_number(args[0], env, arena);
    double K = eval_number(args[1], env, arena);
    double N = eval_number(args[2], env, arena);
    double B = eval_number(args[3], env, arena);
    double R = eval_number(args[4], env, arena);
    if (!(isfinite(K) && isfinite(N) && N > 0 && K >= 0 && K <= N))
        return true;
    size_t s = step_index_at_time(T, N, B, ctx->bpm);
    size_t sR = (s + ((size_t)fmod(R, N) + (size_t)N) % (size_t)N) % (size_t)N;
    bool hit = ((sR * (size_t)K) % (size_t)N) < (size_t)K;
    if (hit)
        builtin_record_env(out, env);
    return true;
}

/* Musical data */
static const int SCALE_IONIAN[] = { 0, 2, 4, 5, 7, 9, 11 };
static const int SCALE_DORIAN[] = { 0, 2, 3, 5, 7, 9, 10 };
static const int SCALE_PHRYGIAN[] = { 0, 1, 3, 5, 7, 8, 10 };
static const int SCALE_LYDIAN[] = { 0, 2, 4, 6, 7, 9, 11 };
static const int SCALE_MIXO[] = { 0, 2, 4, 5, 7, 9, 10 };
static const int SCALE_AEOLIAN[] = { 0, 2, 3, 5, 7, 8, 10 };
static const int SCALE_LOCRIAN[] = { 0, 1, 3, 5, 6, 8, 10 };
static const int SCALE_MAJOR_PENT[] = { 0, 2, 4, 7, 9 };
static const int SCALE_MINOR_PENT[] = { 0, 3, 5, 7, 10 };
static const int SCALE_BLUES[] = { 0, 3, 5, 6, 7, 10 };

typedef struct {
    const char* name;
    const int* steps;
    size_t count;
} ScaleDef;

static const ScaleDef SCALE_DEFS[] = {
    { "ionian", SCALE_IONIAN, sizeof(SCALE_IONIAN) / sizeof(int) },
    { "dorian", SCALE_DORIAN, sizeof(SCALE_DORIAN) / sizeof(int) },
    { "phrygian", SCALE_PHRYGIAN, sizeof(SCALE_PHRYGIAN) / sizeof(int) },
    { "lydian", SCALE_LYDIAN, sizeof(SCALE_LYDIAN) / sizeof(int) },
    { "mixolydian", SCALE_MIXO, sizeof(SCALE_MIXO) / sizeof(int) },
    { "aeolian", SCALE_AEOLIAN, sizeof(SCALE_AEOLIAN) / sizeof(int) },
    { "locrian", SCALE_LOCRIAN, sizeof(SCALE_LOCRIAN) / sizeof(int) },
    { "major_pent", SCALE_MAJOR_PENT, sizeof(SCALE_MAJOR_PENT) / sizeof(int) },
    { "minor_pent", SCALE_MINOR_PENT, sizeof(SCALE_MINOR_PENT) / sizeof(int) },
    { "blues", SCALE_BLUES, sizeof(SCALE_BLUES) / sizeof(int) }
};

static const int CHORD_MAJ[] = { 0, 4, 7 };
static const int CHORD_MIN[] = { 0, 3, 7 };
static const int CHORD_SUS2[] = { 0, 2, 7 };
static const int CHORD_SUS4[] = { 0, 5, 7 };
static const int CHORD_DIM[] = { 0, 3, 6 };
static const int CHORD_AUG[] = { 0, 4, 8 };
static const int CHORD_MAJ7[] = { 0, 4, 7, 11 };
static const int CHORD_DOM7[] = { 0, 4, 7, 10 };
static const int CHORD_MIN7[] = { 0, 3, 7, 10 };

typedef struct {
    const char* name;
    const int* intervals;
    size_t count;
} ChordDef;

static const ChordDef CHORD_DEFS[] = {
    { "maj", CHORD_MAJ, sizeof(CHORD_MAJ) / sizeof(int) },
    { "min", CHORD_MIN, sizeof(CHORD_MIN) / sizeof(int) },
    { "sus2", CHORD_SUS2, sizeof(CHORD_SUS2) / sizeof(int) },
    { "sus4", CHORD_SUS4, sizeof(CHORD_SUS4) / sizeof(int) },
    { "dim", CHORD_DIM, sizeof(CHORD_DIM) / sizeof(int) },
    { "aug", CHORD_AUG, sizeof(CHORD_AUG) / sizeof(int) },
    { "maj7", CHORD_MAJ7, sizeof(CHORD_MAJ7) / sizeof(int) },
    { "dom7", CHORD_DOM7, sizeof(CHORD_DOM7) / sizeof(int) },
    { "min7", CHORD_MIN7, sizeof(CHORD_MIN7) / sizeof(int) }
};

static const ScaleDef* find_scale(const char* name)
{
    for (size_t i = 0; i < sizeof(SCALE_DEFS) / sizeof(ScaleDef); ++i) {
        if (strcmp(SCALE_DEFS[i].name, name) == 0)
            return &SCALE_DEFS[i];
    }
    return NULL;
}

static const ChordDef* find_chord(const char* name)
{
    for (size_t i = 0; i < sizeof(CHORD_DEFS) / sizeof(ChordDef); ++i) {
        if (strcmp(CHORD_DEFS[i].name, name) == 0)
            return &CHORD_DEFS[i];
    }
    return NULL;
}

static bool builtin_scale(BogTerm** args, size_t arity, BogEnv* env,
                          const BogContext* ctx, BogBuiltinResult* out,
                          BogArena* arena)
{
    (void)arity;
    (void)ctx;
    if (args[4]->type != CPROLOG_TERM_VAR)
        return true;
    double root = eval_number(args[0], env, arena);
    BogTerm* mode = eval_term(args[1], env, arena);
    double degree = eval_number(args[2], env, arena);
    double octave = eval_number(args[3], env, arena);
    if (mode->type != CPROLOG_TERM_ATOM)
        return true;
    const ScaleDef* scale = find_scale(mode->value.atom);
    if (!scale)
        return true;
    int zero_idx = (int)degree - 1;
    int step = scale->steps[(zero_idx % (int)scale->count + (int)scale->count)
                            % (int)scale->count];
    int oct_shift = zero_idx / (int)scale->count;
    double midi = root + step + 12.0 * (octave + oct_shift);
    BogTerm* num = bog_make_num(arena, midi);
    BogEnv tmp = bog_env_clone(env);
    bog_env_set(&tmp, args[4]->value.atom, num);
    builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_chord(BogTerm** args, size_t arity, BogEnv* env,
                          const BogContext* ctx, BogBuiltinResult* out,
                          BogArena* arena)
{
    (void)arity;
    (void)ctx;
    if (args[3]->type != CPROLOG_TERM_VAR)
        return true;
    double root = eval_number(args[0], env, arena);
    BogTerm* quality = eval_term(args[1], env, arena);
    double octave = eval_number(args[2], env, arena);
    if (quality->type != CPROLOG_TERM_ATOM)
        return true;
    const ChordDef* chord = find_chord(quality->value.atom);
    if (!chord)
        return true;
    for (size_t i = 0; i < chord->count; ++i) {
        double value = root + chord->intervals[i] + 12.0 * octave;
        BogTerm* num = bog_make_num(arena, value);
        BogEnv tmp = bog_env_clone(env);
        bog_env_set(&tmp, args[3]->value.atom, num);
        builtin_record_env(out, &tmp);
        bog_env_free(&tmp);
    }
    return true;
}

static bool builtin_transpose(BogTerm** args, size_t arity,
                              BogEnv* env, const BogContext* ctx,
                              BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    if (args[2]->type != CPROLOG_TERM_VAR)
        return true;
    double note = eval_number(args[0], env, arena);
    double offset = eval_number(args[1], env, arena);
    BogTerm* num = bog_make_num(arena, note + offset);
    BogEnv tmp = bog_env_clone(env);
    bog_env_set(&tmp, args[2]->value.atom, num);
    builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_add(BogTerm** args, size_t arity, BogEnv* env,
                        const BogContext* ctx, BogBuiltinResult* out,
                        BogArena* arena)
{
    (void)arity;
    (void)ctx;
    if (args[2]->type != CPROLOG_TERM_VAR)
        return true;
    double A = eval_number(args[0], env, arena);
    double B = eval_number(args[1], env, arena);
    BogTerm* num = bog_make_num(arena, A + B);
    BogEnv tmp = bog_env_clone(env);
    bog_env_set(&tmp, args[2]->value.atom, num);
    builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

static bool builtin_range(BogTerm** args, size_t arity, BogEnv* env,
                          const BogContext* ctx, BogBuiltinResult* out,
                          BogArena* arena)
{
    (void)arity;
    (void)ctx;
    if (args[3]->type != CPROLOG_TERM_VAR)
        return true;
    double start = eval_number(args[0], env, arena);
    double end = eval_number(args[1], env, arena);
    double step = fabs(eval_number(args[2], env, arena));
    if (step == 0)
        step = 1;
    int dir = end >= start ? 1 : -1;
    for (double v = start; dir > 0 ? v <= end : v >= end; v += dir * step) {
        BogTerm* num = bog_make_num(arena, v);
        BogEnv tmp = bog_env_clone(env);
        bog_env_set(&tmp, args[3]->value.atom, num);
        builtin_record_env(out, &tmp);
        bog_env_free(&tmp);
    }
    return true;
}

static bool builtin_rotate(BogTerm** args, size_t arity, BogEnv* env,
                           const BogContext* ctx,
                           BogBuiltinResult* out, BogArena* arena)
{
    (void)arity;
    (void)ctx;
    if (args[2]->type != CPROLOG_TERM_VAR)
        return true;
    size_t length = 0;
    BogTerm** items = eval_list(args[0], env, arena, &length);
    if (!items || length == 0)
        return true;
    double shift = eval_number(args[1], env, arena);
    size_t s = ((size_t)llabs((long long)shift) % length);
    BogTerm** rotated = (BogTerm**)bog_arena_alloc(
        arena, sizeof(BogTerm*) * length);
    for (size_t i = 0; i < length; ++i)
        rotated[i] = items[(i + s) % length];
    BogTerm* list = bog_make_list(arena, rotated, length, NULL);
    BogEnv tmp = bog_env_clone(env);
    if (bog_unify(args[2], list, &tmp, arena))
        builtin_record_env(out, &tmp);
    bog_env_free(&tmp);
    return true;
}

/* Builtin registry */
static const BogBuiltin builtin_table[] = {
    { "eq", builtin_eq },
    { "=:=", builtin_eq_numeric },
    { "=\\=", builtin_neq_numeric },
    { "=", builtin_unify_goal },
    { "is", builtin_is },
    { "<", builtin_lt },
    { ">", builtin_gt },
    { "=<", builtin_lte },
    { ">=", builtin_gte },
    { "lt", builtin_lt },
    { "gt", builtin_gt },
    { "lte", builtin_lte },
    { "gte", builtin_gte },
    { "within", builtin_within },
    { "distinct", builtin_distinct },
    { "cooldown", builtin_cooldown },
    { "prob", builtin_prob },
    { "choose", builtin_choose },
    { "pick", builtin_pick },
    { "cycle", builtin_cycle },
    { "rand", builtin_rand },
    { "randint", builtin_randint },
    { "every", builtin_every },
    { "beat", builtin_beat },
    { "phase", builtin_phase },
    { "euc", builtin_euc },
    { "scale", builtin_scale },
    { "chord", builtin_chord },
    { "transpose", builtin_transpose },
    { "add", builtin_add },
    { "range", builtin_range },
    { "rotate", builtin_rotate }
};

BogBuiltins* bog_create_builtins(BogArena* arena)
{
    BogBuiltins* builtins = (BogBuiltins*)bog_arena_alloc(
        arena, sizeof(BogBuiltins));
    builtins->count = sizeof(builtin_table) / sizeof(BogBuiltin);
    builtins->items = (BogBuiltin*)bog_arena_alloc(
        arena, sizeof(BogBuiltin) * builtins->count);
    for (size_t i = 0; i < builtins->count; ++i) {
        builtins->items[i].name = builtin_table[i].name;
        builtins->items[i].fn = builtin_table[i].fn;
    }
    return builtins;
}

const BogBuiltin* bog_find_builtin(const BogBuiltins* builtins,
                                           const char* name)
{
    for (size_t i = 0; i < builtins->count; ++i) {
        if (strcmp(builtins->items[i].name, name) == 0)
            return &builtins->items[i];
    }
    return NULL;
}
