/*
 * State Manager tests - converted from dogalog tests/scheduler/stateManager.test.js
 */
#include "test_framework.h"

#include <stdlib.h>

#include "bog.h"
#include "builtins.h"
#include "scheduler.h"

/* Cycle State */

TEST(state_manager_returns_0_for_new_cycle_key)
{
    BogStateManager* sm = bog_state_manager_create();

    size_t idx = bog_state_manager_get_cycle(sm, "new-key");
    ASSERT_EQ(idx, 0);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_increments_cycle_and_returns_current)
{
    BogStateManager* sm = bog_state_manager_create();

    size_t current = bog_state_manager_increment_cycle(sm, "test-cycle", 3);
    ASSERT_EQ(current, 0);

    size_t after = bog_state_manager_get_cycle(sm, "test-cycle");
    ASSERT_EQ(after, 1);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_wraps_cycle_at_list_length)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_increment_cycle(sm, "test-cycle", 3); /* 0 -> 1 */
    bog_state_manager_increment_cycle(sm, "test-cycle", 3); /* 1 -> 2 */
    size_t current = bog_state_manager_increment_cycle(sm, "test-cycle",
                                                       3); /* 2 -> 0 */

    ASSERT_EQ(current, 2);
    ASSERT_EQ(bog_state_manager_get_cycle(sm, "test-cycle"), 0);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_maintains_independent_cycle_state)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_increment_cycle(sm, "key1", 3); /* key1: 0 -> 1 */
    bog_state_manager_increment_cycle(sm, "key2", 3); /* key2: 0 -> 1 */
    bog_state_manager_increment_cycle(sm, "key1", 3); /* key1: 1 -> 2 */

    ASSERT_EQ(bog_state_manager_get_cycle(sm, "key1"), 2);
    ASSERT_EQ(bog_state_manager_get_cycle(sm, "key2"), 1);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_persists_cycle_state_across_queries)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_increment_cycle(sm, "persistent", 5);
    bog_state_manager_increment_cycle(sm, "persistent", 5);

    ASSERT_EQ(bog_state_manager_get_cycle(sm, "persistent"), 2);

    bog_state_manager_increment_cycle(sm, "persistent", 5);
    ASSERT_EQ(bog_state_manager_get_cycle(sm, "persistent"), 3);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

/* Cooldown State */

TEST(state_manager_can_trigger_returns_true_for_new_key)
{
    BogStateManager* sm = bog_state_manager_create();

    bool can = bog_state_manager_can_trigger(sm, "new-key", 0.0, 1.0);
    ASSERT(can);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_stores_and_retrieves_last_trigger)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_set_last_trigger(sm, "test", 1.5);

    /* After triggering at 1.5, checking at 2.0 with cooldown 1.0 should fail */
    bool can = bog_state_manager_can_trigger(sm, "test", 2.0, 1.0);
    ASSERT(!can);

    /* At time 3.0 with cooldown 1.0, should succeed (1.5 + 1.0 = 2.5 < 3.0) */
    bool can2 = bog_state_manager_can_trigger(sm, "test", 3.0, 1.0);
    ASSERT(can2);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_maintains_independent_trigger_state)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_set_last_trigger(sm, "key1", 1.0);
    bog_state_manager_set_last_trigger(sm, "key2", 2.0);

    /* key1 triggered at 1.0, checking at 1.5 with cooldown 1.0 should fail */
    ASSERT(!bog_state_manager_can_trigger(sm, "key1", 1.5, 1.0));

    /* key2 triggered at 2.0, checking at 2.5 with cooldown 1.0 should fail */
    ASSERT(!bog_state_manager_can_trigger(sm, "key2", 2.5, 1.0));

    /* key1 at time 2.5 should succeed (1.0 + 1.0 = 2.0 < 2.5) */
    ASSERT(bog_state_manager_can_trigger(sm, "key1", 2.5, 1.0));

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

/* Reset */

TEST(state_manager_reset_clears_cycle_state)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_increment_cycle(sm, "key1", 3);
    bog_state_manager_increment_cycle(sm, "key2", 5);

    bog_state_manager_reset(sm);

    ASSERT_EQ(bog_state_manager_get_cycle(sm, "key1"), 0);
    ASSERT_EQ(bog_state_manager_get_cycle(sm, "key2"), 0);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_reset_clears_cooldown_state)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_set_last_trigger(sm, "key1", 1.5);
    bog_state_manager_set_last_trigger(sm, "key2", 2.5);

    bog_state_manager_reset(sm);

    /* After reset, can_trigger should return true for any time */
    ASSERT(bog_state_manager_can_trigger(sm, "key1", 0.0, 1.0));
    ASSERT(bog_state_manager_can_trigger(sm, "key2", 0.0, 1.0));

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

