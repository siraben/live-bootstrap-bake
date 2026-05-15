#!/usr/bin/env python3
"""Print LOC delta for the prepared bake repro worktree."""

import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work" / "live-bootstrap"


@dataclass
class RepoSpec:
    name: str
    path: Path
    base: str
    paths: tuple[str, ...] = ()
    exclude: tuple[str, ...] = ()


SPECS = (
    RepoSpec(
        "live-bootstrap",
        WORK,
        "9a268c4c39cae952b268bc86da342be2175f03d4",
        ("DEVEL.md", "REUSE.toml", "lib", "rootfs.py", "seed", "steps"),
        ("seed/stage0-posix",),
    ),
    RepoSpec(
        "stage0-posix",
        WORK / "seed" / "stage0-posix",
        "45d90f5955b6907dc6cdea9ebafce558359edcd3",
        (),
        ("mescc-tools", "mescc-tools-extra", "x86"),
    ),
    RepoSpec(
        "stage0-posix-x86",
        WORK / "seed" / "stage0-posix" / "x86",
        "3b9c2bb6d4155e4f2e5f642b5e0f59255dfc5934",
    ),
    RepoSpec(
        "mescc-tools",
        WORK / "seed" / "stage0-posix" / "mescc-tools",
        "5adfbf3364261a77109878a56b100aeeb6ef9ac4",
    ),
    RepoSpec(
        "mescc-tools-extra",
        WORK / "seed" / "stage0-posix" / "mescc-tools-extra",
        "a151c245e512076971a3c85bb1502cf92cfa83b6",
    ),
)


def git(repo: Path, *args: str) -> str:
    return subprocess.check_output(("git", "-C", str(repo), *args), text=True)


def include_path(path: str, spec: RepoSpec) -> bool:
    return path not in spec.exclude


def count_file_lines(path: Path) -> int:
    try:
        return sum(1 for _ in path.open("rb"))
    except IsADirectoryError:
        return 0


def numstat(spec: RepoSpec) -> tuple[int, int]:
    args = ["diff", "--numstat", spec.base]
    if spec.paths:
        args.append("--")
        args.extend(spec.paths)
    added = 0
    deleted = 0
    for line in git(spec.path, *args).splitlines():
        if not line.strip():
            continue
        add, delete, path = line.split("\t", 2)
        if add == "-" or delete == "-" or not include_path(path, spec):
            continue
        added += int(add)
        deleted += int(delete)

    others = git(spec.path, "ls-files", "--others", "--exclude-standard")
    for path in others.splitlines():
        if not include_path(path, spec):
            continue
        if spec.paths and not any(path == prefix or path.startswith(prefix + "/") for prefix in spec.paths):
            continue
        added += count_file_lines(spec.path / path)
    return added, deleted


def main() -> None:
    if not WORK.exists():
        raise SystemExit("work/live-bootstrap is missing; run ./scripts/prepare.sh first")

    rows = []
    total_added = 0
    total_deleted = 0
    for spec in SPECS:
        added, deleted = numstat(spec)
        total_added += added
        total_deleted += deleted
        rows.append((spec.name, added, deleted, added - deleted))

    print("| project | added | deleted | delta |")
    print("| --- | --- | --- | --- |")
    for name, added, deleted, delta in rows:
        print(f"| {name} | {added} | {deleted} | {delta:+d} |")
    print(f"| total | {total_added} | {total_deleted} | {total_added - total_deleted:+d} |")


if __name__ == "__main__":
    main()
