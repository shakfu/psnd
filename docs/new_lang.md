# Adding a New Language to psnd

This guide explains how to add a new music programming language to psnd using the language bridge system.

## Quick Start

Use the generator script to create all boilerplate:

```bash
# Create a new language called "foo" with extension .foo
./scripts/new_lang.py foo

# Create with multiple extensions
./scripts/new_lang.py bar -e .bar .br

# Preview what would be created (dry run)
./scripts/new_lang.py baz --dry-run
```

The script generates everything under `source/langs/<name>/`:

- `CMakeLists.txt` - Build config, including the `psnd_register_language()` call, auto-discovered

- `register.c`, `repl.c`, `dispatch.c` - Editor, REPL, and CLI integration

- `impl/` - Parser and runtime skeletons

- `tests/` - Test scaffolding (`CMakeLists.txt`, `test_parser.c`), also auto-discovered

- `README.md` - Documentation template

- `examples/` - A sample program

It also writes `.psnd/languages/<name>.lua` for syntax highlighting.

No parent CMake file, `lang_config.h`, or `lang_dispatch.c` edit is needed: `lang_config_generated.h` and `lang_dispatch_generated.h` are produced at configure time by auto-discovery, and there is no central CMake list to update.

After running the script:

1. Implement your language in `source/langs/<name>/impl/`

2. Update the CMake library file with your sources

3. Run `make clean && make test` to verify

The rest of this document explains the generated code structure in detail.

## Overview

psnd uses a language bridge pattern that allows music languages to integrate with both the editor and standalone REPL without coupling core code to specific implementations. Each language:

1. Lives in its own directory under `source/langs/<langname>/`

2. Implements the `LokiLangOps` interface for editor integration

3. Provides a standalone REPL with shared command handling

4. Uses `SharedContext` for MIDI, audio, and Link integration

5. Registers itself via `source/core/lang_config.h` (single file for all language configuration)

6. Optionally provides Lua API bindings for scripting

## Directory Structure

For a language called "example", create:

```text
source/langs/example/
    register.c         # Editor integration (LokiLangOps)
    register.h         # Header declaring init function
    repl.c             # Standalone REPL implementation
    repl.h             # REPL header
    dispatch.c         # CLI dispatch handler
    impl/              # Core language implementation (optional)
        parser.c
        runtime.c
        ...
    include/example/   # Public headers (optional)

tests/example/
    CMakeLists.txt
    test_parser.c
    test_runtime.c

docs/example/
    README.md          # User documentation
```

## Step 1: Create the Language Library

### Core Implementation

Create your language's core implementation in `source/langs/example/impl/`:

```c
/* source/langs/example/impl/example_runtime.c */

#include "example_runtime.h"

typedef struct ExampleContext {
    /* Language-specific state */
    int tempo;
    int velocity;
    /* ... */
} ExampleContext;

ExampleContext *example_context_new(void) {
    ExampleContext *ctx = calloc(1, sizeof(ExampleContext));
    if (ctx) {
        ctx->tempo = 120;
        ctx->velocity = 80;
    }
    return ctx;
}

void example_context_free(ExampleContext *ctx) {
    if (ctx) free(ctx);
}

int example_eval(ExampleContext *ctx, const char *code) {
    /* Parse and evaluate the code */
    return 0;
}
```

### CMake Library

Create `source/langs/example/CMakeLists.txt`. Languages are auto-discovered by `scripts/cmake/psnd_languages.cmake`, which globs `source/langs/*/CMakeLists.txt` and generates the dispatch and config headers. **No other build file needs to be touched** — there is no central list to register in.

```cmake
# Example language
#
# This CMakeLists.txt is processed by psnd_languages.cmake auto-discovery.
# No modifications to other files are needed to add/remove this language.

set(EXAMPLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/example_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/example_parser.c
)

add_library(example STATIC ${EXAMPLE_SOURCES})
add_library(example::example ALIAS example)

target_include_directories(example
    PUBLIC
        ${PSND_ROOT_DIR}/source/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/impl
    PRIVATE
        ${PSND_ROOT_DIR}/source/core/shared
)

target_link_libraries(example PUBLIC shared)

psnd_platform_add_math(example PUBLIC)
psnd_platform_add_warnings(example)

target_compile_definitions(example PUBLIC LANG_EXAMPLE)

# Register with the psnd language system. This is what wires the language into
# CLI dispatch, the editor, and the generated headers.
psnd_register_language(
    NAME example
    DISPLAY_NAME "Example"
    DESCRIPTION "Example music language"
    COMMANDS example ex
    EXTENSIONS .ex .example
    SOURCES ${EXAMPLE_SOURCES}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/impl
    REPL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/repl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.c
    REGISTER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/register.c
    LINK_LIBRARIES example
)
```

