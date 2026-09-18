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

