#!/bin/sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

if [ "${LIVE_BOOTSTRAP_BAKE_IN_NIX:-0}" != 1 ]; then
	exec nix develop "$ROOT" --command env LIVE_BOOTSTRAP_BAKE_IN_NIX=1 "$0" "$@"
fi

# Keep the pinned objects available when Savannah's Git service is unavailable.
# Command-line Git config is inherited by recursive submodule fetches.
git() {
	command git -c url.https://github.com/oriansj/mescc-tools.git.insteadOf=https://git.savannah.nongnu.org/git/mescc-tools.git "$@"
}

# shellcheck source=scripts/pins.sh
. "$ROOT/scripts/pins.sh"
WORKDIR=${WORKDIR:-"$ROOT/work"}
LIVE="$WORKDIR/live-bootstrap"
STAGE0="$LIVE/seed/stage0-posix"
LIVE_REPO=${LIVE_REPO:-https://github.com/fosslinux/live-bootstrap.git}

checkout_ref() {
	git -C "$1" fetch origin "$2"
	git -C "$1" checkout --detach FETCH_HEAD
}

apply_patch_once() {
	dir=$1
	patch=$2
	if git -C "$dir" apply --check "$patch"; then
		git -C "$dir" apply "$patch"
	elif git -C "$dir" apply --reverse --check "$patch"; then
		printf '%s already applied\n' "$patch"
	else
		printf 'cannot apply %s in %s\n' "$patch" "$dir" >&2
		git -C "$dir" apply --check "$patch"
	fi
}

mkdir -p "$WORKDIR"
if [ ! -d "$LIVE/.git" ]; then
	git clone "$LIVE_REPO" "$LIVE"
fi
checkout_ref "$LIVE" "$LIVE_BASE"
git -C "$LIVE" submodule update --init --recursive
checkout_ref "$LIVE/builder-hex0" "$BUILDER_HEX0_REF"
git -C "$LIVE/builder-hex0" submodule update --init --recursive
checkout_ref "$STAGE0" "$STAGE0_BASE"
git -C "$STAGE0" submodule update --init --recursive

checkout_ref "$STAGE0/M2-Planet" "$M2_PLANET_BASE"
checkout_ref "$STAGE0/mescc-tools" "$MESCC_BASE"
checkout_ref "$STAGE0/mescc-tools-extra" "$MESCC_EXTRA_REF"
checkout_ref "$STAGE0/M2-Mesoplanet" "$M2_MESOPLANET_REF"
checkout_ref "$STAGE0/bootstrap-seeds" "$BOOTSTRAP_SEEDS_REF"
checkout_ref "$STAGE0/x86" "$X86_BASE"
checkout_ref "$STAGE0/AMD64" "$AMD64_BASE"
checkout_ref "$STAGE0/AArch64" "$AARCH64_BASE"
checkout_ref "$STAGE0/armv7l" "$ARMV7L_BASE"
checkout_ref "$STAGE0/riscv32" "$RISCV32_BASE"
checkout_ref "$STAGE0/riscv64" "$RISCV64_BASE"

# Pin all copies, including each tool project's nested libc submodule.
checkout_ref "$STAGE0/M2libc" "$M2LIBC_REF"
for project in "$STAGE0/M2-Planet" "$STAGE0/mescc-tools" \
	"$STAGE0/mescc-tools-extra" "$STAGE0/M2-Mesoplanet"; do
	git -C "$project" submodule update --init --recursive
	checkout_ref "$project/M2libc" "$M2LIBC_REF"
done

git -C "$STAGE0/M2-Planet" fetch origin "$M2_PLANET_REF"
git -C "$STAGE0/M2-Planet" rev-parse FETCH_HEAD > "$WORKDIR/M2-Planet-pr.commit"
rm -rf "$STAGE0/M2-Planet-pr"
mkdir -p "$STAGE0/M2-Planet-pr"
git -C "$STAGE0/M2-Planet" archive FETCH_HEAD | tar -x -C "$STAGE0/M2-Planet-pr"

apply_patch_once "$STAGE0/mescc-tools" "$ROOT/patches/0001-mescc-tools-add-bake.patch"
apply_patch_once "$STAGE0/mescc-tools-extra" "$ROOT/patches/0002-mescc-tools-extra-use-bake.patch"
apply_patch_once "$STAGE0/x86" "$ROOT/patches/0003-stage0-posix-x86-use-bake.patch"
apply_patch_once "$STAGE0" "$ROOT/patches/0004-stage0-posix-bootstrap-bake.patch"
apply_patch_once "$LIVE" "$ROOT/patches/0005-live-bootstrap-use-bake.patch"
printf 'prepared %s\n' "$LIVE"