Auto-discovery also creates a `LANG_EXAMPLE` CMake option (default `ON`), so the language can be excluded with `-DLANG_EXAMPLE=OFF`.

## Step 2: Implement the REPL

Create `source/langs/example/repl.c`:

```c
/**
 * @file repl.c

 * @brief Example language REPL with shared command handling.
 */

#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "shared/repl_commands.h"
#include "shared/context.h"

#include "example_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * REPL State

 * ============================================================================ */

static ExampleContext *g_example_ctx = NULL;
static SharedContext *g_shared_ctx = NULL;

/* ============================================================================
 * Usage and Help

 * ============================================================================ */

static void print_usage(const char *prog) {
    printf("Usage: %s [options] [file.ex]\n", prog);
    printf("\nExample music language interpreter.\n\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -p, --port N      Use MIDI port N\n");
    printf("  --virtual NAME    Create virtual MIDI port\n");
    printf("  -sf PATH          Use built-in synth with soundfont\n");
}

static void print_help(void) {
    shared_print_command_help();  /* Print shared commands */

    printf("Example-specific Commands:\n");
    printf("  :tempo BPM        Set tempo\n");
    printf("\n");
    printf("Example Syntax:\n");
    printf("  play c d e f g    Play notes\n");
    printf("\n");
}

/* ============================================================================
 * Command Processing

 * ============================================================================ */

/* Stop callback for shared commands */
static void example_stop_playback(void) {
    if (g_example_ctx) {
        example_stop(g_example_ctx);
    }
}

/* Process REPL command. Returns: 0=continue, 1=quit, 2=interpret as code */
static int process_command(const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(g_shared_ctx, input, example_stop_playback);
    if (result == REPL_CMD_QUIT) return 1;
    if (result == REPL_CMD_HANDLED) return 0;

    /* Handle language-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':') cmd++;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        print_help();
        return 0;
    }

    /* :tempo BPM */
    if (strncmp(cmd, "tempo ", 6) == 0) {
        int bpm = atoi(cmd + 6);
        if (bpm > 0) {
            example_set_tempo(g_example_ctx, bpm);
            printf("Tempo: %d BPM\n", bpm);
        }
        return 0;
    }

    return 2;  /* Interpret as code */
}

/* ============================================================================
 * REPL Loop

 * ============================================================================ */

static void repl_loop(editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;

    /* Handle piped input */
    if (!isatty(STDIN_FILENO)) {
        char line[4096];
        while (fgets(line, sizeof(line), stdin)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (len == 0) continue;

            int result = process_command(line);
            if (result == 1) break;
            if (result == 0) continue;

            example_eval(g_example_ctx, line);
        }
        return;
    }

    repl_editor_init(&ed);

    printf("Example REPL. Type :help for commands, :quit to exit.\n");

    while ((input = repl_editor_readline(&ed, "example> ", syntax_ctx)) != NULL) {
        if (input[0] == '\0') {
            free(input);
            continue;
        }

        repl_editor_add_history(&ed, input);

        int result = process_command(input);
        if (result == 1) {
            free(input);
            break;
        }
        if (result == 0) {
            free(input);
            continue;
        }

        /* Evaluate as code */
        int eval_result = example_eval(g_example_ctx, input);
        if (eval_result < 0) {
            printf("Error: %s\n", example_get_error(g_example_ctx));
        }

        free(input);
    }

    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * Main Entry Point

 * ============================================================================ */

int example_repl_main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *soundfont = NULL;
    const char *virtual_name = NULL;
    int port = -1;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            shared_midi_list_ports();
            return 0;
        }
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i+1 < argc) {
            port = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--virtual") == 0 && i+1 < argc) {
            virtual_name = argv[++i];
            continue;
        }
        if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) && i+1 < argc) {
            soundfont = argv[++i];
            continue;
        }
        if (argv[i][0] != '-') {
            input_file = argv[i];
        }
    }

    /* Initialize shared context */
    g_shared_ctx = malloc(sizeof(SharedContext));
    if (!g_shared_ctx || shared_context_init(g_shared_ctx) != 0) {
        fprintf(stderr, "Failed to initialize shared context\n");
        return 1;
    }

    /* Initialize language context */
    g_example_ctx = example_context_new();
    if (!g_example_ctx) {
        fprintf(stderr, "Failed to initialize example context\n");
        shared_context_cleanup(g_shared_ctx);
        free(g_shared_ctx);
        return 1;
    }

    /* Connect language to shared context for MIDI output */
    example_set_shared(g_example_ctx, g_shared_ctx);

    /* Setup MIDI output */
    if (virtual_name) {
        shared_midi_open_virtual(g_shared_ctx, virtual_name);
    } else if (port >= 0) {
        shared_midi_open_port(g_shared_ctx, port);
    }

    /* Setup audio backend */
    if (soundfont) {
        shared_audio_tsf_load(g_shared_ctx, soundfont);
    }

    /* If file provided, run it */
    if (input_file) {
        int result = example_eval_file(g_example_ctx, input_file);
        if (result < 0) {
            fprintf(stderr, "Error: %s\n", example_get_error(g_example_ctx));
        }
    }

    /* Run REPL if interactive or no file */
    if (!input_file || isatty(STDIN_FILENO)) {
        editor_ctx_t syntax_ctx = {0};
        repl_loop(&syntax_ctx);
    }

    /* Cleanup */
    shared_midi_panic(g_shared_ctx);
    example_context_free(g_example_ctx);
    shared_context_cleanup(g_shared_ctx);
    free(g_shared_ctx);

    return 0;
}
```

