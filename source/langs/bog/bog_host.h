/**
 * @file bog_host.h
 * @brief Voice-to-MIDI mapping and clock shared by the psnd Bog hosts
 * (register.c for the editor, repl.c for the REPL).
 *
 * Drums use GM channel 10 at a fixed note. Melodic voices play the event's
 * Pitch. Any other voice atom plays Pitch on BOG_SYNTH_CHANNEL.
 */

#ifndef BOG_HOST_H
#define BOG_HOST_H

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define BOG_DRUM_CHANNEL  10  /* GM drums */
#define BOG_SYNTH_CHANNEL 1   /* Melodic voices and unknown voices */

typedef struct {
    const char *name;
    int channel;
    int note;  /* fixed MIDI note, or -1 to play the event's Pitch */
} BogVoiceMap;

static const BogVoiceMap bog_voice_map[] = {
    { "kick",     BOG_DRUM_CHANNEL,      36 },  /* Bass Drum 1 */
    { "snare",    BOG_DRUM_CHANNEL,      38 },  /* Acoustic Snare */
    { "hat",      BOG_DRUM_CHANNEL,      42 },  /* Closed Hi-Hat */
    { "clap",     BOG_DRUM_CHANNEL,      39 },  /* Hand Clap */
    { "noise",    BOG_DRUM_CHANNEL,      46 },  /* Open Hi-Hat */
    { "sine",     BOG_SYNTH_CHANNEL,     -1 },
    { "square",   BOG_SYNTH_CHANNEL + 1, -1 },
    { "triangle", BOG_SYNTH_CHANNEL + 2, -1 },
};

static inline int bog_clamp_midi(int v) {
    return v < 0 ? 0 : (v > 127 ? 127 : v);
}

/* Velocity 0.0-1.0 to MIDI 0-127 */
static inline int bog_vel_to_midi(double velocity) {
    return bog_clamp_midi((int)(velocity * 127.0));
}

/* Resolve a voice atom and event pitch to a MIDI channel and note */
static inline void bog_voice_lookup(const char *voice, double midi,
                                    int *channel, int *note) {
    *channel = BOG_SYNTH_CHANNEL;
    *note = bog_clamp_midi((int)midi);
    for (size_t i = 0; i < sizeof(bog_voice_map) / sizeof(bog_voice_map[0]); i++) {
        if (strcmp(voice, bog_voice_map[i].name) == 0) {
            *channel = bog_voice_map[i].channel;
            if (bog_voice_map[i].note >= 0)
                *note = bog_voice_map[i].note;
            return;
        }
    }
}

/* Monotonic seconds */
static inline double bog_wall_time(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

#endif /* BOG_HOST_H */
