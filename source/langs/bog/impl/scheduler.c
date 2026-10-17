#include "scheduler.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "internal.h"

typedef struct {
    char* key;
    size_t value;
} CycleEntry;

typedef struct {
    char* key;
    double value;
    bool has_value;
} TriggerEntry;

struct BogStateManager {
    CycleEntry* cycles;
    size_t cycle_count;
    size_t cycle_capacity;
    TriggerEntry* triggers;
    size_t trigger_count;
    size_t trigger_capacity;
    uint64_t rng_state;
};

/* An event held for on-time dispatch */
typedef struct {
    char* voice;
    double time;
    double midi;
    double velocity;
    int source_line;
} QueuedEvent;

typedef struct {
    BogBeatCallback cb;
    void* userdata;
    int handle;
    bool active;
} BeatCallbackEntry;

struct BogScheduler {
    BogAudioCallbacks audio;
    const BogBuiltins* builtins;
    BogStateManager* state_manager;
    const BogProgram* program;
    double bpm;
    double swing;
    double lookahead_ms;
    double grid_beats;
    bool running;
    int current_beat;
    double last_scheduled_time;  /* Track last scheduled time to avoid duplicates */
    double last_query_time;      /* Latest event time queried, or -1 */
    int current_source_line;     /* Source line of currently triggering event */
    /* Program swap, applied to queries for times >= pending_boundary */
    const BogProgram* pending_program;
    double pending_boundary;
    bool has_pending;
    bool dispatch_on_time;
    QueuedEvent* queue;          /* sorted by time */
    size_t queue_count;
    size_t queue_capacity;
    BeatCallbackEntry* callbacks;
    size_t callback_count;
    size_t callback_capacity;
    int next_callback_handle;
};

struct BogTransitionManager {
    BogScheduler* scheduler;
    double quantization;
};

static char* dup_string(const char* src)
{
    if (!src)
        return NULL;
    size_t len = strlen(src) + 1;
    char* copy = (char*)malloc(len);
    if (copy)
        memcpy(copy, src, len);
    return copy;
}

static CycleEntry* find_cycle_entry(BogStateManager* mgr, const char* key)
{
    for (size_t i = 0; i < mgr->cycle_count; ++i) {
        if (strcmp(mgr->cycles[i].key, key) == 0)
            return &mgr->cycles[i];
    }
    return NULL;
}

static TriggerEntry* find_trigger_entry(BogStateManager* mgr,
                                        const char* key)
{
    for (size_t i = 0; i < mgr->trigger_count; ++i) {
        if (strcmp(mgr->triggers[i].key, key) == 0)
            return &mgr->triggers[i];
    }
    return NULL;
}

static void free_cycles(BogStateManager* mgr)
{
    for (size_t i = 0; i < mgr->cycle_count; ++i) {
        free(mgr->cycles[i].key);
    }
    free(mgr->cycles);
    mgr->cycles = NULL;
    mgr->cycle_count = 0;
    mgr->cycle_capacity = 0;
}

static void free_triggers(BogStateManager* mgr)
{
    for (size_t i = 0; i < mgr->trigger_count; ++i) {
        free(mgr->triggers[i].key);
    }
    free(mgr->triggers);
    mgr->triggers = NULL;
    mgr->trigger_count = 0;
    mgr->trigger_capacity = 0;
}

BogStateManager* bog_state_manager_create(void)
{
    BogStateManager* mgr = (BogStateManager*)calloc(
        1, sizeof(BogStateManager));
    if (mgr)
        mgr->rng_state = bog_rng_seed_from_clock(mgr);
    return mgr;
}

void bog_state_manager_destroy(BogStateManager* manager)
{
    if (!manager)
        return;
    free_cycles(manager);
    free_triggers(manager);
    free(manager);
}

void bog_state_manager_reset(BogStateManager* manager)
{
    if (!manager)
        return;
    for (size_t i = 0; i < manager->cycle_count; ++i) {
        free(manager->cycles[i].key);
        manager->cycles[i].key = NULL;
        manager->cycles[i].value = 0;
    }
    manager->cycle_count = 0;
    for (size_t i = 0; i < manager->trigger_count; ++i) {
        free(manager->triggers[i].key);
        manager->triggers[i].key = NULL;
        manager->triggers[i].has_value = false;
        manager->triggers[i].value = 0.0;
    }
    manager->trigger_count = 0;
}

