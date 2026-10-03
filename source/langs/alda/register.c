/* loki_alda.c - Alda music language integration for Loki
 *
 * Integrates the Alda music notation language with Loki editor for livecoding.
 * Uses the unified async event queue for completion callbacks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
/* Windows mutex compatibility using Critical Sections */
typedef CRITICAL_SECTION pthread_mutex_t;
#define pthread_mutex_init(m, attr) (InitializeCriticalSection(m), 0)
#define pthread_mutex_destroy(m) DeleteCriticalSection(m)
#define pthread_mutex_lock(m) EnterCriticalSection(m)
#define pthread_mutex_unlock(m) LeaveCriticalSection(m)
#else
#include <pthread.h>
#endif

#include "psnd.h"
#include "register.h"
#include "loki/internal.h"
#include "loki/link.h"
#include "loki/lang_bridge.h"
#include "loki/async_queue.h"  /* For async event queue */

/* Alda library headers */
#include <alda/alda.h>
#include <alda/csound_backend.h>

/* Shared audio backend (for shared_csound_is_available) */
#include "shared/audio/audio.h"

/* Shared context definition (for SharedContext members) */
#include "context.h"

/* Shared MIDI events buffer (for export) */
#include "shared/midi/events.h"

/* Lua headers for callbacks and bindings */
#include "lua.h"
#include "lauxlib.h"
#include "loki/lua.h"  /* For loki_lua_get_editor_context */
#include "alda/scala.h"  /* For Scala microtuning */
#include <math.h>  /* For pow() in scala bindings */

/* ======================= Internal State ======================= */

/* Playback slot for tracking async operations */
typedef struct {
    int active;                  /* Slot in use */
    int playing;                 /* Currently playing */
    int completed;               /* Playback finished, callback pending */
    LokiAldaStatus status;       /* Final status */
    char *lua_callback;          /* Callback function name (owned) */
    char *error_msg;             /* Error message if any (owned) */
    int events_played;           /* Number of events played */
    int duration_ms;             /* Playback duration */
    time_t start_time;           /* Start timestamp */
} AldaPlaybackSlot;

/* Per-context alda state */
struct LokiAldaState {
    int initialized;
    AldaContext alda_ctx;        /* The alda interpreter context */
    AldaPlaybackSlot slots[LOKI_ALDA_MAX_SLOTS];
    char last_error[LOKI_ALDA_ERROR_BUFSIZE];  /* Last error message */
    pthread_mutex_t mutex;       /* Protect concurrent access */
};

/* Get alda state from editor context, returning NULL if not initialized */
static LokiAldaState* get_alda_state(editor_ctx_t *ctx) {
    return ctx ? ctx->model.alda_state : NULL;
}

/* Global scale storage for Scala microtuning */
static ScalaScale *g_current_scale = NULL;

/* ======================= Async Completion Callback ======================= */

/* Info passed to async completion callback */
typedef struct {
    LokiAldaState *state;       /* Alda state for slot management */
    int alda_slot_id;           /* Alda slot index */
    int events_played;          /* Number of events played */
    time_t start_time;          /* When playback started */
    char lua_callback[ASYNC_CALLBACK_NAME_SIZE];  /* Lua callback function name */
} AldaCallbackInfo;

/* Forward declaration */
static void clear_slot(LokiAldaState *state, int slot_id);

/* Completion callback invoked when playback finishes */
static void on_alda_playback_complete(int slot_id, int stopped, void *userdata) {
    AldaCallbackInfo *info = (AldaCallbackInfo *)userdata;
    if (!info) return;

    /* Calculate duration */
    int duration_ms = (int)((time(NULL) - info->start_time) * 1000);

    /* Determine status: 0=complete, 1=stopped, 2=error */
    int status = stopped ? 1 : 0;

    /* Push event to async queue */
    async_queue_push_lang_callback(
        NULL,  /* global queue */
        info->alda_slot_id,
        status,
        info->events_played,
        duration_ms,
        info->lua_callback[0] ? info->lua_callback : NULL,
        NULL   /* no error */
    );

    /* Clear the slot (thread-safe via mutex) */
    if (info->state) {
        pthread_mutex_lock(&info->state->mutex);
        clear_slot(info->state, info->alda_slot_id);
        pthread_mutex_unlock(&info->state->mutex);
    }

    /* Free the callback info */
    free(info);

    (void)slot_id;  /* unused - we track our own slot */
}

/* ======================= Helper Functions ======================= */

static void set_state_error(LokiAldaState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

static int find_free_slot(LokiAldaState *state) {
    if (!state) return -1;
    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        if (!state->slots[i].active) {
            return i;
        }
    }
    return -1;
}

