/*
 * Scheduler tests: note callback, on-time dispatch, program transitions
 */
#include "test_framework.h"

#include <string.h>

#include "bog.h"
#include "builtins.h"
#include "livecoding.h"
#include "scheduler.h"

#define REC_MAX 64

typedef struct {
    double now;
    size_t count;
    char voices[REC_MAX][16];
    double times[REC_MAX];
    double midis[REC_MAX];
    double fired_at[REC_MAX];
    int kick_calls;
} Recorder;

static double rec_time(void* userdata)
{
    return ((Recorder*)userdata)->now;
}

static void rec_note(void* userdata, const char* voice, double time,
                     double midi, double velocity)
{
    (void)velocity;
    Recorder* rec = (Recorder*)userdata;
    if (rec->count >= REC_MAX)
        return;
    strncpy(rec->voices[rec->count], voice, 15);
    rec->voices[rec->count][15] = '\0';
    rec->times[rec->count] = time;
    rec->midis[rec->count] = midi;
    rec->fired_at[rec->count] = rec->now;
    rec->count++;
}

static void rec_kick(void* userdata, double time, double velocity)
{
    rec_note(userdata, "kick", time, 0.0, velocity);
    ((Recorder*)userdata)->kick_calls++;
}

typedef struct {
    BogArena* arena;
    BogBuiltins* builtins;
    BogStateManager* sm;
    BogScheduler* sched;
    Recorder rec;
} Fixture;

static void fixture_init(Fixture* f, bool use_note)
{
    memset(f, 0, sizeof(*f));
    f->arena = bog_arena_create();
    f->builtins = bog_create_builtins(f->arena);
    f->sm = bog_state_manager_create();
    BogAudioCallbacks audio = { 0 };
    audio.userdata = &f->rec;
    audio.time = rec_time;
    audio.kick = rec_kick;
    if (use_note)
        audio.note = rec_note;
    f->sched = bog_scheduler_create(&audio, f->builtins, f->sm);
    bog_scheduler_configure(f->sched, 120.0, 0.0, 80.0, 0.25);
    bog_scheduler_start(f->sched);
}

static void fixture_free(Fixture* f)
{
    bog_scheduler_destroy(f->sched);
    bog_state_manager_destroy(f->sm);
    bog_arena_destroy(f->arena);
}

static BogProgram* parse(Fixture* f, const char* src)
{
    char* error = NULL;
    BogProgram* program = bog_parse_program(src, f->arena, &error);
    free(error);
    return program;
}

/* Tick every 10 ms over [from, to), processing transitions like the hosts */
static void run(Fixture* f, BogTransitionManager* tm, double from, double to)
{
    for (double t = from; t < to - 1e-9; t += 0.01) {
        f->rec.now = t;
        bog_scheduler_tick_at(f->sched, t);
        if (tm)
            bog_transition_manager_process(tm, t);
    }
}

TEST(scheduler_note_callback_receives_any_voice)
{
    Fixture f;
    fixture_init(&f, true);
    BogProgram* program = parse(&f,
        "event(bass, 40, 0.8, T) :- every(T, 1.0).\n"
        "event(kick, 36, 0.9, T) :- every(T, 1.0).");
    ASSERT_NOT_NULL(program);
    bog_scheduler_set_program(f.sched, program);

    run(&f, NULL, 0.0, 0.45);

    ASSERT_EQ(f.rec.count, 2);
    ASSERT_STR_EQ(f.rec.voices[0], "bass");
    ASSERT_NEAR(f.rec.midis[0], 40.0, 1e-9);
    ASSERT_NEAR(f.rec.times[0], 0.5, 1e-9);
    ASSERT_STR_EQ(f.rec.voices[1], "kick");
    /* note takes every event, so the named callback is unused */
    ASSERT_EQ(f.rec.kick_calls, 0);

    fixture_free(&f);
    TEST_PASS();
}

TEST(scheduler_named_callbacks_used_without_note)
{
    Fixture f;
    fixture_init(&f, false);
    BogProgram* program = parse(&f,
        "event(bass, 40, 0.8, T) :- every(T, 1.0).\n"
        "event(kick, 36, 0.9, T) :- every(T, 1.0).");
    bog_scheduler_set_program(f.sched, program);

    run(&f, NULL, 0.0, 0.45);

    ASSERT_EQ(f.rec.kick_calls, 1);
    ASSERT_EQ(f.rec.count, 1);

    fixture_free(&f);
    TEST_PASS();
}