Create `source/langs/example/repl.h`:

```c
#ifndef EXAMPLE_REPL_H
#define EXAMPLE_REPL_H

int example_repl_main(int argc, char **argv);

#endif
```

## Step 3: Create the Dispatch Handler

Create `source/langs/example/dispatch.c`:

```c
/**
 * @file dispatch.c

 * @brief CLI dispatch for Example language.
 */

#include "repl.h"
#include "psnd.h"

int example_dispatch(int argc, char **argv) {
    return example_repl_main(argc, argv);
}
```

## Step 4: Implement Editor Integration (LokiLangOps)

Create `source/langs/example/register.h`:

```c
#ifndef EXAMPLE_REGISTER_H
#define EXAMPLE_REGISTER_H

/**
 * Initialize Example language registration with the language bridge.

 * Called from loki_lang_init() when LANG_EXAMPLE is defined.
 */
void example_loki_lang_init(void);

#endif
```

Create `source/langs/example/register.c`:

```c
/**
 * @file register.c

 * @brief Example language integration with Loki editor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "register.h"
#include "psnd.h"
#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"
#include "lauxlib.h"

#include "example_runtime.h"
#include "shared/context.h"

/* ============================================================================
 * Per-Context State

 * ============================================================================ */

struct LokiExampleState {
    int initialized;
    ExampleContext *example_ctx;
    SharedContext *shared;
    char last_error[256];
};

static LokiExampleState *get_state(editor_ctx_t *ctx) {
    return ctx ? ctx->model.example_state : NULL;
}

static void set_error(LokiExampleState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

/* ============================================================================
 * LokiLangOps Implementation

 * ============================================================================ */

static int example_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Already initialized? */
    if (ctx->model.example_state && ctx->model.example_state->initialized) {
        return 0;
    }

    /* Allocate state */
    LokiExampleState *state = ctx->model.example_state;
    if (!state) {
        state = calloc(1, sizeof(LokiExampleState));
        if (!state) return -1;
        ctx->model.example_state = state;
    }

    /* Create language context */
    state->example_ctx = example_context_new();
    if (!state->example_ctx) {
        set_error(state, "Failed to create example context");
        free(state);
        ctx->model.example_state = NULL;
        return -1;
    }

    /* Create shared context for MIDI/audio */
    state->shared = malloc(sizeof(SharedContext));
    if (!state->shared || shared_context_init(state->shared) != 0) {
        set_error(state, "Failed to initialize shared context");
        example_context_free(state->example_ctx);
        free(state);
        ctx->model.example_state = NULL;
        return -1;
    }

    /* Connect language to shared context */
    example_set_shared(state->example_ctx, state->shared);

    /* Open virtual MIDI port */
    shared_midi_open_virtual(state->shared, PSND_MIDI_PORT_NAME "-example");

    state->initialized = 1;
    set_error(state, NULL);
    return 0;
}

static void example_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiExampleState *state = get_state(ctx);
    if (!state) return;

    if (state->shared) {
        shared_midi_panic(state->shared);
        shared_context_cleanup(state->shared);
        free(state->shared);
    }

    if (state->example_ctx) {
        example_context_free(state->example_ctx);
    }

    free(state);
    ctx->model.example_state = NULL;
}

static int example_is_initialized(editor_ctx_t *ctx) {
    LokiExampleState *state = get_state(ctx);
    return state ? state->initialized : 0;
}

static int example_eval(editor_ctx_t *ctx, const char *code) {
    LokiExampleState *state = get_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_error(state, "Not initialized");
        return -1;
    }

    int result = example_eval_code(state->example_ctx, code);
    if (result < 0) {
        set_error(state, example_get_error(state->example_ctx));
        return -1;
    }

    set_error(state, NULL);
    return 0;
}

static void example_stop(editor_ctx_t *ctx) {
    LokiExampleState *state = get_state(ctx);
    if (!state || !state->initialized) return;

    example_stop_playback(state->example_ctx);
    if (state->shared) {
        shared_midi_panic(state->shared);
    }
}

static const char *example_get_error(editor_ctx_t *ctx) {
    LokiExampleState *state = get_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}

static int example_is_playing(editor_ctx_t *ctx) {
    LokiExampleState *state = get_state(ctx);
    if (!state || !state->initialized) return 0;
    return example_is_active(state->example_ctx);
}

/* ============================================================================
 * Lua API (Optional)

 * ============================================================================ */

static int lua_example_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int result = example_init(ctx);
    lua_pushboolean(L, result == 0);
    return 1;
}

static int lua_example_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);

    int result = example_eval(ctx, code);
    if (result != 0) {
        lua_pushnil(L);
        lua_pushstring(L, example_get_error(ctx));
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_example_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    example_stop(ctx);
    return 0;
}

static void example_register_lua_api(lua_State *L) {
    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_newtable(L);

    lua_pushcfunction(L, lua_example_init);
    lua_setfield(L, -2, "init");

    lua_pushcfunction(L, lua_example_eval);
    lua_setfield(L, -2, "eval");

    lua_pushcfunction(L, lua_example_stop);
    lua_setfield(L, -2, "stop");

    lua_setfield(L, -2, "example");
    lua_pop(L, 1);
}

/* ============================================================================
 * Language Registration

 * ============================================================================ */

static const LokiLangOps example_lang_ops = {
    .name = "example",
    .extensions = {".ex", ".example", NULL},

    /* Lifecycle */
    .init = example_init,
    .cleanup = example_cleanup,
    .is_initialized = example_is_initialized,

    /* Main loop (NULL if not needed) */
    .check_callbacks = NULL,

    /* Playback */
    .eval = example_eval,
    .stop = example_stop,
    .is_playing = example_is_playing,

    /* Export (NULL if not supported) */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = example_get_error,

    /* Backend (NULL if not supported) */
    .configure_backend = NULL,

    /* Lua API */
    .register_lua_api = example_register_lua_api,
};

void example_loki_lang_init(void) {
    loki_lang_register(&example_lang_ops);
}
```

