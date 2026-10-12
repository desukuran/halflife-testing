#!/bin/bash
# Zelda 64 port: copy the mod into a Half-Life install and start it.
#
#   ./run_zelda.sh [game options]           e.g. ./run_zelda.sh +map c1a0
#
# HL_DIR is the Half-Life folder (the one with hl.sh and valve/), ~/half-life/Half-Life by
# default. The repository's zelda/ folder (with the dlls build_zelda.sh put there) is copied
# over $HL_DIR/zelda; files only in the install, like the ROM assets, are left alone.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
HL_DIR="${HL_DIR:-$HOME/half-life/Half-Life}"

if [ ! -x "$HL_DIR/hl.sh" ]; then
	echo "No hl.sh in $HL_DIR (set HL_DIR to your Half-Life folder)"
	exit 1
fi

mkdir -p "$HL_DIR/zelda"
cp -r "$REPO/zelda/." "$HL_DIR/zelda/"

cd "$HL_DIR"
# Old libraries the engine still links against (libpng12, libgcrypt11), shipped in compat/
export LD_LIBRARY_PATH="$PWD/compat/libpng12/usr/lib/i386-linux-gnu:$PWD/compat/libgcrypt11/usr/lib/i386-linux-gnu:$PWD/compat/libpng12/lib/i386-linux-gnu:$PWD/compat/libgcrypt11/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./hl.sh -game zelda "$@"