static void clear_slot(LokiAldaState *state, int slot_id) {
    if (!state) return;
    if (slot_id < 0 || slot_id >= LOKI_ALDA_MAX_SLOTS) return;

    AldaPlaybackSlot *slot = &state->slots[slot_id];
    free(slot->lua_callback);
    free(slot->error_msg);
    memset(slot, 0, sizeof(AldaPlaybackSlot));
}

/* ======================= Initialization ======================= */

int loki_alda_init(editor_ctx_t *ctx, const char *port_name) {
    if (!ctx) return -1;

    /* Check if already initialized for this context */
    if (ctx->model.alda_state && ctx->model.alda_state->initialized) {
        set_state_error(ctx->model.alda_state, "Alda already initialized");
        return -1;
    }

    /* Allocate state if needed */
    LokiAldaState *state = ctx->model.alda_state;
    if (!state) {
        state = (LokiAldaState *)calloc(1, sizeof(LokiAldaState));
        if (!state) return -1;
        ctx->model.alda_state = state;
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&state->mutex, NULL) != 0) {
        set_state_error(state, "Failed to initialize mutex");
        free(state);
        ctx->model.alda_state = NULL;
        return -1;
    }

    /* Initialize alda context */
    alda_context_init(&state->alda_ctx);

    /* Use editor-owned SharedContext instead of allocating our own.
     * This centralizes audio/MIDI/Link state across all languages. */
    if (ctx->model.shared) {
        state->alda_ctx.shared = ctx->model.shared;
    } else {
        set_state_error(state, "No shared context available");
        pthread_mutex_destroy(&state->mutex);
        free(state);
        ctx->model.alda_state = NULL;
        return -1;
    }

    /* Initialize async system (creates libuv thread) */
    if (alda_async_init() != 0) {
        set_state_error(state, "Failed to initialize async playback");
        alda_context_cleanup(&state->alda_ctx);
        pthread_mutex_destroy(&state->mutex);
        free(state);
        ctx->model.alda_state = NULL;
        return -1;
    }

    /* Enable concurrent mode for livecoding */
    alda_async_set_concurrent(1);

    /* Open MIDI output */
    const char *name = port_name ? port_name : PSND_MIDI_PORT_NAME;
    if (alda_midi_open_auto(&state->alda_ctx, name) != 0) {
        set_state_error(state, "Failed to open MIDI output");
        alda_async_cleanup();
        alda_context_cleanup(&state->alda_ctx);
        pthread_mutex_destroy(&state->mutex);
        free(state);
        ctx->model.alda_state = NULL;
        return -1;
    }

    /* Initialize TinySoundFont backend */
    alda_tsf_init();

    /* Clear all slots */
    memset(state->slots, 0, sizeof(state->slots));

    state->initialized = 1;
    set_state_error(state, NULL);

    return 0;
}

void loki_alda_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    /* Stop all playback */
    alda_async_stop();

    /* Clean up TinySoundFont */
    alda_tsf_cleanup();

    /* Clean up async system */
    alda_async_cleanup();

    /* Clean up MIDI */
    alda_midi_cleanup(&state->alda_ctx);

    /* Clean up context */
    alda_context_cleanup(&state->alda_ctx);

    /* Clear all slots */
    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        clear_slot(state, i);
    }

    /* Destroy mutex */
    pthread_mutex_destroy(&state->mutex);

    state->initialized = 0;

    /* Free the state structure */
    free(state);
    ctx->model.alda_state = NULL;
}

int loki_alda_is_initialized(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    return state ? state->initialized : 0;
}

/* ======================= Playback Control ======================= */

