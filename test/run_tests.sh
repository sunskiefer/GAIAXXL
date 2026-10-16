#!/usr/bin/env bash
# GAIAXXL offline tests (x86): the engine test, under AddressSanitizer and UndefinedBehaviorSanitizer.
#   test/run_tests.sh        (-w: also write every factory patch as a WAV into build/wav/)
set -euo pipefail
cd "$(dirname "$0")/.."
. ./build_srcs.sh
python3 tools/gen_params.py --check
mkdir -p build/host build/wav
gcc -O1 -g -std=gnu11 -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc -Ithird_party/mpc-vst-plugins/wrapper \
  $SRCS test/engine_test.c -lm -o build/host/engine_test
build/host/engine_test "$@"
