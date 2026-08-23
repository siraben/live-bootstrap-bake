#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "${LIVE_BOOTSTRAP_BAKE_IN_NIX:-0}" != 1 ]; then
	exec nix develop "$ROOT" --command env LIVE_BOOTSTRAP_BAKE_IN_NIX=1 "$0" "$@"
fi

WORKDIR=${WORKDIR:-"$ROOT/work"}
LIVE="$WORKDIR/live-bootstrap"
STAGE0="$LIVE/seed/stage0-posix"
MESCC="$STAGE0/mescc-tools"
MESCC_EXTRA="$STAGE0/mescc-tools-extra"
M2_PLANET="$STAGE0/M2-Planet"
M2_PLANET_PR="$STAGE0/M2-Planet-pr"
X86="$STAGE0/x86"

LIVE_REPO=${LIVE_REPO:-https://github.com/fosslinux/live-bootstrap.git}
LIVE_BASE=${LIVE_BASE:-9a268c4c39cae952b268bc86da342be2175f03d4}
STAGE0_BASE=${STAGE0_BASE:-643598041bf7639883874fe2cdc9d9693c9b03d5}
MESCC_BASE=${MESCC_BASE:-d59464d2641de8a90032ad2456bdb34b4db3436a}
M2_PLANET_REF=${M2_PLANET_REF:-34fbd5c2a9b6eb634a4f6ad95158dcd1efcf19e0}
MESCC_EXTRA_REF=${MESCC_EXTRA_REF:-d1d814602d83226c5a5e597e13dc8013a005ce76}
X86_BASE=${X86_BASE:-14721630c8d3e4a7de943e27488d0d582788837f}

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

git -C "$LIVE" fetch origin
git -C "$LIVE" checkout "$LIVE_BASE"
git -C "$LIVE" submodule update --init --recursive

git -C "$STAGE0" checkout "$STAGE0_BASE"
git -C "$STAGE0" submodule update --init --recursive
git -C "$X86" checkout "$X86_BASE"

git -C "$MESCC" checkout "$MESCC_BASE"
git -C "$M2_PLANET" fetch origin "$M2_PLANET_REF"
rm -rf "$M2_PLANET_PR"
mkdir -p "$M2_PLANET_PR"
git -C "$M2_PLANET" archive FETCH_HEAD | tar -x -C "$M2_PLANET_PR"
git -C "$MESCC_EXTRA" fetch origin "$MESCC_EXTRA_REF"
git -C "$MESCC_EXTRA" checkout FETCH_HEAD
git -C "$MESCC_EXTRA" submodule update --init --recursive

apply_patch_once "$MESCC" "$ROOT/patches/0001-mescc-tools-add-bake.patch"
apply_patch_once "$MESCC_EXTRA" "$ROOT/patches/0002-mescc-tools-extra-use-bake.patch"
apply_patch_once "$X86" "$ROOT/patches/0003-stage0-posix-x86-use-bake.patch"
apply_patch_once "$STAGE0" "$ROOT/patches/0004-stage0-posix-bootstrap-bake.patch"
apply_patch_once "$LIVE" "$ROOT/patches/0005-live-bootstrap-use-bake.patch"

printf 'prepared %s\n' "$LIVE"
