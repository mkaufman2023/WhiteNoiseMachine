#!/usr/bin/env bash
# Host-side regression test for the Player state machine.
#
# Runs on your PC, no hardware needed. The DFPlayer is mocked and millis() is
# simulated, so fades and command spacing are verified deterministically --
# which is much faster than reflashing the box to find out you broke a fade.
#
#   ./run.sh
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter \
    -I stubs -I ../../lib/Player \
    test_player.cpp ../../lib/Player/Player.cpp \
    -o test_player
./test_player
