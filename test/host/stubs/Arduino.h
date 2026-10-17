/*
 * Host-side stand-in for the Arduino core, just enough for Player.cpp.
 *
 * millis() reads a simulated clock that the test advances by hand, so fades
 * and frame spacing can be checked deterministically.
 */
#pragma once

#include <cstdint>
#include <cstddef>

extern uint32_t g_millis;   // defined by the test

inline uint32_t millis() { return g_millis; }
