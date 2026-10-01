/**
 * @file repl.c
 * @brief TR7 Scheme REPL - Interactive R7RS-small Scheme interpreter with music extensions.
 */

#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/lua.h"
#include "loki/repl_launcher.h"
#include "loki/repl_helpers.h"
#include "shared/repl_commands.h"
#include "shared/context.h"

/* TR7 library headers */
#include "tr7.h"

/* MSVC workaround: TR7ARG macros don't expand correctly with MSVC preprocessor */
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

#include "context.h"
#include "midi/midi.h"
#include "audio/audio.h"
#include "async.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>
#define isatty _isatty
#define getcwd _getcwd
#define STDIN_FILENO 0
#define usleep(us) Sleep((us) / 1000)
#else
#include <unistd.h>
#endif

/* ============================================================================
 * TR7 Usage and Help
 * ============================================================================ */

static void print_tr7_repl_usage(const char *prog) {
    printf("Usage: %s tr7 [options] [file.scm]\n", prog);
    printf("       %s scheme [options] [file.scm]\n", prog);
    printf("\n");
    printf("TR7 R7RS-small Scheme interpreter with music extensions.\n");
    printf("If no file is provided, starts an interactive REPL.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --verbose     Enable verbose output\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -p, --port N      Use MIDI port N (0-based index)\n");
    printf("  --virtual NAME    Create virtual MIDI port with NAME\n");
    printf("\n");
    printf("Built-in Synth Options:\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont (.sf2)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s tr7                   Start interactive Scheme REPL\n", prog);
    printf("  %s tr7 song.scm          Execute a Scheme file\n", prog);
    printf("  %s tr7 -sf gm.sf2        REPL with built-in synth\n", prog);
    printf("  %s tr7 --virtual TR7Out  REPL with virtual MIDI port\n", prog);
    printf("\n");
}

static void print_tr7_repl_help(void) {
    shared_print_command_help();

    printf("TR7-specific Commands:\n");
    printf("  ,load FILE        Load and execute a Scheme file\n");
    printf("\n");
    printf("Music Primitives:\n");
    printf("  (play-note pitch [vel] [dur])  Play a MIDI note\n");
    printf("  (play-chord '(p1 p2 ...) [vel] [dur])  Play chord\n");
    printf("  (play-seq '(p1 p2 ...) [vel] [dur])  Play notes in sequence\n");
    printf("  (note-on pitch [vel])    Send note-on\n");
    printf("  (note-off pitch)         Send note-off\n");
    printf("  (set-tempo bpm)          Set tempo\n");
    printf("  (set-octave n)           Set default octave (0-9)\n");
    printf("  (set-velocity v)         Set default velocity (0-127)\n");
    printf("  (set-channel ch)         Set MIDI channel (0-15)\n");
    printf("  (note \"c#\" [oct])        Convert note name to pitch\n");
    printf("\n");
    printf("MIDI Control:\n");
    printf("  (midi-list)              List MIDI ports\n");
    printf("  (midi-open port)         Open MIDI port by index\n");
    printf("  (midi-virtual name)      Create virtual MIDI port\n");
    printf("  (midi-panic)             All notes off\n");
    printf("  (program-change prog)    Change instrument\n");
    printf("  (control-change cc val)  Send CC message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  (play-note 60 80 500)    ; Middle C, vel 80, 500ms\n");
    printf("  (play-chord '(60 64 67)) ; C major chord\n");
    printf("  (set-tempo 140)          ; 140 BPM\n");
    printf("\n");
}

/* ============================================================================
 * TR7 Engine State
 * ============================================================================ */

/* TR7 engine state for REPL */
static tr7_engine_t g_tr7_repl_engine = NULL;
static SharedContext *g_tr7_repl_shared = NULL;

/* Music context for TR7 REPL */
typedef struct {
    int octave;
    int velocity;
    int tempo;
    int channel;
    int duration_ms;
} Tr7ReplMusicCtx;

static Tr7ReplMusicCtx g_tr7_music = {4, 80, 120, 0, 500};

/* Sleep helper */
static void tr7_sleep_ms(int ms) {
    if (ms > 0) {
        usleep(ms * 1000);
    }
}

/* ============================================================================
 * Scheme Music Primitives
 * ============================================================================ */

