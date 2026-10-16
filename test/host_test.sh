#!/usr/bin/env bash
# The framework's VST2 host test (mpc-vst-plugins tools/host_test.c) on GAIAXXL's wrapper and engine, x86, ASan/UBSan:
# two instances, names, set/get, option select and nudge, stepping (wheel, Q-Link, sweep), note -> audio, the chunk.
set -euo pipefail
cd "$(dirname "$0")/.."
. ./build_srcs.sh
MV=third_party/mpc-vst-plugins
python3 $MV/tools/gen_vst.py vst.json --params-h >/dev/null
mkdir -p build/host
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
gcc $SAN -std=gnu11 -Ibuild -Isrc -I$MV/wrapper $SRCS src/wrapper/vst2_wrap.c $MV/tools/host_test.c -lm -lpthread -o build/host/host_test
build/host/host_test
