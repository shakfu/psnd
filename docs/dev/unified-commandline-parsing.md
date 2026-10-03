# Unified Command-Line Parsing

Status: **draft design**, not yet implemented.

## Overview

psnd currently parses `argv` in five independent places, each with its own hand-rolled `strcmp` chain. This document proposes replacing them with a single table-driven parser and a **parse-once, dispatch-on-the-result** flow in `main.c`.

The immediate motivation was a bug in which `psnd play -sf gm.sf2 song.alda` produced 28 seconds of silence and exit code 0. The trigger is worth stating precisely, because it shapes the design:

```c
/* source/core/main.c - before the fix */
for (int i = 2; i < argc; i++) {
    const LangDispatchEntry *file_lang = lang_dispatch_find_by_extension(argv[i]);
    if (file_lang && file_lang->play_main) {
        return file_lang->play_main(argc - i, argv + i);   /* i = index of the FILE */
    }
}
```

`argv` was scanned to identify the language by file extension, then forwarded **starting at the file**. Every option preceding the filename was discarded before any parser saw it.

The key lesson: **this was not a parsing bug.** A more robust parsing library would have faithfully parsed the truncated array it was handed. The defect lived in the dispatch layer, upstream of parsing. Any design that only swaps the parser leaves this class of bug intact.

## Current State

### Parsing sites

| Site | Role | Style |
|---|---|---|
| `source/core/main.c` | subcommand dispatch | `strcmp` on `argv[1]`, then extension scan |
| `source/core/loki/cli.c` | editor (`EditorCliArgs`) | `strcmp` chain, struct output |
| `source/core/loki/editor.c` | editor entry | `strcmp` chain |
| `source/core/loki/repl_launcher.c` | shared REPL + play (Joy/TR7/Bog) | `strcmp` chain |
| `source/langs/alda/repl.c` | Alda REPL + play | **two** implementations (see below) |
| `source/langs/mhs/repl.c` | MHS REPL + play | `strcmp` chain + passthrough |

### Defects in the current approach

1. **Silent argument loss.** The `play` dispatcher truncated `argv` (fixed; see `play_options_before_filename` in `source/core/tests/cli/test_play_command.c`).

2. **Unknown options are silently ignored.** Every parser ends with a variant of `else if (argv[i][0] != '-') { input_file = argv[i]; }`. An unrecognized flag matches no branch and disappears. `psnd play --soundfont-path gm.sf2 song.alda` runs silently with no synth and exits 0.

3. **A trailing option value is silently dropped.** The guards are written `(strcmp(argv[i], "-sf") == 0 && i + 1 < argc)`. When `-sf` is the last argument the guard fails, the arg falls through every branch, and playback proceeds with no soundfont.

4. **`-v` means two different things.** Version in the editor path (`cli.c:82`, `editor.c:351`), verbose everywhere else. Today `psnd song.alda -v` prints the version while `psnd play -v song.alda` enables verbose output.

5. **Alda has two divergent parsers for the same options.** `alda_repl_main` is split by `#ifdef _WIN32`: a hand-rolled loop on Windows, `getopt_long` on POSIX. The two can drift independently, and only one is exercised on any given build.

6. **`-sf` is not expressible in getopt.** Because `getopt_long` would read `-sf` as `-s -f`, the POSIX branch pre-processes `argv` by blanking entries in place:

   ```c
   /* source/langs/alda/repl.c */
   if (strcmp(argv[i], "-sf") == 0 && i + 1 < argc) {
       soundfont_path = argv[i + 1];
       argv[i] = "";           /* mutates argv */
       argv[i + 1] = "";
   }
   ```

7. **Help text is maintained separately from parsing.** `print_unified_help()` in `main.c` hardcodes the option list as `printf` calls, with no link to the code that parses them.

### Constraints any solution must satisfy

- **`-sf` must keep working.** It is the most-used option and appears throughout the docs and README. Multi-character short options must be supported.

- **MSVC builds are supported.** This rules out `getopt_long`, which is not available in the MSVC runtime. The only `getopt` in-tree is fluidsynth's bundled Windows shim (`source/thirdparty/fluidsynth/contrib/getopt/win/getopt.c`), vendored for fluidsynth's own use.

- **MHS needs passthrough.** `parse_mhs_args` splits psnd flags from arguments forwarded to the embedded MicroHs interpreter (`mhs_argv`). Unrecognized options are *data* there, not errors.

