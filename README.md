# live-bootstrap bake repro

This repository is a standalone repro for replacing the kaem-era
`live-bootstrap` package wrappers with `bake` recipes.

The patches are kept under `patches/` and apply to exact upstream base
revisions:

| project | upstream | base |
| --- | --- | --- |
| live-bootstrap | `https://github.com/fosslinux/live-bootstrap.git` | `9a268c4c39cae952b268bc86da342be2175f03d4` |
| stage0-posix | `https://github.com/oriansj/stage0-posix.git` | `45d90f5955b6907dc6cdea9ebafce558359edcd3` |
| mescc-tools | `https://git.savannah.nongnu.org/git/mescc-tools.git` | `5adfbf3364261a77109878a56b100aeeb6ef9ac4` |
| mescc-tools-extra | `https://github.com/oriansj/mescc-tools-extra.git` | `a151c245e512076971a3c85bb1502cf92cfa83b6` |
| stage0-posix-x86 | `https://github.com/oriansj/stage0-posix-x86.git` | `3b9c2bb6d4155e4f2e5f642b5e0f59255dfc5934` |

## Run

```sh
./scripts/prepare.sh
./scripts/run-kaem-era.sh
```

Both scripts re-enter `nix develop` automatically when needed. The validation
script generates a temporary live-bootstrap target, sets `BUILD_FIWIX=True`, and
runs the build in `bwrap` up to `bash-2.05b-pass1`. That covers every tracked
`steps/*/pass1.kaem` wrapper in the kaem-era part of the build.

The important bootstrap property is that `bake` is not built by the host
compiler. It is built by the stage0 path:

```text
hex0 -> cc_x86 -> M2 -> M1/hex2 -> x86/bin/bake
```

The expected successful tail is a checksum line for `/usr/bin/bash`.

For a fast harness check without launching the bootstrap, run:

```sh
GENERATE_ONLY=1 ./scripts/run-kaem-era.sh
```

## Patch Layout

`patches/0001-mescc-tools-add-bake.patch`
: adds the bootstrappable `bake` implementation and builds it in stage0.

`patches/0002-mescc-tools-extra-use-bake.patch`
: switches the extra stage0 tools build to `bake`.

`patches/0003-stage0-posix-x86-use-bake.patch`
: adds the x86 stage0 bake bootstrap recipe.

`patches/0004-stage0-posix-bootstrap-bake.patch`
: wires `bake` into the x86 stage0 seed answers and required submodule
  revisions.

`patches/0005-live-bootstrap-use-bake.patch`
: teaches `script-generator` to emit bake scripts before bash and adds
  `pass1.bake` recipes for all kaem-era packages.

For the currently covered package wrappers, the recipe layer goes from 2037
lines of `pass1.kaem` to 956 lines of `pass1.bake`.
