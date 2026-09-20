# Third-party licenses

Every file here is the verbatim license text of a vendored dependency, copied
from `source/thirdparty/<dep>/` or, where upstream ships no separate file,
extracted from the source header named at the top of the file.
`scripts/build_release.py` copies this directory into every release archive.

Adding a dependency means adding its license text here.

## Known gaps

Two dependencies are shipped without their license text:

- **zstd** (`source/thirdparty/zstd-1.5.7/`) - the amalgamated sources are
  vendored without upstream's `LICENSE` and `COPYING`. The headers offer
  BSD-3-Clause or GPL-2.0; fetch both texts from the pinned upstream release.
- **JUCE** - `source/thirdparty/minihost/CMakeLists.txt` fetches JUCE 8.0.4 at
  configure time, so nothing is vendored to copy. JUCE is AGPL-3.0 or
  commercial, which affects redistribution of the `minihost` variants.

`TODO.md` records the unresolved mongoose GPL-2.0-only conflict, which blocks
distribution of the web variants regardless of attribution.
