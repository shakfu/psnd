/* tr7.c - TR7 Scheme language integration for Loki
 *
 * Integrates the TR7 R7RS-small Scheme interpreter with Loki editor.
 * TR7 is a tiny Scheme interpreter supporting the R7RS-small standard.
 *
 * Music primitives provide Alda/Joy-like music functionality:
 *   (play-note pitch velocity duration-ms)
 *   (play-chord '(60 64 67) velocity duration-ms)
 *   (note-on pitch velocity)
 *   (note-off pitch)
 *   (set-tempo! bpm)
 *   (set-octave! n)
 *   (set-velocity! v)
 *   (set-channel! ch)
 *   (midi-list)
 *   (midi-open port)
 *   (midi-virtual name)
 *   (midi-panic)
 *   (tsf-load path)
 *   (sleep-ms ms)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#define usleep(us) Sleep((us) / 1000)
#else
#include <unistd.h>
#endif

#include "psnd.h"
#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"    /* For loki_lua_get_editor_context */
#include "lauxlib.h"     /* For luaL_checkstring, etc. */

/* TR7 library header */
#include "tr7.h"

/* MSVC workaround: TR7ARG macros use preprocessor tricks that don't work on MSVC.
 * Provide explicit string definitions instead. Values from tr7.h _TR7ARGNUM_* */
#ifdef _MSC_VER
#undef TR7ARG_ANY
#undef TR7ARG_STRING
#undef TR7ARG_INTEGER
#undef TR7ARG_PROPER_LIST
#define TR7ARG_ANY         "\003"
#define TR7ARG_STRING      "\004"
#define TR7ARG_INTEGER     "\017"
#define TR7ARG_PROPER_LIST "\027"
#endif

/* Shared backend for MIDI/audio */
#include "context.h"
#include "midi/midi.h"
#include "audio/audio.h"

/* ======================= Music Context ======================= */

/* Maximum chord notes */
#define TR7_MAX_CHORD_NOTES 16

/* Music state for TR7 */
typedef struct {
    int octave;           /* Current octave (0-9, default 4) */
    int velocity;         /* Current velocity (0-127, default 80) */
    int tempo;            /* BPM (default 120) */
    int channel;          /* MIDI channel (1-16, default 1) */
    int duration_ms;      /* Default duration in ms (default 500) */
} Tr7MusicContext;

/* Initialize music context with defaults */
static void music_context_init(Tr7MusicContext *mctx) {
    mctx->octave = 4;
    mctx->velocity = 80;
    mctx->tempo = 120;
    mctx->channel = 1;
    mctx->duration_ms = 500;
}

/* ======================= Internal State ======================= */

/* Error buffer size */
#define TR7_ERROR_BUFSIZE 512

/* Per-context TR7 state */
struct LokiTr7State {
    int initialized;
    tr7_engine_t engine;                  /* TR7 interpreter engine */
    char last_error[TR7_ERROR_BUFSIZE];   /* Last error message */
    SharedContext *shared;                /* Shared MIDI/audio context */
    Tr7MusicContext music;                /* Music state */
};

/* Global pointer to current TR7 state (for C callbacks) */
static struct LokiTr7State *g_tr7_state = NULL;

/* Get TR7 state from editor context */
static struct LokiTr7State* get_tr7_state(editor_ctx_t *ctx) {
    return ctx ? ctx->model.tr7_state : NULL;
}

/* ======================= Helper Functions ======================= */

