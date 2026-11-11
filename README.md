# live-bootstrap bake repro

This repository is a standalone repro for replacing the pre-bash
`live-bootstrap` package wrappers with `bake` recipes.

The patches are kept under `patches/` and apply to exact upstream base
revisions:

| project | upstream | base |
| --- | --- | --- |
| live-bootstrap | `https://github.com/fosslinux/live-bootstrap.git` | `199086e8d4aaab9e609b5f5eddaa9dfbb375ed1e` |
| builder-hex0 | `https://github.com/ironmeld/builder-hex0.git` | `b8a2047910d47c62fe65bcd762f6c747c45c7549` |
| stage0-posix | `https://github.com/oriansj/stage0-posix.git` | `643598041bf7639883874fe2cdc9d9693c9b03d5` |
| M2-Planet | `https://github.com/oriansj/M2-Planet.git` | `ebfecc2fcb2c66925b94bdd4aa11fd6ac48bb172` |
| M2-Planet-pr | `https://github.com/oriansj/M2-Planet.git` | `4d2a14d793d6abcc5474b123d7714e6c13f2eac1` (PR #169) |
| mescc-tools | `https://github.com/oriansj/mescc-tools.git` | `d59464d2641de8a90032ad2456bdb34b4db3436a` |
| mescc-tools-extra | `https://github.com/oriansj/mescc-tools-extra.git` | `651d4006076ceb9fe6fca17ecf6a4ecb355c6730` (PR #30) |
| M2libc (all copies) | `https://github.com/oriansj/M2libc.git` | `b83e1243fb28aaf9e56ce53bd1da292d53c4c405` (PR #125) |
| M2-Mesoplanet | `https://github.com/oriansj/M2-Mesoplanet.git` | `e9d604c1fe3430109939176eb38a5814845380f9` |
| bootstrap-seeds | `https://github.com/oriansj/bootstrap-seeds.git` | `cedec6b8066d1db229b6c77d42d120a23c6980ed` |
| stage0-posix-x86 | `https://github.com/oriansj/stage0-posix-x86.git` | `14721630c8d3e4a7de943e27488d0d582788837f` |
| stage0-posix-amd64 | `https://github.com/oriansj/stage0-posix-amd64.git` | `15535f88e25825f01a0de275b6d45f77e618bd6b` |
| stage0-posix-aarch64 | `https://github.com/oriansj/stage0-posix-aarch64.git` | `cedcddd24e74d10d0e41dd647e413fd7ee476530` |
| stage0-posix-armv7l | `https://github.com/oriansj/stage0-posix-armv7l.git` | `ed899b65083d45095b0094a20311bcbf0983aaa8` |
| stage0-posix-riscv32 | `https://github.com/oriansj/stage0-posix-riscv32.git` | `2135f3203632b5d1bd5239f84bac50eccaeaf74f` |
| stage0-posix-riscv64 | `https://github.com/oriansj/stage0-posix-riscv64.git` | `7446a846e3e5b97eba5227e4b756f4ef257aa9e5` |

The defaults live in `scripts/pins.sh`; all are immutable commit IDs, including
snapshots of the still-unmerged PRs. M2-Planet-pr supplies normal pointer
arithmetic for `wrap`, mescc-tools-extra removes its old workaround, and M2libc
supplies their runtime fixes. Other projects track the latest upstream default
branches at the pin refresh. The Nix development environment is pinned separately
in `flake.lock`. Only the x86 pre-bash path is validated end to end.

## Run

```sh
./scripts/prepare.sh
./scripts/run-bake.sh
```

Both scripts re-enter `nix develop` automatically when needed. The validation
script generates a temporary live-bootstrap target, sets `BUILD_FIWIX=True`, and
runs the build in `bwrap` up to `bash-2.05b-pass1`. That covers every tracked
`steps/*/pass1.kaem` wrapper in the pre-bash part of the build.

The important bootstrap property is that `bake` is not built by the host
compiler. It is built by the stage0 path:

```text
hex0 -> cc_x86 -> M2 -> M1/hex2 -> x86/bin/bake
```

This pin set passed a fresh source preparation and end-to-end build with
`UPDATE_CHECKSUMS=False`, ending in `/usr/bin/bash: OK`. The resulting bash is
byte-identical to the existing expected build:

```text
d6f725a9998443629e9d8496df3ed4a938a9bdc80cd1aab2b31b011b99f0dae4
```

The Mes recipe unlinks the SRFI-9 destination symlinks before copying their
referents, avoiding source truncation with the corrected libc stream behavior.
The local run passed 137 checksum entries with recording disabled. Shellcheck,
a smoke test of the bootstrapped bash, and native mescc-tools/bake tests passed.
AMD64 and AArch64 stage0 toolchains each reproduced 19 checked binary hashes.
The AArch64 M1 and GAS compiler regression suites also passed.

RISC-V validation is deferred. Its recipe ports are not certified with these
pins, and its upstream answer lists have not been regenerated; those targets
still need a validated checksum refresh before use.

Use a fresh `WORKDIR` when changing pins, and set the same `WORKDIR` for both
scripts. `TARGET` selects the generated build directory. To record new build
checksums during a pin refresh, `UPDATE_CHECKSUMS=True` writes them into that
target; review them and require a separate clean verifying run afterward.

For a fast harness check without launching the bootstrap, run:

```sh
GENERATE_ONLY=1 ./scripts/run-bake.sh
```

## LOC Audit

The LOC audit should count source that is materialized into the bootstrap root,
not generated build output. For the full branch this means counting the
top-level `seed`/`steps` changes and each checked-out `stage0-posix` submodule
as source, while ignoring parent gitlink rows.

The useful validation boundary is the pre-bash build: minimal kaem still runs
the seed scripts that build `bake`, then `bake` runs through
`bash-2.05b-pass1` and hands off to `bash /steps/1.sh`. Later packages are
useful as broader regression coverage, but they are not needed to measure the
kaem-to-bake replacement.

The full materialized branch audit gives:

| area | added | deleted | delta |
| --- | ---: | ---: | ---: |
| live-bootstrap | 1965 | 2298 | -333 |
| live-bootstrap/builder-hex0 | 0 | 0 | +0 |
| stage0-posix | 177 | 124 | +53 |
| stage0-posix/AArch64 | 37 | 240 | -203 |
| stage0-posix/AMD64 | 11 | 240 | -229 |
| stage0-posix/M2-Mesoplanet | 0 | 0 | +0 |
| stage0-posix/M2-Mesoplanet/M2libc | 0 | 0 | +0 |
| stage0-posix/M2-Planet | 0 | 0 | +0 |
| stage0-posix/M2-Planet/M2libc | 0 | 0 | +0 |
| stage0-posix/M2libc | 0 | 0 | +0 |
| stage0-posix/armv7l | 0 | 0 | +0 |
| stage0-posix/bootstrap-seeds | 0 | 0 | +0 |
| stage0-posix/mescc-tools | 1069 | 2520 | -1451 |
| stage0-posix/mescc-tools/M2libc | 0 | 0 | +0 |
| stage0-posix/mescc-tools-extra | 9 | 45 | -36 |
| stage0-posix/mescc-tools-extra/M2libc | 0 | 0 | +0 |
| stage0-posix/riscv32 | 11 | 244 | -233 |
| stage0-posix/riscv64 | 11 | 240 | -229 |
| stage0-posix/x86 | 12 | 241 | -229 |
| stage0-posix/M2-Planet-pr | 0 | 0 | +0 |
| total | 3302 | 6192 | -2890 |

The converted package wrappers are removed, not retained alongside their bake
replacements. All five stage0 architectures share the full-tool rebuild and
orchestration recipes; only their machine parameters differ. The full kaem
interpreter is removed, including its build/install hooks and obsolete tests.
Seed/minimal kaem remains necessary to bootstrap bake.

These are complete source deltas against each dependency's pinned checkout,
including new regression tests and documentation. The copied PR compiler is
compared against its exact archived commit. Gitlinks are excluded from parent
rows; nested dependencies are counted separately. Generated build outputs and
the textual size of the patch files are not counted.

For this standalone repro checkout, after `./scripts/prepare.sh`:

```sh
python3 scripts/loc-audit.py
```

prints a numstat table for the complete prepared `work/live-bootstrap` tree and
all initialized submodules. Set `WORKDIR` when auditing another prepared tree.

## Patch Layout

The `bake-files/` directory also contains the `.bake` files directly, mirrored
at their intended upstream paths. This makes the recipes readable without first
applying the patch stack.

`patches/0001-mescc-tools-add-bake.patch`
: replaces the full kaem interpreter with bootstrappable `bake`, updates native
  build/install hooks, and adds recipe-runner regression tests.

`patches/0002-mescc-tools-extra-use-bake.patch`
: switches the extra stage0 tools build to `bake`.

`patches/0003-stage0-posix-x86-use-bake.patch`
: builds bake directly in the x86 minimal seed chain and removes its duplicated
  full-tool rebuild script.

`patches/0004-stage0-posix-bootstrap-bake.patch`
: shares `bootstrap.common.bake` and `mescc-tools-full.bake` across the five
  architectures, adds small machine-parameter recipes, and verifies tool hashes.

`patches/0005-live-bootstrap-use-bake.patch`
: teaches `script-generator` to emit bake scripts before bash, replaces the
  seed handoff with `seed.bake`, and adds `pass1.bake` recipes for all
  pre-bash packages, removing their obsolete kaem wrappers.

`patches/0006-stage0-posix-amd64-use-bake.patch` through
`patches/0009-stage0-posix-riscv64-use-bake.patch`
: switch the remaining supported stage0 seed chains to bake and remove their
  duplicated full-tool rebuild scripts. The AArch64 patch also preserves live
  operands across token guards and adds compile-and-run regressions.

For the currently covered package wrappers, the recipe layer goes from 2037
lines of `pass1.kaem` to 843 lines of `pass1.bake`, including preserved SPDX
attribution and license headers. Fiwix and lwext4 now use
their upstream Makefiles after small bootstrap-environment patches.
