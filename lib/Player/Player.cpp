/*************************************************************************
*                                                                        *
*   Player.cpp                                                           *
*   Non-blocking playback engine for the White Noise Machine.            *
*                                                                        *
*   Written by Matt Kaufman - December 2025                              *
*   Rewritten for non-blocking operation - July 2026                     *
*                                                                        *
**************************************************************************/

#include "Player.h"
#include <DFPlayerMini.h>


Player::Player(DFPlayerMini& player) : _df(player) { }


void Player::begin()
{
    load_state();

    _df.begin();
    _t_begin   = millis();
    _t_next_tx = _t_begin;
    _warm      = false;
}


void Player::tick()
{
    const uint32_t now = millis();

    /* ---- 1) Wait out the module's power-on settling time, then run the
     *          boot sequence. Nothing is transmitted before this. ---- */
    if (!_warm) {
        // Whatever the module says while it boots (e.g. its 0x3F "I'm up")
        // only proves it's alive -- don't act on it.
        dfplayer::Frame f;
        while (_df.poll(f)) _heard = true;

        if (now - _t_begin < WARMUP_MS) return;

        _warm = true;
        update_hw();

        // No set-source or power-on-volume commands: the SD card is the only
        // source, and the volume is always set to 0 before anything plays.

        // Come up silent. If the box was playing when it lost power, start that
        // sound again and fade up to the saved level -- a power blip at 2 a.m.
        // must not restart the sound at full volume.
        _s.volume_now  = 0;
        _vol_target    = 0;
        _vol_write_due = true;

        if (_s.playing) start_playback(Cmd::LOOP_TRACK, _s.track);

        mark_dirty();
        return;
    }

    /* ---- 2) Walk the volume toward its target (the fade engine). Each step
     *          produces one setpoint write, rate-limited below. ---- */
    if (_s.volume_now != _vol_target && (now - _t_last_step) >= FADE_STEP_MS) {
        _s.volume_now += (_vol_target > _s.volume_now) ? 1 : -1;
        _t_last_step   = now;
        _vol_write_due = true;
        mark_dirty();
    }

    _s.fading = (_s.volume_now != _vol_target);

    /* ---- 3) When a fade finishes, advance the sequence it belonged to. ---- */
    if (_seq != Seq::IDLE && _s.volume_now == _vol_target) {
        advance_sequence();
    }

    /* ---- 4) Hear out the module, then send the next frame when it's ready
     *          for one. Never blocks. ---- */
    dfplayer::Frame f;
    while (_df.poll(f)) handle_reply(f, now);

    dispatch(now);

    /* ---- 5) Persist state, debounced, so we aren't hammering flash on every
     *          slider movement. ---- */
    if (_save_due && (now - _t_state_change) >= SAVE_DEBOUNCE_MS) {
        save_state();
        _save_due = false;
    }
}


/* =======================================================================
 *  Commands
 * ======================================================================= */

void Player::select_track(uint8_t track)
{
    if (track < 1 || track > TRACK_COUNT) return;

    // The module is audible unless it's at volume 0 or has finished fading
    // out to a pause or stop.
    const bool audible = (_s.volume_now > 0) && (_s.playing || _seq != Seq::IDLE);

    _pending_track = track;
    _s.playing     = true;

    if (audible) {
        // Fade out first so the transition isn't an abrupt cut. A second tap
        // during the fade just retargets _pending_track -- no queue buildup.
        _seq = Seq::OUT_TO_SWITCH;
        begin_fade(0);
    }
    else {
        // Nothing to fade out of -- switch now.
        _s.track = track;
        start_playback(Cmd::LOOP_TRACK, track);
    }

    mark_dirty();
}


void Player::set_volume(uint8_t volume)
{
    if (volume > VOL_MAX) volume = VOL_MAX;

    _s.volume = volume;

    if (_seq == Seq::IDLE) {
        // Direct user adjustment: jump straight there. Dragging a slider should
        // feel immediate, and consecutive writes coalesce in the dispatcher so
        // a fast drag becomes one write per round trip to the module, latest
        // value wins.
        _s.volume_now  = volume;
        _vol_target    = volume;
        _vol_write_due = true;
    }
    else if (_seq == Seq::IN) {
        // Mid fade-in: just move the destination.
        _vol_target = volume;
    }
    // Mid fade-out: leave the fade alone; it'll ramp back up to the new
    // setpoint on the other side of the transition.

    mark_dirty();
}


void Player::pause()
{
    if (!_s.playing) return;

    _s.playing = false;
    _seq       = Seq::OUT_TO_PAUSE;
    begin_fade(0);
    mark_dirty();
}


