# Csound Integration Plan

## Overview

Integrate Csound 6.18.1 as an optional synthesis backend for Psnd, driven by MIDI events from the Alda scheduler. This provides full synthesis capabilities beyond TinySoundFont's sample playback.

## Architecture

```text
[ Alda Parser ] --> [ MIDI Events ] --> [ Scheduler (libuv) ]
                                              |
                    +-------------------------+-------------------------+
                    |                         |                         |
              [ tsf_backend ]          [ csound_backend ]       [ midi_backend ]
              (TinySoundFont)          (Csound synthesis)       (External MIDI)
                    |                         |
                    +------------+------------+
                                 |
                            [ miniaudio ]
                            (Audio Output)
```

### Key Design Decisions

1. **Host-implemented audio I/O**: Csound renders to buffers; miniaudio handles output

2. **MIDI via score events**: Simple `csoundInputMessage()` for note on/off

3. **Modular build**: Csound is optional, controlled by CMake flag

4. **Pre-defined instruments**: Ship default `.csd`, future: edit in editor

## Build Configuration

### Minimal Build (Custom.cmake)

```cmake
set(BUILD_UTILITIES OFF)
set(BUILD_TESTS OFF)
set(BUILD_STATIC_LIBRARY ON)
set(INIT_STATIC_MODULES ON)
set(BUILD_DSSI_OPCODES OFF)
set(BUILD_OSC_OPCODES OFF)
set(BUILD_PADSYNTH_OPCODES OFF)
set(BUILD_SCANSYN_OPCODES OFF)
set(BUILD_DEPRECATED_OPCODES OFF)
set(USE_CURL OFF)
set(USE_GETTEXT OFF)
```

### Dependencies

- **libsndfile**: Built from source in `thirdparty/libsndfile` (minimal config, no external codecs)

- **All other Csound deps**: Disabled for minimal build

See `docs/csound-deps.md` for manual build instructions.

### Build Sizes

| Component | Size |
|-----------|------|
| libsndfile.a (minimal) | ~2.5 MB |
| libCsoundLib64.a | ~19 MB |
| psnd binary | ~1.6 MB |

## API Design

### csound_backend.h

```c
// Initialization
int alda_csound_init(void);
void alda_csound_cleanup(void);

// Instrument loading
int alda_csound_load_csd(const char* path);
int alda_csound_compile_orc(const char* orc);  // Future

// Enable/disable
int alda_csound_enable(void);
void alda_csound_disable(void);
int alda_csound_is_enabled(void);

// MIDI messages (matches tsf_backend pattern)
void alda_csound_send_note_on(int channel, int pitch, int velocity);
void alda_csound_send_note_off(int channel, int pitch);
void alda_csound_send_program(int channel, int program);
void alda_csound_send_cc(int channel, int cc, int value);
void alda_csound_all_notes_off(void);

// Audio rendering (called by miniaudio callback)
void alda_csound_render(float* output, int frames);
```

## MIDI-to-Csound Mapping

### Note Events

```c
// Note on: trigger instrument with pitch/velocity
// i <instr> <start> <dur> <pitch> <velocity>
// dur=-1 means held until note off
snprintf(msg, sizeof(msg), "i %d 0 -1 %d %d", channel+1, pitch, velocity);
csoundInputMessage(csound, msg);

// Note off: negative instrument number turns off
snprintf(msg, sizeof(msg), "i -%d 0 0 %d", channel+1, pitch);
csoundInputMessage(csound, msg);
```

### Program Change

Map to Csound instruments or use massign in orchestra.

### Control Change

Use Csound's chnset for real-time control:

```c
csoundSetControlChannel(csound, "cc1_ch1", value / 127.0);
```

## Default Instruments

Ship `.psnd/csound/default.csd` with GM-compatible instruments:

