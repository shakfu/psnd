#!/usr/bin/env python3
"""Check that the vendored third-party trees survive a fresh checkout.

Vendored trees carry their upstream .gitignore, whose patterns apply inside
this repo. A nested .gitignore beats any negation in the root one, so an
upstream-tracked file matching such a pattern arrives untracked and is silently
dropped from every clone. Nothing shows up locally; the build fails only on CI.

Two checks:
  1. Every file a local patch applies to is tracked.
  2. No file under source/thirdparty is ignored-but-present, except the
     artifacts listed in ALLOWED. This one only has teeth on a working machine,
     since a fresh checkout has no such files to find.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENDOR = "source/thirdparty"
MHS_PATCH_DIR = ROOT / "source/langs/mhs/patches"
MHS_DIR = "source/thirdparty/MicroHs"

# Upstream tracks these, psnd's build reads none of them, and together they are
# ~4MB of generated blobs. Left out of the repo deliberately.
ALLOWED = (
    f"{MHS_DIR}/bin/",
    f"{MHS_DIR}/generated/base.pkg",
    f"{MHS_DIR}/generated/mhseval.js",
    f"{MHS_DIR}/tests/",
    f"{MHS_DIR}/web-mhs/mhs-embed.js",
)


def git(*args):
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True
    )


def patch_targets():
    """Paths named by `+++ b/<path>` in each MicroHs patch, repo-relative."""
    targets = set()
    for patch in sorted(MHS_PATCH_DIR.glob("*.diff")):
        for line in patch.read_text().splitlines():
            if line.startswith("+++ b/"):
                path = line[len("+++ b/") :].split("\t")[0].strip()
                targets.add((patch.name, f"{MHS_DIR}/{path}"))
    return sorted(targets)


def check_patch_targets():
    errors = []
    for patch_name, path in patch_targets():
        if git("ls-files", "--error-unmatch", "--", path).returncode != 0:
            errors.append(f"{path}: not tracked, but patched by {patch_name}")
    return errors


def check_ignored_files():
    result = git("ls-files", "-o", "-i", "--exclude-standard", "--", VENDOR)
    errors = []
    for path in result.stdout.splitlines():
        if path.endswith(".DS_Store") or path.startswith(ALLOWED):
            continue
        errors.append(f"{path}: ignored, so absent from a fresh checkout")
    return errors


def main():
    errors = check_patch_targets() + check_ignored_files()
    if errors:
        print("vendored source check failed:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        print(
            "\nTrack the file with `git add -f <path>` and un-ignore it in the\n"
            "vendored .gitignore, or add it to ALLOWED in this script.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
