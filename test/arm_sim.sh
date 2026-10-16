#!/usr/bin/env bash
# The engine test built for ARM with the plugin's own target flags (static musl, so it runs under qemu-arm without an
# ARM glibc) and run under qemu: checks the ARM code generation. Needs qemu-user and Zig. (Timings under qemu mean
# nothing: the CPU check is tools/bench.sh on the device.)
set -euo pipefail
cd "$(dirname "$0")/.."
. ./build_srcs.sh
ZIG="${ZIG:-python3 -m ziglang}"
TGT="-target arm-linux-musleabihf -mcpu=generic+v7a+vfp3d16-d32-neon+thumb2 -ffp-contract=off"
mkdir -p build/armsim
$ZIG cc $TGT -O2 -w -std=gnu11 -Isrc -Ithird_party/mpc-vst-plugins/wrapper -static $SRCS test/engine_test.c -lm -o build/armsim/engine_test
(cd build/armsim && mkdir -p build/wav && qemu-arm ./engine_test)
