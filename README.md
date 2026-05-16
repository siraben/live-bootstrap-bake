# live-bootstrap bake repro

This repository is a standalone repro for replacing the pre-bash
`live-bootstrap` package wrappers with `bake` recipes.

The patches are kept under `patches/` and apply to exact upstream base
revisions:

| project | upstream | base |
| --- | --- | --- |
| live-bootstrap | `https://github.com/fosslinux/live-bootstrap.git` | `9a268c4c39cae952b268bc86da342be2175f03d4` |
| stage0-posix | `https://github.com/oriansj/stage0-posix.git` | `45d90f5955b6907dc6cdea9ebafce558359edcd3` |
| M2-Planet-pr | `https://github.com/oriansj/M2-Planet.git` | `refs/pull/169/head` |
| mescc-tools | `https://git.savannah.nongnu.org/git/mescc-tools.git` | `5adfbf3364261a77109878a56b100aeeb6ef9ac4` |
| mescc-tools-extra | `https://github.com/oriansj/mescc-tools-extra.git` | `refs/pull/30/head` |
| stage0-posix-x86 | `https://github.com/oriansj/stage0-posix-x86.git` | `3b9c2bb6d4155e4f2e5f642b5e0f59255dfc5934` |

The M2-Planet-pr and mescc-tools-extra rows intentionally use PR heads instead
of local patches: M2-Planet-pr carries the grammar and pointer-arithmetic fixes,
and mescc-tools-extra removes the matching `wrap.c` workaround.

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

The expected successful tail is a checksum line for `/usr/bin/bash`.

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

The full materialized branch audit currently gives:

| area | added | deleted | delta |
| --- | ---: | ---: | ---: |
| live-bootstrap seed/steps | 1456 | 2246 | -790 |
| stage0-posix wrapper | 129 | 227 | -98 |
| stage0-posix/x86 | 12 | 262 | -250 |
| stage0-posix/AMD64 | 12 | 262 | -250 |
| stage0-posix/AArch64 | 12 | 262 | -250 |
| stage0-posix/riscv32 | 10 | 264 | -254 |
| stage0-posix/riscv64 | 10 | 260 | -250 |
| stage0-posix/armv7l | 1 | 1 | 0 |
| mescc-tools | 954 | 2478 | -1524 |
| mescc-tools-extra | 8 | 61 | -53 |
| total | 2604 | 6323 | -3719 |

The largest reductions are from deleting the full kaem implementation and the
old simple-patch before/after data. The main costs are `bake.c`, the `.bake`
recipes, and the standard patch reader.

For this standalone repro checkout, after `./scripts/prepare.sh`:

```sh
python3 scripts/loc-audit.py
```

prints a numstat table for the prepared `work/live-bootstrap` tree and patched
submodules.

## Patch Layout

The `bake-files/` directory also contains the `.bake` files directly, mirrored
at their intended upstream paths. This makes the recipes readable without first
applying the patch stack.

`patches/0001-mescc-tools-add-bake.patch`
: adds the bootstrappable `bake` implementation and builds it in stage0.

`patches/0002-mescc-tools-extra-use-bake.patch`
: switches the extra stage0 tools build to `bake`.

`patches/0003-stage0-posix-x86-use-bake.patch`
: adds the x86 stage0 bake bootstrap recipe.

`patches/0004-stage0-posix-bootstrap-bake.patch`
: wires `bake` into the x86 stage0 seed answers, hands the x86 full-tool
  rebuild from kaem to bake, and records required submodule revisions.

`patches/0005-live-bootstrap-use-bake.patch`
: teaches `script-generator` to emit bake scripts before bash, replaces the
  seed handoff with `seed.bake`, and adds `pass1.bake` recipes for all
  pre-bash packages.

For the currently covered package wrappers, the recipe layer goes from 2037
lines of `pass1.kaem` to 790 lines of `pass1.bake`.