## Step 5: Register the Language

There is nothing to hand-edit here. `psnd_register_language()` in your language's `CMakeLists.txt` (Step 1) is the single registration point. At configure time `scripts/cmake/psnd_languages.cmake` walks `source/langs/*`, and for every directory containing a `CMakeLists.txt` it defines a `LANG_<NAME>` option (default `ON`), processes the directory, and collects what you registered.

It then generates two headers into `${CMAKE_BINARY_DIR}/generated/`:

| Generated header | Provides |
|------------------|----------|
| `lang_config_generated.h` | `IF_LANG_<NAME>` macros, `struct Loki<Name>State` forward decls, `LOKI_LANG_STATE_FIELDS_GENERATED`, `LOKI_LANG_INIT_ALL_GENERATED` |
| `lang_dispatch_generated.h` | `<name>_dispatch_init()` decls and `LANG_DISPATCH_INIT_ALL_GENERATED` |

`source/core/lang_config.h` and `source/core/lang_dispatch.c` are thin shims that include these and expand the generated macros. Both are marked DO NOT EDIT - earlier revisions of this guide had you edit them by hand, which is no longer correct and will be overwritten.

What this requires of your code is only that the symbols the generator declares actually exist:

- `void example_loki_lang_init(void);` - called by `LOKI_LANG_INIT_ALL()`

