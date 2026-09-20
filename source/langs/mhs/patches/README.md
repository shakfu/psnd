# Local patches to vendored MicroHs

Every `.diff` here is applied at build time to a copy of
`source/thirdparty/MicroHs`, and the compiler is rebuilt from the result. The
vendored tree itself is never modified.

This costs one self-compile of the compiler, measured at 39.7s plus the `cc` of
the generated C. It is cached: the step reruns only when a patch, `bin/mhs`, or
`mhs-patch-xffi.py` changes. `-DMHS_PATCH_MHS=OFF` skips it and links upstream's
committed `generated/mhs.c` instead.

Patches are applied with `patch -p1 -s -N -t -F0`. No fuzz, so a patch whose
context has drifted fails the build rather than landing in the wrong place; no
reversal, so an already-applied patch is refused rather than undone.

## Applied

### `0001-interactive-run-and-print.diff`

At the prompt, an expression of type `IO Bool` or `IO Int` fails instead of
running:

```
mhs> midiInit
*** Exception: error: no location: Cannot satisfy constraint: Show (IO Bool)
```

`PrintOrRun` has an instance for `IO ()` and one for `Show a`, and `IO Bool`
matches neither. The patch compiles such a line a second time as
`(line) >>= print`, and reports the original error if that fails too. Genuine
type errors are unaffected.

This matters here because most of psnd's MIDI API returns a value: `midiInit`,
`midiListPorts`, `midiIsOpen`, `midiRandom`, `midiRecordStop`,
`midiRecordCount`, `midiRecordActive`. Without it, none of them can be typed
bare at the prompt.

The instance that would fix this properly is commented out in
`lib/System/IO/PrintOrRun.hs` upstream. Enabling it against 0.16.5.0 builds,
but the prompt then reports `Multiple constraint solutions for: PrintOrRun (IO
Bool)`, so that route is closed until instance resolution improves.

Sent upstream; drop this file when it lands.

## Not applied

Three other patches were written against 0.16.5.0 and are in the repository
root, to be sent upstream. psnd already avoids what they fix, from its own
side, so carrying them here would only add a second mechanism:

| patch | what it fixes | why psnd does not need it |
|-|-|-|
| `patch-mhs-skip-unchanged-cache.diff` | `-C` re-compresses an unchanged `.mhscache` with LZMA, 1.88s per run | `repl.c` passes `-CR` on a warm cache |
| `patch-mhs-skip-loaded-packages.diff` | `-p` appends another copy of each package per run | `repl.c` passes `-p` only when the cache is cold |
| `patch-mhs-validate-file-arg.diff` | a module named by path is never invalidated, so edits are ignored | `repl.c` passes the module name and `-i`, which validates correctly |

The last one would also make a file-argument module recompile on every run,
which is worse than what psnd does now.

## On a version bump

Re-check each applied patch against the new tree before bumping:

```sh
cd source/thirdparty/MicroHs
patch -p1 --dry-run < ../../langs/mhs/patches/0001-interactive-run-and-print.diff
```

A patch that no longer applies has either been taken upstream, in which case
delete it, or needs rebasing against the new source.
