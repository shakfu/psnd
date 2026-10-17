/*
 * Resolution/Engine tests - converted from dogalog tests/prolog/engine.test.js
 */
#include "test_framework.h"

#include "bog.h"
#include "builtins.h"

TEST(resolution_resolves_builtin_choose)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* event(sine, N, 0.8, T) :- every(T, 1), choose([40,43], N). */
    BogProgram* program = bog_parse_program(
        "event(sine, N, 0.8, T) :- every(T, 1), choose([40,43], N).", arena,
        &error);

    ASSERT_NOT_NULL(program);
    ASSERT_NULL(error);

    /* Query: event(Voice, Pitch, Vel, 0) */
    BogTerm* query_args[4];
    query_args[0] = bog_make_var(arena, "Voice");
    query_args[1] = bog_make_var(arena, "Pitch");
    query_args[2] = bog_make_var(arena, "Vel");
    query_args[3] = bog_make_num(arena, 0);
    BogTerm* query = bog_make_compound(arena, "event", query_args, 4);

    /* Create goal list */
    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 2 solutions (40 and 43) */
    ASSERT_EQ(solutions.count, 2);

    /* Extract pitches */
    double pitches[2];
    for (size_t i = 0; i < 2; i++) {
        BogTerm* pitch = bog_subst_term(bog_make_var(arena, "Pitch"),
                                        &solutions.envs[i], arena);
        ASSERT_NOT_NULL(pitch);
        ASSERT_EQ(pitch->type, CPROLOG_TERM_NUM);
        pitches[i] = pitch->value.number;
    }

    ASSERT_NEAR(pitches[0], 40.0, 1e-9);
    ASSERT_NEAR(pitches[1], 43.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_supports_euclidean_rhythm_gating)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* event(kick, 36, 1.0, T) :- euc(T, 4, 16, 4, 0). */
    BogProgram* program = bog_parse_program(
        "event(kick, 36, 1.0, T) :- euc(T, 4, 16, 4, 0).", arena, &error);

    ASSERT_NOT_NULL(program);

    /* Query: event(Voice, Pitch, Vel, 0) - at time 0, kick should fire */
    BogTerm* query_args[4];
    query_args[0] = bog_make_var(arena, "Voice");
    query_args[1] = bog_make_var(arena, "Pitch");
    query_args[2] = bog_make_var(arena, "Vel");
    query_args[3] = bog_make_num(arena, 0);
    BogTerm* query = bog_make_compound(arena, "event", query_args, 4);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    ASSERT_EQ(solutions.count, 1);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_multiple_clauses)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* Multiple clauses for same predicate */
    BogProgram* program = bog_parse_program("foo(1). foo(2). foo(3).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 3);

    /* Query: foo(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "foo", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 3 solutions */
    ASSERT_EQ(solutions.count, 3);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_conjunctive_goals)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* test(X) :- foo(X), bar(X). with foo(1). foo(2). bar(2). */
    BogProgram* program = bog_parse_program(
        "foo(1). foo(2). bar(2). test(X) :- foo(X), bar(X).", arena, &error);

    ASSERT_NOT_NULL(program);

    /* Query: test(X) - should only match X=2 */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "test", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 1 solution: X=2 */
    ASSERT_EQ(solutions.count, 1);

    BogTerm* x_val = bog_subst_term(bog_make_var(arena, "X"),
                                    &solutions.envs[0], arena);
    ASSERT_NEAR(x_val->value.number, 2.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_tracks_source_lines)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* Multi-line program - clauses on different lines */
    BogProgram* program = bog_parse_program(
        "foo(1).\n"
        "foo(2).\n"
        "foo(3).", arena, &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 3);

    /* Verify source lines are tracked in clauses */
    ASSERT_EQ(program->clauses[0].source_line, 1);
    ASSERT_EQ(program->clauses[1].source_line, 2);
    ASSERT_EQ(program->clauses[2].source_line, 3);

    /* Query: foo(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "foo", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 3 solutions with correct source lines */
    ASSERT_EQ(solutions.count, 3);
    ASSERT_NOT_NULL(solutions.source_lines);
    ASSERT_EQ(solutions.source_lines[0], 1);
    ASSERT_EQ(solutions.source_lines[1], 2);
    ASSERT_EQ(solutions.source_lines[2], 3);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_is_builtin)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* test(X) :- X is 2 + 3. - using infix notation */
    BogProgram* program = bog_parse_program("test(X) :- X is 2 + 3.", arena,
                                            &error);

    ASSERT_NOT_NULL(program);

    /* Query: test(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "test", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    ASSERT_EQ(solutions.count, 1);

    BogTerm* x_val = bog_subst_term(bog_make_var(arena, "X"),
                                    &solutions.envs[0], arena);
    ASSERT_NEAR(x_val->value.number, 5.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* Resolve the body of "__q :- BODY." against program_src */