- `void example_dispatch_init(void);` - called by `LANG_DISPATCH_INIT_ALL()`

- `struct LokiExampleState` - the type named in `ctx->model.example_state`

The `Loki<Name>State` spelling is derived by title-casing the `NAME` you passed to `psnd_register_language()`, so `NAME example` must pair with `LokiExampleState`.

## Step 6: Build Configuration

Also nothing to do. The `LANG_EXAMPLE` option and the `LANG_EXAMPLE=1` compile definition are created by the discovery pass; `psnd_collect_lang_sources()` folds your `SOURCES`, `INCLUDE_DIRS`, `REPL_SOURCES`, `REGISTER_SOURCES`, and `LINK_LIBRARIES` into the `loki` library and the `psnd` binary.

To build without your language:

```sh
cmake -B build -DLANG_EXAMPLE=OFF
```

Confirm discovery worked by looking for these lines in the configure output:


```
-- Discovered language: example
--   Registered: example (Example)
```

## Step 7: Add CLI Dispatch

`source/core/lang_dispatch.c` needs no per-language edit - it expands `LANG_DISPATCH_INIT_ALL_GENERATED()`, which calls the `example_dispatch_init()` you provide in `source/langs/example/dispatch.c`. Register your entry points there against the `LangDispatchEntry` table (`source/core/lang_dispatch.h:25-45`):

```c
/* source/langs/example/dispatch.c */
#include "lang_dispatch.h"

/* Entry points from repl.c */
extern int example_repl_main(int argc, char **argv);
extern int example_play_main(int argc, char **argv);

static const LangDispatchEntry example_entry = {
    .commands        = { "example", "ex" },
    .command_count   = 2,
    .extensions      = { ".ex", ".example" },
    .extension_count = 2,
    .display_name    = "Example",
    .description     = "Example music language",
    .repl_main       = example_repl_main,
    .play_main       = example_play_main,  /* optional; NULL if unsupported */
};

void example_dispatch_init(void) {
    lang_dispatch_register(&example_entry);
}
```

The generated `lang_dispatch_generated.h` declares `example_dispatch_init()` and calls it for you. The commands and extensions you pass to `psnd_register_language()` are what the build advertises; keep them in sync with this table.

## Step 8: Add Tests

Create `source/langs/example/tests/CMakeLists.txt`. Any language directory with a `tests/CMakeLists.txt` is added automatically when `BUILD_TESTING` is on (`source/langs/CMakeLists.txt:24-33`):

```cmake
# Example language tests

macro(add_example_test TEST_NAME)
    add_executable(test_example_${TEST_NAME} test_${TEST_NAME}.c)
    target_link_libraries(test_example_${TEST_NAME} PRIVATE example test_framework)
    target_include_directories(test_example_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/langs/example/impl
        ${PSND_ROOT_DIR}/source/testing
    )
    if(UNIX)
        target_link_libraries(test_example_${TEST_NAME} PRIVATE m)
    endif()
    add_test(
        NAME example_${TEST_NAME}
        COMMAND $<TARGET_FILE:test_example_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(example_${TEST_NAME} PROPERTIES LABELS "unit")
endmacro()

add_example_test(parser)
```

If a test reads fixture files, pass their location as an absolute path from CMake rather than hardcoding a path relative to the build directory:

```cmake
target_compile_definitions(test_example_${TEST_NAME} PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
```

Create `source/langs/example/tests/test_parser.c`:

```c
#include "test_framework.h"
#include "example_parser.h"

TEST(test_parse_simple) {
    /* Test parsing */
    ASSERT_TRUE(1);
}

int main(void) {
    RUN_TEST(test_parse_simple);
    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
```

No parent test file needs updating - the `add_subdirectory` is driven by the discovery loop, not a hand-maintained list.

## Step 9: Add Documentation

Create `docs/example/README.md` with:

- Quick Start guide

- Core concepts

- REPL commands

- Editor integration

- Lua API

- Example programs

