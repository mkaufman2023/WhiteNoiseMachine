#!/usr/bin/env bash
# Host-side regression tests for the Player state machine and the reply parser.
#
# Runs on your PC, no hardware needed. The DFPlayer is mocked and millis() is
# simulated, so fades, command spacing and retries are verified
# deterministically -- which is much faster than reflashing the box to find
# out you broke a fade.
#
#   ./run.sh
set -e
cd "$(dirname "$0")"
FLAGS="-std=c++17 -Wall -Wextra -Wno-unused-parameter -I stubs -I ../../lib/Player -I ../../lib/DFPlayerMini"

g++ $FLAGS test_frames.cpp -o test_frames
./test_frames

g++ $FLAGS test_player.cpp ../../lib/Player/Player.cpp -o test_player
./test_player
