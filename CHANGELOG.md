# CHANGELOG

All notable project-wide changes will be documented in this file. Note that each subproject has its own CHANGELOG.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and [Commons Changelog](https://common-changelog.org). This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Types of Changes

- Added: for new features.

- Changed: for changes in existing functionality.

- Deprecated: for soon-to-be removed features.

- Removed: for now removed features.

- Fixed: for any bug fixes.

- Security: in case of vulnerabilities.

---

## [Unreleased]

## [0.3.0]

### Fixed

- **MHS REPL Echoed Every Line Twice And Printed Results Late**: psnd's editor draws the line, then writes it into the PTY, where the terminal driver echoes it back and psnd printed that too. Output was also collected after a fixed 100ms sleep rather than read until MicroHs prompted again, so a result slower than that -- `midiInit` opening CoreMIDI, for one -- appeared under the *next* prompt, and the echo split across reads showed up as lines missing their first character. psnd now matches the echo against what it just sent and drops it, and drains the PTY until the prompt returns or 5s pass, so a result lands under the line that produced it (`repl.c`)

- **Schedule Growth Ignored `realloc` Failure**: `schedule_grow()` assigned the result of `realloc()` straight to `sched->events` and raised `capacity` whether or not the allocation succeeded, so a failed grow lost the existing buffer and the caller then wrote an event through a null pointer. It now grows through a temporary, rejects a capacity whose byte size would overflow `size_t`, and reports failure; all thirteen scheduling entry points drop the event instead of writing past the buffer (`shared_async.c`)

- **Tracker Gate Wrapped Instead Of Clamping**: `tracker_notes_parse_gate()` cast an unbounded `int` to its `int16_t` output, so `~65536` became a gate of 0 and other values became negative durations. Gates now clamp to `INT16_MAX`, matching the clamp velocity already applies at 127. `parse_int()` saturates as well, having been undefined behaviour on any expression carrying more digits than an `int` holds (`tracker_plugin_notes.c`)

- **MHS Package Preload Grew `.mhscache` Without Bound**: psnd passed `-pbase -pmusic` on every `mhs` invocation. MicroHs appends preloaded packages to the cache it just read without checking whether they are already there, so each run added ~2.3MB and about a second of load time: 3s at run 1, 25s by run 12, and eventually a `mhs_smoke_tests` timeout whose kill left a zero-length cache that every later run aborted on. The packages survive in the cache, so psnd now preloads only when the cache is missing, empty, or older than the binary, and deletes the cache in the latter two cases rather than mixing package copies. `mhs_cache_needs_preload()` sits in `vfs.c` so the standalone `mhs-midi` variants get the same guard. Upstream MicroHs has the same unconditional `addPackage` on `master`, so the version bump does not fix it (`vfs.c`, `repl.c`, `mhs_midi_standalone_main.c`)

- **Edited MHS Files Kept Running Their Old Code**: `psnd mhs -r file.hs` and `psnd play file.hs` served whatever was compiled the first time, until `.mhscache` was deleted by hand. MicroHs keys a cached module by the identifier the command line gave it, so a file argument is stored under `file.hs` while `validateCache` looks the entry up under the module's declared name, finds nothing, and checksums nothing; passing the same module by name validates correctly. psnd now reads the declared module name out of the file and passes it with `-i` for the directory the name implies, falling back to the path when the file does not sit where its module name says it should, which is the one case that stays stale. `mhs_reload_tests` covers it: run a module, rewrite it, run again, assert the new output. Present since MicroHs 0.15.0.0, reported upstream with a patch in `patch-mhs-validate-file-arg.diff` (`repl.c`)

- **Every MHS Run Rewrote The Whole Compile Cache**: psnd passed `-C`, so each run re-serialized `.mhscache` even when nothing in it changed. MicroHs 0.16 compresses that file with LZMA instead of LZ77, which put ~60% of a warm run inside the LZMA encoder: 1.85s of the 2.15s upstream `mhs` spends on one warm file, against 0.30s with `-CR`. psnd now passes `-CR` on a warm cache and `-C` only when cold, keyed off the same test that gates the preload flags. A warm `psnd mhs -r Smoke.hs` is 0.41s, from 4.25s; `mhs_smoke_tests` is 0.49s, from 4.37s. A warm run no longer persists a module it just compiled, which costs 0.3s on an unchanged small file and nothing on a changed one (`vfs.c`, `vfs.h`, `repl.c`, `mhs_midi_standalone_main.c`)

- **macOS Link Flags Overflowed The MHS argv Array**: `LINK_EXTRA_ARGS` was 14 in both branches of its `#ifdef __APPLE__`, but the macOS branch pushes 20 entries -- three static libraries, three frameworks, and `-lc++`. Every `psnd mhs -o exe file.hs` wrote four pointers past the allocated argv and mhs then read heap garbage as a module name, failing with `File not found: "("`. The macOS count is now 20 (`repl.c`)

- **`make test` Had No Per-Test Timeout**: a test blocking on stdin ran until the caller's own timeout with no indication of which test hung. The ten `ctest` targets now share one `PSND_CTEST` variable carrying `--timeout 120`, the ceiling `scripts/build_release.py` already used (`Makefile`)

### Changed

- **Vendored MicroHs 0.15.0.0 -> 0.16.5.0**: 553 upstream commits. The predicted blocker, 0.16's `getPaths` returning no package directory when `MHSDIR` is set, does not apply: `-a` fills `pkgPaths` independently and psnd already passes it. What the bump did need was `mhs.conf`, renamed from `targets.conf` upstream and now fatal rather than advisory on the compile-to-executable path, so `mhs-embed` gained `--conf` and serves it from the virtual root. Compilation itself is unchanged: the same six modules compile to C in 1.44s under 0.15.0.0 and 1.42s under 0.16.5.0, a three-line module in 0.50s under both, and a warm run that only reads the cache is 0.28s against 0.30s. What did change is the cache write, 0.71s against 1.88s, because 0.16 compresses `.mhscache` with LZMA rather than LZ77; the file is smaller for it, 730,587 bytes against 1,324,707. A cold `psnd mhs -r` pays that write once and went 2.77s -> 5.62s, while the warm path is faster than either version was, for the reasons in the two entries above. `docs/dev/updating-microhs.md` records the measurements and why Path A was taken over adopting upstream `--embed-packages` (`source/thirdparty/MicroHs`, `mhs-embed.c`, `langs/mhs/CMakeLists.txt`)

### Added

- **Local Patches To Vendored MicroHs, Applied At Build Time**: `source/langs/mhs/patches/` holds fixes psnd needs before upstream takes them, with a README saying what each one is for. The build copies the vendored tree, patches the copy, and rebuilds the compiler from it with `bin/mhs`, so `source/thirdparty/MicroHs` is never modified. The first patch makes the REPL run an `IO` action that returns a value instead of refusing it: `midiInit` at the prompt reported `Cannot satisfy constraint: Show (IO Bool)`, as did `midiListPorts`, `midiIsOpen`, `midiRandom` and the three record queries. Cost is one 39.7s self-compile, cached until a patch or `bin/mhs` changes; `-DMHS_PATCH_MHS=OFF` builds against upstream's committed `generated/mhs.c` (`langs/mhs/CMakeLists.txt`, `patches/`)

- **Third-Party License Texts In Release Archives**: `docs/licenses/` holds the license text of each vendored dependency, and `build_release.py` copies it into every archive, failing if the directory is absent. Archives carried only psnd's own `LICENSE` while the binaries link LGPL (Csound, FluidSynth, libsndfile, liblo) and GPL-2.0 (Ableton Link) code. zstd and JUCE ship no text in-tree and the mongoose GPL-2.0-only conflict still blocks the web variants; `docs/licenses/README.md` records both gaps (`build_release.py`)

## [0.2.2]

### Security

- **Web Host Access Control Reworked**: the bind address moved from the `PSND_WEB_BIND` environment variable to a `--web-host ADDR` flag, which warns when given anything but loopback. The token is now accepted as an `X-Psnd-Token` header as well as a `?token=` query parameter, is compared in constant time, and also gates static files served from `--web-root`. Cross-origin WebSocket handshakes are rejected on the `Origin` header, closing cross-site WebSocket hijacking: any page the user visited could previously connect to `ws://localhost:8080` and drive the editor (`host_web.c`, `host_web_ui.h`, `cli.c`)

- **`LUA_SANDBOX` Warns When Disabled With The Web Host**: configuring `-DLUA_SANDBOX=OFF -DBUILD_WEB_HOST=ON` now emits a CMake warning. Unsandboxed Lua exposes `os.execute` to anything that reaches the server (`CMakeLists.txt`)

### Added

- **MHS End-To-End Smoke Test**: `mhs_smoke_tests` runs a Haskell module through the psnd binary and checks its output, covering VFS init, the embedded `base` and `music` packages, compilation and evaluation. The two existing MHS tests exercise C entry points without ever starting the interpreter, so nothing verified that path (`source/langs/mhs/tests/smoke_test.cmake`, `Smoke.hs`)

- **`--web-open`**: hands the tokenized startup URL to the default browser (`open`, `xdg-open`, or `ShellExecuteA`) once the listener is bound. Off by default, since opening a browser is wrong for a headless or SSH session. The URL is passed as an argument vector rather than a shell string, and the POSIX path double-forks so the opener is never left as a zombie in a server that never waits (`host_web.c`, `cli.c`)

### Removed

- **`PSND_WEB_BIND`, `PSND_WEB_TOKEN` and `PSND_WEB_NO_AUTH` Environment Variables**: replaced by `--web-host ADDR`; the session token is always generated and cannot be disabled or fixed. Scripts that set these must pass `--web-host` and read the token from the startup URL instead (`host_web.c`)

### Fixed

- **`psnd <lang>` Stopped Delegating Its Arguments**: `85739c8` added a scan to the language-command branch of `main.c` that diverted the whole command line to the editor whenever any argument carried a registered extension. Every documented execute form -- `psnd alda song.alda`, `psnd bog song.bog`, `psnd joy song.joy`, `psnd tr7 song.scm` -- silently became an edit, and `psnd mhs -r file.hs` failed with `Unknown option: -r` because `-r` reached the editor's parser instead of MicroHs. The scan also matched any language's extensions, so `psnd joy song.alda` opened an Alda file under the `joy` subcommand. The branch delegates again: the language owns its flags and is the only thing that knows which take a file operand, and the editor is still reached by `psnd <file>` (`main.c`)

- **Test Harness Leaked Its stdin Into Child Processes**: `test_exec` and `test_exec_capture` redirected the child's stdout and stderr but not its stdin, so a test running a binary that reads stdin -- psnd enters a REPL after loading a file -- either hung or consumed the runner's input, depending on what stdin happened to be. Both now give the child an empty stdin: `/dev/null` on POSIX, a `NUL` handle on Windows. The Windows branch passed `NULL` for `hStdInput` under `STARTF_USESTDHANDLES`, which does not produce an empty stream -- the child fell back to the console `CREATE_NO_WINDOW` gave it, `isatty()` reported a terminal, and every `psnd <lang> <file>` test deadlocked until the runner's six-hour limit. `test_exec` also paired `STARTF_USESTDHANDLES` with `bInheritHandles = FALSE`, which does not deliver the handles at all (`test_process.h`)

- **Captured Test Output Truncated At The First Read**: `test_exec_capture` called `read`/`ReadFile` once, which returns as soon as any bytes are available, so output arriving in more than one chunk was cut short. Both branches now drain to EOF, discarding past the caller's buffer rather than leaving it in the pipe for the child to block against (`test_process.h`)

- **`lang_flag_taking_file_reaches_language_mhs` Asserted A Backend Windows Does Not Have**: MHS integration needs `fmemopen`, so Windows registers `mhs` with stub entry points. The test asserted the module ran and printed, which no stub can do. Reaching the language is the contract under test and still asserted everywhere; the exit code and output checks are gated on `PSND_MHS_ENABLED`, set after `add_subdirectory(langs)` because `core` is configured first and cannot read `ENABLE_MHS_INTEGRATION` (`test_main_dispatch.c`, `source/CMakeLists.txt`)

- **Tracker MIDI Export Truncated Paths Silently**: the bounded `snprintf` strip-then-append introduced in 0.2.0 still produced a wrong output path rather than an error when the source path did not fit, and could strip at a dot inside a directory name. Both call sites now share `derive_midi_path()`, which rejects an over-long path and only treats a dot in the final component as an extension (`tracker_view.c`)

- **Parallel Build Race in MHS**: `make -j` failed nondeterministically because the `mhs-embed` tool, `base.pkg` and `music.pkg` custom commands were duplicated into every consuming target's makefile and run concurrently over the same output paths (`Error 126`, or a failed `copy_directory` into `build/mhs-base-src`). Each shared step now has a single owning target, and consumers depend on that target and on the step's own inputs rather than naming its output file. Naming the output is what copies the recipe into the consumer's makefile, and ordering the consumer later does not stop it running: GNU make caches the stat taken when the submake starts, so a consumer that started before the owning target wrote the file re-runs the recipe anyway. That variant survived the first fix and truncated `music-0.1.0.pkg` to 0 bytes in the embedded header, giving a binary that failed only at run time with `/mhs-embedded/packages/music-0.1.0.pkg: openBinaryFile: does not exist`. `mhs-embed` now rejects an empty or unreadable package, so the same accident stops the build instead. This is the same hazard as the embedded-header race fixed in 0.2.1, on the steps feeding those generators (`source/langs/mhs/CMakeLists.txt`, `scripts/mhs-embed.c`)

- **REPL History Recall (remaining paths)**: the `ARROW_DOWN` branch in `repl_line_editor.c` and both branches of the fallback editor in `core/repl.c` still used unbounded `strcpy`; all are now bounded to `REPL_MAX_INPUT_LENGTH`, matching the `ARROW_UP` fix

- **Themes, Languages and Scales Not Installed**: `install()` matched only `*.lua`, so `cmake --install` shipped none of the 17 `.psnd/themes/*.toml`, `.psnd/languages/*.toml` or `.psnd/scales/*.scl` files despite them being advertised features (`CMakeLists.txt`)

- **Stale CMake Cache Across Build Variants**: each `configure-*` Makefile target now states the full option set, so switching variants in place (e.g. `make csound` then `make`) no longer silently inherits the previous variant's options. `configure-*` targets are also declared `.PHONY`

- **Out-of-Tree Test Fixtures**: `alda_microtuning_tests` hardcoded a data path relative to an in-tree `./build`, and `test_csound_microtuning.c` pointed at the pre-reorganization `tests/alda/data`. `TEST_DATA_DIR` is now supplied by CMake as an absolute path

### Changed

- **MicroHs Runtime No Longer Forked Into The Tree**: `source/langs/mhs/impl/` held 13 files copied from `source/thirdparty/MicroHs/src/runtime/`, of which 12 were byte-identical to upstream. Only `eval.c` differed, solely by renaming `mmalloc`/`mrealloc`/`mcalloc` to `mhs_*` to avoid a clash with Csound, which exports the same names. That rename moved into `mhs-patch-eval.py --rename-malloc`, beside the VFS patch the same script already applied at build time, and the copies are gone. The generated `eval_psnd.c` was diffed against the fork's output and is byte-identical, so no compiled code changed. The fork had to be re-synced by hand on every upstream bump; a bump is now a matter of replacing the vendored tree (`source/langs/mhs/CMakeLists.txt`, `scripts/mhs-patch-eval.py`)

- **MicroHs Version Read From `MicroHs.cabal`**: the version was pinned as a literal `0.15.0.0` in two places, and package paths (`base-<version>.pkg`, the mcabal install layout) are derived from it, so a bump meant editing both and finding out at link time if one was missed. It is now parsed once with a format check that fails configuration. A third pin, `MHS_VERSION_PSND`, was set and never read (`source/langs/mhs/CMakeLists.txt`)

- **CI Jobs And Tests Bounded By Timeouts**: no workflow job declared `timeout-minutes`, so a hung test ran to GitHub's six-hour ceiling -- at the 2x Windows billing rate, across seven variant jobs. All 11 jobs now carry one (30 min for `ci`, 45 for the build workflows, 60 for `release`), and every `ctest` invocation passes `--timeout 120` so a hang fails naming the test instead of killing the job blind. `cli_main_dispatch_tests` keeps a 300s ceiling of its own: it is 4-9s warm, but compiles the MicroHs base packages against a cold `.mhscache` (`.github/workflows/*.yml`, `scripts/build_release.py`, `source/core/tests/cli/CMakeLists.txt`)

- **Variant Matrix Runs Nightly**: `build-matrix.yml` gains a 04:00 UTC schedule, covering the variant and platform combinations the per-push `ci` workflow does not build (`.github/workflows/build-matrix.yml`)

- **`BUILD_FLUID_BACKEND` Declared**: the option was used in five files and passed by CI and the Makefile, but never declared with `option()`, so it was invisible to `ccmake`/`cmake-gui` (`CMakeLists.txt`)

## [0.2.1]

### Fixed

- **Windows Release Builds Failed On Two POSIX Dependencies**: All seven Windows legs of the release matrix failed to compile, on two tests that reached for POSIX interfaces MSVC does not provide. Both build cleanly on Linux and macOS, so neither showed up outside CI:

  - `test_csound_backend.c` timed its render-latency loop with `clock_gettime(CLOCK_MONOTONIC, ...)`. It now uses `uv_hrtime()`, which is monotonic on all three platforms and needs nothing new - the same test already linked libuv and used `uv_thread_create()` a few lines above (`test_csound_backend.c`)

  - `test_examples.c` walked the example corpus through `<dirent.h>`, which MSVC does not ship. Rather than exclude the test on Windows as `test_shared_suite.c` already was, directory iteration moved into `psnd_dirent.h`: a passthrough to `<dirent.h>` on POSIX, and a `FindFirstFileA` wrapper on Windows exposing `opendir`/`readdir`/`closedir` and `d_name`. Two near-identical copies of that shim were already inlined in the TOML theme and language loaders and now share the header (`psnd_dirent.h`, `test_examples.c`, `theme_toml.c`, `lang_toml.c`)

- **MHS Embedded Header Truncated Under A Parallel Build**: The `tsf-csound` Linux leg failed with `unterminated #ifndef` in the generated `mhs_embedded_pkgs_zstd.h`, having compiled `vfs.c` against a half-written file. The generator was not at fault: the header was listed as a custom command output consumed by two independent targets in the same directory - `mhs-midi-pkg-zstd` through its source list, and `mhs-psnd-embedded-header` through `mhs_runtime`. CMake's Makefile generator copies such a rule into every consuming target, so `make -j` ran the generator twice at once. This is the hazard `add_custom_command` documents: do not list an output in more than one independent target, drive it from an `add_custom_target()` and depend on that instead. Each of the four embedded headers now has one owning target, and the standalone variants take an ordering dependency on it rather than naming the file. Being a scheduling race it had passed on other legs and on earlier runs, which is why it surfaced only once (`langs/mhs/CMakeLists.txt`)

## [0.2.0]

### Fixed

- **Alda Parser Rejected Valid Scores**: Differential-testing the parser against [aldakit](https://pypi.org/project/aldakit/), a separate Alda implementation, over both projects' example corpora found that psnd rejected 9 of 40 bundled scores - including 8 of its own. Five distinct gaps, each now covered by a regression test:

  - A part declaration did not close a voice group. `parse_voice()` stopped only at another `V<n>:` or end of input, so the part name after a voice block was fed to the event parser and failed. An explicit `V0:` is now optional rather than required (`parser.c`)

  - A tie could not span a barline. `c4~|2`, `c4 |~2`, and a tie written on both sides across a line break (`d4.~4~|` continued by `|~4.~8`) are standard notation for a note held across a bar and all failed to parse. `parse_duration()` now scans past barlines, newlines, and further tildes when looking for the continuing duration, committing only once one is found so that a tilde before a note letter is still a slur (`parser.c`)

  - Fractional note lengths were unsupported. `scan_number()` consumed only digits, so `c0.25` - a quadruple-whole note - scanned as length `0`, an augmentation dot, then length `25`. Note-length denominators are now `double` from scanner through AST to the tick calculation, with a new `alda_duration_to_ticks_frac()`. A `.` joins the number only when a digit follows, so `c4.` remains a dotted quarter (`scanner.c`, `parser.c`, `ast.h`, `context.h`, `interpreter.c`, `scheduler.c`)

  - The dot accessor was unsupported. `strings.cello:` addresses one member of a group declared as `violin/viola/cello "strings"`. Identifiers may now contain a `.` when an identifier character follows it, and `alda_find_part()` resolves the `<group-alias>.<instrument>` form (`scanner.c`, `context.c`)

  - Banner comments scanned as sharp accidentals. `skip_whitespace()` treated `#` as a comment only when followed by whitespace or a letter, so a line of `########` became a run of accidentals. Alda spells sharp as `+` and never `#`, so `#` now always begins a comment and the `#`-as-sharp mapping is gone (`scanner.c`)

  With these fixed the two implementations agree on all 80 example files across both corpora. The comparison also turned up one fault on the other side - a token that could not be consumed inside an S-expression was retried rather than reported, so a multi-line attribute such as `(tempo!` / `120)` never terminated - which has been fixed in aldakit and recorded in that project's changelog. psnd already parsed that construct correctly, so nothing here changed for it.

  Two divergences are left after the interpreter work below, neither a psnd defect. `(tempo N)` written inside a part is treated as a global tempo change rather than a per-part one: events are scheduled on a single musical tick timeline with tempo as a scheduled event, which cannot express two parts running at different tempos at once. Making it per-part is an architectural change, not a fix. Separately, aldakit mis-resolves the key-name form when the tonic carries a spelled-out accidental. Two shapes share this syntax - `'(e flat minor)` names E-flat minor, while `'(e flat b flat)` sets E-flat and B-flat individually - and `theory.py:296` resolves the ambiguity by always choosing the per-note reading whenever the second symbol is `flat` or `sharp`. So `'(a flat major)` yields only "A is flat", discarding the mode, and `'(e flat minor)` flattens only E. Alda's own documentation, bundled in that project at `docs/alda-language/attributes.md:169`, gives `'(e flat minor)` as a key name. The trailing element is what distinguishes the two shapes: a scale or mode name means the list names a key. The named-key lookup on line 299 would not fix this on its own, since that table is keyed on the glued spelling (`"ab major"`), not `"a flat major"`. psnd handles all four documented shapes; both agree via the string form and on `'(g major)`.

- **Alda Interpreter Divergences**: Comparing scheduled MIDI events with aldakit across both example corpora found psnd playing the wrong notes for six constructs. All are now covered by regression tests in `test_interpreter.c`:

  - Key signatures written as a quoted list were mishandled in every form but `'(g major)`. `'(a flat major)` was read as tonic "a", mode "flat", discarding "major"; the lookup table spells tonics with the accidental attached, so accidental words are now folded into the tonic. The per-note forms - parenthesised `'(e (flat) b (flat))` and bare `'(e flat b flat)` - were not handled at all and now mean the same as the string form `"e- b-"`. The two shapes are told apart by their trailing element: a scale or mode name means the list names a key. Four minor keys missing from the lookup table (Eb, Ab, D#, A#) were silently ignored and have been added (`attributes.c`)

  - A sharp tonic inside an s-expression was split by the comment scanner. `'(c# minor)` scanned as symbol `c` followed by a comment that swallowed the rest of the line. `#` now continues a symbol already in progress, while a `#` starting a token remains a comment (`scanner.c`)

  - **A global `(key-sig! ...)` was discarded entirely.** The handler required a current part, and `visit_lisp_list` only evaluated attributes for active parts - so a directive at the top of a score, before any part is declared, never ran. Global key signatures are now stored on the context, applied to existing parts, and inherited by parts declared later; attributes with no active part are evaluated once with a NULL part so any global form survives (`attributes.c`, `context.c`, `interpreter.c`)

  - A group always created fresh parts, resetting their state. `guitar/sax:` after `electric-guitar-distorted "guitar": o2` played at the default octave instead of the octaves those parts had accumulated. Part resolution is now alias-aware: an aliased declaration names a distinct instance and matches only parts already carrying that alias, while an un-aliased one resolves to the instrument's own part or to a part with that alias (`context.c`)

  - **Notes were resolved from the first part of a group and played on all of them.** `visit_note`, `visit_rest` and `visit_chord` computed pitch, duration and velocity once from `alda_current_part()`, so every member of a group played the first member's notes regardless of its own octave, key signature, transposition or dynamics. All three now resolve per part (`interpreter.c`)

  - Crams had the same fault. `visit_cram` now runs the cram once per active part with that part temporarily the only one selected, which also keeps an octave change written inside a cram local to the part it belongs to (`interpreter.c`)

- **Alda Group Alias Applied To Only One Part**: `visit_part_decl()` set the alias on the first part of a declaration, so for a group such as `violin/viola/cello "strings"` the other members did not carry it. Every part in the declaration is now aliased, which is also what makes the dot accessor resolvable (`interpreter.c`)

- **REPL Tab-Completion Stack Overflow**: The linenoise completion adapter copied the word under the cursor into a `REPL_MAX_INPUT_LENGTH` (1024-byte) stack buffer, but linenoise passes the callback a `LINENOISE_MAX_LINE` (4096-byte) buffer. The length guard covered the `memcpy` and not the NUL terminator, so a word of 1024 characters or more wrote up to ~3KB past the buffer. Reachable in the default build (`WITH_LINENOISE` defaults `ON`) by pressing Tab after a long word, with no network peer or opt-in flag involved. Word extraction moved into a bounds-checked `repl_extract_completion_prefix()`, which declines to complete a word that cannot fit rather than truncating it - such a word would not survive `repl_readline()`'s truncation into the REPL buffer anyway (`repl_line_editor.c`, `repl.h`)

- **libuv Handles Mutated Off The Loop Thread**: `shared_async_play_ex()` called `uv_timer_start()` from the caller's thread while the event loop ran on its own thread, corrupting libuv's internal timer heap. Each playback slot gained a `start_async` handle, and the first timer delay is now computed under the mutex and the start handed to the loop thread, leaving `uv_async_send()` - the one libuv call that is safe cross-thread - as the only one made from outside the loop. `shared_async_stop()` and `shared_async_stop_all()` no longer write `stop_requested` directly either; they set a mutex-guarded pending flag that the loop thread acts on. Verified with helgrind: 3,745 races in 120 contexts before, 3 after, the remainder being the pre-existing `volatile sig_atomic_t` shutdown flags (`shared_async.c`)

- **Stale Async Stop Tearing Down A Recycled Slot**: `uv_async_send()` coalesces, so a stop signal could still be in flight when its slot had already finished and been handed to a new schedule, cancelling the new playback instead. Both the start and stop handlers now verify their pending flag under the mutex before acting (`shared_async.c`)

- **Realtime Audio Callback Blocking On A Compile**: `shared_csound_render()` runs on the miniaudio device thread and took a blocking lock held across `csoundPerformKsmps()`, so a control-thread compile stalled the audio thread - a priority inversion in which a non-realtime thread blocks a realtime one. It now try-locks and emits one period of silence on contention, a bounded and self-correcting glitch rather than a missed callback deadline. Measured on a 600-instrument orchestra, which holds the engine for roughly 30ms against a 23.2ms audio period: worst-case render call fell from 101ms to under 0.25ms (`csound_backend.c`)

- **Signal Handler Writing Non-Atomic State**: `playback_sigint_handler()` wrote `g_play.finished`, a plain `int`, which is undefined behaviour - a signal handler may only touch `volatile sig_atomic_t`. Both `finished` and `active` are now `volatile sig_atomic_t`, matching `g_interrupted` alongside them (`csound_backend.c`)

- **JSON Parser Stack Exhaustion**: `JsonParser` carried no depth counter, so a deeply nested document recursed once per nesting level and exhausted the stack. Nesting is now capped at `JSON_MAX_DEPTH` (64), checked at the single container recursion site in `parse_value()` so the accounting stays symmetric across the many error-return paths in `parse_object()`/`parse_array()`. Without the cap the new test suite segfaults on a 100k-deep input (`json.c`, `json.h`)

- **JSON Parser NULL Input**: `json_parse(NULL)` dereferenced NULL in `strlen()`. It now returns `JSON_ERROR` (`json.c`)

- **JSON Object Grow Double-Free**: `parse_object()` called `realloc` on both the key and value arrays before testing either result. When the first succeeded and the second failed, `keys` had already been freed by `realloc`, and the cleanup path then read through the dangling pointer and freed it a second time. Each reallocation is now adopted as it succeeds (`json.c`)

- **Unsized Web Host File Reads**: `ftell()` results fed `malloc()` and `fread()` unchecked in `handle_api_load()` and the WebSocket `load` command. On a non-seekable file (fifo, `/proc`, device) `ftell` returns `-1`, undersizing the buffer while `fread` read on to EOF. Both sites now reject a negative size before allocating, matching the existing `file_size <= 0` guards in `minihost_backend.c` and `serialize.c` (`host_web.c`)

- **Joy String Lexer Allocation Handling**: `lexer_read_string()` assigned the `realloc` result straight into its buffer, which both leaked the original block on failure and left a NULL to write through on the following line; the initial `malloc` was unchecked as well. Both now fail cleanly, which propagates safely because `joy_strdup()` is NULL-tolerant and the token cleanup already guards on the pointer (`joy_parser.c`)

### Changed

- **`ALDA_MAX_PARTS` raised from 64 to 256**: `examples/all-instruments.alda` names all 128 General MIDI programs plus `midi-percussion`, so it needs 129 parts and could be parsed but not interpreted. Parts are a fixed array inside `AldaContext`, so this grows the struct from 64KB to 147KB - large enough that callers must heap-allocate it rather than place it on the stack (`context.h`)

- **Temp Directory Cleanup Without A Shell**: `vfs_cleanup_temp()` now walks the tree with `nftw()` and `FTW_DEPTH | FTW_PHYS` instead of shelling out to `system("rm -rf ...")`, matching what the CLI test harness already did. `FTW_PHYS` keeps the walk from following symlinks out of the temp tree. The Windows branch still uses `rmdir /s /q`, following the same precedent (`vfs.c`)

### Added

- **Alda Example Corpus Test**: `test_examples.c` parses *and interprets* every score in `source/langs/alda/examples/` (40 files), asserting that each schedules events rather than parsing into silence. Nothing previously parsed that directory, which is how nine unparseable scores came to ship alongside the parser that could not read them. It is a backstop for grammar changes that break real music without tripping a construct-level unit test (`test_examples.c`)

- **Alda Parser, Scanner And Interpreter Regression Tests**: 15 parser tests, 5 scanner tests and 9 interpreter tests covering each construct above, plus the cases that must keep their old meaning - a tilde before a note letter is a slur, a trailing `.` is an augmentation dot, a bare `|` is still its own event (`test_parser.c`, `test_scanner.c`)

- **`alda_duration_to_ticks_frac()`**: Duration-to-ticks conversion accepting a fractional denominator. The existing integer entry point is kept and delegates to it (`scheduler.h`)

- **JSON Test Suite**: 54 tests for the previously untested `json.c`, covering the builder (escaping, nesting, growth past the initial capacity, reset, the error stub) and the parser (scalars, containers, capacity growth, the depth limit, twelve malformed-input cases, accessors, and a build-then-parse round trip). Valgrind-clean at 608 allocations and 608 frees. Several tests deliberately pin current lenient behaviour that callers depend on - `\uXXXX` decoding to `'?'` rather than the real codepoint, and numbers keeping only their integer part - so that changing it is a deliberate act (`test_json.c`)

- **REPL Line Editor Tests**: 17 tests for the completion prefix extractor, using guard-byte buffers around the output to catch overruns, with the boundary cases confirmed to fail against the pre-fix logic (`test_repl_line_editor.c`)

- **Async Playback Tests**: 9 tests that drive real schedules through the libuv loop thread - completion, stop, repeated stop, idle-slot stop, slot reuse after stop, concurrent slots, stop-all, 40 rapid play/stop cycles, and a stop deliberately racing the start signal. The existing tests stopped at argument validation and never reached the event loop (`test_shared_async.c`)

- **Csound Realtime Contention Tests**: 3 tests covering the render skip counter and the two contention properties, compiling on a background thread so the render calls genuinely race a held engine lock. Both contention tests were confirmed to fail against the previous blocking lock. They report and return rather than failing where no playback device is available (`test_csound_backend.c`)

- **`shared_csound_render_skip_count()`**: Reports how many audio buffers the render path filled with silence because the engine was busy, so the degradation around compiles is observable rather than silent. Reset when an orchestra is loaded (`audio.h`, `csound_backend.c`)

- **`JSON_MAX_DEPTH`**: Public constant documenting the parser's nesting limit (`json.h`)

- **`repl_extract_completion_prefix()`**: Bounds-checked word extraction shared by the completion paths, exposed so the boundary behaviour is testable (`repl.h`)

### Removed

- **`#` As An Alda Sharp Accidental** (breaking): `#` was accepted as a synonym for `+`, which forced the scanner to guess whether a given `#` opened a comment or an accidental. Alda spells sharp as `+` and reserves `#` for comments, so the synonym is gone and `#` always starts a comment. A score written against psnd that spelled an accidental `c#` must now spell it `c+`; `c#` reads as `c` followed by a comment.

- **Dead CMake Scripts**: Deleted `psnd_shared_library.cmake`, `psnd_loki_library.cmake`, `psnd_psnd_binary.cmake`, `psnd_tests.cmake`, `psnd_alda_library.cmake`, `psnd_joy_library.cmake`, and `psnd_bog_library.cmake`. None was referenced anywhere in the build, and they had already drifted - `psnd_loki_library.cmake` listed a `terminal.c` that was split into `terminal_posix.c` and `terminal_win.c` some time ago. Only `psnd_platform.cmake` and `psnd_languages.cmake` remain, and both are live (`scripts/cmake/`)

- **Redundant Playback Mutex**: Removed `g_play.mutex`, which was taken only by the audio callback itself. The state actually shared across threads - `finished` and `active` - was read and written entirely outside it, and one of those writers is a signal handler, where a mutex cannot be taken at all. Everything else in `PlaybackState` is published before `ma_device_start()` and torn down after `ma_device_stop()`, so the device lifecycle orders it (`csound_backend.c`)

### Documentation

- **Restored A Truncated Alda Example**: `examples/midi-channel-management.alda` was missing a `V0:` line present in the upstream score, leaving a voice group inside an unbalanced bracket sequence. psnd's own parser masked it by failing earlier on the banner comment. The line is restored, and the file now matches the upstream copy

- **Corrected The Add-A-Language Guide**: `docs/new_lang.md` documented the entire workflow against the dead CMake scripts above, and had contributors hand-editing `lang_config.h` and `lang_dispatch.c`, both of which are now generated from CMake and marked do-not-edit. A contributor following it would have edited four files that affect nothing and produced a language that silently did not build. Steps 5 through 8 and the key-files table were rewritten against the real `psnd_register_language()` auto-discovery; `scripts/new_lang.py` was already correct, only the prose was stale

- **Corrected The README Build Table**: Dropped the "(smallest)" label on `make psnd-tsf`, which contradicted the MHS variant table directly below it describing the same default build at ~5.7MB; every preset includes MHS, so it is the smallest backend choice rather than the smallest binary. Added the two previously undocumented minihost presets and a utility-target table covering `psnd`, `library`, `rebuild`, `reset`, `remake`, and `test-minihost`

- **Recorded The Project Trust Model**: `TODO.md` now states that psnd is a local single-user tool and that untrusted input is out of scope, with the reasoning and the conditions that would void it. The repository contradicted itself on this - the README says the web host should not be network-exposed without authentication, while the roadmap lists a multi-client web UI as a goal - so it could not be inferred from the code. The web host path sandbox is deprioritised accordingly and flagged as a prerequisite should the multi-client work be taken up

## [0.1.7]

### Security

- **Lua Sandbox On By Default**: The `LUA_SANDBOX` build option now defaults to `ON` (`CMakeLists.txt`). Because the editor auto-loads `.psnd/init.lua` and project Lua, an unsandboxed interpreter made opening any file or repository equivalent to arbitrary code execution. The sandbox disables `os`, `io`, `debug`, and `load`/`loadfile`/`dofile`; the bundled `init.lua` already guards these and degrades gracefully. Rebuild with `-DLUA_SANDBOX=OFF` to restore full access.

- **Shell Execution Primitives Gated**: Joy's `system` word is now disabled unless the binary is built with the new `-DPSND_ENABLE_SHELL=ON` option (default `OFF`). When disabled it raises an error instead of running a shell command, preventing untrusted music scripts from executing arbitrary commands (`joy_primitives.c`).

- **Web Host Binds Loopback By Default**: The optional web server now listens on `127.0.0.1` instead of `0.0.0.0` (`host_web.c`), so the filesystem-capable, unauthenticated editor is no longer exposed to the local network. Set `PSND_WEB_BIND=0.0.0.0` to deliberately expose it.

- **Web Host Token Authentication**: The web server now generates a random 128-bit access token at startup and requires it (as a `?token=` query parameter) on the WebSocket control channel (`/ws`) and the REST API (`/api/*`), which run code and read/write files. The HTML page and static assets are still served freely so the browser can bootstrap, and the page forwards the token from its URL to the WebSocket. The full tokenized URL is printed at startup. Set `PSND_WEB_TOKEN` to use a fixed token, or `PSND_WEB_NO_AUTH=1` to disable the check (`host_web.c`, `host_web_ui.h`).

### Fixed

- **Tracker MIDI Export Buffer Safety**: Replaced an unterminated `strncpy` into an uninitialized 256-byte buffer (out-of-bounds read on long `:export` arguments) and the `strcat`/`strcpy(dot, …)` extension-swap paths (up-to-4-byte overflow on near-256-char paths) with bounded `snprintf` strip-then-append logic (`tracker_view.c`)

- **Async Scheduler Data Races**: The cross-thread `running` and `shutdown_requested` flags are now `volatile sig_atomic_t` for indivisible access, and the non-reentrant `g_sort_by_ticks` global comparator state was removed in favor of two self-contained comparators selected at the `qsort` call site (`shared_async.c`)

- **Bog Allocation NULL Dereferences**: Fixed a latent NULL dereference in `bog_arena_alloc()` when `arena_block_create()` fails, and guarded every write site in `term_to_string_rec()`/`bog_term_to_string()` against allocation failure instead of dereferencing NULL (`bog.c`)

- **Joy String Primitive NULL Checks**: Added out-of-memory guards after the unchecked `malloc` calls in the `cons`, `concat`, `take`, and `enconcat` primitives, which previously wrote to a NULL pointer on allocation failure (`joy_primitives.c`)

### Added

- **`PSND_ENABLE_SHELL` Build Option**: Opt-in flag (default `OFF`) controlling whether language-level shell execution primitives are compiled in (`CMakeLists.txt`)

- **`PSND_WEB_BIND` Environment Variable**: Overrides the web host bind address (default `127.0.0.1`)

- **`PSND_WEB_TOKEN` / `PSND_WEB_NO_AUTH` Environment Variables**: Set a fixed web access token (e.g. for automation) or disable token authentication entirely (`host_web.c`)

- **Security Regression Tests**: A Joy test verifying that the `system` primitive raises an error when shell support is compiled out, and two tracker tests driving `:export` with an oversized `file_path` to exercise the MIDI-export filename buffer (catch overflow under AddressSanitizer) (`test_primitives.c`, `test_view.c`)

- **CI and Release Automation**: A `ci` workflow builds and tests psnd on macOS (arm64), Linux (x86_64), and Windows (x86_64) on every push and pull request to `main`; a tag-triggered `release` workflow builds each platform, packages versioned archives, and publishes a GitHub release with notes extracted from this CHANGELOG. Both drive a new `scripts/build_release.py` (configure -> build -> ctest -> smoke -> package) as the single build entry point, with `scripts/release_notes.py` extracting the per-version notes (`.github/workflows/ci.yml`, `.github/workflows/release.yml`, `scripts/`)

### Documentation

- **Corrected Source Paths and Language Counts**: Rewrote `CLAUDE.md` (stale `src/` tree and 2-language / line-count claims), resolved the README's "five languages" vs "four fully integrated" contradiction in the Status section with an accurate per-language maturity description, and fixed the outdated `src/lang/...` paths in `docs/new_lang.md` to match the real `source/langs/` and `source/core/` layout

## [0.1.6]

### Fixed

- **Alda Parser EOF Safety**: `peek()` now returns a static EOF sentinel token instead of NULL, eliminating a class of NULL pointer dereference bugs when parsing truncated or malformed input (lines 288, 534 of `parser.c`)

- **Joy Dictionary SEQ Leak**: `joy_dict_set()` now frees sequence (SEQ) definitions when replacing an existing word, matching the cleanup logic already present in `joy_dict_remove()`

- **Alda Repetition Overflow**: Repetition counts in `parse_rep_spec()` are now capped at 10,000 to prevent integer overflow from malformed input

- **Joy String Primitive Safety**: Replaced unbounded `strcpy`/`strcat` with `memcpy` using pre-computed lengths in string cons and concatenation primitives (`joy_primitives.c`)

- **REPL History Recall Safety**: Replaced unbounded `strcpy` with `strncpy` bounded to `REPL_MAX_INPUT_LENGTH` in the non-linenoise fallback path (`repl_line_editor.c`)

### Added

- **Alda Parser Fuzz Tests**: 26 truncation fuzz tests that parse valid Alda strings at every byte truncation point, asserting no crashes (`test_parser_fuzz.c`)

## [0.1.5]

### Added

- **Split Window Manager**: Vim-style binary tree tiling window manager for the editor

  - `Ctrl+W s` - Split horizontally (top/bottom)

  - `Ctrl+W v` - Split vertically (left/right)

  - `Ctrl+W h/j/k/l` - Navigate between panes (left/down/up/right)

  - `Ctrl+W c` - Close current pane

  - `Ctrl+W w` - Cycle to next pane

  - `Ctrl+W =` - Equalize split ratios

  - `Ctrl+W n` - New empty buffer in current pane

  - `Ctrl+W b` - Cycle through buffers in current pane

  - Per-pane view state: independent cursor position, scroll offset, and selection per pane

  - Visual focus indicator: active pane highlighted with yellow gutter and separator

  - Different buffers per pane: each pane can show a different file

  - Binary tree layout with configurable split ratios (default 50/50)

  - New files: `source/core/loki/window.h`, `source/core/loki/window.c`

  - 29 unit tests for window tree operations

- **Tracker Terminal View API**: New function for creating terminal views with custom file descriptors

  - `tracker_view_terminal_new_with_config_and_fds()` sets fds before initialization

  - Prevents escape sequences from being written to stdout during view creation

  - Useful for testing and embedding the tracker in non-terminal contexts

### Fixed

- **Tracker Terminal Tests**: Fixed escape sequences corrupting CTest output

  - Terminal view tests now redirect output to `/dev/null` during initialization

  - Prevents VT100 escape sequences (alternate screen, cursor hide) from garbling test output

### Changed

- **MHS/MicroHs**: Disabled MHS language integration on Windows

  - The VFS embedding system requires `fmemopen()` which is not available on Windows

  - MHS integration remains fully functional on Linux and macOS

  - Stub functions are provided on Windows so psnd builds without errors

### Added

- **Tree-sitter Prolog Grammar for Bog**: Integrated tree-sitter-prolog for syntax highlighting

  - Bog files (`.bog`, `.pl`) now use tree-sitter-based highlighting instead of keyword-based

  - Highlights: comments, atoms, variables, function calls, numbers, operators, strings, punctuation

  - Grammar located at `source/thirdparty/tree-sitter-grammars/tree-sitter-prolog/`

- **Tempo Tap and Metronome**: New commands for tempo control and beat reference

  - `:tap` - Tap multiple times to set tempo from interval (averages up to 8 taps, 2s timeout)

  - `:metronome [on|off|1-8]` - Toggle metronome with optional subdivisions (quarter/eighth/sixteenth)

  - Beat-synced via Ableton Link, plays drum sounds (kick for downbeat, hi-hat for subdivisions)

  - Stopped automatically by `:stop` command

- **Enhanced Playback Status Bar**: Real-time playback information display

  - Shows bar.beat position and tempo (e.g., `3.2 120BPM`) when playing or metronome active

  - Displays Link peer count (e.g., `[2P]`) when connected to other Link apps

  - Status indicators: `[PLAYING]`, `[METRONOME]`, or `[PLAY+MET]` for combined

- **Alda Source Line Tracking**: Infrastructure for playback visualization

  - Events store source line numbers for future highlight-during-playback feature

  - `ALDA_SOURCE_TRACKING` compile option (ON by default)

  - `ALDA_SET_SOURCE_LINE(ctx, line)` macro for setting source context

  - Instrumented in interpreter: notes, chords, cram blocks, lisp expressions

  - Zero overhead when disabled (compiles to no-op)

- **Shared Async Source Tracking**: Language-agnostic playback visualization infrastructure

  - `SharedAsyncEvent` now stores `source_line` field for all languages

  - `SHARED_SOURCE_TRACKING` compile option (ON by default)

  - `SHARED_SET_SOURCE_LINE(sched, line)` macro for setting context before scheduling

  - `shared_async_get_current_source_line(slot_id)` to query currently playing line

  - Extended scheduling functions (`_ex` variants) for explicit source line

  - Enables Joy, Bog, TR7 to add source tracking when their parsers support it

- **Joy Source Line Tracking**: Parser and scheduler now track source lines

  - Lexer tracks line/column during tokenization

  - `Token` stores `source_line` (1-based)

  - `JoyContext` has `current_source_line` for execution context

  - `ScheduledEvent` and `MidiSchedule` store source line

  - Source lines propagate through `joy_async_play()` to shared async

- **TR7 Async Source Tracking API**: Extended async API with source line support

  - Added `tr7_async_play_note_ex()`, `tr7_async_play_chord_ex()`, `tr7_async_play_sequence_ex()`

  - Added `tr7_async_get_current_source_line()` for playback visualization

  - Source tracking available via `_ex` variants when `SHARED_SOURCE_TRACKING` enabled

  - Note: TR7 interpreter doesn't currently expose source positions to primitives

- **Bog Source Line Tracking**: Complete source tracking for Bog playback visualization

  - Tokenizer tracks line numbers (1-based) per token during lexical analysis

  - `BogClause.source_line` stores the line where each clause is defined

  - `BogSolutions.source_lines` array tracks which clause produced each solution

  - Resolution propagates source line through clause matching

  - `BogScheduler.current_source_line` updated when triggering voices

  - `bog_scheduler_get_current_source_line()` API for external queries

  - `bog_async_get_current_source_line()` for async layer access

  - `LokiLangOps.get_source_line` callback added to language bridge interface

  - `loki_lang_get_source_line()` convenience function for editor queries

  - New test: `resolution_tracks_source_lines` verifies clause and solution tracking

  - Completes source tracking for all music languages (Alda, Joy, TR7, Bog)

- **Playback Line Highlighting**: Visual feedback during music playback

  - Currently playing source line highlighted in editor during async playback

  - Line gutter shows `>` indicator in bright green for the playing line

  - Row background highlighted with dark green (256-color palette: 22)

  - Works with all music languages: Alda, Joy, TR7, and Bog

  - `EditorView.playing_line` field added for tracking (1-based, 0=none)

  - `RenderSegment.is_playing` field added for renderer styling

  - Editor queries `loki_lang_get_source_line()` first, then `shared_async_get_current_source_line()`

- **Link Transport Sync Mode**: Optional mode for synchronized playback with Link peers

  - `:link transport [on|off]` command to enable/disable transport sync

  - When enabled, Link transport state controls buffer playback:

    - Transport start (from Ableton Live, etc.) plays the current buffer

    - Transport stop halts all playback

  - Separate from basic Link tempo sync - can enable tempo sync without transport sync

  - Uses `loki_link_set_transport_sync()` and `loki_link_is_transport_sync_enabled()`

  - Leverages existing Link start/stop sync protocol via `abl_link_enable_start_stop_sync()`

  - **Armed indicator**: Status bar shows `[ARMED]` when transport sync is enabled but waiting for Link start

    - `StatusInfo.transport_armed` field added for renderer integration

    - Helps users see when editor is ready to respond to Link transport

- **Test Framework Expansion**: Enhanced assertion macros and test infrastructure

  - Comparison macros: `ASSERT_GT`, `ASSERT_LT`, `ASSERT_GTE`, `ASSERT_LTE`

  - Test fixtures: `FIXTURE`, `FIXTURE_SETUP`, `FIXTURE_TEARDOWN`, `TEST_F`, `RUN_TEST_F`

  - Suite-level fixtures: `SUITE_SETUP`, `SUITE_TEARDOWN`, `BEGIN_TEST_SUITE_WITH_FIXTURE`

- **Windows/MSVC Native Support**: Full cross-platform compatibility for Windows builds

  - MSVC C-mode atomic handling via volatile semantics (`ASYNC_ATOMIC_SIZE_T`, `PARAM_ATOMIC_FLOAT`)

  - POSIX function implementations: `gettimeofday()`, `getline()`, `strndup()`, `usleep()`

  - Windows directory traversal via minimal `dirent.h` compatibility layer (`opendir`/`readdir`/`closedir`)

  - Windows console raw mode for terminal UI (`ENABLE_VIRTUAL_TERMINAL_INPUT/PROCESSING`)

  - pthread-to-Windows threading: `CRITICAL_SECTION` for mutexes, `_beginthreadex` for threads

  - Signal handling via `SetConsoleCtrlHandler()` for Ctrl-C interrupts

  - Home directory detection via `USERPROFILE` environment variable

  - Conditional math library linking (`-lm` only on non-Windows)

  - Windows macro conflict resolution (MOD_SHIFT, MOD_CONTROL, MOD_ALT from winuser.h)

- **Tracker Terminal View Windows Console**: Native Windows terminal support for tracker mode

  - Windows console input handling via `ReadConsoleInputA` and virtual key codes

  - Console screen buffer info for terminal size detection

  - VT100/ANSI escape sequence processing enabled via console mode flags

- **Memory Leak Detection**: Allocation tracking for test suites (`test_memcheck.h`)

  - Tracked allocation macros: `MEMCHECK_MALLOC`, `MEMCHECK_FREE`, `MEMCHECK_REALLOC`, `MEMCHECK_CALLOC`, `MEMCHECK_STRDUP`

  - Session management: `memcheck_init()`, `memcheck_begin()`, `memcheck_end()`, `memcheck_report()`

  - Statistics: `memcheck_current_bytes()`, `memcheck_peak_bytes()`, `memcheck_leak_count()`

  - Test integration: `ASSERT_NO_LEAKS()`, `RUN_TEST_MEMCHECK()`, `BEGIN_TEST_SUITE_MEMCHECK()`

  - 12 self-tests verifying leak detection functionality

- **Process Execution Utilities**: Fork/exec helpers for CLI integration tests (`test_process.h`)

  - `test_exec()` - Execute binary with arguments via fork/execve

  - `test_exec_capture()` - Execute and capture stdout/stderr

  - `test_mkdtemp()`, `test_rmdir_recursive()` - Temp directory management

  - `test_write_file()` - Create test fixture files

- **REPL Skeleton Module**: Callback-based infrastructure for language REPLs (`repl_helpers.h`)

  - `ReplSkeletonConfig` struct with pluggable command/eval callbacks

  - `repl_skeleton_run()` handles: interactive vs pipe mode, line editor, history, raw mode

  - Joy REPL refactored as proof-of-concept (~50 lines reduced to config struct)

  - Eliminates ~150 lines of duplicated loop code per language

- **Configurable Playback Commands**: `eval_line` and `play_file` now in keybind system

  - `cmd_eval_line()` - Evaluate selection or current Alda part (Ctrl-E)

  - `cmd_play_file()` - Play entire buffer (Ctrl-P)

  - `cmd_quit_keybind()` - Quit command for keybind mapping

  - All commands configurable via TOML `[keybindings]` section

### Changed

- **Language REPLs Cross-Platform**: All music languages now support Windows builds

  - Alda, Bog, Joy, and TR7 REPLs use platform-abstracted threading and timing

  - Async playback uses Windows `_beginthreadex`/`CRITICAL_SECTION` on Windows

  - High-resolution timing via `QueryPerformanceCounter` on Windows

  - Ctrl-C interrupt handling via `SetConsoleCtrlHandler` on Windows

- **CLI Tests Refactored**: Replaced `system()` shell spawning with direct `fork`/`execve`

  - Eliminates shell injection risks in test code

  - Uses `test_process.h` utilities for process execution

  - Uses `mkdtemp()` and `nftw()` for temp directory management

- **CMake Platform Logic Centralized**: New `scripts/cmake/psnd_platform.cmake` module

  - `psnd_platform_link_audio_midi()` - Platform-specific audio/MIDI libraries

  - `psnd_platform_add_warnings()` - Compiler warning flags

  - `psnd_platform_add_math()` - Math library linking

  - `psnd_platform_add_pthread()` - Thread library linking

  - Updated core, alda, joy, and bog CMakeLists.txt to use centralized functions

- **Modal Command Dispatcher Refactored**: Hardcoded commands moved to keybind system

  - Removed duplicate CTRL_E/CTRL_P handlers from `process_normal_mode()` and `process_insert_mode()`

  - Commands now dispatched via `keybind_try_handle()` before mode-specific handlers

  - Enables user remapping of playback commands via config.toml

- **README Prerequisites Section**: Added system dependency documentation

  - Documents required packages: `cmake`, `flex`, `bison`

  - Installation instructions for macOS (Homebrew), Debian/Ubuntu, Fedora/RHEL, Arch Linux

  - Notes that `flex` and `bison` are required for MHS (Micro Haskell) language component

### Fixed

- **Test Framework Build Fix**: Resolved multiple definition error for `test_stats`

  - Removed duplicate `test_stats` definition from `test_memcheck_selftest.c`

  - Variable is already defined in `test_framework.c` and declared as `extern` in header

- **CLI Test Build Fix**: Fixed `nftw()` availability for POSIX compliance

  - Added `_XOPEN_SOURCE 500` to `test_play_command.c` before system includes

  - Enables `nftw()`, `FTW_DEPTH`, and `FTW_PHYS` constants used by `test_process.h`

- **Linux Build Compatibility**: Resolved header and library issues for Linux builds

  - Added missing `#include <string.h>` in command modules (csd.c, link.c)

  - Fixed `strcasecmp`/`strncasecmp` availability via `<strings.h>` on POSIX systems

  - Conditional math library linking in CMake (`-lm` only when needed)

- **Cross-Platform Test Infrastructure**: Test utilities now work on both POSIX and Windows

  - Test directory paths use platform-appropriate locations (`/tmp` vs `C:\Temp`)

  - File access macros (`F_OK`, `R_OK`) defined for Windows

  - pthread compatibility layer for Windows in async queue tests

## [0.1.4]

### Added

- **Modal Picker**: Full-screen selection UI for choosing items from lists

  - Integrated with editor modes as `MODE_PICKER`

  - Vim-style navigation: `j/k` or arrow keys to move, `ENTER` to select, `ESC/q` to cancel

  - Additional keybindings: `g/G` (top/bottom), `CTRL_D/U` (page down/up)

  - Callback-based API for selection notification

  - Scroll support for long lists

  - Renderer-agnostic implementation (terminal + null renderers)

  - Used by `:theme` (when no args) and `:plugin presets` commands

- **Buffer Manager Service**: Injectable buffer management for testability

  - New `buffer_manager_t` struct encapsulates all buffer state

  - Explicit-manager API variants (`buffer_create_in()`, `buffer_switch_in()`, etc.)

  - Global `g_buffer_manager` for backwards compatibility

  - Enables isolated buffer managers for unit testing

- **TOML Configuration System**: Safe, declarative configuration via `.psnd/config.toml`

  - Settings for theme, line numbers, tab width, audio backend, soundfont, and keybindings

  - TOML themes in `.psnd/themes/*.toml` with 51 semantic color types (full tree-sitter support)

  - All 17 themes converted to TOML format (nord, dracula, monokai, gruvbox-dark, catppuccin, etc.)

  - Config loading order: project-local `.psnd/config.toml` > home `~/.psnd/config.toml` > defaults

  - Built on [tomlc99](https://github.com/cktan/tomlc99) (MIT license, ~1000 LOC)

- **Configurable Keybindings**: Command dispatcher for TOML-defined keybindings

  - Define keybindings in `[keybindings]` section of config.toml

  - Available commands: `save`, `find`, `stop`, `lua_repl`, `new_buffer`, `copy`, `word_wrap`

  - Custom keybindings override defaults (e.g., `"ctrl-w" = "save"`)

  - Built-in defaults preserved for unbound keys

  - Extensible: `keybind_register()` API for adding custom commands

- **Opt-in Lua Scripting**: Lua is now disabled by default for improved security

  - Enable with `editor.lua.enabled = true` in config.toml

  - When disabled: core features (Alda, Joy, Bog, themes, keybindings) work normally

  - When enabled: full Lua API, custom commands, `.psnd/init.lua` scripting

  - All Lua-dependent code paths gracefully degrade when Lua is disabled

- **TOML Language Definitions**: Syntax highlighting without Lua via `.psnd/languages/*.toml`

  - Language definitions include extensions, keywords, comments, and separators

  - Primary keywords (keyword1) and secondary keywords (keyword2) support

  - Loaded automatically at startup from project-local and home directories

  - Alda language definition included as reference example

- **Tree-sitter Syntax Highlighting**: Real-time syntax highlighting in REPLs and editor

  - Language-specific grammars: Alda, Joy, Scheme, Haskell, Lua, Csound

  - 51 highlight types for full tree-sitter semantic granularity (functions, variables, keywords, types, operators, etc.)

  - 17 TOML themes in `.psnd/themes/` with true RGB colors

  - `:theme` command loads themes (TOML > Lua > C built-ins)

  - `:theme NAME` to switch to a specific theme (applies to all open buffers)

  - Custom themes: create `.psnd/themes/<name>.toml` with `[colors]` section

  - Theme colors apply to REPL input highlighting

  - Build with `-DWITH_LINENOISE=ON` (enabled by default)

- **SQLite FTS5 Search Plugin**: Full-text search index for `.psnd/` configuration files

  - Fast content search across Lua modules, themes, scales, and compositions

  - Path glob search for finding files by name pattern

  - Incremental indexing (only re-indexes changed files based on mtime)

  - FTS5 query syntax support (phrases, AND/OR, prefix matching)

  - Lua API: `loki.fts.search()`, `loki.fts.find()`, `loki.fts.index()`, `loki.fts.stats()`

  - Ex-commands: `:search`, `:find`, `:reindex`, `:rebuild-index`, `:index-stats`

  - Requires system SQLite3 with FTS5 support (standard on macOS, most Linux)

  - Build with `-DBUILD_PLUGIN_SQLITE=ON`

- **Tracker Sequencer**: MIDI tracker/step sequencer with plugin-based cell evaluation

  - **Core Components**:

    - `tracker_model` - Data structures (TrackerSong, TrackerPattern, TrackerTrack, TrackerCell, TrackerPhrase, TrackerEvent)

    - `tracker_plugin` - Plugin system for different notation languages with capability flags (EVALUATE, VALIDATION, TRANSFORMS)

    - `tracker_engine` - Playback engine with event queue, active note tracking, and transport controls

    - `tracker_view` - View layer with theme, undo, clipboard, and JSON serialization

  - **Notes Plugin** (`tracker_plugin_notes`):

    - Simple note notation: `C4`, `D#5`, `Bb3`, `F##2`

    - Velocity: `C4@100` or `C4 v100`

    - Gate/duration: `C4~2` (2 rows)

    - Chords: `C4 E4 G4` or `C4,E4,G4`

    - Rest: `r` or `-`

    - Note-off: `x` or `off`

    - Transforms: `transpose`/`tr`, `velocity`/`vel`, `octave`/`oct`, `invert`/`inv`

  - **Audio Integration** (`tracker_audio`):

    - `tracker_audio_connect(engine, ctx)` - Wire engine to SharedContext for audio output

    - `tracker_audio_disconnect(engine)` - Disconnect and send all notes off

    - `tracker_audio_engine_new(ctx)` - Create engine already connected to context

    - `tracker_audio_enable_link_sync(engine, ctx)` - Enable Ableton Link tempo/transport sync

    - `tracker_audio_link_poll(engine, ctx)` - Poll Link state for updates

    - Automatic channel mapping (tracker 0-based to shared 1-based)

    - Routes through shared backend priority (Minihost > Csound > TSF/FluidSynth > MIDI)

    - Automatic program change initialization on playback start (required for TSF/FluidSynth)

  - **Terminal UI** (`tracker_view_terminal`):

    - VT100/ANSI terminal rendering with box-drawing characters

    - Vim-style navigation (h/j/k/l or arrow keys)

    - Cell editing with Enter/i to enter edit mode, Escape to exit

    - Playback control with Space (play/stop)

    - Row highlighting during playback

    - Beat row highlighting (every 4th row by default)

    - Track headers with names and channel numbers

    - Configurable cell width and colors

    - True color (24-bit RGB) support for themes

  - **File I/O**:

    - Save songs with Ctrl+S (JSON format, `.trk` extension)

    - Load songs with Ctrl+O (opens file picker prompt)

    - Command line file loading: `./tracker_demo [soundfont.sf2] [song.trk]`

    - JSON serialization preserves all song data (name, BPM, patterns, tracks, cells)

    - JSON parsing using loki's built-in JSON parser

    - Modified indicator tracks unsaved changes

  - **Theme System**:

    - Two built-in themes: default (dark) and retro (classic tracker colors)

    - Theme cycling with 'T' key

    - Supports indexed colors and RGB colors

    - Customizable colors for cursor, selection, headers, cells, playback position

  - **Track Mute/Solo**:

    - 'm' key mutes/unmutes current track (silences immediately)

    - 'S' key solos/unsolos current track (only soloed tracks play)

    - Track headers display [M], [S], or [MS] indicators

    - Muted/soloed tracks styled with distinct theme colors

    - State persisted in JSON save files

  - **Visual Selection Mode**:

    - 'v' key enters visual selection mode (vim-style)

    - Arrow keys or h/j/k/l extend selection while in visual mode

    - Shift+arrows also extend selection (alternative method)

    - Escape exits visual mode and clears selection

    - Status bar shows "VISUAL" mode indicator

    - Selected cells highlighted with selection theme style

  - **Clipboard Operations**:

    - 'y' key copies selection (or current cell if no selection)

    - 'd' key cuts selection (copy + clear)

    - 'p' key pastes at cursor position

    - 'x' key clears current cell

    - Status messages confirm operations ("Copied N cells", "Pasted", etc.)

    - Works with single cells and multi-cell rectangular selections

  - **Pattern Management**:

    - '[' and ']' keys navigate between patterns (with wrap-around)

    - 'n' key creates new empty pattern (inherits track layout from current)

    - 'c' key clones current pattern with all cell content

    - 'D' key (Shift+d) deletes current pattern (cannot delete last pattern)

    - Status bar shows current pattern number (e.g., "Pattern 2/4")

    - All changes sync with playback engine and mark song as modified

  - **Row Operations**:

    - 'o' key inserts empty row at cursor (shifts rows down)

    - 'O' key duplicates current row (inserts copy above)

    - 'X' key deletes current row (shifts rows up)

    - All operations record undo history and mark song as modified

  - **Track Operations**:

    - 'a' key adds new track to pattern (auto-assigns MIDI channel)

    - 'A' key removes current track from pattern

    - Cannot remove the last track (minimum 1 track per pattern)

    - Maximum 64 tracks per pattern

    - New tracks auto-assigned incrementing MIDI channels (1-16)

  - **Help Screen**:

    - '?' key shows comprehensive keybindings help

    - Lists navigation, editing, selection, track, pattern, playback, and file commands

    - Press any key to return to pattern view

  - **Step Size**:

    - '+' / '-' keys adjust step size (0-16 rows)

    - Step size determines cursor advance after note entry

    - Step 0 = no advance, Step 1 = advance one row (default)

    - Displayed in status bar: "Step:N"

  - **Default Octave**:

    - '>' / '<' keys (or '.' / ',') adjust default octave (0-9)

    - Default is octave 4 (middle C range)

    - Displayed in status bar: "Oct:N"

  - **Follow Mode**:

    - 'f' key toggles cursor following playback position

    - Status message shows "Follow: ON/OFF"

  - **Loop Mode**:

    - 'L' key toggles loop on/off

    - Playback loops within current pattern boundaries

    - Status bar shows [LOOP] indicator when active

  - **BPM Control**:

    - '{' / '}' keys decrease / increase BPM by 5

    - BPM range: 20-300

    - Changes apply immediately to playback and update song

    - BPM always displayed in status bar

  - **MIDI Export**:

    - 'E' or Ctrl+E exports song to Standard MIDI File

    - Exports all patterns, respects track mute/solo state

    - Converts cell expressions to MIDI events via plugin evaluation

    - Proper note timing with gate lengths for note-off events

    - Tempo embedded in MIDI file header

    - Type 0 (single track) or Type 1 (multi-track) based on channel usage

    - Output filename derived from song name or file path (replaces .trk with .mid)

  - **MIDI Import**:

    - `:import file.mid` command imports Standard MIDI Files

    - Automatic channel-to-track mapping

    - Note quantization to tracker rows based on song timing settings

    - Tempo extraction from MIDI file

    - Velocity preservation in cell expressions (e.g., `C4@100`)

    - Gate/duration encoding for long notes (e.g., `C4~4` for 4-row duration)

    - Polyphony support (multiple notes at same position as chord)

    - Configurable options: rows_per_beat, pattern_rows, quantize_strength

    - Creates sequence from imported patterns automatically

  - **Phrase System**:

    - Named reusable phrases that can be referenced in cells

    - `:phrase name expr` command defines a phrase (e.g., `:phrase intro C4 E4 G4`)

    - `:phrase name` shows phrase content

    - `:phrases` lists all defined phrases

    - `:delphrase name` deletes a phrase

    - Use `@name` in any cell to reference a phrase (e.g., `@intro`)

    - Phrases can reference other phrases (with recursion detection)

    - Phrases saved/loaded with song JSON

    - Maximum recursion depth of 16 prevents infinite loops

  - **Pattern Sequence/Arrangement**:

    - 'r' key enters Arrange mode to edit the song sequence

    - Navigate sequence with j/k or arrow keys

    - 'a' adds current pattern to sequence

    - 'x' removes entry from sequence

    - 'K' / 'J' (shift+k/j) moves entry up/down in sequence

    - Enter jumps to the pattern at cursor position

    - Escape returns to pattern view

    - Sequence displayed with pattern numbers, names, and row counts

    - Scroll indicator shows position in long sequences

  - **Undo/Redo System**:

    - 'u' or Ctrl+Z to undo, 'R' or Ctrl+Y to redo

    - Status messages show what action was undone/redone

    - Cell edits, row operations, clipboard operations all recorded

    - Grouped operations (paste, cut, clear selection) undo as single action

    - Cursor position restored when undoing

    - "Nothing to undo/redo" feedback when stack is empty

  - **Song Mode Playback**:

    - 'P' key toggles between Pattern mode and Song mode

    - Pattern mode (default): loops current pattern

    - Song mode: plays through the sequence in order

    - Status bar shows [PAT] or [SONG] indicator

    - Automatically stops at end of sequence in song mode

    - Requires sequence entries (use 'r' to enter arrange mode, 'a' to add patterns)

  - **Command Mode**:

    - Press ':' to enter command mode (vim-style)

    - Full command line editing (cursor, backspace, delete)

    - Commands: :w (save), :q (quit), :wq (save+quit), :q! (force quit)

    - :bpm N - set tempo (20-300)

    - :rows N - resize current pattern (1-256 rows)

    - :export [filename] - export to MIDI file

    - :set step N - set step size (0-16)

    - :set octave N - set default octave (0-9)

    - :set follow on/off - toggle follow mode

    - :set loop on/off - toggle loop mode

    - :set swing N - set swing amount (0-100%)

    - :name [text] - set pattern name

    - :help - show help screen

  - **Pattern Length**:

    - :rows command resizes current pattern dynamically

    - Cells are preserved when shrinking/growing

    - Cursor automatically adjusted if beyond new length

  - **Repeat Count**:

    - In arrange mode, +/- adjusts repeat count for sequence entries

    - Repeat count shown as "xN" in sequence list

    - Range: 1-99 repeats per entry

  - **MIDI Input**:

    - MIDI note callback system for external controller input

    - Note-on messages routed to tracker when in record mode

    - Callback registration via shared_midi_set_note_callback()

  - **Record Mode**:

    - Ctrl+R toggles record mode

    - Status bar shows [REC] indicator (red) when active

    - MIDI note-on messages are recorded to current cell

    - Cursor advances by step size after each note

    - Notes converted to expression format (e.g., "C4", "D#5")

    - Full undo support for recorded notes

  - **Swing/Groove**:

    - Engine supports swing_amount setting (0-100%)

    - :set swing N command to configure

    - 50% = straight timing, higher = more swing

  - **FX Chains UI**:

    - 'F' key enters FX edit mode

    - Edit FX chains at three levels: Cell, Track, or Master

    - 'c' / 't' / 'm' keys switch between FX targets

    - 'a' key adds new effect to chain

    - 'x' key removes current effect

    - 'K' / 'J' keys move effect up/down in chain

    - Space toggles effect enabled/disabled

    - Available FX: transpose, velocity, arpeggio, delay, ratchet, octave, humanize, chance, reverse, stutter

    - Esc returns to pattern view

  - **FX Parameter Editing**:

    - Enter, 'i', or 'e' key to edit selected FX

    - Tab switches between name and params fields

    - Up/Down arrows cycle through FX types when editing name

    - Enter saves changes, Esc cancels

    - Inline text editing with cursor movement (left/right, home/end, backspace/delete)

  - **FX Processing Runtime**:

    - Full FX chain processing during playback at cell, track, and master levels

    - 11 transform types with parameter support:

      - `transpose`/`tr` - Transpose by semitones

      - `velocity`/`vel` - Set velocity (0-127)

      - `octave`/`oct` - Shift by octaves

      - `invert`/`inv` - Invert around pivot note

      - `arpeggio`/`arp` - Spread chord notes across time

      - `delay` - Echo/delay effect (time,count,decay)

      - `ratchet`/`rat` - Rapid note repeats

      - `humanize`/`hum` - Random timing/velocity variation

      - `chance`/`prob` - Probability-based triggering (0-100%)

      - `reverse`/`rev` - Reverse note order

      - `stutter`/`stut` - Repeat phrase with velocity decay

  - **Mixer View**:

    - 'M' key enters mixer mode

    - Per-track volume control (0-127, default 100)

    - Per-track pan control (-64 to +63, 0 = center)

    - Mute/solo toggles with visual indicators

    - Arrow keys navigate tracks and fields

    - +/- adjust volume and pan values

    - 'm' and 'S' toggle mute/solo directly

    - 'x' resets selected field to default

    - Volume meter visualization per track

    - Horizontal scrolling for many tracks

    - Volume scales note velocity during playback

    - Pan sends MIDI CC 10 at playback start

  - **Interactive Demo** (`tracker_demo`):

    - Standalone demo program for testing the terminal UI

    - Pre-populated 4-track, 16-row pattern (lead, bass, drums, pad)

    - TinySoundFont audio output

    - Run with: `./build/tests/tracker/tracker_demo [soundfont.sf2] [song.trk]`

    - Supports loading saved tracker files from command line

  - **Tests**: 80 unit tests (55 for notes plugin, 20 for audio integration, 5 for MIDI import)

  - **Files Added**:

    - `source/core/tracker/tracker_model.h`, `source/core/tracker/tracker_model.c`

    - `source/core/tracker/tracker_plugin.h`, `source/core/tracker/tracker_plugin.c`

    - `source/core/tracker/tracker_plugin_notes.h`, `source/core/tracker/tracker_plugin_notes.c`

    - `source/core/tracker/tracker_engine.h`, `source/core/tracker/tracker_engine.c`

    - `source/core/tracker/tracker_audio.h`, `source/core/tracker/tracker_audio.c`

    - `source/core/tracker/tracker_midi_import.h`, `source/core/tracker/tracker_midi_import.cpp`

    - `source/core/tracker/tracker_view.h`, `source/core/tracker/tracker_view.c`

    - `source/core/tracker/tracker_view_theme.c`, `source/core/tracker/tracker_view_undo.c`

    - `source/core/tracker/tracker_view_clipboard.c`, `source/core/tracker/tracker_view_json.c`

    - `source/core/tracker/tracker_view_terminal.h`, `source/core/tracker/tracker_view_terminal.c`

    - `source/core/tests/tracker/CMakeLists.txt`

    - `source/core/tests/tracker/test_plugin_notes.c`, `source/core/tests/tracker/test_audio.c`

    - `source/core/tests/tracker/test_midi_import.c`, `source/core/tests/tracker/tracker_demo.c`

- **Parameter Binding System**: Bind named parameters to OSC addresses and MIDI CC for real-time control from physical controllers (knobs, faders)

  - Thread-safe atomic float values for lock-free access from MIDI/OSC threads

  - Parameters have name, type (float/int/bool), min/max/default values

  - Up to 128 named parameters with automatic scaling from controller range

  - **MIDI CC Binding**: Incoming CC messages automatically update bound parameters

    - Channel (1-16) and CC number (0-127) mapping

    - Automatic scaling from 0-127 to parameter's min/max range

  - **OSC Binding**: Parameters can be bound to arbitrary OSC paths

    - Wildcard handler intercepts bound paths automatically

  - **OSC Endpoints** (when OSC is enabled):

    - `/psnd/param/set sf name value` - Set parameter by name

    - `/psnd/param/get s name` - Query parameter (replies `/psnd/param/value`)

    - `/psnd/param/list` - List all defined parameters

  - **Lua API** (`loki.param` / `param` module):

    - `param.define(name, opts)` - Define parameter with min, max, default, type

    - `param.get(name)` - Get parameter value

    - `param.set(name, value)` - Set parameter value

    - `param.bind_osc(name, path)` - Bind to OSC path

    - `param.bind_midi(name, channel, cc)` - Bind to MIDI CC

    - `param.unbind_osc(name)`, `param.unbind_midi(name)` - Remove bindings

    - `param.undefine(name)` - Remove parameter

    - `param.list()` - List all parameters with their bindings

    - `param.info(name)` - Get detailed parameter info

  - **Joy Primitives**:

    - `"name" param` - Get parameter value (push to stack)

    - `value "name" param!` - Set parameter value

    - `param-list` - Print all parameters

  - **MIDI Input API** (new in `loki.midi`):

    - `midi.in_list_ports()` - List MIDI input ports

    - `midi.in_port_count()` - Get number of input ports

    - `midi.in_port_name(idx)` - Get input port name

    - `midi.in_open_port(idx)` - Open MIDI input port for CC reception

    - `midi.in_open_virtual(name)` - Create virtual MIDI input

    - `midi.in_close()` - Close MIDI input

    - `midi.in_is_open()` - Check if input port is open

  - **Files Added**: `source/core/shared/param/param.h`, `source/core/shared/param/param.c`, `source/core/shared/midi/midi_input.c`

  - **Files Modified**: `source/core/shared/context.h`, `source/core/shared/context.c`, `source/core/shared/midi/midi.h`, `source/core/shared/osc/osc.c`, `source/core/loki/lua.c`, `source/langs/joy/midi/midi_primitives.c`, `source/core/CMakeLists.txt`

- **OSC (Open Sound Control) Support**: Remote control and inter-application communication via liblo

  - Enable with `--osc` flag: `psnd --osc song.alda`

  - Custom port with `--osc-port N`: `psnd --osc-port 7770 song.alda`

  - Broadcast events with `--osc-send H:P`: `psnd --osc-send 127.0.0.1:8000 song.alda`

  - **Incoming Messages**:

    - `/psnd/ping` - Connection test (replies with `/psnd/pong`)

    - `/psnd/tempo` - Set tempo (float BPM)

    - `/psnd/note`, `/psnd/noteon` - Play note (channel, pitch, velocity)

    - `/psnd/noteoff` - Stop note (channel, pitch)

    - `/psnd/cc` - Control change (channel, cc, value)

    - `/psnd/pc` - Program change (channel, program)

    - `/psnd/bend` - Pitch bend (channel, value -8192 to 8191)

    - `/psnd/panic` - All notes off

    - `/psnd/play` - Play entire file

    - `/psnd/stop` - Stop all playback

    - `/psnd/eval` - Evaluate code string

  - **Outgoing Messages** (when broadcast target is set via `--osc-send`):

    - `/psnd/status/playing` - Playback state changes (auto-sent on play/stop from any source)

    - `/psnd/status/tempo` - Tempo changes

    - `/psnd/midi/note` - Note events (auto-forwarded for all MIDI note on/off)

  - **Lua API** (`loki.osc` / `osc` module):

    - `osc.init(port)` - Initialize OSC on specified port (default 7770)

    - `osc.start()` - Start OSC server

    - `osc.stop()` - Stop OSC server

    - `osc.enabled()` - Check if OSC is running

    - `osc.port()` - Get current port number

    - `osc.broadcast(host, port)` - Set broadcast target

    - `osc.send(path, ...)` - Send OSC message to broadcast target

    - `osc.send_to(host, port, path, ...)` - Send to specific address

    - `osc.on(path, callback_name)` - Register callback for OSC path

    - `osc.off(path)` - Remove callback for OSC path

  - **Build Option**: Requires `-DBUILD_OSC=ON`

  - **Files Added**: `source/core/shared/osc/osc.h`, `source/core/shared/osc/osc.c`

  - **Files Modified**: `source/core/loki/lua.c`, `source/core/loki/cli.c`, `source/core/loki/cli.h`, `source/core/loki/session.c`, `source/core/loki/session.h`, `source/core/loki/editor.c`, `source/core/loki/lang_bridge.c`, `source/core/main.c`, `source/core/shared/context.c`, `source/core/shared/context.h`, `source/core/CMakeLists.txt`, `source/thirdparty/CMakeLists.txt`

  - **Design Document**: `docs/PSND_OSC.md`

- **Native Webview Mode**: Self-contained native window UI using the webview library

  - Run with `--native` flag: `psnd --native song.alda`

  - Same xterm.js-based UI as web mode but in a native window (no browser required)

  - Works offline with no external dependencies

  - Full editor functionality: vim keybindings, syntax highlighting, playback controls

  - Play/Stop/Eval buttons in toolbar

  - Automatic SharedContext initialization for MIDI/audio

  - Clean shutdown handling on window close

  - **Build Option**: Requires `-DBUILD_WEBVIEW_HOST=ON`

  - **Platform Support**: macOS (WebKit), Linux (GTK + WebKitGTK)

  - **Files Added**: `source/core/loki/host_webview.cpp`, `source/core/loki/host_webview.h`

  - **Files Modified**: `source/core/loki/host_web_ui.h` (shared HTML), `source/core/loki/cli.c`, `source/core/loki/cli.h`, `source/core/main.c`, `source/core/loki/session.c`, `source/thirdparty/CMakeLists.txt`, `source/core/CMakeLists.txt`

- **MHS Language Integration**: Micro Haskell with MIDI support for music programming

  - **REPL Mode**: `psnd mhs` starts interactive Haskell REPL with MIDI libraries

    - PTY-based stdin interposition: MicroHs runs in forked child process with pseudo-terminal

    - Syntax-highlighted input for Haskell keywords, types, and MIDI primitives

    - Tab completion for 80+ Haskell keywords and MIDI functions

    - History persistence (`~/.psnd/mhs_history`)

    - All shared REPL commands (`:help`, `:stop`, `:panic`, `:list`, `:sf`, `:link`, etc.)

    - Ableton Link callback integration for tempo sync

    - MIDI initialization in child process for proper handle inheritance after fork

  - **Run Mode**: `psnd mhs -r file.hs` runs Haskell files with MIDI support

  - **Compile Mode**: `psnd mhs -oMyProg file.hs` compiles to standalone executable

  - **VFS Embedding**: Self-contained binary with Virtual File System serving embedded content

  - **Fast Startup**: ~2s startup with precompiled packages (PKG_ZSTD mode)

  - **MIDI Modules**: Midi, Music, MusicPerform, MidiPerform, Async

  - **CLI Flags**: `--virtual NAME`, `-sf PATH`, `-p N`, `-l`, `-v` (same as other languages)

  - **Build Variants** (Makefile targets):

    - `make` - Full MHS (~5.7MB, ~2s startup, compilation support)

    - `make mhs-small` - No compilation (~4.5MB, ~2s startup)

    - `make mhs-src` - Source embedding (~4.1MB, ~17s startup)

    - `make mhs-src-small` - Smallest with MHS (~2.9MB, ~17s startup)

    - `make no-mhs` - MHS disabled (~2.1MB)

  - **CMake Options**:

    - `ENABLE_MHS_INTEGRATION` - Enable/disable MHS in psnd

    - `MHS_EMBED_MODE` - PKG_ZSTD, PKG, SRC_ZSTD, SRC

    - `MHS_ENABLE_COMPILATION` - Enable `-o` executable output

  - **Files Added**: `source/langs/mhs/` (repl.c, dispatch.c, register.c, vfs.c, midi_ffi.c, etc.)

  - **Dependencies**: MicroHs (BSD), zstd, libremidi

- **REPL Tab Completion**: TAB key now cycles through completions in all language REPLs

  - Standard mechanism: `repl_set_completion_words(ed, words, count)` for static word lists

  - Callback mechanism: `repl_set_completion(ed, callback, user_data)` for dynamic sources

  - TAB cycles through matches; any other key clears completion state

  - **Joy**: Completes dictionary words (including user-defined words via DEFINE)

  - **Alda**: Completes General MIDI instrument names (128 instruments + percussion)

  - **TR7**: Completes music primitives (play-note, set-tempo, midi-list, etc.)

  - **Bog**: Completes built-in predicates (every, beat, euc, scale, chord) and voices (kick, snare, hat, etc.)

  - **Files Modified**: `source/core/repl.h`, `source/core/repl.c`

  - **Files Modified**: `source/langs/joy/repl.c`, `source/langs/alda/repl.c`, `source/langs/tr7/impl/repl.c`, `source/langs/bog/repl.c`

- **FluidSynth Backend**: Optional higher-quality synthesizer as alternative to TinySoundFont

  - Compile-time selection: TinySoundFont and FluidSynth are mutually exclusive

  - Same API: `-sf soundfont.sf2` works with either backend

  - `builtin_synth_*` macro abstraction selects backend at compile time

  - FluidSynth built with lean configuration (`-Dosal=cpp11`) for smaller binary

  - **Build Option**: `make psnd-fluid` or `-DBUILD_FLUID_BACKEND=ON`

  - **Files Added**: `source/core/shared/audio/fluid_backend.c`, `source/core/shared/audio/fluid_backend.h`

  - **Files Modified**: `source/core/shared/context.c`, `source/langs/alda/backends/tsf_backend_wrapper.c`

- **Build Presets**: Named Makefile targets for common build configurations

  - `psnd-tsf` (alias: `default`) - TinySoundFont only (smallest)

  - `psnd-tsf-csound` (alias: `csound`) - TinySoundFont + Csound

  - `psnd-fluid` - FluidSynth only (higher quality)

  - `psnd-fluid-csound` - FluidSynth + Csound

  - `psnd-tsf-web` (alias: `web`) - TinySoundFont + Web UI

  - `psnd-fluid-web` - FluidSynth + Web UI

  - `psnd-fluid-csound-web` (alias: `full`) - Everything

  - **Files Modified**: `Makefile`

- **Dynamic Help Text**: `--help` now shows the correct synth backend name based on build configuration

  - Shows "TinySoundFont" for TSF builds, "FluidSynth" for Fluid builds

  - **Files Modified**: `source/core/main.c`, `source/core/CMakeLists.txt`

- **REPL Language Switching**: Switch between language REPLs without exiting

  - `:lang NAME` - Switch to another language REPL (e.g., `:lang joy`, `:lang alda`)

  - `:langs` - List available languages

  - Uses `exec` to restart the process with the new language, preserving terminal state

  - Available in all language REPLs (Alda, Joy, TR7, Bog)

  - **Files Modified**: `source/core/shared/repl_commands.c`

- **Ableton Link Beat-Aligned Start**: Playback can quantize to the Link beat grid

  - `loki.link.launch_quantize(quantum)` - Set launch quantization (0=immediate, 1=beat, 4=bar)

  - When Link is enabled and launch_quantize > 0, playback waits for the next beat boundary

  - Ensures notes land on the same beats as other Link peers

  - Works with both Alda and Joy async playback

  - **New API**: `shared_link_ms_to_next_beat(quantum)` - Calculate ms until next boundary

  - **Files Modified**:

    - `source/core/shared/link/link.c`, `source/core/shared/link/link.h`

    - `source/core/shared/async/shared_async.c`, `source/core/shared/async/shared_async.h`

    - `source/core/shared/context.h`

    - `source/core/loki/lua.c`

    - `source/langs/alda/async.c`

    - `source/langs/joy/midi/joy_async.c`

- **Lua Sandbox Mode**: Lua scripting is sandboxed by default for security

  - Disables dangerous libraries: `os` (shell execution), `io` (file access), `debug`

  - Removes `load`, `loadfile`, `dofile` from base library

  - Keeps safe libraries: `table`, `string`, `math`, `utf8`, `coroutine`, `package`

  - Protects against malicious `.psnd/init.lua` in cloned repositories

  - Compile-time option: `-DLUA_SANDBOX=ON` (default) or `-DLUA_SANDBOX=OFF` for full access

  - **Files Modified**:

    - `CMakeLists.txt` - Added `LUA_SANDBOX` option

    - `source/core/CMakeLists.txt` - Passes define to libloki

    - `source/core/loki/lua.c` - Conditional library loading

    - `.psnd/init.lua` - Updated debug logging to handle sandbox

- **Ableton Link Configuration in init.lua**: Added commented configuration options

  - All Link options documented with descriptions (init, enable, tempo, launch_quantize, etc.)

  - Callback registration examples for peers, tempo, and transport changes

  - **Files Modified**: `.psnd/init.lua`

- **Build System Improvements**:

  - **AddressSanitizer**: `cmake -B build -DPSND_ENABLE_ASAN=ON .` for memory error detection

  - **Code Coverage**: `cmake -B build -DPSND_ENABLE_COVERAGE=ON .` for gcov/lcov reports

  - **Install Target**: `cmake --install build` installs binary to `bin/` and config to `share/psnd/`

  - **Files Modified**: `CMakeLists.txt`

- **MIDI Port Selection from Editor**: Full Lua API for MIDI port management

  - `midi.list_ports()` - Get table of all available MIDI port names

  - `midi.port_count()` - Get number of available MIDI ports

  - `midi.port_name(index)` - Get name of port at index (1-based)

  - `midi.open_port(index)` - Open MIDI port by index (1-based)

  - `midi.open_by_name(name)` - Open first port matching name substring

  - `midi.open_virtual(name)` - Create virtual MIDI port (default: "PSND_MIDI")

  - `midi.close()` - Close current MIDI port

  - `midi.is_open()` - Check if a MIDI port is currently open

  - **Files Modified**: `source/core/loki/lua.c`

- **Global Lua Module Aliases**: Commonly used modules available without `loki.` prefix

  - `midi` is now a global alias for `loki.midi`

  - `link` is now a global alias for `loki.link`

  - Shortens commands in Ctrl-L REPL: `midi.list_ports()` instead of `loki.midi.list_ports()`

  - **Files Modified**: `source/core/loki/lua.c`

- **Lua Modules for All Languages**: Added `.psnd/modules/` wrappers for Joy, TR7, and Bog

  - `joy.lua` - Stack-based music programming helpers (`joy.eval()`, `joy.define()`, `joy.ports()`)

  - `tr7.lua` - Scheme music programming helpers (`tr7.eval()`, `tr7.play_note()`, `tr7.play_chord()`)

  - `bog.lua` - Prolog pattern helpers (`bog.eval()`, `bog.kick()`, `bog.euclidean()`)

  - Each module documents the C API, provides convenience functions, and registers REPL help

  - Matches existing `alda.lua` module pattern

  - **Files Added**: `.psnd/modules/joy.lua`, `.psnd/modules/tr7.lua`, `.psnd/modules/bog.lua`

- **Architecture Diagram**: Added D2 language diagram of system architecture

  - Shows entry points, editor core, language bridge, shared backends, Lua config

  - Render with: `d2 docs/architecture.d2 docs/architecture.svg`

  - **Files Added**: `docs/architecture.d2`, `docs/architecture.svg`, `docs/architecture.png`

- **Synthesis Backend Tests**: Added unit tests for TSF and Csound audio backends

  - `test_tsf_backend.c` - 19 tests covering initialization, soundfont loading, enable/disable, MIDI messages, boundary conditions

  - `test_csound_backend.c` - 31 tests covering availability, initialization, CSD/orchestra loading, enable/disable, MIDI messages, render, playback control

  - Tests verify API behavior without crashing; full audio output requires manual verification

  - Csound tests handle conditional compilation (`BUILD_CSOUND_BACKEND`)

  - **Files Added**: `source/core/tests/shared/test_tsf_backend.c`, `source/core/tests/shared/test_csound_backend.c`

  - **Files Modified**: `source/core/tests/shared/CMakeLists.txt`

- **Lua-to-Joy Primitive Callbacks**: Register Lua functions as Joy primitives

  - `loki.joy.register_primitive(name, callback)` - Register a Lua function as a Joy word

  - Callback receives Joy stack as Lua array (index 1 = bottom, #stack = top)

  - Return modified stack or `nil, "error"` on failure

  - Supports all Joy types: integers, floats, booleans, strings, lists, quotations, symbols, sets

  - Quotations represented as `{type="quotation", value={...terms...}}`

  - Up to 64 Lua primitives can be registered

  - Example:

    ```lua
    loki.joy.register_primitive("double", function(stack)
        if #stack < 1 then return nil, "stack underflow" end
        local top = table.remove(stack)
        table.insert(stack, top * 2)
        return stack
    end)
    ```

  - Enables extending Joy with Lua's ecosystem (HTTP, JSON, file I/O, etc.)

  - **Files Modified**: `source/langs/joy/register.c`

### Changed

- **Theme Loading Priority**: `:theme` command now loads TOML themes first, then falls back to Lua themes, then C built-ins

  - TOML themes provide true RGB colors in a safe, declarative format

  - Lua themes still work when Lua is enabled

  - C built-in themes remain as final fallback

- **Renamed `tsf_enabled` to `builtin_synth_enabled`**: Flag name now reflects that it controls whichever built-in synth is compiled (TSF or FluidSynth)

  - Updated in `SharedContext`, `AldaContext`, and all language contexts

  - **Files Modified**: `source/core/shared/context.h`, `source/langs/alda/include/alda/context.h`, and 12+ other files

- **Unified MIDI Port Name**: All languages now use `PSND_MIDI` as the default virtual MIDI port name

  - Previously each language used its own port name (Alda, Loki, JoyMIDI, TR7MIDI, BogMIDI, etc.)

  - Now all languages share `PSND_MIDI_PORT_NAME` ("PSND_MIDI") for consistency

  - Simplifies DAW setup: connect once, works with all languages

  - Language switching via `:lang` preserves MIDI connection

  - **Files Modified**:

    - `source/langs/alda/repl.c`, `source/langs/alda/register.c`

    - `source/langs/joy/repl.c`, `source/langs/joy/register.c`

    - `source/langs/joy/midi/joy_midi_backend.c`, `source/langs/joy/midi/midi_primitives.c`

    - `source/langs/tr7/impl/repl.c`, `source/langs/tr7/impl/register.c`

    - `source/langs/bog/repl.c`

- **Centralized SharedContext Ownership**: Single SharedContext instance shared across all languages in editor mode

  - Previously each language (Alda, Joy, TR7, Bog) allocated its own SharedContext, causing conflicts on singleton backends (TSF, Csound, Link)

  - `EditorModel` now owns a single `SharedContext*` that all languages share

  - Editor creates SharedContext in `loki_editor_main()` before language initialization

  - Editor cleans up SharedContext in `editor_cleanup_resources()` after language cleanup

  - Each language's `register.c` now uses `ctx->model.shared` instead of allocating its own

  - REPL mode unchanged: each REPL process still owns its own SharedContext (appropriate for standalone processes)

  - Eliminates undefined behavior when switching between language buffers

  - Prevents inconsistent audio routing based on buffer/language initialization order

  - **Files Modified**:

    - `source/core/loki/internal.h` - Added `SharedContext *shared` to `EditorModel`

    - `source/core/loki/editor.c` - SharedContext creation and cleanup

    - `source/langs/alda/impl/context.c` - Removed SharedContext allocation/cleanup

    - `source/langs/alda/register.c` - Use `ctx->model.shared`

    - `source/langs/alda/repl.c` - Create REPL-owned SharedContext

    - `source/langs/joy/register.c` - Use `ctx->model.shared`

    - `source/langs/tr7/impl/register.c` - Use `ctx->model.shared`

    - `source/langs/bog/register.c` - Use `ctx->model.shared`

### Fixed

- **Buffer switch display bug**: Commands that create and switch to new buffers (e.g., `:plugin presets`) now correctly display the new buffer content

  - Root cause: `command_mode_exit()` was called with stale context after buffer-switching commands

  - Fix: Get fresh buffer context via `buffer_get_current()` after command execution

  - Affects: `:plugin presets`, `:e`, and any command that switches buffers

- **Stale context in `:plugin presets`**: Fixed stale `ctx` pointer after `buffer_switch()` in plugin.c

  - After `buffer_switch()`, the original `ctx` pointer becomes invalid

  - Now refreshes `ctx = buffer_get_current()` immediately after buffer switch

### Removed

- **Lua theme files**: `.psnd/themes/*.lua` replaced by TOML equivalents

  - All 17 themes now in TOML format with same color definitions

  - Lua themes still supported when Lua is enabled (for dynamic themes)

- **Lua language files**: `.psnd/languages/*.lua` replaced by TOML equivalents

  - Language definitions now parsed by C loader (`lang_toml.c`)

  - No runtime Lua dependency for syntax highlighting

- **Lua modules for themes/languages**: `.psnd/modules/theme.lua` and `.psnd/modules/languages.lua`

  - Replaced by C-based TOML loaders

  - Language integration modules (alda.lua, joy.lua, tr7.lua, bog.lua) retained

## [0.1.3]

### Added

- **Renderer Interface**: Abstract rendering layer for platform-agnostic output

  - Decouples editor logic from terminal-specific VT100 escape codes

  - Enables alternative frontends (web, GUI, tests) via renderer callbacks

  - `Renderer` interface with callbacks for:

    - Frame management (`begin_frame`, `end_frame`)

    - Content rendering (`render_tabs`, `render_row`, `render_status`, `render_message`, `render_repl`)

    - Cursor management (`set_cursor`, `show_cursor`, `hide_cursor`)

    - Clipboard operations (`clipboard_copy`)

  - Built-in renderer implementations:

    - `terminal_renderer_create()` - VT100 terminal output

    - `null_renderer_create()` - Discards output (for testing/headless)

  - Structured render data types:

    - `RenderSegment` - Text spans with highlight type and selection state

    - `StatusInfo` - Status bar information (mode, filename, position)

    - `ReplInfo` - REPL pane state (prompt, input, log lines)

    - `HighlightType` - Abstract highlight categories (comment, keyword, string, etc.)

  - `editor_refresh_screen()` now delegates to renderer when available

    - Uses `build_render_segments()` to convert row content to segments

    - Falls back to legacy VT100 code path when no renderer is set

  - OSC-52 clipboard abstracted behind renderer interface

    - `copy_selection_to_clipboard()` uses renderer if available

    - Falls back to direct terminal output for backwards compatibility

  - `editor_ctx_set_renderer()` to set/replace context renderer

  - `buffers_get_tab_info()` / `buffers_free_tab_info()` for tab rendering abstraction

  - **Files Added**: `source/core/loki/renderer.h`, `source/core/loki/renderer.c`

  - **Files Modified**: `source/core/loki/internal.h`, `source/core/loki/core.c`, `source/core/loki/selection.c`, `source/core/loki/buffers.h`, `source/core/loki/buffers.c`, `source/core/CMakeLists.txt`

- **EditorSession API**: Opaque handle for embedding the editor

  - Clean, self-contained API that hides all internal implementation details

  - `EditorSession` opaque handle encapsulates editor state

  - Session lifecycle:

    - `editor_session_new(const EditorConfig*)` - Create session with configuration

    - `editor_session_free(session)` - Free session and resources

  - Event handling:

    - `editor_session_handle_event(session, const EditorEvent*)` - Process input

    - Returns 0 on success, 1 if editor should quit, -1 on error

  - View model (render state snapshot):

    - `editor_session_snapshot(session)` - Get deep copy of render state

    - `editor_viewmodel_free(vm)` - Free view model

    - `EditorViewModel` contains all data for rendering:

      - `EditorRowView` array with segments and owned text

      - `EditorCursor` with screen and file positions

      - `EditorTabInfo` for tab bar

      - `StatusInfo`, `ReplInfo` with owned string copies

  - Configuration via `EditorConfig`:

    - Screen dimensions (rows, cols)

    - Initial filename

    - Line numbers, word wrap flags

    - Lua scripting enable flag

    - Undo limits

  - Convenience accessors:

    - `editor_session_get_mode()` - Get current mode

    - `editor_session_is_dirty()` - Check for unsaved changes

    - `editor_session_get_filename()` - Get current filename

    - `editor_session_resize()` - Update screen dimensions

    - `editor_session_open()` / `editor_session_save()` - File operations

  - Thread-safe view model: snapshot is a deep copy safe to use from any thread

  - **Files Added**: `source/core/loki/session.h`, `source/core/loki/session.c`

  - **Files Modified**: `source/core/CMakeLists.txt`

- **EditorHost Abstraction**: Pluggable host layer for alternate editor environments

  - Separates CLI parsing and terminal orchestration from session logic

  - Enables alternate hosts: HTTP server, headless scripting, test harness

  - `EditorHost` interface with callbacks:

    - `read_event()` - Read next input event (blocking with timeout)

    - `render()` - Render current session state

    - `should_continue()` - Check if host should keep running

    - `destroy()` - Cleanup host resources

  - Optional lifecycle callbacks: `on_start`, `on_tick`, `on_quit`, `on_error`

  - Common entry points:

    - `editor_host_run(host, config)` - Create session and run to completion

    - `editor_host_loop(host, session)` - Run main loop with existing session

  - Built-in host implementations:

    - `editor_host_terminal_create(fd)` - Interactive terminal editing

    - `editor_host_headless_create()` - Scripted/automated editing with event queue

  - Headless host API for automation:

    - `editor_host_headless_queue_event()` - Queue programmatic input

    - `editor_host_headless_quit()` - Signal quit

  - CLI argument parsing extracted to separate module:

    - `EditorCliArgs` struct for parsed arguments

    - `editor_cli_parse()` - Parse argc/argv into config

    - `editor_cli_print_usage()` / `editor_cli_print_version()`

  - **Files Added**: `source/core/loki/host.h`, `source/core/loki/host.c`, `source/core/loki/cli.h`, `source/core/loki/cli.c`

  - **Files Modified**: `source/core/CMakeLists.txt`

- **JSON-RPC Test Harness**: stdio-based command interface for testing editor abstractions

  - Validates EditorSession API without terminal I/O

  - Enables automated integration testing and scripting

  - **Commands**:

    - `{"cmd": "load", "file": "path"}` - Load file into editor

    - `{"cmd": "save"}` - Save current file

    - `{"cmd": "event", "type": "key", "code": N, "modifiers": M}` - Send key event

    - `{"cmd": "event", "type": "resize", "rows": N, "cols": M}` - Send resize event

    - `{"cmd": "event", "type": "quit"}` - Send quit event

    - `{"cmd": "insert", "text": "..."}` - Insert text as key events

    - `{"cmd": "snapshot"}` - Get full viewmodel as JSON

    - `{"cmd": "status"}` - Get editor status (mode, filename, dirty)

    - `{"cmd": "resize", "rows": N, "cols": M}` - Resize screen

    - `{"cmd": "quit"}` - Exit harness

  - **Responses**: `{"ok": true, ...}` or `{"ok": false, "error": "message"}`

  - **Run modes**:

    - `jsonrpc_run_interactive()` - Read commands from stdin until quit/EOF

    - `jsonrpc_run_single()` - Process single command and exit

  - **CLI flags** for invoking JSON-RPC mode:

    - `--json-rpc` - Run in interactive JSON-RPC mode

    - `--json-rpc-single` - Run single JSON-RPC command and exit

    - `--rows N` - Screen rows for headless mode (default: 24)

    - `--cols N` - Screen cols for headless mode (default: 80)

  - Minimal JSON library (no external dependency):

    - `JsonBuilder` - Streaming JSON serialization

    - `json_parse()` - Parse JSON from string

    - `json_object_get_string()`, `json_object_get_int()`, `json_object_get_bool()` - Value accessors

  - `jsonrpc_serialize_viewmodel()` - Full viewmodel to JSON for snapshot command

  - **Files Added**: `source/core/loki/json.h`, `source/core/loki/json.c`, `source/core/loki/jsonrpc.h`, `source/core/loki/jsonrpc.c`

  - **Files Modified**: `source/core/loki/cli.h`, `source/core/loki/cli.c`, `source/core/CMakeLists.txt`

- **Abstract Input Handling Layer**: Structured event abstraction for editor input

  - Replaces raw keycodes with `EditorEvent` objects for cleaner input processing

  - Modifier flags (`MOD_CTRL`, `MOD_SHIFT`, `MOD_ALT`) separated from keycodes

    - `SHIFT_ARROW_UP` becomes `(ARROW_UP, MOD_SHIFT)` internally

  - Event types: `EVENT_KEY`, `EVENT_COMMAND`, `EVENT_ACTION`, `EVENT_RESIZE`, `EVENT_MOUSE`, `EVENT_QUIT`

  - `EventSource` interface for polymorphic input sources:

    - `event_source_terminal(fd)` - wraps `terminal_read_key()` for terminal input

    - `event_source_test()` - queue-based source for unit testing without I/O

  - Backward-compatible conversion functions:

    - `event_from_keycode()` - legacy keycode to event

    - `event_to_keycode()` - event back to legacy keycode

  - New entry point `modal_process_event()` for event-based modal processing

    - Handles Ctrl-X prefix sequences via `pending_prefix` state (no fd required)

    - Handles Ctrl-T (new buffer), Ctrl-Q (quit) directly

  - Decoupled `modal_process_keypress()` from file descriptor dependency

    - Now a thin wrapper that reads from terminal and delegates to `modal_process_event()`

    - Terminal-specific operations (Ctrl-F find) intercepted before delegation

  - Enables test injection without terminal I/O

  - Foundation for future transports (WebSocket, RPC)

  - **Files Added**: `source/core/loki/event.c`, `source/core/loki/event.h`

  - **Files Modified**: `source/core/loki/modal.c`, `source/core/loki/internal.h`, `source/core/CMakeLists.txt`

- **Web Host Mode**: Browser-based editor using xterm.js terminal emulation

  - Access the editor via web browser at `http://localhost:8080`

  - Full terminal emulation with xterm.js

  - WebSocket-based communication for real-time editing

  - **CLI Flags**:

    - `--web` - Start web server (default port 8080)

    - `--web-port N` - Use custom port

  - **Features**:

    - Mouse click-to-position support

    - All editor keybindings work as in terminal mode

    - Language switching commands (`:alda`, `:joy`, `:langs`, `:lang NAME`)

    - First-line directives (`#alda`, `#joy`) for automatic language detection

    - REPL mode with language-aware evaluation

  - **Build Options**:

    - `-DBUILD_WEB_HOST=ON` - Enable web server mode

    - `-DLOKI_EMBED_XTERM=ON` - Embed xterm.js in binary (no CDN dependency)

  - Embedded mode adds ~300KB to binary size but enables offline use

  - **Dependencies**: mongoose (embedded web server), xterm.js (terminal emulator)

  - **Files Added**: `source/core/loki/host_web.c`, `source/core/loki/host_web.h`, `source/core/loki/host_web_xterm.h`

- **Live Loop Feature**: Re-evaluate buffer content on beat boundaries synced to Ableton Link

  - `:loop <beats>` - Start live loop that re-evaluates buffer every N beats (e.g., `:loop 4`)

  - `:unloop` - Stop live loop for current buffer

  - `:loop` (no args) - Show current loop status

  - Requires Ableton Link to be enabled first (`:link on`)

  - Up to 16 concurrent loops across different buffers

  - Enables concurrent multi-language live coding with synchronized playback

  - **Files Added**: `src/loki/live_loop.c`, `src/loki/live_loop.h`, `src/loki/command/loop.c`

- **Playback Ex Commands**: Command-line equivalents for playback control keys

  - `:play` - Play entire buffer (equivalent to Ctrl-P)

  - `:eval [code]` - Evaluate given code or current line (equivalent to Ctrl-E)

  - `:stop` - Stop all playback and live loops (equivalent to Ctrl-G)

  - All commands dispatch through language bridge based on file extension

- **Bog Language Integration**: Prolog-based live coding language for music, inspired by [dogalog](https://github.com/danja/dogalog)

  - **REPL Mode**: `psnd bog` starts interactive Bog REPL with syntax highlighting

    - Declarative event rules: `event(kick, 36, 0.9, T) :- every(T, 1.0).`

    - Named slots for managing multiple patterns: `:def kick ...`, `:undef kick`, `:slots`

    - Mute/unmute/solo controls for live performance: `:mute kick`, `:solo hat`

    - Virtual MIDI port creation: `--virtual NAME`

    - Non-blocking scheduler with ~10ms tick interval

  - **Timing Predicates**: Flexible rhythm generation

    - `every(T, N)` - Fire every N beats

    - `beat(T, N)` - Fire on beat N of bar

    - `euc(T, K, N, B, R)` - Euclidean rhythms (K hits over N steps)

    - `phase(T, P, L, O)` - Phase patterns

  - **Selection Predicates**: Variation and randomness

    - `choose(X, List)` - Random selection

    - `seq(X, List)` - Cycle through list

    - `shuffle(X, List)` - Random permutation

    - `wrand(X, List)` - Weighted random

    - `chance(P, Goal)` - Probabilistic execution

  - **Voice Mapping**: Bog voices map to General MIDI

    - Drums: `kick` (36), `snare` (38), `hat` (42), `clap` (39), `noise` (46) on channel 10

    - Melodic: `sine`, `square`, `triangle` on channel 1

  - **Editor Support**: Full livecoding for `.bog` files

    - `Ctrl-E` - Evaluate current buffer

    - `Ctrl-S` - Stop playback

    - `Ctrl-P` - Panic (all notes off)

  - **Lua API** (`loki.bog` table):

    - `loki.bog.init()` - Initialize Bog subsystem

    - `loki.bog.eval(code)` - Evaluate Bog code

    - `loki.bog.stop()` - Stop playback

    - `loki.bog.is_playing()` - Check if playing

    - `loki.bog.set_tempo(bpm)` - Set tempo

    - `loki.bog.set_swing(amount)` - Set swing

  - **Files Added**:

    - `src/lang/bog/` - Bog language implementation (REPL, dispatch, register, async)

    - `src/lang/bog/impl/` - Core Bog engine (parser, unifier, builtins, scheduler, state manager)

    - `tests/bog/` - Bog test suite (parser, unify, builtins, state manager, resolution, livecoding)

    - `docs/bog/` - Bog documentation (README.md, overview.md)

- **REPL History Persistence**: Command history is now saved between sessions for all REPLs

  - Prefers local `.psnd/` directory if it exists, falls back to `~/.psnd/` if present

  - Joy: `{.psnd}/joy_history`, Alda: `{.psnd}/alda_history`, TR7: `{.psnd}/tr7_history`

  - New shared functions `repl_history_load()` and `repl_history_save()` in `src/repl.c`

  - History limited to 64 entries per REPL

- **Shared Async Playback Service**: Unified non-blocking MIDI playback for all languages

  - New `src/shared/async/shared_async.c/h` - language-agnostic async playback engine

  - Supports both millisecond-based timing (Joy) and tick-based timing with tempo changes (Alda)

  - Up to 8 concurrent playback slots for polyphonic layering

  - libuv-based timer dispatch in background thread

  - Event types: NOTE, NOTE_ON, NOTE_OFF, CC, PROGRAM, TEMPO

  - Tick-based scheduling functions: `shared_async_schedule_*_tick()`

  - `shared_async_ticks_to_ms()` conversion with dynamic tempo tracking

- **Non-blocking Joy REPL**: Joy REPL now remains responsive during MIDI playback

  - `joy_async.c` - thin wrapper around shared async service

  - Commands like `[c d e] play` return immediately while notes play in background

  - Multiple `play` commands layer concurrently instead of blocking sequentially

  - `:stop` command halts all playback

- **Non-blocking TR7 REPL**: TR7 Scheme REPL now remains responsive during MIDI playback

  - `async.c/h` - thin wrapper around shared async service

  - `(play-note pitch [vel] [dur])` returns immediately while note plays in background

  - `(play-chord '(pitches...) [vel] [dur])` plays chord asynchronously

  - `(play-seq '(pitches...) [vel] [dur])` plays notes sequentially without blocking

  - `:stop` command halts all playback

- **Unified `:play` Command**: Generic file playback command that dispatches by extension

  - `:play file.csd` - plays Csound file (blocking)

  - `:play file.alda` - interprets and plays Alda file (from Alda REPL)

  - `:play file.joy` - loads and executes Joy file (from Joy REPL)

  - `:play file.scm` - loads and executes Scheme file (from TR7 REPL)

  - Replaces the previous `:cs-play` command

- **Ableton Link Callbacks in REPLs**: All REPLs now receive Link event notifications

  - Added `shared_repl_link_init_callbacks()`, `shared_repl_link_check()`, `shared_repl_link_cleanup_callbacks()` to `src/shared/repl_commands.c`

  - Joy, Alda, and TR7 REPLs poll for Link events after each command

  - Prints `[Link] Tempo: N BPM`, `[Link] Peers: N`, `[Link] Transport: playing/stopped` to stdout when changes occur

  - Tempo changes automatically sync to the REPL's SharedContext

- **Ableton Link Tempo Sync for Playback**: All languages now use Link tempo when playing

  - Alda: Uses `shared_link_effective_tempo()` to set initial playback tempo

  - Joy: Scales note timings at playback time based on `local_tempo / link_tempo` ratio

  - TR7: Scales note timings at playback time based on `local_tempo / link_tempo` ratio

  - When Link is enabled and connected to peers, playback tempo matches the Link session

### Changed

- **Unified Async Event Queue**: Migrated language playback callbacks to event-driven architecture

  - Replaced polling-based `loki_alda_check_callbacks()` with completion callback mechanism

  - Playback completion now pushes events to unified async queue via `on_alda_playback_complete()`

  - Main loop dispatches all async events through single `async_queue_dispatch_lua()` call

  - Link callbacks, beat boundaries, and language callbacks all use the same event path

  - Added `shared_async_play_ex()` with completion callback parameter

  - Added `alda_events_play_async_ex()` for callback-based playback

  - Extended `async_queue_push_lang_callback()` with events_played, duration_ms, callback name, error

  - Removed `loki_alda_check_callbacks()` - slot clearing now happens in completion callback

  - Marginal efficiency gain (no polling when idle), but main benefit is architectural consistency

- **Makefile Build Targets**: Separated clean and reset functionality

  - `make clean` now uses CMake's clean target (removes compiled objects, keeps cache for faster rebuilds)

  - `make reset` removes the entire build directory (forces full CMake reconfiguration)

  - `make remake` combines reset + build for convenience

  - Use `make reset && make` after adding new languages with `new_lang.py`

- **LuaHost Architecture Refactoring**: Moved Lua state from EditorView to dedicated LuaHost struct

  - Previously `lua_State *L` and `t_lua_repl repl` were embedded in `EditorView`, but Lua is a scripting platform, not a presentation concern

  - New `LuaHost` struct owns both Lua state and REPL state, stored as pointer in `editor_ctx`

  - Accessor macros `ctx_L(ctx)` and `ctx_repl(ctx)` provide indirect access with NULL safety

  - LuaHost lifecycle functions: `lua_host_create()`, `lua_host_free()`, `lua_host_init_repl()`

  - Proper sharing semantics across buffer contexts via pointer assignment

  - Files modified: `internal.h`, `lua.c`, `editor.c`, `buffers.c`, `core.c`, `modal.c`, `command.c`, `repl_launcher.c`, `alda/repl.c`, `bog/repl.c`

- **Alda Async Migrated to Shared Service**: Alda now uses the shared async playback engine

  - Reduced `src/lang/alda/async.c` from ~570 lines to ~135 lines (thin wrapper)

  - Tick-based timing with tempo change support preserved

  - Sequential/concurrent mode flag maintained for backwards compatibility

  - Removed direct `uv_a` dependency (now comes transitively from shared library)

- **Consolidated Dispatch Systems**: Both CLI (`lang_dispatch`) and editor (`loki_lang_bridge`) now use explicit initialization

  - Removed `__attribute__((constructor))` from all `register.c` files (Alda, Joy, TR7)

  - Added `loki_lang_init()` in `src/loki/lang_bridge.c` that calls per-language init functions

  - Each language exports `*_loki_lang_init()` (alda, joy, tr7)

  - `loki_editor_main()` calls `loki_lang_init()` before any language operations

  - CMake passes `LANG_ALDA`, `LANG_JOY`, `LANG_TR7` defines for conditional compilation

  - Both dispatch systems now portable to MSVC (no GCC/Clang-specific attributes)

- **Shared REPL Launcher**: Extracted common REPL startup logic into reusable module

  - New `src/loki/repl_launcher.c/h` with `SharedReplCallbacks` and `SharedReplArgs`

  - Languages provide callbacks for: print_usage, list_ports, init, cleanup, exec_file, repl_loop

  - Shared launcher handles: CLI parsing (`-h`, `-v`, `-l`, `-p`, `--virtual`, `-sf`), syntax highlighting setup, common flow control

  - Joy refactored to use `shared_lang_repl_main()` and `shared_lang_play_main()`

  - TR7 refactored to use the same shared launcher pattern

  - Reduced ~350 lines of duplicate code between Joy and TR7

- **Centralized Constants**: New `include/psnd.h` replaces hardcoded strings throughout codebase

  - `PSND_NAME` - Program name ("psnd")

  - `PSND_VERSION` - Version string ("0.1.2")

  - `PSND_CONFIG_DIR` - Configuration directory (".psnd")

  - `PSND_MIDI_PORT_NAME` - Default virtual MIDI port name ("PSND_MIDI")

  - Updated 12 source files to use these constants

  - Removed `include/version.h` (superseded by `psnd.h`)

- **Shared Csound Backend**: Moved Csound synthesis to shared layer

  - Real implementation now in `src/shared/audio/csound_backend.c`

  - `src/alda/csound_backend.c` provides thin wrappers calling `shared_csound_*` functions

  - Csound synthesis now available to all languages (Alda, Joy) through the shared backend

  - CMakeLists.txt updated to add Csound dependency to shared library

- **Language-agnostic `:csd` Command**: Refactored Csound command to be language-independent

  - New `src/loki/csound.c` provides editor-level Csound control

  - `:csd` command no longer requires Alda initialization

  - Works regardless of whether editing Alda or Joy files

- **Shared MIDI Event Buffer**: Added common event format for MIDI export

  - New `SharedMidiEvent` type in `src/shared/midi/events.h`

  - Shared event buffer API (`shared_midi_events_*`)

  - Languages convert their events to shared format at export time

  - New `loki_midi_export_shared()` reads from shared buffer

  - Enables future languages to support MIDI export

- **Language-agnostic `:export` Command**: Refactored MIDI export command to be language-independent

  - New `src/loki/export.c` provides editor-level export control

  - Converts Alda events to shared format before export

  - Generic error messages (not Alda-specific)

- **Modular Command System**: Refactored editor ex-commands into separate files

  - New `src/loki/command/` directory with one file per command category

  - `command_impl.h` - Shared header with documentation on adding new commands

  - `file.c` - File operations (`:w`, `:e`)

  - `basic.c` - Core commands (`:q`, `:wq`, `:help`, `:set`)

  - `goto.c` - Navigation (`:goto`, `:<number>`)

  - `substitute.c` - Search and replace (`:s/old/new/`)

  - `link.c` - Ableton Link (`:link`)

  - `csd.c` - Csound synthesis (`:csd`)

  - `export.c` - MIDI export (`:export`)

  - Main `command.c` now contains only dispatch logic and command table

- **Unified REPL Command API**: Both Alda and Joy REPLs now share the same command set

  - Common commands work identically in both REPLs (`:help`, `:quit`, `:list`, `:sf`, `:link`, `:cs`, etc.)

  - Commands can be used with or without `:` prefix in both REPLs

  - New shared command processor in `src/shared/repl_commands.c`

  - Language-specific commands remain separate (Alda: `:sequential`/`:concurrent`, Joy: `.` for stack)

- **CLI Normalization**: Simplified command-line interface

  - `psnd` (no args) now shows help and exits with code 1 (was: start Alda REPL)

  - `psnd alda` starts Alda REPL (replaces bare `psnd` and `psnd repl`)

  - `psnd joy` starts Joy REPL (unchanged)

  - Removed implicit REPL fallback for `-sf` without subcommand

### Added

- **Go-to-line Command**: Jump to specific line numbers in the editor

  - `:123` - Jump to line 123

  - `:goto 123` - Same using explicit command name

  - Auto-scrolls viewport to show target line

  - Implemented in `src/loki/command/goto.c`

- **Search and Replace Command**: Vim-style substitution on current line

  - `:s/old/new/` - Replace first occurrence on current line

  - `:s/old/new/g` - Replace all occurrences on current line (global flag)

  - Supports escaped characters (`\/` for literal `/`)

  - Reports number of substitutions made

  - Implemented in `src/loki/command/substitute.c`

- **Shared REPL Commands**: New unified commands available in both Alda and Joy REPLs

  - `:q` `:quit` `:exit` - Exit REPL

  - `:h` `:help` `:?` - Show help

  - `:l` `:list` - List MIDI ports

  - `:s` `:stop` - Stop playback

  - `:p` `:panic` - All notes off

  - `:sf PATH` - Load soundfont and enable built-in synth

  - `:synth` `:builtin` - Switch to built-in synth

  - `:midi` - Switch to MIDI output

  - `:presets` - List soundfont presets

  - `:virtual [NAME]` - Create virtual MIDI port

  - `:link [on|off]` - Enable/disable Ableton Link

  - `:link-tempo BPM` - Set Link tempo

  - `:link-status` - Show Link status

  - `:cs PATH` - Load CSD file and enable Csound

  - `:csound` - Enable Csound backend

  - `:cs-disable` - Disable Csound

  - `:cs-status` - Show Csound status

- **Shared Service Tests**: New test suite for shared backend services (`tests/shared/`)

  - `test_link.c` - 17 tests for Ableton Link (init, enable/disable, tempo, peers, start/stop sync, beat/phase)

  - `test_midi_events.c` - 17 tests for shared MIDI event buffer (recording, retrieval, sorting, capacity)

  - `test_midi_export.c` - 12 tests for MIDI file export (Type 0/Type 1, header validation via "MThd" magic)

  - Link tests excluded from ctest (network discovery can hang); run manually with `./build/tests/shared/test_link`

### Fixed

- **REPL Syntax Highlighting in Generated Languages**: Fixed black text in REPLs created by `new_lang.py`

  - Generated REPLs were missing `editor_ctx_init()` and `syntax_init_default_colors()` calls

  - Without color initialization, all text rendered as black (default zero values)

  - Now properly initializes editor context, default colors, and Lua host for theme loading

  - Updated template in `scripts/new_lang.py` for future language generation

- **PEG Parser Exits on Syntax Error**: Fixed REPL crash on invalid input in languages created by `new_lang_peg.py`

  - PackCC's default `PCC_ERROR` macro calls `exit(1)` on parse failure

  - Added `#define PCC_ERROR(auxil) ((void)0)` to suppress exit behavior

  - Parser now returns failure instead of terminating, REPL continues with error message

  - Updated template in `scripts/new_lang_peg.py` for future language generation

- **Panic Leaves Stuck Notes on Secondary Backends**: Fixed `shared_send_panic()` only silencing the highest-priority backend

  - Previously returned after first active backend (Csound > TSF > MIDI), leaving other backends ringing

  - Now broadcasts "all notes off" to ALL enabled backends regardless of priority

  - Prevents stuck notes when switching backends mid-session or during cleanup

- **Context Cleanup Kills Audio for Other Sessions**: Added reference counting to backend singletons (TSF, Csound, Link)

  - Previously, `shared_context_cleanup()` would unconditionally disable backends that the context had enabled

  - Multiple contexts (REPL, editor, multiple buffers) share the same backend singletons

  - Quitting one REPL would stop audio for all other active sessions

  - Now enable/disable are ref-counted: backend only actually enables on first reference (0->1) and disables on last release (1->0)

  - Files: `src/shared/audio/tsf_backend.c`, `src/shared/audio/csound_backend.c`, `src/shared/link/link.c`

- **Removed Duplicate Alda MIDI Observer**: Eliminated legacy observer that duplicated shared context functionality

  - `alda_midi_init_observer()` was maintaining BOTH a shared observer AND a legacy observer copy

  - This doubled enumeration work, risked memory leaks if one path failed, and complicated cleanup

  - All Alda MIDI operations now delegate entirely to the shared context (`shared_midi_*` functions)

  - Legacy MIDI fields in `AldaContext` (`midi_observer`, `out_ports[]`, `out_port_count`) marked as deprecated

  - The `midi_out` pointer is still synced from `shared->midi_out` for API compatibility

  - Removed ~200 lines of redundant code from `src/lang/alda/backends/midi_backend.c`

- **Joy Multi-Context Support**: Fixed Joy MIDI backend stomping shared context between sessions

  - Previously, Joy's MIDI backend used a global `g_shared` that could be overwritten by multiple callers (REPL vs editor)

  - When one session cleaned up, it would free the context still in use by another session

  - Added ownership tracking (`g_owns_context` flag) to `joy_midi_backend.c`

  - `joy_set_shared_context()` now marks external contexts as not owned (won't free on cleanup)

  - Joy REPL (`repl.c`) now creates and owns its own SharedContext via `g_joy_repl_shared`

  - Joy editor integration (`register.c`) creates per-editor-context SharedContext

  - TR7 already had per-context ownership in both REPL and editor integration

- **MIDI Export Multi-track Crash**: Fixed segfault when exporting multi-channel compositions

  - Track 0 (conductor) was empty when no tempo events existed in the shared buffer

  - Now always adds a default tempo (120 BPM) to ensure track 0 has content

## [0.1.2]

### Added

- **Piped Input Support**: Both Alda and Joy REPLs now support non-interactive piped input

  - `echo ':q' | psnd` - Alda REPL processes piped commands

  - `echo 'quit' | psnd joy` - Joy REPL processes piped commands

  - Detects `!isatty(STDIN_FILENO)` and uses `fgets()` instead of interactive line editor

  - Useful for scripting and automation

- **Joy Csound Backend Integration**: Joy language now supports Csound synthesis

  - **REPL Commands**:

    - `cs-load PATH` - Load a CSD file and auto-enable Csound

    - `cs-enable` - Enable Csound as audio backend

    - `cs-disable` - Disable Csound

    - `cs-status` - Show Csound status

    - `cs-play PATH` - Play a CSD file (blocking)

  - **Joy Primitives**: `cs_load_`, `cs_enable_`, `cs_disable_`, `cs_status_`, `cs_play_`

  - **C API** (`joy_midi_backend.h`):

    - `joy_csound_init()`, `joy_csound_cleanup()`

    - `joy_csound_load(path)`, `joy_csound_enable()`, `joy_csound_disable()`

    - `joy_csound_is_enabled()`, `joy_csound_play_file()`, `joy_csound_get_error()`

  - Routes MIDI events through Alda's Csound backend with priority routing (Csound > TSF > MIDI)

  - Proper cleanup on quit (auto-disables Csound before exit)

- **Joy Language Integration**: Full support for Joy, a concatenative (stack-based) music language from [midi-langs](https://github.com/shakfu/midi-langs)

  - **Joy REPL**: `psnd joy` starts interactive Joy REPL with syntax highlighting

    - Stack-based evaluation: `[c d e] play`, `c major chord`

    - Music theory primitives: `major`, `minor`, `dom7`, `dim`, `aug`

    - MIDI control: `tempo`, `vol`, `pan`, `midi-note`, `midi-cc`

    - Virtual MIDI port creation: `midi-virtual` or `--virtual NAME`

  - **Editor Support**: Full livecoding for `.joy` files

    - `Ctrl-E` - Evaluate current line/selection

    - `Ctrl-P` - Play entire file

    - `Ctrl-G` - Stop playback (MIDI panic)

    - Auto-initialization of Joy context and virtual MIDI port

  - **Play Mode**: `psnd play file.joy` for headless playback

  - **Syntax Highlighting**: Built-in highlighting for Joy files

    - Stack operations (`dup`, `swap`, `pop`, `dip`, `i`, `x`)

    - Combinators (`map`, `fold`, `filter`, `each`)

    - Music primitives (`note`, `chord`, `play`, `rest`)

    - Note names (`c`, `d`, `e`, `f`, `g`, `a`, `b`)

  - **Lua API** (`loki.joy` table):

    - `loki.joy.init()` - Initialize Joy subsystem

    - `loki.joy.eval(code)` - Evaluate Joy code

    - `loki.joy.load(path)` - Load Joy file

    - `loki.joy.define(name, body)` - Define Joy word

    - `loki.joy.stop()` - Stop playback (MIDI panic)

    - `loki.joy.open_port(n)` - Open MIDI port by index

    - `loki.joy.open_virtual(name)` - Create virtual MIDI port

    - `loki.joy.list_ports()` - List available MIDI ports

    - Stack operations: `push_int`, `push_string`, `stack_depth`, `stack_clear`, `stack_print`

  - **Files Added**:

    - `src/joy/` - Joy runtime (parser, primitives, MIDI backend)

    - `src/loki/joy.c`, `include/loki/joy.h` - Loki-Joy bridge

    - `tests/joy/` - Joy test suite (parser, primitives, MIDI)

  - **Files Modified**:

    - `src/loki/modal.c` - Joy keybinding handlers (Ctrl-E, Ctrl-P, Ctrl-G)

    - `src/main.c` - Joy REPL and play mode dispatch

    - `src/repl.c` - Joy REPL implementation

- **Comprehensive Alda Interpreter Tests**: Unit tests for MIDI event generation (`tests/alda/test_interpreter.c`)

  - 44 test cases covering core interpreter functionality

  - Basic notes: pitches, accidentals, octaves, sequences

  - Durations: note lengths, dotted, tied

  - Chords and chord voicings with octave changes

  - Tempo and volume/dynamics attributes

  - Repeats and alternate endings

  - Variables (definition, reference, redefinition)

  - Markers (@marker jumps)

  - Polyphonic voices (V1:, V2:, V0:)

  - Cram expressions (time compression)

  - Key signatures and transposition

  - Pan and quantization

  - Multiple parts and part groups

  - Error handling (undefined variables, markers, missing parts)

  - Unit tests for pitch calculation and duration functions

- **Csound CSD Syntax Highlighting**: Section-aware syntax highlighting for Csound `.csd`, `.orc`, `.sco` files

  - Parses CSD XML structure to detect `<CsOptions>`, `<CsInstruments>`, `<CsScore>` sections

  - **Orchestra section** (`<CsInstruments>`): Full Csound language highlighting

    - Control flow: `if`, `then`, `else`, `endif`, `while`, `do`, `od`, `goto`, etc.

    - Structure: `instr`, `endin`, `opcode`, `endop`

    - Header variables: `sr`, `kr`, `ksmps`, `nchnls`, `0dbfs`, `A4`

    - Common opcodes: `oscili`, `vco2`, `moogladder`, `pluck`, `reverb`, etc.

    - Comments: `;` single-line, `/* */` block comments

    - Strings and numbers

  - **Score section** (`<CsScore>`): Statement-based highlighting

    - Score statements (`i`, `f`, `e`, `s`, etc.) as keywords

    - Numeric parameters

    - `;` comments

  - **Options section** (`<CsOptions>`): Command-line flag highlighting

  - Section tags highlighted as keywords

  - Section state tracked across rows (like markdown code blocks)

  - Keywords extracted from Csound 6.18.1 lexer (`csound_orcparse.h`)

  - **Files Modified**: `internal.h`, `languages.c`, `languages.h`, `syntax.c`

- **Scala Scale File Support**: Parse and use Scala tuning files (`.scl`) for microtuning

  - **C Parser** (`include/alda/scala.h`, `src/alda/scala.c`):

    - `scala_load(path)` - Load .scl file from disk

    - `scala_load_string(buf, len)` - Parse from string buffer

    - `scala_get_ratio(scale, degree)` - Get frequency ratio for scale degree

    - `scala_get_frequency(scale, degree, base)` - Get frequency in Hz

    - `scala_midi_to_freq(scale, midi, root, freq)` - MIDI note to frequency with octave wrapping

    - `scala_cents_to_ratio(cents)` / `scala_ratio_to_cents(ratio)` - Unit conversions

  - **Lua API** (`loki.scala` table):

    - `loki.scala.load(path)` - Load scale file

    - `loki.scala.load_string(content)` - Load from string

    - `loki.scala.unload()` - Unload current scale

    - `loki.scala.loaded()` - Check if scale is loaded

    - `loki.scala.description()` - Get scale name/description

    - `loki.scala.length()` - Number of degrees (excluding implicit 1/1)

    - `loki.scala.ratio(degree)` - Get frequency ratio

    - `loki.scala.frequency(degree, base_freq)` - Get frequency in Hz

    - `loki.scala.midi_to_freq(note, [root], [freq])` - MIDI to Hz with scale

    - `loki.scala.degrees()` - Get all degrees as Lua table

    - `loki.scala.csound_ftable([base_freq], [fnum])` - Generate Csound f-table statement

    - `loki.scala.cents_to_ratio(cents)` / `loki.scala.ratio_to_cents(ratio)` - Utilities

  - **Syntax Highlighting**: `.scl` files with `!` comment highlighting and number highlighting

  - **Sample Scales** (`.psnd/scales/`):

    - `12tet.scl` - 12-tone equal temperament

    - `just.scl` - 5-limit just intonation major

    - `pythagorean.scl` - Pythagorean 12-tone chromatic

  - Format spec: <https://www.huygens-fokker.org/scala/scl_format.html>

- **Standalone CSD File Support**: Edit and play Csound .csd files directly

  - `psnd song.csd` - Open CSD file in editor with syntax highlighting

  - `psnd play song.csd` - Play CSD file headlessly and exit

  - `Ctrl-P` in editor plays the CSD file using Csound's embedded score section

  - `Ctrl-G` stops CSD playback

  - Async playback allows editing while audio plays

  - **Lua API**:

    - `loki.alda.csound_play(path)` - Play a CSD file asynchronously

    - `loki.alda.csound_playing()` - Check if CSD playback is active

    - `loki.alda.csound_stop()` - Stop CSD playback

  - Lua keybindings in `.psnd/keybindings/alda_keys.lua` automatically detect .csd files

- **Csound Synthesis Backend**: Optional advanced synthesis engine as alternative to TinySoundFont

  - Full Csound 6.18.1 integration for powerful synthesis beyond sample playback

  - Independent miniaudio audio device (each backend manages its own audio output)

  - MIDI events translated to Csound score events with fractional instrument IDs

  - Pre-defined instruments in `.psnd/csound/default.csd` (16 GM-compatible instruments)

  - **Build Option**: `make csound` or `-DBUILD_CSOUND_BACKEND=ON`

  - **CLI Options**:

    - `-cs PATH` - Load .csd file and enable Csound when opening an Alda file

    - `-sf PATH` - Load soundfont and enable TinySoundFont when opening an Alda file

  - **Ex Command**: `:csd [on|off|1|0]` - Toggle or set Csound synthesis backend

  - **Lua API** (`loki.alda` extensions):

    - `loki.alda.csound_available()` - Check if Csound is compiled in

    - `loki.alda.csound_load(path)` - Load a .csd instrument file

    - `loki.alda.set_csound(bool)` - Enable/disable Csound synthesis

    - `loki.alda.set_backend(name)` - Unified backend selection ("tsf", "csound", or "midi")

  - **Dependencies**: Csound 6.18.1, libsndfile (both built from source in thirdparty/)

  - **Binary Size**: ~4.4MB with Csound vs ~1.6MB without

  - **Files Added**:

    - `src/alda/csound_backend.c`

    - `include/alda/csound_backend.h`

    - `.psnd/csound/default.csd`

- **Ableton Link Integration**: Tempo synchronization with other Link-enabled applications

  - Sync tempo with Ableton Live, hardware devices, and other Link-compatible software on local network

  - **Ex Command**: `:link [on|off|1|0]` - Toggle or set Link synchronization

  - **Status Bar**: Shows "ALDA LINK" instead of "ALDA NORMAL" when Link is active

  - **Playback Integration**: When Link is enabled, playback uses the Link session tempo

  - **Lua API** (`loki.link` table):

    - `loki.link.init([bpm])` - Initialize Link with optional starting tempo (default 120)

    - `loki.link.cleanup()` - Clean up Link resources

    - `loki.link.enable(bool)` - Enable/disable Link networking

    - `loki.link.is_enabled()` - Check if Link is enabled

    - `loki.link.tempo()` - Get current session tempo

    - `loki.link.set_tempo(bpm)` - Set session tempo (propagates to peers)

    - `loki.link.beat([quantum])` - Get current beat position (default quantum: 4)

    - `loki.link.phase([quantum])` - Get phase within quantum [0, quantum)

    - `loki.link.peers()` - Get number of connected peers

    - `loki.link.start_stop_sync(bool)` - Enable/disable transport sync

    - `loki.link.is_playing()` - Get transport state

    - `loki.link.play()` - Start transport

    - `loki.link.stop()` - Stop transport

    - `loki.link.on_peers(fn)` - Register callback for peer count changes

    - `loki.link.on_tempo(fn)` - Register callback for tempo changes

    - `loki.link.on_start_stop(fn)` - Register callback for transport changes

  - **Files Added**: `src/loki_link.c`, `include/loki/link.h`

  - **Dependencies**: Ableton Link 3.1.5 (GPL v2+)

- **MIDI File Export**: Export Alda compositions to Standard MIDI Files (.mid)

  - **Ex Command**: `:export <filename.mid>` - Export current events to MIDI file

  - **Lua API**: `loki.midi.export(filename)` - Returns true on success, nil + error on failure

  - Exports as Type 0 MIDI (single track) for single-channel compositions

  - Exports as Type 1 MIDI (multi-track) for multi-channel compositions

  - Preserves tempo, program changes, pan, and all MIDI events

  - **Files Added**: `src/loki_midi_export.cpp`, `include/loki/midi_export.h`

  - **Dependencies**: midifile library (BSD-2-Clause)

- **Lua Keybinding Customization System**: User-definable keybindings via Lua

  - `loki.keymap(modes, key, callback, [description])` - Register a keybinding

  - `loki.keyunmap(modes, key)` - Remove a keybinding

  - Supports modes: 'n' (normal), 'i' (insert), 'v' (visual), 'c' (command)

  - Key notation: single chars ('a'), control keys ('<C-a>'), special keys ('<Enter>', '<Esc>', '<Tab>', etc.)

  - Lua callbacks are checked before built-in handlers in each mode

  - Alda keybindings (Ctrl-E, Ctrl-P, Ctrl-G) now customizable via `.psnd/keybindings/alda_keys.lua`

- **REPL Syntax Highlighting**: Real-time Alda syntax highlighting in the REPL as you type

  - Custom line editor with terminal raw mode (no external dependencies)

  - Keywords (tempo, volume, pan, etc.) highlighted in magenta

  - Note names and octave markers highlighted in cyan

  - Numbers highlighted in purple

  - Comments (starting with #) highlighted in gray

  - Full editing support: arrow keys, backspace, delete, home/end

  - Command history with up/down arrows

- **Built-in Alda Syntax**: Alda syntax highlighting now built into the editor

  - No longer requires Lua to load language definition

  - Works immediately in both editor and REPL modes

### Changed

- **Renamed project to psnd instead of aldalog**.

- **Project Restructure**: Reorganized source code for cleaner separation of concerns

  - Moved alda-midi library from `thirdparty/alda-midi/lib/` to `src/alda/` and `include/alda/`

  - Renamed loki source files from `src/loki_*.c` to `src/loki/*.c`

  - Organized tests into `tests/loki/` and `tests/alda/` subdirectories

  - Simplified CMakeLists.txt (~360 lines to ~210 lines)

- **Renamed project to aldalog instead of aldev**.

- **Stripped Binary**: Binary is now stripped by default, reducing size significantly

- **Simplified Configuration**: Renamed `.loki/` to `.psnd/` configuration directory

  - Removed unused modules (ai, editor, markdown, modal, languages, test, example)

  - Removed non-Alda language definitions (13 languages)

  - Kept only Alda-specific files: alda.lua (syntax), alda.lua (module), theme.lua, themes/

### Removed

- **editline/readline Dependency**: Removed external line editing library

  - REPL now uses custom line editor built on terminal raw mode

  - Eliminates dynamic library dependency (libedit.3.dylib)

  - Binary is now fully self-contained

- **libcurl Dependency**: Removed async HTTP and AI integration

  - Removed `loki.async_http()` Lua API

  - Removed `--complete` and `--explain` CLI options

  - Simplifies codebase for Alda-focused use case

- **cmark Dependency**: Removed Markdown parsing library

  - Removed `markdown` Lua module (to_html, parse, validate, etc.)

  - Removed `src/loki_markdown.c` and `src/loki_markdown.h`

  - Binary size reduced from 1.2MB to 1.1MB

### Fixed

- **Alda REPL Cleanup Crash in Pipe Mode**: Fixed segfault during cleanup when using piped input

  - Root cause: Double-free of MIDI output handle due to pointer aliasing

  - `ctx->midi_out` was synced to point to `ctx->shared->midi_out` (same pointer)

  - `alda_midi_cleanup` freed `ctx->midi_out`, then `alda_context_cleanup` tried to free `ctx->shared->midi_out`

  - Fix: Only free `ctx->midi_out` when NOT using shared context; otherwise just clear the pointer

- **Csound Audio Quality**: Fixed poor audio quality in Csound backend

  - Root cause: Audio output was not normalized by Csound's 0dBFS scaling factor

  - CSD files without explicit `0dbfs` setting default to 32768, causing clipping/distortion

  - Now divides all audio samples by `csoundGet0dBFS()` to normalize to -1.0 to 1.0 range

  - Also increased audio buffer size from 512 to 1024 frames to reduce glitches

- **Clean Ctrl-C Handling for CSD Playback**: Fixed messy exit when interrupting `psnd play`

  - Previously required multiple Ctrl-C presses and produced backtrace/crash output

  - Added proper SIGINT signal handler for blocking playback mode

  - Now cleanly stops playback and shows "Stopping playback" message

  - Exits gracefully without error codes or crash output

- **Csound Audio Not Playing**: Fixed Csound synthesis producing no audio output

  - Root cause: `async.c` event dispatcher was routing MIDI events only to TSF, ignoring Csound

  - Added Csound routing to `send_event()` with highest priority

  - Each backend now has its own independent miniaudio device (cleaner architecture)

  - Disproved theory that macOS couldn't handle multiple miniaudio instances

- **Parser Infinite Loop on Invalid Syntax**: Fixed hang when entering invalid Alda syntax in REPL

  - `tempo 120` (missing parentheses) now shows error instead of hanging

  - Parser now properly advances past unexpected tokens

  - Correct syntax `(tempo 120)` continues to work as expected

- **Lua API Cursor Position Bug**: Fixed `loki.get_cursor()` returning screen position instead of file position

  - Was returning `ctx->cy` (screen row) instead of `ctx->rowoff + ctx->cy` (file row)

  - Caused Lua keybindings like Ctrl-E to operate on wrong line when scrolled

  - Also fixed column position to include horizontal scroll offset

- **Lua API Stale Context Bug**: Fixed Lua API using stale editor context with multiple buffers

  - `lua_get_editor_context()` was returning a pointer stored at Lua init time

  - Now dynamically calls `buffer_get_current()` to get the active buffer's context

  - Falls back to registry pointer for backwards compatibility with tests

- **Line Numbers Not Displaying**: Fixed `loki.line_numbers(true)` having no effect

  - Root cause: `buffers_init()` and `buffer_create()` didn't copy the `line_numbers` field

  - Settings from init.lua were applied to initial context but lost when buffer system initialized

  - Now properly copies `line_numbers` setting when creating buffers

- **REPL Syntax Highlighting Now Uses Themes**: REPL now uses the same color theme as the editor

  - Previously: REPL used hardcoded default colors, ignoring `.psnd/init.lua` theme settings

  - Now: REPL initializes Lua and loads themes from `.psnd/init.lua`

  - Both editor and REPL now share consistent syntax highlighting colors

  - Refactored `repl.c` to thread `editor_ctx_t` through rendering functions

---

## [0.1.1]

### Added

- **Unified Binary**: Merged editor, REPL, and playback into single `psnd` binary

  - `psnd` (no args) - Interactive REPL for direct Alda notation input

  - `psnd file.alda` - Editor mode with live-coding

  - `psnd play file.alda` - Headless playback

  - `psnd -sf soundfont.sf2` - REPL with built-in synthesizer

  - Single 1.7MB distributable binary

- **Direct Alda REPL**: New interactive mode for typing Alda notation directly

  - No Lua wrapper required - type `piano: c d e f g` directly

  - Vim-style colon commands (`:q`, `:h`, `:stop`, etc.)

  - Soundfont loading (`:sf PATH`)

  - MIDI port listing (`:list`)

  - Concurrent/sequential playback modes

  - Line editing with history (editline/readline)

### Changed

- **Ctrl-E Plays Part**: `Ctrl-E` now plays the current Alda "part" (instrument declaration and all its notes) instead of just the current line

  - A part starts at an instrument declaration (e.g., `piano:`, `violin "alias":`) and extends until the next part declaration or end of file

  - Selection still takes precedence if text is visually selected

  - More musically meaningful for livecoding workflows

- **Eager Language Loading**: Changed from lazy to eager loading of language definitions in `.psnd/init.lua`

  - Ensures syntax highlighting works immediately when opening files

  - All languages in `.psnd/languages/` are now loaded at startup

- **Self-Contained Lua Build**: Switched from system Lua to local Lua 5.5.0 in thirdparty/

  - Project now builds without requiring system Lua installation

  - Added `thirdparty/lua-5.5.0/CMakeLists.txt` to build Lua as static library

  - Updated `thirdparty/CMakeLists.txt` to include Lua subdirectory

  - Updated main `CMakeLists.txt` to use local Lua instead of `find_package(Lua)`

  - Lua 5.5.0 features available (coroutine improvements, new warnings system)

  - Binary remains self-contained with no external Lua dependency

### Removed

- **Separate Binaries**: Removed `alda-editor` and `alda-repl` as separate executables

  - Replaced by unified `psnd` binary with mode dispatch

  - Deleted `src/main_editor.c` and `src/main_repl.c`

- **Lua REPL**: Removed standalone Lua REPL (`alda-repl`)

  - Lua scripting remains available in editor via `Ctrl-L`

  - New direct Alda REPL is simpler for music composition

  - Reduces user-facing complexity (Lua is internal for extension only)

### Fixed

- **Single-Character Comment Delimiters**: Fixed syntax highlighting for languages using single-character comment delimiters (e.g., `#` for Python, Alda, shell scripts)

  - Previously only two-character delimiters like `//` and `--` worked

  - Now correctly highlights comments starting with `#`

- **Dynamic Language Syntax Highlighting**: Fixed syntax highlighting not working for Lua-registered languages (like Alda)

  - Rows are now re-highlighted after dynamic languages are loaded

  - Syntax selection is re-run after Lua bootstrap completes

- **Keyword Matching Bounds Check**: Fixed potential out-of-bounds read when matching keywords at end of line

### Added

- **Alda Music Language Integration**: Complete livecoding support for the Alda music notation language

  - **Core Integration** (`src/loki_alda.c`, `src/loki_alda.h`):

    - Async playback using libuv event loop (non-blocking, editor remains responsive)

    - Built-in FluidSynth-based synthesizer with SoundFont support

    - Up to 8 concurrent playback slots for layered compositions

    - Polling callback mechanism for Lua integration

  - **Keybindings** (work in both NORMAL and INSERT modes):

    - `Ctrl-E` - Play current part or visual selection as Alda

    - `Ctrl-P` - Play entire file as Alda

    - `Ctrl-G` - Stop all playback

  - **Auto-Initialization**:

    - Automatically initializes Alda when opening `.alda` files

    - Status bar shows "ALDA" indicator when in Alda mode

    - Status bar shows "[PLAYING]" during active playback

  - **Syntax Highlighting** (`.psnd/languages/alda.lua`):

    - All General MIDI instruments (piano, violin, trumpet, etc.)

    - Alda attributes (tempo, volume, pan, quantization, etc.)

    - Note names (c, d, e, f, g, a, b) and rests (r)

    - Octave markers (o0-o9)

    - Line comments (#)

  - **Lua Helper Module** (`.psnd/modules/alda.lua`):

    - `alda.play(code, [callback])` - Play Alda code asynchronously

    - `alda.play_sync(code)` - Play Alda code (blocking)

    - `alda.play_line()` - Play current line (editor only)

    - `alda.play_paragraph()` - Play current paragraph

    - `alda.play_file()` - Play entire buffer

    - `alda.stop()` - Stop all playback

    - `alda.tempo(bpm)` - Set tempo (20-400 BPM)

    - `alda.soundfont(path)` - Load SoundFont (.sf2) file

    - `alda.synth(bool)` - Enable/disable built-in synthesizer

    - `alda.is_playing()` - Check if currently playing

    - `alda.get_tempo()` - Get current tempo

    - `alda.demo()` - Play demo melody

    - `alda.help()` - Show help

  - **Lua API** (`loki.alda` table):

    - `loki.alda.init()` - Initialize Alda subsystem

    - `loki.alda.eval(code, [callback])` - Evaluate Alda code async

    - `loki.alda.eval_sync(code)` - Evaluate Alda code sync

    - `loki.alda.stop_all()` - Stop all playback

    - `loki.alda.is_initialized()` - Check if initialized

    - `loki.alda.is_playing()` - Check if playing

    - `loki.alda.set_tempo(bpm)` - Set tempo

    - `loki.alda.get_tempo()` - Get tempo

    - `loki.alda.load_soundfont(path)` - Load SoundFont

    - `loki.alda.set_synth(bool)` - Enable/disable synth

  - **Dependencies** (in `thirdparty/`):

    - `alda-midi` - Alda parser and MIDI generation library

    - `libuv` - Async I/O for non-blocking playback

    - `libremidi` - Cross-platform MIDI output

  - **Files Added**:

    - `src/loki_alda.c`, `src/loki_alda.h` - Core Alda integration

    - `.psnd/languages/alda.lua` - Syntax highlighting

    - `.psnd/modules/alda.lua` - Lua helper module

  - **Files Modified**:

    - `src/loki_internal.h` - Added `CTRL_P`, `CTRL_G` keys and `alda_mode` flag

    - `src/loki_modal.c` - Added keybinding handlers

    - `src/loki_core.c` - Added status bar indicators

    - `src/loki_editor.c` - Added auto-initialization for .alda files

    - `src/loki_lua.c` - Added Lua bindings

    - `.psnd/init.lua` - Load alda module

    - `CMakeLists.txt` - Build integration

## [0.1.0] - Initial Release

- Project created
