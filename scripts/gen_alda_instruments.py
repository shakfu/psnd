#!/usr/bin/env python3
"""Generate Alda's instrument table from the Alda language docs.

Reads source/langs/alda/docs/alda-language/list-of-instruments.md, Alda's list
of instrument names and aliases, and writes
source/langs/alda/impl/instruments_alda.inc.

The document lists instruments grouped by General MIDI patch group, eight per
group, in program order. Each bullet has the form::

    * canonical-name (alias, alias, ...)

Run from the project root::

    python3 scripts/gen_alda_instruments.py

The generated file is committed, so building needs no Python.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

DOC_PATH = Path("source/langs/alda/docs/alda-language/list-of-instruments.md")
OUT_PATH = Path("source/langs/alda/impl/instruments_alda.inc")

PROGRAMS = 128
HEADING_RE = re.compile(r"^###\s+(.+?)\s*$")
BULLET_RE = re.compile(r"^\*\s+(?P<name>[a-z0-9+\-]+)\s*(?:\((?P<aliases>[^)]*)\)?)?\s*$")


def parse_doc(text: str) -> list[tuple[str, str, list[str]]]:
    """(group, canonical name, aliases) for each instrument, in program order."""
    entries = []
    group = ""
    for line in text.splitlines():
        heading = HEADING_RE.match(line)
        if heading:
            group = heading.group(1)
            continue
        bullet = BULLET_RE.match(line.strip())
        if bullet:
            aliases = [a.strip() for a in (bullet.group("aliases") or "").split(",")]
            entries.append((group, bullet.group("name"), [a for a in aliases if a]))
    return entries


def render(entries: list[tuple[str, str, list[str]]]) -> str:
    seen: dict[str, int] = {}
    lines = [
        "/* Alda's instrument names and their General MIDI programs.",
        " *",
        " * GENERATED FILE - do not edit by hand. Regenerate with",
        " * scripts/gen_alda_instruments.py, which reads",
        " * source/langs/alda/docs/alda-language/list-of-instruments.md. */",
        "",
    ]
    current = None
    for program, (group, name, aliases) in enumerate(entries):
        if group != current:
            lines.append(f"/* {group} */")
            current = group
        for key in [name, *aliases]:
            if key in seen and seen[key] != program:
                raise SystemExit(f"{key!r} names programs {seen[key]} and {program}")
            seen[key] = program
            lines.append(f'{{"{key}", {program}}},')
    return "\n".join(lines) + "\n"


def main() -> int:
    if not DOC_PATH.exists():
        print(f"Not found: {DOC_PATH}. Run from the project root.", file=sys.stderr)
        return 1
    entries = parse_doc(DOC_PATH.read_text(encoding="utf-8"))
    if len(entries) != PROGRAMS:
        print(f"Expected {PROGRAMS} instruments, parsed {len(entries)}; "
              "the document format may have changed.", file=sys.stderr)
        return 1
    OUT_PATH.write_text(render(entries), encoding="utf-8")
    names = sum(1 + len(aliases) for _, _, aliases in entries)
    print(f"Wrote {OUT_PATH}: {PROGRAMS} programs, {names} names")
    return 0


if __name__ == "__main__":
    sys.exit(main())