See existing documentation in `docs/alda/`, `docs/joy/`, `docs/tr7/`, or `docs/bog/` for reference.

## Step 10: Add Syntax Highlighting (Optional)

Create `.psnd/languages/example.toml`:

```lua
loki.register_language({
    name = "Example",
    extensions = {".ex", ".example"},
    keywords = {"play", "note", "rest", "tempo", "volume"},
    types = {},
    line_comment = "#",
    block_comment_start = nil,
    block_comment_end = nil,
    string_delimiters = {'"', "'"},
})
```

## Architecture Reference

### LokiLangOps Interface

```c
typedef struct LokiLangOps {
    /* Identification */
    const char *name;                              /* e.g., "example" */
    const char *extensions[LOKI_MAX_EXTENSIONS];   /* e.g., {".ex", NULL} */

    /* Lifecycle */
    int (*init)(editor_ctx_t *ctx);
    void (*cleanup)(editor_ctx_t *ctx);
    int (*is_initialized)(editor_ctx_t *ctx);

    /* Main loop integration (optional) */
    void (*check_callbacks)(editor_ctx_t *ctx, lua_State *L);

    /* Playback */
    int (*eval)(editor_ctx_t *ctx, const char *code);
    void (*stop)(editor_ctx_t *ctx);
    int (*is_playing)(editor_ctx_t *ctx);

    /* Export (optional) */
    int (*has_events)(editor_ctx_t *ctx);
    int (*populate_shared_buffer)(editor_ctx_t *ctx);

    /* Error handling */
    const char *(*get_error)(editor_ctx_t *ctx);

    /* Backend configuration (optional) */
    int (*configure_backend)(editor_ctx_t *ctx, const char *sf_path, const char *csd_path);

    /* Lua API (optional) */
    void (*register_lua_api)(lua_State *L);
} LokiLangOps;
```

### SharedContext

The `SharedContext` from `source/core/shared/` provides unified MIDI, audio, and Link access:

```c
/* MIDI */
int shared_midi_open_port(SharedContext *ctx, int port);
int shared_midi_open_virtual(SharedContext *ctx, const char *name);
void shared_midi_list_ports(void);
void shared_send_note_on(SharedContext *ctx, int channel, int note, int velocity);
void shared_send_note_off(SharedContext *ctx, int channel, int note);
void shared_midi_panic(SharedContext *ctx);

/* Audio backends */
int shared_audio_tsf_load(SharedContext *ctx, const char *path);
int shared_audio_csound_load(SharedContext *ctx, const char *path);

/* Ableton Link */
int shared_link_init(SharedContext *ctx, double tempo);
int shared_link_enable(SharedContext *ctx, int enable);
double shared_link_tempo(SharedContext *ctx);
```

### Key Files

| File | Purpose |
|------|---------|
| `source/langs/<name>/CMakeLists.txt` | **The only file you register in** - `psnd_register_language()` |
| `scripts/cmake/psnd_languages.cmake` | Discovery, registration, and header generation |
| `source/core/lang_config.h` | Generated-header shim - do not hand-edit |
| `source/core/lang_dispatch.c` | Generated-macro dispatch - do not hand-edit |
| `source/core/shared/context.h` | Shared MIDI/audio context |
| `source/core/shared/repl_commands.h` | Common REPL command handling |
| `source/langs/alda/register.c` | Reference: Alda integration |
| `source/langs/joy/register.c` | Reference: Joy integration |
| `source/langs/tr7/impl/register.c` | Reference: TR7 integration |
| `source/langs/bog/register.c` | Reference: Bog integration |

## Tips

1. **Use existing languages as reference** - Study `source/langs/joy/` or `source/langs/bog/` for well-structured examples

2. **Keep state per-context** - Store language state in `ctx->model.<lang>_state`, not globals (except for REPL mode)

3. **Use SharedContext** - Don't implement MIDI/audio directly; use the shared services

4. **Handle errors gracefully** - Always set meaningful error messages via `set_error()`

5. **Support piped input** - Check `isatty(STDIN_FILENO)` and handle non-interactive mode

6. **Use shared REPL commands** - Call `shared_process_command()` for common commands like `:stop`, `:midi`, `:link`

7. **Test thoroughly** - Add unit tests for parser and runtime, integration tests for REPL

8. **Document well** - Create comprehensive documentation in `docs/<langname>/README.md`
