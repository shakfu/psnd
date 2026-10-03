/**
 * @file test_treesitter_alda.c
 * @brief The Alda tree-sitter grammar parses every Alda score psnd ships.
 *
 * The grammar only drives highlighting, so nothing else notices when it
 * stops matching the language: an error node silently degrades the colours.
 */

#include "test_framework.h"
#include "loki/treesitter.h"
#include <psnd_dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(ALDA_EXAMPLES_DIR) || !defined(ALDA_SUITE_DIR)
#error "ALDA_EXAMPLES_DIR and ALDA_SUITE_DIR must be defined by the build"
#endif

#ifdef LOKI_USE_LINENOISE

static char* read_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* text = malloc((size_t)size + 1);
    *len = fread(text, 1, (size_t)size, f);
    text[*len] = '\0';
    fclose(f);
    return text;
}

/* Parse every .alda file in dir; returns the number with error nodes */
static int parse_dir(TreeSitterState* ts, const char* dir, int* files) {
    int bad = 0;
    DIR* d = opendir(dir);
    if (!d) return -1;
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        size_t n = strlen(entry->d_name);
        if (n < 5 || strcmp(entry->d_name + n - 5, ".alda") != 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        size_t len = 0;
        char* text = read_file(path, &len);
        if (!text) continue;

        treesitter_reparse(ts, text, len);
        if (!ts->tree || ts_node_has_error(ts_tree_root_node(ts->tree))) {
            printf("    parse error in %s\n", path);
            bad++;
        }
        (*files)++;
        free(text);
    }
    closedir(d);
    return bad;
}

/* The editor's embedded highlight query compiles against the grammar */
TEST(treesitter_alda_query_compiles) {
    TreeSitterState* ts = treesitter_init("alda");
    ASSERT_NOT_NULL(ts);
    treesitter_free(ts);
}

TEST(treesitter_alda_parses_every_score) {
    TreeSitterState* ts = treesitter_init("alda");
    ASSERT_NOT_NULL(ts);

    const char* dirs[] = {ALDA_EXAMPLES_DIR, ALDA_SUITE_DIR};
    int files = 0, bad = 0;
    for (int i = 0; i < 2; i++) {
        int result = parse_dir(ts, dirs[i], &files);
        ASSERT_TRUE(result >= 0);
        bad += result;
    }
    ASSERT_GT(files, 50);
    ASSERT_EQ(bad, 0);
    treesitter_free(ts);
}

TEST(treesitter_alda_constructs) {
    static const char* sources[] = {
        "midi-bass+lead: c",                       /* '+' in a name */
        "piano: c4 d8 e o5 c2.",                   /* c4 and o5 are not names */
        "riff = c8 d e\npiano: riff*2\nviolin: c", /* parts and variables */
        "piano: c4./e4 g c1/e/g/r4 b",             /* chord durations, rests */
        "piano: a-8~|2. b4~ c g2 | ~2",            /* ties and slurs */
        "piano: (key-sig '(e (flat))) (octave 'up) (key-sig \"f+\")*2",
        "strings.cello: V1: c1 V2: e2 V0: g {c d}4 [e f]'1",
    };
    TreeSitterState* ts = treesitter_init("alda");
    ASSERT_NOT_NULL(ts);
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        treesitter_reparse(ts, sources[i], strlen(sources[i]));
        ASSERT_NOT_NULL(ts->tree);
        ASSERT_FALSE(ts_node_has_error(ts_tree_root_node(ts->tree)));
    }
    treesitter_free(ts);
}

BEGIN_TEST_SUITE("Alda tree-sitter grammar")
    RUN_TEST(treesitter_alda_query_compiles);
    RUN_TEST(treesitter_alda_parses_every_score);
    RUN_TEST(treesitter_alda_constructs);
END_TEST_SUITE()

#else

int main(void) { return 0; }  /* No tree-sitter in this build */

#endif