size_t bog_state_manager_get_cycle(const BogStateManager* manager,
                                       const char* key)
{
    if (!manager || !key)
        return 0;
    for (size_t i = 0; i < manager->cycle_count; ++i) {
        if (strcmp(manager->cycles[i].key, key) == 0)
            return manager->cycles[i].value;
    }
    return 0;
}

size_t bog_state_manager_increment_cycle(BogStateManager* manager,
                                             const char* key,
                                             size_t list_length)
{
    if (!manager || !key || list_length == 0)
        return 0;
    CycleEntry* entry = find_cycle_entry(manager, key);
    if (!entry) {
        if (manager->cycle_count == manager->cycle_capacity) {
            size_t new_cap = manager->cycle_capacity
                ? manager->cycle_capacity * 2
                : 8;
            CycleEntry* new_items = (CycleEntry*)realloc(
                manager->cycles, new_cap * sizeof(CycleEntry));
            if (!new_items)
                return 0;
            manager->cycles = new_items;
            manager->cycle_capacity = new_cap;
        }
        manager->cycles[manager->cycle_count].key = dup_string(key);
        manager->cycles[manager->cycle_count].value = 0;
        entry = &manager->cycles[manager->cycle_count++];
    }
    size_t current = entry->value;
    entry->value = (entry->value + 1) % list_length;
    return current;
}

double
bog_state_manager_get_last_trigger(const BogStateManager* manager,
                                       const char* key, bool* found)
{
    if (found)
        *found = false;
    if (!manager || !key)
        return 0.0;
    for (size_t i = 0; i < manager->trigger_count; ++i) {
        if (strcmp(manager->triggers[i].key, key) == 0
            && manager->triggers[i].has_value) {
            if (found)
                *found = true;
            return manager->triggers[i].value;
        }
    }
    return 0.0;
}

void bog_state_manager_set_last_trigger(BogStateManager* manager,
                                            const char* key, double time_value)
{
    if (!manager || !key)
        return;
    TriggerEntry* entry = find_trigger_entry(manager, key);
    if (!entry) {
        if (manager->trigger_count == manager->trigger_capacity) {
            size_t new_cap = manager->trigger_capacity
                ? manager->trigger_capacity * 2
                : 8;
            TriggerEntry* new_items = (TriggerEntry*)realloc(
                manager->triggers, new_cap * sizeof(TriggerEntry));
            if (!new_items)
                return;
            manager->triggers = new_items;
            manager->trigger_capacity = new_cap;
        }
        manager->triggers[manager->trigger_count].key = dup_string(key);
        manager->triggers[manager->trigger_count].value = 0.0;
        manager->triggers[manager->trigger_count].has_value = false;
        entry = &manager->triggers[manager->trigger_count++];
    }
    entry->value = time_value;
    entry->has_value = true;
}

bool bog_state_manager_can_trigger(const BogStateManager* manager,
                                       const char* key, double now, double gap)
{
    bool found = false;
    double last = bog_state_manager_get_last_trigger(manager, key, &found);
    if (!found)
        return true;
    return (now - last) >= gap;
}

void bog_state_manager_seed(BogStateManager* manager, uint64_t seed)
{
    if (manager)
        manager->rng_state = seed;
}

double bog_state_manager_random(BogStateManager* manager)
{
    if (!manager)
        return 0.0;
    return bog_rng_unit(&manager->rng_state);
}

/* Scheduler */

static double swing_adjust(double t, double bpm, double swing_amt)
{
    double eighth = (60.0 / bpm) / 2.0;
    double pos = t / eighth;
    long is_odd = ((long)floor(pos)) % 2;
    return (is_odd == 1) ? t + swing_amt * eighth : t;
}

