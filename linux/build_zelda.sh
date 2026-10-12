#!/bin/bash
# Zelda 64 port: build hl.so + client.so with a current gcc (no Steam runtime container) and
# copy them into the zelda mod folder. Needs gcc/g++ multilib (apt install g++-multilib).
#
#   ./build_zelda.sh            release build
#   ./build_zelda.sh clean
set -e
cd "$(dirname "$0")"

if [ "$1" == "clean" ]; then
	make clean CC='"gcc -m32"' CPLUS='"g++ -m32"'
	exit 0
fi

# -MMD -MP: the makefiles include the .d files, so editing a header (zl_sounds.h, ...) rebuilds
# everything that uses it.
# The SDK makefiles also copy their output into a Valve tree (../game/valve); keep that inside
# the build folder instead of next to the repository
mkdir -p release/game/dlls release/game/cl_dlls
make hl hl_cdll HL_OUTPUT_DIR="$PWD/release/game" \
	CC='"gcc -m32 -std=gnu11 -MMD -MP -fno-delete-null-pointer-checks -fno-gnu-unique -fvisibility=hidden"' \
	CPLUS='"g++ -m32 -std=gnu++11 -MMD -MP -fno-delete-null-pointer-checks -fno-gnu-unique -fvisibility=hidden"' \
	CPP_LIB="-lstdc++ -ldl -lpthread"

mkdir -p ../zelda/dlls ../zelda/cl_dlls
cp release/hl.so ../zelda/dlls/hl.so
cp release/client.so ../zelda/cl_dlls/client.so
echo "Copied to zelda/dlls/hl.so and zelda/cl_dlls/client.so"