int loki_alda_eval_async(editor_ctx_t *ctx, const char *code, const char *lua_callback) {
    LokiAldaState *state = get_alda_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_state_error(state, "Empty code");
        return -2;
    }

    pthread_mutex_lock(&state->mutex);

    /* Find a free slot */
    int slot_id = find_free_slot(state);
    if (slot_id < 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "No free playback slots");
        return -1;
    }

    AldaPlaybackSlot *slot = &state->slots[slot_id];

    /* Reset context for new evaluation (keeps MIDI connection) */
    alda_context_reset(&state->alda_ctx);

    /* Parse and interpret the code */
    if (alda_interpret_string(&state->alda_ctx, code, "<loki>") != 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "Parse error in Alda code");
        return -2;
    }

    /* Check if we have any events to play */
    if (state->alda_ctx.event_count == 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "No events generated");
        return -2;
    }

    /* Sort events for playback */
    alda_events_sort(&state->alda_ctx);

    /* Use Link tempo if enabled */
    double effective_tempo = loki_link_effective_tempo(
        ctx, (double)state->alda_ctx.global_tempo);
    state->alda_ctx.global_tempo = (int)effective_tempo;

    /* Set up slot */
    slot->active = 1;
    slot->playing = 1;
    slot->completed = 0;
    slot->status = LOKI_ALDA_STATUS_PLAYING;
    slot->lua_callback = lua_callback ? strdup(lua_callback) : NULL;
    slot->error_msg = NULL;
    slot->events_played = state->alda_ctx.event_count;
    slot->start_time = time(NULL);

    /* Create callback info for completion notification.
     * Always create it so slots get cleared even without Lua callback. */
    AldaCallbackInfo *cb_info = (AldaCallbackInfo *)malloc(sizeof(AldaCallbackInfo));
    if (cb_info) {
        cb_info->state = state;
        cb_info->alda_slot_id = slot_id;
        cb_info->events_played = state->alda_ctx.event_count;
        cb_info->start_time = slot->start_time;
        if (lua_callback) {
            strncpy(cb_info->lua_callback, lua_callback, ASYNC_CALLBACK_NAME_SIZE - 1);
            cb_info->lua_callback[ASYNC_CALLBACK_NAME_SIZE - 1] = '\0';
        } else {
            cb_info->lua_callback[0] = '\0';
        }
    }

    /* Start async playback with completion callback */
    int result = alda_events_play_async_ex(&state->alda_ctx,
                                            on_alda_playback_complete, cb_info);
    if (result < 0) {
        slot->active = 0;
        slot->playing = 0;
        free(slot->lua_callback);
        slot->lua_callback = NULL;
        free(cb_info);
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "Failed to start playback");
        return -1;
    }

    pthread_mutex_unlock(&state->mutex);
    set_state_error(state, NULL);

    return slot_id;
}

int loki_alda_eval_sync(editor_ctx_t *ctx, const char *code) {
    LokiAldaState *state = get_alda_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_state_error(state, "Empty code");
        return -1;
    }

    pthread_mutex_lock(&state->mutex);

    /* Reset context for new evaluation */
    alda_context_reset(&state->alda_ctx);

    /* Parse and interpret */
    if (alda_interpret_string(&state->alda_ctx, code, "<loki>") != 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "Parse error in Alda code");
        return -1;
    }

    /* Sort events */
    alda_events_sort(&state->alda_ctx);

    /* Use Link tempo if enabled */
    double effective_tempo = loki_link_effective_tempo(
        ctx, (double)state->alda_ctx.global_tempo);
    state->alda_ctx.global_tempo = (int)effective_tempo;

    /* Play */
    int result = alda_events_play(&state->alda_ctx);

    pthread_mutex_unlock(&state->mutex);

    if (result != 0) {
        set_state_error(state, "Playback error");
        return -1;
    }

    set_state_error(state, NULL);
    return 0;
}

void loki_alda_stop(editor_ctx_t *ctx, int slot_id) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    if (slot_id < 0) {
        loki_alda_stop_all(ctx);
        return;
    }

    if (slot_id >= LOKI_ALDA_MAX_SLOTS) return;

    pthread_mutex_lock(&state->mutex);

    AldaPlaybackSlot *slot = &state->slots[slot_id];
    if (slot->active && slot->playing) {
        alda_async_stop();
        slot->playing = 0;
        slot->completed = 1;
        slot->status = LOKI_ALDA_STATUS_STOPPED;
    }

    pthread_mutex_unlock(&state->mutex);
}

void loki_alda_stop_all(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    pthread_mutex_lock(&state->mutex);

    alda_async_stop();
    alda_midi_all_notes_off(&state->alda_ctx);

    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        AldaPlaybackSlot *slot = &state->slots[i];
        if (slot->active && slot->playing) {
            slot->playing = 0;
            slot->completed = 1;
            slot->status = LOKI_ALDA_STATUS_STOPPED;
        }
    }

    pthread_mutex_unlock(&state->mutex);
}

/* ======================= Status Queries ======================= */

LokiAldaStatus loki_alda_get_status(editor_ctx_t *ctx, int slot_id) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return LOKI_ALDA_STATUS_IDLE;
    if (slot_id < 0 || slot_id >= LOKI_ALDA_MAX_SLOTS) return LOKI_ALDA_STATUS_IDLE;

    return state->slots[slot_id].status;
}

int loki_alda_is_playing(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return 0;
    return alda_async_is_playing();
}

int loki_alda_active_count(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return 0;
    return alda_async_active_count();
}

