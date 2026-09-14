# TODO

## Critical

## High

- [ ] Phrase arrays leaked on tracker engine teardown #known-bugs

  **Status:** Open

  `tracker_engine.c:659` frees `engine->recent_by_track` but not the phrase arrays
  each entry owns:

  ```c
  if (engine->recent_by_track) {
      /* TODO: free phrase arrays */
      free(engine->recent_by_track);
  ```

  Bounded per engine instance, so it does not grow during a session, but it does
  leak on every engine create/destroy cycle and shows up under ASan.

- [ ] **Resolve the mongoose license conflict before distributing web builds** #licensing-third-party-attribution
  - psnd is GPL-3.0. Mongoose (`source/thirdparty/mongoose-7.20/`) is
    **GPL-2.0-only or commercial** — GPL-2.0-only is incompatible with
    distributing a combined GPL-3.0 work.
  - Affects the `web`, `tsf-web`, `fluid-web` and `fluid-csound-web` variants,
    which CI now builds and uploads as artifacts.
  - Options: obtain a commercial mongoose license, relicense psnd to
    GPL-2.0-or-later, or replace mongoose with a permissively licensed HTTP/WS
    server for the web host.
  - This is a legal question, not a technical one — get a definitive answer
    before publishing binaries of those variants.

- [ ] Add license texts for all vendored dependencies #licensing-third-party-attribution
  - `docs/licenses/` currently contains only `KILO-LICENSE`; there are 23
    dependencies under `source/thirdparty/`.
  - Several are LGPL (Csound, FluidSynth, libsndfile, liblo) or GPL-2.0+
    (Ableton Link), so attribution is an obligation, not a courtesy.

- [ ] Add `THIRD-PARTY.md` recording each dependency's upstream URL and pinned version #licensing-third-party-attribution
  - 67 MB is vendored with no submodules, so there is currently no way to tell
    what version anything is or to audit for upstream security fixes.

- [ ] Windows support (in progress) #cross-platform-support

  - [x] Terminal abstraction layer (`terminal.h`, `terminal_posix.c`, `terminal_win.c`)

    - Windows Console API with VT mode (Windows 10+) and legacy fallback

    - Alternate screen buffer, raw mode, resize detection

  - [x] Platform compatibility header (`compat/platform.h`)

    - Cross-platform `unistd.h` equivalents: `access()`, `isatty()`, `usleep()`, etc.

    - Path handling utilities

  - [x] Threading abstraction (`compat/thread.h`)

    - `psnd_mutex_t` with Windows CRITICAL_SECTION / POSIX pthread_mutex

    - Thread creation, condition variables

  - [x] CMake Windows support (`psnd_platform.cmake`)

    - Windows audio/MIDI linking (winmm, ws2_32)

  - [ ] Test on Windows

    - Requires Windows build environment (MSVC or MinGW)

    - Test terminal VT mode detection

    - Test audio/MIDI backends

- [ ] Build troubleshooting guide #cross-platform-support

  - Platform-specific dependency installation (macOS, Linux distros, Windows/MSYS2)

  - Common build errors and solutions

  - Audio/MIDI backend configuration per platform

- [ ] **Merge the duplicated REPL line editors** #stability-robustness
  - `source/core/repl.c` (648 lines) and `source/core/loki/repl_line_editor.c`
    (823 lines) are near-identical implementations of the same line editor;
    `source/core/CMakeLists.txt:334-337` picks `repl.c` as the fallback when
    loki is not built.
  - The duplication has already caused a divergent fix: the `strcpy` hardening
    was applied to one copy's `ARROW_UP` path only, and the other copy had
    neither branch fixed (both are now bounded, but the asymmetry will recur).
  - Extract the shared core into one file with the linenoise-backed path behind
    `#ifdef LOKI_USE_LINENOISE` — the mechanism `repl_line_editor.c` already
    uses internally — and delete the duplicate.
  - Also collapses the duplicate headers `source/core/repl.h` (126 lines) and
    `source/core/loki/repl.h` (125 lines).

- [ ] Define `MAX_INPUT_LENGTH` exactly once #stability-robustness
  - `source/core/repl.h:20` defines it as **1024**;
    `source/core/loki/repl_helpers.c:31` defines it as **4096** behind an
    `#ifndef`, so the effective value depends on include order per translation
    unit. Folds into the REPL merge above.

- [ ] Add checked-allocation wrappers and migrate incrementally #stability-robustness
  - A heuristic scan (allocation whose next line is not a NULL check) flags
    ~255 sites across `source/core` and `source/langs`. Many are false
    positives, but the discipline is not uniform outside `loki/core.c`.
  - Add `psnd_xmalloc`/`psnd_xcalloc`/`psnd_xstrdup` to `shared/` rather than
    auditing every site by hand.

- [ ] Reconsider `exit(1)` on allocation failure in `loki/core.c` #stability-robustness
  - Sites at `core.c:252, 282, 293, 368, 380, 396` and elsewhere call
    `perror("Out of memory"); exit(1);` (inherited from kilo).
  - Two problems: unsaved buffer contents are lost with no recovery attempt,
    and `libloki` is buildable as a shared library (`LOKI_BUILD_SHARED`), where
    calling `exit()` from library code is not acceptable.
  - The overflow guards preceding these allocations are correct — only the
    failure response needs changing.

