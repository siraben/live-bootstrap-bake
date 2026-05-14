#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "${LIVE_BOOTSTRAP_BAKE_IN_NIX:-0}" != 1 ]; then
	exec nix develop "$ROOT" --command env LIVE_BOOTSTRAP_BAKE_IN_NIX=1 "$0" "$@"
fi

WORKDIR=${WORKDIR:-"$ROOT/work"}
LIVE="$WORKDIR/live-bootstrap"
TARGET=${TARGET:-"$ROOT/target-bake-kaem"}

if [ ! -d "$LIVE/.git" ]; then
	"$ROOT/scripts/prepare.sh"
fi

cd "$LIVE"

rm -rf "$TARGET"
backup=$(mktemp)
cp steps/manifest "$backup"
restore_manifest() {
	cp "$backup" steps/manifest
	rm -f "$backup"
}
trap restore_manifest EXIT

awk '{ print; if ($0 == "build: bash-2.05b") exit }' "$backup" > steps/manifest

python3 - "$TARGET" <<'PY'
from pathlib import Path
import sys
from lib.generator import Generator
from lib.target import Target

target = Path(sys.argv[1])
g = Generator(arch="x86", external_sources=False, early_preseed=None, repo_path=None, mirrors=[])
g.prepare(Target(path=str(target)), using_kernel=False)

(target / "steps/bootstrap.cfg").write_text("""ARCH=x86
ARCH_DIR=x86
FORCE_TIMESTAMPS=False
CHROOT=True
UPDATE_CHECKSUMS=False
JOBS=2
SWAP_SIZE=0
FINAL_JOBS=2
INTERNAL_CI=pass1
INTERACTIVE=False
QEMU=False
BARE_METAL=False
DISK=sda1
KERNEL_BOOTSTRAP=False
BUILD_KERNELS=True
CONFIGURATOR=False
MIRRORS_LEN=0
BUILD_FIWIX=True
CONSOLES=False
BUILD_LINUX=False
DISTFILES=/external/distfiles
PREFIX=/usr
BINDIR=/usr/bin
LIBDIR=/usr/lib/mes
INCDIR=/usr/include/mes
SRCDIR=/steps
TMPDIR=/tmp
PATH=/usr/bin:/x86/bin
""")

seed = target / "seed.bake"
seed.write_text(seed.read_text().replace(
    "bake --file /steps/0.bake all",
    "bake --file /steps/0.bake bash-2.05b-pass1",
))
PY

trap - EXIT
restore_manifest

if [ "${GENERATE_ONLY:-0}" = 1 ]; then
	printf 'generated %s\n' "$TARGET"
	exit 0
fi

env -i bwrap \
	--unshare-user --uid 0 --gid 0 --unshare-net \
	--setenv PATH /usr/bin \
	--bind "$TARGET" / \
	--dir /dev \
	--dev-bind /dev/null /dev/null \
	--dev-bind /dev/zero /dev/zero \
	--dev-bind /dev/random /dev/random \
	--dev-bind /dev/urandom /dev/urandom \
	--dev-bind /dev/ptmx /dev/ptmx \
	--dev-bind /dev/tty /dev/tty \
	--tmpfs /dev/shm \
	--proc /proc \
	--bind /sys /sys \
	--tmpfs /tmp \
	/bootstrap-seeds/POSIX/x86/kaem-optional-seed
