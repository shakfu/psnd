# Updating Vendored MicroHs

Status: **investigation**. Written 2026-09-01 against psnd `30a3a47`.
The preparatory work in the last section is done; the version bump is not.

## Recommendation

Defer the version bump. Do the decoupling work in [Preparatory work](#preparatory-work) first.

The mechanical part of the bump is cheap and low-risk. The blocking part is a semantic
change to `MHSDIR` that breaks psnd's default package-embedding mode and requires
redesigning how the embedded interpreter finds `base.pkg`. Upstream added a native
facility for exactly that job (`--embed-packages`), which is the right destination, but
adopting it is a refactor of ~2500 lines of psnd-specific code, not a version bump.

## Version facts

There is no MicroHs 0.17.0.

| | Version | Date |
|-|-|-|
| Vendored in psnd | `0.15.0.0` | 2025-12-17 |
| Latest release tag | `v0.16.5.0` | 2026-06-20 |
| `master` | `0.16.6.0` | 2026-08-29 |

`RELEASE-v0.17.0.0.md` exists in the repository. It is the notes file for the *next*
release, listing qualified strings, string interpolation, and imath-by-default bignums.
The gap to close is 0.15.0.0 -> 0.16.5.0: 553 commits over six months.

## How psnd vendors MicroHs today

`source/thirdparty/MicroHs/` is the upstream tree, 412 tracked files, 5.3MB. `bin/`
is gitignored; CMake runs upstream `make` to build `bin/mhs` from the committed
`generated/mhs.c` bootstrap. No GHC needed.

Until the prep work below, `source/langs/mhs/impl/` held a second copy: a checked-in
fork of `src/runtime/`, 13 files. It is gone. The runtime now compiles straight from
the vendored tree, with `mhs-patch-eval.py` applying the psnd-specific edits at build
time.

The build pipeline (`source/langs/mhs/CMakeLists.txt`):

1. Build `bin/mhs`, `bin/cpphs`, `bin/mcabal` via upstream `make`.
2. Copy `lib/` to `build/mhs-base-src`, run `mcabal build` to produce `base-0.15.0.0.pkg`.
3. `mhs -Q` installs that pkg into `build/mcabal/mhs-0.15.0.0/packages/`, and the
   runtime sources plus `targets.conf` are copied alongside it.
4. Build `music-0.1.0.pkg` from `source/langs/mhs/lib/*.hs`.
5. `mhs-embed` packs the pkgs, the `.txt` module index, `src/runtime/`, three static
   libraries, and `midi_ffi.h` into a C header.
6. `vfs.c` serves that header by intercepting `mhs_fopen`. At runtime psnd sets
   `MHSDIR=/mhs-embedded` (`vfs.h:11`) so every compiler file lookup lands in the VFS.

### Measured local delta over upstream 0.15.0.0

The fork was measured before deleting it:

| File | Delta |
|-|-|
| `impl/eval.c` | 154 diff lines, **all** `mmalloc`/`mrealloc`/`mcalloc` -> `mhs_*`. Zero other changed lines. |
| `impl/bfile.c`, `lz77.c`, `md5.c`, `md5.h`, `mhseval.c`, `mhseval.h`, `mhsffi.h`, `mhs.c`, `unix/*`, `windows/*` | byte-identical to upstream |

Only `eval.c` references those three allocators; `bfile.c`, `md5.c`, `lz77.c`, and
`extra.c` have zero occurrences, which is why renaming one file was sufficient. The
rename avoids a collision with Csound, which exports the same names. It is now applied
by `mhs-patch-eval.py --rename-malloc`, and the generated `eval_psnd.c` is byte-identical
to what the fork produced.

Everything else is applied at build time by two Python scripts. All three of their text
anchors survive verbatim at `v0.16.5.0`:

| Anchor | Location at 0.16.5.0 |
|-|-|
| `const struct ffi_entry *xffi_table = imp_table;` | `generated/mhs.c` (1 occurrence at both tags) |
| `from_t mhs_fopen(int s)` | `src/runtime/eval.c:7489` |
| `from_t mhs_opendir/mhs_readdir/mhs_closedir(int s)` | `src/runtime/eval.c:7627-7629` |
| `const struct ffi_entry ffi_table[] = {` | `src/runtime/eval.c:7809` |

`mmalloc` still exists at 0.16.5.0 (20 occurrences in `eval.c`). Both patch scripts and
the sed-equivalent rename should apply unchanged.

## What breaks at 0.16.5.0

### 1. `MHSDIR` disables package lookup

This is the blocker. `src/MicroHs/Compile.hs:599` at 0.16.5.0:

```haskell
getPaths = do
  mdir <- lookupEnv "MHSDIR"
  let srcs = ["."]
  case mdir of
    -- If MHSDIR is set, use that and no package directories
    Just dir -> return (dir, srcs ++ [dir </> "lib"], Nothing)
    Nothing -> do
      ...
```

At 0.15.0.0 the same function was one line and preserved package search:

```haskell
getMhsDir = maybe getDataDir return =<< lookupEnv "MHSDIR"
```

psnd sets `MHSDIR=VFS_VIRTUAL_ROOT` in `repl.c:967`, `repl.c:1015`, `repl.c:1199`, and
`mhs_midi_standalone_main.c:96`, and relies on package-directory lookup to find the
embedded `packages/base-0.15.0.0.pkg`. Under 0.16.5.0 the `Nothing` in the third
tuple slot means no package path at all. The default `PKG_ZSTD` mode stops working.

Two fixes exist. Either make the VFS answer the "installed" layout that the `Nothing`
branch probes (`<bin>/../mhs-VER/packages/...`, resolved from `getExecutablePath`, which
inside psnd is the psnd binary), or move to `--embed-packages`. The second is better and
is analysed below.

### 2. `targets.conf` renamed to `mhs.conf`

`targets.conf.in` was deleted. `mhs.conf` is generated from `mhs.conf.in` by a `sed` rule
in the upstream Makefile, and `Main.hs:205` reads `mhsdir </> "mhs.conf"`.
`CMakeLists.txt:826` copies `targets.conf` and is the only reference in psnd.

`readConfig` degrades gracefully: a missing file is a warning (suppressed under `-q`) and
an empty config. `findSection` only errors when a target is actually needed, which is the
C-generation path. Pure eval and REPL work without the file.

### 3. Six new runtime files

The runtime is still a single translation unit: `eval.c` includes `extra.c`,
`ffi_errno.c`, `bfile.c`, `md5.c`, `lz77.c`, and `imgmp.h`; `bfile.c` includes `lzma.h`;
`imgmp.h` includes `imath.c`; `imath.c` includes `macros.c`. So no CMake source-list
restructuring, but `impl/` must gain `ffi_errno.c`, `imath.c`, `imath.h`, `imgmp.h`,
`macros.c`, and `lzma.h` - about 16,800 new lines.

No symbol collisions: `mp_int_`, `LzmaDec`, and `ELzma` appear nowhere in `source/`.

### 4. Configuration and arithmetic semantics

`src/runtime/unix/config.h` gains `WANT_GMP 0`, `WANT_IMATH 1`, `WANT_SOCKET 1`,
`WANT_IO_POLL 1`, `WANT_OVERFLOW 1`. The last is a behaviour change: Int arithmetic
raises on overflow instead of wrapping. `source/langs/mhs/lib/*.hs` imports only
`Control.Concurrent`, `Control.Exception`, `Data.IORef`, `Foreign.C.String`,
`Foreign.C.Types`, `Foreign.Ptr`, and `System.IO.Unsafe`, all still in `base.cabal`, but
the overflow change is not visible to a grep.

`WANT_IMATH` also means bignums no longer need an external GMP.

### 5. Cheap items

- Two version pins: `CMakeLists.txt:124` and `CMakeLists.txt:645`.
- `lib/gmp` moved to `lib/no-gmp`. Internal to upstream; `getPaths` only adds `no-gmp`
  when neither GMP nor imath is wanted, which is not psnd's configuration.
- CLI flags psnd uses (`-Q`, `-P`, `-i`, `-o`) all still exist.
- `base.cabal` changes are additive: `Data.Any`, `Data.STRef.Lazy/Strict`,
  `Control.Monad.ST.Lazy/Strict`, `System.IO.FD`, `Data.String.Interpolate`, others.
- Windows is unaffected. MHS is already disabled there (`CMakeLists.txt:14`, no
  `fmemopen`).

## Upstream package embedding

Added 2026-04-29 through 2026-05-05 on an `embed` branch (`3017ce71` "First stab at
embedding packages", `0bb8883a`, `0fc2fd1d`, `974decb2`, merged in `8337211b`).
Released in 0.16.0.0 as "Enable embedding packages in the mhs binary". Unchanged on
`master` since the merge.

### Mechanism

`src/MicroHs/Embed.hs` is eight lines:

```haskell
module MicroHs.Embed where
import Data.ByteString(ByteString)

-- This list is filled by the compiler when compiled with
-- the --embed-packages PKG:PKG:... flag.
-- It is simply the contents of the corresponding .pkg file.
packages :: [ByteString]
packages = []
```

`--embed-packages` acts when compiling the compiler itself. `Main.hs:528` rewrites the
linked combinator graph, replacing the definition of `MicroHs.Embed.packages` with a
literal list of raw `.pkg` bytes:

```haskell
addEmbedPkgs flags ds = do
  bss <- mapM get (embedPkgs flags)
  let ps = encList $ map (Lit . LBStr) bss
      rep ie@(i, _) | i == mkIdent "MicroHs.Embed.packages" = (i, ps)
                    | otherwise = ie
  return $ map rep ds
```

The resulting `mhs.c` carries the package bytes as combinator string literals. At
runtime `loadEmbedPkg` deserializes each one into the compile cache. It runs
unconditionally at the top of `compile` (`Compile.hs:124`), `compileMany`
(`Compile.hs:80`), and the interactive entry point (`Interactive.hs:82`), before any
module resolution. `CompileCache.getEmbedPkgs` distinguishes them by a `Nothing` file path.

`--embed-ffis` is the output-side half, split out in `0fc2fd1d`. When generating C for a
program, `makeFFI` needs the FFI declarations of packages already embedded in the running
binary so their `foreign import` stubs are re-emitted. `--embed-packages` implies
`--embed-ffis` (`Main.hs:177`).

### What it would replace

Module imports resolve from the in-memory cache with no filesystem access. That removes
psnd's need to serve, through the VFS:

- `packages/base-VER.pkg` and `packages/music-0.1.0.pkg`
- the `.txt` module-to-package index (`Package.hs:25`, collected by `mhs-embed --txt-dir`)
- the package-directory probing that item 1 above breaks

`loadDependencies` only touches disk for dependencies not already loaded. With `base`
embedded and `music` depending on it, nothing is left to find.

### What it would not replace

`--embed-packages` covers packages only. Compile-to-executable (`MHS_ENABLE_COMPILATION=ON`,
the `-o` path) still needs a filesystem for:

- `src/runtime/*.c` and `config.h`, passed to `cc` as include paths
- `mhs.conf`, for the target's compiler and linker flags
- `liblibremidi.a`, `libmidi_ffi.a`, `libmusic_theory.a`, and `midi_ffi.h`

So `vfs.c` survives in reduced form, or `MHS_ENABLE_COMPILATION=OFF` becomes the default
and psnd drops the `-o` feature.

### Caveats

- **Undocumented.** No README coverage. Not used by upstream's own Makefile or
  `Makefile.packages`. It is a facility, not a structural part of upstream's build,
  so it carries less real-world testing than the rest of the compiler.
- **Version lockstep.** `loadPkg'` errors on `pkgCompiler pkg /= mhsVersion`
  (`Compile.hs:540`). psnd builds both from one tree, so this is automatic, but it rules
  out mixing a prebuilt pkg with a different compiler.
- **Requires regenerating `mhs.c`.** psnd currently ships upstream's committed
  `generated/mhs.c`. Embedding means self-compiling the compiler at psnd build time.

### Cost of the self-compile

Measured on this machine, with the vendored 0.15.0.0 `bin/mhs`:

```
MHSDIR=. ./bin/mhs -imhs -isrc -ilib -ipaths MicroHs.Main -o out.c
33.31s user  0.12s system  99% cpu  33.456 total    ->  1.6MB C
```

Upstream's own rule adds `-z` (LZ77), which is why the committed `generated/mhs.c` is
511KB rather than 1.6MB. Add `cc` time on that file. The step caches: it only reruns when
MicroHs sources or the embedded packages change. This is comparable to what the current
`mcabal build` plus `mhs-embed` plus zstd pipeline already costs.

### Provenance

No reference to psnd, loki, or the author appears anywhere in the MicroHs repository or
its commit messages. The design is a plausible independent solution to the same problem.
Treat any claim of direct influence as unverified.

## Migration paths

### Path A - minimal bump

Keep the VFS. Fix `MHSDIR` by teaching `vfs.c` to answer the installed-layout probe
instead of the `MHSDIR` shortcut.

1. Replace `source/thirdparty/MicroHs/` with the `v0.16.5.0` tree.
2. Regenerate `impl/` from `src/runtime/`, including the six new files, applying the
   `mmalloc` rename.
3. Update the two version pins and the `targets.conf` -> `mhs.conf` copy.
4. Stop setting `MHSDIR`; make the VFS serve `<bin>/../mhs-0.16.5.0/packages/...` keyed
   off the psnd executable path.
5. Rebuild `base.pkg` and `music.pkg`; verify PKG_ZSTD startup.

Estimate: half a day mechanical, one to three days on step 4 with no automated coverage
to catch regressions.

### Path B - adopt `--embed-packages`

1. Steps 1 through 3 of Path A.
2. Add a build step that self-compiles the compiler with
   `--embed-packages base-0.16.5.0:music-0.1.0 -o mhs_psnd.c`, replacing the committed
   `generated/mhs.c` for psnd's build.
3. Delete the pkg, `.txt`, and package-path handling from `vfs.c`, `mhs-embed.c`, and
   `mhs-embed.py`.
4. Keep a reduced VFS for the runtime sources, `mhs.conf`, and static libraries, or set
   `MHS_ENABLE_COMPILATION=OFF` by default and delete the rest.

This deletes more code than it adds. Candidate for removal:

| File | Lines |
|-|-|
| `scripts/mhs-embed.c` | 1273 |
| `vfs.c` | 1148 |
| `scripts/mhs-embed.py` | 719 |
| `mhs_ffi_override.c` | 93 |
| `scripts/mhs-patch-eval.py` | 61 |
| `scripts/mhs-patch-xffi.py` | 27 |

Not all of it goes: the `-o` path keeps a reduced VFS, and `mhs-patch-xffi.py` is
independent of embedding. But the package half of `vfs.c` and most of `mhs-embed.c` do.

Path B is the correct destination. It should not be attempted at the same time as the
version bump.

## Why defer

- **No payoff psnd needs.** 0.16.x delivers a larger base library, LZMA, imath, and
  overflow checking. 0.17.0 adds string interpolation and qualified strings. None of it
  is used by a music DSL. No bug in psnd is fixed by the bump.
- **The cost is concentrated in one unavoidable redesign.** Item 1 is not a chore.
- **The safety net is two C files.** `source/langs/mhs/tests/` contains `test_context.c`
  and `test_midi_ffi.c`. Nothing exercises the Haskell path end to end. A bump cannot be
  verified as things stand.
- **Upstream is mid-flight.** `master` is 0.16.6.0 with 0.17.0 notes already written and
  an active `imath` branch. Porting to 0.16.5.0 now probably means porting again within
  months.

The counter-argument, and the strongest case for acting sooner: `--embed-packages` means
the bump can retire psnd-specific machinery rather than add to it. That argues for doing
the bump once, on Path B, when 0.17.0.0 ships.

## Preparatory work

Done. All three were independent of the bump and make it cheaper.

1. **Deleted the `impl/` fork.** `mhs-patch-eval.py` gained `--rename-malloc`, so the
   psnd runtime is generated from `source/thirdparty/MicroHs/src/runtime/eval.c` at build
   time alongside the VFS patch it already applied. `mhs.c` now comes from
   `generated/mhs.c`, the same file the standalone targets use. The generated
   `eval_psnd.c` was diffed against the fork's output and is byte-identical, so the
   refactor changed no compiled code. Item 3 of "What breaks" is now a non-event: the six
   new runtime files arrive with the vendored tree and need no manual copy.
2. **Single version pin.** `MHS_VERSION` is parsed from `MicroHs.cabal` with a format
   check. The two hardcoded `0.15.0.0` strings are gone, and `MHS_VERSION_PSND` was dead
   and was removed.
3. **End-to-end smoke test.** `mhs_smoke_tests` runs `tests/Smoke.hs` through the psnd
   binary and asserts on its output. It covers VFS init, the embedded base and music
   packages, compilation, and evaluation - the path the two existing C tests never touch.
   Runtime 2.25s. Both failure modes were checked: wrong output and a missing fixture each
   fail with a diagnostic.

   Writing the test exposed a separate regression: `main.c` diverted any command line
   carrying a registered extension to the editor, so `psnd mhs -r Smoke.hs` opened an
   editor instead of running the file. That affected all five languages and is fixed
   under its own CHANGELOG entry; the smoke test now uses the documented form.

## Reproducing this investigation

```sh
git clone --filter=blob:none --no-checkout https://github.com/augustss/MicroHs.git
git -C MicroHs diff --stat v0.15.0.0 v0.16.5.0 -- src/runtime/ lib/ src/MicroHs/
git -C MicroHs show v0.16.5.0:src/MicroHs/Compile.hs | sed -n '595,625p'   # getPaths
git -C MicroHs show v0.16.5.0:src/MicroHs/Main.hs    | sed -n '528,545p'   # addEmbedPkgs
```

Note that `zsh` applies history modifiers to `$tag:src/...`, so assign the path to a
variable before interpolating it into a `git show` argument.
