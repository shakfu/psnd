/* bog/register.c - Bog language integration for Loki editor
 *
 * Implements the LokiLangOps interface to integrate the Bog Prolog-based
 * music live coding language with the Loki editor.
 *
 * Voice-to-MIDI mapping: see bog_host.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psnd.h"
#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"
#include "lauxlib.h"

/* Bog library headers */
#include "bog.h"
#include "scheduler.h"
#include "livecoding.h"
#include "bog_host.h"

/* Shared backend for MIDI/audio */
#include "context.h"
#include "midi/midi.h"
#include "audio/audio.h"

/* ======================= Constants ======================= */

/* Error buffer size */
#define BOG_ERROR_BUFSIZE 512

/* ======================= Internal State ======================= */

/* Per-context Bog state */
struct LokiBogState {
    int initialized;
    BogArena *arena;
    BogBuiltins *builtins;
    BogStateManager *state_manager;
    BogScheduler *scheduler;
    BogTransitionManager *transition_manager;
    BogProgram *current_program;
    SharedContext *shared;
    char last_error[BOG_ERROR_BUFSIZE];
    int running;
    double tempo;
    double swing;
    double start_time;  /* bog_wall_time() at scheduler start */
};

/* Get Bog state from editor context */
static struct LokiBogState* get_bog_state(editor_ctx_t *ctx) {
    return ctx ? ctx->model.bog_state : NULL;
}

/* ======================= Helper Functions ======================= */

static void set_error(struct LokiBogState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

/* ======================= Audio Callbacks ======================= */

static void bog_audio_init(void *userdata) {
    struct LokiBogState *state = (struct LokiBogState *)userdata;
    if (state) state->start_time = bog_wall_time();
}

static double bog_audio_time(void *userdata) {
    struct LokiBogState *state = (struct LokiBogState *)userdata;
    if (!state) return 0.0;
    return bog_wall_time() - state->start_time;
}

static void bog_audio_note(void *userdata, const char *voice, double time,
                           double midi, double velocity) {
    (void)time;  /* the scheduler calls this when the event is due */
    struct LokiBogState *state = (struct LokiBogState *)userdata;
    if (!state || !state->shared) return;
    int channel, note;
    bog_voice_lookup(voice, midi, &channel, &note);
    shared_send_note_on(state->shared, channel, note, bog_vel_to_midi(velocity));
}

/* ======================= Lifecycle Functions ======================= */

static int bog_lang_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Check if already initialized */
    if (ctx->model.bog_state && ctx->model.bog_state->initialized) {
        return 0;
    }

    /* Allocate state if needed */
    struct LokiBogState *state = ctx->model.bog_state;
    if (!state) {
        state = (struct LokiBogState *)calloc(1, sizeof(struct LokiBogState));
        if (!state) return -1;
        ctx->model.bog_state = state;
    }

    /* Initialize defaults */
    state->tempo = 120.0;
    state->swing = 0.0;

    /* Create arena */
    state->arena = bog_arena_create();
    if (!state->arena) {
        set_error(state, "Failed to create Bog arena");
        free(state);
        ctx->model.bog_state = NULL;
        return -1;
    }

    /* Create builtins */
    state->builtins = bog_create_builtins(state->arena);
    if (!state->builtins) {
        set_error(state, "Failed to create Bog builtins");
        bog_arena_destroy(state->arena);
        free(state);
        ctx->model.bog_state = NULL;
        return -1;
    }

    /* Create state manager */
    state->state_manager = bog_state_manager_create();
    if (!state->state_manager) {
        set_error(state, "Failed to create Bog state manager");
        bog_arena_destroy(state->arena);
        free(state);
        ctx->model.bog_state = NULL;
        return -1;
    }

    /* Setup audio callbacks */
    BogAudioCallbacks audio = {
        .userdata = state,
        .init = bog_audio_init,
        .time = bog_audio_time,
        .note = bog_audio_note,
    };

    /* Create scheduler */
    state->scheduler = bog_scheduler_create(&audio, state->builtins, state->state_manager);
    if (!state->scheduler) {
        set_error(state, "Failed to create Bog scheduler");
        bog_state_manager_destroy(state->state_manager);
        bog_arena_destroy(state->arena);
        free(state);
        ctx->model.bog_state = NULL;
        return -1;
    }

    /* Configure scheduler. shared_send_note_on plays immediately, so the
     * scheduler holds queried events until they are due. */
    bog_scheduler_configure(state->scheduler, state->tempo, state->swing, 50.0, 0.25);
    bog_scheduler_set_dispatch_on_time(state->scheduler, true);

    /* Create transition manager (quantization of 4 beats = 1 bar) */
    state->transition_manager = bog_transition_manager_create(state->scheduler, 4.0);
    if (!state->transition_manager) {
        set_error(state, "Failed to create Bog transition manager");
        bog_scheduler_destroy(state->scheduler);
        bog_state_manager_destroy(state->state_manager);
        bog_arena_destroy(state->arena);
        free(state);
        ctx->model.bog_state = NULL;
        return -1;
    }

    /* Use editor-owned SharedContext instead of allocating our own.
     * This centralizes audio/MIDI/Link state across all languages. */
    if (ctx->model.shared) {
        state->shared = ctx->model.shared;
    } else {
        set_error(state, "No shared context available");
        bog_transition_manager_destroy(state->transition_manager);
        bog_scheduler_destroy(state->scheduler);
        bog_state_manager_destroy(state->state_manager);
        bog_arena_destroy(state->arena);
        free(state);
        ctx->model.bog_state = NULL;
        return -1;
    }

    state->initialized = 1;
    set_error(state, NULL);

    return 0;
}