TEST(scheduler_dispatch_on_time_fires_at_event_time)
{
    Fixture f;
    fixture_init(&f, true);
    bog_scheduler_set_dispatch_on_time(f.sched, true);
    BogProgram* program = parse(&f, "event(hat, 42, 0.5, T) :- every(T, 0.5).");
    bog_scheduler_set_program(f.sched, program);

    run(&f, NULL, 0.0, 1.0);

    /* events at 0.25, 0.5, 0.75; 1.0 is queried but not yet due */
    ASSERT_EQ(f.rec.count, 3);
    for (size_t i = 0; i < f.rec.count; i++) {
        ASSERT_GTE(f.rec.fired_at[i], f.rec.times[i] - 1e-9);
        ASSERT_LT(f.rec.fired_at[i], f.rec.times[i] + 0.01 + 1e-9);
    }

    fixture_free(&f);
    TEST_PASS();
}

TEST(scheduler_stop_drops_queued_events)
{
    Fixture f;
    fixture_init(&f, true);
    bog_scheduler_set_dispatch_on_time(f.sched, true);
    BogProgram* program = parse(&f, "event(hat, 42, 0.5, T) :- every(T, 0.5).");
    bog_scheduler_set_program(f.sched, program);

    run(&f, NULL, 0.0, 0.2);  /* queries 0.25 */
    ASSERT_EQ(f.rec.count, 0);
    bog_scheduler_stop(f.sched);
    bog_scheduler_start(f.sched);
    bog_scheduler_set_program(f.sched, NULL);
    run(&f, NULL, 0.2, 0.4);
    ASSERT_EQ(f.rec.count, 0);

    fixture_free(&f);
    TEST_PASS();
}

TEST(transition_swaps_at_event_time)
{
    Fixture f;
    fixture_init(&f, true);
    BogProgram* old_prog = parse(&f, "event(kick, 36, 0.9, T) :- every(T, 0.5).");
    BogProgram* new_prog = parse(&f, "event(snare, 38, 0.9, T) :- every(T, 0.5).");
    bog_scheduler_set_program(f.sched, old_prog);
    BogTransitionManager* tm = bog_transition_manager_create(f.sched, 4.0);

    run(&f, tm, 0.0, 1.5);
    f.rec.count = 0;
    bog_transition_manager_schedule(tm, new_prog);  /* bar line at 2.0 s */
    ASSERT_TRUE(bog_transition_manager_has_pending(tm));
    run(&f, tm, 1.5, 2.6);
    ASSERT_FALSE(bog_transition_manager_has_pending(tm));

    ASSERT_GT(f.rec.count, 0);
    for (size_t i = 0; i < f.rec.count; i++) {
        const char* want = f.rec.times[i] < 2.0 - 1e-9 ? "kick" : "snare";
        ASSERT_STR_EQ(f.rec.voices[i], want);
    }

    bog_transition_manager_destroy(tm);
    fixture_free(&f);
    TEST_PASS();
}

TEST(transition_after_queried_bar_waits_for_next_bar)
{
    Fixture f;
    fixture_init(&f, true);
    BogProgram* old_prog = parse(&f, "event(kick, 36, 0.9, T) :- every(T, 0.5).");
    BogProgram* new_prog = parse(&f, "event(snare, 38, 0.9, T) :- every(T, 0.5).");
    bog_scheduler_set_program(f.sched, old_prog);
    BogTransitionManager* tm = bog_transition_manager_create(f.sched, 4.0);

    run(&f, tm, 0.0, 1.95);  /* events at 2.0 s already queried */
    f.rec.count = 0;
    bog_transition_manager_schedule(tm, new_prog);
    run(&f, tm, 1.95, 4.6);

    for (size_t i = 0; i < f.rec.count; i++) {
        const char* want = f.rec.times[i] < 4.0 - 1e-9 ? "kick" : "snare";
        ASSERT_STR_EQ(f.rec.voices[i], want);
    }
    ASSERT_STR_EQ(f.rec.voices[f.rec.count - 1], "snare");

    bog_transition_manager_destroy(tm);
    fixture_free(&f);
    TEST_PASS();
}

