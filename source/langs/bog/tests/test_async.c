/*
 * bog_async tests: REPL-style program swaps while the tick thread runs.
 * Run under ASan or TSan to check the swap and the old program's release.
 */
#include "test_framework.h"

#include <stdatomic.h>
#include <time.h>

#include "bog.h"
#include "bog_async.h"
#include "builtins.h"
#include "livecoding.h"
#include "scheduler.h"

static atomic_int g_notes;
static struct timespec g_start;

static double wall_time(void* userdata)
{
    (void)userdata;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)(ts.tv_sec - g_start.tv_sec)
        + (double)(ts.tv_nsec - g_start.tv_nsec) / 1e9;
}

static void count_note(void* userdata, const char* voice, double time,
                       double midi, double velocity)
{
    (void)userdata;
    (void)voice;
    (void)time;
    (void)midi;
    (void)velocity;
    atomic_fetch_add(&g_notes, 1);
}

TEST(async_swaps_under_lock_while_ticking)
{
    static const char* programs[] = {
        "event(kick, 36, 0.9, T) :- every(T, 0.125).",
        "event(snare, 38, 0.9, T) :- every(T, 0.125).",
        "event(hat, 42, 0.5, T) :- every(T, 0.125), prob(0.5).",
    };
    clock_gettime(CLOCK_MONOTONIC, &g_start);
    atomic_store(&g_notes, 0);

    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogStateManager* sm = bog_state_manager_create();
    BogAudioCallbacks audio = { 0 };
    audio.time = wall_time;
    audio.note = count_note;
    BogScheduler* sched = bog_scheduler_create(&audio, builtins, sm);
    /* 1-beat bars at 960 BPM: a swap lands every 62.5 ms */
    bog_scheduler_configure(sched, 960.0, 0.0, 20.0, 0.125);
    bog_scheduler_set_dispatch_on_time(sched, true);
    BogTransitionManager* tm = bog_transition_manager_create(sched, 1.0);
    BogLiveEvaluator* ev = bog_live_evaluator_create(sched, 0.0);
    bog_live_evaluator_set_transition(ev, tm);

    ASSERT_TRUE(bog_live_evaluator_evaluate(ev, programs[0], NULL));
    bog_scheduler_start(sched);
    ASSERT_EQ(bog_async_start(sched, tm), 0);

    struct timespec pause = { 0, 3 * 1000 * 1000 };
    for (int i = 0; i < 200; i++) {
        bog_async_lock();
        bool ok = bog_live_evaluator_evaluate(ev, programs[i % 3], NULL);
        bog_async_unlock();
        ASSERT_TRUE(ok);
        nanosleep(&pause, NULL);
    }

    bog_async_stop();
    bog_async_cleanup();
    ASSERT_GT(atomic_load(&g_notes), 0);

    bog_live_evaluator_destroy(ev);
    bog_transition_manager_destroy(tm);
    bog_scheduler_destroy(sched);
    bog_state_manager_destroy(sm);
    bog_arena_destroy(arena);
    TEST_PASS();
}

int main(void)
{
    printf("Async Tests\n");
    printf("===========\n");

    RUN_TEST(async_swaps_under_lock_while_ticking);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
