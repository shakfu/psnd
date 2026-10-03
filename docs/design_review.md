# Architecture Review – Replacing the Terminal Editor with a Web Editor

> **HISTORICAL — describes the pre-`host.h` codebase. Superseded.** > > This review was written against the old `src/` layout (now `source/core/`) and > its central finding — that there was no abstraction a non-terminal front-end > could reuse — has since been addressed: > > - `source/core/loki/host.h` defines the `EditorHost` interface >   (`read_event` / `render` / `should_continue` / `destroy`). > - `source/core/loki/session.h` provides the headless session API the review >   asked for, with `editor_session_snapshot()` feeding a ViewModel to any host. > - Three hosts implement it today: terminal (`host.c`), web >   (`host_web.c`, mongoose + xterm.js) and native webview (`host_webview.cpp`). > > The file paths cited below no longer exist. Kept for the rationale that led to > the host abstraction; do not use it as a description of the current design.

## Context

psnd combines a terminal-first modal editor (Loki), polyglot language runtimes, and a shared audio/MIDI backend. The request is to evaluate how the current architecture affects the feasibility of swapping the terminal UI with a web-based editor and to highlight code or structural changes that would improve decoupling.

## Key Observations

### Terminal-centric rendering path

- `editor_refresh_screen()` in `src/loki/core.c:544-782` directly writes VT100 escape sequences with `terminal_buffer_append()` and derives layout from terminal rows/columns. Rendering, layout (line numbers, gutters, REPL panes), and cursor management all live in this function, so there is no abstraction layer that a different front-end could reuse.

- The Lua REPL UI (`src/loki/lua.c:1675-1709`) also emits VT100 sequences directly, assuming a contiguous TTY scrollback buffer.

- `editor_ctx_t` ( `src/loki/internal.h:163-219` ) stores `screenrows`, `screencols`, `winsize_changed`, REPL layout, and cursor offsets in terminal coordinates along with buffer data. A web view would need logical buffer/mode data without TTY-specific state, but there is no clean separation today.

### Input/event loop tightly bound to POSIX terminals

- `modal_process_keypress()` (`src/loki/modal.c:820-905`) blocks on `terminal_read_key(fd)` and interprets escape sequences itself. The call stack always assumes a Unix file descriptor, so there is no way to inject synthetic events from another host/UI.

- `terminal_enable_raw_mode()` and `terminal_read_key()` (`src/loki/terminal.c:32-174`) depend on `termios`, `ioctl`, and `isatty`. They install global state (`orig_termios`, `signal_context`) and manipulate the real terminal (alternate buffer, cursor reporting). None of that maps to a browser sandbox, and the design doesn’t offer alternate implementations.

### Global singletons and implicit runtime coupling

- `loki/editor.c:273-420` owns a single static `editor_ctx_t E`, registers `atexit()` handlers, and drives rendering + language callbacks inside a `while(1)` loop. The loop assumes it exclusively owns STDIN/STDOUT and the process’ signal handlers, preventing multiple editor sessions or headless hosting.

- The buffer manager (`src/loki/buffers.c:79-198`) keeps static global state with shallow copies of `editor_ctx`. It shares the Lua state pointer and even raw terminal flags between buffers. This approach cannot be safely exposed through an RPC boundary or shared between a terminal and web view.

- `terminal.c` stores a single `signal_context` pointer globally ( `src/loki/terminal.c:20-81` ), so only one editor can respond to resize signals. Any attempt to host the core in a background process for a browser client would have to rewrite this logic.

### Editor core and domain logic are interwoven

- Domain concerns (buffers, syntax, commands, language states, REPL state) coexist with presentation fields in `editor_ctx_t`. Functions throughout the codebase receive `editor_ctx_t *` even when they only need buffer data, tying them to the terminal runtime. For example, language bridges (`src/loki/lang_bridge.c:60-210`) look up the current file via `ctx->filename` and rely on editor-managed initialization.

- Clipboard, selection, and search modules implicitly use terminal helpers (see OSC-52 handling inside `src/loki/selection.c` and search prompts in `src/loki/search.c`), so even secondary features assume a VT100 host.

### Lua/REPL and language runtimes share the UI thread

- REPL activation, Lua execution, and language callback polling all run inside the same blocking loop (`src/loki/editor.c:364-418`). There is no message bus or task queue, so replacing the UI would require rewriting the control loop rather than plugging into an API.