- **Per-language options exist.** Alda additionally has `-o/--output`, `-s/--sequential`. The spec must compose common options with language-specific ones.

## Proposed Design

### Why not a third-party framework

Considered and rejected for this codebase:

- **`getopt_long`** — not on MSVC; cannot express `-sf`.

- **argtable3** — mature, BSD-3, CMake-native, builds on MSVC. The best fit if a dependency is wanted, and it would slot into `source/thirdparty/` like the other 23 vendored libraries. Rejected only because the option surface is ~15 flags and argtable3 does not solve the dispatch problem, which is the actual defect.

- **cofyc/argparse, cargs, docopt.c** — all single-file and adequate, all GNU-style short options, so `-sf` still needs a workaround.

The recommendation is ~200 lines in-tree, extending the struct-based precedent already set by `EditorCliArgs`. This is a judgement call about dependency weight rather than a strong technical constraint; adopting argtable3 for the parser layer while keeping the dispatch design below would also be a coherent outcome.

### Layer 1: a table-driven parser

New files: `source/core/cli/cli_parse.h`, `source/core/cli/cli_parse.c`.

Options are described by a table rather than a `strcmp` chain, so parsing and help text derive from one source and cannot drift:

```c
typedef enum {
    PSND_OPT_FLAG,    /* no value:        --verbose        */
    PSND_OPT_STRING,  /* required value:  -sf PATH         */
    PSND_OPT_INT,     /* required value:  -p N             */
} PsndOptType;

typedef struct {
    const char  *long_name;   /* "soundfont" -> --soundfont            */
    const char  *short_name;  /* "sf"        -> -sf   (string, not char) */
    PsndOptType  type;
    size_t       offset;      /* offsetof() into the destination struct */
    const char  *arg_label;   /* "PATH", for generated help             */
    const char  *help;
} PsndOptSpec;
```

`short_name` is a string, not a `char`. That is the single decision that makes `-sf` expressible without mutating `argv`.

```c
typedef enum {
    PSND_UNKNOWN_ERROR,    /* default: reject and report        */
    PSND_UNKNOWN_COLLECT,  /* MHS: gather into passthrough[]    */
} PsndUnknownPolicy;

typedef struct {
    const PsndOptSpec *opts;
    int                opt_count;
    PsndUnknownPolicy  unknown;
} PsndCliSpec;

typedef struct {
    const char **positional;      int positional_count;
    const char **passthrough;     int passthrough_count;
    char         error[256];      /* human-readable message on failure */
} PsndCliResult;

int psnd_cli_parse(const PsndCliSpec *spec, void *dest,
                   int argc, char **argv, PsndCliResult *out);
```

Specified semantics — each line below is a current defect made impossible:

| Rule | Fixes |
|---|---|
| Options may appear before *or* after positionals | the truncation bug class |
| Unknown option is an error (unless policy is `COLLECT`) | defect 2 |
| Missing value for a value-taking option is an error | defect 3 |
| `--` terminates option parsing; the rest is positional | (new) escape hatch |
| `--name=value` accepted alongside `--name value` | (new) |
| `argv` is never mutated; all pointers alias `argv` | defect 6 |
| Help is generated from the same table | defect 7 |

### Layer 2: parse once, dispatch on the result

This is the part that fixes the actual bug. `main.c` resolves the whole command line into one struct *before* any language code runs, and no downstream code ever sees `argv` again:

```c
typedef enum {
    PSND_CMD_HELP, PSND_CMD_VERSION, PSND_CMD_REPL,
    PSND_CMD_PLAY, PSND_CMD_EDIT,    PSND_CMD_WEB,
} PsndCommand;

typedef struct {
    int         verbose;
    int         port_index;      /* -1 if unset */
    const char *virtual_name;
    const char *soundfont_path;
    const char *csound_path;
    int         list_ports;
} PsndCommonOpts;

typedef struct {
    PsndCommand              command;
    const LangDispatchEntry *lang;        /* resolved from subcommand or extension */
    const char              *input_file;
    PsndCommonOpts           common;
    EditorCliArgs            editor;      /* only for PSND_CMD_EDIT / _WEB */
    const char             **passthrough; int passthrough_count;
} PsndCli;
```

