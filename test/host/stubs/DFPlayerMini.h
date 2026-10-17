/*
 * Host-side mock of the DFPlayerMini driver.
 *
 * Instead of writing UART frames it records each command Player sends, with
 * the simulated time it was sent, so the test can check what went out, in
 * what order, and how far apart. It also plays the module's side: replying
 * "got it" (0x41) to each command after a short delay, unless a test tells it
 * to drop or reject one, stop replying altogether, or speak up unprompted.
 */
#pragma once

#include <Arduino.h>
#include <Commands.h>      // the real reply codes
#include <FrameParser.h>   // the real Frame type
#include <list>
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

    /* How the pretend module behaves. */
    bool     acks      = true;   // reply "got it" to each command
    uint32_t ack_delay = 15;     // ms between a command and its reply

    /** Ignore the next `n` frames of `cmd` -- no reply, as if it never arrived. */
    void drop(const char* cmd, int n = 1) { _drop_cmd = cmd; _drop_n = n; }

    /** Answer the next frame of `cmd` with an error (0x40) instead of "got it". */
    void reject(const char* cmd) { _reject_cmd = cmd; }

    /** Have the module send `cmd` unprompted, `delay` ms from now. */
    void say(uint8_t cmd, uint32_t delay = 0) { _rx.push_back({ millis() + delay, { cmd, 0, 0 } }); }

    void begin()                        { tx.push_back({ "begin", -1, millis() }); }

    void set_volume(uint8_t v)          { receive("set_volume", v); }
    void loop_track(uint8_t track)      { receive("loop_track", track); }
    void play()                         { receive("play"); }
    void pause()                        { receive("pause"); }
    void stop_all_playback()            { receive("stop"); }

    bool poll(dfplayer::Frame& out)
    {
        for (auto it = _rx.begin(); it != _rx.end(); ++it) {
            if (it->due <= millis()) {
                out = it->frame;
                _rx.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    struct Pending { uint32_t due; dfplayer::Frame frame; };

    std::list<Pending> _rx;
    std::string        _drop_cmd;
    int                _drop_n = 0;
    std::string        _reject_cmd;

    void receive(const char* cmd, int arg = -1)
    {
        tx.push_back({ cmd, arg, millis() });

        if (_drop_n > 0 && _drop_cmd == cmd) { _drop_n--; return; }

        if (_reject_cmd == cmd) {
            _reject_cmd.clear();
            _rx.push_back({ millis() + ack_delay, { dfplayer::cmd::ERROR_RESEND, 0, 0 } });
            return;
        }

        if (acks) _rx.push_back({ millis() + ack_delay, { dfplayer::cmd::RESPONSE, 0, 0 } });
    }
};