TEST(state_manager_allows_rebuild_after_reset)
{
    BogStateManager* sm = bog_state_manager_create();

    bog_state_manager_increment_cycle(sm, "test", 3);
    bog_state_manager_reset(sm);

    bog_state_manager_increment_cycle(sm, "test", 3);
    ASSERT_EQ(bog_state_manager_get_cycle(sm, "test"), 1);

    bog_state_manager_destroy(sm);
    TEST_PASS();
}

/* Random stream */

TEST(state_manager_same_seed_repeats_sequence)
{
    BogStateManager* a = bog_state_manager_create();
    BogStateManager* b = bog_state_manager_create();
    bog_state_manager_seed(a, 42);
    bog_state_manager_seed(b, 42);
    for (int i = 0; i < 16; i++) {
        double x = bog_state_manager_random(a);
        ASSERT_GTE(x, 0.0);
        ASSERT_LT(x, 1.0);
        ASSERT_NEAR(x, bog_state_manager_random(b), 0.0);
    }
    bog_state_manager_destroy(a);
    bog_state_manager_destroy(b);
    TEST_PASS();
}

TEST(state_manager_streams_are_independent)
{
    BogStateManager* a = bog_state_manager_create();
    BogStateManager* other = bog_state_manager_create();
    BogStateManager* ref = bog_state_manager_create();
    bog_state_manager_seed(a, 7);
    bog_state_manager_seed(ref, 7);
    for (int i = 0; i < 16; i++) {
        bog_state_manager_random(other);
        srand((unsigned)i);  /* the C library stream must not matter */
        ASSERT_NEAR(bog_state_manager_random(a),
                    bog_state_manager_random(ref), 0.0);
    }
    bog_state_manager_destroy(a);
    bog_state_manager_destroy(other);
    bog_state_manager_destroy(ref);
    TEST_PASS();
}

TEST(state_manager_unseeded_instances_differ)
{
    BogStateManager* a = bog_state_manager_create();
    BogStateManager* b = bog_state_manager_create();
    ASSERT_TRUE(bog_state_manager_random(a) != bog_state_manager_random(b));
    bog_state_manager_destroy(a);
    bog_state_manager_destroy(b);
    TEST_PASS();
}

/* rand/3 through the resolver, drawing from the context's state manager */
static double query_rand(BogStateManager* sm)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;
    BogProgram* program = bog_parse_program("q(X) :- rand(0, 1, X).", arena,
                                            &error);
    free(error);
    BogTerm* args[1] = { bog_make_var(arena, "X") };
    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = bog_make_compound(arena, "q", args, 1);
    BogGoalList goals = { &goal, 1 };
    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = sm;
    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };
    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);
    double x = -1.0;
    if (solutions.count == 1)
        x = bog_subst_term(args[0], &solutions.envs[0], arena)->value.number;
    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    free(solutions.source_lines);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    return x;
}

TEST(rand_builtin_uses_state_manager_stream)
{
    BogStateManager* a = bog_state_manager_create();
    BogStateManager* b = bog_state_manager_create();
    bog_state_manager_seed(a, 99);
    bog_state_manager_seed(b, 99);
    srand(1);
    double x = query_rand(a);
    srand(2);
    double y = query_rand(b);
    ASSERT_GTE(x, 0.0);
    ASSERT_NEAR(x, y, 0.0);
    bog_state_manager_destroy(a);
    bog_state_manager_destroy(b);
    TEST_PASS();
}

int main(void)
{
    printf("State Manager Tests\n");
    printf("===================\n");

    /* Cycle State */
    RUN_TEST(state_manager_returns_0_for_new_cycle_key);
    RUN_TEST(state_manager_increments_cycle_and_returns_current);
    RUN_TEST(state_manager_wraps_cycle_at_list_length);
    RUN_TEST(state_manager_maintains_independent_cycle_state);
    RUN_TEST(state_manager_persists_cycle_state_across_queries);

    /* Cooldown State */
    RUN_TEST(state_manager_can_trigger_returns_true_for_new_key);
    RUN_TEST(state_manager_stores_and_retrieves_last_trigger);
    RUN_TEST(state_manager_maintains_independent_trigger_state);

    /* Reset */
    RUN_TEST(state_manager_reset_clears_cycle_state);
    RUN_TEST(state_manager_reset_clears_cooldown_state);
    RUN_TEST(state_manager_allows_rebuild_after_reset);

    /* Random stream */
    RUN_TEST(state_manager_same_seed_repeats_sequence);
    RUN_TEST(state_manager_streams_are_independent);
    RUN_TEST(state_manager_unseeded_instances_differ);
    RUN_TEST(rand_builtin_uses_state_manager_stream);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