void Player::resume()
{
    if (_s.playing) return;

    if (_seq == Seq::OUT_TO_PAUSE || _seq == Seq::OUT_TO_STOP) {
        // Still fading out, so nothing has been sent to the module yet and it
        // is still playing. Just turn the fade around.
        _s.playing = true;
        _seq       = Seq::IN;
        begin_fade(_s.volume);
    }
    else {
        // Only a paused module can simply be unpaused. After a fresh boot
        // there's no track loaded, and after a Stop the module may restart on
        // a different file outside loop mode -- so start the selected track.
        start_playback(_paused ? Cmd::PLAY : Cmd::LOOP_TRACK, _s.track);
    }

    mark_dirty();
}


void Player::toggle()
{
    _s.playing ? pause() : resume();
}


void Player::stop()
{
    // Already paused or stopped: nothing to fade out of.
    if (!_s.playing && _seq == Seq::IDLE) return;

    _s.playing = false;
    _seq       = Seq::OUT_TO_STOP;
    begin_fade(0);
    mark_dirty();
}


void Player::toggle_favorite(uint8_t track)
{
    if (track < 1 || track > TRACK_COUNT) return;
    _s.favorites ^= (1UL << (track - 1));
    mark_dirty();
}


bool Player::consume_dirty()
{
    if (!_dirty) return false;
    _dirty = false;
    return true;
}


/* =======================================================================
 *  Internals
 * ======================================================================= */

void Player::begin_fade(uint8_t target)
{
    _vol_target  = (target > VOL_MAX) ? VOL_MAX : target;
    _t_last_step = millis();
    _s.fading    = (_s.volume_now != _vol_target);
}


/**
 * Starts sound from silence. An explicit volume-0 frame is queued ahead of
 * the play command, so the module is silent when audio begins -- even if the
 * slider moved while it was paused, or it powered on at its own stored volume
 * -- and then the volume fades up to the setpoint.
 */
void Player::start_playback(Cmd cmd, uint8_t arg)
{
    _s.volume_now  = 0;
    _vol_write_due = false;          // the queued SET_VOL covers it
    queue_push(Cmd::SET_VOL, 0);
    queue_push(cmd, arg);

    _paused    = false;
    _s.playing = true;
    _seq       = Seq::IN;
    begin_fade(_s.volume);
}


void Player::advance_sequence()
{
    switch (_seq) {
        case Seq::OUT_TO_SWITCH:
            _s.track = _pending_track;
            start_playback(Cmd::LOOP_TRACK, _pending_track);
            break;

        case Seq::OUT_TO_PAUSE:
            queue_push(Cmd::PAUSE);
            _paused = true;
            _seq    = Seq::IDLE;
            break;

        case Seq::OUT_TO_STOP:
            queue_push(Cmd::STOP);
            _paused = false;
            _seq    = Seq::IDLE;
            break;

        case Seq::IN:
        case Seq::IDLE:
        default:
            _seq = Seq::IDLE;
            break;
    }

    mark_dirty();
}


/* =======================================================================
 *  Talking to the module
 * ======================================================================= */

void Player::handle_reply(const dfplayer::Frame& f, uint32_t now)
{
    using namespace dfplayer::cmd;

    _heard = true;
    _fault = false;

    switch (f.cmd) {
        case RESPONSE:              // 0x41 -- "got it"
            _acked    = true;
            _give_ups = 0;

            if (_awaiting_ack) {
                _awaiting_ack = false;

                // Starting a track keeps the module busy reading the SD card
                // for a moment; give it longer before the next frame.
                const bool track_start = (_cur.cmd == Cmd::LOOP_TRACK || _cur.cmd == Cmd::PLAY);
                _t_next_tx = now + (track_start ? TRACK_SETTLE_MS : ACK_GAP_MS);
            }
            break;

        case ERROR_RESEND:          // 0x40 -- the module rejected the frame
            if (_awaiting_ack) _t_sent = now - ACK_TIMEOUT_MS;   // resend now, don't wait out the timeout
            break;

        case QU_TF_SD_COMPL:        // 0x3D -- the track finished
            // In loop mode this shouldn't happen. If it does the sound has
            // stopped, so start it again: white noise mustn't quietly end at 3 a.m.
            if (_s.playing && _seq == Seq::IDLE && (now - _t_last_heal) >= HEAL_DEBOUNCE_MS) {
                queue_push(Cmd::LOOP_TRACK, _s.track);
                _t_last_heal = now;
            }
            break;

        case SEND_INIT_PARAMS:      // 0x3F -- the module (re)started and forgot everything
            if (_s.playing) start_playback(Cmd::LOOP_TRACK, _s.track);
            break;

        default:
            break;
    }

    update_hw();
}


