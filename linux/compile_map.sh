#!/bin/bash
# Zelda 64 port: compile a map from zelda/maps with SDHLT's Linux tools.
#
#   ./compile_map.sh [name]          default zl_test -> zelda/maps/zl_test.bsp
#
# SDHLT_DIR: the tools folder from https://github.com/seedee/SDHLT/releases (with Linux/ in it),
# ~/half-life/sdhlt by default. HL_DIR: the Half-Life folder whose valve/*.wad the map uses,
# ~/half-life/Half-Life by default. The .map's wad key holds Windows paths; the compile runs on a
# copy with it pointed at HL_DIR, the .map in the repository is left alone.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
NAME="${1:-zl_test}"
SDHLT_DIR="${SDHLT_DIR:-$HOME/half-life/sdhlt}"
HL_DIR="${HL_DIR:-$HOME/half-life/Half-Life}"
TOOLS="$SDHLT_DIR/Linux"
V="$HL_DIR/valve"

for t in sdHLCSG sdHLBSP sdHLVIS sdHLRAD; do
	[ -x "$TOOLS/$t" ] || { echo "No $TOOLS/$t (set SDHLT_DIR)"; exit 1; }
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
sed "s|^\"wad\" .*|\"wad\" \"$V/halflife.wad;$V/liquids.wad;$V/xeno.wad\"|" \
	"$REPO/zelda/maps/$NAME.map" > "$WORK/$NAME.map"

cd "$WORK"
for t in sdHLCSG sdHLBSP sdHLVIS sdHLRAD; do
	echo "== $t"
	"$TOOLS/$t" "$NAME" > "$t.log" 2>&1 || { tr '\r' '\n' < "$t.log" | tail -20; exit 1; }
	tr '\r' '\n' < "$t.log" | grep -iE "^ *(warning|error)|leak" || true
done

cp "$NAME.bsp" "$REPO/zelda/maps/$NAME.bsp"
echo "wrote zelda/maps/$NAME.bsp"
