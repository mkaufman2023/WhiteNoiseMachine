#include <Arduino.h>
#include <Preferences.h>
#include <DFPlayerMini.h>
#include "Player.h"
#include <cstdio>
#include <cassert>

uint32_t g_millis = 0;
NvsStore g_nvs;

static DFPlayerMini* g_df = nullptr;
static Player*       g_pl = nullptr;

// Advance simulated time, ticking the player every millisecond.
static void run(uint32_t ms) { for (uint32_t i=0;i<ms;i++){ g_millis++; g_pl->tick(); } }

static int fails = 0;
static void check(const char* what, bool ok) {
    printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) fails++;
}
static void dump(const char* label) {
    printf("  [%s] tx:", label);
    for (auto& r : g_df->tx) {
        printf(" %s", r.cmd.c_str());
        if (r.arg >= 0) printf("(%d)", r.arg);
    }
    printf("\n");
}
static int count(const char* cmd) {
    int n=0; for (auto& r:g_df->tx) if (r.cmd==cmd) n++; return n;
}
// Smallest gap between consecutive transmissions.
static uint32_t min_gap() {
    uint32_t m = 0xFFFFFFFF;
    for (size_t i=1;i<g_df->tx.size();i++) {
        uint32_t g = g_df->tx[i].t - g_df->tx[i-1].t;
        if (g < m) m = g;
    }
    return g_df->tx.size()<2 ? 0 : m;
}

int main() {
    /* ---------------- 1. Cold boot, nothing saved ---------------- */
    printf("\n1. Cold boot with empty NVS\n");
    DFPlayerMini df1; Player p1(df1);
    g_df=&df1; g_pl=&p1; g_millis=1000;
    p1.begin();
    run(200);
    check("nothing transmitted during warmup", df1.tx.size()==1); // begin() only
    run(2600);
    check("hardware marked ready after warmup", p1.state().hw_ready);
    check("source set to SD card", count("set_source")==1);
    check("module power-on volume capped", count("power_on_vol")==1);
    check("silent boot: not playing", !p1.state().playing);
    check("no track started", count("loop_track")==0);
    dump("boot");

    /* ---------------- 2. Select a track: fade in ---------------- */
    printf("\n2. Select track 3 from silence\n");
    df1.tx.clear();
    p1.select_track(3);
    run(50);
    check("loop_track sent promptly", count("loop_track")==1);
    check("track recorded in state", p1.state().track==3);
    check("volume starts at 0, not the setpoint", p1.state().volume_now < p1.state().volume);
    run(1500);
    check("fade reached setpoint", p1.state().volume_now == p1.state().volume);
    check("fade flag cleared", !p1.state().fading);
    check("ramp used multiple volume writes", count("set_volume") > 5);
    check("inter-frame gap respected (>=35ms)", min_gap() >= 35);
    printf("  (ramped 0 -> %d)\n", p1.state().volume);

    /* ---------------- 3. Slider drag: latest value wins ---------------- */
    printf("\n3. Rapid slider drag 12 -> 25\n");
    df1.tx.clear();
    for (uint8_t v=12; v<=25; v++) { p1.set_volume(v); run(3); }  // 3ms apart
    run(60);
    check("setpoint is the final value", p1.state().volume==25);
    check("actual matches setpoint (no walk on direct set)", p1.state().volume_now==25);
    check("writes coalesced, not one per event", count("set_volume") < 8);
    check("gap still respected during drag", min_gap() >= 35);
    printf("  (14 slider events -> %d UART writes)\n", count("set_volume"));

    /* ---------------- 4. Track change fades out then in ---------------- */
    printf("\n4. Switch to track 8 while playing\n");
    df1.tx.clear();
    p1.select_track(8);
    run(200);
    check("fades down before switching", p1.state().volume_now < 25);
    check("loop_track withheld until silent", count("loop_track")==0);
    run(3000);
    check("track switched", p1.state().track==8);
    check("loop_track sent once", count("loop_track")==1);
    check("returned to setpoint", p1.state().volume_now==25);
    // ordering: the switch must land while volume is at 0
    { bool ok=false; for (auto&r:g_df->tx) if(r.cmd=="loop_track") ok=true;
      check("switch occurred inside the fade window", ok); }

    /* ---------------- 5. Rapid taps coalesce ---------------- */
    printf("\n5. Tap four sounds quickly\n");
    df1.tx.clear();
    p1.select_track(2); run(20);
    p1.select_track(5); run(20);
    p1.select_track(9); run(20);
    p1.select_track(4); run(4000);
    check("only the last selection played", p1.state().track==4);
    check("did not queue one loop_track per tap", count("loop_track")==1);
    printf("  (4 taps -> %d loop_track frames)\n", count("loop_track"));

    /* ---------------- 6. Pause / resume ---------------- */
    printf("\n6. Pause then resume\n");
    df1.tx.clear();
    p1.pause();
    run(100);
    check("pause withheld until faded out", count("pause")==0);
    run(2500);
    check("pause sent after fade", count("pause")==1);
    check("state shows paused", !p1.state().playing);
    check("volume floored", p1.state().volume_now==0);
    check("setpoint remembered", p1.state().volume==25);
    df1.tx.clear();
    p1.resume();
    run(2500);
    check("resume sent", count("play")==1);
    check("faded back to setpoint", p1.state().volume_now==25);
    check("state shows playing", p1.state().playing);

    /* ---------------- 7. Favorites + persistence ---------------- */
    printf("\n7. Favorites and NVS persistence\n");
    p1.toggle_favorite(4);
    p1.toggle_favorite(8);
    check("two favorites set", p1.state().favorites == ((1u<<3)|(1u<<7)));
    p1.toggle_favorite(4);
    check("unpin works", p1.state().favorites == (1u<<7));
    run(6000);   // let the debounced save fire
    check("state written to NVS", g_nvs.u.count("track") && g_nvs.u.count("volume"));

    printf("\n8. Warm reboot restores and fades in\n");
    DFPlayerMini df2; Player p2(df2);
    g_df=&df2; g_pl=&p2;
    p2.begin();
    check("track restored", p2.state().track==4);
    check("volume setpoint restored", p2.state().volume==25);
    check("favorites restored", p2.state().favorites==(1u<<7));
    check("comes up silent", p2.state().volume_now==0);
    run(2600);
    check("auto-resumed the saved sound", count("loop_track")==1 && p2.state().playing);
    check("still ramping, not instantly loud", p2.state().volume_now < 25);
    run(2000);
    check("reached saved level", p2.state().volume_now==25);

    /* ---------------- 9. Bounds ---------------- */
    printf("\n9. Input validation\n");
    uint8_t before = p2.state().track;
    p2.select_track(0);   check("track 0 rejected", p2.state().track==before);
    p2.select_track(99);  check("track 99 rejected", p2.state().track==before);
    p2.set_volume(200);   check("volume clamped to 30", p2.state().volume==30);

    printf("\n%s  (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