`LangDispatchEntry` gains two entry points taking the parsed struct:

```c
typedef struct {
    /* ... existing fields ... */
    int (*repl_main)(int argc, char **argv);   /* deprecated, removed in phase 4 */
    int (*play_main)(int argc, char **argv);   /* deprecated, removed in phase 4 */

    int (*repl)(const PsndCli *cli);           /* new */
    int (*play)(const PsndCli *cli);           /* new */
} LangDispatchEntry;
```

The dispatch loop then selects a language without slicing anything:

```c
case PSND_CMD_PLAY:
    if (!cli.lang || !cli.lang->play) { /* error */ }
    return cli.lang->play(&cli);        /* no argc/argv arithmetic anywhere */
```

The truncation bug becomes unrepresentable: there is no index arithmetic on `argv` to get wrong, and language selection is decoupled from argument ownership.

### Language selection

Selection rules move into the parser and are stated once:

1. `argv[1]` matching a registered command (`alda`, `joy`, …) selects that language.

2. Otherwise the first positional with a registered extension selects it.

3. `.csd` maps to the Csound backend, which is not a `LangDispatchEntry`.

4. No match with a `play` subcommand is an error, replacing the current "fall back to the first language that happens to have `play_main`" behaviour, which feeds a file with an unrecognized extension to whichever language registered first.

## Migration Plan

Staged so that each phase is independently reviewable and testable.

- **Phase 0 — parser only.** Add `cli_parse.{h,c}` plus unit tests. No call sites change, no behaviour changes.

- **Phase 1 — dual entry points.** Add `repl`/`play` to `LangDispatchEntry` and the `PsndCli` resolution in `main.c`. Dispatch prefers the new pointers and falls back to `*_main(argc - 2, argv + 2)` when they are NULL, so unmigrated languages keep working.

- **Phase 2 — migrate languages one at a time.** Alda first (it has the most duplication and the `#ifdef _WIN32` split), then the `repl_launcher` group, then MHS with `PSND_UNKNOWN_COLLECT`. One commit per language.

- **Phase 3 — editor and web.** Fold `EditorCliArgs` onto the shared parser, retaining the existing struct as the destination type.

- **Phase 4 — delete.** Remove `repl_main`/`play_main`, the five `strcmp` chains, and the hardcoded help text in `print_unified_help()`.

Behaviour changes are deliberately deferred to their own commits, because they are user-visible rather than internal:

- Unknown options becoming errors (previously ignored).

- Missing option values becoming errors (previously ignored).

- The `-v` unification (see below).

## Testing

The existing suite asserted only exit codes, which is exactly how the original bug survived: the broken build also returned 0. Tests must assert observable behaviour.

- **Parser unit tests** — table-driven over the semantics table above: order independence, `--`, `--name=value`, unknown option, missing value, `-sf` exact match, passthrough collection.

- **Dispatch tests** — `PsndCli` resolution for each subcommand and extension, asserting the resolved struct rather than a process exit code.

- **Integration tests** — extend `test_play_command.c`, which now has `play_options_before_filename` as the regression guard. It asserts on captured stdout via `test_exec_capture`, and has been verified to fail against the pre-fix dispatcher.

Every phase must keep the full suite green (currently 75 tests).

## Open Questions

1. **How to resolve the `-v` collision.** The proposal is `-v`/`--verbose` everywhere and `-V`/`--version` for version, which matches what `main.c` already does at the top level and makes the majority of sites correct. It breaks `psnd song.alda -v`, which currently prints the version. Alternatives: keep the editor's meaning and rename the REPL flag (breaks far more invocations), or make `-v` context-dependent (preserves compatibility, keeps the footgun). Needs a decision before phase 3.

2. **Should `.csd` become a `LangDispatchEntry`?** It is currently special-cased in both `main.c` (`has_csd_extension`) and `alda_play_main` (`is_csd_file`). Registering it would remove both special cases, but Csound is a backend rather than a language and has no REPL.

3. **Passthrough ordering for MHS.** `PSND_UNKNOWN_COLLECT` must preserve the relative order of forwarded arguments and decide whether a psnd flag appearing *after* `--` is forwarded rather than consumed. Current behaviour has no `--` concept, so this is new surface.

4. **Whether to adopt argtable3 instead of layer 1.** Layer 2 is the structural part of this design and is independent of that choice.
