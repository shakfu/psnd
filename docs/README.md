# psnd Documentation

This directory contains documentation for psnd, a polyglot editor and REPL for music programming languages.

## Language Guides

psnd supports five music programming languages, each with its own paradigm. Each language's guide lives beside its source:

| Language | Paradigm | Guide |
|----------|----------|-------|
| Alda | Declarative music notation | [source/langs/alda/README.md](../source/langs/alda/README.md) |
| Joy | Concatenative (stack-based) | [source/langs/joy/README.md](../source/langs/joy/README.md) |
| TR7 Scheme | Functional (Lisp/Scheme) | [source/langs/tr7/README.md](../source/langs/tr7/README.md) |
| Bog | Logic programming (Prolog-based) | [source/langs/bog/README.md](../source/langs/bog/README.md) |
| MHS | Functional (Micro Haskell) | [source/langs/mhs/README.md](../source/langs/mhs/README.md) |

## Quick Comparison

### Alda - Declarative Notation

Write music as you would on sheet music:

```alda
piano:
  (tempo 120)
  c4 d e f | g a b > c
```

Best for: Traditional composition, readable scores, multi-instrument arrangements.

### Joy - Stack-Based

Compose using postfix operations on a stack:

```joy
120 tempo
[c d e f g] play
c major chord
```

Best for: Algorithmic composition, pattern manipulation, functional transformations.

### TR7 Scheme - Functional

Use Scheme's expressiveness for music:

```scheme
(set-tempo 120)
(play-seq '(60 62 64 65 67) 80 250)
(play-chord '(60 64 67) 80 500)
```

Best for: Complex algorithms, recursive structures, Lisp enthusiasts.

### Bog - Logic Programming

Define musical events through declarative rules:

```prolog
event(kick, 36, 0.9, T) :- every(T, 1.0).
event(hat, 42, 0.5, T) :- every(T, 0.25).
```

Best for: Generative music, pattern-based rhythms, live coding with named slots.

## Common Features

All languages share:

- **Non-blocking async playback** - REPLs remain responsive during playback

- **Virtual MIDI ports** - Create named ports for routing to DAWs

- **Built-in synthesis** - TinySoundFont for standalone playback

- **Csound backend** - Advanced synthesis (optional build)

- **Ableton Link** - Tempo sync with other applications

- **Editor integration** - Syntax highlighting and live evaluation

- **Lua scripting** - Automation and customization

## Directory Structure

```text
docs/
  README.md                  # This file
  cheatsheet.md              # Editor key bindings
  cheatsheet-tracker.md      # Tracker key bindings
  new_lang.md                # How to add a language
  language-extension-api.md  # LokiLangOps / editor integration API
  LANG_IMPL_COMPARISON.md    # How the languages are implemented
  psnd_osc.md                # OSC control surface
  scsynth.md                 # SuperCollider notes
  refactor.md                # Refactoring notes
  design_review.md           # Historical architecture review
  unused.md                  # Dead code notes
  csound/                    # Csound backend documentation
  licenses/                  # Third-party license texts

source/langs/<name>/README.md  # Per-language guides
```

## Getting Started

1. **Install**: Build psnd with `make` (or `make csound` for Csound support)

2. **Choose a language**: Pick based on your preferred paradigm

   - Notation-based? Try **Alda**

   - Stack-based/functional? Try **Joy**

   - Lisp fan? Try **TR7 Scheme**

   - Logic programming/live coding? Try **Bog**

3. **Start the REPL**:

   ```bash
   psnd alda    # Alda REPL
   psnd joy     # Joy REPL
   psnd tr7     # TR7 Scheme REPL
   psnd bog     # Bog REPL
   ```

4. **Edit files**:

   ```bash
   psnd song.alda   # Open in editor
   psnd song.joy
   psnd song.scm
   psnd song.bog
   ```

5. **Play files**:

   ```bash
   psnd play song.alda   # Headless playback
   ```

## REPL Commands

Common commands work across all REPLs:

| Command | Action |
|---------|--------|
| `:h` | Show help |
| `:q` | Quit |
| `:s` | Stop playback |
| `:l` | List MIDI ports |
| `:sf PATH` | Load soundfont |
| `:link on` | Enable Ableton Link |

See individual language guides for language-specific commands.

## Editor Keybindings

| Key | Action |
|-----|--------|
| `Ctrl-E` | Evaluate current part/line |
| `Ctrl-P` | Play entire file |
| `Ctrl-G` | Stop playback |
| `Ctrl-S` | Save |
| `Ctrl-L` | Lua console |

## Further Reading

- [Project README](../README.md) - Full project overview

- [CHANGELOG](../CHANGELOG.md) - Version history

- [CLAUDE.md](../CLAUDE.md) - Development guidelines
