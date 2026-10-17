/*
 * Host-side stand-in for the Arduino core, just enough for Player.cpp and
 * Logbook.cpp.
 *
 * millis() reads a simulated clock that the test advances by hand, so fades
 * and frame spacing can be checked deterministically. Serial output is
 * dropped (set WNM_TEST_SERIAL=1 in the environment to see it).
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

extern uint32_t g_millis;   // defined by the test

inline uint32_t millis() { return g_millis; }

struct HostSerial {
    void println(const char* s) {
        static const bool show = std::getenv("WNM_TEST_SERIAL") != nullptr;
        if (show) std::printf("    serial: %s\n", s);
    }
};

inline HostSerial Serial;