static void bog_lang_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    struct LokiBogState *state = get_bog_state(ctx);
    if (!state) return;

    /* Stop scheduler */
    if (state->scheduler && state->running) {
        bog_scheduler_stop(state->scheduler);
        state->running = 0;
    }

    /* Send panic before cleanup */
    if (state->shared) {
        shared_send_panic(state->shared);
    }

    /* SharedContext is NOT cleaned up here - editor owns it.
     * Just clear the pointer to avoid dangling reference. */
    state->shared = NULL;

    /* Cleanup bog resources */
    if (state->transition_manager) {
        bog_transition_manager_destroy(state->transition_manager);
        state->transition_manager = NULL;
    }
    if (state->scheduler) {
        bog_scheduler_destroy(state->scheduler);
        state->scheduler = NULL;
    }
    if (state->state_manager) {
        bog_state_manager_destroy(state->state_manager);
        state->state_manager = NULL;
    }
    if (state->current_program) {
        bog_free_program(state->current_program);
        state->current_program = NULL;
    }
    if (state->arena) {
        bog_arena_destroy(state->arena);
        state->arena = NULL;
    }

    state->initialized = 0;

    /* Free the state structure */
    free(state);
    ctx->model.bog_state = NULL;
}

static int bog_lang_is_initialized(editor_ctx_t *ctx) {
    struct LokiBogState *state = get_bog_state(ctx);
    return state ? state->initialized : 0;
}

/* ======================= Evaluation Functions ======================= */

static int bog_lang_eval(editor_ctx_t *ctx, const char *code) {
    struct LokiBogState *state = get_bog_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_error(state, "Bog not initialized");
        return -1;
    }

    if (!code || !*code) {
        return 0;  /* Empty code is success */
    }

    /* Parse the program */
    char *error_msg = NULL;
    BogArena *prog_arena = bog_arena_create();
    if (!prog_arena) {
        set_error(state, "Failed to create arena for program");
        return -1;
    }

    BogProgram *program = bog_parse_program(code, prog_arena, &error_msg);
    if (!program) {
        if (error_msg) {
            set_error(state, error_msg);
            free(error_msg);
        } else {
            set_error(state, "Parse error");
        }
        bog_arena_destroy(prog_arena);
        return -1;
    }

    /* Schedule transition to new program */
    bog_transition_manager_schedule(state->transition_manager, program);

    /* Start scheduler if not running */
    if (!state->running) {
        bog_scheduler_start(state->scheduler);
        state->running = 1;
    }

    /* Free old program if any (the transition manager takes ownership) */
    if (state->current_program) {
        /* Note: We can't free the old program arena here because
         * the transition manager might still be using it. The
         * transition manager should handle cleanup. For now, we
         * just replace the pointer. */
    }
    state->current_program = program;

    set_error(state, NULL);
    return 0;
}

static void bog_lang_stop(editor_ctx_t *ctx) {
    struct LokiBogState *state = get_bog_state(ctx);
    if (!state) return;

    /* Stop scheduler */
    if (state->scheduler && state->running) {
        bog_scheduler_stop(state->scheduler);
        state->running = 0;
    }

    /* Reset state manager for fresh start */
    if (state->state_manager) {
        bog_state_manager_reset(state->state_manager);
    }

    /* Send panic to stop all notes */
    if (state->shared) {
        shared_send_panic(state->shared);
    }
}

static int bog_lang_is_playing(editor_ctx_t *ctx) {
    struct LokiBogState *state = get_bog_state(ctx);
    return state ? state->running : 0;
}

static int bog_lang_get_source_line(editor_ctx_t *ctx) {
    struct LokiBogState *state = get_bog_state(ctx);
    if (!state || !state->scheduler || !state->running) return 0;
    return bog_scheduler_get_current_source_line(state->scheduler);
}

