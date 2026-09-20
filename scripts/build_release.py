#!/usr/bin/env python3
"""Build psnd in Release mode and package a versioned, compressed archive.

This is the single build entry point shared by CI and the release workflow:

  - CI (`.github/workflows/ci.yml`) runs it with `--skip-package` to get a
    build + test + smoke check on every push / PR.
  - Release (`.github/workflows/release.yml`) runs it per matrix leg to
    produce `psnd-<version>-<target>[-<variant>].{tar.gz,zip}` in `dist/`,
    which the publish job attaches to the GitHub release.

It also works locally with zero arguments: it auto-detects the host target,
builds the default (TinySoundFont) variant, runs the test suite, smoke-tests
the binary, and writes the archive to `dist/`.

Steps: configure (CMake, Release) -> build -> ctest -> smoke (`psnd -V`) ->
stage + strip -> archive. Each step can be skipped with a flag.

The binary lands at `build/psnd` on macOS/Linux and `build/Release/psnd.exe`
under MSVC; both locations are probed. Archives use only the Python stdlib
(tarfile/zipfile via shutil.make_archive), so no `tar`/`7z`/`zip` tool is
required on the runner.
"""

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent

# variant -> extra CMake configure flags. "tsf" is the default, self-contained
# TinySoundFont build and is the one shipped as the bare `psnd-<ver>-<target>`
# asset; other variants append their name to the archive stem.
VARIANT_FLAGS: dict[str, list[str]] = {
    "tsf": [],
    "tsf-csound": ["-DBUILD_CSOUND_BACKEND=ON"],
    "fluid": ["-DBUILD_FLUID_BACKEND=ON"],
    "fluid-csound": ["-DBUILD_FLUID_BACKEND=ON", "-DBUILD_CSOUND_BACKEND=ON"],
    "tsf-web": ["-DBUILD_WEB_HOST=ON"],
    "fluid-web": ["-DBUILD_FLUID_BACKEND=ON", "-DBUILD_WEB_HOST=ON"],
    "fluid-csound-web": [
        "-DBUILD_FLUID_BACKEND=ON",
        "-DBUILD_CSOUND_BACKEND=ON",
        "-DBUILD_WEB_HOST=ON",
    ],
}


def log(msg: str) -> None:
    print(f"[build_release] {msg}", flush=True)


def run(cmd: list[str], cwd: Optional[Path] = None) -> None:
    log("$ " + " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd) if cwd else None, check=True)


def read_version(cmakelists: Path) -> str:
    """Parse the project version out of CMakeLists.txt."""
    text = cmakelists.read_text()
    m = re.search(r"project\(psnd_project\s+VERSION\s+(\d+\.\d+\.\d+)", text)
    if not m:
        raise SystemExit(f"could not find project VERSION in {cmakelists}")
    return m.group(1)


def detect_target() -> str:
    """Map the host OS + machine to a release target label."""
    system = platform.system()
    machine = platform.machine().lower()
    arch = {
        "x86_64": "x86_64",
        "amd64": "x86_64",
        "arm64": "arm64",
        "aarch64": "arm64",
    }.get(machine, machine)
    os_label = {"Darwin": "macos", "Linux": "linux", "Windows": "windows"}.get(
        system, system.lower()
    )
    return f"{os_label}-{arch}"


def locate_binary(build_dir: Path) -> Path:
    """Find the freshly built psnd binary across platform layouts."""
    candidates = [
        build_dir / "psnd",
        build_dir / "Release" / "psnd.exe",
        build_dir / "psnd.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c
    listing = "\n".join(
        f"  {p}" for p in sorted(build_dir.rglob("psnd*")) if p.is_file()
    )
    raise SystemExit(
        "could not locate built psnd binary. Searched:\n"
        + "\n".join(f"  {c}" for c in candidates)
        + (f"\nFound psnd* files:\n{listing}" if listing else "")
    )


def configure(build_dir: Path, variant: str, extra: list[str]) -> None:
    if variant not in VARIANT_FLAGS:
        raise SystemExit(
            f"unknown variant '{variant}'. Known: {', '.join(VARIANT_FLAGS)}"
        )
    cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_TESTING=ON",
        *VARIANT_FLAGS[variant],
        *extra,
    ]
    run(cmd)


def build(build_dir: Path, jobs: int) -> None:
    run(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--config",
            "Release",
            "--parallel",
            str(jobs),
        ]
    )


def run_tests(build_dir: Path) -> None:
    run(
        [
            "ctest",
            "--test-dir",
            str(build_dir),
            "--build-config",
            "Release",
            "--output-on-failure",
            # Default per-test ceiling. Tests carrying their own TIMEOUT
            # property keep it. Without this a test that blocks on stdin hangs
            # the whole job until the runner's 6-hour limit.
            "--timeout",
            "120",
        ]
    )


def smoke(binary: Path) -> None:
    """Run `psnd -V` and confirm it prints a version and exits cleanly."""
    log(f"$ {binary} -V")
    result = subprocess.run(
        [str(binary), "-V"], capture_output=True, text=True, check=True
    )
    out = (result.stdout + result.stderr).strip()
    log(f"smoke output: {out!r}")
    if "psnd" not in out:
        raise SystemExit(f"smoke check failed: unexpected output {out!r}")