- [ ] Give `tracker_notes_to_string()` a buffer-size parameter #stability-robustness
  - `tracker_plugin_notes.h:86` takes a bare `char* buffer` and
    `tracker_plugin_notes.c:186` writes into it with `sprintf`. Bounded in
    practice (max output `"A#-1"`), but the API invites a future overflow.

- [ ] Replace `atoi` with `strtol` in config and command parsing #stability-robustness
  - 29 call sites. `atoi` silently returns 0 on garbage and is undefined on
    overflow, so malformed user input becomes a valid-looking 0 instead of an
    error.

- [ ] Web/host layer - `host.c`, `host_web.c`, `session.c`, `event.c` #code-coverage

  - `host.h` already defines the `EditorHost` / `EditorSession` layer these need, so a queue-backed test host is the natural harness. Nothing exercises it today.

  - `host_web.c` is the only network-facing file in the project and now carries access-control logic that should not regress silently: cover token accept/reject, cross-origin rejection, and the loopback-by-default bind.

- [ ] Smaller uncovered modules: `export.c`, `live_loop.c`, `cli.c`, `lang_toml.c`, `repl_helpers.c`, `repl_launcher.c`, `repl_line_editor.c`, `terminal_win.c` #code-coverage

  - `repl_line_editor.c` should get its test alongside the P0 overflow fix

  - `keybind.c` and `theme_toml.c` are covered transitively via `test_config.c`; `midi_input.c` via `test_midi.c`. Not gaps.

  - `editor.c` is **not** a meaningful gap despite `REVIEW.md` calling it the largest untested module. It is 697 lines of CLI entry point, Lua highlight glue, and teardown. The editor logic it appears to own actually lives in `core.c`, `modal.c`, `renderer.c`, `undo.c`, and `search.c` - all tested.

- [ ] `host_webview.cpp` - no tests #code-coverage

- [ ] `tr7/impl/repl.c` (975 lines) - no tests #code-coverage
  - TR7 coverage is `test_reader.c` and `test_music.c` only.

- [ ] `mhs/vfs.c` (1107 lines) - no tests #code-coverage

- [ ] Make disabled-backend tests visibly skip rather than silently pass #code-coverage
  - `shared_csound_backend_tests`, `shared_fluid_backend_tests` and
    `shared_minihost_backend_tests` compile to stubs and pass in ~0.02s when
    their backend is off, so a green local run says nothing about them.
  - Report them as skipped (`ctest` `SKIP_RETURN_CODE`) so the gap is legible.

- [ ] Add an AddressSanitizer job #code-coverage
  - `-DPSND_ENABLE_ASAN=ON` is already wired and the suite runs in ~2.5s, so
    this is nearly free. Would likely surface more of the allocation issues
    listed under Stability & Robustness.

- [ ] Add a `-Werror` job over first-party targets only #code-coverage
  - `-Wall -Wextra -Wpedantic` is on for every first-party target but nothing
    is `-Werror`, so warnings can accumulate unnoticed. Keep it to one
    dedicated job so local builds do not break on new compiler versions, and
    so the 23 vendored dependencies stay exempt.

- [ ] Add a coverage job #code-coverage
  - `-DPSND_ENABLE_COVERAGE=ON` is wired but never exercised; would replace the
    hand-maintained coverage estimates in this section with real numbers.

- [ ] `lua.c` - Lua integration (extend existing `test_lua_api.c`) #code-coverage

  - [ ] Lua state lifecycle (init, cleanup, error recovery)

  - [ ] API bindings coverage (loki.*, alda.*, joy.*, link.*)

  - [ ] Script loading and execution

  - [ ] Error handling and sandboxing

  - [ ] Callback registration

- [ ] MHS (MicroHaskell) - 19 files, initial tests added (47 tests) #code-coverage

  - [x] MIDI FFI (32 tests) - cents-to-bend, random, recording, channel validation

  - [x] Context state (15 tests) - null safety, port API, lifecycle

  - [ ] Lexer/tokenizer (MicroHs runtime - low priority)

  - [ ] Parser (MicroHs runtime - low priority)

  - [ ] Type checker (MicroHs runtime - low priority)

  - [ ] Interpreter/evaluator (MicroHs runtime - low priority)

- [ ] Consolidate the in-editor Lua REPL onto the shared line-editor API #code-quality

  - `REVIEW.md` reports the REPL line editor "duplicated three times across `repl.c`, `loki/repl_line_editor.c`, and `lua.c`". Two of the three are fine: `repl.c` and `repl_line_editor.c` export the *same* symbols (`repl_add_history`, `repl_history_load`, `repl_history_save`) and are selected mutually exclusively by `WITH_LINENOISE` (`source/core/CMakeLists.txt:191`, `:334-337`). That is a backend split behind one API, not drift.

  - The real third copy is `lua.c:3111-3160`, which carries its own `history[]`, `history_index`, and key loop for the in-editor REPL. That one can diverge.