static void trigger_voice(const BogScheduler* scheduler, const char* voice,
                          double time_value, double midi, double velocity)
{
    if (!voice)
        return;
    const BogAudioCallbacks* audio = &scheduler->audio;
    if (audio->note) {
        audio->note(audio->userdata, voice, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "kick") == 0 && audio->kick) {
        audio->kick(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "snare") == 0 && audio->snare) {
        audio->snare(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "hat") == 0 && audio->hat) {
        audio->hat(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "clap") == 0 && audio->clap) {
        audio->clap(audio->userdata, time_value, velocity);
        return;
    }
    if (strcmp(voice, "sine") == 0 && audio->sine) {
        audio->sine(audio->userdata, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "square") == 0 && audio->square) {
        audio->square(audio->userdata, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "triangle") == 0 && audio->triangle) {
        audio->triangle(audio->userdata, time_value, midi, velocity);
        return;
    }
    if (strcmp(voice, "noise") == 0 && audio->noise) {
        audio->noise(audio->userdata, time_value, velocity);
        return;
    }
}

static void clear_queue(BogScheduler* scheduler)
{
    for (size_t i = 0; i < scheduler->queue_count; ++i)
        free(scheduler->queue[i].voice);
    scheduler->queue_count = 0;
}

/* Takes ownership of voice */
static void enqueue_event(BogScheduler* scheduler, char* voice, double time,
                          double midi, double velocity, int source_line)
{
    if (scheduler->queue_count == scheduler->queue_capacity) {
        size_t new_cap = scheduler->queue_capacity
            ? scheduler->queue_capacity * 2
            : 16;
        QueuedEvent* items = (QueuedEvent*)realloc(
            scheduler->queue, new_cap * sizeof(QueuedEvent));
        if (!items) {
            free(voice);
            return;
        }
        scheduler->queue = items;
        scheduler->queue_capacity = new_cap;
    }
    size_t pos = scheduler->queue_count;
    while (pos > 0 && scheduler->queue[pos - 1].time > time)
        pos--;
    memmove(&scheduler->queue[pos + 1], &scheduler->queue[pos],
            (scheduler->queue_count - pos) * sizeof(QueuedEvent));
    scheduler->queue[pos] = (QueuedEvent) { voice, time, midi, velocity,
                                            source_line };
    scheduler->queue_count++;
}

static void dispatch_due_events(BogScheduler* scheduler, double now_seconds)
{
    size_t due = 0;
    while (due < scheduler->queue_count
           && scheduler->queue[due].time <= now_seconds + 1e-9) {
        QueuedEvent* ev = &scheduler->queue[due];
        scheduler->current_source_line = ev->source_line;
        trigger_voice(scheduler, ev->voice, ev->time, ev->midi, ev->velocity);
        free(ev->voice);
        due++;
    }
    if (due) {
        memmove(scheduler->queue, &scheduler->queue[due],
                (scheduler->queue_count - due) * sizeof(QueuedEvent));
        scheduler->queue_count -= due;
    }
}

static void ensure_callback_capacity(BogScheduler* scheduler)
{
    if (scheduler->callback_count < scheduler->callback_capacity)
        return;
    size_t new_cap = scheduler->callback_capacity
        ? scheduler->callback_capacity * 2
        : 4;
    BeatCallbackEntry* entries = (BeatCallbackEntry*)realloc(
        scheduler->callbacks, new_cap * sizeof(BeatCallbackEntry));
    if (!entries)
        return;
    for (size_t i = scheduler->callback_capacity; i < new_cap; ++i) {
        entries[i].active = false;
        entries[i].cb = NULL;
        entries[i].userdata = NULL;
        entries[i].handle = -1;
    }
    scheduler->callbacks = entries;
    scheduler->callback_capacity = new_cap;
}

static void notify_beat_callbacks(BogScheduler* scheduler, int beat)
{
    for (size_t i = 0; i < scheduler->callback_capacity; ++i) {
        if (scheduler->callbacks[i].active && scheduler->callbacks[i].cb) {
            scheduler->callbacks[i].cb(beat, scheduler->callbacks[i].userdata);
        }
    }
}

static void scheduler_init(BogScheduler* scheduler,
                           const BogAudioCallbacks* audio,
                           const BogBuiltins* builtins,
                           BogStateManager* state_manager)
{
    if (!scheduler)
        return;
    memset(scheduler, 0, sizeof(*scheduler));
    if (audio)
        scheduler->audio = *audio;
    scheduler->builtins = builtins;
    scheduler->state_manager = state_manager;
    scheduler->bpm = 120.0;
    scheduler->swing = 0.0;
    scheduler->lookahead_ms = 80.0;
    scheduler->grid_beats = 0.25;
    scheduler->current_beat = 0;
    scheduler->last_scheduled_time = -1.0;  /* No events scheduled yet */
    scheduler->last_query_time = -1.0;
    scheduler->next_callback_handle = 1;
}