static const char *bog_lang_get_error(editor_ctx_t *ctx) {
    struct LokiBogState *state = get_bog_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}

/* ======================= Main Loop Callback ======================= */

static void bog_lang_check_callbacks(editor_ctx_t *ctx, lua_State *L) {
    (void)L;
    struct LokiBogState *state = get_bog_state(ctx);
    if (!state || !state->scheduler || !state->running) return;

    /* Tick the scheduler */
    bog_scheduler_tick(state->scheduler);

    /* Process any pending transitions */
    if (state->transition_manager) {
        double now = bog_scheduler_now(state->scheduler);
        bog_transition_manager_process(state->transition_manager, now);
    }
}

/* ======================= Lua API Bindings ======================= */

/* loki.bog.init() - Initialize Bog interpreter */
static int lua_bog_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);

    int result = bog_lang_init(ctx);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = bog_lang_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to initialize Bog");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* loki.bog.eval(code) - Evaluate Bog code */
static int lua_bog_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);

    int result = bog_lang_eval(ctx, code);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = bog_lang_get_error(ctx);
        lua_pushstring(L, err ? err : "Evaluation failed");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* loki.bog.stop() - Stop Bog playback */
static int lua_bog_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    bog_lang_stop(ctx);
    return 0;
}

/* loki.bog.is_playing() - Check if Bog is playing */
static int lua_bog_is_playing(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, bog_lang_is_playing(ctx));
    return 1;
}

/* loki.bog.is_initialized() - Check if Bog is initialized */
static int lua_bog_is_initialized(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, bog_lang_is_initialized(ctx));
    return 1;
}

/* loki.bog.set_tempo(bpm) - Set tempo */
static int lua_bog_set_tempo(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    double bpm = luaL_checknumber(L, 1);

    struct LokiBogState *state = get_bog_state(ctx);
    if (!state || !state->initialized) {
        lua_pushnil(L);
        lua_pushstring(L, "Bog not initialized");
        return 2;
    }

    if (bpm < 20.0) bpm = 20.0;
    if (bpm > 400.0) bpm = 400.0;

    state->tempo = bpm;
    if (state->scheduler) {
        bog_scheduler_configure(state->scheduler, bpm, state->swing, 50.0, 0.25);
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* loki.bog.set_swing(amount) - Set swing amount (0.0 - 1.0) */
static int lua_bog_set_swing(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    double swing = luaL_checknumber(L, 1);

    struct LokiBogState *state = get_bog_state(ctx);
    if (!state || !state->initialized) {
        lua_pushnil(L);
        lua_pushstring(L, "Bog not initialized");
        return 2;
    }

    if (swing < 0.0) swing = 0.0;
    if (swing > 1.0) swing = 1.0;

    state->swing = swing;
    if (state->scheduler) {
        bog_scheduler_configure(state->scheduler, state->tempo, swing, 50.0, 0.25);
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Register Bog Lua API as loki.bog subtable */
static void bog_register_lua_api(lua_State *L) {
    if (!loki_lua_begin_api(L, "bog")) return;

    loki_lua_add_func(L, "init", lua_bog_init);
    loki_lua_add_func(L, "eval", lua_bog_eval);
    loki_lua_add_func(L, "stop", lua_bog_stop);
    loki_lua_add_func(L, "is_playing", lua_bog_is_playing);
    loki_lua_add_func(L, "is_initialized", lua_bog_is_initialized);
    loki_lua_add_func(L, "set_tempo", lua_bog_set_tempo);
    loki_lua_add_func(L, "set_swing", lua_bog_set_swing);

    loki_lua_end_api(L, "bog");
}

/* ======================= Language Registration ======================= */

static const LokiLangOps bog_lang_ops = {
    .name = "bog",
    .extensions = {".bog", NULL},

    /* Lifecycle */
    .init = bog_lang_init,
    .cleanup = bog_lang_cleanup,
    .is_initialized = bog_lang_is_initialized,

    /* Main loop - tick scheduler */
    .check_callbacks = bog_lang_check_callbacks,

    /* Playback */
    .eval = bog_lang_eval,
    .stop = bog_lang_stop,
    .is_playing = bog_lang_is_playing,
    .get_source_line = bog_lang_get_source_line,

    /* Export (not supported yet) */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = bog_lang_get_error,

    /* Backend (not supported yet) */
    .configure_backend = NULL,

    /* Lua API */
    .register_lua_api = bog_register_lua_api,
};

/* Register Bog with the language bridge - called from loki_lang_init() */
void bog_loki_lang_init(void) {
    loki_lang_register(&bog_lang_ops);
}
