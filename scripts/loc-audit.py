#!/usr/bin/env python3
"""Audit source deltas across the complete prepared tree, without gitlink duplication."""

import os
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKDIR = Path(os.environ.get("WORKDIR", ROOT / "work")).resolve()
WORK = WORKDIR / "live-bootstrap"
STAGE0 = WORK / "seed" / "stage0-posix"


def git(repo, *args):
    return subprocess.check_output(("git", "-C", str(repo), *args), text=True)


def child_repos(repo):
    children = []
    for line in git(repo, "ls-files", "--stage").splitlines():
        metadata, name = line.split("\t", 1)
        if metadata.split()[0] == "160000":
            path = repo / name
            if not (path / ".git").exists():
                raise SystemExit(f"uninitialized dependency: {path}")
            children.append((name, path))
    return children


def source_lines(path):
    if path.is_symlink():
        return 1
    data = path.read_bytes()
    if b"\0" in data:
        return 0
    return len(data.splitlines())


def repository_delta(repo, children):
    excluded = {name for name, _ in children}
    if repo == STAGE0:
        # This is an immutable dependency snapshot, audited separately below.
        excluded.add("M2-Planet-pr")

    def included(name):
        return not any(name == p or name.startswith(p + "/") for p in excluded)

    added = deleted = 0
    for line in git(repo, "diff", "--no-renames", "--numstat", "HEAD").splitlines():
        plus, minus, name = line.split("\t", 2)
        if included(name) and plus != "-" and minus != "-":
            added += int(plus)
            deleted += int(minus)
    for name in git(repo, "ls-files", "--others", "--exclude-standard").splitlines():
        if included(name):
            added += source_lines(repo / name)
    return added, deleted


def copied_compiler_delta():
    """Compare materialized PR sources against their exact source snapshot."""
    copied = STAGE0 / "M2-Planet-pr"
    marker = WORKDIR / "M2-Planet-pr.commit"
    if not marker.exists():
        raise SystemExit("compiler snapshot marker missing; run prepare.sh in a fresh WORKDIR")
    revision = marker.read_text().strip()
    with tempfile.TemporaryDirectory(prefix="bake-loc-") as directory:
        temp = Path(directory)
        archive = temp / "compiler.tar"
        with archive.open("wb") as output:
            subprocess.run(("git", "-C", str(STAGE0 / "M2-Planet"), "archive", revision),
                           stdout=output, check=True)
        reference = temp / "reference"
        reference.mkdir()
        with tarfile.open(archive) as source:
            source.extractall(reference, filter="data")
        result = subprocess.run(("git", "diff", "--no-index", "--no-renames", "--numstat",
                                 str(reference), str(copied)),
                                text=True, capture_output=True)
        if result.returncode not in (0, 1):
            raise SystemExit(result.stderr)
        added = deleted = 0
        for line in result.stdout.splitlines():
            plus, minus, _ = line.split("\t", 2)
            if plus != "-" and minus != "-":
                added += int(plus)
                deleted += int(minus)
        return added, deleted


def main():
    if not (WORK / ".git").exists():
        raise SystemExit("prepared tree missing; run ./scripts/prepare.sh first")
    rows = []

    def visit(name, repo):
        children = child_repos(repo)
        added, deleted = repository_delta(repo, children)
        rows.append((name, added, deleted))
        for child_name, child in children:
            label = "stage0-posix" if repo == WORK and child_name == "seed/stage0-posix" else name + "/" + child_name
            visit(label, child)
        if repo == STAGE0:
            added, deleted = copied_compiler_delta()
            rows.append((name + "/M2-Planet-pr", added, deleted))

    visit("live-bootstrap", WORK)
    print("| area | added | deleted | delta |")
    print("| --- | ---: | ---: | ---: |")
    for name, added, deleted in rows:
        print(f"| {name} | {added} | {deleted} | {added - deleted:+d} |")
    added = sum(row[1] for row in rows)
    deleted = sum(row[2] for row in rows)
    print(f"| total | {added} | {deleted} | {added - deleted:+d} |")


if __name__ == "__main__":
    main()