BogScheduler* bog_scheduler_create(const BogAudioCallbacks* audio,
                                           const BogBuiltins* builtins,
                                           BogStateManager* state_manager)
{
    BogScheduler* scheduler = (BogScheduler*)calloc(
        1, sizeof(BogScheduler));
    if (!scheduler)
        return NULL;
    scheduler_init(scheduler, audio, builtins, state_manager);
    return scheduler;
}

void bog_scheduler_destroy(BogScheduler* scheduler)
{
    if (!scheduler)
        return;
    clear_queue(scheduler);
    free(scheduler->queue);
    free(scheduler->callbacks);
    free(scheduler);
}

void bog_scheduler_set_program(BogScheduler* scheduler,
                                   const BogProgram* program)
{
    if (!scheduler)
        return;
    scheduler->program = program;
}

const BogProgram* bog_scheduler_get_program(const BogScheduler* scheduler)
{
    return scheduler ? scheduler->program : NULL;
}

void bog_scheduler_set_dispatch_on_time(BogScheduler* scheduler,
                                         bool enabled)
{
    if (scheduler)
        scheduler->dispatch_on_time = enabled;
}

void bog_scheduler_configure(BogScheduler* scheduler, double bpm,
                                 double swing, double lookahead_ms,
                                 double grid_beats)
{
    if (!scheduler)
        return;
    if (bpm > 0.0)
        scheduler->bpm = bpm;
    scheduler->swing = swing;
    if (lookahead_ms > 0.0)
        scheduler->lookahead_ms = lookahead_ms;
    if (grid_beats > 0.0)
        scheduler->grid_beats = grid_beats;
}

void bog_scheduler_start(BogScheduler* scheduler)
{
    if (!scheduler)
        return;
    if (scheduler->audio.init)
        scheduler->audio.init(scheduler->audio.userdata);
    scheduler->last_scheduled_time = -1.0;  /* Reset for fresh start */
    scheduler->last_query_time = -1.0;
    /* The clock restarts, so a boundary computed before start is stale */
    if (scheduler->has_pending) {
        scheduler->program = scheduler->pending_program;
        scheduler->pending_program = NULL;
        scheduler->has_pending = false;
    }
    scheduler->current_source_line = 0;
    scheduler->running = true;
}

void bog_scheduler_stop(BogScheduler* scheduler)
{
    if (!scheduler)
        return;
    scheduler->running = false;
    scheduler->current_beat = 0;
    scheduler->current_source_line = 0;
    clear_queue(scheduler);
    notify_beat_callbacks(scheduler, scheduler->current_beat);
}

double bog_scheduler_now(const BogScheduler* scheduler)
{
    if (!scheduler || !scheduler->audio.time)
        return 0.0;
    return scheduler->audio.time(scheduler->audio.userdata);
}

int bog_scheduler_get_current_source_line(const BogScheduler* scheduler)
{
    if (!scheduler)
        return 0;
    return scheduler->current_source_line;
}

int bog_scheduler_add_beat_callback(BogScheduler* scheduler,
                                        BogBeatCallback cb, void* userdata)
{
    if (!scheduler || !cb)
        return -1;
    ensure_callback_capacity(scheduler);
    for (size_t i = 0; i < scheduler->callback_capacity; ++i) {
        if (!scheduler->callbacks[i].active) {
            scheduler->callbacks[i].active = true;
            scheduler->callbacks[i].cb = cb;
            scheduler->callbacks[i].userdata = userdata;
            scheduler->callbacks[i].handle = scheduler->next_callback_handle++;
            scheduler->callback_count++;
            return scheduler->callbacks[i].handle;
        }
    }
    return -1;
}