/* ======================= Configuration ======================= */

void loki_alda_set_tempo(editor_ctx_t *ctx, int bpm) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    if (bpm < LOKI_ALDA_TEMPO_MIN) bpm = LOKI_ALDA_TEMPO_MIN;
    if (bpm > LOKI_ALDA_TEMPO_MAX) bpm = LOKI_ALDA_TEMPO_MAX;

    state->alda_ctx.global_tempo = bpm;
}

int loki_alda_get_tempo(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return LOKI_ALDA_TEMPO_DEFAULT;
    return state->alda_ctx.global_tempo;
}

/* ======================= MIDI Export Support ======================= */

const AldaScheduledEvent* loki_alda_get_events(editor_ctx_t *ctx, int *count) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (count) *count = 0;
        return NULL;
    }

    if (count) *count = state->alda_ctx.event_count;
    return state->alda_ctx.events;
}

int loki_alda_get_channel_count(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized || state->alda_ctx.event_count == 0) {
        return 0;
    }

    /* Track which channels are used with a bitmask */
    unsigned int channels_used = 0;
    int event_count = state->alda_ctx.event_count;
    const AldaScheduledEvent *events = state->alda_ctx.events;

    for (int i = 0; i < event_count; i++) {
        if (events[i].channel >= 0 && events[i].channel < 16) {
            channels_used |= (1u << events[i].channel);
        }
    }

    /* Count set bits */
    int count = 0;
    while (channels_used) {
        count += channels_used & 1;
        channels_used >>= 1;
    }

    return count;
}

int loki_alda_has_events(editor_ctx_t *ctx) {
    if (!loki_alda_is_initialized(ctx)) {
        return 0;
    }
    int event_count = 0;
    loki_alda_get_events(ctx, &event_count);
    return event_count > 0;
}

int loki_alda_populate_shared_buffer(editor_ctx_t *ctx) {
    if (!loki_alda_has_events(ctx)) {
        return -1;
    }

    int event_count = 0;
    const AldaScheduledEvent *events = loki_alda_get_events(ctx, &event_count);

    if (!events || event_count == 0) {
        return -1;
    }

    /* Initialize shared buffer with Alda's ticks per quarter */
    if (shared_midi_events_init(ALDA_TICKS_PER_QUARTER) != 0) {
        return -1;
    }

    shared_midi_events_clear();

    /* The starting tempo, unless the score sets its own at tick 0: two tempo
     * events at one tick leave the file's tempo to the sort order. */
    int has_initial_tempo = 0;
    for (int i = 0; i < event_count; i++) {
        if (events[i].type == ALDA_EVT_TEMPO && events[i].tick == 0) {
            has_initial_tempo = 1;
        }
    }
    if (!has_initial_tempo) {
        shared_midi_events_tempo(0, loki_alda_get_tempo(ctx));
    }

    /* Convert each Alda event to shared format */
    for (int i = 0; i < event_count; i++) {
        const AldaScheduledEvent *evt = &events[i];

        switch (evt->type) {
            case ALDA_EVT_NOTE_ON:
                shared_midi_events_note_on(evt->tick, evt->channel,
                                           evt->data1, evt->data2);
                break;

            case ALDA_EVT_NOTE_OFF:
                shared_midi_events_note_off(evt->tick, evt->channel, evt->data1);
                break;

            case ALDA_EVT_PROGRAM:
                shared_midi_events_program(evt->tick, evt->channel, evt->data1);
                break;

            case ALDA_EVT_CC:
                shared_midi_events_cc(evt->tick, evt->channel,
                                      evt->data1, evt->data2);
                break;

            case ALDA_EVT_PAN:
                /* Pan is CC #10 */
                shared_midi_events_cc(evt->tick, evt->channel, 10, evt->data1);
                break;

            case ALDA_EVT_TEMPO:
                shared_midi_events_tempo(evt->tick, evt->data1);
                break;
        }
    }

    shared_midi_events_sort();
    return 0;
}

int loki_alda_set_synth_enabled(editor_ctx_t *ctx, int enable) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return -1;

    if (enable) {
        if (!alda_tsf_has_soundfont()) {
            set_state_error(state, "No soundfont loaded");
            return -1;
        }
        alda_tsf_enable();
        state->alda_ctx.builtin_synth_enabled = 1;
        if (state->alda_ctx.shared) {
            state->alda_ctx.shared->builtin_synth_enabled = 1;
        }
    } else {
        alda_tsf_disable();
        state->alda_ctx.builtin_synth_enabled = 0;
        if (state->alda_ctx.shared) {
            state->alda_ctx.shared->builtin_synth_enabled = 0;
        }
    }

    return 0;
}

