/*
 * Host-side mock of the DFPlayerMini driver.
 *
 * Instead of writing UART frames it records each command Player sends, with
 * the simulated time it was sent, so the test can check what went out, in
 * what order, and how far apart.
 */
#pragma once

#include <Arduino.h>
#include <string>
#include <vector>


class DFPlayerMini {
public:
    struct Tx {
        std::string cmd;   // driver method, e.g. "loop_track"
        int         arg;   // its argument, or -1 if it takes none
        uint32_t    t;     // millis() when it was sent
    };

    std::vector<Tx> tx;

    void begin()                        { log("begin"); }

    void set_source(uint8_t source)     { log("set_source", source); }
    void set_power_on_volume(uint8_t v) { log("power_on_vol", v); }
    void set_volume(uint8_t v)          { log("set_volume", v); }
    void loop_track(uint8_t track)      { log("loop_track", track); }
    void play()                         { log("play"); }
    void pause()                        { log("pause"); }
    void stop_all_playback()            { log("stop"); }

private:
    void log(const char* cmd, int arg = -1) { tx.push_back({ cmd, arg, millis() }); }
};
