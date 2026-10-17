#include "livecoding.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "internal.h"

typedef struct {
    BogLiveValidatedCallback cb;
    void* userdata;
    int handle;
    bool active;
} LiveCallbackEntry;

struct BogLiveEvaluator {
    BogScheduler* scheduler;
    double debounce_seconds;
    char* last_code;
    BogTransitionManager* transition;
    BogArena* program_arena;     /* latest program; may be a pending swap */
    const BogProgram* program;
    BogArena* playing_arena;     /* program still playing while latest is
                                  * pending, else NULL */
    LiveCallbackEntry* callbacks;
    size_t callback_count;
    size_t callback_capacity;
    int next_handle;
};

static char* trim_copy(const char* text)
{
    if (!text)
        text = "";
    const char* start = text;
    while (*start && isspace((unsigned char)*start))
        start++;
    const char* end = text + strlen(text);
    while (end > start && isspace((unsigned char)end[-1]))
        end--;
    size_t len = (size_t)(end - start);
    char* out = (char*)malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

bool bog_validate_code(const char* text, BogProgram** program_out,
                           BogArena** arena_out, char** error_message)
{
    if (program_out)
        *program_out = NULL;
    if (arena_out)
        *arena_out = NULL;
    if (error_message)
        *error_message = NULL;

    char* trimmed = trim_copy(text);
    if (!trimmed)
        return false;
    size_t len = strlen(trimmed);

    if (len == 0) {
        BogArena* arena = bog_arena_create();
        if (!arena) {
            free(trimmed);
            return false;
        }
        BogProgram* program = (BogProgram*)bog_arena_alloc(
            arena, sizeof(BogProgram));
        if (!program) {
            bog_arena_destroy(arena);
            free(trimmed);
            return false;
        }
        program->clauses = NULL;
        program->count = 0;
        if (arena_out) {
            *arena_out = arena;
            if (program_out)
                *program_out = program;
        } else {
            if (program_out)
                *program_out = NULL;
            bog_arena_destroy(arena);
        }
        free(trimmed);
        return true;
    }

    bool normalized_is_trimmed = true;
    bool ends_with_dot = trimmed[len - 1] == '.';
    char* normalized = trimmed;
    if (!ends_with_dot) {
        normalized = (char*)malloc(len + 2);
        if (!normalized) {
            free(trimmed);
            return false;
        }
        memcpy(normalized, trimmed, len);
        normalized[len] = '.';
        normalized[len + 1] = '\0';
        normalized_is_trimmed = false;
        free(trimmed);
    }

    BogArena* arena = bog_arena_create();
    if (!arena) {
        if (normalized_is_trimmed)
            free(trimmed);
        else
            free(normalized);
        return false;
    }

    char* parse_error = NULL;
    BogProgram* program = bog_parse_program(normalized, arena,
                                                    &parse_error);
    if (normalized_is_trimmed)
        free(trimmed);
    else
        free(normalized);

    if (!program) {
        if (error_message && parse_error) {
            *error_message = parse_error;
        } else {
            free(parse_error);
        }
        bog_arena_destroy(arena);
        return false;
    }

    free(parse_error);
    if (arena_out) {
        *arena_out = arena;
        if (program_out)
            *program_out = program;
    } else {
        if (program_out)
            *program_out = NULL;
        bog_arena_destroy(arena);
    }
    return true;
}

static void ensure_callback_capacity(BogLiveEvaluator* evaluator)
{
    if (evaluator->callback_count < evaluator->callback_capacity)
        return;
    size_t new_cap = evaluator->callback_capacity
        ? evaluator->callback_capacity * 2
        : 4;
    LiveCallbackEntry* entries = (LiveCallbackEntry*)realloc(
        evaluator->callbacks, new_cap * sizeof(LiveCallbackEntry));
    if (!entries)
        return;
    for (size_t i = evaluator->callback_capacity; i < new_cap; ++i) {
        entries[i].active = false;
        entries[i].cb = NULL;
        entries[i].userdata = NULL;
        entries[i].handle = -1;
    }
    evaluator->callbacks = entries;
    evaluator->callback_capacity = new_cap;
}

static void notify_callbacks(BogLiveEvaluator* evaluator, bool success,
                             const char* text, const char* error_message)
{
    for (size_t i = 0; i < evaluator->callback_capacity; ++i) {
        if (evaluator->callbacks[i].active && evaluator->callbacks[i].cb) {
            evaluator->callbacks[i].cb(
                success, success ? evaluator->program : NULL, text,
                error_message, evaluator->callbacks[i].userdata);
        }
    }
}

BogLiveEvaluator*
bog_live_evaluator_create(BogScheduler* scheduler,
                              double debounce_seconds)
{
    BogLiveEvaluator* evaluator = (BogLiveEvaluator*)calloc(
        1, sizeof(BogLiveEvaluator));
    if (!evaluator)
        return NULL;
    evaluator->scheduler = scheduler;
    evaluator->debounce_seconds = debounce_seconds;
    evaluator->next_handle = 1;
    return evaluator;
}

void bog_live_evaluator_destroy(BogLiveEvaluator* evaluator)
{
    if (!evaluator)
        return;
    free(evaluator->last_code);
    if (evaluator->program_arena)
        bog_arena_destroy(evaluator->program_arena);
    if (evaluator->playing_arena)
        bog_arena_destroy(evaluator->playing_arena);
    free(evaluator->callbacks);
    free(evaluator);
}

void bog_live_evaluator_set_scheduler(BogLiveEvaluator* evaluator,
                                          BogScheduler* scheduler)
{
    if (!evaluator)
        return;
    evaluator->scheduler = scheduler;
}

void bog_live_evaluator_set_transition(BogLiveEvaluator* evaluator,
                                       BogTransitionManager* transition)
{
    if (!evaluator)
        return;
    evaluator->transition = transition;
}

/* Swap through the transition manager. The playing program's arena stays
 * alive until the scheduler stops using it. */
static void install_at_transition(BogLiveEvaluator* evaluator,
                                  BogArena* arena, BogProgram* program)
{
    if (evaluator->playing_arena
        && bog_scheduler_get_program(evaluator->scheduler)
            == evaluator->program) {
        bog_arena_destroy(evaluator->playing_arena);
        evaluator->playing_arena = NULL;
    }
    BogArena* replaced = NULL;
    if (evaluator->playing_arena)
        replaced = evaluator->program_arena; /* pending, never played */
    else
        evaluator->playing_arena = evaluator->program_arena;
    evaluator->program_arena = arena;
    evaluator->program = program;
    bog_transition_manager_schedule(evaluator->transition, program);
    if (replaced)
        bog_arena_destroy(replaced);
}

bool bog_live_evaluator_evaluate(BogLiveEvaluator* evaluator,
                                     const char* text, char** error_message)
{
    if (!evaluator)
        return false;
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* err = NULL;
    bool ok = bog_validate_code(text, &program, &arena, &err);
    if (!ok) {
        if (arena)
            bog_arena_destroy(arena);
        notify_callbacks(evaluator, false, text ? text : "", err);
        if (error_message)
            *error_message = err;
        else
            free(err);
        return false;
    }

    if (error_message)
        *error_message = NULL;
    free(evaluator->last_code);
    evaluator->last_code = text ? strdup(text) : NULL;

    if (evaluator->transition && evaluator->scheduler) {
        install_at_transition(evaluator, arena, program);
    } else {
        if (evaluator->scheduler)
            bog_scheduler_set_program(evaluator->scheduler, program);
        if (evaluator->program_arena)
            bog_arena_destroy(evaluator->program_arena);
        if (evaluator->playing_arena)
            bog_arena_destroy(evaluator->playing_arena);
        evaluator->playing_arena = NULL;
        evaluator->program_arena = arena;
        evaluator->program = program;
    }

    notify_callbacks(evaluator, true,
                     evaluator->last_code ? evaluator->last_code : "", NULL);
    return true;
}

const char*
bog_live_evaluator_last_code(const BogLiveEvaluator* evaluator)
{
    return evaluator ? evaluator->last_code : NULL;
}

const BogProgram*
bog_live_evaluator_program(const BogLiveEvaluator* evaluator)
{
    return evaluator ? evaluator->program : NULL;
}

int bog_live_evaluator_on_validated(BogLiveEvaluator* evaluator,
                                        BogLiveValidatedCallback callback,
                                        void* userdata)
{
    if (!evaluator || !callback)
        return -1;
    ensure_callback_capacity(evaluator);
    for (size_t i = 0; i < evaluator->callback_capacity; ++i) {
        if (!evaluator->callbacks[i].active) {
            evaluator->callbacks[i].active = true;
            evaluator->callbacks[i].cb = callback;
            evaluator->callbacks[i].userdata = userdata;
            evaluator->callbacks[i].handle = evaluator->next_handle++;
            evaluator->callback_count++;
            return evaluator->callbacks[i].handle;
        }
    }
    return -1;
}

void bog_live_evaluator_remove_callback(BogLiveEvaluator* evaluator,
                                            int handle)
{
    if (!evaluator || handle < 0)
        return;
    for (size_t i = 0; i < evaluator->callback_capacity; ++i) {
        if (evaluator->callbacks[i].active
            && evaluator->callbacks[i].handle == handle) {
            evaluator->callbacks[i].active = false;
            evaluator->callbacks[i].cb = NULL;
            evaluator->callbacks[i].userdata = NULL;
            evaluator->callbacks[i].handle = -1;
            if (evaluator->callback_count > 0)
                evaluator->callback_count--;
            break;
        }
    }
}