int loki_alda_load_soundfont(editor_ctx_t *ctx, const char *path) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_state_error(state, "Invalid soundfont path");
        return -1;
    }

    if (alda_tsf_load_soundfont(path) != 0) {
        set_state_error(state, "Failed to load soundfont");
        return -1;
    }

    return 0;
}

/* ======================= Microtuning ======================= */

int loki_alda_set_part_scale(editor_ctx_t *ctx, const char *part_name,
                             struct ScalaScale *scale, int root_note, double root_freq) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        return -1;
    }

    if (!part_name) {
        set_state_error(state, "Part name is required");
        return -1;
    }

    /* Find the part by name */
    AldaPartState *part = alda_find_part(&state->alda_ctx, part_name);
    if (!part) {
        set_state_error(state, "Part not found");
        return -1;
    }

    /* Set the scale and tuning parameters */
    part->scale = scale;
    part->scale_root_note = root_note;
    part->scale_root_freq = root_freq;

    return 0;
}

int loki_alda_clear_part_scale(editor_ctx_t *ctx, const char *part_name) {
    return loki_alda_set_part_scale(ctx, part_name, NULL, 60, 261.6255653);
}

/* ======================= Csound Backend ======================= */

int loki_alda_csound_is_available(void) {
    /* Use shared Csound backend availability check */
    return shared_csound_is_available();
}

int loki_alda_csound_is_enabled(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    return state && state->initialized && state->alda_ctx.csound_enabled;
}

int loki_alda_csound_set_enabled(editor_ctx_t *ctx, int enable) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (enable) {
        if (!alda_csound_has_instruments()) {
            set_state_error(state, "No Csound instruments loaded");
            return -1;
        }
        /* Disable TSF first if enabled */
        if (state->alda_ctx.builtin_synth_enabled) {
            alda_tsf_disable();
            state->alda_ctx.builtin_synth_enabled = 0;
        }
        if (alda_csound_enable() != 0) {
            set_state_error(state, alda_csound_get_error());
            return -1;
        }
        state->alda_ctx.csound_enabled = 1;
    } else {
        alda_csound_disable();
        state->alda_ctx.csound_enabled = 0;
    }

    return 0;
}

int loki_alda_csound_load_csd(editor_ctx_t *ctx, const char *path) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_state_error(state, "Invalid CSD path");
        return -1;
    }

    /* Initialize Csound backend if not already */
    if (alda_csound_init() != 0) {
        set_state_error(state, "Csound backend not available");
        return -1;
    }

    if (alda_csound_load_csd(path) != 0) {
        set_state_error(state, alda_csound_get_error());
        return -1;
    }

    return 0;
}

int loki_alda_csound_play_async(const char *path) {
    if (!path || !*path) {
        fprintf(stderr, "loki_alda: Invalid CSD path\n");
        return -1;
    }

    int result = alda_csound_play_file_async(path);
    if (result != 0) {
        const char *err = alda_csound_get_error();
        fprintf(stderr, "loki_alda: %s\n", err ? err : "Failed to start playback");
        return -1;
    }

    return 0;
}

int loki_alda_csound_playback_active(void) {
    return alda_csound_playback_active();
}

void loki_alda_csound_stop_playback(void) {
    alda_csound_stop_playback();
}

/* ======================= Utility Functions ======================= */

int loki_alda_list_ports(editor_ctx_t *ctx, char **ports, int max_ports) {
    (void)ctx;
    (void)ports;
    (void)max_ports;

    /* TODO: Implement port enumeration */
    /* alda_midi_list_ports only prints to stdout currently */
    return 0;
}

const char *loki_alda_get_error(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}

/* ======================= Lua API Bindings ======================= */

/* Lua API: loki.alda.init(port_name) */
static int lua_alda_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *port_name = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        port_name = luaL_checkstring(L, 1);
    }
    int result = loki_alda_init(ctx, port_name);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to initialize alda");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_cleanup(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_alda_cleanup(ctx);
    return 0;
}

static int lua_alda_is_initialized(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, loki_alda_is_initialized(ctx));
    return 1;
}

static int lua_alda_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);
    const char *callback = NULL;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        callback = luaL_checkstring(L, 2);
    }
    int slot = loki_alda_eval_async(ctx, code, callback);
    if (slot < 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Eval failed");
        return 2;
    }
    lua_pushinteger(L, slot);
    return 1;
}

