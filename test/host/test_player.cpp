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
// Argument of the first `cmd` frame, or -1 if none was sent.
static int first_arg(const char* cmd) {
    for (auto& r:g_df->tx) if (r.cmd==cmd) return r.arg;
    return -1;
}
// Time of the n-th (0-based) `cmd` frame, or 0 if there aren't that many.
static uint32_t t_of(const char* cmd, int n = 0) {
    for (auto& r:g_df->tx) if (r.cmd==cmd && n-- == 0) return r.t;
    return 0;
}
// Time from the first `cmd` frame to whatever frame went out next.
static uint32_t gap_after(const char* cmd) {
    for (size_t i=0; i+1<g_df->tx.size(); i++)
        if (g_df->tx[i].cmd==cmd) return g_df->tx[i+1].t - g_df->tx[i].t;
    return 0;
}
// True if the last volume sent before the first `cmd` frame was 0 -- i.e. the
// module was silent at the moment that command started the sound.
static bool silent_before(const char* cmd) {
    int vol = -1;
    for (auto& r:g_df->tx) {
        if (r.cmd=="set_volume") vol = r.arg;
        if (r.cmd==cmd) return vol==0;
    }
    return false;
}

int main() {
    /* ---------------- 1. Cold boot, nothing saved ---------------- */
    printf("\n1. Cold boot with empty NVS\n");
    DFPlayerMini df1; Player p1(df1);
    g_df=&df1; g_pl=&p1; g_millis=1000;
    p1.begin();
    run(200);
    check("nothing transmitted during warmup", df1.tx.size()==1); // begin() only
    check("status says warming up", p1.state().hw == Player::Hw::WARMING);
    run(2600);
    check("status ok once the module replied", p1.state().hw == Player::Hw::OK);
    check("no set-source frame (SD is the only source)", count("set_source")==0);
    check("no power-on-volume frame", count("power_on_vol")==0);
    check("module set to volume 0", first_arg("set_volume")==0);
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
    check("module silenced before the track starts", silent_before("loop_track"));
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
    check("switch landed while the module was at volume 0", silent_before("loop_track"));

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
    check("unpaused rather than restarting the track", count("loop_track")==0);
    check("silenced before unpausing", silent_before("play"));
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
    run(2700);   // warm-up, then the boot frames
    check("auto-resumed the saved sound", count("loop_track")==1 && p2.state().playing);
    check("silenced before the sound starts", silent_before("loop_track"));
    check("still ramping, not instantly loud", p2.state().volume_now < 25);
    run(2000);
    check("reached saved level", p2.state().volume_now==25);

    /* ---------------- 9. Bounds ---------------- */
    printf("\n9. Input validation\n");
    uint8_t before = p2.state().track;
    p2.select_track(0);   check("track 0 rejected", p2.state().track==before);
    p2.select_track(99);  check("track 99 rejected", p2.state().track==before);
    p2.set_volume(200);   check("volume clamped to 30", p2.state().volume==30);

    /* ---------------- 10. Play after a fresh boot ---------------- */
    printf("\n10. Play after a fresh boot starts the saved sound, looping\n");
    g_nvs.u.clear();
    g_nvs.u["track"] = 6; g_nvs.u["volume"] = 10; g_nvs.u["playing"] = 0;
    DFPlayerMini df3; Player p3(df3);
    g_df=&df3; g_pl=&p3;
    p3.begin();
    run(2700);
    check("boots silent", !p3.state().playing && count("loop_track")==0);
    df3.tx.clear();
    p3.resume();
    run(100);
    check("started the track rather than unpausing", count("loop_track")==1 && count("play")==0);
    check("started the saved track (6)", first_arg("loop_track")==6);
    check("silenced before starting", silent_before("loop_track"));
    run(1000);
    check("faded up to the saved level", p3.state().volume_now==10 && p3.state().playing);

    /* ---------------- 11. Play after Stop ---------------- */
    printf("\n11. Play after Stop restarts the sound, looping\n");
    df3.tx.clear();
    p3.stop();
    check("state shows stopped immediately", !p3.state().playing);
    run(1000);
    check("stop sent after fade", count("stop")==1);
    df3.tx.clear();
    p3.resume();
    run(100);
    check("restarted the track rather than unpausing", count("loop_track")==1 && count("play")==0);
    run(1000);
    check("back at the saved level", p3.state().volume_now==10);

    /* ---------------- 12. Play during a fade-out ---------------- */
    printf("\n12. Play during a fade-out to pause turns the fade around\n");
    df3.tx.clear();
    p3.pause();
    run(150);
    check("mid fade-out", p3.state().volume_now > 0 && p3.state().volume_now < 10);
    p3.resume();
    run(1000);
    check("pause never sent", count("pause")==0);
    check("no restart or unpause needed", count("loop_track")==0 && count("play")==0);
    check("back up at the setpoint", p3.state().volume_now==10 && p3.state().playing);

    /* ---------------- 13. Double-tap Play/Pause ---------------- */
    printf("\n13. Double-tapping Play/Pause ends up playing\n");
    df3.tx.clear();
    p3.toggle(); run(100);
    p3.toggle(); run(1000);
    check("still playing", p3.state().playing && count("pause")==0);
    check("at the setpoint", p3.state().volume_now==10);

    /* ---------------- 14. Slider moved while paused ---------------- */
    printf("\n14. Volume changed while paused still fades in from silence\n");
    p3.pause(); run(1000);
    p3.set_volume(20); run(100);
    df3.tx.clear();
    p3.resume();
    run(40);
    check("silenced before unpausing", silent_before("play"));
    check("not instantly at the new level", p3.state().volume_now < 20);
    run(1500);
    check("faded up to the new level", p3.state().volume_now==20);

    /* ---------------- 15. Dropped command ---------------- */
    printf("\n15. A command the module never answers is resent\n");
    df3.tx.clear();
    df3.drop("loop_track");
    p3.select_track(9);
    run(3000);
    check("track command sent twice (lost once, resent)", count("loop_track")==2);
    check("resend waited for the reply timeout", t_of("loop_track", 1) - t_of("loop_track", 0) >= 200);
    check("then carried on: switched and faded back up", p3.state().track==9 && p3.state().volume_now==20);
    check("status still ok", p3.state().hw == Player::Hw::OK);

    /* ---------------- 16. Rejected command ---------------- */
    printf("\n16. A command the module rejects is resent right away\n");
    df3.tx.clear();
    df3.reject("loop_track");
    p3.select_track(3);
    run(3000);
    check("track command sent twice", count("loop_track")==2);
    check("without waiting out the timeout", t_of("loop_track", 1) - t_of("loop_track", 0) < 200);
    check("switched and faded back up", p3.state().track==3 && p3.state().volume_now==20);

    /* ---------------- 17. Track ended on its own ---------------- */
    printf("\n17. If the module says the track ended, the loop is restarted\n");
    df3.tx.clear();
    df3.say(dfplayer::cmd::QU_TF_SD_COMPL);
    run(200);
    check("restarted the track", count("loop_track")==1 && first_arg("loop_track")==3);
    df3.say(dfplayer::cmd::QU_TF_SD_COMPL);
    run(200);
    check("but not again within a second", count("loop_track")==1);

    /* ---------------- 18. Module restarted on its own ---------------- */
    printf("\n18. If the module restarts by itself, the sound comes back from silence\n");
    run(1000);
    df3.tx.clear();
    df3.say(dfplayer::cmd::SEND_INIT_PARAMS);
    run(200);
    check("sound restarted", count("loop_track")==1);
    check("silenced first (the module forgot its volume)", silent_before("loop_track"));
    run(1500);
    check("faded back up", p3.state().volume_now==20);

    /* ---------------- 19. Module stops answering ---------------- */
    printf("\n19. Module stops answering, then comes back\n");
    df3.tx.clear();
    df3.acks = false;
    p3.set_volume(15); run(700);   // 3 tries x 200 ms, then give up
    check("one give-up isn't a fault yet", p3.state().hw == Player::Hw::OK);
    p3.set_volume(16); run(700);
    check("two in a row is", p3.state().hw == Player::Hw::FAULT);
    check("each was tried 3 times", count("set_volume")==6);
    df3.acks = true;
    p3.set_volume(17); run(300);
    check("ok again once it replies", p3.state().hw == Player::Hw::OK);
    check("and the new level got through", p3.state().volume_now==17);

    /* ---------------- 20. Module that never replies ---------------- */
    printf("\n20. A module that never replies gets fixed timings instead\n");
    g_nvs.u.clear();
    DFPlayerMini df4; df4.acks = false; Player p4(df4);
    g_df=&df4; g_pl=&p4;
    p4.begin();
    run(2700);
    check("status says no replies", p4.state().hw == Player::Hw::NO_REPLIES);
    df4.tx.clear();
    p4.select_track(2);
    run(2000);
    check("still started the track and faded up", count("loop_track")==1 && p4.state().volume_now==p4.state().volume);
    check("nothing resent", count("loop_track")==1 && count("set_volume") <= p4.state().volume + 1);
    check("waited >=250 ms after starting the track", gap_after("loop_track") >= 250);
    check("frames always >=50 ms apart", min_gap() >= 50);

    /* ---------------- 21. Module's own boot message ---------------- */
    printf("\n21. The module's \"I'm up\" during warm-up doesn't double-start the sound\n");
    g_nvs.u.clear();
    g_nvs.u["track"] = 5; g_nvs.u["volume"] = 10; g_nvs.u["playing"] = 1;
    DFPlayerMini df5; Player p5(df5);
    g_df=&df5; g_pl=&p5;
    p5.begin();
    df5.say(dfplayer::cmd::SEND_INIT_PARAMS, 1500);
    run(3000);
    check("restored sound started once", count("loop_track")==1);
    check("status ok", p5.state().hw == Player::Hw::OK);

    printf("\n%s  (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
