/**
 * @file test_soundfont.c
 * @brief Tests for default soundfont discovery (soundfont.c).
 *
 * Discovery against the real default directories is not tested: its result
 * depends on the machine's soundfonts and MIDI ports.
 */

#include "test_framework.h"
#include "loki/soundfont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
static void set_env(const char *k, const char *v) { _putenv_s(k, v ? v : ""); }
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(p) mkdir(p, 0755)
#define remove_dir(p) rmdir(p)
static void set_env(const char *k, const char *v) {
    if (v) setenv(k, v, 1); else unsetenv(k);
}
#endif

test_stats_t test_stats;

static char g_root[512];

/* Create <root>/<sub> and the named empty files in it; returns the dir. */
static const char *make_sf_dir(const char *sub, const char *const *files, size_t n) {
    static char dirs[4][600];
    static int next = 0;
    char *dir = dirs[next++ % 4];
    snprintf(dir, sizeof(dirs[0]), "%s/%s", g_root, sub);
    make_dir(dir);
    for (size_t i = 0; i < n; i++) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        FILE *f = fopen(path, "wb");
        if (f) fclose(f);
    }
    return dir;
}

static void remove_sf_dir(const char *dir, const char *const *files, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        remove(path);
    }
    remove_dir(dir);
}

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

TEST(search_prefers_general_midi_name) {
    const char *files[] = {"aaa.sf2", "TimGM6mb.sf2"};
    const char *dir = make_sf_dir("pref", files, COUNT(files));
    char out[1024], expected[1024];
    snprintf(expected, sizeof(expected), "%s/TimGM6mb.sf2", dir);

    ASSERT_EQ(psnd_soundfont_search(&dir, 1, out, sizeof(out)), 0);
    ASSERT_STR_EQ(out, expected);
    remove_sf_dir(dir, files, COUNT(files));
}

TEST(search_preferred_name_in_later_dir_beats_other_sf2_in_earlier) {
    const char *f1[] = {"aaa.sf2"};
    const char *f2[] = {"FluidR3_GM.sf2"};
    const char *dirs[2];
    dirs[0] = make_sf_dir("first", f1, COUNT(f1));
    dirs[1] = make_sf_dir("second", f2, COUNT(f2));
    char out[1024], expected[1024];
    snprintf(expected, sizeof(expected), "%s/FluidR3_GM.sf2", dirs[1]);

    ASSERT_EQ(psnd_soundfont_search(dirs, 2, out, sizeof(out)), 0);
    ASSERT_STR_EQ(out, expected);
    remove_sf_dir(dirs[0], f1, COUNT(f1));
    remove_sf_dir(dirs[1], f2, COUNT(f2));
}

TEST(search_falls_back_to_first_sorted_sf2) {
    const char *files[] = {"b.sf2", "a.SF2", "0.txt"};
    const char *dir = make_sf_dir("fallback", files, COUNT(files));
    char out[1024], expected[1024];
    snprintf(expected, sizeof(expected), "%s/a.SF2", dir);

    ASSERT_EQ(psnd_soundfont_search(&dir, 1, out, sizeof(out)), 0);
    ASSERT_STR_EQ(out, expected);
    remove_sf_dir(dir, files, COUNT(files));
}

TEST(search_skips_missing_dirs_and_non_sf2) {
    const char *files[] = {"notes.txt"};
    const char *dirs[2];
    dirs[0] = "/nonexistent/psnd/soundfonts";
    dirs[1] = make_sf_dir("none", files, COUNT(files));
    char out[1024];

    ASSERT_EQ(psnd_soundfont_search(dirs, 2, out, sizeof(out)), -1);
    remove_sf_dir(dirs[1], files, COUNT(files));
}

TEST(search_rejects_small_buffer) {
    const char *files[] = {"TimGM6mb.sf2"};
    const char *dir = make_sf_dir("small", files, COUNT(files));
    char out[8];

    ASSERT_EQ(psnd_soundfont_search(&dir, 1, out, sizeof(out)), -1);
    remove_sf_dir(dir, files, COUNT(files));
}

TEST(resolve_explicit_wins_over_env) {
    set_env("PSND_SOUNDFONT", "/env/gm.sf2");
    ASSERT_STR_EQ(psnd_soundfont_resolve("/cli/gm.sf2", 0), "/cli/gm.sf2");
    set_env("PSND_SOUNDFONT", NULL);
}

TEST(resolve_other_backend_disables_lookup) {
    set_env("PSND_SOUNDFONT", "/env/gm.sf2");
    ASSERT_NULL(psnd_soundfont_resolve(NULL, 1));
    set_env("PSND_SOUNDFONT", NULL);
}

TEST(resolve_env_none_disables_lookup) {
    set_env("PSND_SOUNDFONT", "none");
    ASSERT_NULL(psnd_soundfont_resolve(NULL, 0));
    set_env("PSND_SOUNDFONT", NULL);
}

TEST(resolve_uses_env) {
    set_env("PSND_SOUNDFONT", "/env/gm.sf2");
    ASSERT_STR_EQ(psnd_soundfont_resolve(NULL, 0), "/env/gm.sf2");
    set_env("PSND_SOUNDFONT", NULL);
}

#ifndef _WIN32
TEST(resolve_expands_home_in_env) {
    const char *old_home = getenv("HOME");
    char saved[512];
    snprintf(saved, sizeof(saved), "%s", old_home ? old_home : "");
    set_env("HOME", "/home/test");
    set_env("PSND_SOUNDFONT", "~/sf/gm.sf2");

    ASSERT_STR_EQ(psnd_soundfont_resolve(NULL, 0), "/home/test/sf/gm.sf2");

    set_env("PSND_SOUNDFONT", NULL);
    set_env("HOME", old_home ? saved : NULL);
}
#endif

BEGIN_TEST_SUITE("Soundfont Discovery Tests")
#ifdef _WIN32
    const char *tmp = getenv("TEMP");
    snprintf(g_root, sizeof(g_root), "%s/psnd_sf_test_%d", tmp ? tmp : ".", (int)getpid());
#else
    snprintf(g_root, sizeof(g_root), "/tmp/psnd_sf_test_%d", (int)getpid());
#endif
    make_dir(g_root);

    RUN_TEST(search_prefers_general_midi_name);
    RUN_TEST(search_preferred_name_in_later_dir_beats_other_sf2_in_earlier);
    RUN_TEST(search_falls_back_to_first_sorted_sf2);
    RUN_TEST(search_skips_missing_dirs_and_non_sf2);
    RUN_TEST(search_rejects_small_buffer);
    RUN_TEST(resolve_explicit_wins_over_env);
    RUN_TEST(resolve_other_backend_disables_lookup);
    RUN_TEST(resolve_env_none_disables_lookup);
    RUN_TEST(resolve_uses_env);
#ifndef _WIN32
    RUN_TEST(resolve_expands_home_in_env);
#endif

    remove_dir(g_root);
END_TEST_SUITE()