static int lua_alda_eval_sync(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);
    int result = loki_alda_eval_sync(ctx, code);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Eval failed");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int slot = -1;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        slot = (int)luaL_checkinteger(L, 1);
    }
    loki_alda_stop(ctx, slot);
    return 0;
}

static int lua_alda_stop_all(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_alda_stop_all(ctx);
    return 0;
}

static int lua_alda_is_playing(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, loki_alda_is_playing(ctx));
    return 1;
}

static int lua_alda_active_count(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushinteger(L, loki_alda_active_count(ctx));
    return 1;
}

static int lua_alda_set_tempo(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int bpm = (int)luaL_checkinteger(L, 1);
    loki_alda_set_tempo(ctx, bpm);
    return 0;
}

static int lua_alda_get_tempo(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushinteger(L, loki_alda_get_tempo(ctx));
    return 1;
}

static int lua_alda_set_synth(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int enable = lua_toboolean(L, 1);
    int result = loki_alda_set_synth_enabled(ctx, enable);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to set synth");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_load_soundfont(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *path = luaL_checkstring(L, 1);
    int result = loki_alda_load_soundfont(ctx, path);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to load soundfont");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_get_error(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *err = loki_alda_get_error(ctx);
    if (err) {
        lua_pushstring(L, err);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int lua_alda_csound_available(lua_State *L) {
    lua_pushboolean(L, loki_alda_csound_is_available());
    return 1;
}

static int lua_alda_csound_load(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *path = luaL_checkstring(L, 1);
    int result = loki_alda_csound_load_csd(ctx, path);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to load CSD");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_set_csound(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int enable = lua_toboolean(L, 1);
    int result = loki_alda_csound_set_enabled(ctx, enable);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to set Csound");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_csound_play(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int result = loki_alda_csound_play_async(path);
    if (result != 0) {
        editor_ctx_t *ctx = loki_lua_get_editor_context(L);
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to start playback");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_csound_playing(lua_State *L) {
    lua_pushboolean(L, loki_alda_csound_playback_active());
    return 1;
}

static int lua_alda_csound_stop(lua_State *L) {
    (void)L;
    loki_alda_csound_stop_playback();
    return 0;
}

static int lua_alda_set_backend(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *name = luaL_checkstring(L, 1);
    int result = 0;
    if (strcmp(name, "tsf") == 0) {
        loki_alda_csound_set_enabled(ctx, 0);
        result = loki_alda_set_synth_enabled(ctx, 1);
    } else if (strcmp(name, "csound") == 0) {
        result = loki_alda_csound_set_enabled(ctx, 1);
    } else if (strcmp(name, "midi") == 0) {
        loki_alda_csound_set_enabled(ctx, 0);
        loki_alda_set_synth_enabled(ctx, 0);
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid backend name. Use 'tsf', 'csound', or 'midi'");
        return 2;
    }
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to set backend");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_set_part_scale(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *part_name = luaL_checkstring(L, 1);
    int root_note = 60;
    if (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
        root_note = (int)lua_tointeger(L, 2);
    }
    double root_freq = 261.6255653;
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
        root_freq = lua_tonumber(L, 3);
    }
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, "No scale loaded. Call loki.scala.load() first.");
        return 2;
    }
    int result = loki_alda_set_part_scale(ctx, part_name, g_current_scale, root_note, root_freq);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to set part scale");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_alda_clear_part_scale(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *part_name = luaL_checkstring(L, 1);
    int result = loki_alda_clear_part_scale(ctx, part_name);
    if (result != 0) {
        const char *err = loki_alda_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to clear part scale");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* ======================= Scala Lua Bindings ======================= */

static int lua_scala_load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (g_current_scale) {
        scala_free(g_current_scale);
    }
    g_current_scale = scala_load(path);
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, scala_get_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_scala_load_string(lua_State *L) {
    size_t len;
    const char *content = luaL_checklstring(L, 1, &len);
    if (g_current_scale) {
        scala_free(g_current_scale);
    }
    g_current_scale = scala_load_string(content, len);
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, scala_get_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_scala_unload(lua_State *L) {
    (void)L;
    if (g_current_scale) {
        scala_free(g_current_scale);
        g_current_scale = NULL;
    }
    return 0;
}

static int lua_scala_loaded(lua_State *L) {
    lua_pushboolean(L, g_current_scale != NULL);
    return 1;
}

static int lua_scala_description(lua_State *L) {
    if (g_current_scale) {
        const char *desc = scala_get_description(g_current_scale);
        if (desc) {
            lua_pushstring(L, desc);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int lua_scala_length(lua_State *L) {
    if (g_current_scale) {
        lua_pushinteger(L, scala_get_length(g_current_scale));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int lua_scala_ratio(lua_State *L) {
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, "No scale loaded");
        return 2;
    }
    int degree = (int)luaL_checkinteger(L, 1);
    double ratio = scala_get_ratio(g_current_scale, degree);
    if (ratio < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Degree out of range");
        return 2;
    }
    lua_pushnumber(L, ratio);
    return 1;
}

static int lua_scala_frequency(lua_State *L) {
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, "No scale loaded");
        return 2;
    }
    int degree = (int)luaL_checkinteger(L, 1);
    double base_freq = 261.6255653;  /* Middle C */
    if (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
        base_freq = lua_tonumber(L, 2);
    }
    double freq = scala_get_frequency(g_current_scale, degree, base_freq);
    if (freq < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid degree");
        return 2;
    }
    lua_pushnumber(L, freq);
    return 1;
}

static int lua_scala_midi_to_freq(lua_State *L) {
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, "No scale loaded");
        return 2;
    }
    int midi_note = (int)luaL_checkinteger(L, 1);
    int root_note = 60;
    double root_freq = 261.6255653;
    if (lua_gettop(L) >= 2) root_note = (int)luaL_checkinteger(L, 2);
    if (lua_gettop(L) >= 3) root_freq = luaL_checknumber(L, 3);
    double freq = scala_midi_to_freq(g_current_scale, midi_note, root_note, root_freq);
    lua_pushnumber(L, freq);
    return 1;
}

static int lua_scala_cents_to_ratio(lua_State *L) {
    double cents = luaL_checknumber(L, 1);
    lua_pushnumber(L, scala_cents_to_ratio(cents));
    return 1;
}

static int lua_scala_ratio_to_cents(lua_State *L) {
    double ratio = luaL_checknumber(L, 1);
    lua_pushnumber(L, scala_ratio_to_cents(ratio));
    return 1;
}

static int lua_scala_degrees(lua_State *L) {
    if (!g_current_scale) {
        lua_pushnil(L);
        lua_pushstring(L, "No scale loaded");
        return 2;
    }
    lua_newtable(L);
    for (int i = 0; i < g_current_scale->degree_count; i++) {
        lua_newtable(L);
        lua_pushnumber(L, g_current_scale->degrees[i].ratio);
        lua_setfield(L, -2, "ratio");
        lua_pushnumber(L, g_current_scale->degrees[i].cents);
        lua_setfield(L, -2, "cents");
        if (g_current_scale->degrees[i].cents_format) {
            lua_pushboolean(L, 1);
            lua_setfield(L, -2, "cents_format");
        } else {
            lua_pushinteger(L, g_current_scale->degrees[i].numerator);
            lua_setfield(L, -2, "numerator");
            lua_pushinteger(L, g_current_scale->degrees[i].denominator);
            lua_setfield(L, -2, "denominator");
        }
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/* Register scala module as loki.scala subtable */
static void alda_register_scala_module(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_scala_load);
    lua_setfield(L, -2, "load");
    lua_pushcfunction(L, lua_scala_load_string);
    lua_setfield(L, -2, "load_string");
    lua_pushcfunction(L, lua_scala_unload);
    lua_setfield(L, -2, "unload");
    lua_pushcfunction(L, lua_scala_loaded);
    lua_setfield(L, -2, "loaded");
    lua_pushcfunction(L, lua_scala_description);
    lua_setfield(L, -2, "description");
    lua_pushcfunction(L, lua_scala_length);
    lua_setfield(L, -2, "length");
    lua_pushcfunction(L, lua_scala_ratio);
    lua_setfield(L, -2, "ratio");
    lua_pushcfunction(L, lua_scala_frequency);
    lua_setfield(L, -2, "frequency");
    lua_pushcfunction(L, lua_scala_midi_to_freq);
    lua_setfield(L, -2, "midi_to_freq");
    lua_pushcfunction(L, lua_scala_cents_to_ratio);
    lua_setfield(L, -2, "cents_to_ratio");
    lua_pushcfunction(L, lua_scala_ratio_to_cents);
    lua_setfield(L, -2, "ratio_to_cents");
    lua_pushcfunction(L, lua_scala_degrees);
    lua_setfield(L, -2, "degrees");
    lua_setfield(L, -2, "scala");
}

/* Register alda module as loki.alda subtable */
static void alda_register_lua_api(lua_State *L) {
    if (!loki_lua_begin_api(L, "alda")) return;

    loki_lua_add_func(L, "init", lua_alda_init);
    loki_lua_add_func(L, "cleanup", lua_alda_cleanup);
    loki_lua_add_func(L, "is_initialized", lua_alda_is_initialized);
    loki_lua_add_func(L, "eval", lua_alda_eval);
    loki_lua_add_func(L, "eval_sync", lua_alda_eval_sync);
    loki_lua_add_func(L, "stop", lua_alda_stop);
    loki_lua_add_func(L, "stop_all", lua_alda_stop_all);
    loki_lua_add_func(L, "is_playing", lua_alda_is_playing);
    loki_lua_add_func(L, "active_count", lua_alda_active_count);
    loki_lua_add_func(L, "set_tempo", lua_alda_set_tempo);
    loki_lua_add_func(L, "get_tempo", lua_alda_get_tempo);
    loki_lua_add_func(L, "set_synth", lua_alda_set_synth);
    loki_lua_add_func(L, "load_soundfont", lua_alda_load_soundfont);
    loki_lua_add_func(L, "get_error", lua_alda_get_error);
    loki_lua_add_func(L, "csound_available", lua_alda_csound_available);
    loki_lua_add_func(L, "csound_load", lua_alda_csound_load);
    loki_lua_add_func(L, "set_csound", lua_alda_set_csound);
    loki_lua_add_func(L, "csound_play", lua_alda_csound_play);
    loki_lua_add_func(L, "csound_playing", lua_alda_csound_playing);
    loki_lua_add_func(L, "csound_stop", lua_alda_csound_stop);
    loki_lua_add_func(L, "set_backend", lua_alda_set_backend);
    loki_lua_add_func(L, "set_part_scale", lua_alda_set_part_scale);
    loki_lua_add_func(L, "clear_part_scale", lua_alda_clear_part_scale);

    loki_lua_end_api(L, "alda");

    /* Also register scala module under loki.scala */
    lua_getglobal(L, "loki");
    if (lua_istable(L, -1)) {
        alda_register_scala_module(L);
    }
    lua_pop(L, 1);
}

/* ======================= Language Bridge Registration ======================= */

/* Wrapper for init (bridge interface doesn't take port_name) */
static int alda_bridge_init(editor_ctx_t *ctx) {
    return loki_alda_init(ctx, NULL);
}

/* Wrapper for eval (bridge uses async eval for non-blocking playback) */
static int alda_bridge_eval(editor_ctx_t *ctx, const char *code) {
    int slot = loki_alda_eval_async(ctx, code, NULL);
    return (slot >= 0) ? 0 : slot;  /* Convert slot_id to 0 success, negative for error */
}

/* Wrapper for stop (bridge doesn't take slot_id) */
static void alda_bridge_stop(editor_ctx_t *ctx) {
    loki_alda_stop_all(ctx);
}

/* Wrapper for backend configuration */
static int alda_bridge_configure_backend(editor_ctx_t *ctx, const char *sf_path, const char *csd_path) {
    /* CSD takes precedence over soundfont */
    if (csd_path && *csd_path) {
        if (loki_alda_csound_is_available()) {
            if (loki_alda_csound_load_csd(ctx, csd_path) == 0) {
                if (loki_alda_csound_set_enabled(ctx, 1) == 0) {
                    return 0;  /* Success with Csound */
                }
            }
        }
        return -1;  /* Csound requested but failed */
    }

    if (sf_path && *sf_path) {
        if (loki_alda_load_soundfont(ctx, sf_path) == 0) {
            if (loki_alda_set_synth_enabled(ctx, 1) == 0) {
                return 0;  /* Success with TSF */
            }
        }
        return -1;  /* Soundfont requested but failed */
    }

    return 1;  /* No backend requested */
}

/* Language operations for Alda */
static const LokiLangOps alda_lang_ops = {
    .name = "alda",
    .extensions = {".alda", NULL},

    /* Lifecycle */
    .init = alda_bridge_init,
    .cleanup = loki_alda_cleanup,
    .is_initialized = loki_alda_is_initialized,

    /* Main loop - callbacks handled via async event queue */
    .check_callbacks = NULL,

    /* Playback */
    .eval = alda_bridge_eval,
    .stop = alda_bridge_stop,
    .is_playing = loki_alda_is_playing,

    /* Export */
    .has_events = loki_alda_has_events,
    .populate_shared_buffer = loki_alda_populate_shared_buffer,

    /* Error */
    .get_error = loki_alda_get_error,

    /* Backend configuration */
    .configure_backend = alda_bridge_configure_backend,

    /* Lua API registration */
    .register_lua_api = alda_register_lua_api,
};

/* Register Alda with the language bridge - called from loki_lang_init() */
void alda_loki_lang_init(void) {
    loki_lang_register(&alda_lang_ops);
}