def package(
    binary: Path,
    version: str,
    target: str,
    variant: str,
    dist_dir: Path,
    strip_symbols: bool,
) -> Path:
    """Stage the binary (optionally stripped) and produce a versioned archive."""
    stem = f"psnd-{version}-{target}"
    if variant != "tsf":
        stem += f"-{variant}"

    stage_dir = ROOT / "stage"
    if stage_dir.exists():
        shutil.rmtree(stage_dir)
    stage_dir.mkdir(parents=True)
    dist_dir.mkdir(parents=True, exist_ok=True)

    bin_name = "psnd.exe" if binary.suffix == ".exe" else "psnd"
    staged = stage_dir / bin_name
    shutil.copy2(binary, staged)

    # Bundle the licence alongside the binary for redistribution.
    license_file = ROOT / "LICENSE"
    if license_file.is_file():
        shutil.copy2(license_file, stage_dir / "LICENSE")

    # Vendored dependencies carry their own terms, several of them LGPL or
    # GPL, so the archive has to ship their notices too. Missing the directory
    # is a packaging error, not a variant that happens to need no attribution.
    licenses_dir = ROOT / "docs" / "licenses"
    if not licenses_dir.is_dir():
        raise SystemExit(f"third-party licenses missing: {licenses_dir}")
    shutil.copytree(licenses_dir, stage_dir / "licenses")

    if strip_symbols and platform.system() != "Windows":
        strip = shutil.which("strip")
        if strip:
            before = staged.stat().st_size
            run([strip, str(staged)])
            after = staged.stat().st_size
            log(f"stripped {bin_name}: {before} -> {after} bytes")
        else:
            log("strip not found; shipping unstripped binary")

    fmt = "zip" if platform.system() == "Windows" else "gztar"
    base = dist_dir / stem
    archive = shutil.make_archive(str(base), fmt, root_dir=str(stage_dir))
    archive_path = Path(archive)
    log(f"archive: {archive_path} ({archive_path.stat().st_size} bytes)")
    return archive_path


def emit_github_output(**values: str) -> None:
    """Write key=value lines to $GITHUB_OUTPUT when running under Actions."""
    out = os.environ.get("GITHUB_OUTPUT")
    if not out:
        return
    with open(out, "a", encoding="utf-8") as fh:
        for key, value in values.items():
            fh.write(f"{key}={value}\n")


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Build and package a psnd release artifact."
    )
    parser.add_argument(
        "--variant",
        default="tsf",
        choices=sorted(VARIANT_FLAGS),
        help="Build variant (default: tsf, the self-contained TSF build).",
    )
    parser.add_argument(
        "--target",
        default=None,
        help="Release target label, e.g. macos-arm64 (default: auto-detect).",
    )
    parser.add_argument(
        "--build-dir", default="build", help="CMake build directory (default: build)."
    )
    parser.add_argument(
        "--dist-dir",
        default="dist",
        help="Where to write archives (default: dist).",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=os.cpu_count() or 2,
        help="Parallel build jobs (default: CPU count).",
    )
    parser.add_argument(
        "--cmake-arg",
        action="append",
        default=[],
        dest="cmake_args",
        help="Extra -D flag passed to CMake configure (repeatable).",
    )
    parser.add_argument(
        "--skip-configure", action="store_true", help="Reuse an existing build dir."
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip cmake --build.")
    parser.add_argument("--skip-tests", action="store_true", help="Skip ctest.")
    parser.add_argument(
        "--skip-smoke", action="store_true", help="Skip the `psnd -V` smoke check."
    )
    parser.add_argument(
        "--skip-package",
        action="store_true",
        help="Build/test only; do not produce an archive (CI mode).",
    )
    parser.add_argument(
        "--no-strip",
        action="store_true",
        help="Do not strip debug symbols from the staged binary.",
    )
    args = parser.parse_args(argv)

    version = read_version(ROOT / "CMakeLists.txt")
    target = args.target or detect_target()
    build_dir = (ROOT / args.build_dir).resolve()
    dist_dir = (ROOT / args.dist_dir).resolve()

    log(f"psnd {version} | variant={args.variant} | target={target}")

    if not args.skip_configure:
        configure(build_dir, args.variant, args.cmake_args)
    if not args.skip_build:
        build(build_dir, args.jobs)
    if not args.skip_tests:
        run_tests(build_dir)

    binary = locate_binary(build_dir)
    log(f"binary: {binary}")

    if not args.skip_smoke:
        smoke(binary)

    if args.skip_package:
        log("packaging skipped (--skip-package)")
        emit_github_output(version=version, target=target)
        return 0

    archive = package(
        binary,
        version,
        target,
        args.variant,
        dist_dir,
        strip_symbols=not args.no_strip,
    )
    emit_github_output(
        version=version,
        target=target,
        archive=str(archive),
        archive_name=archive.name,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