void bog_scheduler_remove_beat_callback(BogScheduler* scheduler,
                                            int handle)
{
    if (!scheduler || handle < 0)
        return;
    for (size_t i = 0; i < scheduler->callback_capacity; ++i) {
        if (scheduler->callbacks[i].active
            && scheduler->callbacks[i].handle == handle) {
            scheduler->callbacks[i].active = false;
            scheduler->callbacks[i].cb = NULL;
            scheduler->callbacks[i].userdata = NULL;
            scheduler->callbacks[i].handle = -1;
            if (scheduler->callback_count > 0)
                scheduler->callback_count--;
            break;
        }
    }
}

static void scheduler_query_and_schedule(BogScheduler* scheduler, double t)
{
    if (scheduler->has_pending && t + 1e-9 >= scheduler->pending_boundary) {
        scheduler->program = scheduler->pending_program;
        scheduler->pending_program = NULL;
        scheduler->has_pending = false;
    }
    if (t > scheduler->last_query_time)
        scheduler->last_query_time = t;
    if (!scheduler->program || !scheduler->builtins)
        return;
    BogArena* arena = bog_arena_create();
    if (!arena)
        return;

    BogTerm voiceVar = { .type = CPROLOG_TERM_VAR, .value.atom = "Voice" };
    BogTerm pitchVar = { .type = CPROLOG_TERM_VAR, .value.atom = "Pitch" };
    BogTerm velVar = { .type = CPROLOG_TERM_VAR, .value.atom = "Vel" };
    BogTerm timeTerm = { .type = CPROLOG_TERM_NUM };
    timeTerm.value.number = t;
    BogTerm* args[4] = { &voiceVar, &pitchVar, &velVar, &timeTerm };
    BogTerm eventTerm;
    eventTerm.type = CPROLOG_TERM_COMPOUND;
    eventTerm.value.compound.functor = "event";
    eventTerm.value.compound.args = args;
    eventTerm.value.compound.arity = 4;

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = &eventTerm;
    BogGoalList goals;
    goals.count = 1;
    goals.items = &goal;

    BogEnv env;
    bog_env_init(&env);

    BogSolutions solutions = { 0 };
    BogContext ctx;
    ctx.bpm = scheduler->bpm;
    ctx.state_manager = scheduler->state_manager;
    bog_resolve(&goals, &env, scheduler->program, &ctx,
                    scheduler->builtins, &solutions, arena);
    bog_env_free(&env);

    for (size_t i = 0; i < solutions.count; ++i) {
        BogArena* subst_arena = bog_arena_create();
        if (!subst_arena)
            continue;
        BogTerm* voiceValue = bog_subst_term(
            &voiceVar, &solutions.envs[i], subst_arena);
        char* voiceStr = voiceValue
            ? bog_term_to_string(voiceValue, subst_arena)
            : NULL;

        BogTerm* pitchValue = bog_subst_term(
            &pitchVar, &solutions.envs[i], subst_arena);
        double midi = 48.0;
        if (pitchValue && pitchValue->type == CPROLOG_TERM_NUM) {
            midi = pitchValue->value.number;
        }

        BogTerm* velValue = bog_subst_term(&velVar, &solutions.envs[i],
                                                   subst_arena);
        double vel = 0.7;
        if (velValue && velValue->type == CPROLOG_TERM_NUM) {
            if (velValue->value.number < 0.0)
                vel = 0.0;
            else if (velValue->value.number > 1.0)
                vel = 1.0;
            else
                vel = velValue->value.number;
        }

        double scheduled_time = swing_adjust(t, scheduler->bpm,
                                             scheduler->swing);
        int source_line = solutions.source_lines ? solutions.source_lines[i]
                                                 : 0;
        if (voiceStr && scheduler->dispatch_on_time) {
            enqueue_event(scheduler, voiceStr, scheduled_time, midi, vel,
                          source_line);
            voiceStr = NULL;
        } else if (voiceStr) {
            /* Track source line of the matched event clause */
            scheduler->current_source_line = source_line;
            trigger_voice(scheduler, voiceStr, scheduled_time, midi, vel);
        }
        free(voiceStr);
        bog_env_free(&solutions.envs[i]);
        bog_arena_destroy(subst_arena);
    }
    free(solutions.envs);
    free(solutions.source_lines);
    bog_arena_destroy(arena);
}

static void query_ahead(BogScheduler* scheduler, double now_seconds);

void bog_scheduler_tick_at(BogScheduler* scheduler, double now_seconds)
{
    if (!scheduler)
        return;
    if (scheduler->program || scheduler->has_pending)
        query_ahead(scheduler, now_seconds);
    dispatch_due_events(scheduler, now_seconds);
}