## Medium

- [ ] Fix remaining stale path references in docs #documentation
  - The source tree moved from `src/` to `source/core/`; `new_lang.md` and
    `docs/README.md` have been corrected, these have not:
  - `docs/refactor.md` - ~30 dead `src/...` paths (whole document predates the
    reorganization; consider marking historical like `design_review.md`)
  - `docs/scsynth.md` - `src/supercollider/...`, `.psnd/synthdefs/`, `.psnd/config`
  - `docs/LANG_IMPL_COMPARISON.md` - `source/langs/tr7/dispatch.c`,
    `.psnd/mhs_history`, `source/core/loki/syntax/lang_*`
  - `docs/language-extension-api.md` - refers to `.psnd/languages/alda.lua`;
    the real file is `.psnd/languages/alda.toml`

- [ ] Document the threading contract for the audio backend globals #documentation
  - `g_tsf`, `g_fluid`, `g_cs`, `g_link`, `g_async`, `g_queue` are reachable
    from both the async playback thread and the UI thread. The locking
    discipline works but is undocumented, which makes it easy to break.

- [ ] API reference generation #documentation

  - Consider Doxygen for generated docs

  - Focus on public APIs: Lua bindings, language bridge, shared backend

- [ ] Contributing guide #documentation

  - Code style, PR process, test requirements

- [ ] Refresh the design docs against the current tree #documentation

  - `docs/design_review.md` (13 hits) and `docs/refactor.md` (41 hits) still cite pre-rename paths `src/loki/...` and `src/shared/...`, now `source/core/loki/...` and `source/core/shared/...`, with `terminal.c` split into `terminal_posix.c` / `terminal_win.c`.

  - More misleading than the paths: both still present the model/view split and the host/session layer as future work. Both landed. Reviewers reading these docs reach wrong conclusions about the code - `REVIEW.md` did exactly that.

- [ ] Split windows #editor-features

  - Already designed for in `editor_ctx_t`

  - Requires screen rendering changes

## Low

- [ ] **P3 (was P1) - Sandbox the web host file endpoints** (`loki/host_web.c:434-451`, `:484-491`, `:600-616`) #security-hardening

  - Demoted by the trust model above: this is only reachable by someone the user does not trust, which is out of scope. Auth plus the loopback bind is the accepted mitigation. **Promote back to P1 before any multi-client work.**

  - `handle_api_save` / `handle_api_load` and the WebSocket `load` command `fopen` a caller-supplied `filename` verbatim - no traversal, absolute-path, or symlink check. With `/api/run` and `/api/repl` evaluating arbitrary language code this is a full host-compromise primitive.

  - Mitigated by default, not fixed: `/api/*` and `/ws` require a random per-session token (`web_host_token_ok`) and the listener binds `127.0.0.1`. It becomes remotely reachable under `--web-host ADDR`, which the token still gates.

  - Fix: resolve against a configured root, reject `..`, absolute paths, and symlinks escaping it. Do this before the multi-client work below.

- [ ] Consider C11 atomics for the async shutdown flags #security-hardening

  - `g_async.running` and `g_async.shutdown_requested` are `volatile sig_atomic_t`, a convention this file and `csound_backend.c` document deliberately. It is safe in practice but is not a synchronization primitive, and it is the only thing helgrind still reports in the async layer.

  - Moving them (and the `csound_backend.c` equivalents) to `atomic_int` would make the intent checkable by tooling. Deliberately left alone here to keep the thread-safety fix reviewable and to avoid changing a shared convention in one place only.

