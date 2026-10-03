# psnd — a polyglot editor for music programming languages

**psnd** is a self-contained modal editor, REPL, and playback environment aimed at music programming languages. The project is a polyglot platform for composing, live-coding, and rendering music DSLs from one binary.

Five languages are currently supported:

- **[Alda](https://alda.io)** - Declarative music notation language

- **[Joy](https://github.com/shakfu/midi-langs/tree/main/projects/joy-midi)** - Concatenative (stack-based) functional language for music

- **[TR7](https://gitlab.com/jobol/tr7)** - R7RS-small Scheme with music extensions

- **[Bog](https://github.com/shakfu/bog)** - C implementation of [dogalog](https://github.com/danja/dogalog), a prolog-based beats-oriented language for music

- **[MHS](https://github.com/augustss/MicroHs)** - Micro Haskell with MIDI support for functional music programming

All are practical for daily live-coding, REPL sketches, and headless playback. The Alda and Joy MIDI cores are from the [midi-langs](https://github.com/shakfu/midi-langs) project. Languages register themselves via a modular dispatch system, allowing additional DSLs to be integrated without modifying core dispatch logic. Audio output is handled by the built-in [TinySoundFont](https://github.com/schellingb/TinySoundFont) synthesizer or, optionally, a [Csound](https://csound.com/) backend for advanced synthesis. macOS, Linux, and Windows are supported.

## Features

- **Vim-style editor** with INSERT/NORMAL modes, live evaluation shortcuts, and opt-in Lua scripting (built on [loki](https://github.com/shakfu/loki), a fork of [kilo](https://github.com/antirez/kilo))

- **Split window support** with vim-style `Ctrl-W` commands for horizontal/vertical splits, pane navigation, and per-pane independent view state

- **TOML configuration** via `.psnd/config.toml` for themes, keybindings, audio settings, and editor options

- **Tree-sitter syntax highlighting** in REPLs with 17 TOML themes (monokai, dracula, nord, gruvbox, solarized, catppuccin, tokyo-night, kanagawa, and more) plus custom theme support via `.psnd/themes/`

- **MIDI tracker/step sequencer** with terminal UI, plugin-based cell notation, and pattern looping

- **Native webview mode** for a self-contained GUI window without requiring a browser (optional)

- **Web-based editor** accessible via browser using xterm.js terminal emulator (optional)

- **Language-aware REPLs** for interactive composition (Alda, Joy, TR7 Scheme, Bog, MHS)

- **Headless play mode** for batch jobs and automation

- **Non-blocking async playback** through [libuv](https://github.com/libuv/libuv) - REPLs remain responsive during playback

- **Integrated MIDI routing** powered by [libremidi](https://github.com/celtera/libremidi)

- **MIDI file export** using [midifile](https://github.com/craigsapp/midifile)

- **TinySoundFont synthesizer** built on [miniaudio](https://github.com/mackron/miniaudio)

- **Optional [Csound](https://csound.com/) backend** for deeper sound design workflows

- **[Ableton Link](https://github.com/Ableton/link) support** for networked tempo sync (playback matches Link session tempo)

- **[OSC (Open Sound Control)](http://opensoundcontrol.org/) support** for remote control and inter-application communication (optional)

- **Parameter binding** for MIDI CC and OSC control of named parameters from physical controllers

- **[Scala .scl](https://www.huygens-fokker.org/scala/scl_format.html) import support** for microtuning

- **Lua APIs** (opt-in) for editor automation, playback control, and extensibility

## Status

psnd is in active development. All five languages register through the same uniform integration layer (`lang_dispatch.h` plus the `LokiLangOps` editor bridge), demonstrating the polyglot architecture and letting new DSLs be added without modifying core dispatch logic. They differ in maturity: Alda and Joy are mature, fully bespoke implementations; Bog is a bespoke implementation in beta; TR7 Scheme and MHS are adapters over their upstream interpreters. Additional mini MIDI languages from [midi-langs](https://github.com/shakfu/midi-langs) can reuse the same editor, REPL, and audio stack. Expect iteration and occasional breaking changes as polyglot support expands.

## Prerequisites

**macOS:**

```bash
brew install cmake flex bison
```

**Linux (Debian/Ubuntu):**

```bash
sudo apt install cmake flex bison
```

**Linux (Fedora/RHEL):**

```bash
sudo dnf install cmake flex bison
```

**Linux (Arch):**

```bash
sudo pacman -S cmake flex bison
```

**Windows:**

```powershell
# Using Visual Studio 2022 with C++ workload
# Install CMake from https://cmake.org/download/
# Install winflexbison from https://github.com/lexxmark/winflexbison/releases
# Add winflexbison to PATH
```

Or with vcpkg:

```powershell
vcpkg install winflexbison
```

`flex` and `bison` are required for building the MHS (Micro Haskell) language component.

## Building

Build presets select the synthesizer backend and optional features:

| Target | Alias | Description |
|--------|-------|-------------|
| `make psnd-tsf` | `make`, `make default`, `make build` | TinySoundFont only |
| `make psnd-tsf-csound` | `make csound` | TinySoundFont + Csound |
| `make psnd-fluid` | | FluidSynth only (higher quality) |
| `make psnd-fluid-csound` | | FluidSynth + Csound |
| `make psnd-tsf-web` | `make web` | TinySoundFont + Web UI |
| `make psnd-fluid-web` | | FluidSynth + Web UI |
| `make psnd-fluid-csound-web` | `make full` | Everything |
| `make psnd-minihost` | `make minihost` | TinySoundFont + audio plugin host |
| `make psnd-minihost-csound` | | TinySoundFont + plugin host + Csound |

All of the above include MHS (Micro Haskell) by default - see the variants below for smaller builds. `make psnd-tsf` is the smallest of the *backend* choices, not the smallest binary overall.

Utility targets:

| Target | Description |
|--------|-------------|
| `make psnd` | Build just the `psnd` binary (TinySoundFont config) |
| `make library` | Build just `libloki` |
| `make rebuild` | `clean`, then build TinySoundFont + Csound, then `test` |
| `make reset` | Delete the build directory |
| `make remake` | `reset`, then a default build |
| `make test-minihost` | Build the plugin-host config and run its tests |

**MHS (Micro Haskell) build variants:**

| Target | Binary Size | Description |
|--------|-------------|-------------|
| `make` (default) | ~5.7MB | Full MHS with fast startup (~2s) and compilation support |
| `make mhs-small` | ~4.5MB | MHS without compilation to executable |
| `make mhs-src` | ~4.1MB | MHS with source embedding (~17s startup) |
| `make mhs-src-small` | ~2.9MB | Smallest binary with MHS |
| `make no-mhs` | ~2.1MB | MHS disabled entirely |

**Synth backends** (mutually exclusive at compile time):

- **TinySoundFont** - Lightweight SoundFont synthesizer, fast compilation

- **FluidSynth** - Higher quality synthesis, more SoundFont features

CMake options (for custom builds):

```bash
cmake -B build -DBUILD_FLUID_BACKEND=ON  # Use FluidSynth instead of TSF
cmake -B build -DBUILD_CSOUND_BACKEND=ON # Enable Csound synthesis
cmake -B build -DBUILD_WEB_HOST=ON       # Enable web server mode
cmake -B build -DBUILD_WEBVIEW_HOST=ON   # Enable native webview mode
cmake -B build -DBUILD_OSC=ON            # Enable OSC (Open Sound Control) support
cmake -B build -DBUILD_PLUGIN_SQLITE=ON  # Enable SQLite FTS5 search index
cmake -B build -DLOKI_EMBED_XTERM=ON     # Embed xterm.js in binary (no CDN)
```

## Usage

psnd exposes three complementary workflows: REPL mode for interactive sketching, editor mode for live-coding within files, and play mode for headless rendering. Running `psnd` with no arguments displays help. Flags (soundfont, csound instruments, etc.) are shared between modes.

### REPL Mode

**Alda REPL**:

```bash
psnd alda               # Start Alda REPL
psnd alda -sf gm.sf2    # REPL with built-in synth
```

Type Alda notation directly:

```text
alda> piano: c d e f g
alda> violin: o5 a b > c d e
alda> :stop
alda> :q
```

**Joy REPL**:

```bash
psnd joy                # Start Joy REPL
psnd joy --virtual out  # Joy REPL with named virtual port
psnd joy -p 0           # Joy REPL using MIDI port 0
```

Type Joy code directly:

```text
joy> :virtual
joy> 120 tempo
joy> [c d e f g] play
joy> c major chord
joy> [c e g] [d f a] [e g b] each chord
joy> :q
```

**TR7 Scheme REPL**:

```bash
psnd tr7                # Start TR7 Scheme REPL
psnd scheme             # Alias for tr7
psnd tr7 -sf gm.sf2     # REPL with built-in synth
psnd tr7 --virtual out  # TR7 REPL with named virtual port
psnd tr7 song.scm       # Run Scheme file
```

Type Scheme code directly:

```text
tr7> (midi-virtual "TR7Out")
tr7> (set-tempo 120)
tr7> (play-note 60 80 500)      ; note 60, velocity 80, 500ms
tr7> (play-chord '(60 64 67) 80 500)  ; C major chord
tr7> (set-octave 5)
tr7> :q
```

**Bog REPL**:

```bash
psnd bog                # Start Bog REPL
psnd bog --virtual out  # Bog REPL with named virtual port
psnd bog -sf gm.sf2     # Bog REPL with built-in synth
```

Type Bog rules directly:

```text
bog> :def kick event(kick, 36, 0.9, T) :- every(T, 1.0).
Slot 'kick' defined (new)
bog> :def hat event(hat, 42, 0.5, T) :- every(T, 0.25).
Slot 'hat' defined (new)
bog> :slots
Slots:
  kick: event(kick, 36, 0.9, T) :- every(T, 1.0).
  hat: event(hat, 42, 0.5, T) :- every(T, 0.25).
bog> :mute hat
bog> :solo kick
bog> :q
```

**MHS REPL** (Micro Haskell):

```bash
psnd mhs                # Start MHS REPL
psnd mhs -r file.hs     # Run a Haskell file
psnd mhs -oMyProg file.hs  # Compile to executable
```

Type Haskell code directly:

```text
mhs> import Midi
mhs> midiInit
mhs> midiOpenVirtual "MHS-MIDI"
mhs> midiNoteOn 0 60 100
mhs> midiSleep 500
mhs> midiNoteOff 0 60
mhs> :quit
```

**MHS-specific commands** (MicroHs built-in commands also work):

| Command | Action |
|---------|--------|
| `:type EXPR` | Show type of expression |
| `:kind TYPE` | Show kind of type |
| `:browse MODULE` | List exports from module |
| `:quit` | Exit MicroHs REPL |

**Shared REPL Commands** (work in Alda, Joy, TR7, Bog, and MHS, with or without `:`):

| Command | Action |
|---------|--------|
| `:q` `:quit` `:exit` | Exit REPL |
| `:h` `:help` `:?` | Show help |
| `:l` `:list` | List MIDI ports |
| `:s` `:stop` | Stop playback |
| `:p` `:panic` | All notes off |
| `:play PATH` | Play file (dispatches by extension) |
| `:sf PATH` | Load soundfont and enable built-in synth |
| `:presets` | List soundfont presets |
| `:midi` | Switch to MIDI output |
| `:synth` `:builtin` | Switch to built-in synth |
| `:virtual [NAME]` | Create virtual MIDI port |
| `:link [on\|off]` | Enable/disable Ableton Link |
| `:link-tempo BPM` | Set Link tempo |
| `:link-status` | Show Link status |
| `:cs PATH` | Load CSD file and enable Csound |
| `:csound` | Enable Csound backend |
| `:cs-disable` | Disable Csound |
| `:cs-status` | Show Csound status |
| `:theme` | List available color themes |
| `:theme NAME` | Switch to a color theme |

**Alda-specific commands**:

| Command | Action |
|---------|--------|
| `:sequential` | Wait for each input to complete |
| `:concurrent` | Enable polyphonic playback (default) |
| `:export FILE` | Export to MIDI file |

**Joy-specific commands**:

| Command | Action |
|---------|--------|
| `.` | Print stack |

**Bog-specific commands**:

| Command | Action |
|---------|--------|
| `:def NAME RULE` | Define a named slot |
| `:undef NAME` | Remove a named slot |
| `:slots` `:ls` | Show all defined slots |
| `:clear` | Remove all slots |
| `:mute NAME` | Mute a slot |
| `:unmute NAME` | Unmute a slot |
| `:solo NAME` | Solo a slot (mute all others) |
| `:unsolo` | Unmute all slots |
| `:tempo BPM` | Set tempo |
| `:swing AMOUNT` | Set swing (0.0-1.0) |

### Editor Mode

```bash
psnd song.alda                        # Open Alda file in editor
psnd song.joy                         # Open Joy file in editor
psnd song.scm                         # Open Scheme file in editor
psnd song.bog                         # Open Bog file in editor
psnd song.csd                         # Open Csound file in editor
psnd -sf gm.sf2 song.alda             # Editor with TinySoundFont synth
psnd -cs instruments.csd song.alda    # Editor with Csound synthesis
```

Keybindings:

| Key | Action |
|-----|--------|
| `Ctrl-E` | Play current part/line (or selection) |
| `Ctrl-P` | Play entire file |
| `Ctrl-G` | Stop playback |
| `Ctrl-S` | Save |
| `Ctrl-Q` | Quit |
| `Ctrl-F` | Find |
| `Ctrl-L` | Lua console |
| `i` | Enter INSERT mode |
| `ESC` | Return to NORMAL mode |

Split Window Commands (in NORMAL mode):

| Key | Action |
|-----|--------|
| `Ctrl-W s` | Split horizontally (top/bottom) |
| `Ctrl-W v` | Split vertically (left/right) |
| `Ctrl-W h` | Move to left pane |
| `Ctrl-W j` | Move to pane below |
| `Ctrl-W k` | Move to pane above |
| `Ctrl-W l` | Move to right pane |
| `Ctrl-W w` | Cycle to next pane |
| `Ctrl-W c` | Close current pane |
| `Ctrl-W =` | Equalize split ratios |
| `Ctrl-W n` | New buffer in current pane |
| `Ctrl-W b` | Cycle buffers in current pane |

Ex Commands (press `:` in NORMAL mode):

| Command | Action |
|---------|--------|
| `:w` | Save file |
| `:q` | Quit (warns if unsaved) |
| `:wq` | Save and quit |
| `:q!` | Quit without saving |
| `:e FILE` | Open file |
| `:123` | Go to line 123 |
| `:goto 123` | Go to line 123 |
| `:s/old/new/` | Replace first occurrence on line |
| `:s/old/new/g` | Replace all occurrences on line |
| `:help` | Show help |
| `:link` | Toggle Ableton Link |
| `:csd` | Toggle Csound synthesis |
| `:export FILE` | Export to MIDI file |
| `:theme` | Open theme picker (full-screen selection) |
| `:theme NAME` | Switch to named color theme |
| `:plugin presets` | Open preset picker (for loaded plugins) |

Picker Keybindings (when picker is open):

| Key | Action |
|-----|--------|
| `j` / `DOWN` | Move selection down |
| `k` / `UP` | Move selection up |
| `g` / `G` | Go to top / bottom |
| `CTRL-D` / `CTRL-U` | Page down / up |
| `ENTER` | Select item |
| `ESC` / `q` | Cancel |

### Play Mode

```bash
psnd play song.alda              # Play Alda file and exit
psnd play song.joy               # Play Joy file and exit
psnd play song.scm               # Play Scheme file and exit
psnd play song.bog               # Play Bog file and exit
psnd play song.csd               # Play Csound file and exit
psnd play -sf gm.sf2 song.alda   # Play Alda with built-in synth
psnd play -v song.csd            # Play with verbose output
```

### Web Mode

Run psnd as a web server and access the editor through a browser using xterm.js terminal emulation.

```bash
psnd --web                           # Start web server on port 8080
psnd --web --web-port 3000           # Use custom port
psnd --web --web-host 0.0.0.0        # Expose to the network (opt-in)
psnd --web --web-open                # Also open the URL in the default browser
psnd --web song.alda                 # Open file in web editor
psnd --web -sf gm.sf2 song.joy       # Web editor with soundfont
```

Open the URL printed at startup, or pass `--web-open` to have psnd hand it to the default browser. It carries a random per-session token that the WebSocket and `/api` endpoints require, so `http://localhost:8080` without it loads the page but cannot connect. The server binds `127.0.0.1` unless `--web-host` says otherwise.

**Features:**

- Full terminal emulation via xterm.js

- Mouse click-to-position support

- Language switching with `:alda`, `:joy`, `:langs` commands

- First-line directives (`#alda`, `#joy`) for automatic language detection

- All editor keybindings work as in terminal mode

**Build requirement:** Web mode requires building with `-DBUILD_WEB_HOST=ON`.

**Embedded mode:** Build with `-DLOKI_EMBED_XTERM=ON` to embed xterm.js in the binary, eliminating CDN dependency for offline use.

### Native Webview Mode

Run psnd in a native window using the system's webview (WebKit on macOS, WebKitGTK on Linux). This provides the same xterm.js-based UI as web mode but in a self-contained native application - no browser required.

```bash
psnd --native song.alda                  # Open file in native window
psnd --native -sf gm.sf2 song.joy        # Native window with soundfont
```

**Features:**

- Same UI as web mode (xterm.js terminal emulation)

- Play/Stop/Eval buttons in toolbar

- All editor keybindings work as in terminal mode

- Works completely offline

- Clean window close handling

**Build requirement:** Native webview mode requires building with `-DBUILD_WEBVIEW_HOST=ON`.

**Platform dependencies:**

- **macOS**: WebKit framework (always available)

- **Linux**: GTK3 and WebKitGTK (`libgtk-3-dev libwebkit2gtk-4.0-dev`)

### Piped Input

All REPLs support non-interactive piped input for scripting and automation:

```bash
# Alda REPL
echo 'piano: c d e f g' | psnd alda
echo -e 'piano: c d e\n:q' | psnd alda

# Joy REPL
echo '[c d e] play' | psnd joy
printf ':cs synth.csd\n:cs-status\n:q\n' | psnd joy

# TR7 Scheme REPL
echo '(play-note 60 80 500)' | psnd tr7

# Bog REPL
echo ':def kick event(kick, 36, 0.9, T) :- every(T, 1.0).' | psnd bog
```

This is useful for testing, CI/CD pipelines, and batch processing.

## Configuration

psnd uses TOML for configuration. Settings are loaded from `.psnd/config.toml` (project-local) or `~/.psnd/config.toml` (home directory).

### Example config.toml

```toml
[editor]
theme = "nord"           # Color theme (see .psnd/themes/)
line_numbers = true      # Show line numbers
tab_width = 4            # Tab width for indentation

[editor.lua]
enabled = false          # Opt-in Lua scripting (disabled by default)

[audio]
backend = "tsf"          # "tsf", "fluid", "csound", or "midi"
soundfont = ""           # Default SoundFont when -sf is absent (~/ allowed)

[link]
enabled = false          # Ableton Link sync
tempo = 120              # Default tempo

# Available commands: save, find, stop, lua_repl, new_buffer, copy, word_wrap
# Note: quit, eval_line, play_file have special built-in handling
# Note: ctrl-w is reserved for split window commands
[keybindings]
"ctrl-s" = "save"
"ctrl-g" = "stop"
"ctrl-f" = "find"
"ctrl-l" = "lua_repl"    # Only works when Lua is enabled
"ctrl-t" = "new_buffer"
"ctrl-c" = "copy"
```

### Default SoundFont

Without `-sf`, psnd picks a SoundFont in this order:

1. `PSND_SOUNDFONT` environment variable (`none` turns off 2 and 3)

2. `[audio] soundfont` in `config.toml`

3. The first General MIDI SoundFont found in `.psnd/soundfonts/`, `~/.psnd/soundfonts/`, `~/.aldakit/soundfonts/`, `~/.local/share/soundfonts/`, `~/.local/share/sounds/sf2/`, then the system directories (`/usr/share/sounds/sf2/` and `/usr/share/soundfonts/` on Linux, `/Library/Audio/Sounds/Banks/` on macOS)

A SoundFont found by search (3) is used only when no MIDI output can reach a synth. ALSA's "Midi Through" and PipeWire's own ports do not count. `-p`, `--virtual`, `-cs` and `--plugin` always take precedence.

### Lua Scripting (Opt-in)

Lua scripting is disabled by default for security. To enable it, set `editor.lua.enabled = true` in your config.toml. When enabled:

- `.psnd/init.lua` is loaded at startup

- `Ctrl-L` opens the Lua console

- Custom commands and keybindings via Lua are available

- Full `loki.*` Lua API is accessible

When Lua is disabled, core functionality (Alda, Joy, Bog, themes, keybindings, audio) works normally.

## Lua Scripting

Press `Ctrl-L` in the editor to access the Lua console (requires `editor.lua.enabled = true`):

```lua
-- Play Alda code
loki.alda.eval_sync("piano: c d e f g a b > c")

-- Async playback with callback
loki.alda.eval("piano: c d e f g", "on_done")

-- Stop playback
loki.alda.stop_all()

-- Load soundfont for built-in synth
loki.alda.load_soundfont("path/to/soundfont.sf2")
loki.alda.set_synth(true)
```

## Joy Language

Joy is a concatenative (stack-based) language for music composition. It provides a different paradigm from Alda's notation-based approach.

### Quick Start

```bash
psnd joy                    # Start Joy REPL
psnd song.joy               # Edit Joy file
psnd play song.joy          # Play Joy file headlessly
```

### Basic Syntax

Joy uses postfix notation where operations follow their arguments. Playback is **non-blocking** - the REPL remains responsive while notes play in the background:

```joy
\ Comments start with backslash
:virtual                    \ Create virtual MIDI port
120 tempo                   \ Set tempo to 120 BPM
80 vol                      \ Set volume to 80

\ Play notes (non-blocking)
c play                      \ Play middle C
[c d e f g] play            \ Play a melody
[c d e] play [f g a] play   \ Layer multiple phrases

\ Chords
[c e g] chord               \ Play C major chord
c major chord               \ Same thing using music theory
a minor chord               \ A minor chord
g dom7 chord                \ G dominant 7th

\ Direct MIDI control
60 80 500 midi-note         \ Note 60, velocity 80, 500ms duration
```

### Music Theory Primitives

Joy includes music theory primitives for building chords:

| Primitive | Description | Example |
|-----------|-------------|---------|
| `major` | Major triad | `c major chord` |
| `minor` | Minor triad | `a minor chord` |
| `dom7` | Dominant 7th | `g dom7 chord` |
| `maj7` | Major 7th | `c maj7 chord` |
| `min7` | Minor 7th | `a min7 chord` |
| `dim` | Diminished triad | `b dim chord` |
| `aug` | Augmented triad | `c aug chord` |

### Stack Operations

Joy is stack-based, so values are pushed onto a stack and operations consume them:

```joy
60 dup                      \ Duplicate: [60 60]
60 70 swap                  \ Swap: [70 60]
60 70 pop                   \ Pop: [60]
[1 2 3] [dup *] map         \ Map: [1 4 9]
```

### Lua API

```lua
-- Initialize Joy
loki.joy.init()

-- Evaluate Joy code
loki.joy.eval(":virtual 120 tempo [c d e] play")

-- Define a custom word
loki.joy.define("cmaj", "[c e g] chord")

-- Stop playback
loki.joy.stop()
```

## TR7 Scheme Language

TR7 is an R7RS-small Scheme interpreter with music extensions. It provides a Lisp-based approach to music composition.

### Quick Start

```bash
psnd tr7                    # Start TR7 REPL
psnd tr7 song.scm           # Run Scheme file
psnd song.scm               # Edit Scheme file
psnd play song.scm          # Play Scheme file headlessly
```

### Music Primitives

TR7 extends R7RS-small Scheme with music-specific procedures:

| Procedure | Description |
|-----------|-------------|
| `(play-note pitch velocity duration-ms)` | Play a single note (non-blocking) |
| `(play-chord '(p1 p2 ...) velocity duration-ms)` | Play a chord (non-blocking) |
| `(play-seq '(p1 p2 ...) velocity duration-ms)` | Play notes in sequence (non-blocking) |
| `(note-on pitch velocity)` | Send note-on message |
| `(note-off pitch)` | Send note-off message |
| `(set-tempo bpm)` | Set tempo |
| `(set-octave n)` | Set octave (0-9) |
| `(set-velocity v)` | Set velocity (0-127) |
| `(set-channel ch)` | Set MIDI channel (0-15) |
| `(program-change prog)` | Change instrument |
| `(control-change cc value)` | Send CC message |
| `(note name [octave])` | Convert note name to MIDI number |

### MIDI Control

| Procedure | Description |
|-----------|-------------|
| `(midi-list)` | List available MIDI ports |
| `(midi-open port)` | Open MIDI port by index |
| `(midi-virtual name)` | Create virtual MIDI port |
| `(midi-panic)` | All notes off |
| `(tsf-load path)` | Load soundfont for built-in synth |
| `(sleep-ms ms)` | Sleep for milliseconds |

### Example

```scheme
; TR7 music composition example
(midi-virtual "TR7Out")
(set-tempo 120)
(set-velocity 80)

; Play a C major scale
(define (play-scale)
  (for-each (lambda (n)
              (play-note n 80 250))
            '(60 62 64 65 67 69 71 72)))

; Play a chord progression
(play-chord '(60 64 67) 80 500)  ; C major
(play-chord '(65 69 72) 80 500)  ; F major
(play-chord '(67 71 74) 80 500)  ; G major
(play-chord '(60 64 67) 80 1000) ; C major
```

## Bog Language

Bog is a Prolog-based live coding language for music, inspired by [dogalog](https://github.com/danja/dogalog). Musical events emerge from declarative logic rules rather than imperative sequences.

### Quick Start

```bash
psnd bog                    # Start Bog REPL
psnd bog song.bog           # Run Bog file
psnd song.bog               # Edit Bog file
psnd play song.bog          # Play Bog file headlessly
```

### Core Concept

All Bog patterns produce `event/4` facts:

```prolog
event(Voice, Pitch, Velocity, Time)
```

- **Voice**: Sound source (`kick`, `snare`, `hat`, `clap`, `noise`, `sine`, `square`, `triangle`)

- **Pitch**: MIDI note number (0-127) or ignored for drums

- **Velocity**: Intensity (0.0-1.0)

- **Time**: Beat time (bound by timing predicates)

### Timing Predicates

| Predicate | Description | Example |
|-----------|-------------|---------|
| `every(T, N)` | Fire every N beats | `every(T, 0.5)` - 8th notes |
| `beat(T, N)` | Fire on beat N of bar | `beat(T, 1)` - beat 1 |
| `euc(T, K, N, B, R)` | Euclidean rhythm | `euc(T, 5, 16, 4, 0)` - 5 hits over 16 steps |

### Named Slots

The REPL uses named slots to manage multiple concurrent patterns:

```prolog
bog> :def kick event(kick, 36, 0.9, T) :- every(T, 1.0).
bog> :def snare event(snare, 38, 0.8, T) :- every(T, 2.0).
bog> :def hat event(hat, 42, 0.5, T) :- every(T, 0.25).
bog> :slots           % List all patterns
bog> :mute hat        % Mute hi-hat
bog> :solo kick       % Solo kick drum
bog> :undef snare     % Remove snare pattern
bog> :clear           % Remove all patterns
```

### Example Patterns

```prolog
% Basic four-on-the-floor
event(kick, 36, 0.9, T) :- every(T, 1.0).
event(snare, 38, 0.8, T) :- beat(T, 2), beat(T, 4).
event(hat, 42, 0.5, T) :- every(T, 0.5).

% Euclidean breakbeat
event(kick, 36, 0.9, T) :- euc(T, 5, 16, 4, 0).
event(snare, 38, 0.85, T) :- euc(T, 3, 8, 4, 2).

% Random variation
event(kick, 36, Vel, T) :- every(T, 1.0), choose(Vel, [0.7, 0.8, 0.9, 1.0]).
event(hat, 42, 0.5, T) :- every(T, 0.25), chance(0.7, true).
```

### Lua API

```lua
-- Initialize Bog
loki.bog.init()

-- Evaluate Bog code
loki.bog.eval("event(kick, 36, 0.9, T) :- every(T, 1.0).")

-- Stop playback
loki.bog.stop()

-- Set tempo and swing
loki.bog.set_tempo(140)
loki.bog.set_swing(0.3)
```

## MHS Language

MHS (Micro Haskell) is a lightweight Haskell implementation with MIDI support, providing functional programming for music composition.

### Quick Start

```bash
psnd mhs                    # Start MHS REPL
psnd mhs --virtual MHS-Out  # REPL with virtual MIDI port
psnd mhs -sf gm.sf2         # REPL with built-in synth
psnd mhs -r file.hs         # Run a Haskell file
psnd mhs -oMyProg file.hs   # Compile to standalone executable
psnd mhs -oMyProg.c file.hs # Output C code only
```

### REPL Features

The MHS REPL provides full feature parity with other psnd languages:

- **Syntax highlighting** for Haskell keywords, types, and MIDI primitives

- **Tab completion** for 80+ Haskell keywords and MIDI functions

- **History persistence** (`~/.psnd/mhs_history`)

- **All shared commands** (`:help`, `:stop`, `:panic`, `:list`, `:sf`, `:link`, etc.)

- **Ableton Link integration** for tempo sync

**Architecture:** MicroHs runs in a forked child process with a pseudo-terminal (PTY). The parent process handles psnd's syntax-highlighted input and forwards Haskell code to MicroHs via the PTY. MIDI is initialized in the child process after fork.

### Available Modules

| Module | Description |
|--------|-------------|
| `Midi` | Low-level MIDI I/O (ports, note on/off, control change) |
| `Music` | High-level music notation (notes, chords, sequences) |
| `MusicPerform` | Music performance/playback |
| `MidiPerform` | MIDI event scheduling |
| `Async` | Asynchronous operations |

### Example

```haskell
import Midi

main :: IO ()
main = do
    midiInit
    midiOpenVirtual "MHS-MIDI"

    -- Play a C major chord
    midiNoteOn 0 60 100  -- C
    midiNoteOn 0 64 100  -- E
    midiNoteOn 0 67 100  -- G
    midiSleep 1000
    midiNoteOff 0 60
    midiNoteOff 0 64
    midiNoteOff 0 67

    midiCleanup
```

### Build Variants

MHS can be built with different configurations to trade off binary size vs features:

| Target | Size | Startup | Features |
|--------|------|---------|----------|
| `make` | 5.7MB | ~2s | Full: packages + compilation |
| `make mhs-small` | 4.5MB | ~2s | Packages, no compilation |
| `make mhs-src` | 4.1MB | ~17s | Source embedding + compilation |
| `make mhs-src-small` | 2.9MB | ~17s | Source only, no compilation |
| `make no-mhs` | 2.1MB | N/A | MHS disabled |

See `source/langs/mhs/README.md` for detailed documentation on VFS embedding, compilation, and standalone builds.

## Tracker Sequencer

psnd includes a MIDI tracker/step sequencer with a terminal-based UI, inspired by classic trackers like FastTracker and Renoise.

### Quick Start

Run the interactive demo:

```bash
./build/tests/tracker/tracker_demo ~/Music/sf2/FluidR3_GM.sf2
```

### Controls

| Key | Action |
|-----|--------|
| `h/j/k/l` or arrows | Navigate cells |
| `Enter` or `i` | Edit cell |
| `Escape` | Exit edit mode / Quit |
| `Space` | Play/Stop |
| `q` or `Q` | Quit |

### Cell Notation (Notes Plugin)

The tracker uses a simple note notation language:

| Syntax | Description |
|--------|-------------|
| `C4` | Middle C |
| `D#5` | D sharp, octave 5 |
| `Bb3` | B flat, octave 3 |
| `C4@80` | C4 with velocity 80 |
| `C4~2` | C4 held for 2 rows |
| `C4 E4 G4` | C major chord |
| `r` or `-` | Rest |
| `x` or `off` | Note off |

### Architecture

The tracker is built with a modular plugin system:

- **`tracker_model`** - Data structures (songs, patterns, tracks, cells)

- **`tracker_plugin`** - Plugin system for notation languages

- **`tracker_engine`** - Playback engine with event scheduling

- **`tracker_view`** - View layer (theme, undo, clipboard, JSON)

- **`tracker_audio`** - Audio integration with SharedContext

### API Example

```c
// Create a song and pattern
TrackerSong* song = tracker_song_new("My Song");
TrackerPattern* pattern = tracker_pattern_new(16, 4, "Pattern 1");
tracker_song_add_pattern(song, pattern);

// Add notes to cells
TrackerCell* cell = tracker_pattern_get_cell(pattern, 0, 0);
tracker_cell_set_expression(cell, "C4@80", "notes");

// Create engine and connect to audio
TrackerEngine* engine = tracker_audio_engine_new(&audio_ctx);
tracker_engine_load_song(engine, song);

// Start playback
tracker_engine_play(engine);
```

## Ableton Link

psnd supports [Ableton Link](https://www.ableton.com/en/link/) for tempo synchronization with other musicians and applications on the same network.

### Quick Start

In the editor, use the `:link` command:

```text
:link on       # Enable Link
:link off      # Disable Link
:link          # Toggle Link
```

In REPLs, use the same commands:

```text
alda> :link on
[Link] Peers: 1
[Link] Tempo: 120.0 BPM
alda> piano: c d e f g    # Plays at Link session tempo
```

When Link is enabled:

- Status bar shows "ALDA LINK" instead of "ALDA NORMAL"

- **Playback tempo automatically syncs** with the Link session for all languages (Alda, Joy, TR7)

- REPLs print notifications when tempo, peers, or transport state changes

- Other Link-enabled apps (Ableton Live, etc.) share the same tempo

### Lua API

```lua
-- Initialize and enable Link
loki.link.init(120)           -- Initialize with 120 BPM
loki.link.enable(true)        -- Start networking

-- Tempo control
loki.link.tempo()             -- Get session tempo
loki.link.set_tempo(140)      -- Set tempo (syncs to all peers)

-- Session info
loki.link.peers()             -- Number of connected peers
loki.link.beat(4)             -- Current beat (4 beats per bar)
loki.link.phase(4)            -- Phase within bar [0, 4)

-- Transport sync (optional)
loki.link.start_stop_sync(true)
loki.link.play()              -- Start transport
loki.link.stop()              -- Stop transport
loki.link.is_playing()        -- Check transport state

-- Callbacks (called when values change)
loki.link.on_tempo("my_tempo_handler")
loki.link.on_peers("my_peers_handler")
loki.link.on_start_stop("my_transport_handler")

-- Cleanup
loki.link.cleanup()
```

## OSC (Open Sound Control)

psnd supports [OSC](http://opensoundcontrol.org/) for remote control and communication with other music software like SuperCollider, Max/MSP, Pure Data, and hardware controllers.

### Building with OSC

```bash
cmake -B build -DBUILD_OSC=ON && make    # Build with OSC support
```

### Quick Start

```bash
# Start editor with OSC server on default port (7770)
psnd --osc song.alda

# Use custom port
psnd --osc-port 8000 song.alda

# Also broadcast events to another application
psnd --osc --osc-send 127.0.0.1:9000 song.alda
```

### Incoming Messages

Control psnd from external applications:

| Address | Arguments | Description |
|---------|-----------|-------------|
| `/psnd/ping` | none | Connection test (replies `/psnd/pong`) |
| `/psnd/tempo` | float bpm | Set tempo |
| `/psnd/note` | int ch, int pitch, int vel | Play note (note on + scheduled off) |
| `/psnd/noteon` | int ch, int pitch, int vel | Note on |
| `/psnd/noteoff` | int ch, int pitch | Note off |
| `/psnd/cc` | int ch, int cc, int val | Control change |
| `/psnd/pc` | int ch, int prog | Program change |
| `/psnd/bend` | int ch, int val | Pitch bend (-8192 to 8191) |
| `/psnd/panic` | none | All notes off |
| `/psnd/play` | none | Play entire file |
| `/psnd/stop` | none | Stop all playback |
| `/psnd/eval` | string code | Evaluate code string |

### Outgoing Messages

When a broadcast target is set (`--osc-send`), psnd automatically sends messages for state changes and MIDI events, regardless of what triggered them (keyboard, Lua API, or incoming OSC):

| Address | Arguments | Description |
|---------|-----------|-------------|
| `/psnd/pong` | none | Reply to ping |
| `/psnd/status/playing` | int playing | Playback state (1=playing, 0=stopped) |
| `/psnd/status/tempo` | float bpm | Tempo changes |
| `/psnd/midi/note` | int ch, int pitch, int vel | Note on (vel>0) or off (vel=0) |

### Example: SuperCollider

```supercollider
// Send notes to psnd
n = NetAddr("127.0.0.1", 7770);
n.sendMsg("/psnd/note", 0, 60, 100);    // C4
n.sendMsg("/psnd/note", 0, 64, 100);    // E4
n.sendMsg("/psnd/note", 0, 67, 100);    // G4
n.sendMsg("/psnd/tempo", 140.0);        // Set tempo
n.sendMsg("/psnd/panic");               // Stop all notes

// Playback control
n.sendMsg("/psnd/play");                // Play entire file
n.sendMsg("/psnd/stop");                // Stop playback
n.sendMsg("/psnd/eval", "piano: c d e f g");  // Evaluate code
```

### Example: Pure Data / Max

Send messages to `udp 127.0.0.1 7770`:

- `/psnd/note 0 60 100` - Play middle C

- `/psnd/tempo 120` - Set tempo

- `/psnd/panic` - Stop all notes

### Lua API

Control OSC from Lua scripts and init.lua:

```lua
-- Initialize and start OSC server
osc.init(7770)           -- Initialize on port 7770 (default)
osc.start()              -- Start the server

-- Check status
osc.enabled()            -- Returns true if running
osc.port()               -- Returns current port number

-- Set broadcast target for outgoing messages
osc.broadcast("localhost", 8000)

-- Send messages
osc.send("/my/path", 1, 2.5, "hello")              -- To broadcast target
osc.send_to("192.168.1.100", 9000, "/custom", 42)  -- To specific address

-- Register callbacks (callback is called by function name)
osc.on("/my/handler", "my_callback_function")
osc.off("/my/handler")   -- Remove handler

-- Stop server
osc.stop()
```

Type auto-detection for `osc.send()` and `osc.send_to()`:

- Lua integers become OSC int32 (`i`)

- Lua floats become OSC float (`f`)

- Lua strings become OSC string (`s`)

- Lua booleans become OSC true/false (`T`/`F`)

- Lua nil becomes OSC nil (`N`)

### Design Document

See `docs/PSND_OSC.md` for the complete OSC address namespace specification and implementation details.

## Parameter Binding System

psnd supports binding named parameters to MIDI CC and OSC addresses, enabling physical controllers (knobs, faders) to modify variables that affect music generation in real-time.

### Quick Start

```lua
-- In Lua console (Ctrl-L) or init.lua

-- Define a parameter with range and default
param.define("cutoff", { min = 20, max = 20000, default = 1000 })
param.define("resonance", { min = 0, max = 1, default = 0.5 })

-- Bind to MIDI CC (channel 1, CC 74)
midi.in_open_virtual("PSND_MIDI_IN")  -- Create virtual MIDI input
param.bind_midi("cutoff", 1, 74)      -- Moving CC 74 updates cutoff

-- Bind to OSC address
param.bind_osc("resonance", "/fader/2")  -- OSC messages update resonance

-- Read values from your music code
local val = param.get("cutoff")       -- Returns current value
```

### Joy Usage

```joy
\ Define parameter (from Lua first)
\ Then read in Joy code:
"cutoff" param            \ Push parameter value onto stack
5000 "cutoff" param!      \ Set parameter value
param-list                \ Print all parameters
```

### MIDI Input

Open a MIDI input port to receive CC messages:

```lua
-- List available input ports
midi.in_list_ports()

-- Open by index (1-based)
midi.in_open_port(1)

-- Or create a virtual input port
midi.in_open_virtual("MyController")

-- Check if open
midi.in_is_open()  -- true/false

-- Close when done
midi.in_close()
```

When MIDI CC messages arrive on a bound channel/CC, the parameter value is automatically updated with scaling from 0-127 to the parameter's min/max range.

### OSC Control

When OSC is enabled (`--osc`), parameters can be controlled via OSC messages:

```bash
# Set parameter value
oscsend localhost 7770 /psnd/param/set sf "cutoff" 5000.0

# Query parameter (replies with /psnd/param/value)
oscsend localhost 7770 /psnd/param/get s "cutoff"

# List all parameters
oscsend localhost 7770 /psnd/param/list
```

Parameters bound to custom OSC paths also respond to those paths:

```bash
# If bound: param.bind_osc("resonance", "/fader/2")
oscsend localhost 7770 /fader/2 f 0.75
```

### Lua API Reference

| Function | Description |
|----------|-------------|
| `param.define(name, opts)` | Define parameter (opts: min, max, default, type) |
| `param.undefine(name)` | Remove parameter definition |
| `param.get(name)` | Get current parameter value |
| `param.set(name, value)` | Set parameter value |
| `param.bind_osc(name, path)` | Bind parameter to OSC path |
| `param.unbind_osc(name)` | Remove OSC binding |
| `param.bind_midi(name, ch, cc)` | Bind to MIDI CC (channel 1-16, CC 0-127) |
| `param.unbind_midi(name)` | Remove MIDI binding |
| `param.list()` | Get table of all parameters |
| `param.info(name)` | Get detailed info (value, range, bindings) |
| `param.count()` | Number of defined parameters |

MIDI Input:

| Function | Description |
|----------|-------------|
| `midi.in_list_ports()` | Print available input ports |
| `midi.in_port_count()` | Get number of input ports |
| `midi.in_port_name(idx)` | Get port name (1-based index) |
| `midi.in_open_port(idx)` | Open input port by index |
| `midi.in_open_virtual(name)` | Create virtual input port |
| `midi.in_close()` | Close current input port |
| `midi.in_is_open()` | Check if input is open |

### Example: Filter Control

```lua
-- init.lua: Set up filter parameter with MIDI control

-- Define parameter
param.define("filter_cutoff", {
    type = "float",
    min = 100,
    max = 10000,
    default = 1000
})

-- Open MIDI input and bind to CC 74 (filter cutoff is often CC 74)
midi.in_open_virtual("PSND_Controller")
param.bind_midi("filter_cutoff", 1, 74)

-- Also allow OSC control
param.bind_osc("filter_cutoff", "/synth/filter")
```

Then in Joy:

```joy
\ Apply filter with parameter value
"filter_cutoff" param  \ Get current cutoff value
\ ... use value in synthesis/playback ...
```

### Thread Safety

Parameter values use atomic floats, making them safe to read from any thread (main, audio, MIDI callback, OSC handler) without locks. This is essential for real-time audio applications where mutex locks could cause audio glitches.

## Full-Text Search (SQLite FTS5)

psnd includes an optional SQLite FTS5 plugin for fast full-text search across `.psnd/` configuration files, modules, themes, and scales.

### Building with FTS

```bash
cmake -B build -DBUILD_PLUGIN_SQLITE=ON && make
```

Requires system SQLite3 with FTS5 support (standard on macOS 10.12+, most Linux distributions).

### Usage

**Ex-commands** (in editor):

```text
:search chord major      # Full-text search in file contents
:find *.alda             # Find files by path pattern
:reindex                 # Update index (incremental)
:rebuild-index           # Full reindex from scratch
:index-stats             # Show index statistics
```

**Lua API**:

```lua
-- Search content
local results = loki.fts.search("chord major", 20)
for _, r in ipairs(results) do
    print(r.path, r.snippet)
end

-- Search file paths
local files = loki.fts.find("*.lua")

-- Index management
loki.fts.index()           -- Incremental index of ~/.psnd
loki.fts.index(path, false) -- Full reindex of specific path
loki.fts.rebuild()         -- Full reindex of default path

-- Statistics
local stats = loki.fts.stats()
print(stats.file_count, stats.total_bytes)
```

### FTS5 Query Syntax

| Query | Description |
|-------|-------------|
| `chord` | Files containing "chord" |
| `"major chord"` | Exact phrase match |
| `chord AND major` | Both terms |
| `chord OR minor` | Either term |
| `cho*` | Prefix match (chord, chorus, etc.) |
| `path:alda` | Search in file paths only |

## MIDI Export

Export Alda compositions to Standard MIDI Files (.mid) for use in DAWs and other music software.

### Quick Start

1. Play some Alda code to generate events (Ctrl-E or Ctrl-P)

2. Run `:export song.mid` to export

### Lua API

```lua
-- Export to MIDI file
local ok, err = loki.midi.export("song.mid")
if not ok then
    loki.status("Export failed: " .. err)
end
```

### Notes

- Single-channel compositions export as Type 0 MIDI (single track)

- Multi-channel compositions export as Type 1 MIDI (multiple tracks)

- All events (notes, program changes, tempo, pan) are preserved

## Csound Synthesis

psnd optionally supports [Csound](https://csound.com/) as an advanced synthesis backend, providing full synthesis capabilities beyond TinySoundFont's sample playback.

### Building with Csound

```bash
make csound       # Build with Csound backend (~4.4MB binary)
```

### Quick Start

**Option 1: Command-line (recommended)**

```bash
psnd -cs .psnd/csound/default.csd song.alda
```

This loads the Csound instruments and enables Csound synthesis automatically when opening the file.

**Option 2: Ex commands in editor**

In the editor, use the `:csd` command after loading a .csd file:

```text
:csd on        # Enable Csound synthesis
:csd off       # Disable Csound, switch to TinySoundFont
:csd           # Toggle between Csound and TSF
```

Or via Lua:

```lua
-- Check if Csound is available
if loki.alda.csound_available() then
    -- Load Csound instruments
    loki.alda.csound_load(".psnd/csound/default.csd")

    -- Switch to Csound backend
    loki.alda.set_backend("csound")
end
```

### Standalone CSD Playback

You can also open and play `.csd` files directly without using them as MIDI instrument definitions:

```bash
psnd song.csd           # Edit CSD file, Ctrl-P to play
psnd play song.csd      # Headless playback
```

This plays the CSD file's embedded score section using Csound's native playback, not the MIDI-driven synthesis mode.

### Lua API

```lua
-- Check availability
loki.alda.csound_available()      -- true if compiled with Csound

-- Load instruments from .csd file (for MIDI-driven synthesis)
loki.alda.csound_load("instruments.csd")

-- Enable/disable Csound (for MIDI-driven synthesis)
loki.alda.set_csound(true)        -- Enable Csound (disables TSF)
loki.alda.set_csound(false)       -- Disable Csound

-- Unified backend selection
loki.alda.set_backend("csound")   -- Use Csound synthesis
loki.alda.set_backend("tsf")      -- Use TinySoundFont (SoundFont)
loki.alda.set_backend("midi")     -- Use external MIDI only

-- Standalone CSD playback (plays score section)
loki.alda.csound_play("song.csd") -- Play CSD file asynchronously
loki.alda.csound_playing()        -- Check if playback is active
loki.alda.csound_stop()           -- Stop playback
```

### Default Instruments

The included `.psnd/csound/default.csd` provides 16 instruments mapped to MIDI channels, including subtractive synth, FM piano, pad, pluck, organ, bass, strings, brass, and drums.

### Architecture Notes

Csound and TinySoundFont each have independent miniaudio audio devices. When you switch backends, the appropriate audio device is started/stopped. They do not share audio resources, allowing clean separation of concerns.

## Syntax Highlighting

psnd provides built-in syntax highlighting for music programming languages with language-aware features.

### Supported Languages

| Extension | Language | Features |
|-----------|----------|----------|
| `.alda` | Alda | Instruments, attributes, note names, octave markers, comments |
| `.joy` | Joy | Stack ops, combinators, music primitives, note names, comments |
| `.scm` `.ss` `.scheme` | TR7 Scheme | Keywords, special forms, music primitives, comments |
| `.bog` | Bog | Predicates, variables, operators, comments |
| `.hs` `.mhs` | MHS (Haskell) | Keywords, types, operators, strings, comments |
| `.csd` | Csound CSD | Section-aware (orchestra/score/options), opcodes, control flow |
| `.orc` | Csound Orchestra | Full orchestra syntax |
| `.sco` | Csound Score | Score statements, parameters |
| `.scl` | Scala Scale | Comments, numbers (for microtuning definitions) |

### Csound CSD Section Awareness

CSD files contain multiple sections with different syntax. psnd detects these sections and applies appropriate highlighting:

- **`<CsInstruments>`** - Full Csound orchestra highlighting:

  - Control flow (`if`, `then`, `else`, `endif`, `while`, `do`, `od`, `goto`)

  - Structure (`instr`, `endin`, `opcode`, `endop`)

  - Header variables (`sr`, `kr`, `ksmps`, `nchnls`, `0dbfs`, `A4`)

  - Common opcodes (`oscili`, `vco2`, `moogladder`, `pluck`, `reverb`, etc.)

  - Comments (`;` single-line, `/* */` block)

  - Strings and numbers

- **`<CsScore>`** - Score statement highlighting:

  - Statement letters (`i`, `f`, `e`, `s`, `t`, etc.)

  - Numeric parameters

  - Comments

- **`<CsOptions>`** - Command-line flag highlighting

Section tags themselves are highlighted as keywords, and section state is tracked across lines.

### Color Themes

psnd supports 17 color themes with true RGB colors. Use `:theme` to list available themes and `:theme NAME` to switch.

**Built-in themes**: ayu-dark, basic16, catppuccin, dracula, everforest, github-light, gruvbox-dark, kanagawa, monokai, nord, norse, one-dark, palenight, rose-pine, solarized-dark, solarized-light, tokyo-night

**Custom TOML themes** (recommended): Create `.psnd/themes/<name>.toml`:

```toml
# Example: .psnd/themes/my-theme.toml
[meta]
name = "My Theme"
description = "A custom color theme"

[colors]
# Base types
normal = [200, 200, 200]
comment = [128, 128, 128]
keyword1 = [220, 100, 220]
keyword2 = [100, 200, 200]
string = [150, 200, 100]
number = [200, 150, 100]

# Extended types (51 total for full tree-sitter support)
function = [100, 150, 220]
function_builtin = [100, 200, 200]
variable = [200, 200, 200]
variable_builtin = [220, 100, 100]
type = [200, 180, 100]
operator = [220, 100, 220]
# ... see existing themes for all 51 types
```

**Lua themes** (requires `editor.lua.enabled = true`): Create `.psnd/themes/<name>.lua` for dynamic themes with Lua logic.

The `:theme` command loads themes in order: TOML > Lua > C built-ins.

## Scala Scale Files (Microtuning)

psnd supports [Scala scale files](https://www.huygens-fokker.org/scala/scl_format.html) (`.scl`) for microtuning and alternative temperaments.

### Loading Scales

```lua
-- Load a scale file
loki.scala.load(".psnd/scales/just.scl")

-- Check if loaded
if loki.scala.loaded() then
    print(loki.scala.description())  -- "5-limit just intonation major"
    print(loki.scala.length())       -- 7
end
```

### Converting MIDI to Frequencies

```lua
-- Convert MIDI note to frequency using loaded scale
-- Arguments: midi_note, root_note (default 60), root_freq (default 261.63)
local freq = loki.scala.midi_to_freq(60)   -- C4 in the scale
local freq = loki.scala.midi_to_freq(67, 60, 261.63)  -- G4 with explicit root

-- Get ratio for a specific degree
local ratio = loki.scala.ratio(4)  -- 4th degree ratio (e.g., 3/2 for perfect fifth)
```

### Generating Csound Pitch Tables

```lua
-- Generate Csound f-table statement for use in .csd files
local ftable = loki.scala.csound_ftable(261.63, 1)
-- Returns: "f1 0 8 -2 261.630000 294.328125 327.031250 ..."
```

### Sample Scales

The `.psnd/scales/` directory includes example scales:

| File | Description |
|------|-------------|
| `12tet.scl` | 12-tone equal temperament (standard Western tuning) |
| `just.scl` | 5-limit just intonation major scale |
| `pythagorean.scl` | Pythagorean 12-tone chromatic scale |

### Lua API Reference

| Function | Description |
|----------|-------------|
| `loki.scala.load(path)` | Load scale file, returns true or nil+error |
| `loki.scala.load_string(content)` | Load from string |
| `loki.scala.unload()` | Unload current scale |
| `loki.scala.loaded()` | Check if scale is loaded |
| `loki.scala.description()` | Get scale description |
| `loki.scala.length()` | Number of degrees (excluding 1/1) |
| `loki.scala.ratio(degree)` | Get frequency ratio for degree |
| `loki.scala.frequency(degree, base)` | Get frequency in Hz |
| `loki.scala.midi_to_freq(note, [root], [freq])` | MIDI note to Hz |
| `loki.scala.degrees()` | Get all degrees as table |
| `loki.scala.csound_ftable([base], [fnum])` | Generate Csound f-table |
| `loki.scala.cents_to_ratio(cents)` | Convert cents to ratio |
| `loki.scala.ratio_to_cents(ratio)` | Convert ratio to cents |

## Roadmap

**Recent additions:**

- Windows/MSVC native support with full cross-platform compatibility

- Playback visualization with source line highlighting during playback

- Link transport sync mode for synchronized playback with Link peers

- Tempo tap and metronome commands for beat reference

- Web-based editor using xterm.js terminal emulation

- Mouse click-to-position support in web mode

- Language switching commands in web REPL

**Planned:**

- Multi-client support for web mode (currently single connection)

- Session persistence across server restarts

- Beat-aligned playback with Ableton Link

- Integrate additional MIDI DSLs from [midi-langs](https://github.com/shakfu/midi-langs)

Feedback and experiments are welcome - polyglot support will be guided by real-world usage.

## Project Architecture

![PSND Architecture](docs/arch-highlevel.svg)

## Project Structure

```text
source/
  core/
    main.c          # Entry point and CLI dispatch
    lang_dispatch.c # Language registry (commands, extensions, entry points)
    loki/           # Editor components (core, modal, syntax, lua, hosts)
      host.c           # Host abstraction + terminal host
      host_web.c       # Web server host (mongoose + xterm.js)
      host_webview.cpp # Native webview host (WebKit/WebKitGTK)
    tracker/        # MIDI tracker/step sequencer
      tracker_model.c    # Data structures (song, pattern, track, cell)
      tracker_plugin.c   # Plugin system for notation languages
      tracker_engine.c   # Playback engine with event scheduling
      tracker_audio.c    # Audio integration with SharedContext
      tracker_view.c     # View layer (theme, undo, clipboard)
      tracker_view_terminal.c  # Terminal UI with VT100 rendering
    shared/         # Language-agnostic backend (audio, MIDI, Link, OSC, params)
    compat/         # Platform shims (platform.h, thread.h)
    include/        # Public headers
    tests/          # Core tests (loki/, shared/, cli/, tracker/)
  langs/
    alda/           # Alda music language (parser, interpreter, backends)
    joy/            # Joy language runtime (parser, primitives, MIDI)
    tr7/            # TR7 Scheme (R7RS-small + music extensions)
    bog/            # Bog language (Prolog-based live coding)
    mhs/            # MHS (Micro Haskell with MIDI support)
    <name>/tests/   # Per-language tests, auto-discovered by CMake
  plugins/          # Optional plugins (sqlite FTS5 full-text search)
  testing/          # Test framework and leak checker
  thirdparty/       # External dependencies (lua, libremidi, TinySoundFont, mongoose, xterm.js)
  web/              # Browser UI assets
scripts/
  cmake/            # psnd_platform.cmake, psnd_languages.cmake
  new_lang.py       # Language scaffolding generator
```

Tests live beside the code they cover (`source/core/tests/` and `source/langs/<name>/tests/`) and are discovered automatically; run them with `ctest --test-dir build`.

## Documentation

See the `docs` folder for full technical documentation.

## Credits

- [Alda](https://alda.io) - music programming language by Dave Yarwood

- [Joy](https://github.com/shakfu/midi-langs/tree/main/projects/joy-midi) - concatenative music language from midi-langs

- [TR7](https://gitlab.com/jobol/tr7) - R7RS-small Scheme interpreter

- [dogalog](https://github.com/danja/dogalog) - Prolog-based live coding inspiration for Bog

- [MicroHs](https://github.com/augustss/MicroHs) - Small Haskell implementation by Lennart Augustsson

- [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo (antirez) - original editor

- [loki](https://github.com/shakfu/loki) - Lua-enhanced fork

- [Csound](https://csound.com/) - sound synthesis system (optional)

- [link](https://github.com/Ableton/link) - Ableton Link

- [midifile](https://github.com/craigsapp/midifile) - C++ library for reading/writing Standard MIDI Files

- [libremidi](https://github.com/celtera/libremidi) - Modern C++ MIDI 1 / MIDI 2 real-time & file I/O library

- [TinySoundFont](https://github.com/schellingb/TinySoundFont) - SoundFont2 synthesizer library in a single C/C++ file

- [miniaudio](https://github.com/mackron/miniaudio) - Audio playback and capture library written in C, in a single source file

- [mongoose](https://github.com/cesanta/mongoose) - Embedded web server/networking library (optional, for web mode)

- [xterm.js](https://xtermjs.org/) - Terminal emulator for the browser (optional, for web mode)

- [webview](https://github.com/webview/webview) - Cross-platform webview library (optional, for native webview mode)

- [liblo](http://liblo.sourceforge.net/) - Lightweight OSC implementation (optional, for OSC support)

## License

GPL-3

See `docs/licenses` for dependent licenses.