static BogSolutions run_query(const char* program_src, const char* query_src)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;
    BogProgram* program = bog_parse_program(program_src, arena, &error);
    free(error);
    error = NULL;
    BogProgram* query = bog_parse_program(query_src, arena, &error);
    free(error);
    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;
    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };
    bog_resolve(&query->clauses[0].body, &env, program, &ctx, builtins,
                &solutions, arena);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    return solutions;
}

static void free_solutions(BogSolutions* solutions)
{
    for (size_t i = 0; i < solutions->count; i++)
        bog_env_free(&solutions->envs[i]);
    free(solutions->envs);
    free(solutions->source_lines);
}

TEST(resolution_stops_self_recursive_clause)
{
    BogSolutions s = run_query(
        "event(V, P, X, T) :- event(V, P, X, T).",
        "__q :- event(V, P, X, 1.0).");
    ASSERT_EQ(s.count, 0);
    ASSERT_TRUE(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_stops_runaway_search)
{
    /* 10^5 combinations, none succeeds: only the step limit ends it */
    BogSolutions s = run_query(
        "d(0). d(1). d(2). d(3). d(4). d(5). d(6). d(7). d(8). d(9).",
        "__q :- d(A), d(B), d(C), d(D), d(E), A > 9.");
    ASSERT_EQ(s.count, 0);
    ASSERT_TRUE(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_caps_solution_count)
{
    BogSolutions s = run_query(
        "d(0). d(1). d(2). d(3). d(4). d(5). d(6). d(7). d(8). d(9).",
        "__q :- d(A), d(B), d(C), d(D).");
    ASSERT_EQ(s.count, BOG_MAX_SOLUTIONS);
    ASSERT_TRUE(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_allows_bounded_recursion)
{
    BogSolutions s = run_query(
        "count(0). count(N) :- N > 0, M is N - 1, count(M).",
        "__q :- count(100).");
    ASSERT_EQ(s.count, 1);
    ASSERT_FALSE(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_exhausted_negation_does_not_succeed)
{
    /* \+ over a runaway goal must not prove the negation */
    BogSolutions s = run_query(
        "loop :- loop.",
        "__q :- \\+ loop.");
    ASSERT_EQ(s.count, 0);
    ASSERT_TRUE(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

int main(void)
{
    printf("Resolution Tests\n");
    printf("================\n");

    RUN_TEST(resolution_resolves_builtin_choose);
    RUN_TEST(resolution_supports_euclidean_rhythm_gating);
    RUN_TEST(resolution_handles_multiple_clauses);
    RUN_TEST(resolution_handles_conjunctive_goals);
    RUN_TEST(resolution_tracks_source_lines);
    RUN_TEST(resolution_handles_is_builtin);
    RUN_TEST(resolution_stops_self_recursive_clause);
    RUN_TEST(resolution_stops_runaway_search);
    RUN_TEST(resolution_caps_solution_count);
    RUN_TEST(resolution_allows_bounded_recursion);
    RUN_TEST(resolution_exhausted_negation_does_not_succeed);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