static void set_error(struct LokiTr7State *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

/* Convert duration value to ms based on tempo */
static int duration_to_ms(int tempo) {
    /* Quarter note = 60000 / tempo ms */
    return 60000 / tempo;
}

/* Cross-platform sleep in milliseconds */
static void sleep_ms(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* ======================= Scheme Music Primitives ======================= */

/* (play-note pitch velocity duration-ms) - Play a single note */
static tr7_C_return_t scm_play_note(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    int velocity = (nvalues > 1) ? TR7_TO_INT(values[1]) : state->music.velocity;
    int duration = (nvalues > 2) ? TR7_TO_INT(values[2]) : state->music.duration_ms;

    /* Clamp values */
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    /* Send note on */
    shared_send_note_on(state->shared, state->music.channel, pitch, velocity);

    /* Wait for duration */
    if (duration > 0) {
        sleep_ms(duration);
    }

    /* Send note off */
    shared_send_note_off(state->shared, state->music.channel, pitch);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (note-on pitch [velocity]) - Send note-on message */
static tr7_C_return_t scm_note_on(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    int velocity = (nvalues > 1) ? TR7_TO_INT(values[1]) : state->music.velocity;

    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    shared_send_note_on(state->shared, state->music.channel, pitch, velocity);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (note-off pitch) - Send note-off message */
static tr7_C_return_t scm_note_off(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;

    shared_send_note_off(state->shared, state->music.channel, pitch);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (play-chord pitches velocity duration-ms) - Play a chord (list of pitches) */
static tr7_C_return_t scm_play_chord(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    tr7_t list = values[0];
    int velocity = (nvalues > 1) ? TR7_TO_INT(values[1]) : state->music.velocity;
    int duration = (nvalues > 2) ? TR7_TO_INT(values[2]) : state->music.duration_ms;

    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    /* Collect pitches from list */
    int pitches[TR7_MAX_CHORD_NOTES];
    int count = 0;

    while (TR7_IS_PAIR(list) && count < TR7_MAX_CHORD_NOTES) {
        tr7_t car = TR7_CAR(list);
        if (TR7_IS_INT(car)) {
            int pitch = TR7_TO_INT(car);
            if (pitch >= 0 && pitch <= 127) {
                pitches[count++] = pitch;
            }
        }
        list = TR7_CDR(list);
    }

    /* Send note-on for all pitches */
    for (int i = 0; i < count; i++) {
        shared_send_note_on(state->shared, state->music.channel, pitches[i], velocity);
    }

    /* Wait for duration */
    if (duration > 0) {
        sleep_ms(duration);
    }

    /* Send note-off for all pitches */
    for (int i = 0; i < count; i++) {
        shared_send_note_off(state->shared, state->music.channel, pitches[i]);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (set-tempo! bpm) - Set tempo in BPM */
static tr7_C_return_t scm_set_tempo(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_raise_error(tsc, "TR7 not initialized", TR7_NIL, 0);
    }

    int tempo = TR7_TO_INT(values[0]);
    if (tempo < 20) tempo = 20;
    if (tempo > 300) tempo = 300;

    state->music.tempo = tempo;
    state->music.duration_ms = duration_to_ms(tempo);

    if (state->shared) {
        state->shared->tempo = tempo;
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (set-octave! n) - Set current octave (0-9) */
static tr7_C_return_t scm_set_octave(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_raise_error(tsc, "TR7 not initialized", TR7_NIL, 0);
    }

    int octave = TR7_TO_INT(values[0]);
    if (octave < 0) octave = 0;
    if (octave > 9) octave = 9;

    state->music.octave = octave;

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (set-velocity! v) - Set default velocity (0-127) */
static tr7_C_return_t scm_set_velocity(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_raise_error(tsc, "TR7 not initialized", TR7_NIL, 0);
    }

    int velocity = TR7_TO_INT(values[0]);
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    state->music.velocity = velocity;

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (set-channel! ch) - Set MIDI channel (1-16) */
static tr7_C_return_t scm_set_channel(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_raise_error(tsc, "TR7 not initialized", TR7_NIL, 0);
    }

    int channel = TR7_TO_INT(values[0]);
    if (channel < 1) channel = 1;
    if (channel > 16) channel = 16;

    state->music.channel = channel;

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (tempo) - Get current tempo */
static tr7_C_return_t scm_get_tempo(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_return_single(tsc, TR7_FROM_INT(120));
    }
    return tr7_C_return_single(tsc, TR7_FROM_INT(state->music.tempo));
}

/* (octave) - Get current octave */
static tr7_C_return_t scm_get_octave(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_return_single(tsc, TR7_FROM_INT(4));
    }
    return tr7_C_return_single(tsc, TR7_FROM_INT(state->music.octave));
}

/* (velocity) - Get current velocity */
static tr7_C_return_t scm_get_velocity(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_return_single(tsc, TR7_FROM_INT(80));
    }
    return tr7_C_return_single(tsc, TR7_FROM_INT(state->music.velocity));
}

/* (channel) - Get current channel */
static tr7_C_return_t scm_get_channel(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    struct LokiTr7State *state = g_tr7_state;
    if (!state) {
        return tr7_C_return_single(tsc, TR7_FROM_INT(1));
    }
    return tr7_C_return_single(tsc, TR7_FROM_INT(state->music.channel));
}

/* (midi-list) - List available MIDI ports */
static tr7_C_return_t scm_midi_list(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    shared_midi_list_ports(state->shared);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (midi-open port-index) - Open a MIDI port by index */
static tr7_C_return_t scm_midi_open(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int port = TR7_TO_INT(values[0]);
    int result = shared_midi_open_port(state->shared, port);

    if (result == 0) {
        const char *name = shared_midi_get_port_name(state->shared, port);
        printf("TR7: Opened MIDI port %d: %s\n", port, name ? name : "(unknown)");
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

/* (midi-virtual name) - Create a virtual MIDI port */
static tr7_C_return_t scm_midi_virtual(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    const char *name = PSND_MIDI_PORT_NAME;
    if (TR7_IS_STRING(values[0])) {
        name = tr7_string_buffer(values[0]);
    }

    int result = shared_midi_open_virtual(state->shared, name);
    if (result == 0) {
        printf("TR7: Created virtual MIDI port '%s'\n", name);
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

/* (midi-panic) - Send all notes off */
static tr7_C_return_t scm_midi_panic(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_return_single(tsc, TR7_VOID);
    }

    shared_send_panic(state->shared);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (tsf-load path) - Load a SoundFont for TinySoundFont playback */
static tr7_C_return_t scm_tsf_load(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    if (!TR7_IS_STRING(values[0])) {
        return tr7_C_raise_error(tsc, "tsf-load: expected string path", TR7_NIL, 0);
    }

    const char *path = tr7_string_buffer(values[0]);
    int result = shared_tsf_init() != 0 ? -1 : shared_tsf_load_soundfont(path);

    if (result == 0) {
        printf("TR7: Loaded SoundFont: %s\n", path);
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

/* (sleep-ms ms) - Sleep for milliseconds */
static tr7_C_return_t scm_sleep_ms(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;

    int ms = TR7_TO_INT(values[0]);
    if (ms > 0) {
        sleep_ms(ms);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (program-change program) - Send program change */
static tr7_C_return_t scm_program_change(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int program = TR7_TO_INT(values[0]);
    if (program < 0) program = 0;
    if (program > 127) program = 127;

    shared_send_program(state->shared, state->music.channel, program);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* (control-change cc value) - Send control change */
static tr7_C_return_t scm_control_change(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;
    if (!state || !state->shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int cc = TR7_TO_INT(values[0]);
    int value = TR7_TO_INT(values[1]);

    if (cc < 0) cc = 0;
    if (cc > 127) cc = 127;
    if (value < 0) value = 0;
    if (value > 127) value = 127;

    shared_send_cc(state->shared, state->music.channel, cc, value);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* Note name to MIDI pitch helper */
static int note_to_pitch(const char *name, int octave) {
    /* Base pitches: c=0, d=2, e=4, f=5, g=7, a=9, b=11 */
    static const int base[] = {9, 11, 0, 2, 4, 5, 7};  /* a, b, c, d, e, f, g */

    if (!name || !*name) return -1;

    char note = name[0];
    if (note >= 'A' && note <= 'G') note = note - 'A' + 'a';
    if (note < 'a' || note > 'g') return -1;

    int pitch = base[note - 'a'];
    int i = 1;

    /* Handle accidentals */
    while (name[i]) {
        if (name[i] == '#' || name[i] == '+') pitch++;
        else if (name[i] == 'b' || name[i] == '-') pitch--;
        else break;
        i++;
    }

    /* Handle octave override in name */
    if (name[i] >= '0' && name[i] <= '9') {
        octave = name[i] - '0';
    }

    return (octave + 1) * 12 + pitch;
}

/* (note name) - Convert note name to MIDI pitch, e.g., (note "c4") -> 60 */
static tr7_C_return_t scm_note(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    struct LokiTr7State *state = g_tr7_state;

    int octave = state ? state->music.octave : 4;

    if (TR7_IS_STRING(values[0])) {
        const char *name = tr7_string_buffer(values[0]);
        int pitch = note_to_pitch(name, octave);
        if (pitch >= 0 && pitch <= 127) {
            return tr7_C_return_single(tsc, TR7_FROM_INT(pitch));
        }
    } else if (TR7_IS_SYMBOL(values[0])) {
        /* Get symbol name */
        const char *name = tr7_symbol_string(values[0]);
        int pitch = note_to_pitch(name, octave);
        if (pitch >= 0 && pitch <= 127) {
            return tr7_C_return_single(tsc, TR7_FROM_INT(pitch));
        }
    }

    return tr7_C_raise_error(tsc, "note: invalid note name", values[0], 0);
}

/* ======================= Function Registration ======================= */

/* Music primitive definitions - must have static lifetime */
static const tr7_C_func_def_t tr7_music_funcs[] = {
    /* Note playing */
    {"play-note", scm_play_note, NULL, TR7ARG_INTEGER, 1, 3},
    {"note-on", scm_note_on, NULL, TR7ARG_INTEGER, 1, 2},
    {"note-off", scm_note_off, NULL, TR7ARG_INTEGER, 1, 1},
    {"play-chord", scm_play_chord, NULL, TR7ARG_PROPER_LIST, 1, 3},

    /* State setters */
    {"set-tempo!", scm_set_tempo, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-octave!", scm_set_octave, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-velocity!", scm_set_velocity, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-channel!", scm_set_channel, NULL, TR7ARG_INTEGER, 1, 1},

    /* State getters */
    {"tempo", scm_get_tempo, NULL, NULL, 0, 0},
    {"octave", scm_get_octave, NULL, NULL, 0, 0},
    {"velocity", scm_get_velocity, NULL, NULL, 0, 0},
    {"channel", scm_get_channel, NULL, NULL, 0, 0},

    /* MIDI control */
    {"midi-list", scm_midi_list, NULL, NULL, 0, 0},
    {"midi-open", scm_midi_open, NULL, TR7ARG_INTEGER, 1, 1},
    {"midi-virtual", scm_midi_virtual, NULL, TR7ARG_STRING, 1, 1},
    {"midi-panic", scm_midi_panic, NULL, NULL, 0, 0},
    {"program-change", scm_program_change, NULL, TR7ARG_INTEGER, 1, 1},
    {"control-change", scm_control_change, NULL, TR7ARG_INTEGER TR7ARG_INTEGER, 2, 2},

    /* Audio backend */
    {"tsf-load", scm_tsf_load, NULL, TR7ARG_STRING, 1, 1},

    /* Utilities */
    {"sleep-ms", scm_sleep_ms, NULL, TR7ARG_INTEGER, 1, 1},
    {"note", scm_note, NULL, TR7ARG_ANY, 1, 1},

    /* Sentinel */
    {NULL, NULL, NULL, NULL, 0, 0}
};

/* Register music primitives with TR7 engine */
static void register_music_primitives(tr7_engine_t engine) {
    tr7_register_C_func_list(engine, tr7_music_funcs);
}

/* ======================= Lifecycle Functions ======================= */

static int tr7_lang_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Check if already initialized */
    if (ctx->model.tr7_state && ctx->model.tr7_state->initialized) {
        return 0;  /* Already initialized is success */
    }

    /* Allocate state if needed */
    struct LokiTr7State *state = ctx->model.tr7_state;
    if (!state) {
        state = (struct LokiTr7State *)calloc(1, sizeof(struct LokiTr7State));
        if (!state) return -1;
        ctx->model.tr7_state = state;
    }

    /* Create TR7 engine */
    state->engine = tr7_engine_create(NULL);
    if (!state->engine) {
        set_error(state, "Failed to create TR7 engine");
        free(state);
        ctx->model.tr7_state = NULL;
        return -1;
    }

    /* Set TR7 search paths to config dir/lib/scm in current working directory */
    /* Note: tr7_set_string stores pointer, doesn't copy - use static storage */
    static char tr7_lib_path[1100];
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(tr7_lib_path, sizeof(tr7_lib_path), "%s/" PSND_CONFIG_DIR "/lib/scm", cwd);
        tr7_set_string(state->engine, Tr7_StrID_Path, tr7_lib_path);
        tr7_set_string(state->engine, Tr7_StrID_Library_Path, tr7_lib_path);
        tr7_set_string(state->engine, Tr7_StrID_Include_Path, tr7_lib_path);
    }

    /* Initialize music context */
    music_context_init(&state->music);

    /* Use editor-owned SharedContext instead of allocating our own.
     * This centralizes audio/MIDI/Link state across all languages. */
    if (ctx->model.shared) {
        state->shared = ctx->model.shared;
    } else {
        set_error(state, "No shared context available");
        tr7_engine_destroy(state->engine);
        free(state);
        ctx->model.tr7_state = NULL;
        return -1;
    }

    /* Set global state for C callbacks */
    g_tr7_state = state;

    /* Load standard scheme libraries */
    tr7_load_string(state->engine,
        "(import (scheme base)"
        "(scheme read)"
        "(scheme write)"
        "(scheme eval))");

    /* Set standard I/O ports */
    tr7_set_standard_ports(state->engine);

    /* Register music primitives */
    register_music_primitives(state->engine);

    state->initialized = 1;
    set_error(state, NULL);

    return 0;
}

static void tr7_lang_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    struct LokiTr7State *state = get_tr7_state(ctx);
    if (!state) return;

    /* Clear global pointer if it's us */
    if (g_tr7_state == state) {
        g_tr7_state = NULL;
    }

    /* Send panic before cleanup */
    if (state->shared) {
        shared_send_panic(state->shared);
    }

    /* SharedContext is NOT cleaned up here - editor owns it.
     * Just clear the pointer to avoid dangling reference. */
    state->shared = NULL;

    /* Destroy TR7 engine */
    if (state->engine) {
        tr7_engine_destroy(state->engine);
        state->engine = NULL;
    }

    state->initialized = 0;

    /* Free the state structure */
    free(state);
    ctx->model.tr7_state = NULL;
}

static int tr7_lang_is_initialized(editor_ctx_t *ctx) {
    struct LokiTr7State *state = get_tr7_state(ctx);
    return state ? state->initialized : 0;
}

/* ======================= Evaluation Functions ======================= */

static int tr7_lang_eval(editor_ctx_t *ctx, const char *code) {
    struct LokiTr7State *state = get_tr7_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_error(state, "TR7 not initialized");
        return -1;
    }

    if (!code || !*code) {
        return 0;  /* Empty code is success */
    }

    /* Ensure global state is set for callbacks */
    g_tr7_state = state;

    /* Use tr7_run_string which returns 0 on error, 1 on success */
    int result = tr7_run_string(state->engine, code);

    if (result == 0) {
        /* Error occurred - get the error from last value */
        tr7_t err_val = tr7_get_last_value(state->engine);
        if (tr7_is_error(err_val)) {
            tr7_t msg = tr7_error_message(err_val);
            if (TR7_IS_STRING(msg)) {
                const char *err_str = tr7_string_buffer(msg);
                if (err_str) {
                    set_error(state, err_str);
                } else {
                    set_error(state, "Evaluation error");
                }
            } else {
                set_error(state, "Evaluation error");
            }
        } else {
            set_error(state, "Unknown error during evaluation");
        }
        return -1;
    }

    set_error(state, NULL);
    return 0;
}

static void tr7_lang_stop(editor_ctx_t *ctx) {
    /* Send panic to stop any playing notes */
    struct LokiTr7State *state = get_tr7_state(ctx);
    if (state && state->shared) {
        shared_send_panic(state->shared);
    }
}

static const char *tr7_lang_get_error(editor_ctx_t *ctx) {
    struct LokiTr7State *state = get_tr7_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}

/* ======================= Lua API Bindings ======================= */

/* loki.tr7.init() - Initialize TR7 interpreter */
static int lua_tr7_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);

    int result = tr7_lang_init(ctx);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = tr7_lang_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to initialize TR7");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* loki.tr7.eval(code) - Evaluate Scheme code */
static int lua_tr7_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);

    int result = tr7_lang_eval(ctx, code);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = tr7_lang_get_error(ctx);
        lua_pushstring(L, err ? err : "Evaluation failed");
        return 2;
    }

    /* Return the result value if available */
    struct LokiTr7State *state = get_tr7_state(ctx);
    if (state && state->engine) {
        tr7_t val = tr7_get_last_value(state->engine);
        if (!TR7_IS_VOID(val)) {
            /* Convert simple types to Lua */
            if (TR7_IS_INT(val)) {
                lua_pushinteger(L, TR7_TO_INT(val));
                return 1;
            } else if (TR7_IS_BOOLEAN(val)) {
                lua_pushboolean(L, TR7_IS_TRUE(val));
                return 1;
            }
            /* For other types, just return true */
        }
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* loki.tr7.stop() - Stop TR7 playback */
static int lua_tr7_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    tr7_lang_stop(ctx);
    return 0;
}

/* loki.tr7.is_initialized() - Check if TR7 is initialized */
static int lua_tr7_is_initialized(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, tr7_lang_is_initialized(ctx));
    return 1;
}

/* Register TR7 Lua API as loki.tr7 subtable */
static void tr7_register_lua_api(lua_State *L) {
    if (!loki_lua_begin_api(L, "tr7")) return;

    loki_lua_add_func(L, "init", lua_tr7_init);
    loki_lua_add_func(L, "eval", lua_tr7_eval);
    loki_lua_add_func(L, "stop", lua_tr7_stop);
    loki_lua_add_func(L, "is_initialized", lua_tr7_is_initialized);

    loki_lua_end_api(L, "tr7");
}

/* ======================= Language Registration ======================= */

static const LokiLangOps tr7_lang_ops = {
    .name = "tr7",
    .extensions = {".scm", ".ss", ".scheme", NULL},

    /* Lifecycle */
    .init = tr7_lang_init,
    .cleanup = tr7_lang_cleanup,
    .is_initialized = tr7_lang_is_initialized,

    /* Main loop (not needed - synchronous) */
    .check_callbacks = NULL,

    /* Playback */
    .eval = tr7_lang_eval,
    .stop = tr7_lang_stop,
    .is_playing = NULL,

    /* Export (not supported yet) */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = tr7_lang_get_error,

    /* Backend (not supported yet) */
    .configure_backend = NULL,

    /* Lua API */
    .register_lua_api = tr7_register_lua_api,
};

/* Register TR7 with the language bridge - called from loki_lang_init() */
void tr7_loki_lang_init(void) {
    loki_lang_register(&tr7_lang_ops);
}
