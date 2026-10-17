// Host test for lib/Logbook -- the real logbook the firmware uses.
#include <Arduino.h>
#include <Logbook.h>
#include <cstdio>
#include <string>

uint32_t g_millis = 0;

static int fails = 0;
static void check(const char* what, bool ok) {
    printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) fails++;
}

static int count_lines(const std::string& s) {
    if (s.empty()) return 0;
    int n = 1; for (char c : s) if (c == '\n') n++; return n;
}

int main() {
    printf("\nDiagnostics logbook\n");

    check("empty to start", logbook::newest() == 0);

    g_millis = 12345;
    logbook::add("Net: online as %s (%d dBm)", "192.168.1.50", -55);
    logbook::add("second");
    logbook::add("third");
    { std::string out; logbook::copy(0, logbook::newest(), out);
      check("lines numbered from 1", logbook::newest() == 3);
      check("formatted, with a timestamp", out.rfind("[12.345] Net: online as 192.168.1.50 (-55 dBm)", 0) == 0);
      check("oldest first, one per line", count_lines(out) == 3 && out.find("third") > out.find("second")); }

    { std::string out; logbook::copy(1, 2, out);
      check("copy(after, upto) gives just that range", out.find("second") != std::string::npos && count_lines(out) == 1); }

    { std::string out; logbook::copy(3, 3, out);
      check("nothing new after the newest line", out.empty()); }

    for (int i = 4; i <= 100; i++) logbook::add("line %d", i);
    { std::string out; logbook::copy(0, logbook::newest(), out);
      check("keeps only the newest 40 lines", count_lines(out) == logbook::CAPACITY);
      check("...which are the newest", out.find("line 61") != std::string::npos && out.find("line 60\n") == std::string::npos
                                     && out.substr(out.size() - 8) == "line 100"); }

    { std::string out; logbook::copy(95, 1000, out);
      check("upto past the newest is clamped", count_lines(out) == 5); }

    std::string longline(500, 'x');
    logbook::add("%s", longline.c_str());
    { std::string out; logbook::copy(100, logbook::newest(), out);
      check("long lines cut short, not overflowing", out.size() == logbook::LINE_LENGTH - 1); }

    printf("\n%s  (%d failures)\n", fails ? "LOGBOOK TESTS FAILED" : "ALL LOGBOOK TESTS PASSED", fails);
    return fails ? 1 : 0;
}
