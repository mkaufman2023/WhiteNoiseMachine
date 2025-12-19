/****************************************************************************************
*                                                                                       *
*   DFPlayerMini.h                                                                      *
*   Library for TD5580A-based DFPlayer Mini clones                                      *
*                                                                                       *
*   Adapted from https://github.com/keith721/TD5580A                                    *
*   Adapted for HiLetgo's clone (see: 2) by Matt Kaufman, December, 2025.               *
*                                                                                       *
*   See:                                                                                *
*     1. http://www.tudasemi.com/static/upload/file/20240905/1725499313437991.pdf       *
*     2. https://www.amazon.com/HiLetgo-Arduino-Player-Module-DFPlayer/dp/B01D1D0E7Q    *
*                                                                                       *
*****************************************************************************************/

#ifndef DF_PLAYER_MINI_H
#define DF_PLAYER_MINI_H

#include <Arduino.h>


class DFPlayerMini {
public:
    DFPlayerMini(int mcu_rx = D7, int mcu_tx = D6);

    void begin(bool debug = false);

    void play_next();
    void play_previous();
    
    void play_track(int track);
    void play_track(byte track);

    void play_track_in_folder(int folder, int track);
    void play_track_in_folder(byte folder, byte track);

    void loop_track(int track);
    void loop_track(byte track);

    void loop_track_in_folder(int folder, int track);
    void loop_track_in_folder(byte folder, byte track);

    // void play_folder(int folder);   // void playF(byte f);
    // void play_folder(byte folder);  // void playF(byte f);

    void loop_folder(int folder);
    void loop_folder(byte folder);

    void loop_all_tracks();

    void shuffle_all_tracks();

    void start_looping_current_track();
    void stop_looping_current_track();
    
    void set_folder(int folder);
    void set_folder(byte folder);

    void set_source(int source);
    void set_source(byte source);

    void increment_volume();
    void decrement_volume();
    void set_volume(int volume);
    void set_volume(byte volume);
    void set_power_on_volume(int volume);
    void set_power_on_volume(byte volume);

    void set_EQ(int eq);
    void set_EQ(byte eq);

    void play();
    void pause();
    void stop_all_playback();
    
    void reset();

    void enable_DAC();
    void disable_DAC();

    void sleep();
    void wakeup();

    uint16_t get_status();
    uint16_t get_volume();
    uint16_t get_folder_count();
    uint16_t get_folder_track_count();
    uint16_t get_total_track_count();
    // uint16_t get_currently_playing_track(); // uint16_t qPlaying();

private:
    int _mcu_rx;    // MCU RX pin
    int _mcu_tx;    // MCU TX pin

    Stream* _serial = nullptr;      // Serial stream
    byte    _ansbuf[15] = {0};      // Response buffer
    bool    _show_debug_messages;   // Show debug flag

    String _sanswer();
    String _sbyte2hex(byte b);
    // int    _shex2int(char *s, int n);

    uint16_t _read_response();

    void _send_command(byte command);
    void _send_command(byte command, byte data2);
    void _send_command(byte command, byte data1, byte data2);
};


#endif  // DF_PLAYER_MINI_H