```csound
<CsoundSynthesizer>
<CsOptions>
-d -n  ; No audio output (host-implemented)
</CsOptions>
<CsInstruments>
sr = 44100
ksmps = 64
nchnls = 2
0dbfs = 1

; Instrument 1: Simple subtractive synth
instr 1
  ipch = cpsmidinn(p4)
  iamp = p5/127 * 0.5

  ; Two detuned oscillators
  a1 = vco2(iamp, ipch)
  a2 = vco2(iamp * 0.5, ipch * 1.003)

  ; Low-pass filter with envelope
  kenv = madsr(0.01, 0.2, 0.6, 0.3)
  afilt = moogladder(a1 + a2, 2000 * kenv + 200, 0.3)

  outs afilt * kenv, afilt * kenv
endin

; Instrument 2: FM piano-like
instr 2
  ipch = cpsmidinn(p4)
  iamp = p5/127 * 0.4
  kenv = madsr(0.001, 0.5, 0.3, 0.5)
  asig = fmb3(iamp, ipch, 2, 2.01, 5)
  outs asig * kenv, asig * kenv
endin

; Add more instruments as needed...

</CsInstruments>
<CsScore>
f0 86400  ; Run for 24 hours
</CsScore>
</CsoundSynthesizer>
```

## Implementation Phases

### Phase 1: Minimal Integration (COMPLETE)

1. Add Csound as optional CMake subdirectory

2. Create csound_backend.h/c with core API

3. Integrate audio rendering with miniaudio

4. Ship default instruments

5. Basic note on/off functionality

6. Lua API for loading CSD and enabling Csound

### Phase 2: Full MIDI Support (COMPLETE)

1. Program change -> instrument selection (tracked per channel)

2. Control change -> real-time parameters (via Csound control channels)

3. Pitch bend support (via control channels)

4. Per-channel state tracking (fractional instrument IDs)

### Phase 3: Live Coding (Future)

1. Lua API: `csound.compile(orc_string)`

2. Editor syntax highlighting for .csd files

3. Hot-reload instruments without stopping playback

4. Error reporting to status bar

## Lua API

The Csound backend is controlled via the `loki.alda` Lua module:

```lua
-- Check if Csound backend is available (compiled in)
loki.alda.csound_available()  --> true/false

-- Load a Csound .csd file
loki.alda.csound_load("path/to/instruments.csd")

-- Enable Csound backend (disables TSF automatically)
loki.alda.set_csound(true)

-- Unified backend selection
loki.alda.set_backend("csound")  -- Use Csound synthesis
loki.alda.set_backend("tsf")     -- Use TinySoundFont (sample playback)
loki.alda.set_backend("midi")    -- Use external MIDI only
```

Example usage in `.aldev/init.lua`:

```lua
if loki.alda.csound_available() then
    loki.alda.csound_load(".aldev/csound/default.csd")
    loki.alda.set_backend("csound")
    loki.status("Csound backend enabled")
end
```

## File Structure

```text
thirdparty/
  csound-6.18.1/
    Custom.cmake              # Minimal build configuration
    ...
  CMakeLists.txt              # Add csound subdirectory

src/
  csound_backend.h            # Public API
  csound_backend.c            # Implementation

.psnd/
  csound/
    default.csd               # Default instruments
    README.md                 # Instrument documentation
```

## Trade-offs vs TinySoundFont

| Aspect | TSF | Csound |
|--------|-----|--------|
| Binary size | ~50KB | ~2-4MB minimal |
| Synthesis | Sample playback | Full synthesis |
| Latency | Sample-accurate | ksmps buffer delay |
| Dependencies | None | libsndfile |
| CPU usage | Low | Higher |
| Live coding | No | Yes (future) |

## Risks and Mitigations

1. **Build complexity**: Use Custom.cmake to minimize; provide CI scripts

2. **Binary size**: Minimal build strips ~80% of opcodes

3. **Latency**: Keep ksmps low (32-64); acceptable for most use

4. **libsndfile dependency**: Common library, available everywhere

## Future Considerations

- Csound 7 migration (API changes)

- WebAssembly build using existing wasm/ infrastructure

- Plugin architecture for user opcodes

- Integration with Ableton Link tempo
