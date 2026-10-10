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

mkdir -p ../../game/valve/dlls ../../game/valve/cl_dlls
make hl hl_cdll \
	CC='"gcc -m32 -fno-delete-null-pointer-checks -fno-gnu-unique -fvisibility=hidden"' \
	CPLUS='"g++ -m32 -std=gnu++11 -fno-delete-null-pointer-checks -fno-gnu-unique -fvisibility=hidden"' \
	CPP_LIB="-lstdc++ -ldl -lpthread"

mkdir -p ../zelda/dlls ../zelda/cl_dlls
cp release/hl.so ../zelda/dlls/hl.so
cp release/client.so ../zelda/cl_dlls/client.so
echo "Copied to zelda/dlls/hl.so and zelda/cl_dlls/client.so"
