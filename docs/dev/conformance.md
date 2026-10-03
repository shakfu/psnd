# Alda Conformance

psnd's Alda implementation is correct when it produces the MIDI that Alda produces. The oracle is `alda export` from Alda 2.4.7. aldakit (`~/projects/aldakit`) commits those exports in `tests/alda_reference/2.4.7/` and derives `.expected` files from them (`scripts/gen_shared_suite.py`). Alda source links are to the `release-2.4.7` tag.

## Status

All 60 scores match: the 40 in `source/langs/alda/examples/` and the 20 in `source/langs/alda/tests/shared_suite/`. Two CTest tests hold that:

| Test | Scores | Expected values |
|-|-|-|
| `alda_midi_test_suite` | `tests/shared_suite/*.alda` | `tests/shared_suite/*.expected` |
| `alda_conformance_examples` | `examples/*.alda` | `tests/alda_reference/*.expected` |

Both run `tests/test_shared_suite.c`, which compares every note on pitch, start, duration and velocity, and on the program and controllers 10 and 11 in effect on its channel when it starts. Timing tolerance is 10ms.

The same 60 scores were also exported through `loki_midi_export_shared()` and compared file against file with aldakit's `scripts/alda_diff.py`; all match.

## Method

The first measurement (2026-10-03, psnd `43a71f6`) found 0 of 60 matching; 38 differed only by P10. Each cause below was isolated with a minimal score run through both implementations.

## Deliberate deviations

- **Channel numbers.** Alda picks a channel per note and moves a part to a new channel after each voice group. psnd keeps one channel per part while parts fit. A synth hears a channel's program and controllers, not its number, and the comparison checks those for every note. The move is audible in one case: a note from inside a voice group still sounding when the part plays the same pitch after the group (`quant` above 100). On one channel the new note-on would cut the old note off, so psnd moves the part to a new channel in that case only (`separate_voice_group_overlaps()` in `impl/finalize.c`). aldakit `docs/dev/alda-deviations.md` (D1) gives the full rationale.

- **Errors.** Alda refuses a score with, for example, `(vol 120)` or an unknown attribute. psnd clamps or ignores such values and continues, as before this work. Not measured beyond that.

## Causes

`file:line` paths are under `source/langs/alda/` at `43a71f6`.

| # | Difference from Alda | Was | Fix |
|-|-|-|-|
| P1 | A local `(tempo N)` changed the tempo of every part: all parts shared one tick timeline and tempo map. | `impl/attributes.c:407-423` | Parts are placed in seconds; `impl/finalize.c` builds the tempo map from the first part's changes and `tempo!` ([`score.go:222-261`](https://github.com/alda-lang/alda/blob/release-2.4.7/client/model/score.go#L222-L261)) and converts to ticks once |
| P2 | Chord notes all got the longest duration; the chord advanced by the longest; only the first note set the default. | `impl/interpreter.c:390-468` | Per-note durations, advance by the shortest note or rest ([`chord.go:41-82`](https://github.com/alda-lang/alda/blob/release-2.4.7/client/model/chord.go#L41-L82)) |
| P3 | Voices shared the part's state, and after the group the part kept the last voice written. | `impl/interpreter.c:571-615` | Each voice is a copy of the part; the last to finish becomes the part ([`voice.go`](https://github.com/alda-lang/alda/blob/release-2.4.7/client/model/voice.go)) |
| P4 | A rest did not set the default duration. | `impl/interpreter.c:365-388` | Rests set it, the whole tied length included |
| P5 | Brackets, repeats and variable references inside a cram weighed nothing. | `impl/interpreter.c:948-972` | Inner duration summed recursively, as Alda's `DurationMs` |
| P6 | Each scaled cram note was rounded to ticks, so positions drifted. | `impl/interpreter.c:884-893` | Crams scale lengths in seconds; ticks are computed once |
| P7 | A cram's explicit duration did not become the default. | cram code | It does ([`cram.go`](https://github.com/alda-lang/alda/blob/release-2.4.7/client/model/cram.go)) |
| P8 | Slurred notes inside a cram were quantized. | cram code | Cram notes go through the normal note path |
| P9 | `(quant N)` above 100 was ignored. | `impl/attributes.c:449-462` | Any non-negative value; 0 is no longer "unset" |
| P10 | No pan (CC 10 = 64) or track volume (CC 11 = 100) when a channel was first used; `track-volume` not implemented. | `impl/attributes.c` | `impl/finalize.c` sends program, pan and track volume with each note whose channel holds a different value; `track-volume` is CC 11 |
| P11 | 36 of 127 canonical instrument names and 76 of 149 aliases fell back to piano. | `impl/instruments.c` | `scripts/gen_alda_instruments.py` generates `impl/instruments_alda.inc` from Alda's list |
| P12 | Volume, pan and dynamics were truncated, and dynamics used the docs' rounded table. | `impl/attributes.c:349-351, 475`; `impl/context.c:465` | `alda_percent_to_midi()` rounds half away from zero; Alda's dynamics values |
| P13 | Beyond 15 parts channels wrapped and every program change was sent at tick 0. | `impl/context.c:237-255` | Channels pass between parts over time, and programs go with notes |
| P14 | `(midi-channel N)` was not implemented. | - | Pins the part; parts sharing a channel each get their program |
| P15 | A program change was sent on the drum channel. | | None sent |

Found while verifying the fixes:

| # | Difference | Fix |
|-|-|-|
| P16 | `midi-bass+lead:` did not parse; Alda allows `+` in names ([`scanner.go:546-557`](https://github.com/alda-lang/alda/blob/release-2.4.7/client/parser/scanner.go#L546-L557)). | `impl/scanner.c` accepts `+` |
| P17 | Exported files carried two or three tempo events at tick 0, so the file's tempo depended on sort order. | `register.c` and `core/loki/midi_export.cpp` add a starting tempo only when the score has none at tick 0 |
| P18 | In exported files a note ending where the same pitch started again ended the new note. `shared_midi_events_sort()` sorted by tick alone with an unstable qsort, and midifile's sort puts note-ons first. Vendored midifile's `sortNoteOffsBeforeOns()` uses the note-ons-first comparator too (`thirdparty/midifile/src/MidiEventList.cpp:650`). | `core/shared/midi/events.c` orders each tick: note-offs, tempo, program, controllers, note-ons; the export keeps that order with `markSequence()` |
| P19 | In async playback a note-on could precede its own tick's program or controller change. | `core/shared/async/shared_async.c`: note-offs, then settings, then note-ons (`shared_async_sort_events()`) |

## Suite runner

`tests/test_shared_suite.c` was rewritten. It had these defects:

- R1. It converted ticks to seconds itself, with each duration at the tempo at its start; `gau.alda` and `bach-prelude.alda` failed there although psnd's output matched.

- R2. It read a pan event's value from `data2`; pan stores it in `data1`.

- R3. It reported only the first mismatch per file.

- R4. Fixed limits: 1024 notes, 64 program changes, 256 controller changes.

- R5. Program and controller expectations were parsed but never compared.

- R6. Notes at velocity 0 were compared, but MIDI cannot carry them, so a file derived from MIDI does not contain them.

## Keeping it current

- A new example needs Alda's export: copy the score into aldakit's `examples/`, run `make alda-diff` there (needs Alda), then `python scripts/gen_shared_suite.py --examples <psnd>/source/langs/alda/tests/alda_reference`.

- Alda's instrument list changes: update `source/langs/alda/docs/alda-language/list-of-instruments.md` and run `python3 scripts/gen_alda_instruments.py`.