static tr7_C_return_t repl_scm_play_note(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;
    int duration = (nvalues >= 3) ? TR7_TO_INT(values[2]) : g_tr7_music.duration_ms;

    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    /* Use async playback to avoid blocking the REPL */
    tr7_async_play_note(g_tr7_repl_shared, g_tr7_music.channel, pitch, velocity, duration, g_tr7_music.tempo);

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_note_on(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;

    if (pitch >= 0 && pitch <= 127 && velocity >= 0 && velocity <= 127) {
        shared_send_note_on(g_tr7_repl_shared, g_tr7_music.channel, pitch, velocity);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_note_off(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    if (pitch >= 0 && pitch <= 127) {
        shared_send_note_off(g_tr7_repl_shared, g_tr7_music.channel, pitch);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_play_chord(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    if (!TR7_IS_PAIR(values[0]) && !TR7_IS_NIL(values[0])) {
        return tr7_C_raise_error(tsc, "play-chord: expected list of pitches", values[0], 0);
    }

    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;
    int duration = (nvalues >= 3) ? TR7_TO_INT(values[2]) : g_tr7_music.duration_ms;

    /* Collect pitches */
    int pitches[16];
    int count = 0;
    tr7_t list = values[0];

    while (TR7_IS_PAIR(list) && count < 16) {
        tr7_t note = TR7_CAR(list);
        if (TR7_IS_INT(note)) {
            int p = TR7_TO_INT(note);
            if (p >= 0 && p <= 127) {
                pitches[count++] = p;
            }
        }
        list = TR7_CDR(list);
    }

    /* Use async playback to avoid blocking the REPL */
    if (count > 0) {
        tr7_async_play_chord(g_tr7_repl_shared, g_tr7_music.channel,
                             pitches, count, velocity, duration, g_tr7_music.tempo);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_play_seq(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    if (!TR7_IS_PAIR(values[0]) && !TR7_IS_NIL(values[0])) {
        return tr7_C_raise_error(tsc, "play-seq: expected list of pitches", values[0], 0);
    }

    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;
    int duration = (nvalues >= 3) ? TR7_TO_INT(values[2]) : g_tr7_music.duration_ms;

    /* Collect pitches */
    int pitches[128];
    int count = 0;
    tr7_t list = values[0];

    while (TR7_IS_PAIR(list) && count < 128) {
        tr7_t note = TR7_CAR(list);
        if (TR7_IS_INT(note)) {
            int p = TR7_TO_INT(note);
            if (p >= 0 && p <= 127) {
                pitches[count++] = p;
            }
        }
        list = TR7_CDR(list);
    }

    /* Use async playback to play notes sequentially without blocking */
    if (count > 0) {
        tr7_async_play_sequence(g_tr7_repl_shared, g_tr7_music.channel,
                                pitches, count, velocity, duration, g_tr7_music.tempo);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_tempo(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int tempo = TR7_TO_INT(values[0]);
    if (tempo >= 20 && tempo <= 400) {
        g_tr7_music.tempo = tempo;
        g_tr7_music.duration_ms = 60000 / tempo;  /* Quarter note duration */
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_octave(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int octave = TR7_TO_INT(values[0]);
    if (octave >= 0 && octave <= 9) {
        g_tr7_music.octave = octave;
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_velocity(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int velocity = TR7_TO_INT(values[0]);
    if (velocity >= 0 && velocity <= 127) {
        g_tr7_music.velocity = velocity;
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_channel(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int channel = TR7_TO_INT(values[0]);
    if (channel >= 0 && channel <= 15) {
        g_tr7_music.channel = channel;
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_tempo(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.tempo));
}

static tr7_C_return_t repl_scm_octave(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.octave));
}

static tr7_C_return_t repl_scm_velocity(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.velocity));
}

static tr7_C_return_t repl_scm_channel(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.channel));
}

static tr7_C_return_t repl_scm_midi_list(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    shared_midi_list_ports(g_tr7_repl_shared);
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_midi_open(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int port = TR7_TO_INT(values[0]);
    int result = shared_midi_open_port(g_tr7_repl_shared, port);

    if (result == 0) {
        printf("TR7: Opened MIDI port %d\n", port);
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

static tr7_C_return_t repl_scm_midi_virtual(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    if (!TR7_IS_STRING(values[0])) {
        return tr7_C_raise_error(tsc, "midi-virtual: expected string name", values[0], 0);
    }

    const char *name = tr7_string_buffer(values[0]);
    int result = shared_midi_open_virtual(g_tr7_repl_shared, name);

    if (result == 0) {
        printf("TR7: Created virtual MIDI port: %s\n", name);
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

static tr7_C_return_t repl_scm_midi_panic(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    if (g_tr7_repl_shared) {
        shared_send_panic(g_tr7_repl_shared);
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_tsf_load(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;

    if (!TR7_IS_STRING(values[0])) {
        return tr7_C_raise_error(tsc, "tsf-load: expected string path", values[0], 0);
    }

    const char *path = tr7_string_buffer(values[0]);
    int result = shared_tsf_init() != 0 ? -1 : shared_tsf_load_soundfont(path);

    if (result == 0) {
        printf("TR7: Loaded SoundFont: %s\n", path);
        if (g_tr7_repl_shared) {
            g_tr7_repl_shared->builtin_synth_enabled = 1;
        }
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

static tr7_C_return_t repl_scm_sleep_ms(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int ms = TR7_TO_INT(values[0]);
    tr7_sleep_ms(ms);
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_program_change(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int program = TR7_TO_INT(values[0]);
    if (program < 0) program = 0;
    if (program > 127) program = 127;

    shared_send_program(g_tr7_repl_shared, g_tr7_music.channel, program);

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_control_change(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int cc = TR7_TO_INT(values[0]);
    int value = TR7_TO_INT(values[1]);

    if (cc < 0) cc = 0;
    if (cc > 127) cc = 127;
    if (value < 0) value = 0;
    if (value > 127) value = 127;

    shared_send_cc(g_tr7_repl_shared, g_tr7_music.channel, cc, value);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* Note name to MIDI pitch helper */
static int tr7_note_to_pitch(const char *name, int octave) {
    static const int base[] = {9, 11, 0, 2, 4, 5, 7};  /* a, b, c, d, e, f, g */

    if (!name || !*name) return -1;

    char note = name[0];
    if (note >= 'A' && note <= 'G') note = note - 'A' + 'a';
    if (note < 'a' || note > 'g') return -1;

    int pitch = base[note - 'a'];
    const char *p = name + 1;

    /* Handle accidentals */
    while (*p) {
        if (*p == '#' || *p == '+') pitch++;
        else if (*p == 'b' || *p == '-') pitch--;
        p++;
    }

    /* Calculate final pitch */
    pitch = (octave + 1) * 12 + pitch;

    return pitch;
}

static tr7_C_return_t repl_scm_note(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    int octave = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.octave;

    if (TR7_IS_STRING(values[0])) {
        const char *name = tr7_string_buffer(values[0]);
        int pitch = tr7_note_to_pitch(name, octave);
        if (pitch >= 0 && pitch <= 127) {
            return tr7_C_return_single(tsc, TR7_FROM_INT(pitch));
        }
    } else if (TR7_IS_SYMBOL(values[0])) {
        const char *name = tr7_symbol_string(values[0]);
        int pitch = tr7_note_to_pitch(name, octave);
        if (pitch >= 0 && pitch <= 127) {
            return tr7_C_return_single(tsc, TR7_FROM_INT(pitch));
        }
    }

    return tr7_C_raise_error(tsc, "note: invalid note name", values[0], 0);
}

/* Register all music primitives with TR7 engine */
static const tr7_C_func_def_t tr7_repl_music_funcs[] = {
    /* Note playing */
    {"play-note", repl_scm_play_note, NULL, TR7ARG_INTEGER, 1, 3},
    {"note-on", repl_scm_note_on, NULL, TR7ARG_INTEGER, 1, 2},
    {"note-off", repl_scm_note_off, NULL, TR7ARG_INTEGER, 1, 1},
    {"play-chord", repl_scm_play_chord, NULL, TR7ARG_PROPER_LIST, 1, 3},
    {"play-seq", repl_scm_play_seq, NULL, TR7ARG_PROPER_LIST, 1, 3},

    /* State setters */
    {"set-tempo", repl_scm_set_tempo, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-octave", repl_scm_set_octave, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-velocity", repl_scm_set_velocity, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-channel", repl_scm_set_channel, NULL, TR7ARG_INTEGER, 1, 1},

    /* State getters */
    {"tempo", repl_scm_tempo, NULL, NULL, 0, 0},
    {"octave", repl_scm_octave, NULL, NULL, 0, 0},
    {"velocity", repl_scm_velocity, NULL, NULL, 0, 0},
    {"channel", repl_scm_channel, NULL, NULL, 0, 0},

    /* MIDI control */
    {"midi-list", repl_scm_midi_list, NULL, NULL, 0, 0},
    {"midi-open", repl_scm_midi_open, NULL, TR7ARG_INTEGER, 1, 1},
    {"midi-virtual", repl_scm_midi_virtual, NULL, TR7ARG_STRING, 1, 1},
    {"midi-panic", repl_scm_midi_panic, NULL, NULL, 0, 0},

    /* Utilities */
    {"tsf-load", repl_scm_tsf_load, NULL, TR7ARG_STRING, 1, 1},
    {"sleep-ms", repl_scm_sleep_ms, NULL, TR7ARG_INTEGER, 1, 1},
    {"program-change", repl_scm_program_change, NULL, TR7ARG_INTEGER, 1, 1},
    {"control-change", repl_scm_control_change, NULL, TR7ARG_INTEGER, 2, 2},
    {"note", repl_scm_note, NULL, NULL, 1, 2},

    {NULL, NULL, NULL, NULL, 0, 0}  /* Sentinel */
};

static void tr7_repl_register_music_funcs(tr7_engine_t engine) {
    /* Register in tr7/foreigns library and import it */
    tr7_lib_register_C_func_list(engine, TR7_FOREIGNS_LIBNAME, tr7_repl_music_funcs);
    tr7_import_lib(engine, TR7_FOREIGNS_LIBNAME);
}

/* ============================================================================
 * TR7 Tab Completion
 * ============================================================================ */

/* TR7 completion words: music primitive names */
static const char *tr7_completion_words[] = {
    /* Note playing */
    "play-note", "note-on", "note-off", "play-chord", "play-seq",
    /* State setters */
    "set-tempo", "set-octave", "set-velocity", "set-channel",
    /* State getters */
    "tempo", "octave", "velocity", "channel",
    /* MIDI control */
    "midi-list", "midi-open", "midi-virtual", "midi-panic",
    /* Utilities */
    "tsf-load", "sleep-ms", "program-change", "control-change", "note",
};
#define TR7_COMPLETION_WORD_COUNT (sizeof(tr7_completion_words) / sizeof(tr7_completion_words[0]))

/* ============================================================================
 * TR7 REPL Loop
 * ============================================================================ */

/* Stop callback for TR7 REPL */
static void tr7_stop_playback(void) {
    tr7_async_stop();
    if (g_tr7_repl_shared) {
        shared_send_panic(g_tr7_repl_shared);
    }
}

/* Note: Using repl_repl_starts_with() from loki/repl_helpers.h */

/* Helper to load and run a Scheme file */
static int tr7_load_and_run_file(const char* filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Cannot open file: %s\n", filename);
        return -1;
    }

    int status = tr7_run_file(g_tr7_repl_engine, f, filename);
    fclose(f);

    if (status != 0) {
        tr7_t err = tr7_get_last_value(g_tr7_repl_engine);
        if (tr7_is_error(err)) {
            tr7_t msg = tr7_error_message(err);
            if (TR7_IS_STRING(msg)) {
                printf("Error loading %s: %s\n", filename, tr7_string_buffer(msg));
            } else {
                printf("Error loading %s\n", filename);
            }
        }
        return -1;
    }

    return 0;
}

/* Process a TR7 REPL command. Returns: 0=continue, 1=quit, 2=evaluate as Scheme */
static int tr7_process_command(const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(g_tr7_repl_shared, input, tr7_stop_playback);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        return 0;
    }

    /* Handle TR7-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_tr7_repl_help();
        return 0;
    }

    /* :play file.scm - load and execute a Scheme file */
    if (repl_starts_with(cmd, "play ")) {
        const char* path = cmd + 5;
        while (*path == ' ') path++;
        if (*path) {
            printf("Loading %s...\n", path);
            if (tr7_load_and_run_file(path) == 0) {
                printf("Loaded: %s\n", path);
            }
        } else {
            printf("Usage: :play PATH\n");
        }
        return 0;
    }

    /* ,load command (legacy syntax) */
    if (strncmp(input, ",load ", 6) == 0) {
        const char *filename = input + 6;
        while (*filename == ' ') filename++;

        if (*filename) {
            if (tr7_load_and_run_file(filename) == 0) {
                printf("Loaded: %s\n", filename);
            }
        }
        return 0;
    }

    return 2; /* evaluate as Scheme */
}

/* Non-interactive TR7 REPL loop for piped input */
static void tr7_repl_loop_pipe(void) {
    char line[MAX_INPUT_LENGTH];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = repl_strip_newlines(line);
        if (len == 0) continue;

        int result = tr7_process_command(line);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Evaluate as Scheme - use tr7_play_string which handles result printing */
        tr7_play_string(g_tr7_repl_engine, line,
            Tr7_Play_Show_Result | Tr7_Play_Show_Errors | Tr7_Play_Keep_Playing);
        fflush(stdout);
    }
}

static void tr7_repl_loop(editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;
    char history_path[512] = {0};

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        tr7_repl_loop_pipe();
        return;
    }

#ifdef LOKI_USE_LINENOISE
    repl_editor_init_with_language(&ed, REPL_LANG_SCHEME);
#else
    repl_editor_init(&ed);
#endif

    /* Set up tab completion for TR7 music primitives */
    repl_set_completion_words(&ed, tr7_completion_words, TR7_COMPLETION_WORD_COUNT);

    /* Build history file path and load history */
    if (repl_get_history_path("tr7", history_path, sizeof(history_path))) {
        repl_history_load(&ed, history_path);
    }

    printf("TR7 Scheme REPL %s (type :h for help, :q to quit)\n", PSND_VERSION);

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        input = repl_readline(syntax_ctx, &ed, "tr7> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = tr7_process_command(input);
        if (result == 1) break;      /* quit */
        if (result == 0) {
            /* Command handled - poll Link callbacks */
            shared_repl_link_check();
            continue;
        }

        /* Evaluate Scheme code - use tr7_play_string which handles result printing */
        tr7_play_string(g_tr7_repl_engine, input,
            Tr7_Play_Show_Result | Tr7_Play_Show_Errors | Tr7_Play_Keep_Playing);
        fflush(stdout);

        /* Poll Link callbacks after evaluation */
        shared_repl_link_check();
    }

    /* Disable raw mode before exit */
    repl_disable_raw_mode();

    /* Save history */
    if (history_path[0]) {
        repl_history_save(&ed, history_path);
    }

    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * Shared REPL Launcher Callbacks
 * ============================================================================ */

/* Helper to set up TR7 library paths */
static void tr7_setup_lib_paths(void) {
    static char tr7_lib_path[1100];
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(tr7_lib_path, sizeof(tr7_lib_path), "%s/" PSND_CONFIG_DIR "/lib/scm", cwd);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Path, tr7_lib_path);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Library_Path, tr7_lib_path);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Include_Path, tr7_lib_path);
    }
}

/* Helper to import standard Scheme libraries */
static void tr7_import_std_libs(void) {
    tr7_import_lib(g_tr7_repl_engine, "scheme/base");
    tr7_import_lib(g_tr7_repl_engine, "scheme/read");
    tr7_import_lib(g_tr7_repl_engine, "scheme/write");
    tr7_import_lib(g_tr7_repl_engine, "scheme/file");
    tr7_import_lib(g_tr7_repl_engine, "scheme/load");
    tr7_import_lib(g_tr7_repl_engine, "scheme/eval");
}

/* List MIDI ports */
static void tr7_cb_list_ports(void) {
    SharedContext tmp;
    shared_context_init(&tmp);
    shared_midi_list_ports(&tmp);
    shared_context_cleanup(&tmp);
}

/* Initialize TR7 context and MIDI/audio */
static void *tr7_cb_init(const SharedReplArgs *args) {
    /* Initialize async playback system */
    tr7_async_init();

    /* Initialize TR7 engine */
    g_tr7_repl_engine = tr7_engine_create(0);
    if (!g_tr7_repl_engine) {
        fprintf(stderr, "Error: Failed to create TR7 engine\n");
        tr7_async_cleanup();
        return NULL;
    }

    /* Set library paths and import standard libraries */
    tr7_setup_lib_paths();
    tr7_import_std_libs();

    /* Initialize shared context for MIDI/audio */
    g_tr7_repl_shared = calloc(1, sizeof(SharedContext));
    if (!g_tr7_repl_shared) {
        fprintf(stderr, "Error: Failed to create shared context\n");
        tr7_engine_destroy(g_tr7_repl_engine);
        g_tr7_repl_engine = NULL;
        return NULL;
    }
    shared_context_init(g_tr7_repl_shared);

    /* Register music primitives */
    tr7_repl_register_music_funcs(g_tr7_repl_engine);

    /* Setup output */
    if (args->soundfont_path) {
        /* Use built-in synth */
        if (shared_tsf_init() != 0 || shared_tsf_load_soundfont(args->soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", args->soundfont_path);
            shared_context_cleanup(g_tr7_repl_shared);
            free(g_tr7_repl_shared);
            tr7_engine_destroy(g_tr7_repl_engine);
            g_tr7_repl_shared = NULL;
            g_tr7_repl_engine = NULL;
            return NULL;
        }
        g_tr7_repl_shared->builtin_synth_enabled = 1;
        if (args->verbose) {
            printf("Using built-in synth: %s\n", args->soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (args->virtual_name) {
            if (shared_midi_open_virtual(g_tr7_repl_shared, args->virtual_name) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI port: %s\n", args->virtual_name);
                }
            }
        } else if (args->port_index >= 0) {
            if (shared_midi_open_port(g_tr7_repl_shared, args->port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            /* Try to open a virtual port by default */
            if (shared_midi_open_virtual(g_tr7_repl_shared, PSND_MIDI_PORT_NAME) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI output: " PSND_MIDI_PORT_NAME "\n");
                }
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    /* Initialize Link callbacks for REPL notifications */
    shared_repl_link_init_callbacks(g_tr7_repl_shared);

    /* Return non-NULL to indicate success (we use g_tr7_repl_engine as context) */
    return g_tr7_repl_engine;
}

/* Cleanup TR7 context and MIDI/audio */
static void tr7_cb_cleanup(void *lang_ctx) {
    (void)lang_ctx;

    /* Cleanup Link callbacks */
    shared_repl_link_cleanup_callbacks();

    /* Wait for async playback to finish, then cleanup */
    tr7_async_wait(1000);  /* Wait up to 1 second */
    tr7_async_cleanup();

    /* Wait for audio buffer to drain */
    if (g_tr7_repl_shared && g_tr7_repl_shared->builtin_synth_enabled) {
        usleep(300000);  /* 300ms for audio tail */
    }

    /* Cleanup */
    if (g_tr7_repl_shared) {
        shared_send_panic(g_tr7_repl_shared);
        shared_context_cleanup(g_tr7_repl_shared);
        free(g_tr7_repl_shared);
        g_tr7_repl_shared = NULL;
    }
    if (g_tr7_repl_engine) {
        tr7_engine_destroy(g_tr7_repl_engine);
        g_tr7_repl_engine = NULL;
    }
}

/* Execute a Scheme file */
static int tr7_cb_exec_file(void *lang_ctx, const char *path, int verbose) {
    (void)lang_ctx;
    (void)verbose;

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return 1;
    }

    int status = tr7_run_file(g_tr7_repl_engine, f, path);
    fclose(f);

    if (status == 0) {
        /* tr7_run_file returns 0 on load failure */
        fprintf(stderr, "Error: Failed to load file\n");
        return 1;
    }

    /* Execution happened - check if result was an error */
    tr7_t val = tr7_get_last_value(g_tr7_repl_engine);
    if (tr7_is_error(val)) {
        tr7_t msg = tr7_error_message(val);
        if (TR7_IS_STRING(msg)) {
            fprintf(stderr, "Error: %s\n", tr7_string_buffer(msg));
        } else {
            fprintf(stderr, "Error: Failed to execute file\n");
        }
        return 1;
    }

    return 0;
}

/* Run the TR7 REPL loop */
static void tr7_cb_repl_loop(void *lang_ctx, editor_ctx_t *syntax_ctx) {
    (void)lang_ctx;
    tr7_repl_loop(syntax_ctx);
}

/* TR7 shared REPL callbacks */
static const SharedReplCallbacks tr7_repl_callbacks = {
    .name = "tr7",
    .file_ext = ".scm",
    .prog_name = PSND_NAME,
    .print_usage = print_tr7_repl_usage,
    .list_ports = tr7_cb_list_ports,
    .init = tr7_cb_init,
    .cleanup = tr7_cb_cleanup,
    .exec_file = tr7_cb_exec_file,
    .repl_loop = tr7_cb_repl_loop,
};

/* ============================================================================
 * TR7 REPL Main Entry Point
 * ============================================================================ */

int tr7_repl_main(int argc, char **argv) {
    return shared_lang_repl_main(&tr7_repl_callbacks, argc, argv);
}

/* ============================================================================
 * TR7 Play Main Entry Point (headless file execution)
 * ============================================================================ */

int tr7_play_main(int argc, char **argv) {
    return shared_lang_play_main(&tr7_repl_callbacks, argc, argv);
}
