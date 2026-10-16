# Testing GAIAXXL

## Offline (every change)
| Check | Command | Result (0.1.0, 2026-10-16) |
|---|---|---|
| Engine test, x86, ASan/UBSan: 64 patches, 21 waves, filters, SYNC/RING, 12 arp motifs, randomizer x 96, state | `test/run_tests.sh` | PASSED |
| mpc-vst-plugins host test on the wrapper (two instances, stepping, programs, chunk, two threads) | `test/host_test.sh` | PASSED |
| Engine test as ARM code (Zig, qemu-arm) | `test/arm_sim.sh` | PASSED |
| Skin: touch boxes, edges, Q-Links (skin_check.py) | `./build.sh` | clean |
| Worst case CPU, x86 -O2 (8 voices x 3 super saws, every effect) | engine test | 146 us / 2902 us |

## On the device (to do, then add the result to tested.json)
1. Install the release zip (it stops and restarts MPC), insert GAIAXXL on a track.
2. Play Supersaw Lead; step through patches with ◀ ▶ and the PRESET menu; check the names.
3. Each page: turn every knob and fader; check the LCD values and that the sound follows.
4. Q-Links on each page and bank.
5. ARPEGGIO with MPC playing and stopped; KEY HOLD.
6. RANDOM on each type; AMOUNT 20 for variations.
7. Save and reload the project: the sound comes back.
8. CPU: `tools/bench.sh build/arm/gaiaxxl.so <ip> -j` (worst case: three S-SAW tones, all effects, eight notes).