- [ ] Consider Csound's async APIs to shorten the engine lock hold #security-hardening

  - Csound 6.18 provides `csoundCompileOrcAsync` (parses on the caller's thread, queues the merge for the performance thread) and `csoundReadScoreAsync`. Using them in `shared_csound_compile_orc` and the note senders would cut the contention window rather than just making it non-blocking, so recompiles would not drop audio at all.

  - Not done here: it changes when compile errors surface and when score events take effect, which is a semantic change beyond the thread-safety fix.

- [ ] Web host as primary cross-platform UI #cross-platform-support

  - Already functional with xterm.js terminal emulator

  - [x] ~~Authentication (required before exposing to network)~~ **DONE**

    - Binds `127.0.0.1` by default; `--web-host ADDR` is an explicit opt-in

    - Random per-session token required on `/ws` and the `/api` endpoints

    - Cross-origin WebSocket handshakes rejected (`Origin` check)

    - `LUA_SANDBOX` defaults ON, and warns if turned off with `BUILD_WEB_HOST=ON`

  - [ ] Path sandbox for `/api/save`, `/api/load`, and the WebSocket `load` command

    - Blocks network exposure regardless of auth; tracked as P3 above

  - [ ] Multiple client support (currently single WebSocket connection)

    - `WebHostData.ws_conn` is one pointer; a second client overwrites it

    - See "Support multiple editor sessions in one process" under Low Priority

  - [ ] Session persistence (save/restore editor state across restarts)

  - [ ] Test coverage for the access-control logic (see Code Coverage)

- [ ] Add fuzzing infrastructure (optional) #stability-robustness

  - Consider libFuzzer or AFL++ for parser/scanner testing

  - Low priority unless targeting wider distribution

  - If it happens, `loki/json.c` and the web host request path are the best targets

- [ ] Consider splitting the largest first-party files #code-quality

  - `lua.c` (3,644), `tracker/tracker_view.c` (3,058), `tracker/tracker_view_terminal.c` (2,044), `joy/impl/joy_primitives.c` (5,185).

  - Low urgency - all four are well covered by tests. Note `REVIEW.md` names `editor.c` as the largest module; it is 697 lines.

- [ ] Route the terminal path through the existing host abstraction #future-architecture

  - Scoped down from `REVIEW.md` recommendation 4, most of which is already done: `internal.h:143-236` splits `EditorModel` (document) from `EditorView` (presentation) inside `editor_ctx`, and `host.h` / `session.h` already define the `EditorHost` / `EditorSession` layer that the web and webview hosts use.

  - What is actually left: `loki_editor_main` (`editor.c:317-660`) still runs its own `while(1)` against `STDIN_FILENO` and calls `editor_refresh_screen` directly, bypassing that layer. Moving it onto `EditorHost` would leave one loop instead of two and make the terminal path testable like the others.

  - The design docs oversell this: `docs/design_review.md` and `docs/refactor.md` still describe the pre-split code and pre-rename paths (see Documentation).

- [ ] Wrap editor core in standalone service process #future-architecture

  - Small RPC protocol (stdio JSON or gRPC)

  - Would enable embedding editor in other applications

  - Depends on the terminal-path work above

- [ ] Lua-to-language primitive callbacks for TR7/Bog #future-architecture

  - Joy already implemented (`loki.joy.register_primitive`)

  - TR7 (Scheme): Moderate complexity

  - Bog (Prolog): High complexity (unification, backtracking)

- [ ] Plugin architecture for language modules #future-architecture

  - Dynamic loading of language support

- [ ] JACK backend #future-architecture

  - For pro audio workflows on Linux

- [ ] Split the largest first-party files #future-architecture
  - `loki/lua.c` (3644 lines) mixes binding registration, sandbox policy
    (`:2923`) and UI output — separate registration from implementation.
  - `tracker/tracker_view.c` (3059 lines) already has `_terminal`, `_json`,
    `_undo`, `_clipboard` and `_theme` siblings; the remaining candidate is the
    `strcmp` command-dispatch chain around `:2340`, which wants a table.
  - `joy/impl/joy_primitives.c` (5173 lines) is a primitive table and is
    arguably fine as-is.

- [ ] Reduce the vendored dependency footprint #future-architecture
  - 67 MB across 23 dependencies with no submodules; `.git` is 16 MB.
  - `tree-sitter-grammars` alone is 23 MB and is the obvious candidate for
    `FetchContent` instead of vendoring.
  - Depends on `THIRD-PARTY.md` (High Priority) to know what versions are pinned.

- [ ] Support multiple editor sessions in one process #future-architecture
  - Blocked by ~25 file-scope globals in `loki/` and `shared/` (`g_config`,
    `g_commands`, `g_current_lang`, the audio singletons, ...).
  - Prerequisite for the web host's "multiple client support" item, since
    `WebHostData.ws_conn` is a single connection pointer today.

- [ ] LSP client integration #editor-features

  - Would provide IDE-like features

  - High complexity undertaking

- [ ] Git integration #editor-features

  - Gutter diff markers

  - Stage/commit commands

- [ ] bytebeat (see: <https://dollchan.net/bytebeat>) #backlog-new-languages

- [ ] funcbeat #backlog-new-languages

- [ ] drumbeat (see: <https://wavepot.com>) #backlog-new-languages

- [ ] Automation lanes (per-row parameter automation) #backlog-tracker-enhancements

- [ ] Live jam mode (real-time pattern triggering) #backlog-tracker-enhancements

- [ ] Quantize (snap MIDI notes to grid) #backlog-tracker-enhancements

- [ ] Piano roll view (graphical note editing) #backlog-tracker-enhancements

- [ ] Pattern templates (save/load for reuse) #backlog-tracker-enhancements

- [ ] Sample trigger (one-shot audio samples from cells) #backlog-tracker-enhancements

- [ ] Preset browser & layering UI #backlog-feature-opportunities

- [ ] Session capture & arrangement (MIDI timeline) #backlog-feature-opportunities

- [ ] Controller & automation mapping (MIDI CC, OSC) #backlog-feature-opportunities

- [ ] Cross-language patch sharing (messaging bus) #backlog-feature-opportunities

- [ ] Real-time visualization (playback state via OSC/WebSocket) #backlog-feature-opportunities

## Done

- [x] `:plugin presets` buffer switch not working #known-bugs

  **Status:** Resolved (2026-01-31)

  **Root cause:** After `buffer_switch()` in `command/plugin.c`, the local `ctx` pointer was stale - it still pointed to the old buffer's context. Subsequent operations like `editor_set_status_msg(ctx, ...)` were modifying the wrong buffer.

  **Fix:** Refresh the context pointer after buffer switch:
  ```c
  buffer_switch(buf_id);
  ctx = buffer_get_current();  /* Refresh stale pointer */
  ```

  **Original description:** The `:plugin presets` command was intended to open a new scratch buffer displaying all plugin presets. The command executed without errors, but the new buffer did not appear - the editor stayed on the original buffer.

- [x] ~~**P0 - Stack buffer overflow in REPL tab-completion**~~ **DONE** #security-hardening

  - `prefix[REPL_MAX_INPUT_LENGTH]` was 1024 bytes while linenoise hands the callback a `LINENOISE_MAX_LINE` (4096) buffer. When `word_len >= 1024` the `memcpy` was skipped but `prefix[word_len] = '\0'` still executed, writing up to ~3KB past the buffer. Reachable in the default build (`WITH_LINENOISE` defaults `ON`) with no auth and no opt-in flag.

  - Extracted `repl_extract_completion_prefix()` (`loki/repl.h`, `loki/repl_line_editor.c`) as a bounds-checked, testable layer. It returns -1 when the word plus terminator does not fit and the adapter skips completion - correct rather than truncating, since such a word would not survive `repl_readline()`'s truncation into `ReplLineEditor.buf` anyway.

  - `test_repl_line_editor.c`: 17 tests, guard-byte buffers around the output. Verified the four boundary tests fail against the pre-fix logic.

- [x] ~~**P1 - Unchecked `ftell` in web host file reads**~~ **DONE** #security-hardening

  - Both sites now reject a negative `ftell` before allocating: `handle_api_load` returns HTTP 400 "Not a regular file"; the WebSocket `load` command skips the read. Sizes are cast to `size_t` only after the check.

  - Mirrors the existing `file_size <= 0` pattern in `minihost_backend.c:756-779` and `serialize.c:293-315`.

- [x] ~~**P2 - Unbounded recursion in the JSON parser**~~ **DONE** #security-hardening

  - `JsonParser` gained a `depth` counter, capped at `JSON_MAX_DEPTH` (64, defined in `json.h`). The check lives in `parse_value` at the single container recursion site rather than inside `parse_object`/`parse_array`, so the accounting stays symmetric across their many error-return paths.

  - Verified: with the cap removed, `test_json` **segfaults (exit 139)** on the 100k-deep input. With it, the input is rejected cleanly.

  - Two further bugs found while reading the parser for the tests:

    - `json_parse(NULL)` dereferenced NULL in `strlen`. Also confirmed by removing the guard: exit 139. Now returns `JSON_ERROR`.

    - `parse_object`'s grow path called `realloc` on both the key and value arrays before testing either result. If the first succeeded and the second failed, `keys` was left dangling and the cleanup read through it and freed it twice. Each realloc is now adopted immediately. OOM-only, so it is fixed by inspection rather than covered by a test.

- [x] ~~**P1 - libuv handle mutation off the loop thread**~~ **DONE** #security-hardening

  - `shared_async_play_ex` called `uv_timer_start` from the caller's thread while the loop thread ran `uv_run`, corrupting libuv's internal timer heap. `slot->stop_requested` and `slot->active` were also read/written outside the mutex while the loop thread used them.

  - Each slot gained a `start_async` handle. `play_ex` now computes the first delay under the mutex, stores it, and `uv_async_send`s; `on_start_signal` starts the timer on the loop thread. `uv_async_send` is the only libuv call safe to make cross-thread, so it is now the only one made.

  - `shared_async_stop`/`stop_all` no longer write `stop_requested` directly. They set a mutex-guarded `stop_pending`, and `on_stop_signal` acts on it. This also closes a latent bug: `uv_async_send` coalesces, so a stale stop signal could previously tear down a slot that had already been recycled for a new schedule. Both handlers now verify the pending flag under the mutex.

  - `shared_async_cleanup` waits up to 250ms for stops to drain before shutting the loop down, so note-offs are still emitted now that stops are processed asynchronously.

  - `last_source_line` is `volatile sig_atomic_t`, matching the convention already documented for the `g_async` cross-thread flags.

  - Verified with helgrind: **3,745 races in 120 contexts before, 3 in 3 after**. The before-run showed the race inside libuv's own `heap_insert` from `shared_async.c:875`. The 3 remaining are the pre-existing `volatile sig_atomic_t` shutdown flags, which helgrind cannot model as synchronization - see below.

  - `test_shared_async.c` gained 9 tests that drive real schedules through the loop thread (completion, stop, repeated stop, slot reuse, concurrent slots, stop-all, 40 rapid play/stop cycles, and a stop racing the start signal). The pre-existing tests never reached the event loop at all.

- [x] ~~**P2 - Blocking mutex in the realtime audio callback**~~ **DONE** #security-hardening

  - `shared_csound_render` runs on the miniaudio device thread and took `cs_mutex_lock`, holding it across `csoundPerformKsmps`. A control-thread compile on the same mutex stalled the audio thread - a priority inversion where a non-realtime thread blocks a realtime one.

  - Now try-locks (`cs_mutex_trylock`, added to both the POSIX and Windows mutex wrappers) and emits one period of silence on contention. This is a bounded, self-correcting glitch instead of a missed callback deadline, and it lets the compile finish sooner since it no longer contends with rendering.

  - `shared_csound_render_skip_count()` reports how many buffers were dropped this way, so the degradation is observable rather than silent. Reset on load.

  - Measured on a 600-instrument orchestra (~56KB, a realistic live-coding recompile): the compile holds the mutex for **~30ms against a 23.2ms audio period**, so it always outlasts a callback deadline. Worst-case `shared_csound_render` call, compile running on another thread:

    | | worst render call | render calls completed |
    |---|---|---|
    | blocking lock | 101.43 ms | 1,232 |
    | try-lock | 0.02 - 0.24 ms | ~1.6M |

  - `test_csound_backend.c` gained 3 tests. Both contention tests were confirmed to fail against the pre-fix locking. They need a playback device to put the engine in its enabled state and report-and-return without failing where none is available.

- [x] ~~`g_play.mutex` in `csound_backend.c` is locked only by `play_audio_callback`~~ **DONE** #security-hardening

  - Removed. It was worse than merely dead: the state that *is* shared across threads - `finished` and `active` - was read and written entirely outside it (`shared_csound_play_stop`, the blocking wait loop, and the audio callback).

  - `playback_sigint_handler` also writes `finished`, and a signal handler may only touch `volatile sig_atomic_t`, so a mutex could never have covered it. Both flags are now `volatile sig_atomic_t`, matching `g_interrupted` directly below them.

  - Everything else in `PlaybackState` is published before `ma_device_start` and torn down after `ma_device_stop`, so the device lifecycle orders it.

  - Verified end to end by playing a 400ms CSD through `shared_csound_play_file`: returns 0 after ~514ms, so the callback still signals completion correctly.

- [x] ~~**P2 - Unchecked `realloc` in the Joy string lexer**~~ **DONE** #security-hardening

  - `lexer_read_string` assigned the `realloc` result straight into `buffer`, which both leaked the old block and left a NULL to write through on the very next line. The initial `malloc` was unchecked too. Both now return NULL, which propagates safely: `joy_strdup` is NULL-safe and the token cleanup at `joy_parser.c:220` already guards on the pointer.

  - The other two sites `REVIEW.md` lists under this heading are false positives. `search.c:143` guards with `if (saved_hl)` and `async_queue.c:296-300` guards with `if (!event.heap_data) return -1`.

- [x] ~~**P3 - MHS gives untrusted `.hs` files shell and filesystem access**~~ **WON'T FIX** #security-hardening

  - Closed by the trust model above: `.hs` files are the user's own. Recorded here rather than deleted so the analysis is not redone.

  - `vfs_cleanup_temp` (`langs/mhs/vfs.c:829-836`) shells out via `system("rm -rf ...")`. The path comes from `mkdtemp`, so it is not directly injectable, but it should be `nftw`-based like the CLI test harness already is.

  - The `system` primitive at `source/thirdparty/MicroHs/src/runtime/eval.c:6765` is the real exposure. Two caveats the review missed: that file is vendored MicroHs upstream (Augustsson, 7,288 lines), so gating it means carrying a patch; and the adjacent `mhs_fopen`, `mhs_open`, and `mhs_unlink` grant equivalent filesystem access, so gating `system` alone does not sandbox anything.

  - Gating `system` alone would buy nothing regardless, and a `WANT_STDIO`-scoped build variant is a large change against vendored upstream. Joy's `PSND_ENABLE_SHELL` gate (`joy_primitives.c:4755-4764`) already exists and can stay as-is; it is not worth mirroring in MHS.

  - [x] ~~`vfs_cleanup_temp` shelling out to `rm -rf`~~ **DONE** - now walks the
    tree with `nftw(FTW_DEPTH | FTW_PHYS)` like `test_process.h` already did.
    `FTW_PHYS` keeps it from following symlinks out of the temp directory. The
    Windows branch still uses `rmdir /s /q`, matching the same precedent.

- [x] ~~**P4 - Web host auth hygiene**~~ **DONE** #security-hardening

  - `/api/*` accepts the token as an `X-Psnd-Token` header, so it need not land in browser history or an intermediary log. `/ws` still takes it as a query parameter because a browser cannot set headers on a WebSocket handshake.

  - `web_host_token_ok` compares in constant time (`web_host_secret_eq`).

  - Cross-origin WebSocket handshakes are rejected on the `Origin` header, and the bind address moved from the `PSND_WEB_BIND` environment variable to the `--web-host ADDR` flag.

- [x] ~~Add scanner/lexer unit tests for all languages~~ **DONE** #stability-robustness

  - Alda scanner: 44 tests including vulnerability tests

  - Joy lexer: 59 tests including edge cases

  - Bog tokenizer: 41 tests including error recovery

  - TR7 reader: 61 tests including buffer boundary tests

- [x] ~~Refactor CLI tests to avoid shell spawning~~ **DONE** #stability-robustness

  - Replaced `system()` with `fork`/`execve` via `test_exec()` in `test_process.h`

  - Uses `mkdtemp()` and `nftw()` for temp directory management

- [x] ~~Add missing test coverage~~ **DONE** #stability-robustness

  - Added `test_lang_bridge.c`: 38 tests for language bridge dispatch

  - Extended `test_link.c`: 10 new tests for callbacks and tempo clamping

  - Added `test_repl_commands.c`: 52 tests for shared REPL command processor

- [x] ~~Expand test framework~~ **DONE** #stability-robustness

  - Comparison macros: `ASSERT_GT`, `ASSERT_LT`, `ASSERT_GTE`, `ASSERT_LTE`

  - Fixture support: `FIXTURE`, `TEST_F`, `SUITE_SETUP`, `SUITE_TEARDOWN`

  - Memory leak detection: `test_memcheck.h` with allocation tracking

- [x] ~~Prune the dead CMake scripts in `scripts/cmake/`~~ **DONE** #stability-robustness

  - Deleted all seven (`psnd_shared_library`, `psnd_loki_library`, `psnd_psnd_binary`, `psnd_tests`, `psnd_alda_library`, `psnd_joy_library`, `psnd_bog_library`). Only `psnd_platform` and `psnd_languages` remain, and both are live. Verified with a clean `cmake -B` configure.

  - Follow-on found while pruning: `docs/new_lang.md` documented the whole add-a-language workflow against those dead files, and also had contributors hand-editing `lang_config.h` and `lang_dispatch.c`. Both are now generated (`lang_config_generated.h`, `lang_dispatch_generated.h`) and marked DO NOT EDIT, so that guide would have produced a language that silently did not build. Steps 5-8 and the Key Files table rewritten against the real `psnd_register_language()` auto-discovery. `scripts/new_lang.py` was already correct - only the prose was stale.

- [x] ~~`main.c` / `lang_dispatch.c` - Entry point and mode selection~~ **DONE** (70 tests: 44 unit + 26 integration) #code-coverage

- [x] ~~`midi.c` / `midi_input.c` - Core MIDI subsystem~~ **DONE** (68 tests) #code-coverage

- [x] ~~`music_theory.c` - Chord construction, scale handling~~ **DONE** (73 tests) #code-coverage

- [x] ~~Tracker module (11 files)~~ **DONE** (654 tests) #code-coverage

  - [x] ~~tracker_model.c~~ **DONE** (70 tests)

  - [x] ~~tracker_engine.c~~ **DONE** (104 tests)

  - [x] ~~tracker_plugin.c~~ **DONE** (65 tests)

  - [x] ~~tracker_plugin_notes.c~~ **DONE** (included in plugin tests)

  - [x] ~~tracker_audio.c~~ **DONE** (included in engine tests)

  - [x] ~~tracker_view_json.c~~ **DONE** (77 tests)

  - [x] ~~tracker_view_undo.c~~ **DONE** (47 tests)

  - [x] ~~tracker_view_theme.c~~ **DONE** (49 tests)

  - [x] ~~tracker_view_clipboard.c~~ **DONE** (77 tests)

  - [x] ~~tracker_view.c~~ **DONE** (121 tests)

  - [x] ~~tracker_view_terminal.c~~ **DONE** (44 tests)

- [x] ~~Command system (11 files in `loki/command/`)~~ **DONE** (51 tests) #code-coverage

  - All handlers tested: goto, substitute, basic, file, export, metronome, link, loop, csd, theme, plugin

- [x] ~~`config.c`, `keybind.c`, `theme_toml.c` - Configuration loading~~ **DONE** (41 tests) #code-coverage

- [x] ~~`renderer.c` - Terminal rendering~~ **DONE** (50 tests) #code-coverage

- [x] ~~`async.c` / `shared_async.c` - Async scheduling~~ **DONE** (62 tests) #code-coverage

- [x] ~~`jsonrpc.c`, `osc.c` - Protocol handlers~~ **DONE** (67 tests: 42 jsonrpc + 25 osc) #code-coverage

- [x] ~~`loki/json.c` - hand-rolled JSON parser, **no test file at all**~~ **DONE** #code-coverage

  - `test_json.c`: 54 tests covering the builder (escaping, nesting, buffer growth past `INITIAL_CAP`, reset, error stub) and the parser (scalars, containers, capacity growth, depth limit, 12 malformed-input cases, accessors, round-trip). Valgrind clean: 608 allocs, 608 frees, 0 errors.

  - Note it is used well beyond the web host - `jsonrpc.c`, `tracker_view_json.c`, and `host_webview.cpp` all depend on it.

  - Several tests deliberately pin *current* lenient behaviour rather than correct JSON, since callers rely on it: `\uXXXX` decodes to `'?'` rather than the real codepoint, and numbers keep only the integer part (`3.99` parses as `3`). Both are marked in the test file so changing them is a deliberate act rather than an accidental regression.

- [x] ~~Extract shared REPL loop skeleton~~ **DONE** #code-quality-completed

- [x] ~~Centralize platform CMake logic~~ **DONE** #code-quality-completed

- [x] ~~Complete command dispatcher for all keybindings~~ **DONE** #code-quality-completed

- [x] ~~Extract buffer manager to injectable service~~ **DONE** #code-quality-completed

- [x] ~~Expand editor highlight vocabulary for full tree-sitter support~~ **DONE** #code-quality-completed

- [x] ~~README build-table fixes~~ **DONE** #documentation

  - Dropped the misleading "(smallest)" on `make psnd-tsf` and added a note that every preset includes MHS by default, so it is the smallest *backend* choice rather than the smallest binary.

  - Added the two minihost presets, and a utility-target table covering `psnd`, `library`, `rebuild`, `reset`, `remake`, and `test-minihost`.

- [x] ~~Playback visualization~~ **DONE** #editor-features

  - [x] ~~Highlight currently playing region~~ **DONE**

    - [x] ~~Source line tracking in Alda events~~ **DONE** (`ALDA_SOURCE_TRACKING`)

    - [x] ~~Source line tracking in shared async~~ **DONE** (`SHARED_SOURCE_TRACKING`)

    - [x] ~~Joy parser source line tracking~~ **DONE**

    - [x] ~~TR7 async API source tracking~~ **DONE** (API ready, primitives use 0 - TR7 interpreter doesn't expose source positions)

    - [x] ~~Bog parser source line tracking~~ **DONE**

      - Tokenizer tracks line numbers per token

      - `BogClause.source_line` stores clause definition line

      - `BogSolutions.source_lines` tracks matched clause per solution

      - `bog_scheduler_get_current_source_line()` API for querying

      - `LokiLangOps.get_source_line` callback for language-agnostic query

    - [x] ~~Editor integration to highlight lines during playback~~ **DONE**

      - Line gutter shows `>` indicator in bright green for playing line

      - Row background highlighted with dark green during playback

      - `EditorView.playing_line` updated each frame from `shared_async_get_current_source_line()`

  - [x] ~~Show playback progress in status bar~~ **DONE**

    - Status bar shows bar.beat, tempo, Link peers, play/metronome state

- [x] ~~Tempo tap / Metronome toggle~~ **DONE** #editor-features

  - `:tap` command for tap tempo (averages intervals, sets BPM via Link or context)

  - `:metronome` command with subdivisions (1=quarter, 2=eighth, 4=sixteenth)

  - Beat-synced via Link, uses drum sounds (kick/hi-hat)

- [x] ~~Full transport sync~~ **DONE** #ableton-link-integration

  - [x] ~~Wire transport callbacks to start/stop playback~~ **DONE**

    - `:link transport [on|off]` command to enable optional transport sync mode

    - When enabled, Link transport start plays the buffer, stop halts playback

    - Works independently of basic Link tempo sync

  - [x] ~~"Armed for playback" state~~ **DONE**

    - Status bar shows `[ARMED]` when transport sync enabled but waiting for Link start

    - `StatusInfo.transport_armed` field for renderer

## Notes

### Security Hardening

**Trust model (decided 2026-08-24): psnd is a local single-user tool. Untrusted input is out of scope.** The web host stays on loopback for the user's own use, and the `.hs` / `.joy` / `.csd` files it runs are the user's own. This is written down because the repo contradicts itself on it - `README.md` says the web host should not be network-exposed without auth, while the Cross-Platform section below lists multi-client web UI as a goal. **If the multi-client work is ever picked up, this decision is void and the path sandbox becomes a prerequisite.**

Findings from `REVIEW.md` (2026-08-24), re-verified against the tree. Ordered by validated exploitability, not by the review's original ranking: the REPL overflow outranked the web host because it needed neither a build flag nor a network peer. The memory-safety items are all fixed; what remains is scoped by the trust model above.

### Code Coverage

Current state (recounted 2026-08-24): **77 first-party test files** (90 tree-wide including vendored), **~2,515 test functions**. 75 CTest suites, all passing. Direct file mapping coverage ~70%.

**Critical gaps (no unit tests):**

**Secondary gaps:**

**Remaining test coverage work:**

**CI hardening:**

**Well-tested areas (for reference):**

| Module | Tests | Notes |
|--------|-------|-------|
| Alda parser/scanner | 169 | Strong core parsing coverage |
| Joy parser/primitives | 106 | Core language well tested |
| Bog language | 187 | Tokenizer, parser, builtins |
| TR7 reader/music | 102 | Good coverage |
| Loki editor core | 365 | Modal editing, undo, search |
| Shared backends | 168 | TSF, Csound, FluidSynth, Minihost |
| Music theory | 73 | Pitch, chords, scales, microtonal |
| Command handlers | 51 | All 11 command files covered |
| MIDI I/O | 68 | Context, ports, callbacks, timing |
| Async playback | 62 | Schedule, events, tick/ms modes, lifecycle |
| Config system | 41 | TOML parsing, keybindings, themes |
| Tracker model | 70 | Events, phrases, cells, tracks, patterns, songs |
| Tracker engine | 104 | Lifecycle, timing, transport, event queue, sync |
| Tracker plugin | 65 | Registry, compilation, evaluation, context, RNG |
| Tracker view | 415 | JSON, undo, theme, clipboard, core, terminal |
| MHS (MicroHaskell) | 47 | MIDI FFI, context state, port API |

### Backlog: Tracker Enhancements

Future features for the tracker sequencer (`tracker_demo`):
