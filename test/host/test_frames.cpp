// Host test for the reply-frame parser in lib/DFPlayerMini/FrameParser.h.
// This is the real parser the firmware uses, not a mock.
#include <FrameParser.h>
#include <cstdio>
#include <vector>

using dfplayer::Frame;
using dfplayer::FrameParser;

static int fails = 0;
static void check(const char* what, bool ok) {
    printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) fails++;
}

// Feeds bytes through a parser and returns every frame it completed.
static std::vector<Frame> parse(FrameParser& p, const std::vector<uint8_t>& bytes) {
    std::vector<Frame> got;
    Frame f;
    for (uint8_t b : bytes) if (p.feed(b, f)) got.push_back(f);
    return got;
}

// A valid 10-byte frame (with checksum) for `cmd`, data1, data2.
static std::vector<uint8_t> frame10(uint8_t cmd, uint8_t d1 = 0, uint8_t d2 = 0) {
    uint16_t ck = (uint16_t)(0 - (0xFF + 0x06 + cmd + 0x00 + d1 + d2));
    return { 0x7E, 0xFF, 0x06, cmd, 0x00, d1, d2, (uint8_t)(ck >> 8), (uint8_t)ck, 0xEF };
}

static std::vector<uint8_t> cat(std::vector<uint8_t> a, const std::vector<uint8_t>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

int main() {
    printf("\nReply frame parsing\n");

    { FrameParser p;
      auto got = parse(p, { 0x7E, 0xFF, 0x06, 0x41, 0x00, 0x00, 0x00, 0xFE, 0xBA, 0xEF });
      check("10-byte \"got it\" frame (example from the datasheet)",
            got.size()==1 && got[0].cmd==0x41); }

    { FrameParser p;
      auto got = parse(p, { 0x7E, 0xFF, 0x06, 0x41, 0x00, 0x00, 0x00, 0xEF });
      check("8-byte frame without checksum", got.size()==1 && got[0].cmd==0x41); }

    { FrameParser p;
      auto got = parse(p, frame10(0x3F, 0x00, 0x02));
      check("data bytes come through", got.size()==1 && got[0].cmd==0x3F && got[0].data2==0x02); }

    { FrameParser p;
      auto got = parse(p, frame10(0x43, 0x7E, 0xEF));
      check("0x7E / 0xEF inside the data don't confuse it",
            got.size()==1 && got[0].data1==0x7E && got[0].data2==0xEF); }

    { FrameParser p;
      auto got = parse(p, cat({ 0x00, 0x13, 0xEF, 0x7E, 0x55 }, frame10(0x41)));
      check("noise (including a stray 0x7E) before a frame is skipped",
            got.size()==1 && got[0].cmd==0x41); }

    { FrameParser p;
      auto bad = frame10(0x41); bad[8] ^= 0x01;
      auto got = parse(p, cat(bad, frame10(0x3D)));
      check("bad checksum rejected, the next good frame still parsed",
            got.size()==1 && got[0].cmd==0x3D); }

    { FrameParser p;
      auto got = parse(p, cat(frame10(0x41), cat(frame10(0x3D), frame10(0x41))));
      check("back-to-back frames all parsed", got.size()==3 && got[1].cmd==0x3D); }

    { FrameParser p;
      std::vector<uint8_t> junk;
      for (int i = 0; i < 500; i++) junk.push_back((uint8_t)(i * 37 + 11));
      auto got = parse(p, cat(junk, frame10(0x41)));
      check("500 bytes of junk can't wedge it", !got.empty() && got.back().cmd==0x41); }

    printf("\n%s  (%d failures)\n", fails ? "FRAME TESTS FAILED" : "ALL FRAME TESTS PASSED", fails);
    return fails ? 1 : 0;
}
