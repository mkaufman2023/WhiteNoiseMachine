/*************************************************************************
*                                                                        *
*   Player.h                                                             *
*   Non-blocking playback engine for the White Noise Machine.            *
*                                                                        *
*   Owns the authoritative device state, a small command queue, a        *
*   volume-fade engine, and NVS persistence. Nothing in here blocks:     *
*   every public method returns immediately and the work happens in      *
*   tick(), called from loop().                                          *
*                                                                        *
*   Written by Matt Kaufman - December 2025                              *
*   Rewritten for non-blocking operation - July 2026                     *
*                                                                        *
**************************************************************************/

#ifndef PLAYER_H
#define PLAYER_H

#include <Arduino.h>
#include <Preferences.h>

class DFPlayerMini;


class Player {
public:
    static constexpr uint8_t VOL_MAX     = 30;  /**< DFPlayer hardware maximum */
    static constexpr uint8_t TRACK_COUNT = 14;  /**< Files in /mp3 on the SD card */

    /**
     * Everything the phone needs to render the UI. This is the single source
     * of truth -- the UI never guesses, it only draws what the box reports.
     *
     * `playing` is what the listener asked for: it turns false the moment
     * Pause or Stop is pressed, even though the sound takes a second to fade.
     *
     * `volume` is the setpoint (what was asked for); `volume_now` is what the
     * module is actually at, which differs from `volume` while a fade walks.
     * The UI shows the slider at `volume` and the numeric readout at
     * `volume_now`, so you can watch a fade happen.
     */
    struct State {
        bool     playing    = false;
        uint8_t  track      = 1;
        uint8_t  volume     = 12;
        uint8_t  volume_now = 0;
        bool     fading     = false;
        bool     hw_ready   = false;
        uint32_t favorites  = 0;   /**< Bitmask; track N is bit (N-1) */
    };

    explicit Player(DFPlayerMini& player);

    /** Starts serial comms and restores saved state. Does not block. */
    void begin();

    /** Pumps the engine. Must be called every loop() iteration. */
    void tick();

    /* ---- Commands. All non-blocking; all return immediately. ---- */

    /** Fades out, switches to `track`, fades back in. */
    void select_track(uint8_t track);

    /** Sets the volume setpoint (0-30). Direct user adjustments jump; fades walk. */
    void set_volume(uint8_t volume);

    /** Fades out, then pauses. */
    void pause();

    /**
     * Starts the sound again, fading up from silence to the setpoint.
     * Unpauses if the module is paused; otherwise (fresh boot, after Stop)
     * starts the selected track in loop mode. Pressed during a fade-out to
     * pause or stop, it turns the fade back around instead.
     */
    void resume();

    /** Convenience: pause if playing, resume if not. */
    void toggle();

    /** Fades out, then stops playback entirely. */
    void stop();

    /** Pins or unpins a track in the UI's favorites row. */
    void toggle_favorite(uint8_t track);

    /** Current authoritative state. */
    const State& state() const { return _s; }

    /**
     * Returns true once after anything in State changes, then clears the flag.
     * loop() uses this to decide when to broadcast to connected phones.
     */
    bool consume_dirty();


private:
    /* Timing constants. The old driver blocked 520 ms per command; the module
     * only needs a short gap between frames, which we enforce without delay(). */
    static constexpr uint32_t MIN_TX_GAP_MS  = 35;    /**< Min spacing between UART frames */
    static constexpr uint32_t FADE_STEP_MS   = 50;    /**< One volume unit per this, during fades */
    static constexpr uint32_t WARMUP_MS      = 2500;  /**< Module power-on settling time */
    static constexpr uint32_t SAVE_DEBOUNCE_MS = 5000;/**< Delay before writing NVS, to spare flash */

    enum class Cmd : uint8_t { NONE, SET_SOURCE, SET_VOL, LOOP_TRACK, PLAY, PAUSE, STOP, POWER_ON_VOL };
    enum class Seq : uint8_t { IDLE, OUT_TO_SWITCH, OUT_TO_PAUSE, OUT_TO_STOP, IN };

    struct QCmd { Cmd cmd; uint8_t arg; };

    DFPlayerMini& _df;
    State         _s;
    Preferences   _nvs;

    /* Command queue. Small ring buffer -- deep enough for the boot sequence,
     * and track changes coalesce into _pending_track rather than queueing up,
     * so rapid tapping can never flood it. */
    static constexpr uint8_t Q_LEN = 8;
    QCmd    _q[Q_LEN] = {};
    uint8_t _q_head = 0;
    uint8_t _q_tail = 0;

    /* Fade / setpoint state */
    Seq      _seq            = Seq::IDLE;
    uint8_t  _vol_target     = 0;
    uint8_t  _pending_track  = 1;
    bool     _vol_write_due  = false;   /**< Latest-value-wins volume write */
    bool     _paused         = false;   /**< Module is paused mid-track, so PLAY resumes it */

    /* Timing */
    uint32_t _t_begin        = 0;
    uint32_t _t_last_tx      = 0;
    uint32_t _t_last_step    = 0;
    uint32_t _t_state_change = 0;
    bool     _warm           = false;
    bool     _dirty          = false;
    bool     _save_due       = false;

    bool queue_push(Cmd cmd, uint8_t arg = 0);
    bool queue_pop(QCmd& out);
    void transmit(const QCmd& c);
    void start_playback(Cmd cmd, uint8_t arg = 0);
    void advance_sequence();
    void begin_fade(uint8_t target);
    void mark_dirty();
    void load_state();
    void save_state();
};


#endif  // PLAYER_H
