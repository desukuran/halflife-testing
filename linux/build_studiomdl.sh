#!/bin/bash
# Zelda 64 port: build the SDK's studiomdl (utils/studiomdl) for Linux, for
# tools/zelda/make_link_model.py --studiomdl linux/release/studiomdl
#
# The sources are Windows-only; this compiles a patched copy (the originals are left alone):
# backslash includes, <windows.h> / MSVC names from studiomdl_shim/, and the "gamma" global that
# clashes with libm's. 32 bit, since the file structures use long.
set -e
cd "$(dirname "$0")"
REPO="$(cd .. && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp "$REPO"/utils/studiomdl/*.c "$REPO"/utils/studiomdl/*.h "$WORK"/
cp "$REPO"/utils/common/{cmdlib,scriplib,mathlib,lbmlib}.[ch] "$WORK"/
cd "$WORK"
sed -i 's|#include "\.\./\.\./[a-z]*/|#include "|; s|\.\.\\\.\.\\engine\\||; s|<STDIO.H>|<stdio.h>|; s|<WINDOWS.H>|<windows.h>|' ./*.c ./*.h
sed -i 's/\bgamma\b/smdl_gamma/g' studiomdl.c studiomdl.h write.c tristrip.c
sed -i '/^typedef long\t\t\tLONG;/d' lbmlib.h

mkdir -p "$REPO/linux/release"
gcc -m32 -w -O1 -std=gnu89 -fcommon -include "$REPO/linux/studiomdl_shim/compat.h" -I "$REPO/linux/studiomdl_shim" -I . \
	-I "$REPO/engine" -I "$REPO/common" -I "$REPO/public" -I "$REPO/dlls" \
	studiomdl.c write.c tristrip.c lbmlib.c cmdlib.c scriplib.c mathlib.c -lm -o "$REPO/linux/release/studiomdl"
echo "wrote linux/release/studiomdl"
