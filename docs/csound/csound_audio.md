# Csound Audio Integration - Debug Notes

**Status**: FIXED - Audio working **Date**: 2026-01-14

## Summary

Csound synthesis backend is now fully working. The issue was that `async.c` (the playback event dispatcher) was routing MIDI events only to TSF, completely ignoring the Csound backend.

### Root Cause

In `thirdparty/alda-midi/lib/src/async.c`, the `send_event()` function only checked for TSF:

```c
if (async_sys.ctx && async_sys.ctx->tsf_enabled && alda_tsf_is_enabled()) {
    // routes to TSF...
}
```

It never checked `csound_enabled` or routed to `alda_csound_send_note_on()`.

### Fix

Added Csound routing to `async.c:send_event()` with highest priority (before TSF check), mirroring the pattern in `midi_backend.c`.

## What Works

- Csound 6.18.1 compiles and links correctly

- CSD file loads without errors

- Csound initializes and starts properly

- Message suppression via `csoundCreateMessageBuffer()` works

- Editor opens and shows "ALDA CSD" status

- **Audio output works** when playing Alda code with Ctrl-P

- MIDI events are correctly routed to Csound instruments

## Architecture

### Current Design

Each backend has its own independent miniaudio device:

1. `tsf_backend.c` - Contains `MINIAUDIO_IMPLEMENTATION`, owns its own `ma_device` for TSF synthesis

2. `csound_backend.c` - Has its own `ma_device` for Csound synthesis (includes `miniaudio.h` without implementation)

The backends are completely independent - no delegation or shared audio devices. The `async.c` event dispatcher routes MIDI events to the appropriate backend based on which is enabled (Csound takes priority).

### Key Files

- `thirdparty/alda-midi/lib/src/csound_backend.c` - Csound synthesis backend

- `thirdparty/alda-midi/lib/src/tsf_backend.c` - TSF synthesis backend

- `thirdparty/alda-midi/lib/include/alda/csound_backend.h` - Csound API

- `.psnd/csound/default.csd` - Default Csound instruments

## Historical Notes

### Failed Theory: Multiple miniaudio instances

Early debugging suggested macOS CoreAudio couldn't support multiple miniaudio devices. This was **incorrect** - the actual issue was MIDI event routing.

### What Actually Fixed It

The `async.c` event dispatcher was only routing to TSF, ignoring Csound entirely. Adding Csound routing to `send_event()` fixed the issue. Separate miniaudio instances work fine.

## CSD File Notes

The default CSD file (`.psnd/csound/default.csd`) was fixed for:

- `endop` vs `endin` (opcodes use `endop`, instruments use `endin`)

- FM synthesis instrument 2 (replaced `fmb3` with manual FM)

- Added `--daemon` flag to CsOptions

## Build Command

```bash
make csound  # or: cmake --build build --target alda_bin
```

## Test Command

```bash
./build/psnd -cs .psnd/csound/default.csd docs/examples/simple.alda
# Then press Ctrl-P to play
```

## Plugin Warning

The warning about missing plugin directory is harmless:

```text
WARNING: Error opening plugin directory '/Users/sa/Library/Frameworks/CsoundLib64.framework/Versions/6.0/Resources/Opcodes64': No such file or directory
```

This is because we're using a statically linked Csound, not the framework version.