/**
 * Sends the next frame once the module is ready for it: one command at a
 * time, each waiting for the module's reply (or, for a module that never
 * replies, a fixed gap). Commands that get no reply are resent.
 */
void Player::dispatch(uint32_t now)
{
    if (_awaiting_ack) {
        if (now - _t_sent < ACK_TIMEOUT_MS) return;     // still waiting for "got it"

        _awaiting_ack = false;

        // A fade step that a newer level has already overtaken isn't worth resending.
        const bool superseded = _cur_is_fade_step && _vol_write_due;

        if (!superseded) {
            if (_cur_tries < MAX_TRIES) {
                send(now);                              // lost or rejected: resend it
                return;
            }
            give_up();
        }
    }

    if ((int32_t)(now - _t_next_tx) < 0) return;      // still in the gap after the last frame

    if (queue_pop(_cur)) {
        _cur_is_fade_step = false;
    }
    else if (_vol_write_due) {
        _cur              = { Cmd::SET_VOL, _s.volume_now };
        _cur_is_fade_step = true;
        _vol_write_due    = false;
    }
    else {
        return;
    }

    _cur_tries = 0;
    send(now);
}


void Player::send(uint32_t now)
{
    transmit(_cur);
    _cur_tries++;
    _t_sent = now;

    if (_acked) {
        _awaiting_ack = true;                           // next frame waits for "got it"
    }
    else {
        // This module hasn't replied (yet): leave a gap that's safe without one.
        _t_next_tx = now + (_cur.cmd == Cmd::SET_VOL ? TIMED_VOL_GAP_MS : TIMED_GAP_MS);
    }
}


void Player::give_up()
{
    if (++_give_ups < FAULT_AFTER) return;

    // The module has stopped answering. Report it, and fall back to fixed
    // timings so commands keep flowing; its next reply undoes both.
    _fault = true;
    _acked = false;
    update_hw();
}


void Player::update_hw()
{
    const Hw hw = !_warm ? Hw::WARMING
                : _fault ? Hw::FAULT
                : _heard ? Hw::OK
                :          Hw::NO_REPLIES;

    if (hw != _s.hw) {
        _s.hw = hw;
        mark_dirty();
    }
}


void Player::transmit(const QCmd& c)
{
    switch (c.cmd) {
        case Cmd::SET_VOL:       _df.set_volume(c.arg);           break;
        case Cmd::LOOP_TRACK:    _df.loop_track(c.arg);           break;
        case Cmd::PLAY:          _df.play();                      break;
        case Cmd::PAUSE:         _df.pause();                     break;
        case Cmd::STOP:          _df.stop_all_playback();         break;
        case Cmd::NONE:
        default:                                                  break;
    }
}


bool Player::queue_push(Cmd cmd, uint8_t arg)
{
    const uint8_t next = (uint8_t)((_q_head + 1) % Q_LEN);
    if (next == _q_tail) return false;   // full; drop rather than block

    _q[_q_head] = { cmd, arg };
    _q_head     = next;
    return true;
}


bool Player::queue_pop(QCmd& out)
{
    if (_q_tail == _q_head) return false;
    out     = _q[_q_tail];
    _q_tail = (uint8_t)((_q_tail + 1) % Q_LEN);
    return true;
}


void Player::mark_dirty()
{
    _dirty          = true;
    _save_due       = true;
    _t_state_change = millis();
}


void Player::load_state()
{
    _nvs.begin("wnm", true);   // read-only

    _s.track     = (uint8_t) _nvs.getUChar("track", 1);
    _s.volume    = (uint8_t) _nvs.getUChar("volume", 12);
    _s.playing   =           _nvs.getBool ("playing", false);
    _s.favorites = (uint32_t)_nvs.getULong("favs", 0);

    _nvs.end();

    if (_s.track < 1 || _s.track > TRACK_COUNT) _s.track  = 1;
    if (_s.volume > VOL_MAX)                    _s.volume = 12;

    _s.volume_now = 0;
}


void Player::save_state()
{
    _nvs.begin("wnm", false);

    _nvs.putUChar("track",   _s.track);
    _nvs.putUChar("volume",  _s.volume);
    _nvs.putBool ("playing", _s.playing);
    _nvs.putULong("favs",    _s.favorites);

    _nvs.end();
}
