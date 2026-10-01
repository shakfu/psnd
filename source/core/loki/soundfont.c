/* soundfont.c - Default soundfont discovery (see soundfont.h) */

#include "soundfont.h"
#include "config.h"
#include "psnd_dirent.h"
#include "shared/midi/midi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

#define SF_PATH_MAX 1024

/* General MIDI soundfonts, most complete first (same list as aldakit) */
static const char *const PREFERRED[] = {
    "FluidR3_GM.sf2",
    "FluidR3_GS.sf2",
    "GeneralUser_GS.sf2",
    "TimGM6mb.sf2",
    "GeneralUser-GS.sf2",
    "SGM-V2.01.sf2",
    "Arachno.sf2",
    "default.sf2",
};

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int has_sf2_ext(const char *name) {
    size_t len = strlen(name);
    return len > 4 && strncasecmp(name + len - 4, ".sf2", 4) == 0;
}

static const char *home_dir(void) {
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home || !*home) home = getenv("USERPROFILE");
#endif
    return (home && *home) ? home : NULL;
}

/* Copy path to out, expanding a leading "~/". Returns 0 on success. */
static int expand_home(const char *path, char *out, size_t out_size) {
    const char *home = home_dir();
    int n;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0') && home) {
        n = snprintf(out, out_size, "%s%s", home, path + 1);
    } else {
        n = snprintf(out, out_size, "%s", path);
    }
    return (n < 0 || (size_t)n >= out_size) ? -1 : 0;
}

int psnd_soundfont_search(const char *const *dirs, size_t ndirs,
                          char *out, size_t out_size) {
    char path[SF_PATH_MAX];

    for (size_t d = 0; d < ndirs; d++) {
        for (size_t i = 0; i < sizeof(PREFERRED) / sizeof(PREFERRED[0]); i++) {
            int n = snprintf(path, sizeof(path), "%s/%s", dirs[d], PREFERRED[i]);
            if (n < 0 || (size_t)n >= sizeof(path)) continue;
            if (file_exists(path)) {
                return expand_home(path, out, out_size);
            }
        }
    }

    for (size_t d = 0; d < ndirs; d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        char best[256] = {0};
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            if (!has_sf2_ext(name) || strlen(name) >= sizeof(best)) continue;
            if (best[0] == '\0' || strcmp(name, best) < 0) {
                strcpy(best, name);
            }
        }
        closedir(dir);
        if (best[0] != '\0') {
            int n = snprintf(path, sizeof(path), "%s/%s", dirs[d], best);
            if (n >= 0 && (size_t)n < sizeof(path) && file_exists(path)) {
                return expand_home(path, out, out_size);
            }
        }
    }
    return -1;
}

/* Project-local and psnd dirs first, then aldakit's, then the usual user and
 * system locations. Returns the number of dirs written. */
static size_t default_dirs(char bufs[][SF_PATH_MAX], size_t max) {
    static const char *const home_rel[] = {
        ".psnd/soundfonts",
        ".aldakit/soundfonts",
        ".local/share/soundfonts",
        ".local/share/sounds/sf2",
#if defined(__APPLE__)
        "Library/Audio/Sounds/Banks",
#elif defined(_WIN32)
        "Documents/SoundFonts",
#endif
    };
    static const char *const system_dirs[] = {
#if defined(__APPLE__)
        "/Library/Audio/Sounds/Banks",
#elif defined(_WIN32)
        "C:/soundfonts",
#else
        "/usr/share/sounds/sf2",
        "/usr/share/soundfonts",
        "/usr/local/share/soundfonts",
#endif
    };
    const char *home = home_dir();
    size_t n = 0;

    if (n < max) snprintf(bufs[n++], SF_PATH_MAX, ".psnd/soundfonts");
    for (size_t i = 0; home && i < sizeof(home_rel) / sizeof(home_rel[0]) && n < max; i++) {
        snprintf(bufs[n++], SF_PATH_MAX, "%s/%s", home, home_rel[i]);
    }
    for (size_t i = 0; i < sizeof(system_dirs) / sizeof(system_dirs[0]) && n < max; i++) {
        snprintf(bufs[n++], SF_PATH_MAX, "%s", system_dirs[i]);
    }
    return n;
}

const char *psnd_soundfont_resolve(const char *explicit_sf, int other_backend) {
    static char result[SF_PATH_MAX];
    char bufs[12][SF_PATH_MAX];
    const char *dirs[12];

    if (explicit_sf) return explicit_sf;
    if (other_backend) return NULL;

    /* A named soundfont states intent, so it applies even with MIDI ports */
    const char *env = getenv("PSND_SOUNDFONT");
    if (env && strcmp(env, "none") == 0) return NULL;
    if (env && *env) {
        return expand_home(env, result, sizeof(result)) == 0 ? result : NULL;
    }
    loki_config_t *config = config_global();
    if (config && config->soundfont[0]) {
        return expand_home(config->soundfont, result, sizeof(result)) == 0 ? result : NULL;
    }

    size_t n = default_dirs(bufs, sizeof(bufs) / sizeof(bufs[0]));
    for (size_t i = 0; i < n; i++) dirs[i] = bufs[i];
    if (psnd_soundfont_search(dirs, n, result, sizeof(result)) != 0) {
        return NULL;
    }

    /* A discovered one only replaces MIDI when no port can reach a synth */
    return shared_midi_count_synth_outputs() > 0 ? NULL : result;
}
