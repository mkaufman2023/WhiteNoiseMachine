/****************************************************************************************
*                                                                                       *
*   FrameParser.h - Reassembles reply frames from a DFPlayer Mini's serial stream      *
*                                                                                       *
*   Written by Matt Kaufman - October 2026                                              *
*                                                                                       *
*   Replies arrive in one of two shapes:                                                *
*                                                                                       *
*       7E FF 06 CMD FB D1 D2 CKH CKL EF    10 bytes, with checksum (the usual one)     *
*       7E FF 06 CMD FB D1 D2 EF            8 bytes, no checksum (some clones)          *
*                                                                                       *
*   They can't be confused: CKH is the high byte of -(FF + 06 + CMD + FB + D1 + D2),    *
*   which always lands in 0xFA-0xFE, so it is never the 0xEF end byte.                  *
*                                                                                       *
*   Bytes that don't fit either shape are dropped and the parser resyncs on the next    *
*   0x7E, so line noise can neither overrun the buffer nor wedge it.                    *
*                                                                                       *
*   No Arduino dependencies, so it can be tested on the host.                          *
*                                                                                       *
*****************************************************************************************/

#pragma once

#include <stdint.h>
#include <string.h>


namespace dfplayer
{
    /** A frame received from the module. */
    struct Frame {
        uint8_t cmd;     /**< What it's about, e.g. cmd::RESPONSE (0x41) for "got it" */
        uint8_t data1;
        uint8_t data2;
    };


    class FrameParser {
    public:
        /** Feeds one received byte. Returns true, and fills `out`, when it completes a frame. */
        bool feed(uint8_t b, Frame& out)
        {
            if (_len == 0 && b != START) return false;   // wait for a start byte

            _buf[_len++] = b;

            if (!header_ok(0)) {
                resync();
                return false;
            }

            if (_len == SHORT_LEN && b == END) return finish(out);
            if (_len < LONG_LEN)               return false;

            const uint16_t sum = _buf[1] + _buf[2] + _buf[3] + _buf[4] + _buf[5] + _buf[6];
            const uint16_t ck  = (uint16_t)((_buf[7] << 8) | _buf[8]);

            if (_buf[9] == END && (uint16_t)(sum + ck) == 0) return finish(out);

            resync();
            return false;
        }


    private:
        static constexpr uint8_t START     = 0x7E;
        static constexpr uint8_t END       = 0xEF;
        static constexpr uint8_t SHORT_LEN = 8;
        static constexpr uint8_t LONG_LEN  = 10;

        uint8_t _buf[LONG_LEN] = {};
        uint8_t _len           = 0;

        bool finish(Frame& out)
        {
            out  = { _buf[3], _buf[5], _buf[6] };
            _len = 0;
            return true;
        }

        /** True if the bytes after a start byte at `i` read FF 06, as far as they've arrived. */
        bool header_ok(uint8_t i) const
        {
            return (i + 1 >= _len || _buf[i + 1] == 0xFF) &&
                   (i + 2 >= _len || _buf[i + 2] == 0x06);
        }

        /**
         * Drops the bad frame start and slides forward to the next plausible one.
         * Header bytes are checked as they arrive, so a real frame can only begin
         * deep enough into a rejected one that it can't be complete yet -- nothing
         * is lost by not re-checking for completion here.
         */
        void resync()
        {
            uint8_t i = 1;
            while (i < _len && !(_buf[i] == START && header_ok(i))) i++;

            memmove(_buf, _buf + i, _len - i);
            _len = (uint8_t)(_len - i);
        }
    };
}