static void query_ahead(BogScheduler* scheduler, double now_seconds)
{
    double ahead = scheduler->lookahead_ms / 1000.0;
    double beat_duration = 60.0 / scheduler->bpm;
    double step = beat_duration * scheduler->grid_beats;
    if (step <= 0.0)
        step = beat_duration * 0.25;
    double start_quantized = floor(now_seconds / step) * step;

    int new_beat = (int)floor(now_seconds / beat_duration);
    if (new_beat != scheduler->current_beat) {
        scheduler->current_beat = new_beat;
        notify_beat_callbacks(scheduler, scheduler->current_beat);
    }

    /* Start from after last scheduled time to avoid duplicates */
    double start_time = start_quantized;
    if (scheduler->last_scheduled_time >= 0.0 &&
        scheduler->last_scheduled_time >= start_quantized) {
        start_time = scheduler->last_scheduled_time + step;
    }

    for (double t = start_time; t < now_seconds + ahead; t += step) {
        scheduler_query_and_schedule(scheduler, t + step);
        scheduler->last_scheduled_time = t;
    }
}

void bog_scheduler_tick(BogScheduler* scheduler)
{
    if (!scheduler || !scheduler->running)
        return;
    double now = bog_scheduler_now(scheduler);
    bog_scheduler_tick_at(scheduler, now);
}

/* Transition manager */

static void transition_manager_init(BogTransitionManager* manager,
                                    BogScheduler* scheduler,
                                    double quantization_beats)
{
    if (!manager)
        return;
    manager->scheduler = scheduler;
    manager->quantization = (quantization_beats > 0.0) ? quantization_beats
                                                       : 4.0;
}

BogTransitionManager*
bog_transition_manager_create(BogScheduler* scheduler,
                                  double quantization_beats)
{
    BogTransitionManager* manager = (BogTransitionManager*)calloc(
        1, sizeof(BogTransitionManager));
    if (!manager)
        return NULL;
    transition_manager_init(manager, scheduler, quantization_beats);
    return manager;
}

void bog_transition_manager_destroy(BogTransitionManager* manager)
{
    if (!manager)
        return;
    free(manager);
}

/* The swap lands on the first bar line at or after now that no query has
 * reached yet, so no event from that bar on comes from the old program. */
void bog_transition_manager_schedule(BogTransitionManager* manager,
                                         const BogProgram* program)
{
    if (!manager || !manager->scheduler || !program)
        return;
    BogScheduler* scheduler = manager->scheduler;
    double now = bog_scheduler_now(scheduler);
    double bpm = scheduler->bpm > 0.0 ? scheduler->bpm : 120.0;
    double quant_duration = (60.0 / bpm) * manager->quantization;
    double bar = ceil(now / quant_duration - 1e-9);
    if (scheduler->last_query_time >= 0.0) {
        double after_queried
            = floor(scheduler->last_query_time / quant_duration + 1e-9) + 1.0;
        if (after_queried > bar)
            bar = after_queried;
    }
    scheduler->pending_boundary = bar * quant_duration;
    scheduler->pending_program = program;
    scheduler->has_pending = true;
}

void bog_transition_manager_cancel(BogTransitionManager* manager)
{
    if (!manager || !manager->scheduler)
        return;
    manager->scheduler->pending_program = NULL;
    manager->scheduler->pending_boundary = 0.0;
    manager->scheduler->has_pending = false;
}

bool bog_transition_manager_has_pending(
    const BogTransitionManager* manager)
{
    return manager && manager->scheduler && manager->scheduler->has_pending;
}

/* Fallback for when no query reaches the boundary, e.g. a stopped scheduler */
void bog_transition_manager_process(BogTransitionManager* manager,
                                        double now_seconds)
{
    if (!bog_transition_manager_has_pending(manager))
        return;
    BogScheduler* scheduler = manager->scheduler;
    if (now_seconds + 1e-9 >= scheduler->pending_boundary) {
        scheduler->program = scheduler->pending_program;
        scheduler->pending_program = NULL;
        scheduler->has_pending = false;
        scheduler->pending_boundary = 0.0;
    }
}