TEST(transition_without_program_applies_at_boundary)
{
    Fixture f;
    fixture_init(&f, true);
    BogProgram* prog = parse(&f, "event(snare, 38, 0.9, T) :- every(T, 0.5).");
    BogTransitionManager* tm = bog_transition_manager_create(f.sched, 4.0);

    f.rec.now = 0.5;
    bog_transition_manager_schedule(tm, prog);
    run(&f, tm, 0.5, 2.6);

    ASSERT_GT(f.rec.count, 0);
    for (size_t i = 0; i < f.rec.count; i++)
        ASSERT_GTE(f.rec.times[i], 2.0 - 1e-9);

    bog_transition_manager_destroy(tm);
    fixture_free(&f);
    TEST_PASS();
}

TEST(start_applies_pending_swap)
{
    Fixture f;
    fixture_init(&f, true);
    BogProgram* prog = parse(&f, "event(snare, 38, 0.9, T) :- every(T, 0.5).");
    BogTransitionManager* tm = bog_transition_manager_create(f.sched, 4.0);

    bog_scheduler_stop(f.sched);
    f.rec.now = 1000.3;  /* clock still running from the last start */
    bog_transition_manager_schedule(tm, prog);
    f.rec.now = 0.0;     /* hosts restart the clock in start */
    bog_scheduler_start(f.sched);

    ASSERT_PTR_EQ(bog_scheduler_get_program(f.sched), prog);
    ASSERT_FALSE(bog_transition_manager_has_pending(tm));

    bog_transition_manager_destroy(tm);
    fixture_free(&f);
    TEST_PASS();
}

/* Under ASan this also checks that a program's arena outlives its use */
TEST(evaluator_swaps_on_bar_and_drops_unplayed_program)
{
    Fixture f;
    fixture_init(&f, true);
    BogTransitionManager* tm = bog_transition_manager_create(f.sched, 4.0);
    BogLiveEvaluator* ev = bog_live_evaluator_create(f.sched, 0.0);
    bog_live_evaluator_set_transition(ev, tm);

    ASSERT_TRUE(bog_live_evaluator_evaluate(
        ev, "event(kick, 36, 0.9, T) :- every(T, 0.5).", NULL));
    run(&f, tm, 0.0, 1.5);
    ASSERT_TRUE(bog_live_evaluator_evaluate(
        ev, "event(snare, 38, 0.9, T) :- every(T, 0.5).", NULL));
    run(&f, tm, 1.5, 2.5);
    /* hat is replaced before its bar, so it never plays */
    ASSERT_TRUE(bog_live_evaluator_evaluate(
        ev, "event(hat, 42, 0.9, T) :- every(T, 0.5).", NULL));
    run(&f, tm, 2.5, 2.6);
    ASSERT_TRUE(bog_live_evaluator_evaluate(
        ev, "event(clap, 39, 0.9, T) :- every(T, 0.5).", NULL));
    run(&f, tm, 2.6, 4.6);

    ASSERT_GT(f.rec.count, 0);
    for (size_t i = 0; i < f.rec.count; i++) {
        double t = f.rec.times[i];
        const char* want = t < 2.0 - 1e-9 ? "kick"
            : t < 4.0 - 1e-9              ? "snare"
                                          : "clap";
        ASSERT_STR_EQ(f.rec.voices[i], want);
    }
    ASSERT_STR_EQ(f.rec.voices[f.rec.count - 1], "clap");

    bog_live_evaluator_destroy(ev);
    bog_transition_manager_destroy(tm);
    fixture_free(&f);
    TEST_PASS();
}

int main(void)
{
    printf("Scheduler Tests\n");
    printf("===============\n");

    RUN_TEST(scheduler_note_callback_receives_any_voice);
    RUN_TEST(scheduler_named_callbacks_used_without_note);
    RUN_TEST(scheduler_dispatch_on_time_fires_at_event_time);
    RUN_TEST(scheduler_stop_drops_queued_events);
    RUN_TEST(transition_swaps_at_event_time);
    RUN_TEST(transition_after_queried_bar_waits_for_next_bar);
    RUN_TEST(transition_without_program_applies_at_boundary);
    RUN_TEST(start_applies_pending_swap);
    RUN_TEST(evaluator_swaps_on_bar_and_drops_unplayed_program);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