- Lua bindings emit terminal output (`terminal_buffer_append()` usages around `src/loki/lua.c:1675-1709`) and expect synchronous keyboard focus toggling (`exec_lua_command()` in `src/loki/editor.c:101-139`), which conflicts with browser event models.

### Platform-specific dependencies limit reuse

- Terminal support relies on POSIX-only headers (`termios`, `unistd`, `sys/ioctl`, signals) inside `src/loki/terminal.c`. Compiling the editor core to WebAssembly or embedding it in a service process would require stubbing every one of these calls or rewriting the module.

- The entry point (`loki_editor_main`) mixes CLI parsing, audio backend selection, Lua bootstrap, and terminal orchestration. There is no headless API such as `editor_session_create()`, `editor_apply_input()`, `editor_render()` that a new front-end could call.

### Bright spots

- The shared audio/MIDI layer (`src/shared/`) is already UI-agnostic; language runtimes interact with it through `SharedContext`. Likewise, the language dispatch system (`src/lang_dispatch.c` and `src/loki/lang_bridge.c`) provides a modular pattern worth mirroring for the editor host/view separation.

## Recommendations for a Decoupled Design

1. **Introduce a host-agnostic session API**: Encapsulate buffer state, cursor/mode info, and language hooks in an opaque `EditorSession`. Expose functions such as `editor_session_new(const EditorConfig*)`, `editor_session_handle_event(session, const EditorEvent*)`, and `editor_session_snapshot(session, EditorViewModel*)`. Terminal mode would become one consumer of this API; a web server could become another.

2. **Split rendering from state**: Replace `editor_refresh_screen()` with a renderer interface (e.g., callbacks to emit rows, gutters, status segments, REPL panes). The terminal implementation would translate that into VT100, while a web implementation could serialize JSON to the client.

3. **Abstract input handling**: Model keystrokes, commands, and editor actions as structured events instead of raw key codes. Implement a converter from terminal escape sequences to events in the current CLI build, and allow other transports (WebSocket, tests) to inject their own event stream without touching `termios`.

4. **Eliminate global singletons**: Make `terminal_enable_raw_mode()` operate on an explicit `TerminalHost` object, move buffer state out of static globals, and avoid sharing mutable pointers between contexts. This is a prerequisite for running multiple sessions (tabs, tests, remote users) safely.

5. **Separate process responsibilities**: Extract CLI parsing + terminal orchestration into a thin wrapper that owns one `EditorSession`. This allows building alternate hosts (HTTP server exposing the same session API, headless scripting harness, etc.) without duplicating editor logic.

6. **Untangle Lua/REPL I/O**: Route REPL output through the renderer abstraction instead of emitting terminal escapes directly. Store REPL logs as plain strings/events and let each front-end decide how to paint them.

7. **Plan for async tasks**: Introduce an event queue (potentially leveraging libuv, already a dependency for playback) so language callbacks, timers, and UI events can be scheduled without blocking the render loop. A web host could then drive the session via an async RPC instead of a busy `while(1)` loop.

## Potential Migration Path to a Web Editor

1. **Refactor `editor_ctx_t`** into two structs: a pure model (buffers, syntax, selections, language handles) and a view adapter (terminal metrics, theme). Provide serialization helpers for the model.

2. **Implement an alternate renderer** that outputs a structured diff (JSON/protobuf) representing visible rows, selections, and status bars. Initially feed that into tests; later expose it via a simple HTTP/WebSocket server.

3. **Wrap the editor core in a service process** that speaks a small RPC protocol (e.g., stdio JSON or gRPC) handling “load file/save/apply keystroke” commands. The existing terminal binary can talk to it locally, proving the abstraction before the web UI exists.

4. **Build the browser front-end** once the service API stabilizes. It would connect to the RPC service, send high-level editor events, render the diff tree, and reuse the existing shared audio/MIDI context for playback commands.

5. **Gradually retire terminal assumptions** (OSC-52 clipboard, SIGWINCH, alternate screen) from shared modules by guarding them behind the terminal adapter so the browser host never links against termios/signal code.

Adopting these steps turns Loki into a reusable editing engine with multiple hosts. The shared backend work already completed for audio/MIDI shows the project benefits from explicit layering; applying the same discipline to the editor will make a web front-end practical without rewriting the music language integrations.
